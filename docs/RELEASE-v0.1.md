# NZPortable Wii v0.1

An unofficial Nintendo Wii port of [Nazi Zombies: Portable](https://github.com/nzp-team/nzportable),
originally created by NZP Team and its contributors.

## Installation

1. Download `NZPortable-Wii-v0.1-full.zip` and extract it.
2. Copy the included `nzportable` folder directly into `apps` on your SD/USB.
3. Launch NZPortable Wii from Homebrew Channel.

The package includes single player, game data, maps, the experimental online
client and its support files. No existing game installation is required.
Keep your existing settings, saves and private seed when updating.

## Online status

**Online gameplay does not work yet.** Server browsing and initial connections
work, but the client cannot yet complete entry into a playable game and may time
out during loading. A Connected message is not confirmation of working gameplay.

Single player does not need a TLS seed. For experimental online startup on a new
console, run `Generate-TLS-Seed.cmd` inside the extracted `nzportable` folder on
Windows before copying it. This creates a private file in `online`. A separate
seed is needed for each Wii; existing valid seeds are preserved.

## Downloads

- `NZPortable-Wii-v0.1-full.zip`: complete `nzportable` app folder.
- `NZPortable-Wii-v0.1-source.zip`: port and integration sources.
- `SHA256SUMS.txt`: archive checksums.

Release version 0.1 includes single player build r20 and online build r18ax.
PC reference connection tests do not establish that online works on Wii. No
claim of stable 60 FPS or full hardware compatibility is made.

## Credits and licenses

Original game: NZP Team and contributors. Wii port: txmbrs.
Game assets use their upstream CC BY-SA 4.0 terms and component attributions;
modified asset contributions retain the appropriate license. Engine sources,
fonts and dependencies retain their own license notices. See the included
`licenses` and `nzp/licenses` folders, CREDITS.md and THIRD_PARTY_NOTICES.md.
