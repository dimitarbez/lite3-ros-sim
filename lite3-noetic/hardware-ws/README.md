# Lite3 hardware workspace

This is a separate catkin workspace for the perception computer. It does not
modify or overlay the vendor `~/lite_cog` workspace or `qnx2ros`. The normal
`lite3-noetic` simulation image does not launch this package.

The package starts fail closed:

- `transmit_enabled: false` and `dynamic_actions_enabled: false`;
- all nine posture amplitudes and rates are zero;
- the Deeprcs `2.0.153` `0x0906` layout identifier is blank;
- STOP-preemption is unverified and all action trajectories are empty.

Consequently neither posture nor direct-joint output can arm from the checked-in
configuration. These fields are commissioning records, not convenience flags.

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
- The mapper, supervisor, posture bridge, and action controller use the
  `/emotion_bot/hardware/...` namespace. `/simple_cmd` is the sole posture path.
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

Public services are `/emotion_bot/hardware/set_armed` (`std_srvs/SetBool`),
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
The maintained host-side bridge now owns this exact handshake and pairing when
`run-emotion-hardware` is active. The persistent perception services remain
`DISARMED` and unable to transmit; the host bridge starts by default, acquires
an exclusive lock, and acts only while the received emotion is `neutral` and
every read-only gate remains fresh. A 250 ms relay watchdog and every normal
exit send five paired yaw/height zeros.

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

The dynamic controller expects a separately reviewed executable configured as
`~sdk_sender_path`. That executable must link the official aarch64 MotionSDK,
own the 1 kHz loop, acquire and release SDK control, validate high-rate telemetry,
and release control on every exit. Its stdout protocol is exactly one flushed
line for each completed boundary: `SDK_ACQUIRED`, `ACTION_FINISHED`, and
`ROBOT_RELEASED`. Missing or out-of-order markers fail the action. `SIGTERM` must
perform the same release path; a stuck sender is killed after a bounded grace
period and remains a fault. No sender is included or enabled because the
official public repository does not identify a byte layout as compatible with
the deployed Deeprcs `2.0.153`. Do not set the layout identifier or install a
sender until that compatibility and Retroid STOP preemption are physically
verified with a hoist or equivalent independent restraint.

A 2026-09-19 hoisted direct-joint commissioning attempt additionally proved
that `Sender::ControlGet(SDK)` (`0x0114`) is not a seamless handoff from the
vendor standing controller. A measured-position hold and a 10%-scale neutral
request both produced large acquisition transients before tracking-error release.
`direct_joint.takeover_transition_commissioned` therefore remains false and is
an independent hard gate even when a launch supplies `transmit_enabled=true`.
Do not bypass it to command small offsets from a vendor-controlled standing
pose. Direct-joint work now requires a reviewed vendor-supported zero-to-stand
SDK transition before expression trajectories can be commissioned.

The standalone official-SDK neutral-breathing commissioning runner uses a
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

The commissioned animal-like neutral cycle is a deliberately slower physical
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

The hard gate is deployed on the perception computer and defaults false in both
configuration and the bridge executable. Local verification covers a clean
Release build, 27 unit tests, exact packet codes, exclusive sender ownership,
graceful release, independent crash-watchdog release, and the standalone C++
state-safety and animal-profile bounds tests. A live full-amplitude symmetric
run completed 19 five-second cycles before fresh state `8` triggered the
intended no-retry return-to-robot path. A subsequent animal-like run completed
at least 27 3.25-second cycles and remained active at the documented handoff;
four measured telemetry gaps paused and recovered without a tracking or state
fault. See `docs/HARDWARE_APP_CONTROL.md` for the complete dated attempt history
and measurements.

After the `SDK_ACQUIRED` marker the coordinator writes a mode-0600 ownership
marker in `/dev/shm`. A restarted supervisor enters `RECOVERY`, and the sender
must support `--recover` and emit `ROBOT_RELEASED` before the marker is cleared.

## Development-side commands

After the one-time installation, the normal workflow is exactly two terminals:

```bash
make -C lite3-noetic run-emotion-hardware
make -C lite3-noetic run-emotion-chat
```

The first target checks that both robot services are active and both transmit
flags remain false, opens a loopback-only SSH tunnel through the motion host,
starts the ephemeral OpenAI sidecar, and runs the development-computer brain.
Ctrl-C closes the tunnel and sidecar. Use `run-emotion-hardware-offline` for the
deterministic backend.

These targets are intentionally distinct from every simulation target.
