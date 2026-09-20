# Physical Integration: Chat-Driven Emotion Reactions

[Ticket index](README.md) ·
[Hardware runtime](../lite3-noetic/hardware-ws/README.md) ·
[ROS chat and EmotionEngine adapter](../lite3-noetic/ws/Lite3_VMC/src/emotion_bot_ros/README.md)

## Status — implemented; Joy/Anger recovery revalidation and operator verdict pending

The development-computer chat, `EmotionEngine`, validated emotion-state uplink,
and continuous robot-side MotionSDK owner now drive the accepted Neutral, Joy,
Sadness, Fear, and Anger reactions. On 2026-09-20 the physical robot completed
normal state-driven Neutral, Joy, Sadness, Fear, and Anger selection, exact-
neutral accepted/fallback retargets, all four unsupported-category fallbacks,
and a rapid newest-request-only transition under the sole continuous owner.
Every run ended in state/gait/motion `1/0/0`, zero errors, centered Retroid
input, STOP false, and a clean SDK release.

The first session's explicit 75% floor correctly aborted at 74%. With the
operator's explicit confirmation that the remaining battery was acceptable, a
separate session used the project's configured 25% abort floor without
changing any code or weakening any other safety gate. That session closed the
Anger-to-Disgust, unsupported-category, rapid-retarget, and final-release
telemetry gates. The normal checked-in allowlist remains `neutral` until the
operator records the consolidated visual verdict for the complete sweep.
A later natural-language Joy run exposed a terminal release after a safely
recovered unload miss. The new persistent/non-releasing recovery policy also
requires live revalidation before Joy can be considered closed end to end.
The subsequent Anger run exposed the same release-policy class after confirmed
landing and four-foot recovery; its non-releasing correction is deployed and
also requires live revalidation.

This ticket connects those accepted reactions to the same conversation-driven
emotional state used by Gazebo. It does not authorize a physical run by itself.
Every live acceptance step still requires explicit current-task approval and
the normal robot, operator, STOP, state, telemetry, ownership, and clear-area
checks.

Completion of this ticket requires a supervised run on the physical Lite3.
Gazebo, ROS topic publication, an offline suite, or a successful aarch64 build
may support preparation, but none of them is acceptance evidence for the live
chat-selection, retargeting, fallback, or final-release requirements below.

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
| `joy` | Accepted 50 mm alternating front-paw joy gesture | Persistent recovery deployed; live revalidation pending |
| `sadness` | Accepted front-only 48 mm planted bow and heave loop | Normal physical selection and exact-neutral retarget passed |
| `anger` | Accepted canonical-reset alternating controlled paw placements | Persistent recovery deployed; live revalidation pending |
| `fear` | Accepted all-feet-planted flinch/recoil/cower/freeze loop | Normal physical selection and Fear-to-Anger retarget passed |
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

- [x] Add a table-driven physical profile resolver with all nine engine
  categories and explicit neutral fallbacks for unaccepted categories.
- [x] Integrate the accepted Joy paw state machine into the continuous runner,
  including safe mid-left-paw and mid-right-paw retarget behavior.
- [x] Route normal `sadness` state to the accepted front-only 48 mm planted bow
  and verify newest-request retarget behavior.
- [x] Route normal `fear` state to the accepted all-feet-planted physical Fear
  animation and verify newest-request retarget behavior.
- [x] Retain the accepted canonical-reset Anger selector and integrate its
  normal-chat/retarget path offline; live validation remains below.
- [x] Preserve requested emotion separately from resolved/active profile in
  expression status, including unsupported-to-neutral fallback evidence.
- [x] Add an operator-visible way to inspect chat turn, engine category,
  requested physical category, resolved profile, phase, pending request, and
  fallback reason together.
- [x] Keep `HARDWARE_COMMISSIONED_EMOTIONS=neutral` as the fail-closed default
  while live functional tests and the consolidated operator verdict are open.
- [ ] After the operator's consolidated verdict, enable and document only the
  completed accepted set.
- [x] Update the hardware runtime, usage, verification, and relevant per-emotion
  tickets with the exact implementation and dated evidence.

## Offline verification

- [x] Unit-test all nine category resolutions: Neutral, Joy, Sadness, Fear, and
  Anger select their final accepted implementations; the other four select
  neutral.
- [x] Verify fallback does not mutate emotion-state schema `1.1`, category,
  valence/arousal, turn correlation, or transport sequence.
- [x] Verify requested and active/resolved status for every supported and
  fallback category, including two unsupported categories in succession.
- [x] Test same-category updates and unsupported-category changes without loop
  restart or discontinuous commands.
- [x] Test rapid accepted -> unsupported -> accepted retargeting so only the
  newest request starts after the exact-neutral transition.
- [x] Test retargeting during every Joy and Anger raised-paw/placement phase and
  during every Sadness and Fear phase; a raised paw must land and four supports
  must be restored before neutral return completes.
- [x] Test stale link, authenticated STOP, state `8`, nonzero robot error,
  feedback pause/death, tracking violation, and runner crash/release behavior.
- [x] Run the non-Gazebo portions of `verify-emotion` and the complete
  `make -C lite3-noetic verify-hardware-offline`; offline success is not
  physical commissioning evidence. Gazebo was intentionally excluded because
  this ticket requires the physical robot.
- [x] Clean-build and pass the complete runner suite against the robot's actual
  aarch64 MotionSDK before any authorized live run.

## Live acceptance plan

Live work must be separately authorized and recorded with correlated chat
turn/state, transport sequence, requested/resolved/active profile, phase,
joint/IMU/contact/robot-state telemetry, feedback age, ownership, fault/release
state, and operator observation.

Run every item in this section against the physical robot, not Gazebo. Keep the
simulator graph, Gazebo expression mapper, simulator safety bridge, and
`/emotion_bot/joy_out` out of the hardware session.

- [x] Prove a real chat turn influences `EmotionEngine` and reaches the robot as
  the same validated category, valence, arousal, turn ID, and ordered sequence.
- [x] Validate neutral -> Joy, Joy -> neutral, and a mid-paw Joy -> unsupported
  request that lands safely, returns through exact neutral, and remains neutral.
- [x] Validate neutral -> Sadness, Sadness -> neutral, and mid-loop Sadness ->
  another accepted reaction through exact neutral.
- [x] Validate neutral -> Fear, Fear -> neutral, and mid-loop Fear -> another
  accepted/fallback reaction through exact neutral.
- [x] Validate neutral -> Anger, Anger -> neutral, and mid-paw Anger -> newest
  accepted/fallback request through the canonical-reset path.
- [ ] Run a bounded nine-category chat sweep. Neutral, Joy, Sadness, Fear, and
  Anger must be visually recognizable as their accepted physical reactions;
  Affection, Curiosity, Disgust, and Surprise must visibly remain on neutral
  breathing while status preserves each requested category.
- [x] Verify rapid turns never start an obsolete pending reaction and never
  leave a paw raised, residual offset, second SDK owner, safety fault, or
  unreleased process after shutdown.
- [ ] Record separate operator verdicts for conversation influence, correct
  reaction selection/fallback, transition quality, and final stationary
  release. Joint movement or topic publication alone is not a pass.

## Physical integration evidence — 2026-09-20

The authorized runs used the physical Lite3, not Gazebo. Every session began
from state/gait/motion `1/0/0`, zero error flags, centered Retroid input, STOP
false, fresh `0x0901`/`0x0906` records, active safety services, and no competing
runner or ownership marker. The development computer retained chat and
`EmotionEngine`; the perception computer remained the sole MotionSDK owner.

- Neutral arrived as `turn-000001`, state sequence `2`, transport sequence
  `266`, and selected `neutral_animal_breath` with correlated status.
- Final Joy uses the accepted 50 mm lift with 30 mm left and 35 mm right
  rearward support shifts. Two consecutive loops passed: left unload/landing
  `3.863/34.021 N` then `4.987/34.137 N`; right unload/landing
  `6.838/30.434 N` then `1.225/30.249 N`. A later Neutral request arrived
  during a left-paw cycle; the paw unloaded to `3.060 N`, landed at `28.840 N`,
  and completed the 1.5-second return plus 0.35-second hold without releasing
  ownership. A Surprise request during a right-paw cycle similarly landed at
  `24.124 N`, then status preserved `requested_emotion=surprise` while resolving
  to Neutral with `fallback_active=true`.
- Sadness completed its full planted bow/heave/recovery loop with four supports,
  then accepted a mid-loop Neutral request and completed the same exact-neutral
  contract under uninterrupted ownership.
- Fear completed two full planted five-second loops, then retargeted through
  exact neutral. A later Fear request transitioned to Anger under the same
  owner through the validated chat input and `EmotionEngine` path.
- The first normal Anger repeat missed the unchanged left unload threshold by
  about `0.009 N`. Increasing only the left rearward support shift from 20 to
  25 mm retained every lift, contact, landing, and four-support gate. The
  revised normal path then passed two complete loops: left unload/landing
  `5.319/29.136 N` and `5.234/28.739 N`; right `0.836/26.034 N` and
  `0.327/25.891 N`. A third loop also completed both placements before the
  battery interlock fired during the final hold.
- At 74%, below the explicit 75% session floor, the battery safety gate aborted
  immediately and released SDK ownership. Fresh postflight remained
  state/gait/motion `1/0/0`, errors `0`, four estimated supports, no runner,
  and no ownership marker. That interrupted Disgust request was not counted as
  a pass.

After the operator explicitly confirmed that the remaining battery was
acceptable, the unfinished gates were rerun with the existing project default
`HARDWARE_MINIMUM_BATTERY=25`; no source threshold and no contact, attitude,
tracking, feedback, state, STOP, watchdog, or ownership gate changed.

- The resumed session started at battery 64% with a valid 728-sample,
  `124.697 N` contact baseline. Anger ran normally, then a Disgust request
  completed the lower/settle, 1.5-second canonical return, and 0.35-second
  hold. Status preserved `requested_emotion=disgust`, resolved and activated
  `neutral_animal_breath`, reported `fallback_active=true`, four supports, no
  fault, and uninterrupted ownership.
- Affection, Curiosity, and Surprise were then selected in turn. Each retained
  its truthful requested category while resolving to Neutral with reason
  `physical_reaction_not_accepted`; the already-active neutral profile did not
  restart needlessly. Together with Disgust, this completed all four physical
  fallback checks.
- During a later Anger sequence, Affection was requested and replaced 0.4
  seconds later by Sadness. Anger completed its current placements and exact-
  neutral transition, then started Sadness directly; the obsolete Affection
  target never started. A final Neutral request cancelled the remaining
  Sadness repeat and completed its exact-neutral recovery.
- That session released with no safety fault. Fresh postflight was
  state/gait/motion `1/0/0`, battery 59%, errors zero, centered fresh Retroid,
  STOP false, no runner, and no ownership marker.
- A final independent session started at battery 59% with a valid 716-sample,
  `123.592 N` baseline. Fear was requested during an active Sadness loop.
  Sadness completed recovery and the exact 1.5 + 0.35-second neutral contract,
  then Fear started directly with four supports. The final Neutral request
  completed Fear's 1.5-second recovery and 0.35-second hold. Correlated status
  showed state sequence `6`, transport sequence `1493`, Neutral active, four
  supports, no fault, and SDK ownership before shutdown.
- Final release reported maximum feedback age/update gap
  `148.544/149.037 ms`, four bounded pause/recoveries, no robot safety fault,
  and a removed ownership marker. Fresh postflight was state/gait/motion
  `1/0/0`, battery 54%, errors zero, roll/pitch `-0.070/1.193 deg`, centered
  fresh Retroid, STOP false, released status, and no SDK owner, lock, or runner.

The interactive client also exposed an ordering race: the 2 Hz heartbeat could
overwrite the one-shot assistant appraisal while the client waited, even though
the robot had already received the state. `emotion_chat.py` now retains the
matching assistant state for the active turn. A regression test reproduces the
assistant-then-heartbeat ordering. This fix passes the shared unit and ROS
integration suites. It also completed five consecutive interactive turns over
the real hardware uplink without actuation: Fear, Affection, Curiosity, Disgust,
and Surprise. The last arrived at the robot receiver as schema `1.1`, state
sequence `10`, transport sequence `1528`, and matching `turn-000005`. The
subsequent actuated sessions completed the corresponding fallback sweep.

That check also exposed and fixed the standalone tunnel target: its prior
`ClearAllForwardings=yes` option silently removed its own `-L` forward. The
corrected target opened `127.0.0.1:8767`, and the five turns above traversed it.

The pre-correction aarch64 runner used for the following live sessions passed
all nine native suites and was installed at SHA-256
`c2723ef4140a1bda88d18d6bfd09494febb2f2dea8d8db3f9d32b1e683a8025a`.
Its fault precedence also ensures a hard battery/attitude/state safety abort is
not mislabeled as a choreography support-recovery failure.

### Live OpenAI chat retry — 2026-09-20

The first physical OpenAI attempt was not counted as a pass. The user appraisal
reached the robot as Joy (`turn-000001`, valence `0.7`, arousal `0.6`), but the
interactive client timed out before a correlated assistant response. Joy safely
completed left/right unload and landing, then the next left unload retained
`9.047 N` and failed the unchanged gate. The runner lowered and landed that paw
at `34.453 N`, restored four supports, and released with no robot safety fault.
Fresh postflight was state/gait/motion `1/0/0`, battery 48%, errors zero, STOP
false, and no SDK owner or runner.

Isolation reproduced an `APIConnectionError`: WSL and the sidecar intermittently
could not resolve `api.openai.com`. The sidecar launch now supplies explicit
`1.1.1.1` and `8.8.8.8` resolvers, and the interactive client's bounded wait is
derived from every configured provider attempt plus fallback margin instead of
ending before that window. All 42 package unit tests passed. Two fresh
`emotion-openai-live-smoke` runs then passed with real Responses API streams in
`8.239 s` and `12.015 s`.

The authorized physical retry used the planted-only temporary allowlist
`neutral,sadness,fear`. Preflight was state/gait/motion `1/0/0`, battery 45%,
errors zero, centered fresh Retroid, STOP false, and no owner. The 676-sample
contact baseline measured `122.895 N`. An ordinary interactive message streamed
a real OpenAI reply and produced the matching assistant appraisal
`fear`, valence `-0.799`, arousal `0.748`, with
`chat_backend=openai`. Robot status preserved the same `turn-000001`, requested,
resolved, and activated `fear_planted_flinch_cower`, four supports, no fault,
and uninterrupted SDK ownership.

A second live OpenAI turn, `event:neutral`, reported
`chat_backend=openai`, state sequence `4`, `turn-000002`, and returned Fear over
the exact 1.5-second recovery plus 0.35-second Neutral hold. Before shutdown,
status showed `neutral_animal_breath`, four supports, and no fault. All seven
bounded feedback pauses recovered; maximum feedback age/update gap was
`152.142/152.692 ms` and maximum pause was `80.016 ms`. Release removed the SDK
marker with no robot safety fault. Fresh postflight was state/gait/motion
`1/0/0`, battery 41%, errors zero, roll/pitch `-0.188/1.454 deg`, centered fresh
Retroid, STOP false, released Neutral status, and no SDK owner, lock, or runner.

### Follow-up accepted-profile OpenAI session — 2026-09-20

A later explicitly authorized session used the temporary accepted-profile
allowlist `neutral,joy,sadness,fear,anger` at scale `1.0` and the unchanged 25%
hard battery floor. Read-only preflight and acquisition both began at
state/gait/motion `1/0/0`, battery 37%, errors zero, STOP false, compatible
fresh telemetry, active safety services, no competing runner, and the installed
runner SHA-256 recorded above. The exact-stand contact hold accepted 694 samples
and reported `124.732 N`; per-foot baseline values in status were
`28.4165/24.2228/35.9225/38.4533 N`.

All six turns streamed real OpenAI responses and printed
`chat_backend=openai`. A natural-language sad message remained Neutral at
valence/arousal `-0.420/0.300`; this was a valid deterministic appraisal, not a
transport failure. Explicit `event:sadness`, `event:fear`, `event:anger`, and
`event:neutral` turns then selected the corresponding accepted profiles under
one uninterrupted SDK owner. Sadness retained four supports through every
observed bow/heave cycle. Fear entered only after Sadness recovery and the
1.5-second plus 0.35-second exact-neutral transition. Anger did the same from
Fear; its first complete left/right pair confirmed unload at `4.910/0.732 N`
and restored four-foot landing at `27.005/25.937 N`. Subsequent repeated Anger
placements also retained their unload and landing gates. The final Neutral turn
completed Anger recovery and the exact-neutral hold before shutdown.

Battery declined from 37% to 30%, so Joy was deliberately not requested after
the immediately preceding session's repeated-left unload failure. Nine bounded
feedback pauses all recovered; maximum age/gap was `148.692/147.822 ms` and
maximum pause was `78.006 ms`. Operator Ctrl-C produced the expected termination
status and exit 130 after release. The runner reported no robot safety fault,
removed the ownership marker, and left no process. Fresh postflight was
state/gait/motion `1/0/0`, battery 30%, errors zero, roll/pitch
`0.242/0.008 deg`, STOP false, four estimated supports, and released Neutral
status. This adds functional evidence; the operator's consolidated visual
verdict and default-allowlist decision remain open.

### Natural-language Joy repeat regression — 2026-09-20

A later ordinary OpenAI prompt correctly produced Joy at valence/arousal
`0.700/0.600`. The robot completed one full alternating-paw reaction, with both
unload and landing gates passing. Joy remained the current engine state, so the
runner correctly began the next cycle without requiring another chat turn. The
second left lift retained `12.5215 N`; the hard unload gate rejected it, the paw
landed at `36.1785 N` with four supports, and the runner released without a
robot safety fault. The operator observed that the robot stopped and lay down.

The offline correction keeps Joy active and repeating until the validated
engine category changes or the link becomes stale. State-sequence gating keeps
heartbeats from being mistaken for new semantic updates, but it does not stop
the current profile. A contact miss with confirmed landing/four-foot recovery
now completes the existing 1.5 + 0.35-second exact-Neutral contract and resumes
Joy under the same owner; incomplete landing and every hard safety fault still
release. The complete offline hardware gate passes and no threshold changed. A
no-motion aarch64 build/deployment also passed all nine native suites and
installed SHA-256
`aa449b7883a3baf6ae816fc832dbf3b8b74bb5a1ac88c2e0313f3acab1c8f353`
from a clean sitting/no-owner preflight. The correction is deployed but not
live-validated, so normal Joy recovery remains open and the checked-in allowlist
remains Neutral-only.

### Persistent Anger unload-miss regression — 2026-09-20

The next normal Anger attempt correctly rejected a front-left unload at
`6.24688 N`. Controlled placement, the landing dwell, canonical relatch, and
the four-foot hold all completed; landing was confirmed at `28.4243 N` with
four supports and loads of `28.4243/23.2969/30.0154/38.4726 N`. The runner then
completed the 1.5-second recovery but classified the safely recovered contact
miss as terminal and released ownership. No robot safety fault occurred. One
bounded feedback pause recovered, with maximum age/gap of
`149.276/150.027 ms` and a `74.0034 ms` maximum pause.

The development correction mirrors Joy's non-releasing policy without changing
any gate: normal chat Anger may return a recovered-contact result only after
confirmed target landing and four-foot support, completes the existing 1.5 +
0.35-second exact-Neutral contract, logs `ANGER_CONTACT_MISS_RECOVERED`, and
resumes Anger if it remains current. Explicit suites and every incomplete
landing or hard safety failure remain terminal. The full offline hardware gate
passes. Once the robot returned to sitting state `1`, a fresh no-motion
preflight confirmed gait/motion `0/0`, battery 74%, errors zero, centered fresh
Retroid, STOP false, and no owner. The clean aarch64 build passed all nine
native suites and installed SHA-256
`e72a2db71d1f569b55d5c5af10b82c6b48a6a850513bb71aa3bacd9a84d4e5cd`.
Postflight remained `1/0/0`, battery 73%, errors zero, STOP false, and released.
No motion command was sent, so live validation remains pending.

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
  exclusive ownership, no safety fault, and an operator visual verdict on the
  physical Lite3; a Gazebo run cannot satisfy this criterion.
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
