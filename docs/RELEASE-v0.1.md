# NZPortable Wii v0.1

First public development release of an unofficial Nintendo Wii port of
[Nazi Zombies: Portable](https://github.com/nzp-team/nzportable), originally
created by NZP Team and its contributors. See CREDITS.md and the retained licenses.

## Status

**Online gameplay does not work yet.** Server browsing and the initial connection
work, but entry into a playable game is not complete and may time out during
loading. The online client is provided for development/testing only.

The single player executable and online client are separate. PC reference tests
are not a confirmation that online works on Wii. No claim of stable 60 FPS or
fully tested hardware compatibility is made by this release.

## Downloads

- `NZPortable-Wii-v0.1-single-player-update.zip`: single player executable
  (internal build r20) and Homebrew metadata for an existing Wii installation.
- `NZPortable-Wii-v0.1-online-experimental-update.zip`: online executable
  (internal build r18ax), Windows seed generator, instructions and licenses.
- `NZPortable-Wii-v0.1-source.zip`: port sources, build/integration helpers,
  dependency source archives and license/attribution notices.
- `SHA256SUMS.txt`: checksums for the downloadable archives.

These executable ZIPs are **updates for an existing installation**, not a full
standalone game download. Keep your existing `nzp` game data and online support
files (`compat.pk3`, configuration, certificates and program packages). The
online update replaces only `online/client.dol`; it contains no game packages,
settings or private TLS seed. New installations need those support files first.

## FTP installation

1. Fully exit the game before replacing an executable.
2. Extract the appropriate update ZIP on your PC. Upload its `apps` contents over
   FTP to the corresponding SD/USB folders. Keep the folder name `nzportable`.
3. For a console without an existing TLS seed, run `Generate-TLS-Seed.cmd` on
   Windows and upload the generated `apps/nzportable/online/tls-seed.bin` in binary
   mode. Preserve an existing seed. Generate a separate seed for each Wii.
4. Launch from Homebrew Channel. The online option remains experimental.

ELF/debug files do not need to be uploaded. Build/setup documentation is still
being improved; a clean-machine build has not yet been independently verified.
