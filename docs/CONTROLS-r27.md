# Wii control recovery and double forward-tap sprint r27

## Recovery from changed or missing bindings

Open Configuration > Controls > Restore Defaults. This restores and saves the
factory Wii button bindings, hold actions, sensitivity, deadzone, acceleration,
aim assist, inversion, analog mode and sprint gesture. It clears custom normal,
hold and double-tap button assignments first, so an old hold binding cannot
silently override a restored button. Held actions are released using the old
bindings before replacement. Video, audio, game progress and game data are not
reset. Factory controls are embedded in boot.dol: no replacement config is needed.
Restoring is explicit; installing this update does not discard custom controls.

Factory layout: A jumps, B changes stance, X/Minus reloads (hold to interact),
Y/1 switches weapon, left trigger/Nunchuk Z aims, right trigger/Wiimote B shoots,
Z/Nunchuk C throws a grenade. D-pad Up is an alternate sprint button, Down melee,
Left secondary grenade, Right scores. Start/Plus pauses. The existing factory
keyboard/mouse bindings are also restored.

## Gesture detection and display

The previous gesture required raw forward >=0.8 and centred both axes within
0.2. Wii raw readings are not guaranteed to cover the full [-1,1] range: the
GameCube path divides signed stick values by 100, and expansion calibration can
describe a range greater than physical travel. Hardware axis traces have not
been supplied, so this is a concrete sensitivity weakness, not a measured
explanation of this particular controller's readings.

The forward threshold is now 0.5, neutral is within 0.3 on each axis, and the
second push must reach the threshold within 600 ms of the first. Each push
must reach the threshold within 250 ms after leaving neutral. Backward motion
or strong sideways movement cancels the gesture. A single held push continues
walking and does not repeat sprint impulses. Menus, pause, death and aiming
still block the gesture. The original sprint command, stamina and stopping
rules are unchanged.

Configuration > Controls > Bindings now shows SPRINT: STICK UP x2 instead of
displaying an unrelated button or UNBOUND. Selecting the row toggles the
gesture; DISABLED reflects its saved setting. Normal button remapping for
the other actions remains available. A successful gesture emits one log line:
`Wii sprint: double forward tap detected; impulse 23 queued`.
This confirms detection and queuing, not server acceptance of the command.

## Validation

Wii cross-compilation and ASan/UBSan host regressions passed. The actual gesture
detector is tested with reduced travel (0.55/0.75), off-centre neutral, a 500 ms
double tap, single held walking, timeout, slow push, pause/menu-style blocking,
source changes, disconnect and timer wrap. The actual reset function is tested
against dirty normal/hold/double-tap bindings, held action release, factory
layout restoration, controls-only settings, immediate saving, repeated resets
and rejection of remote commands. SDK/game APIs are mocked in the host tests.

Wii runtime validation is still required. Replace apps/nzportable/boot.dol with
this update, restore controls using the menu, and test a single forward push
and then two forward pushes with a return to centre between them. Retains the
r26 bounded menu pictures. Online gameplay remains unfinished and unchanged.
