# Single player r28 and cooperation r18ba repair candidate

This update follows the 2026-11-01 source audit. Both targets cross-compile;
host regressions pass. A fresh official PC reference session reaches a playable
server. The new Wii binaries have NOT been runtime-tested on a Wii. Online
gameplay remains experimental and is not claimed to work yet.

## Online transport

- Remove the added blind 400 ms reliable resend. A delivered request with a
  missing ACK must not be reissued with a new packet sequence. Keep the normal
  Netchan ACK-progress loss detection; existing empty loading keepalives probe
  the peer without duplicating a reliable command.
- A bounded, immutable 16-datagram FIFO owns accepted outgoing DTLS packets.
  Retries use the same bytes, length and buffer address required by mbedTLS.
  Receiving and periodic timeout processing drain the FIFO as well as sends.
  Acceptance returns SENT even if the packet waits for socket writability;
  a full queue rejects the NEW packet without overwriting accepted data.
- On Wii, rejected SCTP DATA/DCEP sends do not consume a TSN. Restore a pending
  SACK when its final send is rejected. These rejection semantics are scoped
  to the Wii provider rather than imposed on other DTLS implementations.
- Game-layer MTU is 1000; DTLS datagram MTU remains 1200. This reserves room for
  SCTP headers and encryption instead of treating both layers' 1200 limits as
  interchangeable. The provider still checks its negotiated maximum payload.
- Keep the corrected package/FS_ROOT existence lookup and multi-chunk SCTP
  parsing from the reviewed working tree. Existence is not a pure-hash check;
  normal filesystem/pure validation must still succeed.
- Broaden the existing resource-purge path to failed loading from ca_connected,
  before a fully accepted server state. References are detached before purge.
  This is not proof that all memory pressure or fragmentation has disappeared.
- Avoid calling net_deinit while an asynchronous init is still busy, and avoid
  a two-second retry sleep when less than that budget remains. libogc calls
  themselves are not guaranteed bounded; no claim of a full Wi-Fi driver reset.

## Allocation, graphics and controls

- Image resampling uses a fallible, overflow-checked workspace allocation.
  Allocation failure returns to the existing caller path that retains original
  pixels/dimensions, rather than exiting inside BZ_Malloc before a NULL check.
  This makes that workspace optional; it is not unlimited texture memory.
- The lighting hash multiplies unsigned values, avoiding signed-overflow UB.
  Cache generations invalidate on every lighting frame and map start; cached
  samples can be reused within the frame without retaining stale lightstyles
  or map data across frames/reloads.
- Restore exact animation interpolation and remove the two-decals-per-update
  cap. Retain batching/VBO optimizations without rounding animation or silently
  discarding extra blood/burn effects. Retains the r26 two-picture menu cache.
- Preserve the startup input-registration fix. USB keyboard/mouse processing
  is no longer discarded wholesale. Native controller events are filtered;
  mouse-button events overlapping native button activity are suppressed to
  avoid capturing the same physical button twice. IR motion does not change
  the binding device. USB clicks simultaneous with native buttons are therefore
  intentionally suppressed; hardware validation of mixed inputs remains needed.
- Binding swaps preserve full command strings and move normal/hold/double-tap
  assignments together. Assigning an initially UNBOUND action to an occupied
  button is rejected with a visible request to choose another button. ESC/Start
  cancel. All three binding setter types mark configuration dirty, and setting
  an identical binding is a no-op (also safe when passed its existing pointer).

## Logging

Both Wii targets retain runtime logs in a 64 KiB memory ring instead of writing
FAT during every message. Overflow discards oldest bytes and records a count.
Bootstrap logging is synchronous so early startup diagnostics still reach disk.
Runtime buffers flush on normal exit/fatal error. Single player also provides
`wii_dump_log` for an explicit flush and `wii_perf 1` for CPU phase profiling.
Exit the online client normally after a failed join to save its buffered log.
A forced hardware reset can lose the RAM-only tail; this is an explicit tradeoff.

## Executed tests

- Actual DTLS transmit/tick with mocked mbedTLS: repeated WANT_WRITE, immutable
  buffer retries, FIFO order, full queue rejection, ring wrap, MTU, handshake
  wait and fatal failure. More than 2000 accepted transmissions checked.
- Production Netchan resend predicate in lost-request/lost-ACK/both scenarios
  with peer ACK probes. These are controlled protocol tests, not packet-loss
  injection into a running Wii or full FTE client.
- Actual image allocation branches with allocation failure and retained input;
  actual lighting-cache code under ASan/UBSan; bounded log ring overflow/drain.
- Actual menu binding/setter code: occupied action preservation, commands longer
  than 64 bytes, compound binding swap, exact command matching, cancel, autosave.
- Actual IN_Init reaches Wii cvar and command registration before pad polling.
- Model test repaired and run with static vertex budget both enabled/disabled:
  2148 + 2150 draws, exact positions, UV/cache limits, fallback and cleanup.
- Sprint, default-control reset, menu-picture cache, MEM2 wrappers, lightmap
  conversion and VBO upload regression tests pass. SDK/engine dependencies are
  mocked where necessary. Existing compiler warnings remain; cross-build success
  does not mean a warning-free build or hardware validation.

## Fresh PC reference

Isolated local server/client under build/reference/repair-r18ba, official legacy
PC binaries, ports 30953/30954, ICE/DTLS/SCTP, NDU. Fresh generated identities
initially failed Windows SChannel. Retrying with the existing identities from
the previously working reference succeeded, without changing verification policy.

Both logs show on 2026-11-01:

- 15:00:48: `prespawn 1 0 -990240093`
- 15:00:50: `spawn 1`
- 15:00:51: `begin 1`
- 15:00:51: `RepairReferenceRTC has joined the game.`

Evidence and log/asset hashes are saved in
build/reference/repair-r18ba/connection-results.json. Only processes started for
this test were stopped afterward. No private identity/seed files are distributed.
The PC run uses the official PC transport, not the Wii mbedTLS provider; it is
a reference sequence, NOT proof that Wii passed the same stages.

## Install and verify on Wii

Extract nzportable into apps, replacing boot.dol and online/client.dol. Existing
game data, dlcache, configuration and private TLS seed stay in place.

Check single player first: startup, Restore Defaults if needed, double forward
tap sprint, NDU/Warehouse visuals and memory stability. Then connect online once
and exit normally so wii-online-startup.txt is committed. Compare its stages with
the PC reference; CONNECTED or a queued command alone is not a successful join.

No 60 FPS or successful Wii online session is claimed. Location-specific FPS
work still needs Wii CPU-phase and present/GPU-wait measurements at the same
camera position. The published v0.1 full archive is not automatically replaced
with this unverified candidate.
