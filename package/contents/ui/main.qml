import QtQuick
import QtQuick.Layouts
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.private.mpris as Mpris
import "../code/spotifybridge" as SpotifyBridge

PlasmoidItem {
    id: root
    SpotifyBridge.SpotifyController {
        id: spotify
        clientId: plasmoid.configuration.spotifyClientId
        maintenanceAction: plasmoid.configuration.maintenanceAction
        maintenanceRevision: plasmoid.configuration.maintenanceRevision
    }

    // Plasma uses this multiplexer for its own media controls. Feeding its
    // active identity to the controller makes our card follow the same source
    // as the hardware play/pause keys, rather than D-Bus service-name order.
    Repeater {
        model: Mpris.MultiplexerModel { }
        delegate: Item {
            visible: false
            property string activeIdentity: model.identity || ""
            property string activeArtwork: model.artUrl || ""
            Component.onCompleted: { spotify.activeMprisIdentity = activeIdentity; spotify.activeMprisArtUrl = activeArtwork }
            onActiveIdentityChanged: spotify.activeMprisIdentity = activeIdentity
            onActiveArtworkChanged: spotify.activeMprisArtUrl = activeArtwork
        }
    }

    toolTipMainText: spotify.playerAvailable ? spotify.title : i18n("No media player is active")
    toolTipSubText: spotify.playerAvailable ? spotify.artist + (spotify.playerName ? " · " + spotify.playerName : "") : i18n("Start Spotify, YouTube, or another MPRIS-compatible player")
    // Planar containments show the full card; panels use Plasma's normal
    // compact-to-popup behavior when expanded is toggled.
    preferredRepresentation: Plasmoid.formFactor === PlasmaCore.Types.Planar ? fullRepresentation : null

    compactRepresentation: CompactRepresentation { controller: spotify; plasmoidItem: root }
    fullRepresentation: FullRepresentation { controller: spotify }
}
