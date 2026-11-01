# r18ba Wii log assessment and r18bb output correction

The submitted Wii log reaches ICE connected, authenticated DTLS, sound/model
lists, local package cache hits, world load and prespawn. It does not reach
spawn/begin. The selected connection is /611; the earlier /489 attempt never
reaches that same stage and should not be conflated with it.

Observed /611 sequence (milliseconds from the client's clock):

- 203886: ICE connected; then pinned peer DTLS authentication succeeds.
- 208xxx: model list complete (251 entries), deferred map loading begins.
- 219942: deferred world load returned after 11457 ms.
- 224602: sound checks returned after 4629 ms.
- 226926: model checks returned after 2324 ms.
- 242651: game CSQC initialization returned after 15370 ms.
- 248997/249006: prespawn queued/serialized (9 ms between these steps).
- 249026: Netchan transmits a reliable packet containing pending commands.
- 302673: timeout after 60 seconds without an accepted game packet.

During the stall, SCTP SACK RX and DTLS packet counters keep increasing, while
SCTP DATA RX stays at 49 and the client's game sequence remains 49. Transport
traffic therefore continues; it is not proof that the server's game channel
accepted prespawn. There is no evidence here for the next spawn/begin stage.

Both game.pk3.bbf391a3 (94276834 bytes) and progs.pk3.bbbb2131 (491379 bytes)
are cache hits. CSQC checksum 787660fe is verified locally. No repeated package
download is demonstrated in this attempt. Memory is tight (around 2.2 MB free
before world precache), but this log does not terminate with allocation failure.

## Confirmed remaining output cost

The log explicitly measures one 413-byte message at file=0 ms, stdout=156 ms.
Although r18ba buffered FAT logging, Sys_Printf still decoded colors/characters,
called putc per glyph and flushed libogc stdout after the game screen was active.
That path was not covered by the earlier memory-log fix.

r18bb keeps WiiOnline_Log before an early runtime return in Sys_Printf. It avoids
hidden-console character conversion/output once the startup menu is ready.
Startup and fatal console output are retained. This changes no Netchan, SCTP,
DTLS, checksum or timeout policy. It is a targeted correction for an observed
cost, not a claimed fix for the missing prespawn response or for all CSQC time.

Host ASan/UBSan test runs the actual Sys_Printf: 1000 runtime messages retained
with zero console parse/color calls; startup/fatal mode still reaches output.
Wii cross-compilation passes with existing warnings. Runtime improvement and
successful joining remain unverified on Wii.

## Required evidence for the remaining game-channel failure

Obtain the server console/log for the same /611 attempt if available. Check
whether it received prespawn, rejected an earlier command/checksum/stage, or
dropped the client during loading. A server-side timeout is currently a
hypothesis, not a conclusion from the client log alone. Do not reintroduce
blind reliable retransmission based only on the absence of a game reply.

Install only apps/nzportable/online/client.dol from the r18bb update. Keep the
current boot.dol, assets, dlcache and private TLS seed. Exit the client normally
after testing to flush its RAM log; a forced reset can lose its latest messages.
