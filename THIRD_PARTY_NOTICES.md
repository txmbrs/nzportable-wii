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
| Game assets | `docs/LICENSE-NZP-ASSETS.md`, `nzp/licenses` in the full app; CC BY-SA 4.0 and listed component attributions. https://github.com/nzp-team/assets |
| QuakeC | https://github.com/nzp-team/quakec; retain its terms and credits. |

The engine license does not grant a blanket license to bundled music, artwork,
trademarks or other game assets. The full app preserves data and dependency notices, including the original
asset attribution list and notices for Wii font/HUD changes. Public source publication must
include the port changes and required build/integration sources, and exclude
private TLS seeds, logs, configuration and credentials.
