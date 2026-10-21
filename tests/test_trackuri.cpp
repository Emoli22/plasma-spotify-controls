#include <QtTest>
#include "spotifycontroller.h"

class TrackUriTest : public QObject {
    Q_OBJECT
private slots:
    void acceptsSpotifyTrack() { QCOMPARE(SpotifyController::spotifyUriFromMetadata({{"xesam:url", "spotify:track:abc"}}), "spotify:track:abc"); }
    void acceptsSpotifyWebTrack() { QCOMPARE(SpotifyController::spotifyUriFromMetadata({{"xesam:url", "https://open.spotify.com/track/abc?si=123"}}), "spotify:track:abc"); }
    void acceptsSpotifyWebEpisode() { QCOMPARE(SpotifyController::spotifyUriFromMetadata({{"xesam:url", "https://open.spotify.com/episode/abc"}}), "spotify:episode:abc"); }
    void rejectsExternalUrls() { QVERIFY(SpotifyController::spotifyUriFromMetadata({{"xesam:url", "https://example.com/track/abc"}}).isEmpty()); }
};
QTEST_GUILESS_MAIN(TrackUriTest)
#include "test_trackuri.moc"
