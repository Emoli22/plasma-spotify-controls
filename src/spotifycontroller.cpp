#include "spotifycontroller.h"
#include <KWallet>
#include <QCryptographicHash>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusVariant>
#include <QDBusInterface>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QDesktopServices>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QRandomGenerator>
#include <QSettings>
#include <QTcpSocket>
#include <QUrlQuery>

namespace { constexpr auto spotifyService = "org.mpris.MediaPlayer2.spotify"; constexpr auto path = "/org/mpris/MediaPlayer2"; constexpr auto iface = "org.mpris.MediaPlayer2.Player"; }

SpotifyController::SpotifyController(QObject *parent) : QObject(parent)
{
    connect(&m_positionTimer, &QTimer::timeout, this, [this] {
        refreshMpris();
        if (m_playing) { m_position += 1000; emit playbackChanged(); }
        if (spotifySource() && authorized() && ++m_likedPollTicks >= 60) {
            m_likedPollTicks = 0;
            requestLiked();
        }
        if (m_rateLimitUntil.isValid()) emit rateLimitChanged();
    });
    m_positionTimer.start(1000);
    connect(QDBusConnection::sessionBus().interface(), &QDBusConnectionInterface::serviceOwnerChanged, this,
            [this](const QString &) { refreshMpris(); });
    QDBusConnection::sessionBus().connect(spotifyService, path, "org.freedesktop.DBus.Properties", "PropertiesChanged", this, SLOT(refreshMpris()));
    connect(&m_callbackServer, &QTcpServer::newConnection, this, [this] {
        auto *socket = m_callbackServer.nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
            const auto request = QString::fromUtf8(socket->readAll());
            const auto first = request.section("\r\n", 0, 0).section(' ', 1, 1);
            const QUrlQuery query(QUrl("http://127.0.0.1:8888" + first));
            const auto callbackState = query.queryItemValue("state");
            const bool stateMatches = !m_oauthState.isEmpty() && callbackState == m_oauthState;
            socket->write(stateMatches ? "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\nYou may close this window and return to Plasma." : "HTTP/1.1 400 Bad Request\r\nContent-Type: text/html\r\n\r\nInvalid authorization response. You may close this window."); socket->disconnectFromHost(); m_callbackServer.close();
            if (!stateMatches) { m_oauthState.clear(); setError("Spotify authorization response was rejected because its state did not match."); return; }
            m_oauthState.clear();
            const auto code = query.queryItemValue("code");
            if (code.isEmpty()) { const auto reason = query.queryItemValue("error"); setError(reason.isEmpty() ? "Authorization was cancelled." : reason); return; }
            QUrlQuery form; form.addQueryItem("grant_type", "authorization_code"); form.addQueryItem("code", code); form.addQueryItem("redirect_uri", "http://127.0.0.1:8888/callback"); form.addQueryItem("client_id", m_clientId); form.addQueryItem("code_verifier", m_verifier);
            QNetworkRequest r(QUrl("https://accounts.spotify.com/api/token")); r.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
            auto *reply = m_network.post(r, form.toString(QUrl::FullyEncoded).toUtf8()); setBusy(true);
            connect(reply, &QNetworkReply::finished, this, [this, reply] { setBusy(false); const auto obj = QJsonDocument::fromJson(reply->readAll()).object(); reply->deleteLater(); if (obj.contains("access_token")) { clearPlaylistCache(); m_accessToken=obj["access_token"].toString(); m_refreshToken=obj["refresh_token"].toString(); saveTokens(); setStatus("Spotify connected; playlist cache will refresh for this account"); emit authorizationChanged(); requestLiked(); } else setError("Spotify authorization failed."); });
        });
    });
    refreshMpris();
}

QString SpotifyController::spotifyUriFromMetadata(const QVariantMap &metadata)
{
    const auto rawUrl = metadata.value("xesam:url").toString();
    if (rawUrl.startsWith("spotify:track:") || rawUrl.startsWith("spotify:episode:"))
        return rawUrl;

    // The Linux Spotify client currently exposes open.spotify.com links over
    // MPRIS, while other clients commonly use spotify:track: URIs.
    const QUrl url(rawUrl);
    if (url.host() != "open.spotify.com")
        return {};
    const auto parts = url.path().split('/', Qt::SkipEmptyParts);
    if (parts.size() != 2 || (parts[0] != "track" && parts[0] != "episode") || parts[1].isEmpty())
        return {};
    return QStringLiteral("spotify:") + parts[0] + ':' + parts[1];
}
void SpotifyController::setClientId(const QString &v) { if (m_clientId==v) return; m_clientId=v.trimmed(); restorePlaylistCache(); restoreErrorHistory(); emit clientIdChanged(); restoreTokens(); }
int SpotifyController::rateLimitSeconds() const { return qMax<qint64>(0, QDateTime::currentDateTimeUtc().secsTo(m_rateLimitUntil)); }
void SpotifyController::setError(const QString &v) { if (m_error==v) return; m_error=v; if(!v.isEmpty()){m_errorHistory.prepend(QVariantMap{{"time",QDateTime::currentDateTime().toString(Qt::ISODate)},{"message",v}});while(m_errorHistory.size()>20)m_errorHistory.removeLast();saveErrorHistory();emit errorHistoryChanged();} emit errorChanged(); }
void SpotifyController::setBusy(bool v) { if (m_busy==v) return; m_busy=v; emit busyChanged(); }
void SpotifyController::setStatus(const QString &v) { if(m_status==v)return;m_status=v;emit statusChanged(); }
void SpotifyController::restorePlaylistCache(){if(m_clientId.isEmpty())return;QSettings s("KDE","SpotifyPlasmoid");const auto key="playlist-cache/"+m_clientId+"/";m_userId=s.value(key+"user").toString();m_playlistCache=s.value(key+"items").toList();m_rateLimitUntil=s.value("rate-limit/"+m_clientId).toDateTime().toUTC();if(!m_playlistCache.isEmpty())m_playlistsExpiry=QDateTime::currentDateTimeUtc().addDays(30);if(m_rateLimitUntil>QDateTime::currentDateTimeUtc()){setStatus(QString("Spotify requests paused for %1 seconds").arg(rateLimitSeconds()));emit rateLimitChanged();}}
void SpotifyController::savePlaylistCache(){if(m_clientId.isEmpty()||m_userId.isEmpty())return;QSettings s("KDE","SpotifyPlasmoid");const auto key="playlist-cache/"+m_clientId+"/";s.setValue(key+"user",m_userId);s.setValue(key+"items",m_playlistCache);s.sync();}
void SpotifyController::clearPlaylistCache(){m_userId.clear();m_playlistCache.clear();m_playlistsExpiry={};m_nextPlaylistsOffset=-1;QSettings s("KDE","SpotifyPlasmoid");s.remove("playlist-cache/"+m_clientId);s.sync();emit playlistsPaginationChanged();}
void SpotifyController::restoreErrorHistory(){m_errorHistory.clear();if(m_clientId.isEmpty())return;QSettings s("KDE","SpotifyPlasmoid");m_errorHistory=s.value("error-history/"+m_clientId).toList();emit errorHistoryChanged();}
void SpotifyController::saveErrorHistory(){if(m_clientId.isEmpty())return;QSettings s("KDE","SpotifyPlasmoid");s.setValue("error-history/"+m_clientId,m_errorHistory);s.sync();}
void SpotifyController::clearErrorHistory(){if(m_errorHistory.isEmpty())return;m_errorHistory.clear();QSettings s("KDE","SpotifyPlasmoid");s.remove("error-history/"+m_clientId);s.sync();emit errorHistoryChanged();}
void SpotifyController::clearCachedPlaylists(){clearPlaylistCache();setStatus("Playlist cache cleared. Use Refresh now to download it again.");}
void SpotifyController::resetConnectionData(){forgetAuthorization();clearPlaylistCache();setError({});setStatus("Spotify connection and local playlist cache were reset.");}
void SpotifyController::setMaintenanceAction(const QString &value){m_maintenanceAction=value;}
void SpotifyController::setMaintenanceRevision(qulonglong value){if(value==0||value==m_maintenanceRevision)return;m_maintenanceRevision=value;if(m_maintenanceAction=="reset-connection")resetConnectionData();else if(m_maintenanceAction=="clear-playlist-cache")clearCachedPlaylists();else if(m_maintenanceAction=="clear-error-log")clearErrorHistory();}
void SpotifyController::setActiveMprisIdentity(const QString &value){if(m_activeMprisIdentity==value)return;m_activeMprisIdentity=value;refreshMpris();}
void SpotifyController::setActiveMprisArtUrl(const QString &value){if(m_activeMprisArtUrl==value)return;m_activeMprisArtUrl=value;if(!value.isEmpty()&&m_artUrl!=value){m_artUrl=value;emit metadataChanged();}}
void SpotifyController::openCurrentTrack(){if(m_trackUri.isEmpty())return;const auto parts=m_trackUri.split(':');if(parts.size()!=3)return;QDesktopServices::openUrl(QUrl("https://open.spotify.com/"+parts[1]+"/"+parts[2]));}
void SpotifyController::callMpris(const QString &method) { QDBusInterface p(m_service,path,iface,QDBusConnection::sessionBus()); if (!p.isValid()) { setError("No media player is running."); return; } p.asyncCall(method); }
void SpotifyController::playPause(){ callMpris("PlayPause"); } void SpotifyController::next(){ callMpris("Next"); } void SpotifyController::previous(){ callMpris("Previous"); }
void SpotifyController::seek(qint64 positionMs){if(!m_playerAvailable||!m_seekable||m_trackId.isEmpty())return;QDBusInterface p(m_service,path,iface,QDBusConnection::sessionBus());if(!p.isValid()){setError("The active media player is no longer available.");return;}p.asyncCall("SetPosition",QVariant::fromValue(QDBusObjectPath(m_trackId)),qBound<qint64>(0,positionMs,m_length)*1000);}
void SpotifyController::refreshMpris() {
    const auto names = QDBusConnection::sessionBus().interface()->registeredServiceNames().value();
    QString fallback, selected;
    for (const auto &name : names) {
        if (!name.startsWith("org.mpris.MediaPlayer2.")) continue;
        if (name == spotifyService) fallback = name;
        QDBusInterface candidate(name, path, "org.freedesktop.DBus.Properties", QDBusConnection::sessionBus());
        const auto identityReply = candidate.call("Get", "org.mpris.MediaPlayer2", "Identity");
        const auto identity = qvariant_cast<QDBusVariant>(identityReply.arguments().value(0)).variant().toString();
        // Plasma's MultiplexerModel tracks the same active MPRIS player as the
        // keyboard media controls. Prefer it over arbitrary D-Bus name order.
        if (!m_activeMprisIdentity.isEmpty() && identity.compare(m_activeMprisIdentity, Qt::CaseInsensitive) == 0) { selected = name; break; }
        const auto reply = candidate.call("Get", iface, "PlaybackStatus");
        if (qvariant_cast<QDBusVariant>(reply.arguments().value(0)).variant().toString() == "Playing") {
            // Keep the player that was already active while it is still playing.
            // This avoids jumping back to Spotify merely because D-Bus returned it first.
            if (name == m_service) { selected = name; break; }
            if (selected.isEmpty()) selected = name;
        }
    }
    if (selected.isEmpty()) selected = fallback;
    m_service = selected;
    QDBusInterface props(m_service,path,"org.freedesktop.DBus.Properties",QDBusConnection::sessionBus()); m_playerAvailable=props.isValid();
    if (!m_playerAvailable) { m_title=m_artist=m_album=m_artUrl=m_playerName=m_trackUri=m_trackId={}; m_playing=false;m_seekable=false; emit metadataChanged(); emit playbackChanged(); return; }
    const auto metadataReply = props.call("Get", iface, "Metadata");
    const auto stateReply = props.call("Get", iface, "PlaybackStatus");
    const auto positionReply = props.call("Get", iface, "Position");
    const auto seekReply = props.call("Get", iface, "CanSeek");
    const auto metadata = qdbus_cast<QVariantMap>(qvariant_cast<QDBusVariant>(metadataReply.arguments().value(0)).variant());
    const auto state = qvariant_cast<QDBusVariant>(stateReply.arguments().value(0)).variant().toString();
    m_position = qvariant_cast<QDBusVariant>(positionReply.arguments().value(0)).variant().toLongLong() / 1000;
    m_title=metadata.value("xesam:title").toString(); m_artist=metadata.value("xesam:artist").toStringList().join(", "); m_album=metadata.value("xesam:album").toString(); const auto metadataArt=metadata.value("mpris:artUrl").toString();m_artUrl=m_activeMprisArtUrl.isEmpty()?metadataArt:m_activeMprisArtUrl; m_playerName=m_service.section('.', 3).section('.', 0, 0); if(!m_playerName.isEmpty())m_playerName[0]=m_playerName[0].toUpper(); m_length=metadata.value("mpris:length").toLongLong()/1000; m_trackId=qvariant_cast<QDBusObjectPath>(metadata.value("mpris:trackid")).path();if(m_trackId.isEmpty())m_trackId=metadata.value("mpris:trackid").toString();m_seekable=qvariant_cast<QDBusVariant>(seekReply.arguments().value(0)).variant().toBool(); const auto old=m_trackUri; m_trackUri=spotifyUriFromMetadata(metadata); m_playing=(state=="Playing"); emit metadataChanged(); emit playbackChanged(); if (old!=m_trackUri) { m_likedPollTicks=0; requestLiked(); }
}
void SpotifyController::beginAuthorization() {
    if (m_clientId.isEmpty()) { setError("Set your Spotify Client ID in the widget settings first."); return; }
    if (!m_callbackServer.listen(QHostAddress::LocalHost,8888)) { setError("Port 8888 is in use; close the other authorization attempt."); return; }
    m_verifier=QString::fromLatin1(QCryptographicHash::hash(QByteArray::number(QRandomGenerator::global()->generate64()), QCryptographicHash::Sha256).toBase64(QByteArray::Base64UrlEncoding|QByteArray::OmitTrailingEquals)); const auto challenge=QString::fromLatin1(QCryptographicHash::hash(m_verifier.toUtf8(),QCryptographicHash::Sha256).toBase64(QByteArray::Base64UrlEncoding|QByteArray::OmitTrailingEquals)); m_oauthState=QString::fromLatin1(QCryptographicHash::hash(QByteArray::number(QRandomGenerator::global()->generate64()), QCryptographicHash::Sha256).toBase64(QByteArray::Base64UrlEncoding|QByteArray::OmitTrailingEquals));
    QUrlQuery q; q.addQueryItem("client_id",m_clientId); q.addQueryItem("response_type","code"); q.addQueryItem("redirect_uri","http://127.0.0.1:8888/callback"); q.addQueryItem("code_challenge_method","S256"); q.addQueryItem("code_challenge",challenge); q.addQueryItem("state",m_oauthState); q.addQueryItem("scope","user-library-read user-library-modify playlist-read-private playlist-read-collaborative playlist-modify-public playlist-modify-private"); QDesktopServices::openUrl(QUrl("https://accounts.spotify.com/authorize?"+q.toString(QUrl::FullyEncoded)));
}
void SpotifyController::api(const QString &method,const QUrl &url,const QJsonObject &body,std::function<void(QNetworkReply*)> done,bool retryAuth,int rateRetries) { if (m_accessToken.isEmpty()) { setError("Spotify is not connected. Open settings and select Connect Spotify."); return; } const auto now=QDateTime::currentDateTimeUtc();if(m_rateLimitUntil>now){const int wait=qMax(1,now.msecsTo(m_rateLimitUntil));const int slot=m_rateLimitQueued++;setStatus(QString("Spotify requests paused; %1 request(s) queued for %2 seconds").arg(m_rateLimitQueued).arg(rateLimitSeconds()));QTimer::singleShot(wait+slot*250,this,[this,method,url,body,done,retryAuth,rateRetries]{m_rateLimitQueued=qMax(0,m_rateLimitQueued-1);api(method,url,body,done,retryAuth,rateRetries);});return;} QNetworkRequest r(url); r.setRawHeader("Authorization", "Bearer "+m_accessToken.toUtf8()); r.setHeader(QNetworkRequest::ContentTypeHeader,"application/json"); const auto payload=body.isEmpty()?QByteArray{}:QJsonDocument(body).toJson(QJsonDocument::Compact); QNetworkReply *reply= method=="GET"?m_network.get(r):method=="PUT"?m_network.put(r,payload):method=="DELETE"?m_network.sendCustomRequest(r,"DELETE",payload):m_network.post(r,payload); connect(reply,&QNetworkReply::finished,this,[this,reply,done,retryAuth,rateRetries,method,url,body] { const int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(); if (status == 0 && reply->error() != QNetworkReply::NoError) setError(QString("Network request failed: %1. Check your internet connection and try again.").arg(reply->errorString())); if (status==401 && retryAuth) { refreshToken([this,reply,method,url,body,done,rateRetries](bool ok){ if(ok) { reply->deleteLater(); api(method,url,body,done,false,rateRetries); } else done(reply); }); return; } if (status==429 && rateRetries < 1) { const auto retryHeader=reply->rawHeader("Retry-After");const int seconds=retryHeader.isEmpty()?60:qMax(1,retryHeader.toInt());m_rateLimitUntil=QDateTime::currentDateTimeUtc().addSecs(seconds);QSettings s("KDE","SpotifyPlasmoid");s.setValue("rate-limit/"+m_clientId,m_rateLimitUntil);s.sync();emit rateLimitChanged();setStatus(QString("Rate limited; requests blocked for %1 seconds").arg(seconds));reply->deleteLater(); setError(QString("Spotify rate limit reached (HTTP 429). Requests are paused for %1 seconds.").arg(seconds)); QTimer::singleShot(seconds*1000,this,[this,method,url,body,done,retryAuth,rateRetries]{setStatus({});api(method,url,body,done,retryAuth,rateRetries+1);}); return; } done(reply); }); }
void SpotifyController::refreshToken(std::function<void(bool)> done) { if(m_refreshToken.isEmpty()){forgetAuthorization();setError("Spotify authorization expired. Please connect Spotify again.");done(false);return;} QUrlQuery f; f.addQueryItem("grant_type","refresh_token");f.addQueryItem("refresh_token",m_refreshToken);f.addQueryItem("client_id",m_clientId); QNetworkRequest r(QUrl("https://accounts.spotify.com/api/token"));r.setHeader(QNetworkRequest::ContentTypeHeader,"application/x-www-form-urlencoded"); auto *reply=m_network.post(r,f.toString(QUrl::FullyEncoded).toUtf8()); connect(reply,&QNetworkReply::finished,this,[this,reply,done]{const int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();auto o=QJsonDocument::fromJson(reply->readAll()).object();reply->deleteLater();if(o.contains("access_token")){m_accessToken=o["access_token"].toString();saveTokens();emit authorizationChanged();requestLiked();done(true);}else{forgetAuthorization();setError(QString("Spotify authorization expired (HTTP %1). Please connect Spotify again.").arg(status));done(false);}}); }
void SpotifyController::requestLiked(){ if(!spotifySource()||m_accessToken.isEmpty()) return; QUrlQuery q;q.addQueryItem("uris",m_trackUri); api("GET",QUrl("https://api.spotify.com/v1/me/library/contains?"+q.toString(QUrl::FullyEncoded)),{},[this](QNetworkReply *r){ const int s=r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();const auto response=r->readAll();r->deleteLater();if(s<200||s>=300){setError(QString("Could not check your library (HTTP %1): %2").arg(s).arg(QString::fromUtf8(response)));return;}const auto a=QJsonDocument::fromJson(response).array();const bool value=!a.isEmpty()&&a.first().toBool();if(m_liked!=value){m_liked=value;emit likedChanged();} }); }
void SpotifyController::toggleLiked(){ if(m_trackUri.isEmpty()||m_busy)return; const auto uri=m_trackUri;setError({});setBusy(true);QUrl url("https://api.spotify.com/v1/me/library"); QUrlQuery query; query.addQueryItem("uris",uri); url.setQuery(query); api(m_liked?"DELETE":"PUT",url,{},[this,uri](QNetworkReply *r){const int s=r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();const auto response=QString::fromUtf8(r->readAll());r->deleteLater();setBusy(false);if(s>=200&&s<300){QTimer::singleShot(600,this,[this,uri]{if(m_trackUri==uri)requestLiked();});QTimer::singleShot(3000,this,[this,uri]{if(m_trackUri==uri)requestLiked();});}else setError(QString("Could not update your library (HTTP %1): %2").arg(s).arg(response));}); }
void SpotifyController::loadPlaylists(bool force){ if (!force && m_playlistsExpiry > QDateTime::currentDateTimeUtc()) { setStatus("Using cached playlist list");emit playlistsLoaded(m_playlistCache); return; } m_nextPlaylistsOffset=-1;emit playlistsPaginationChanged();setStatus("Loading playlists");if(!m_userId.isEmpty()){fetchEditablePlaylists();return;} api("GET",QUrl("https://api.spotify.com/v1/me"),{},[this](QNetworkReply *r){const int s=r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();const auto response=r->readAll();r->deleteLater();if(s<200||s>=300){setError(QString("Could not identify the Spotify account (HTTP %1): %2").arg(s).arg(QString::fromUtf8(response)));return;}m_userId=QJsonDocument::fromJson(response).object()["id"].toString();if(m_userId.isEmpty()){setError("Could not identify the Spotify account.");return;}fetchEditablePlaylists();}); }
void SpotifyController::loadMorePlaylists(){if(m_nextPlaylistsOffset<0||m_busy)return;setStatus("Loading more playlists");fetchEditablePlaylists(m_nextPlaylistsOffset);}
void SpotifyController::fetchEditablePlaylists(int offset){QUrl url("https://api.spotify.com/v1/me/playlists");QUrlQuery query;query.addQueryItem("limit","50");query.addQueryItem("offset",QString::number(offset));url.setQuery(query);api("GET",url,{},[this,offset](QNetworkReply *r){const int s=r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();const auto response=r->readAll();r->deleteLater();if(s<200||s>=300){setError(QString("Could not load playlists (HTTP %1): %2").arg(s).arg(QString::fromUtf8(response)));return;}const auto o=QJsonDocument::fromJson(response).object();QVariantList page;for(const auto &v:o["items"].toArray()){const auto p=v.toObject();const bool editable=p["collaborative"].toBool()||p["owner"].toObject()["id"].toString()==m_userId;if(editable&&!p["id"].toString().isEmpty())page<<QVariantMap{{"id",p["id"].toString()},{"name",p["name"].toString()}};}if(offset==0)m_playlistCache=page;else m_playlistCache+=page;m_nextPlaylistsOffset=o["next"].isNull()||o["next"].toString().isEmpty()?-1:offset+50;emit playlistsPaginationChanged();if(m_playlistCache.isEmpty()){setError("No editable Spotify playlists were found.");return;}m_playlistsExpiry=QDateTime::currentDateTimeUtc().addDays(30);savePlaylistCache();setStatus(QString("Loaded %1 playlists").arg(m_playlistCache.size()));emit playlistsLoaded(m_playlistCache);}); }
void SpotifyController::addToPlaylist(const QString &id) { addToPlaylists({id}); }
void SpotifyController::addToPlaylists(const QStringList &ids)
{
    if (m_trackUri.isEmpty() || ids.isEmpty() || m_busy)
        return;
    m_playlistWriteQueue = ids;
    m_playlistWriteQueue.removeAll(QString());
    m_playlistWriteQueue.removeDuplicates();
    if (m_playlistWriteQueue.isEmpty())
        return;
    m_playlistWriteTrackUri = m_trackUri;
    m_playlistWritesFailed = 0;
    m_playlistWritesSkipped = 0;
    m_playlistWriteError.clear();
    setError({});
    setBusy(true);
    addNextToPlaylist();
}
void SpotifyController::addNextToPlaylist()
{
    if (m_playlistWriteQueue.isEmpty()) {
        setBusy(false);
        if (m_playlistWritesFailed)
            setError(QString("Could not add this item to %1 playlist(s): %2").arg(m_playlistWritesFailed).arg(m_playlistWriteError));
        else if (m_playlistWritesSkipped)
            setStatus(QString("Track was already present in %1 selected playlist(s)").arg(m_playlistWritesSkipped));
        else
            setStatus("Track added to selected playlists");
        return;
    }
    const auto id = m_playlistWriteQueue.takeFirst();
    checkPlaylistBeforeAdding(id);
}
void SpotifyController::checkPlaylistBeforeAdding(const QString &id, int offset)
{
    QUrl url("https://api.spotify.com/v1/playlists/" + id + "/items");
    QUrlQuery query;
    query.addQueryItem("limit", "50");
    query.addQueryItem("offset", QString::number(offset));
    query.addQueryItem("fields", "items(item(uri),track(uri)),next");
    url.setQuery(query);
    api("GET", url, {}, [this, id, offset](QNetworkReply *r) {
        const int status = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto response = r->readAll();
        r->deleteLater();
        if (status < 200 || status >= 300) {
            ++m_playlistWritesFailed;
            if (m_playlistWriteError.isEmpty())
                m_playlistWriteError = QString("HTTP %1: %2").arg(status).arg(QString::fromUtf8(response));
            addNextToPlaylist();
            return;
        }
        const auto object = QJsonDocument::fromJson(response).object();
        for (const auto &value : object["items"].toArray()) {
            const auto item = value.toObject();
            auto uri = item["item"].toObject()["uri"].toString();
            if (uri.isEmpty()) uri = item["track"].toObject()["uri"].toString();
            if (uri == m_playlistWriteTrackUri) { ++m_playlistWritesSkipped; addNextToPlaylist(); return; }
        }
        if (!object["next"].isNull() && !object["next"].toString().isEmpty()) {
            QTimer::singleShot(120, this, [this, id, offset] { checkPlaylistBeforeAdding(id, offset + 50); });
            return;
        }
    api("POST", QUrl("https://api.spotify.com/v1/playlists/" + id + "/items"),
        QJsonObject{{"uris", QJsonArray{m_playlistWriteTrackUri}}},
        [this](QNetworkReply *r) {
            const int status = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const auto response = QString::fromUtf8(r->readAll());
            r->deleteLater();
            if (status < 200 || status >= 300) {
                ++m_playlistWritesFailed;
                if (m_playlistWriteError.isEmpty())
                    m_playlistWriteError = QString("HTTP %1: %2").arg(status).arg(response);
            }
            addNextToPlaylist();
        });
    });
}
void SpotifyController::restoreTokens(){ if(m_clientId.isEmpty())return; auto *w=KWallet::Wallet::openWallet(KWallet::Wallet::NetworkWallet(),0,KWallet::Wallet::Synchronous);if(!w)return;w->setFolder("com.github.spotifyplasmoid");w->readPassword("refresh-token-"+m_clientId,m_refreshToken);w->readPassword("access-token-"+m_clientId,m_accessToken); if(!m_accessToken.isEmpty()){emit authorizationChanged();requestLiked();} }
void SpotifyController::saveTokens(){auto *w=KWallet::Wallet::openWallet(KWallet::Wallet::NetworkWallet(),0,KWallet::Wallet::Synchronous);if(!w)return;w->createFolder("com.github.spotifyplasmoid");w->setFolder("com.github.spotifyplasmoid");w->writePassword("refresh-token-"+m_clientId,m_refreshToken);w->writePassword("access-token-"+m_clientId,m_accessToken);}
void SpotifyController::clearTokens(){auto *w=KWallet::Wallet::openWallet(KWallet::Wallet::NetworkWallet(),0,KWallet::Wallet::Synchronous);if(!w)return;w->setFolder("com.github.spotifyplasmoid");w->removeEntry("refresh-token-"+m_clientId);w->removeEntry("access-token-"+m_clientId);}
void SpotifyController::forgetAuthorization(){clearTokens();m_accessToken.clear();m_refreshToken.clear();emit authorizationChanged();}
