# r18bc: cooperative transport servicing during CSQC startup

The r18ba Wii log measures CSQC initialization at 15370 ms and reports a
16522 ms age for the most recent DTLS input immediately afterward. Existing
keepalive calls cover frame-loader boundaries and several resource builtins,
but not long QuakeC instruction execution or all bytecode normalization loops.

## Change

- Add a Wii CLIENTONLY cooperative checkpoint to the interpreter dispatch path
  shared by 16-bit and 32-bit instructions, once per 4096 dispatch iterations.
- Add checkpoints every 256 entries in relevant bytecode-loader conversion
  loops. No public qclib ABI/struct layout changes. Non-Wii/non-client builds
  compile the helper to a no-op with no external callback dependency.
- Gate the engine callback by the exact game CSQC VM and connected/loading
  state. Menu VMs, unrelated VMs and normal active gameplay do not pump the
  transport through this callback.
- Reuse the existing loading keepalive's main-thread, busy/reentry and game-
  parser ownership guards. DTLS/SCTP may be serviced and received game packets
  queued, but the game parser is not re-entered from the VM checkpoint.
  Actual pump/transmit timing remains bounded by the existing 100 ms / 1 s
  cadence; no blind reliable retransmission or timeout extension is introduced.
- Measure VM configure, world clear, field setup, bytecode load, module init
  and CSQC_Init callback separately with `PERF CSQC` lines. Keep r18bb's removal
  of hidden stdout rendering during runtime and preserve buffered diagnostic
  logs. Initial bootstrap and fatal output remain available.

## Validation

Wii cross-compilation passes, including both interpreter implementations and
the loader. ASan/UBSan host test runs the production checkpoint macro, exact
VM gate and loading keepalive against a mocked network/clock: long CPU work,
periodic servicing, untouched arithmetic result, wrong/menu VM rejection,
active-game rejection, worker-thread rejection, parser-depth protection and
recursive transport callback rejection. A separate no-Wii compile confirms
the checkpoint header introduces no engine callback dependency there.

The test does not execute the actual CSQC bytecode or a live Wii connection;
there is no claim of a guaranteed 100 ms wall-clock maximum between network
operations. A blocking filesystem call/builtin still cannot be interrupted by
an instruction checkpoint. New phase timing will distinguish that case.

This corrects a gap in the client's cooperative network servicing. It does NOT
prove that the server accepted prespawn or that this gap caused the recorded
failure. The r18ba trace still stops after prespawn while SCTP control traffic
continues. Server-side rejection/timeout requires corresponding server evidence.

## Install/test

Replace only apps/nzportable/online/client.dol. Keep current boot.dol, assets,
dlcache and private seed. Try joining once and exit normally to persist the RAM
log. Compare PERF CSQC phases, LOAD KEEPALIVE activity during startup, DTLS input
age and progression from prespawn to spawn/begin. Hardware performance and
successful online gameplay remain unverified. A forced reset can lose the
latest RAM-only log tail.
