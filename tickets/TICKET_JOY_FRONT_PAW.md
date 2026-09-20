# Physical Joy: Torque-Gated Front-Paw Taps

[← Emotion ticket index](README.md) ·
[Neutral reference](TICKET_NEUTRAL_BREATHING.md) ·
[Next: Affection](TICKET_AFFECTION.md)

## Status — persistent chat recovery deployed; live revalidation pending

**The physical joy gesture is implemented, telemetry-validated, and
operator-accepted.** It is a five-second alternating front-paw expression, not
an airborne hop: the robot transfers support, lifts and gently replaces the
front-left paw, then repeats with the front-right paw.

The original accepted suite runner was compiled and tested against the robot's
aarch64 MotionSDK and installed with SHA-256
`b00a7aacf331a3944eaa19937091f2eaa75a60fe9090f6cc135894f0739b0092`.
The accepted live suite was invoked explicitly with the joy suite enabled and a
50 mm paw-lift target. The continuous runner now selects this trajectory for an
allowlisted Joy state, and its normal state-driven selection plus lower-first
Joy-to-Neutral and Joy-to-unsupported retargets passed physically on 2026-09-20.
A later natural-language Joy turn exposed that a safely recovered unload miss
was still classified as a terminal session failure. The persistent recovery
correction described below passes offline and aarch64 verification and is now
deployed, but has not been physically revalidated.

## Accepted behavior

Each paw uses five bounded phases:

1. transfer load for 0.50 seconds;
2. raise the selected front paw for 0.60 seconds;
3. hold for 0.30 seconds;
4. lower gently for 0.60 seconds;
5. settle back to four-foot stand for 0.50 seconds.

That is 2.50 seconds per paw and exactly 5.00 seconds for joy. The initial
single-suite acceptance used a 50 mm paw target, 20 mm lateral transfer on each
side, 20 mm rearward transfer for the left paw, and 35 mm rearward transfer for
the right paw. The right-side asymmetry was deliberate: live load evidence
showed that a symmetric transfer left excessive residual load on the
front-right paw.

The final repeated-chat geometry uses a 50 mm paw target, 20 mm lateral
transfer on each side, 30 mm rearward transfer for the left paw, and 35 mm
rearward transfer for the right paw. The original accepted single suite used
20 mm on the left; normal repetition showed that 20 mm and then 25 mm retained
too much left-paw load on later cycles. The 30 mm revision passed two complete
loops and a mid-paw retarget without weakening any unload, support, landing, or
workspace gate.

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
stable diagonal distribution. A missed unload or landing never strands the
paw. The explicit commissioning suite and unsafe/incomplete landing paths still
fail and release. In normal chat, the offline-hardened path treats only an
unchanged-threshold unload miss followed by a confirmed four-foot landing as a
recoverable event: it completes exact Neutral, retains the sole owner, and
resumes Joy if Joy is still the current emotion.

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

The cross-emotion selector and live chat acceptance are tracked by the
[chat-driven physical emotion integration ticket](TICKET_PHYSICAL_EMOTION_CHAT.md).

- [x] Make the accepted paw trajectory selectable by normal validated joy state in
  the continuous chat session, using the same exact-neutral transition engine.
- [x] Preserve the explicit bounded suite as a regression/commissioning mode
  after normal joy-state integration.
- [x] Add a deterministic state-machine test for retargeting while either paw is
  raised: finish lower/settle, return and hold exact neutral, then enter only the
  newest pending emotion.
- [x] Complete live neutral→joy, joy→neutral, and joy→another-emotion tests
  through the normal chat path.
- [ ] Use this implementation pattern—not necessarily this choreography—for the
  seven remaining emotions in the [emotion ticket index](README.md).

## Non-goals

- An airborne joy/surprise hop.
- Forceful impacts or torque-controlled strikes.
- Vendor gait/action or long-twist jump primitives during SDK ownership.
- Open-loop lifted-foot commands without unload, support, and landing evidence.
- Skipping exact neutral when changing emotions.

## Normal chat integration evidence — 2026-09-20

The first normal Joy run used the original 20 mm left transfer and correctly
failed unload at `8.582 N`; it lowered, restored four supports, and released
without a safety fault. A 25 mm retry passed one loop but the next left lift
retained `8.017 N`, so it also failed closed. Neither threshold was weakened.

With the final 30/35 mm left/right rearward shifts, two consecutive loops
passed under one continuous owner:

- loop 1: left unload/landing `3.863/34.021 N`, right
  `6.838/30.434 N`;
- loop 2: left `4.987/34.137 N`, right `1.225/30.249 N`.

A Neutral chat state arrived during the next left-paw cycle. The paw unloaded
to `3.060 N`, landed at `28.840 N` with four supports, returned to exact
canonical stand over 1.5 seconds, and held it for 0.35 seconds without releasing
ownership. A separate Surprise request arrived during a right-paw cycle; that
paw unloaded to `0.364 N`, landed at `24.124 N`, and completed the same
transition. Status retained `requested_emotion=surprise` while selecting
`neutral_animal_breath` with fallback reason
`physical_reaction_not_accepted`.

The pre-correction integrated aarch64 runner used for those live checks,
including the unchanged hard gates, was installed at SHA-256
`c2723ef4140a1bda88d18d6bfd09494febb2f2dea8d8db3f9d32b1e683a8025a`.

## Natural-language Joy repeat-stop failure and offline correction — 2026-09-20

The operator entered “you are so amazing we love you!” in the live OpenAI chat.
The correlated state was Joy at valence/arousal `0.700/0.600`. With a valid
684-sample, `124.444 N` baseline, the first complete Joy reaction passed:
front-left unload/landing was `3.49652/32.7384 N`, and front-right was
`4.58882/30.6913 N`. Because Joy was still current, the runner correctly
started a second reaction without requiring another conversational event. Its
front-left paw retained
`12.5215 N`, so the unchanged unload gate rejected it. The paw was lowered and
landed at `36.1785 N` with all four supports restored, after which the runner
released with no robot safety fault. The operator observed that the robot moved
in Joy, stopped, and then lay down; that final posture followed the ownership
release, not an OpenAI classification failure.

The offline correction makes normal Joy persistent like the other commissioned
profiles:

- the accepted alternating-paw reaction repeats while Joy remains the latest
  validated EmotionEngine category;
- new state sequences still update the engine, while transport heartbeats only
  maintain link freshness and neither start nor stop the current profile;
- successful Joy cycles use their accepted settle phase and continue without
  releasing MotionSDK ownership;
- an unload miss is recoverable only after the paw is down and four supports
  are confirmed; it performs the 1.5-second canonical return and 0.35-second
  Neutral hold, logs `JOY_CONTACT_MISS_RECOVERED`, and resumes Joy if it remains
  current; and
- landing, STOP, robot-state, feedback, estimator, and other hard failures keep
  their existing fail-closed release behavior.

No unload, support, landing, workspace, attitude, battery, STOP, or feedback
threshold changed. `make -C lite3-noetic verify-hardware-offline` passed all
nine native suites, 29 Python tests, the clean catkin build, and the loopback
ownership/watchdog integration. A no-motion deployment from state/gait/motion
`1/0/0`, battery 91%, zero errors, fresh centered Retroid, STOP false, and no
owner then passed all nine aarch64 suites and installed SHA-256
`aa449b7883a3baf6ae816fc832dbf3b8b74bb5a1ac88c2e0313f3acab1c8f353`.
The old `c2723...` binary was retained as a checksum-named backup. This is not
physical acceptance evidence; bounded persistent-Joy revalidation is still
required.
