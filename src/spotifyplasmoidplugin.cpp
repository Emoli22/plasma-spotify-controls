#include "spotifyplasmoidplugin.h"
#include "spotifycontroller.h"
#include <qqml.h>

void SpotifyPlasmoidPlugin::registerTypes(const char *uri)
{
    qmlRegisterType<SpotifyController>(uri, 1, 0, "SpotifyController");
}
