import QtQuick
import org.kde.plasma.configuration

ConfigModel {
    ConfigCategory {
        name: i18n("Plasma Spotify Controls")
        icon: "spotify-companion"
        source: "configGeneral.qml"
    }
    ConfigCategory {
        name: i18n("Diagnostics")
        icon: "dialog-information"
        source: "configDiagnostics.qml"
    }
}
