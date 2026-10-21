# Plasma Spotify Controls

Plasma 6 widget that follows the currently playing MPRIS media player. It shows track/video title, artist and artwork for Spotify, browsers such as YouTube, and other MPRIS-compatible players. Playback controls always target that active player. Spotify-only actions (Liked Songs and Add to playlist) appear only when the active item is a Spotify track.

![Plasma Spotify Controls icon](assets/spotify-companion.svg)

## Requirements

- KDE Plasma 6 and Qt 6.5+
- KDE Frameworks 6 Wallet (KWallet)
- CMake 3.21+, a C++20 compiler, and Qt 6 development packages: Core, DBus, Gui, Network, Qml, Test
- `gettext` / `msgfmt` when building the included Turkish translation
- Spotify desktop client with MPRIS enabled, for Spotify controls

For Spotify Web API features you also need your own Spotify Developer application and a Spotify account. A client secret is never used or stored.

## Build and install

```sh
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=$HOME/.local
cmake --build build
ctest --test-dir build --output-on-failure
cmake --install build
```

Restart Plasma (`kquitapp6 plasmashell && kstart plasmashell`) or log out and back in. Then add **Plasma Spotify Controls** from Plasma's widget picker.

## First Spotify connection

1. Create an app in the [Spotify Developer Dashboard](https://developer.spotify.com/dashboard).
2. Add `http://127.0.0.1:8888/callback` to its Redirect URIs.
3. Open the widget's **Configure** window, enter the app's **Client ID**, and apply the setting. Do not enter a client secret.
4. Open the widget and select **Connect Spotify**. Browser authorization uses PKCE and OAuth state validation. Access and refresh tokens are stored in KWallet under `com.github.spotifyplasmoid`.

The app requests only the library and playlist scopes required for its visible Spotify actions. Spotify’s MPRIS metadata must contain a Spotify track URI for Liked Songs and playlist actions to be enabled.

## Settings and diagnostics

- **Clear playlist cache** removes only local playlist metadata. The next explicit **Refresh now** downloads it again.
- **Reset Spotify connection** removes the local KWallet authorization and playlist cache; it never changes Spotify library data.
- The **Diagnostics** settings page keeps the latest 20 local API/connection errors for the configured Client ID. It contains no tokens or media data.

## Network behaviour

Playlist metadata is cached locally for 30 days and never refreshes merely because the track changes. Additional playlist pages load only after **Load more playlists**. Searching and sorting are local operations. Playlist content is inspected only when the user explicitly adds a track, so duplicates are not added. Album/video artwork is loaded asynchronously and cached by Qt.

On HTTP 429 the widget blocks further Spotify requests until `Retry-After` expires. This state survives a widget/Plasma restart. Spotify may still impose application-wide limits; restarting the widget does not reset them.

## Uninstall

```sh
rm -rf "$HOME/.local/share/plasma/plasmoids/com.github.spotifyplasmoid"
```

Removing the widget files does not remove KWallet tokens. Use **Reset Spotify connection** first if you want to revoke this local authorization, or remove the two `*-<Client ID>` entries from the `com.github.spotifyplasmoid` KWallet folder.

## Development

```sh
ctest --test-dir build --output-on-failure
```

To contribute translations, edit `po/tr/spotifyplasmoid.po`; English is the source language. The install target compiles the Turkish catalogue with `msgfmt`.

## Publishing to GitHub

The repository includes a `.gitignore`, contribution guide, license declaration,
and a GitHub Actions build/test workflow. Create an empty GitHub repository,
then run the following from this directory with your own remote URL:

```sh
git init
git add .
git commit -m "Initial Plasma Spotify Controls release"
git branch -M main
git remote add origin https://github.com/YOUR-ACCOUNT/plasma-spotify-controls.git
git push -u origin main
```

Do not commit KWallet exports, Spotify tokens, a Spotify client secret, or a
personal playlist cache. The widget requires only the public Spotify Client ID.

## License

GPL-3.0-or-later. See [LICENSE](LICENSE).
