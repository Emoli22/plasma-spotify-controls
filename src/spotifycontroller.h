#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QJsonObject>
#include <QDateTime>
#include <QTcpServer>
#include <QTimer>
#include <QStringList>

class SpotifyController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString clientId READ clientId WRITE setClientId NOTIFY clientIdChanged)
    Q_PROPERTY(bool playerAvailable READ playerAvailable NOTIFY playbackChanged)
    Q_PROPERTY(QString title READ title NOTIFY metadataChanged)
    Q_PROPERTY(QString artist READ artist NOTIFY metadataChanged)
    Q_PROPERTY(QString album READ album NOTIFY metadataChanged)
    Q_PROPERTY(QString artUrl READ artUrl NOTIFY metadataChanged)
    Q_PROPERTY(QString playerName READ playerName NOTIFY metadataChanged)
    Q_PROPERTY(QString trackUri READ trackUri NOTIFY metadataChanged)
    Q_PROPERTY(qint64 length READ length NOTIFY metadataChanged)
    Q_PROPERTY(qint64 position READ position NOTIFY playbackChanged)
    Q_PROPERTY(bool seekable READ seekable NOTIFY playbackChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playbackChanged)
    Q_PROPERTY(bool liked READ liked NOTIFY likedChanged)
    Q_PROPERTY(bool spotifySource READ spotifySource NOTIFY metadataChanged)
    Q_PROPERTY(bool authorized READ authorized NOTIFY authorizationChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(int rateLimitSeconds READ rateLimitSeconds NOTIFY rateLimitChanged)
    Q_PROPERTY(QVariantList errorHistory READ errorHistory NOTIFY errorHistoryChanged)
    Q_PROPERTY(bool morePlaylistsAvailable READ morePlaylistsAvailable NOTIFY playlistsPaginationChanged)
    Q_PROPERTY(qulonglong maintenanceRevision READ maintenanceRevision WRITE setMaintenanceRevision)
    Q_PROPERTY(QString maintenanceAction READ maintenanceAction WRITE setMaintenanceAction)
    Q_PROPERTY(QString activeMprisIdentity READ activeMprisIdentity WRITE setActiveMprisIdentity)
    Q_PROPERTY(QString activeMprisArtUrl READ activeMprisArtUrl WRITE setActiveMprisArtUrl)
public:
    explicit SpotifyController(QObject *parent = nullptr);
    static QString spotifyUriFromMetadata(const QVariantMap &metadata);
    QString clientId() const { return m_clientId; }
    void setClientId(const QString &value);
    bool playerAvailable() const { return m_playerAvailable; }
    QString title() const { return m_title; }
    QString artist() const { return m_artist; }
    QString album() const { return m_album; }
    QString artUrl() const { return m_artUrl; }
    QString playerName() const { return m_playerName; }
    QString trackUri() const { return m_trackUri; }
    qint64 length() const { return m_length; }
    qint64 position() const { return m_position; }
    bool seekable() const { return m_seekable; }
    bool playing() const { return m_playing; }
    bool liked() const { return m_liked; }
    bool spotifySource() const { return m_service == QStringLiteral("org.mpris.MediaPlayer2.spotify") && !m_trackUri.isEmpty(); }
    bool authorized() const { return !m_accessToken.isEmpty(); }
    bool busy() const { return m_busy; }
    QString error() const { return m_error; }
    QString status() const { return m_status; }
    int rateLimitSeconds() const;
    QVariantList errorHistory() const { return m_errorHistory; }
    bool morePlaylistsAvailable() const { return m_nextPlaylistsOffset >= 0; }
    qulonglong maintenanceRevision() const { return m_maintenanceRevision; }
    QString maintenanceAction() const { return m_maintenanceAction; }
    QString activeMprisIdentity() const { return m_activeMprisIdentity; }
    QString activeMprisArtUrl() const { return m_activeMprisArtUrl; }
    Q_INVOKABLE void playPause();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void seek(qint64 positionMs);
    Q_INVOKABLE void beginAuthorization();
    Q_INVOKABLE void forgetAuthorization();
    Q_INVOKABLE void toggleLiked();
    Q_INVOKABLE void loadPlaylists(bool force = false);
    Q_INVOKABLE void loadMorePlaylists();
    Q_INVOKABLE void openCurrentTrack();
    Q_INVOKABLE void clearErrorHistory();
    Q_INVOKABLE void clearCachedPlaylists();
    Q_INVOKABLE void resetConnectionData();
    Q_INVOKABLE void addToPlaylist(const QString &playlistId);
    Q_INVOKABLE void addToPlaylists(const QStringList &playlistIds);
    void setMaintenanceRevision(qulonglong value);
    void setMaintenanceAction(const QString &value);
    void setActiveMprisIdentity(const QString &value);
    void setActiveMprisArtUrl(const QString &value);
signals:
    void clientIdChanged(); void metadataChanged(); void playbackChanged();
    void likedChanged(); void authorizationChanged(); void busyChanged(); void errorChanged(); void statusChanged(); void rateLimitChanged(); void errorHistoryChanged(); void playlistsPaginationChanged();
    void playlistsLoaded(const QVariantList &playlists);
private:
    void setError(const QString &message); void setBusy(bool value); void setStatus(const QString &message); void restorePlaylistCache(); void savePlaylistCache(); void clearPlaylistCache(); void restoreErrorHistory(); void saveErrorHistory();
    void callMpris(const QString &method); void requestLiked();
    void api(const QString &method, const QUrl &url, const QJsonObject &body,
             std::function<void(QNetworkReply *)> complete, bool retryAuth = true, int rateRetries = 0);
    void refreshToken(std::function<void(bool)> complete);
    void restoreTokens(); void saveTokens(); void clearTokens(); void addNextToPlaylist(); void fetchEditablePlaylists(int offset = 0);
    void checkPlaylistBeforeAdding(const QString &playlistId, int offset = 0);
    QString m_clientId, m_accessToken, m_refreshToken, m_title, m_artist, m_album, m_artUrl, m_playerName, m_trackUri, m_trackId, m_error, m_status, m_verifier, m_oauthState, m_service, m_userId, m_maintenanceAction, m_activeMprisIdentity, m_activeMprisArtUrl;
    qint64 m_length = 0, m_position = 0;
    bool m_playerAvailable = false, m_playing = false, m_seekable = false, m_liked = false, m_busy = false;
    QNetworkAccessManager m_network;
    QTcpServer m_callbackServer;
    QTimer m_positionTimer;
    QVariantList m_playlistCache;
    QDateTime m_playlistsExpiry;
    QDateTime m_rateLimitUntil;
    int m_rateLimitQueued = 0;
    QVariantList m_errorHistory;
    QStringList m_playlistWriteQueue;
    QString m_playlistWriteTrackUri;
    QString m_playlistWriteError;
    int m_playlistWritesSkipped = 0;
    int m_nextPlaylistsOffset = -1;
    int m_likedPollTicks = 0;
    int m_playlistWritesFailed = 0;
    qulonglong m_maintenanceRevision = 0;
private slots:
    void refreshMpris();
};
