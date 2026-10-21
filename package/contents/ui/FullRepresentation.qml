import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kirigami as Kirigami

PlasmaExtras.Representation {
    id: view
    required property var controller
    Layout.minimumWidth: 320
    Layout.minimumHeight: 400
    Layout.preferredWidth: 380
    Layout.preferredHeight: 500
    property var playlists: []
    property var visiblePlaylists: []
    property var selectedPlaylistIds: []
    property string playlistSearch: ""
    property bool playlistSortAscending: true
    function rebuildVisiblePlaylists() {
        const needle = playlistSearch.toLocaleLowerCase()
        const items = playlists.filter(function(item) { return item.name.toLocaleLowerCase().indexOf(needle) !== -1 })
        items.sort(function(a, b) {
            const comparison = a.name.localeCompare(b.name)
            return playlistSortAscending ? comparison : -comparison
        })
        visiblePlaylists = items
    }
    function setPlaylistSelected(id, selected) {
        const ids = selectedPlaylistIds.slice()
        const index = ids.indexOf(id)
        if (selected && index === -1) ids.push(id)
        else if (!selected && index !== -1) ids.splice(index, 1)
        selectedPlaylistIds = ids
    }
    Component.onCompleted: {
        if (controller.spotifySource && controller.authorized)
            controller.loadPlaylists()
    }
    Connections {
        target: controller
        function onPlaylistsLoaded(items) {
            view.playlists = items
            view.selectedPlaylistIds = []
            view.rebuildVisiblePlaylists()
        }
    }

    header: PlasmaExtras.BasicPlasmoidHeading {}

    contentItem: ColumnLayout {
        spacing: Kirigami.Units.smallSpacing
        Item { Layout.fillWidth: true; Layout.preferredHeight: 105
            Image { anchors.centerIn: parent; width: 98; height: 98; source: controller.artUrl; fillMode: Image.PreserveAspectCrop; visible: controller.artUrl.length > 0; asynchronous: true; cache: true; sourceSize.width: 196; sourceSize.height: 196; Accessible.name: i18n("Album or video artwork") }
            Kirigami.Icon { anchors.centerIn: parent; width: 72; height: 72; source: controller.spotifySource ? "spotify-companion" : "media-playback-start"; visible: !controller.artUrl }
        }
        ColumnLayout { Layout.fillWidth: true
            PlasmaComponents.Label { Layout.fillWidth: true; text: controller.title || i18n("No media player is active"); elide: Text.ElideRight; wrapMode: Text.NoWrap; font.bold: true; horizontalAlignment: Text.AlignHCenter }
            PlasmaComponents.Label { Layout.fillWidth: true; text: controller.artist; elide: Text.ElideRight; horizontalAlignment: Text.AlignHCenter; opacity: 0.7 }
            PlasmaComponents.Label { Layout.fillWidth: true; visible: controller.playerName.length > 0; text: controller.playerName; elide: Text.ElideRight; horizontalAlignment: Text.AlignHCenter; opacity: 0.55; font.pointSize: 8 }
        }
        QQC2.Slider {
            Layout.fillWidth: true
            from: 0
            to: Math.max(1, controller.length)
            value: controller.position
            enabled: controller.seekable && controller.length > 0
            live: true
            Accessible.name: i18n("Playback position")
            onMoved: controller.seek(value)
        }
        RowLayout { Layout.alignment: Qt.AlignHCenter
            PlasmaComponents.ToolButton { icon.name: "media-skip-backward"; Accessible.name: i18n("Previous track"); enabled: controller.playerAvailable; onClicked: controller.previous() }
            PlasmaComponents.ToolButton { icon.name: controller.playing ? "media-playback-pause" : "media-playback-start"; Accessible.name: controller.playing ? i18n("Pause") : i18n("Play"); enabled: controller.playerAvailable; onClicked: controller.playPause() }
            PlasmaComponents.ToolButton { icon.name: "media-skip-forward"; Accessible.name: i18n("Next track"); enabled: controller.playerAvailable; onClicked: controller.next() }
            PlasmaComponents.ToolButton { visible: controller.spotifySource; icon.name: controller.liked ? "rating" : "rating-unrated"; Accessible.name: controller.liked ? i18n("Remove from Liked Songs") : i18n("Add to Liked Songs"); enabled: controller.authorized && !controller.busy; onClicked: controller.toggleLiked() }
            PlasmaComponents.ToolButton { visible: controller.spotifySource; icon.name: "spotify"; text: i18n("Open in Spotify"); Accessible.name: i18n("Open current track in Spotify"); onClicked: controller.openCurrentTrack() }
        }
        ColumnLayout {
            visible: controller.spotifySource && controller.authorized
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing
            RowLayout {
                Layout.fillWidth: true
                PlasmaComponents.Label { text: i18n("Add this track to playlists"); font.bold: true; Layout.fillWidth: true }
                PlasmaComponents.ToolButton { icon.name: "view-refresh"; text: i18n("Refresh now"); enabled: !controller.busy; onClicked: controller.loadPlaylists(true) }
            }
            RowLayout {
                Layout.fillWidth: true
                QQC2.TextField {
                    Layout.fillWidth: true
                    placeholderText: i18n("Search playlists")
                    Accessible.name: i18n("Search playlists")
                    text: view.playlistSearch
                    onTextChanged: { view.playlistSearch = text; view.rebuildVisiblePlaylists() }
                }
                PlasmaComponents.ToolButton {
                    icon.name: view.playlistSortAscending ? "view-sort-ascending" : "view-sort-descending"
                    text: i18n("Sort")
                    Accessible.name: i18n("Reverse playlist sort order")
                    onClicked: { view.playlistSortAscending = !view.playlistSortAscending; view.rebuildVisiblePlaylists() }
                }
            }
            PlasmaComponents.Label {
                visible: view.playlists.length === 0 && !controller.busy
                text: i18n("Loading playlists…")
                opacity: 0.7
            }
            PlasmaComponents.Label { visible: controller.status.length > 0; text: controller.status; opacity: 0.7; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            PlasmaComponents.Label { visible: controller.rateLimitSeconds > 0; text: i18n("Spotify rate limit: retry available in %1 seconds").arg(controller.rateLimitSeconds); color: Kirigami.Theme.negativeTextColor; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            QQC2.Frame {
                visible: view.playlists.length > 0
                Layout.fillWidth: true
                Layout.preferredHeight: 115
                contentItem: ListView {
                    id: playlistView
                    clip: true
                model: view.visiblePlaylists
                    delegate: QQC2.CheckBox {
                        width: playlistView.width
                        text: modelData.name
                    checked: view.selectedPlaylistIds.indexOf(modelData.id) !== -1
                    Accessible.name: i18n("Select playlist %1").arg(modelData.name)
                    enabled: !controller.busy
                    onToggled: view.setPlaylistSelected(modelData.id, checked)
                    }
                    QQC2.ScrollBar.vertical: QQC2.ScrollBar { }
                }
            }
            QQC2.Button {
                Layout.alignment: Qt.AlignHCenter
                visible: controller.morePlaylistsAvailable
                text: i18n("Load more playlists")
                enabled: !controller.busy
                onClicked: controller.loadMorePlaylists()
            }
            QQC2.Button {
                Layout.alignment: Qt.AlignRight
                text: i18n("Add to selected (%1)").arg(view.selectedPlaylistIds.length)
                enabled: view.selectedPlaylistIds.length > 0 && !controller.busy
                onClicked: { controller.addToPlaylists(view.selectedPlaylistIds); view.selectedPlaylistIds = [] }
            }
            PlasmaComponents.Label {
                visible: controller.error.length > 0
                Layout.fillWidth: true
                text: i18n("Spotify API error:\n%1").arg(controller.error)
                color: Kirigami.Theme.negativeTextColor
                wrapMode: Text.Wrap
            }
            ColumnLayout {
                visible: controller.errorHistory.length > 0
                Layout.fillWidth: true
                RowLayout {
                    Layout.fillWidth: true
                    PlasmaComponents.Label { text: i18n("Recent API errors"); font.bold: true; Layout.fillWidth: true }
                    QQC2.Button { text: i18n("Clear"); onClicked: controller.clearErrorHistory() }
                }
                Repeater {
                    model: controller.errorHistory
                    PlasmaComponents.Label {
                        visible: index < 3
                        Layout.fillWidth: true
                        text: modelData.time + " — " + modelData.message
                        opacity: 0.7
                        wrapMode: Text.Wrap
                    }
                }
            }
        }
        PlasmaComponents.Button {
            Layout.alignment: Qt.AlignHCenter
            icon.name: "configure"
            text: i18n("Spotify settings")
            onClicked: Plasmoid.internalAction("configure").trigger()
        }
        PlasmaComponents.Button { Layout.alignment: Qt.AlignHCenter; visible: !controller.authorized; text: i18n("Connect Spotify"); Accessible.name: i18n("Connect Spotify account"); onClicked: controller.beginAuthorization() }
    }
}
