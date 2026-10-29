# NZPortable Wii

An unofficial, community Nintendo Wii port of **Nazi Zombies: Portable**.
The original game is made by **NZP Team and its contributors**. This port is
based on their work; it is not an official NZP Team release.

- Original project: https://github.com/nzp-team/nzportable
- Vril engine: https://github.com/nzp-team/vril-engine
- FTEQW fork used by the online client: https://github.com/nzp-team/fteqw
- Upstream documentation: https://docs.nzp.gay/

## Current status

The single player port and the experimental FTE online client are separate executables.

**Online gameplay does not work yet.** The client can retrieve and display
servers and establish an initial connection, but joining a playable game is
still failing. 
## Downloads

Release version: **0.1**. Download updates from
[GitHub Releases](https://github.com/txmbrs/nzportable-wii/releases).
See [v0.1 release notes](docs/RELEASE-v0.1.md) for requirements and known issues.

The experimental online ZIP includes a Windows generator for a private TLS seed.
See [the seed/FTP instructions](release-tools/README.md). The seed is generated
locally for each console and is never included in a public download.

## Installation

The existing installation uses `apps/nzportable/boot.dol` for single player and
`apps/nzportable/online/client.dol` for the experimental online client on SD/USB.

## Source and building

- `source/`: Vril engine with Wii port changes.
- `fte-online/fteqw-master/`: FTEQW sources with Wii port changes.
- `build_wii.sh`: single player build entry point.
- `fte-online/build_wii_online.sh`: online build entry point.
- `tools/`: port and dependency integration helpers.

Builds require devkitPPC/libogc, Wii port libraries, SDL2/OpenGX and the port's
SDK integration changes. The scripts currently assume `/src` inside the build
container. A clean-machine build guide and dependency packaging still need
verification before the first public release. See [WII_PORT.md](WII_PORT.md)
and [the upstream build overview](docs/UPSTREAM_VRIL_README.md).

## Authors and licensing

See [CREDITS.md](CREDITS.md) for upstream attribution. Existing copyright and
license notices are retained. The engine includes GNU GPL v2 licensing;
individual source notices, third-party components, fonts and game assets keep
their respective terms. See [LICENSE](LICENSE) and
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Game data is not automatically
covered by the engine's license.
