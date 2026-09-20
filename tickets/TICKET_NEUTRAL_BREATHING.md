# Physical Neutral: Animal-Like Breathing

[← Emotion ticket index](README.md) ·
[Next: Joy](TICKET_JOY_FRONT_PAW.md)

## Status — accepted 2026-09-19

Neutral is implemented, physically commissioned, and operator-accepted. It is
both an emotion animation and the mandatory transition waypoint for every
category change. Its motion must remain unchanged unless a new explicitly
authorized commissioning task replaces the recorded evidence.

## Intended emotional read

The standing robot should look alive and calm rather than frozen: slow breathing
through coordinated leg compression, a small animal-like roll/pitch sway, and
no foot lift. It must not look excited, distressed, or ready to walk.

## Accepted choreography

- One loop lasts 3.25 seconds and joins continuously with zero endpoint
  velocity.
- All four paws remain planted.
- HipX and yaw remain zero.
- Per-leg HipY/knee motion follows the accepted compression relationship, with
  knee displacement and velocity exactly twice the combined HipY compression.
- The accepted combined HipY scalar is bounded to `-0.015..+0.027 rad`.
- Roll bias is bounded to `±0.004 rad`; front/rear pitch bias is bounded to
  `±0.003 rad`.
- Neutral does not use locomotion, a vendor gait/action, torque feed-forward,
  external forces, or a second command owner.

## Runtime state machine

The operator-started session performs
`RobotStateInit → PreStandUp → StandUp → stand hold` once. After that:

1. hold exact canonical stand for the transition hold;
2. blend into the neutral breathing loop;
3. repeat the 3.25-second loop while neutral remains current;
4. on another emotion request, complete the current safe sample, return to
   exact canonical stand over 1.5 seconds, and hold it for 0.35 seconds;
5. start only the newest pending emotion.

Stale chat completes the neutral return and then releases control so the
established vendor path may sit the robot. STOP or a safety fault bypasses the
conversational transition and uses immediate verified hold/release behavior.

## Safety and invariants

- Initial robot basic state must be `1`; gait/motion must be `0/0` with zero
  errors and acceptable battery/attitude.
- One official MotionSDK runner owns all 12 joints continuously.
- Fresh state, feedback, STOP, and validated emotion records are mandatory.
- The 100 ms feedback pause, 20-fresh-frame recovery, 250 ms dead-feedback
  release, tracking bound, watchdog, and exclusive ownership marker remain
  active.
- No category may bypass neutral merely because its pose begins near the
  current joint command.

## Completion checklist

- [x] Declarative 3.25-second profile implemented.
- [x] Finite samples, seams, velocity/acceleration, joint bounds, and planted
  invariants tested offline.
- [x] aarch64 MotionSDK build and tests passed.
- [x] Initialization, stand, neutral loop, and clean release completed on the
  physical robot.
- [x] Feedback-pause and fault-release behavior retained.
- [x] Operator visually accepted the breathing animation.
- [x] Neutral is the canonical stance used by the transition planner.
- [ ] Re-run neutral regression as part of the final nine-emotion chat sweep.
- [ ] Confirm every completed emotion implementation returns through the same
  1.5-second neutral return and 0.35-second exact hold.

## Acceptance criteria

- Calm breathing is visibly distinct but subtle.
- Feet remain planted without twist, slip, or gait transition.
- Every loop is continuous and every exit reaches exact canonical stand.
- State/error, tracking, feedback, STOP, and watchdog gates remain effective.
- End-of-session ownership release leaves no marker or concurrent sender.

## Non-goals

- Making neutral more expressive than another emotion.
- Modulating yaw or walking position.
- Re-running the stand sequence on every emotion transition.
- Replacing emergency release with the normal 1.5-second transition.

