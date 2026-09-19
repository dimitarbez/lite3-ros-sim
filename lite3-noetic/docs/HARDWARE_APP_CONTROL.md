# Lite3 app control path and hardware research

This note records the investigation and first bounded hardware test performed on
2026-09-19 with a physical DEEP Robotics Lite3 and the Android **DEEP Robotics
Lite3** app on a Retroid controller. It is an evidence record and implementation
input. The existing ROS/Gazebo launch targets remain simulation-only.

At the end of the test the robot reported `robot_basic_state = 1` (sitting). No
motion process or persistent robot configuration was changed. Temporary packet
captures were removed and no capture process was left running.

Do not store the robot password, API keys, or other credentials in this
repository.

## Network arrangement used

The development computer retained internet access through USB tethering while
using Wi-Fi exclusively for the robot:

```text
Internet
   |
   | USB tether, computer address 10.43.155.23
   v
Windows host / WSL
   |
   | robot Wi-Fi, computer address 192.168.2.28
   v
Lite3 access point and motion host, 192.168.2.1
   ^
   | robot Wi-Fi, observed Retroid address 192.168.2.196
   |
Retroid controller
```

Windows kept its default internet route through USB tethering. WSL could ping
`192.168.2.1` and reach SSH and the robot UDP network through the Wi-Fi route.
This allowed documentation and development traffic to use the internet without
routing robot commands away from the direct robot network.

The observed robot Wi-Fi SSID was `YSC-JYML-he2asx-5G`.

The observed robot interfaces were:

- `p2p0`: `192.168.2.1/24`, used by the direct Wi-Fi network;
- `eth1`: `192.168.1.120/24` and `192.168.137.120/24`, used by the internal
  motion/perception network.

## Confirmed app control path

```text
Retroid physical controls
        |
        v
DEEP Robotics Lite3 Android app
        |
        | Wi-Fi, little-endian UDP command packets
        v
Lite3 motion host at 192.168.2.1:43893
        |
        v
jy_exe / DEEP Robotics locomotion controller
```

The full Lite3 app is a high-level robot controller. It selects operating modes
and sends command codes and axis values to the motion host. It does not expose a
ROS `/joy` or `/cmd_vel` publisher to this workspace.

The manufacturer architecture includes a return path for robot status. Camera
video is documented separately at `rtsp://192.168.2.1:8554/test` when the
corresponding robot hardware and service are enabled. The video stream was not
tested during this session.

## Connected robot inventory

The following was observed through SSH, socket inspection, configuration reads,
and binary version strings:

- Hostname: `ysc`.
- Operating system: Ubuntu 20.04.5 LTS on `aarch64`.
- Kernel: `5.10.160`.
- SSH service: OpenSSH 8.2 for Ubuntu.
- ROS: `rosversion` was absent; the tested motion-host path does not use ROS.
- Motion executable: `/home/ysc/jy_exe/bin/jy_exe`, running as root.
- `jy_exe` was a link to the deployed `backup/deeprcs` binary.
- UDP listeners owned by the motion process included `43893` and `43899`.
- Robot profile: `professional_1`.
- Local robot metadata identified the product family as `JY-S5` and the profile
  as `standard`.
- Reported software strings: DeepROS `0.2.24(39)`, Profile `1.1.9(36)`, and
  Deeprcs `2.0.153(153)`.

The deployed `network.toml` contained:

```toml
ip = "192.168.1.103"
target_port = 43897
local_port = 43893
```

This matched the observed internal telemetry flow from
`192.168.1.120:<ephemeral>` to `192.168.1.103:43897`. No configuration file was
edited.

## Two different DEEP Robotics Android apps

DEEP Robotics publishes a separate generic Retroid forwarding app in the
official [`DeepRoboticsLab/gamepad`](https://github.com/DeepRoboticsLab/gamepad)
repository. Its documentation and APK describe a development tool that sends
raw Retroid button and joystick events to a configurable development computer.
Its example receiver uses UDP port `12121` and a packed gamepad structure.

That forwarding app is different from the blue **DEEP Robotics Lite3** app used
here. The full Lite3 app sends high-level motion-host commands to UDP `43893`.
Code written for the port-12121 gamepad format will not control the Lite3 motion
host directly.

## Captured behavior of the full Lite3 app

With the full app connected and its joysticks untouched, the Retroid sent from
`192.168.2.196:43897` to `192.168.2.1:43893`. Twelve consecutive sampled
packets were all the same 12-byte simple command:

| Field | Value |
| --- | --- |
| command code | `0x21040001` (heartbeat) |
| command value | `0` |
| command type | `0` (simple command) |

No joystick, pose, gait, action, Stand/Sit, or STOP command appeared in that
sample. This confirms that the installed full app maintains the documented
heartbeat while idle. It does not establish how competing non-heartbeat motion
commands are arbitrated.

## Motion-host command protocol

The primary local reference is
[`Jueying Lite3 Motion Host Communication Interface(beta) V1.0.7-0`](../../lite3-robot-docs/Lite%203%20secondary%20development/Jueying%20Lite3%20Motion%20Host%20Communication%20Interface(beta)%20V1.0.7-0.md).
The matching vendor PDF remains authoritative for tables and diagrams.

Commands use UDP with little-endian fields. The robot target is
`192.168.2.1:43893`. A simple command is exactly 12 bytes:

```c
struct CommandHead {
    uint32_t code;
    uint32_t parameter_or_size;
    uint32_t type;
};
```

For a simple command, `type` is zero and the second word carries the command
value. When a command has no meaningful value, the second word is zero. A
complex command sets `type` to one and appends a body whose byte length is in
the second word. No checksum or envelope was required for the two validated
simple commands.

The exact packet encodings used successfully were:

| Command | Little-endian words | Bytes |
| --- | --- | --- |
| Heartbeat | `0x21040001, 0, 0` | `01 00 04 21 00 00 00 00 00 00 00 00` |
| Stand/Sit toggle | `0x21010202, 0, 0` | `02 02 01 21 00 00 00 00 00 00 00 00` |

Relevant documented behavior and commands:

- Heartbeat `0x21040001` must be sent at least at 2 Hz.
- Stand/Sit `0x21010202` toggles between the two states. It is not a separate
  state-setting command.
- Software STOP `0x21020C0E` makes the robot go down immediately and is intended
  for an emergency. It was not transmitted during this test.
- Reset/zero uses `0x21010C05`.
- Pose mode uses `0x21010D05`.
- Move mode uses `0x21010D06`.
- Manual mode uses `0x21010C02`.
- Navigation mode uses `0x21010C03`.
- Pose axes return toward the normal standing pose if no axis command arrives
  for more than one second.
- Move axis commands must be published at 20 Hz or faster. The robot stops the
  commanded movement if updates cease for more than 250 ms.

The same axis command codes are interpreted according to the selected mode:

| Code | Pose mode | Move mode |
| --- | --- | --- |
| `0x21010131` | body roll | left/right velocity |
| `0x21010130` | body pitch | forward/back velocity |
| `0x21010135` | body yaw | turn rate |
| `0x21010102` | body height | not used as a movement axis |

The documented pose-value bounds are:

- Roll: `[-12553, 12553]`; positive rolls right.
- Pitch: `[-6553, 6553]`; positive lowers the head.
- Height: `[-20000, 20000]`; positive raises the body.
- Yaw: `[-9553, 9553]`; positive rotates the body right.

These are protocol limits, not suitable emotion-expression defaults. A hardware
expression bridge must use materially smaller configured limits and rate limits.

## App control mapping

The controls described in the
[`Lite3 Pro User Manual V1.0.7-0`](../../lite3-robot-docs/Lite%203%20user%20manual/Jueying%20Lite3%20Pro%20User%20Manual%20V1.0.7-0.md)
match the full Lite3 app:

- In Move mode, the left joystick controls planar translation and the right
  joystick controls rotation.
- In Pose mode, the left joystick controls pitch and roll and the right
  joystick controls height and yaw.
- UI controls select Stand/Sit, Move/Pose, Flat/RUG, gait speed, voice features,
  and built-in actions.
- The app STOP control makes the robot go down immediately and is intended for
  loss of control or another emergency.

## Observed telemetry

The motion host continuously transmitted complex status messages from the
internal `eth1` interface to `192.168.1.103:43897`. Every observed message began
with the same 12-byte little-endian header used by commands.

A 200-packet sample contained:

| Count | Code | Body bytes | UDP payload bytes | Interpretation |
| ---: | --- | ---: | ---: | --- |
| 18 | `0x0901` | 200 | 212 | documented `RobotStateUpload` |
| 18 | `0x0902` | 96 | 108 | documented 12 joint angles |
| 18 | `0x0903` | 96 | 108 | documented 12 joint velocities |
| 18 | `0x0904` | 4 | 16 | present, not decoded in this investigation |
| 18 | `0x0905` | 48 | 60 | present, not decoded in this investigation |
| 92 | `0x0906` | 368 | 380 | present, used by bundled SDK code, not decoded here |
| 18 | `0x010901` | 40 | 52 | present, not decoded in this investigation |

The first four bytes of the `0x0901` body are the little-endian signed integer
`robot_basic_state`. The states relevant to the validated test are:

| Value | Meaning |
| ---: | --- |
| `1` | sitting |
| `4` | prepare |
| `5` | sit-to-stand transition |
| `6` | torque-control state, standing |
| `7` | stand-to-sit transition |
| `8` | lose-control protection |
| `9` | posture adjustment |
| `17` | reset-to-zero |

The vendor manual documents `0x0901` at 50 Hz. The internal telemetry target is
fixed by robot configuration rather than automatically replying to each command
sender. During this test, state was verified by passive capture on the robot's
`eth1` interface. A development-host bridge will need an intentional telemetry
route or a read-only status relay instead of changing that target ad hoc.

## Validated Stand/Sit test

The test was explicitly authorized with the robot on a clear floor and a human
holding the Retroid controller with its STOP control available.

Before transmission:

- live `0x0901` telemetry reported state `1` (sitting);
- the Retroid app was connected;
- twelve consecutive Retroid packets were heartbeat-only;
- the target and listener were confirmed as `192.168.2.1:43893`;
- the host had no ROS installation, so no ROS node or topic was involved.

### Stand transition

The development computer used one UDP socket and:

1. sent four heartbeat packets at 4 Hz;
2. sent one Stand/Sit toggle;
3. sent 28 further heartbeat packets at 4 Hz over seven seconds;
4. stopped transmitting and read the robot's status stream.

Observed state progression:

```text
1 sitting -> 4 prepare -> 5 sit-to-stand -> 6 standing
```

The final state `6` was confirmed with a separate fresh telemetry sample after
the transition capture ended.

### Sit transition

Starting from confirmed state `6`, the development computer:

1. sent four heartbeat packets at 4 Hz;
2. sent one Stand/Sit toggle;
3. sent 36 further heartbeat packets at 4 Hz over nine seconds;
4. stopped transmitting and read a fresh robot-state packet.

The final telemetry value was `robot_basic_state = 1`, confirming that the robot
returned to the sitting state. The transition-state capture did not run because
its background `sudo` command paused for authentication, so states `7 -> 1` were
not recorded as a sequence. No second toggle was sent. A direct final-state
sample was used to avoid acting on an assumption.

No STOP, gait, joystick, pose-axis, action, joint, or torque command was sent in
either direction. The Retroid continued to provide its heartbeat. The specific
combination of Retroid heartbeat traffic and one custom Stand/Sit toggle was
therefore validated; concurrent motion commands from two controllers remain
untested.

## Required state-aware Stand/Sit procedure

Because `0x21010202` is a toggle, it must never be treated as an unconditional
"stand" or "sit" command. A repeatable implementation must:

1. Receive and parse a fresh `0x0901` packet.
2. Require state `1` before requesting stand, or state `6` before requesting sit.
3. Refuse the toggle in a transition, action, protection, unknown, or stale
   state.
4. Confirm the Retroid is heartbeat-only and keep a human ready at STOP.
5. Establish a heartbeat at no less than 2 Hz; 4 Hz was validated here.
6. Send exactly one 12-byte Stand/Sit packet.
7. Continue the heartbeat through the transition.
8. Observe the expected transition and require final state `6` for stand or `1`
   for sit.
9. Do not retry blindly after a timeout. Read a fresh state and let the operator
   inspect the robot first.
10. Stop custom transmission and leave the robot in a confirmed stable state.

The software STOP command is an emergency action that makes the robot go down
immediately. It should not be used as ordinary timeout recovery.

## Implication for emotion-bot hardware support

The first hardware implementation should preserve the existing separation
between emotional reasoning and actuation:

```text
emotion-bot state
        |
        v
bounded expression mapper
        |
        v
explicitly enabled Lite3 UDP bridge with watchdog
        |
        v
pose mode plus bounded height/roll/pitch/yaw commands
        |
        v
192.168.2.1:43893
```

Initial hardware work should use small posture changes while standing. Walking,
hops, stomps, direct joint/torque control, and built-in dynamic actions require
separate tests and safety decisions.

A hardware bridge needs all of the following before it can transmit expressive
motion:

- explicit physical-robot enablement separate from simulation motion enablement;
- confirmed robot identity, deployed software, network target, and current state;
- a clear operating area and a human holding the Retroid controller;
- a tested STOP path and documented manual takeover procedure;
- bounded pose values, rate limits, and exact neutral output on stale state;
- heartbeat ownership and deterministic shutdown behavior;
- a watchdog that stops pose output and returns to neutral on failure;
- state feedback that rejects stale or incompatible operating states;
- protection against conflicting motion commands from the app and custom bridge.

## Remaining unknowns and next checks

- Capture each app UI action separately and map its exact command sequence.
- Determine whether the full app changes heartbeat cadence during motion.
- Record the installed full Lite3 app version from its General screen.
- Identify and decode observed telemetry codes `0x0904`, `0x0905`, `0x0906`, and
  `0x010901` from the exact deployed software version.
- Establish a supported telemetry path to the development computer without
  disrupting the existing perception-host destination.
- Test the RTSP stream and its ownership behavior.
- Measure safe posture amplitudes and rates before connecting emotion output.
- Verify neutral recovery and watchdog behavior on packet loss.
- Determine command arbitration when the app and bridge are both present. Do not
  test conflicting movement commands casually.

## Sources

- Local vendor manuals under [`lite3-robot-docs`](../../lite3-robot-docs/README.md).
- [Official Lite3 perception development manual](https://www.deeprobotics.us/wp-content/uploads/2025/10/Jueying-Lite3-Perception-Development-Manual-beta-V2.2.2-0.pdf).
- [Official Lite3 MotionSDK](https://github.com/DeepRoboticsLab/Lite3_MotionSDK).
- [Official Retroid gamepad repository](https://github.com/DeepRoboticsLab/gamepad).
