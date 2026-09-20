# Physical Joy: Torque-Gated Front-Paw Taps

[← Emotion ticket index](README.md) ·
[Neutral reference](TICKET_NEUTRAL_BREATHING.md) ·
[Next: Affection](TICKET_AFFECTION.md)

## Status — accepted 2026-09-20

**The physical joy gesture is implemented, telemetry-validated, and
operator-accepted.** It is a five-second alternating front-paw expression, not
an airborne hop: the robot transfers support, lifts and gently replaces the
front-left paw, then repeats with the front-right paw.

The accepted runner was compiled and tested against the robot's aarch64
MotionSDK and installed with SHA-256:
`b00a7aacf331a3944eaa19937091f2eaa75a60fe9090f6cc135894f0739b0092`.
The accepted live suite was invoked explicitly with the joy suite enabled and a
50 mm paw-lift target. Continuous chat-driven selection of this trajectory and
the seven remaining emotion implementations are tracked in the
[emotion ticket index](README.md).

## Accepted behavior

Each paw uses five bounded phases:

1. transfer load for 0.50 seconds;
2. raise the selected front paw for 0.60 seconds;
3. hold for 0.30 seconds;
4. lower gently for 0.60 seconds;
5. settle back to four-foot stand for 0.50 seconds.

That is 2.50 seconds per paw and exactly 5.00 seconds for joy. The accepted
geometry uses a 50 mm paw target, 20 mm lateral transfer on each side, 20 mm
rearward transfer for the left paw, and 35 mm rearward transfer for the right
paw. The right-side asymmetry is deliberate: live load evidence showed that a
symmetric transfer left excessive residual load on the front-right paw.

The gesture remains inside the sole continuous official MotionSDK owner. It
does not use a vendor gait/action, external wrench, torque feed-forward,
world-frame locomotion, or a second sender.

## Final commissioning evidence

The successful run began from state/gait/motion `1/0/0`, battery 28%, zero
error flags, normal attitude, fresh STOP and feedback records, and no ownership
marker. Initialization completed through the existing vendor stand sequence.

- Four-foot baseline: valid, 724 distinct samples, 120.632 N total estimated
  vertical load.
- Neutral window: 5.00008 seconds.
- Front-left: unload confirmed at 0.974908 N; landing confirmed at 36.1391 N
  with four supports restored.
- Front-right: unload confirmed at 6.50436 N; landing confirmed at 30.0885 N
  with four supports restored.
- Joy window: both complete 2.50-second paw cycles, 5.00 seconds total.
- Maximum feedback age: 8.65699 ms; maximum consecutive feedback gap:
  9.50925 ms; feedback pauses/recoveries: `0/0`.
- End state: ownership marker absent, safety fault false, process exit `0`,
  `PASS official SDK expression session`.
- Post-run state remained `1/0/0`, battery 27%, zero errors, and normal
  attitude.
- Operator verdict: “I like how joy animates now.”

## Why the earlier planted versions were rejected

The earlier full-scale planted joy choreography moved safely but read mostly as
breathing, leaning, or rocking. Stronger HipX and crouch motion still left all
four paws planted. The accepted visual event required a real support transfer,
front-paw unload, visible lift, gentle landing, and alternation.

The official `RobotData::contact_force` array was zero even while the robot was
standing, `/joint_states` lacked usable effort data, and no separate physical
contact ROS topic was available. The implementation therefore estimates foot
load from MotionSDK joint position and torque rather than claiming those empty
signals prove contact.

## Contact estimator and gate

`hardware-ws/tools/motion_sdk_contact_estimator.hpp` uses the maintained Lite3
kinematic model to estimate each foot's vertical load:

```text
J(q)^T F = tau
F = inverse(J(q)^T) tau
vertical load = max(0, -Fz)
```

The estimator rejects non-finite, singular, and implausible solutions; accepts
only distinct feedback ticks; filters each leg; establishes a stable four-foot
baseline; and uses consecutive-frame hysteresis for unload and landing.

An initial four-feet-planted capture produced 23.44, 24.34, 31.23, and
35.73 N by leg, totaling 114.74 N versus 116.15 N expected for the configured
11.84 kg model. This motivated the live calibration, but the final acceptance
rests on the successful bounded run and operator observation, not that sample
alone.

The first lifted-paw suite exposed an overly strict rule that required all
three non-target paws to retain measurable load. The stable robot naturally
settled onto two strong diagonal supports while one other paw became nearly
unloaded. The accepted gate therefore requires:

- the target paw to cross its baseline-relative unload threshold for the
  required consecutive fresh frames;
- at least two non-target paws at or above 5 N;
- at least 70 N aggregate non-target support;
- valid baseline/estimator state throughout; and
- restored target contact and four-foot support after landing.

This preserves a quantitative support requirement while allowing the observed
stable diagonal distribution. A missed unload or landing does not strand the
paw: the runner completes lowering and settling, reports failure, and releases.

## Transition and preemption requirements

The accepted joy motion does not weaken the global transition contract:

1. A requested emotion change is stored as the newest pending category.
2. If a paw is raised, its lower and settle phases complete first.
3. The runner returns to exact canonical stand over 1.5 seconds.
4. It holds exact neutral for 0.35 seconds.
5. Only then may the next emotion begin.

STOP, invalid state, nonzero error flags, tracking faults, stale/dead feedback,
or watchdog faults retain their immediate hold/release behavior. STOP
preemption was separately observed working during this commissioning effort.

## Implemented verification

- [x] Native tests cover the recorded standing estimate, distinct-tick baseline,
  unload/landing hysteresis, diagonal-support sufficiency, weak total support,
  non-finite input, and singular kinematics.
- [x] IK tests cover neutral identity, 50 mm lifts, the accepted left and asymmetric
  right transfers, finite velocities, joint workspace, and invalid targets.
- [x] The complete ROS-independent suite contains five tests and passed `5/5` both
  locally and on the robot's aarch64 perception computer before the live run.
- [x] Live telemetry confirmed both unloads, both landings, complete timing, fresh
  feedback, no pause, no safety fault, and clean ownership release.
- [x] The operator visually accepted the completed joy animation.

## Remaining integration work

- [ ] Make the accepted paw trajectory selectable by normal validated joy state in
  the continuous chat session, using the same exact-neutral transition engine.
- [ ] Preserve the explicit bounded suite as a regression/commissioning mode
  after normal joy-state integration.
- [ ] Add a deterministic state-machine test for retargeting while either paw is
  raised: finish lower/settle, return and hold exact neutral, then enter only the
  newest pending emotion.
- [ ] Complete live neutral→joy, joy→neutral, and joy→another-emotion tests
  through the normal chat path.
- [ ] Use this implementation pattern—not necessarily this choreography—for the
  seven remaining emotions in the [emotion ticket index](README.md).

## Non-goals

- An airborne joy/surprise hop.
- Forceful impacts or torque-controlled strikes.
- Vendor gait/action or long-twist jump primitives during SDK ownership.
- Open-loop lifted-foot commands without unload, support, and landing evidence.
- Skipping exact neutral when changing emotions.
