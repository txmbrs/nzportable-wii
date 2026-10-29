# Third-party notices

Preserve the original notices in source files and dependencies when copying,
modifying or redistributing this port. This overview does not replace them.

| Component | Local notice / upstream |
| --- | --- |
| Vril / Quake-derived engine | `LICENSE`, individual source headers; https://github.com/nzp-team/vril-engine |
| FTEQW fork | `fte-online/fteqw-master/LICENSE` and individual component notices; https://github.com/nzp-team/fteqw |
| Mbed TLS 3.6.7 | `LICENSE` inside `fte-online/dependencies/mbedtls-3.6.7.tar.bz2` (Apache-2.0 OR GPL-2.0-or-later as stated upstream) |
| Font | `tools/fonts/FONT-LICENSE.txt` (SIL Open Font License 1.1) |
| libogc, SDL2, OpenGX and linked libraries | Preserve the exact SDK/component notices and corresponding modifications when distributing builds. |
| Game data / QuakeC | https://github.com/nzp-team/assets and https://github.com/nzp-team/quakec; retain their own terms and credits. |

The engine license does not grant a blanket license to bundled music, artwork,
trademarks or other game assets. Review data and dependency notices separately
before uploading a full binary/game-data release. Public source publication must
include the port changes and required build/integration sources, and exclude
private TLS seeds, logs, configuration and credentials.
