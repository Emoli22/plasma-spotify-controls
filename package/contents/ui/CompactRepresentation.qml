import QtQuick
import QtQuick.Layouts
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.components as PlasmaComponents
import org.kde.kirigami as Kirigami

Item {
    id: compact
    required property var controller
    required property var plasmoidItem
    readonly property bool horizontal: plasmoid.formFactor === PlasmaCore.Types.Horizontal
    readonly property bool roomy: horizontal && width >= 140
    Layout.minimumWidth: Kirigami.Units.iconSizes.medium
    Layout.minimumHeight: Kirigami.Units.iconSizes.medium
    Layout.preferredWidth: horizontal ? 270 : Kirigami.Units.iconSizes.medium
    Layout.preferredHeight: Kirigami.Units.iconSizes.medium

    TapHandler { onTapped: compact.plasmoidItem.expanded = true }
    RowLayout {
        anchors.fill: parent; spacing: Kirigami.Units.smallSpacing
        Item { Layout.preferredWidth: Kirigami.Units.iconSizes.medium; Layout.preferredHeight: width
            Image { anchors.fill: parent; source: controller.artUrl; visible: controller.artUrl.length > 0; fillMode: Image.PreserveAspectCrop; asynchronous: true; cache: true; sourceSize.width: 96; sourceSize.height: 96; Accessible.name: i18n("Album or video artwork") }
            Kirigami.Icon { anchors.fill: parent; visible: controller.artUrl.length === 0; source: controller.spotifySource ? "spotify-companion" : "media-playback-start" }
        }
        ColumnLayout {
            visible: compact.roomy
            Layout.preferredWidth: 145
            Layout.maximumWidth: 145
            Layout.alignment: Qt.AlignVCenter
            spacing: 0
            PlasmaComponents.Label { Layout.fillWidth: true; text: controller.title || i18n("Media player"); elide: Text.ElideRight; font.bold: true }
            PlasmaComponents.Label { Layout.fillWidth: true; text: controller.artist || controller.playerName; elide: Text.ElideRight; opacity: 0.7 }
        }
        PlasmaComponents.ToolButton { visible: compact.roomy && controller.spotifySource && controller.authorized; enabled: !controller.busy; icon.name: controller.liked ? "rating" : "rating-unrated"; Accessible.name: controller.liked ? i18n("Remove from Liked Songs") : i18n("Add to Liked Songs"); onClicked: controller.toggleLiked() }
        PlasmaComponents.ToolButton { visible: compact.roomy && controller.spotifySource && controller.authorized; enabled: !controller.busy; icon.name: "view-media-playlist"; Accessible.name: i18n("Add current track to playlists"); onClicked: { controller.loadPlaylists(); compact.plasmoidItem.expanded = true } }
    }
}
