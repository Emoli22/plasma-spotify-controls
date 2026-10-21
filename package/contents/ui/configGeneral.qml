import QtQuick
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents
import org.kde.kirigami as Kirigami
import "../code/spotifybridge" as SpotifyBridge

Kirigami.FormLayout {
    property alias cfg_spotifyClientId: clientId.text
    property alias cfg_maintenanceAction: maintenanceAction.text
    property alias cfg_maintenanceRevision: maintenanceRevision.text
    SpotifyBridge.SpotifyController { id: maintenanceController; clientId: clientId.text }
    QtObject { id: maintenanceAction; property string text: "" }
    QtObject { id: maintenanceRevision; property string text: "0" }
    PlasmaComponents.TextField {
        id: clientId
        Kirigami.FormData.label: i18n("Spotify Client ID:")
        placeholderText: i18n("Paste the Client ID from Spotify Developer Dashboard")
        Layout.fillWidth: true
    }
    PlasmaComponents.Label {
        Kirigami.FormData.label: i18n("Redirect URI:")
        text: "http://127.0.0.1:8888/callback"
        wrapMode: Text.Wrap
        Layout.fillWidth: true
    }
    PlasmaComponents.Label {
        Kirigami.FormData.label: i18n("Connection maintenance:")
        text: i18n("These actions affect only this widget's local Spotify data. They do not change your Spotify library or playlists.")
        wrapMode: Text.Wrap
        Layout.fillWidth: true
    }
    RowLayout {
        Layout.fillWidth: true
        PlasmaComponents.Button {
            text: i18n("Clear playlist cache")
            icon.name: "edit-clear-history"
            Accessible.name: i18n("Clear cached playlist metadata")
            onClicked: { maintenanceAction.text = "clear-playlist-cache"; maintenanceRevision.text = Date.now().toString(); maintenanceController.clearCachedPlaylists() }
        }
        PlasmaComponents.Button {
            text: i18n("Reset Spotify connection")
            icon.name: "edit-delete"
            Accessible.name: i18n("Remove saved Spotify authorization and cache")
            onClicked: { maintenanceAction.text = "reset-connection"; maintenanceRevision.text = Date.now().toString(); maintenanceController.resetConnectionData() }
        }
    }
}
