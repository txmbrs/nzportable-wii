# Cooperation r18 - compiled experimental integration

COOPERATIVE in the existing Vril menu checks the FTE module and required files,
saves settings and shuts down the old engine at the outer frame boundary.
FTE opens the server browser; ordinary quit returns to Solo and HOME exits.
The user explicitly approved the volatile RAM DOL handoff. No NAND or firmware
writes are performed. Startup and online gameplay remain unverified on hardware.

## Update and shared game data

Extract NZPortable-Wii-cooperation-r18-update.zip on Windows and run
Install-Cooperation.cmd with the existing SD/USB installation connected.
The installer verifies existing shared data and payload hashes, backs up boot.dol,
adds only the module and differences, and preserves existing settings.

1051 byte-identical web files are reused from apps/nzportable/nzp. 101 different
or missing files are stored in online/compat.pk3 (about 55 MB compressed, mostly
alternate WAV music plus changed maps/HUD assets). Original loose files and
progs.dat are not replaced. FTE loads basegame nzp, then private basegame *online.
That overlay takes priority, while the public network game directory stays nzp.
Its configuration writes go to online, independently from Solo.

    apps/nzportable/boot.dol           Vril Solo/menu, r18
    apps/nzportable/nzp/               existing game files
    apps/nzportable/online/client.dol  FTE Cooperation
    apps/nzportable/online/compat.pk3  differences only
    apps/nzportable/online/progs.pk3   official web menu/CSQC
    apps/nzportable/online/wii-online.cfg
    apps/nzportable/online/cacert.pem
    apps/nzportable/online/tls-seed.bin  private, generated during installation

The Windows installer uses the OS CSPRNG to provision a unique private 64-byte
TLS seed on the card. No private seed is included in update/source archives.
Do not share the generated seed between installations.

## Sources and implementation

FTE: NZP Team archived revision f68a2b547d2dc4bf6886b922baa1bff487cc5038,
GPL-2.0-or-later. Corresponding sources and Mbed TLS source archive are supplied
separately as NZPortable-Wii-cooperation-r18-source.zip. Official web assets:
https://nzp.gay/nzp/game.pk3 and https://nzp.gay/nzp/progs.pk3.

Wii additions provide IOS IPv4 UDP/TCP, OpenGX resolution, MEM2 allocation,
Wii/Nunchuk/Classic/GameCube input and logging. CSQC, server browser,
ICE/SCTP/WebRTC, TLS 1.2/WSS and DTLS 1.2 are compiled. TLS verifies CA and
hostname; WebRTC DTLS checks the signalling fingerprint. The profile uses
NZP-REBOOT-WEB and NZP-REBOOT and the frag-net TLS broker.

Mbed TLS 3.6.7 uses its documented NV-seed entropy mechanism with default DRBG
reseeding. Missing seeds or failed persistence stop secure-network startup:
https://mbed-tls.readthedocs.io/en/latest/kb/how-to/add-entropy-sources-to-entropy-pool/

The loader accepts only bounded non-overlapping MEM1 DOL sections, valid BSS
and a text entry. A maximum 16 MiB image is staged in MEM2. A freestanding MEM2
stub with its own stack copies sections, clears BSS, passes libogc argv and
synchronizes data/instruction caches. No allocation/storage calls occur after
fixed-address staging. The FTE heap excludes the argv/stack reservation.

Build from repository root inside devkitpro/devkitppc:

    NZP_WII_BUILD_DIR=build/wii-startup-r3 bash build_wii.sh
    bash fte-online/build_wii_online.sh
    python3 fte-online/package_cooperation.py

## Validation boundary

Both engines compile and link. Data reuse, hashes, DOL memory bounds, stub
entry address and installer syntax are inspected statically. No game, emulator,
automated test or live server connection is run. Startup, OpenGX rendering,
controller navigation, peak RAM and joining actual nzp.gay players still require
hardware validation. This is an experimental integrated build, not verified
working online gameplay. Logs: wii-startup.txt and online/wii-online-startup.txt.
