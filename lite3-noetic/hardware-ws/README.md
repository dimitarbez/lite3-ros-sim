# Lite3 hardware workspace

This is a separate catkin workspace for the perception computer. It does not
modify or overlay the vendor `~/lite_cog` workspace or `qnx2ros`. The normal
`lite3-noetic` simulation image does not launch this package.

The legacy diagnostic package starts fail closed:

- `transmit_enabled: false` and `dynamic_actions_enabled: false`;
- all nine posture amplitudes and rates are zero;
- its posture and dynamic-action transmit flags are false;
- STOP-preemption is unverified and all action trajectories are empty.

Consequently neither legacy posture nor action output can arm from the
checked-in configuration. The separate official runtime uses the reviewed
Deeprcs `2.0.153` layout and an independent lease; its default commissioned
allowlist contains only the already proven neutral profile.

## One-time fail-closed installation

Connect the development computer to the robot Wi-Fi, keep the robot sitting, and
run the installer from the wrapper root:

```bash
make -C lite3-noetic setup-emotion-hardware
```

The installer verifies the observed host identities and interfaces, copies this
package into the separate `~/emotion_bot_lite3_hw_ws` workspace, builds it
against ROS Noetic and the existing `message_transformer` package, and installs
three narrowly scoped systemd services. It never copies into or edits
`~/lite_cog`, `jy_exe`, or `network.toml`.

Only the perception telemetry service and motion-host STOP observer receive
`CAP_NET_RAW`; the ROS core remains unprivileged. A generated HMAC key
authenticates the STOP/status relay and is installed outside Git under
`/etc/emotion-bot`. Staged copies of that key are removed after installation.

## Architecture and interfaces

- `emotion_receiver.py` binds only `127.0.0.1:8767`, limits frames to 2 KiB,
  validates transport schema 1.0 and emotion schema 1.1, and rejects replayed
  sequences within a session.
- `telemetry_tap.py` accepts only UDP from `192.168.1.120` to port `43897`,
  decodes the documented 200-byte `0x0901`, and puts 368-byte `0x0906` bodies in
  a versioned sequence-locked shared-memory file only after reviewed layout and
  value validation.
- `motion_stop_observer.py` runs without ROS on the motion host, passively watches
  `p2p0`, and sends only authenticated STOP/axis observations to the perception
  host on UDP `43910`. It never targets the robot command port and never changes
  `jy_exe`.
- `stop_relay_receiver.py` accepts frames only from
  `192.168.1.120:43911`, verifies their HMAC/session/sequence, and publishes the
  local ROS STOP and Retroid-health topics. A stale or lost relay removes the
  arming gate.
- The mapper, supervisor, posture bridge, and action controller are excluded
  from the official core launch. The explicit `hardware_diagnostic.launch`
  retains them fail closed under `/emotion_bot/hardware/...`; `/simple_cmd` is
  that diagnostic graph's sole posture path.
- The passive telemetry tap publishes each perception-to-motion-host UDP command
  observed on `eth0` as inspectable JSON on
  `/emotion_bot/hardware/wire_command`. This topic proves interface-level packet
  emission; it does not by itself prove that `jy_exe` accepted or executed the
  command. The exact 240-byte official MotionSDK joint command `0x0111` is the
  sole exception: it runs at 1 kHz and is consumed without ROS publication so
  diagnostic serialization cannot starve the shared-memory `0x0906` safety feed.
- The deployed vendor `/ros2qnx` subscriber has a queue depth of one. The posture
  bridge therefore serializes heartbeat, Pose-mode, and commissioned-axis
  messages and emits only axes with nonzero commissioned limits/rates. Sending
  four back-to-back axis messages lets zero-valued uncommissioned axes evict the
  active height command before the vendor callback runs.

Diagnostic services are `/emotion_bot/hardware/set_armed` (`std_srvs/SetBool`),
`/emotion_bot/hardware/set_dynamic_actions_enabled` (`std_srvs/SetBool`), and
`/emotion_bot/hardware/neutral` (`std_srvs/Trigger`). Status is latched JSON on
`/emotion_bot/hardware/status`; robot state is the typed
`emotion_bot_lite3_hw/RobotState` message.

The hardware neutral profile is static by default. An optional, disabled-by-
default height-only neutral breather reproduces the Gazebo profile's 0.25-second
quintic entrance and 1.375-second idle cadence without enabling attitude,
locomotion, or dynamic actions. It may be enabled only in a temporary reviewed
commissioning configuration. The posture bridge independently expires stale
posture intentions to exact zero using `posture.intent_timeout`, so a failed
mapper cannot leave a nonzero target active while the AI link remains fresh.

### Retroid-compatible posture framing

A 2026-09-19 direct diagnostic finally produced measured body motion after
matching the full Lite3 app's captured session and right-stick framing. From the
development computer's Windows interface `192.168.2.28:43897` to motion host
`192.168.2.1:43893`, the successful order was:

1. four heartbeat commands (`0x21040001`) at 2 Hz;
2. one Move command (`0x21010D06`), then a 2.0-second wait;
3. one Pose command (`0x21010D05`), then a 1.5-second wait;
4. at 50 Hz, a paired `0x21010135` value `32768` immediately followed by the
   `0x21010102` height value; and
5. on every exit path, five pairs of yaw `0` followed by height `0`, 20 ms apart.

The `32768` value is an observed Retroid companion value, outside the documented
signed yaw posture range. Treat it as opaque compatibility framing: do not scale
it, interpret it as a requested yaw angle, or generalize it to another firmware.
The diagnostic host-side bridge owns this exact handshake only when
`run-emotion-hardware-retroid-diagnostic` is explicitly active. It refuses to
coexist with an official or legacy direct-joint ownership marker, starts the
separate fail-closed diagnostic graph, and acts only while the received emotion
is `neutral` and every read-only gate remains fresh. A 250 ms relay watchdog and
every normal exit send five paired yaw/height zeros.

Phase-isolated testing showed that the earlier `+10000..-3900` waveform spent
its periodic range inside the deployed controller's effective deadband; its
`0.2544403 rad` whole-run span included the Move-to-Pose transition and was not
proof of a continuing breath. A later fixed `-10000` trial reset measurement
after that handshake and measured `0.2484894 rad` joint span and `0.0078653` IMU
orientation-component span during height actuation. The default neutral cycle
therefore ramps smoothly between `0` and `-10000`, never exceeding 50% of the
documented `20000` magnitude, at most `4000 units/s` with a 10-second period.

The operator explicitly selected a 25% runtime floor; output stops below it and
resumes only after all gates hold continuously for two seconds. The Pro manual's
75% start recommendation remains documented separately.

The older action coordinator still expects a separately configured
`~sdk_sender_path`, but it is now diagnostic-only and absent from the official
launch. Do not configure it with the official expression executable: that
would create a second owner and violate the continuous-session design.

A 2026-09-19 hoisted direct-joint commissioning attempt additionally proved
that `Sender::ControlGet(SDK)` (`0x0114`) is not a seamless handoff from the
vendor standing controller. A measured-position hold and a 10%-scale neutral
request both produced large acquisition transients before tracking-error release.
`direct_joint.takeover_transition_commissioned` therefore remains false and is
an independent hard gate even when a launch supplies `transmit_enabled=true`.
Do not bypass it to command small offsets from a vendor-controlled standing
pose. Direct-joint work now requires a reviewed vendor-supported zero-to-stand
SDK transition before expression trajectories can be commissioned.

The official `motion_sdk_expression_runner` uses the commissioned
two-stage feedback watchdog for that zero-to-stand path. A `0x0906` age above
100 ms freezes trajectory time and repeatedly sends the last validated joint
positions with zero desired velocity and torque. It does not advance the stand
or breathing profile while feedback is stale. Motion resumes only after 20
distinct finite feedback ticks arrive at no more than 20 ms age. If feedback
reaches 250 ms, or if values or tracking are invalid, the runner still executes
the vendor return-to-robot sequence and never retries automatically. This
bounded hold addresses measured 100-166 ms passive-tap scheduling gaps without
turning a dead stream into an unbounded blind stand; a persistent fault can
still make the vendor release sequence lower the robot.

The commissioned animal-like neutral cycle remains a deliberately slower physical
translation of Gazebo's two-sided height/roll/pitch loop. It takes 0.75 seconds
from exact neutral to an expanded endpoint, 1.25 seconds through the compressed
endpoint, and 1.25 seconds back to exact neutral. Base HipY compression ranges
from `-0.008 rad` extension to `+0.020 rad` compression; knee motion is twice
that scalar. Opposing left/right `0.004 rad` and front/rear `0.003 rad` biases
produce bounded roll and pitch while all four feet remain planted. The combined
per-leg HipY scalar remains inside `-0.015..+0.027 rad`, HipX stays fixed, and
yaw remains zero because Gazebo neutral also commands zero yaw. Every segment is
quintic with zero velocity at its endpoints and the 3.25-second loop seam.

The passive tap also publishes each validated raw `0x0901` body through the
sequence-locked `/dev/shm/emotion_bot_lite3_robot_state` record. The standalone
runner checks this independent safety channel throughout initialization,
standing, holding, and breathing. Basic state `8` (lose-control protection) or
any nonzero error flags immediately stop trajectory advancement and enter the
same no-retry MotionSDK release path. As with the high-rate reader, a transient
sequence-lock collision retains the last fully validated state record rather
than being misclassified as a safety fault.

The legacy hard gate is deployed on the perception computer and defaults false
in both configuration and the bridge executable. The 2026-09-20 offline gate
covers a clean Release build, 29 Python tests, four ROS-independent C++ suites,
exact packet codes, exclusive sender ownership, graceful release, independent
crash-watchdog release, and launch enumeration. A live full-amplitude symmetric
run completed 19 five-second cycles before fresh state `8` triggered the
intended no-retry return-to-robot path. A subsequent animal-like run completed
at least 27 3.25-second cycles and remained active at the documented handoff;
four measured telemetry gaps paused and recovered without a tracking or state
fault. See `docs/HARDWARE_APP_CONTROL.md` for the complete dated attempt history
and measurements.

Validated emotion state is copied into
`/dev/shm/emotion_bot_lite3_emotion_state` with the state-1.1 fields plus the
transport session/sequence and monotonic receive time. The ROS-independent
runner is its only actuator consumer. Runner status is copied back through
`/dev/shm/emotion_bot_lite3_expression_status` and published read-only as JSON
schema `1.0` on `/emotion_bot/hardware/expression_status`.

All nine planted fallback profiles are compiled and tested, but the runtime
allowlist is `neutral` by default. Set `HARDWARE_COMMISSIONED_EMOTIONS` only to
categories with complete physical evidence. Ordinary planted category switches
return to exact stand for 1.5 seconds and hold it for 0.35 seconds; rapid changes
replace one pending target. The allowlisted Anger path instead uses its bounded
lifted-paw state machine and completes paw landing/re-latch before that same
neutral contract. A planted front-shoulder experiment did not read visually as
front-paw stomping and was reverted; joy retains its previously accepted
stage-one profile. True airborne hops and forceful strikes remain unimplemented.
The live SDK-owned feedback reports zero in the vendor contact array, so lifted
paw gates use the separate guarded torque-derived estimate below.

The runner now also computes a **read-only torque-derived foot-load estimate**
from the joint positions and torques in the same official SDK feedback. It uses
the maintained Lite3 leg Jacobian and solves `J(q)^T F = tau`; it does not use
the all-zero vendor contact array. During the exact-stand hold it collects at
least 50 distinct samples and accepts a baseline only when every foot and the
total load are plausible and stable. The additive `estimated_contact` object in
`/emotion_bot/hardware/expression_status` exposes validity, baseline validity,
support count, total and per-foot vertical load, and the per-foot baseline.
`motion_gate_enabled` is deliberately `false`: this diagnostic cannot yet
authorize a lift or landing. Its deterministic test reproduces a recorded
standing sample at `23.44/24.34/31.23/35.73 N` (total `114.74 N`, versus
`116.15 N` configured static weight) and exercises fail-closed invalid,
singular, weak-baseline, unload, and landing cases. See
[`TICKET_JOY_FRONT_PAW.md`](../../tickets/TICKET_JOY_FRONT_PAW.md) for the
separate calibration and lifted-paw movement plan.

The Anger candidate is isolated behind explicit commissioning flags and is not
part of the normal category allowlist. `HARDWARE_ANGER_SINGLE_STOMP_TEST=true`
runs five seconds of accepted neutral breathing followed by one 35 mm
front-paw controlled placement. `HARDWARE_ANGER_SUITE_TEST=true` runs the full
left/right sequence, and `HARDWARE_ANGER_FIRST_PAW=left|right` selects its
order. Each placement uses a 0.35-second quintic lowering and a 0.25-second
stationary landing dwell. The commissioned implementation then returns shift,
brace, and stance width to canonical stand over 1.0 second and holds it for
0.35 seconds. The unchanged four-support gate is evaluated after that support
reset, and the second placement cannot start before it passes. See
[`TICKET_ANGER.md`](../../tickets/TICKET_ANGER.md). The 2026-09-20 bounded
single-placement and complete alternating physical suites both passed with
confirmed unload and four-foot landing, no safety fault, and clean release.
The operator subsequently reported that the motion looked good, providing
visual acceptance. The source now has an offline-tested Anger selection
and newest-request retarget path through 1.5 seconds of recovery plus a
0.35-second exact-neutral hold. Normal chat selection remains disabled pending
physical validation of that chat-driven selection and retarget path.

Commissioning suite repetition is explicitly capped at three with
`HARDWARE_ANGER_SUITE_CYCLES=1..3`. The first 15-second run passed two complete
cycles, then failed closed on cycle 3 because the final right landing restored
only three estimated supports after its dwell. Recovery and SDK release still
completed without a safety fault. Do not weaken the four-support gate or retry
the repeated run without reviewing that support redistribution. The positive
operator visual verdict does not override this failed telemetry gate.

The first source fix did not lower any load threshold: it added a 0.30-second
planted x/y recenter before the gate. It clean-built against the actual aarch64
MotionSDK, passed all seven tests, and was installed as SHA-256
`37793e3eb35f7dbc8e99bbd0d21619a74b0a52558572dea315e387c85169b3ee`.
Its authorized live repeat passed cycle 1, then failed closed on cycle 2's final
right landing at 13.066 N because support count remained three. It recovered to
four loaded feet, released with no safety fault, and ended in state `1/0/0`.

The replacement uses the full canonical support reset described above and logs
all four landing forces. It passed the seven local and aarch64 native tests and
the authorized three-cycle physical suite. Its installed SHA-256 is
`a77433ace5afb56a4bbd204df28c167cf7d46022d2b3155568086ac78fdd630c`.
The 722-sample baseline measured 121.912 N; all six placements restored four
supports. Cycle 1 left/right unload-to-landing values were
`3.901/29.767 N` and `1.386/29.509 N`; cycle 2 values were
`1.849/26.950 N` and `0.466/29.667 N`; cycle 3 values were
`2.085/26.949 N` and `0.160/29.563 N`. There was no safety fault or feedback
pause, ownership released, and fresh post-run state was `1/0/0` with battery
77%, zero errors, STOP false, centered axes, and no owner.

The normal allowlist remains `neutral`. Do not add `anger` to
`HARDWARE_COMMISSIONED_EMOTIONS` until a separately authorized live chat
selection and mid-motion retarget test succeeds.

## Development-side commands

After the one-time installation, the normal workflow is exactly two terminals:

```bash
make -C lite3-noetic run-emotion-hardware
make -C lite3-noetic run-emotion-chat
```

The first target checks that both robot services are active and both transmit
flags remain false, opens a loopback-only SSH tunnel through the motion host,
starts the ephemeral OpenAI sidecar and development-computer brain, and then
executes the official runner on the perception computer. Ctrl-C releases SDK
ownership before closing the tunnel and sidecar. Use
`run-emotion-hardware-offline` for the deterministic backend. The old height
bridge is isolated behind `run-emotion-hardware-retroid-diagnostic`.

These targets are intentionally distinct from every simulation target.
