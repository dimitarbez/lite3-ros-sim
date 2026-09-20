# Physical Integration: Chat-Driven Emotion Reactions

[Ticket index](README.md) ·
[Hardware runtime](../lite3-noetic/hardware-ws/README.md) ·
[ROS chat and EmotionEngine adapter](../lite3-noetic/ws/Lite3_VMC/src/emotion_bot_ros/README.md)

## Status — planned integration; physical execution not authorized by this ticket

The development-computer chat, `EmotionEngine`, validated emotion-state uplink,
and continuous robot-side MotionSDK owner already exist. Neutral is the only
category enabled in the normal hardware allowlist today. Joy, Sadness, Fear,
and Anger each have an operator-accepted physical reaction, but their normal
chat-driven selection and transition paths are not all physically validated.

This ticket connects those accepted reactions to the same conversation-driven
emotional state used by Gazebo. It does not authorize a physical run by itself.
Every live acceptance step still requires explicit current-task approval and
the normal robot, operator, STOP, state, telemetry, ownership, and clear-area
checks.

## Goal

An operator chats through the existing `emotion_chat.py` client. The same
`EmotionEngine` appraisal, personality, memory, valence, arousal, categorical
emotion, turn correlation, and assistant-response flow used by the simulator
produces `/emotion_bot/state`. The separate hardware path forwards that
validated state to the robot-side continuous expression runner, which chooses
the currently accepted physical reaction for that category.

If the engine selects a category without an accepted physical implementation,
the robot performs the accepted neutral breathing reaction. The engine state
must not be changed to neutral: the requested emotion remains available to the
conversation, logs, and status, while only the physical reaction falls back.

```text
emotion_chat.py -> chat adapter -> EmotionEngine -> /emotion_bot/state
                                                    |
                                                    v
hardware uplink -> loopback SSH tunnel -> validated shared state
                                                    |
                                                    v
                                    physical profile resolver
                                      | accepted -> category reaction
                                      ` otherwise -> neutral breathing
                                                    |
                                                    v
                              sole MotionSDK owner + expression status
```

The hardware path must not start the Gazebo expression mapper, simulator safety
bridge, `/emotion_bot/joy_out`, or any second actuator sender.

## Required physical mapping

For this ticket, “physically implemented” means the final reaction has passed
its bounded physical telemetry run and received an operator visual acceptance.
Compiled profiles, planted prototypes, and rejected candidates do not qualify.

| EmotionEngine category | Physical reaction at ticket completion | Current integration state |
| --- | --- | --- |
| `neutral` | Accepted 3.25-second animal-like breathing | Normal hardware path accepted |
| `joy` | Accepted 50 mm alternating front-paw joy gesture | Physical suite accepted; continuous chat selection and retarget remain |
| `sadness` | Accepted front-only 48 mm planted bow and heave loop | Physical loop accepted; normal chat selection and retarget remain |
| `anger` | Accepted canonical-reset alternating controlled paw placements | Selector/retarget logic passes offline; normal live chat validation remains |
| `fear` | Accepted all-feet-planted flinch/recoil/cower/freeze loop | Physical run and operator visual verdict accepted; normal chat selection and retarget remain |
| `affection` | Neutral breathing fallback | Final physical reaction not implemented and accepted |
| `curiosity` | Neutral breathing fallback | Final physical reaction not implemented and accepted |
| `disgust` | Neutral breathing fallback | Final physical reaction not implemented and accepted |
| `surprise` | Neutral breathing fallback | Final physical reaction not implemented and accepted |

Future emotion tickets may replace a neutral fallback only after that category
meets the same physical evidence, operator-acceptance, normal chat-selection,
and retarget requirements. Merely adding a category to
`HARDWARE_COMMISSIONED_EMOTIONS` is not acceptance evidence.

## Functional requirements

### 1. Preserve simulator conversation semantics

- Reuse `chat_adapter.py`, `emotion_adapter.py`, and the reusable headless
  `EmotionEngine`; do not create a hardware-only emotion classifier.
- Preserve conversation-event schema `1.0`, emotion-state schema `1.1`, turn
  IDs/indexes, state sequences, user/assistant appraisal ordering, personality,
  memory, valence `[-1, 1]`, arousal `[0, 1]`, and all nine categories.
- Keep both deterministic/offline and OpenAI-backed chat modes working through
  the existing two-terminal operator flow.
- Do not send simulator posture/action intentions to hardware. The common
  boundary is the validated emotional state, not Gazebo `Twist`, `Joy`, hop, or
  stomp commands.

### 2. Resolve accepted reactions and neutral fallback on the robot side

- Keep the transmitted state unchanged. Resolve `requested emotion -> physical
  profile` only inside the single robot-side expression owner.
- Make Neutral, Joy, Sadness, Fear, and Anger the intended completed mapping, but
  enable each non-neutral category in the normal allowlist only after its live
  chat-selection and retarget acceptance steps pass.
- Select the accepted Joy paw state machine, not the older planted joy proxy.
- Select the accepted front-only Sadness bow, not the rejected deeper bow or
  optional withdrawn-paw candidate.
- Select the canonical-reset Anger implementation that passed the three-cycle
  suite, not either earlier repeat implementation.
- Select the accepted Fear flinch/recoil/cower/freeze animation that worked on
  the robot and was visually verified by the operator.
- Resolve Affection, Curiosity, Disgust, and Surprise to the unchanged accepted
  neutral breathing profile. No unaccepted candidate may run through normal
  chat.
- Same-category updates may affect the next safe loop through bounded
  valence/arousal scaling, but must not restart the current loop or bypass that
  profile's commissioned envelope.

### 3. Make requested state, resolution, and fallback observable

- `/emotion_bot/hardware/expression_status` must continue to distinguish the
  engine-requested category from the active physical profile.
- Add backward-compatible status detail if needed so an operator can determine
  the requested emotion, resolved profile, whether neutral is a fallback, the
  fallback reason, active phase/cycle, newest pending request, link age,
  ownership, contact state, last fault, and release state.
- The interactive chat output must continue to display the EmotionEngine state;
  operator diagnostics must also expose the active physical profile. Do not
  label a neutral fallback as though the engine itself became neutral.

### 4. Apply one transition contract to every category change

For accepted reactions and fallbacks alike, a category change must:

1. finish lowering and settling any raised paw;
2. cancel the old repeating choreography;
3. return from the current command to exact canonical stand over 1.5 seconds;
4. hold exact canonical stand for 0.35 seconds;
5. start only the newest pending resolved profile.

Rapid chat turns replace one pending target without restarting the neutral
return. A change from one unsupported category to another keeps the active
neutral profile without pretending that the requested category is unchanged.
A request that resolves to the same physical profile must not create needless
motion, but its newest requested category and sequence must remain observable.

STOP, state/error faults, invalid feedback, tracking violations, or ownership
loss bypass conversational timing and use the existing immediate safe
hold/release path. A stale or lost chat link is not an unsupported-emotion
fallback: recover through neutral and release safely instead of breathing
forever on stale state.

### 5. Preserve the operator workflow and ownership boundary

- Retain the normal two-terminal interface:

  ```bash
  make -C lite3-noetic run-emotion-hardware       # or the offline variant
  make -C lite3-noetic run-emotion-chat
  ```

- Keep emotional reasoning and network-backed response generation on the
  development computer. Keep trajectory generation, contact gates, safety
  interlocks, and MotionSDK ownership on the perception computer.
- Acquire MotionSDK once per operator-started session, require initial robot
  state `1`, stand once through `RobotStateInit -> PreStandUp -> StandUp`, and
  retain one exclusive owner across chat-driven emotion changes.
- Preserve explicit bounded commissioning modes for Joy, Sadness, Fear, and
  Anger as regression tools; normal chat integration must not remove or
  silently alter them.
- Keep the Retroid diagnostic bridge and every legacy posture/action sender
  stopped while the official runner owns MotionSDK.

## Implementation checklist

- [ ] Add a table-driven physical profile resolver with all nine engine
  categories and explicit neutral fallbacks for unaccepted categories.
- [ ] Integrate the accepted Joy paw state machine into the continuous runner,
  including safe mid-left-paw and mid-right-paw retarget behavior.
- [ ] Route normal `sadness` state to the accepted front-only 48 mm planted bow
  and verify newest-request retarget behavior.
- [ ] Route normal `fear` state to the accepted all-feet-planted physical Fear
  animation and verify newest-request retarget behavior.
- [ ] Retain the accepted canonical-reset Anger selector and close its remaining
  live normal-chat/retarget validation gap.
- [ ] Preserve requested emotion separately from resolved/active profile in
  expression status, including unsupported-to-neutral fallback evidence.
- [ ] Add an operator-visible way to inspect chat turn, engine category,
  requested physical category, resolved profile, phase, pending request, and
  fallback reason together.
- [ ] Keep `HARDWARE_COMMISSIONED_EMOTIONS=neutral` as the fail-closed default
  until each accepted reaction passes its authorized live chat tests; then
  document and enable only the completed accepted set.
- [ ] Update the hardware runtime, usage, verification, and relevant per-emotion
  tickets with the exact implementation and dated evidence.

## Offline verification

- [ ] Unit-test all nine category resolutions: Neutral, Joy, Sadness, Fear, and
  Anger select their final accepted implementations; the other four select
  neutral.
- [ ] Verify fallback does not mutate emotion-state schema `1.1`, category,
  valence/arousal, turn correlation, or transport sequence.
- [ ] Verify requested and active/resolved status for every supported and
  fallback category, including two unsupported categories in succession.
- [ ] Test same-category updates and unsupported-category changes without loop
  restart or discontinuous commands.
- [ ] Test rapid accepted -> unsupported -> accepted retargeting so only the
  newest request starts after the exact-neutral transition.
- [ ] Test retargeting during every Joy and Anger raised-paw/placement phase and
  during every Sadness and Fear phase; a raised paw must land and four supports
  must be restored before neutral return completes.
- [ ] Test stale link, authenticated STOP, state `8`, nonzero robot error,
  feedback pause/death, tracking violation, and runner crash/release behavior.
- [ ] Run `make -C lite3-noetic verify-emotion` and
  `make -C lite3-noetic verify-hardware-offline`; offline success is not
  physical commissioning evidence.
- [ ] Clean-build and pass the complete runner suite against the robot's actual
  aarch64 MotionSDK before any authorized live run.

## Live acceptance plan

Live work must be separately authorized and recorded with correlated chat
turn/state, transport sequence, requested/resolved/active profile, phase,
joint/IMU/contact/robot-state telemetry, feedback age, ownership, fault/release
state, and operator observation.

- [ ] Prove a real chat turn influences `EmotionEngine` and reaches the robot as
  the same validated category, valence, arousal, turn ID, and ordered sequence.
- [ ] Validate neutral -> Joy, Joy -> neutral, and a mid-paw Joy -> unsupported
  request that lands safely, returns through exact neutral, and remains neutral.
- [ ] Validate neutral -> Sadness, Sadness -> neutral, and mid-loop Sadness ->
  another accepted reaction through exact neutral.
- [ ] Validate neutral -> Fear, Fear -> neutral, and mid-loop Fear -> another
  accepted/fallback reaction through exact neutral.
- [ ] Validate neutral -> Anger, Anger -> neutral, and mid-paw Anger -> newest
  accepted/fallback request through the canonical-reset path.
- [ ] Run a bounded nine-category chat sweep. Neutral, Joy, Sadness, Fear, and
  Anger must be visually recognizable as their accepted physical reactions;
  Affection, Curiosity, Disgust, and Surprise must visibly remain on neutral
  breathing while status preserves each requested category.
- [ ] Verify rapid turns never start an obsolete pending reaction and never
  leave a paw raised, residual offset, second SDK owner, safety fault, or
  unreleased process after shutdown.
- [ ] Record separate operator verdicts for conversation influence, correct
  reaction selection/fallback, transition quality, and final stationary
  release. Joint movement or topic publication alone is not a pass.

## Acceptance criteria

- The existing chat client influences the same `EmotionEngine` state and
  assistant-response flow in hardware mode as in Gazebo mode.
- Every validated engine category is handled deterministically: the five
  accepted physical reactions select their final implementations and every
  other category selects neutral breathing.
- The requested engine emotion remains truthful and inspectable when the active
  physical profile is neutral fallback.
- Accepted reactions are entered only through the normal allowlist and the
  shared exact-neutral transition contract; lifted paws always land first.
- The live transition and nine-category sweep pass with fresh telemetry,
  exclusive ownership, no safety fault, and an operator visual verdict.
- Stale chat, STOP, safety failure, or shutdown leaves the robot in the existing
  verified neutral/release path with no residual command or competing sender.

## Non-goals

- Implementing new final Affection, Curiosity, Disgust, or Surprise
  choreography in this ticket.
- Making hardware imitate Gazebo joint amplitudes, airborne hops, forceful
  stomps, gait/action primitives, external wrenches, or simulation Joy output.
- Moving emotional reasoning, OpenAI access, or conversation memory onto the
  robot computers.
- Weakening contact, support, attitude, battery, feedback, tracking, state,
  STOP, watchdog, ownership, or release gates to make a chat transition pass.
