# Contributing

Thanks for contributing to Plasma Spotify Controls.

1. Keep Spotify credentials, tokens, KWallet exports, and personal playlist data out of commits.
2. Build and run the test suite before opening a pull request:

   ```sh
   cmake -S . -B build -DCMAKE_INSTALL_PREFIX=$HOME/.local
   cmake --build build
   ctest --test-dir build --output-on-failure
   ```

3. Test panel and desktop layouts on Plasma 6 when changing QML.
4. Preserve the MPRIS/Web API boundary: playback belongs to MPRIS; Spotify library and playlist mutations belong to the Web API.
