# Private TLS seed generator

The full release contains a `nzportable` folder ready to copy into `apps`.
Single player does not require a TLS seed.

For experimental online startup on a new console, open the extracted
`nzportable` folder on Windows and double-click `Generate-TLS-Seed.cmd` before
copying it. Python is not required. The tool creates the private 64-byte file
`online/tls-seed.bin`. It preserves an existing valid seed.

Use a separate seed for each Wii. Do not share that file or put it on GitHub.
The release archive contains no private seed. Keep the folder and seed writable.
Do not replace an existing seed when updating.

The generator does not make online gameplay work. Server browsing and the
initial connection work, but entry into a playable game remains incomplete.
