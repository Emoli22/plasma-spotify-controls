import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents
import org.kde.kirigami as Kirigami
import "../code/spotifybridge" as SpotifyBridge

Kirigami.ScrollablePage {
    id: page
    title: i18n("Diagnostics")
    SpotifyBridge.SpotifyController {
        id: diagnostics
        clientId: plasmoid.configuration.spotifyClientId
    }
    ColumnLayout {
        width: page.width
        spacing: Kirigami.Units.smallSpacing
        PlasmaComponents.Label {
            Layout.fillWidth: true
            text: i18n("Saved API and connection errors")
            font.bold: true
        }
        PlasmaComponents.Label {
            Layout.fillWidth: true
            text: diagnostics.errorHistory.length === 0 ? i18n("No saved errors for this Spotify Client ID.") : i18n("The most recent 20 errors are kept locally. Tokens and Spotify content are never written here.")
            wrapMode: Text.WordWrap
            opacity: 0.7
        }
        QQC2.Frame {
            Layout.fillWidth: true
            Layout.preferredHeight: 260
            contentItem: ListView {
                id: errorList
                clip: true
                model: diagnostics.errorHistory
                spacing: Kirigami.Units.smallSpacing
                delegate: PlasmaComponents.Label {
                    width: errorList.width
                    text: modelData.time + "\n" + modelData.message
                    wrapMode: Text.WordWrap
                    textFormat: Text.PlainText
                    padding: Kirigami.Units.smallSpacing
                }
                QQC2.ScrollBar.vertical: QQC2.ScrollBar { }
            }
        }
        PlasmaComponents.Button {
            text: i18n("Clear saved error log")
            icon.name: "edit-clear-history"
            Accessible.name: i18n("Clear saved error log")
            enabled: diagnostics.errorHistory.length > 0
            onClicked: diagnostics.clearErrorHistory()
        }
        PlasmaComponents.Label {
            Layout.fillWidth: true
            text: diagnostics.error.length > 0 ? diagnostics.error : i18n("Current status: %1").arg(diagnostics.status || i18n("Ready"))
            color: diagnostics.error.length > 0 ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.neutralTextColor
            wrapMode: Text.WordWrap
        }
    }
}
