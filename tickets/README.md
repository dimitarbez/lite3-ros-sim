# Lite3 Physical Emotion Tickets

## Cross-emotion integration ticket

| Scope | Ticket | Current state |
| --- | --- | --- |
| Emotion chat -> EmotionEngine -> accepted physical reactions | [Chat-driven physical emotion reactions](TICKET_PHYSICAL_EMOTION_CHAT.md) | Live functional sweep passed; consolidated operator verdict and default enable remain |

The integration ticket owns the end-to-end conversational selector: it preserves
the actual EmotionEngine category and chooses an operator-accepted physical
reaction, with explicit neutral fallback for categories that do not yet have
one. The per-emotion tickets below continue to own choreography, safety gates,
physical evidence, and operator visual acceptance.

A 2026-09-20 follow-up live OpenAI session also completed
Neutral -> Sadness -> Fear -> Anger -> Neutral under one SDK owner with every
observed hard gate intact and a clean release. Joy was deliberately skipped as
the battery declined to 30% after a preceding repeated-left unload failure. This
adds functional evidence but does not close the consolidated operator visual
verdict or change the Neutral-only default allowlist.

A subsequent operator-driven natural-language Joy turn completed one accepted
left/right gesture and correctly continued because Joy remained current. The
second left paw missed the unchanged unload gate, safely landed with four
supports, but the runner classified that recovered contact miss as terminal and
released; the robot then lay down. The runner is now hardened offline to keep
every current emotion persistent and, after a safely recovered Joy unload miss,
complete canonical Neutral and resume Joy under the same owner. The correction
passed the complete offline gate and is deployed at SHA-256
`aa449b7883a3baf6ae816fc832dbf3b8b74bb5a1ac88c2e0313f3acab1c8f353`,
but is not physically revalidated.

## Ticket index

| Emotion | Ticket | Current state |
| --- | --- | --- |
| Neutral | [Neutral breathing](TICKET_NEUTRAL_BREATHING.md) | Implemented and operator-accepted |
| Joy | [Alternating front-paw joy](TICKET_JOY_FRONT_PAW.md) | Gesture accepted; persistent chat recovery passes offline, live revalidation pending |
| Affection | [Affection bow and paw offer](TICKET_AFFECTION.md) | Planned |
| Curiosity | [Curiosity lean and paw hover](TICKET_CURIOSITY.md) | Planned |
| Sadness | [Sadness lowered posture](TICKET_SADNESS.md) | Planted bow accepted; normal physical selection and retarget passed |
| Disgust | [Disgust recoil](TICKET_DISGUST.md) | Planned |
| Fear | [Fear crouch and guarded reaction](TICKET_FEAR.md) | Planted animation accepted; normal physical selection and retarget passed |
| Surprise | [Surprise rise and freeze](TICKET_SURPRISE.md) | Planned |
| Anger | [Anger controlled front-paw stomps](TICKET_ANGER.md) | Gesture accepted; persistent contact-miss recovery deployed, live revalidation pending |

All tickets inherit the [shared transition rule](#non-negotiable-transition-rule):
finish any limb recovery, return to canonical stand over 1.5 seconds, hold exact
neutral for 0.35 seconds, then start only the newest pending emotion.

## Status — updated 2026-09-20

- **Neutral is operator-accepted:** retain the commissioned 3.25-second
  animal-like breathing loop unchanged.
- **Joy is operator-accepted:** use the torque-gated, alternating front-paw
  gesture documented in [the joy ticket](TICKET_JOY_FRONT_PAW.md). The accepted bounded
  suite completed five seconds of neutral followed by five seconds of joy,
  confirmed unload and landing for both front paws, released SDK ownership,
  and reported no safety fault. A later natural-language turn exposed unsafe
  recovery semantics rather than an unsafe motion threshold: Joy correctly
  remained active, the next left unload missed its hard gate, and the terminal
  release was followed by the robot lying down. The offline fix keeps Joy
  repeating while it remains current and uses canonical Neutral as a bounded
  recovery waypoint before resuming Joy after a safely recovered miss; physical
  revalidation is still pending.
- **Sadness's planted bow is operator-accepted.** The paw candidate failed front-left
  unload at `8.875 N`. Three planted redesigns passed telemetry but were judged
  too subtle. A deeper 70 mm front-to-rear bow then reached `10.006 deg` during
  its sink and correctly tripped the hard 10-degree attitude gate; it released
  safely and the operator judged that partial bow too deep. A 56 mm midpoint
  also reached `10.005 deg` during its sink and failed closed before the
  heaves. The final front-only 48 mm bow passed its single run and received the
  operator's positive visual verdict. An unchanged two-cycle 15.6-second repeat
  retained four supports through all six heaves, recovered, and released with
  no safety fault. Normal physical selection and Sadness-to-Neutral retargeting
  now pass; the global allowlist remains unchanged pending the complete sweep.
- **Fear is implemented and operator-accepted.** One first-candidate single-hover run passed
  unload/landing, but the full loop failed its first unload on a new baseline,
  and the operator said the motion looked nothing like fear. All trials
  recovered and released safely. A stronger whole-body flinch/recoil redesign
  passed local and aarch64 tests, but its bounded physical run retained
  `7.264 N` on the front-left paw and failed the unload gate. It landed,
  recovered, and released without a safety fault or feedback pause; no second
  paw was attempted. A separate all-feet-planted 15-second visual diagnostic
  subsequently completed three exact five-second cycles, restored four
  supports each time, and released without a safety fault. The operator
  confirmed that the final Fear animation worked correctly on the robot and
  visually accepted it. Normal physical selection, repeated loops,
  Fear-to-Neutral, and Fear-to-Anger retargeting now pass; the global allowlist
  remains unchanged pending the complete sweep.
- **Anger is implemented and its single-placement and alternating physical
  suites passed.** Both paws produced confirmed unload and four-foot landing,
  with clean release and no safety fault. A later 15-second repeat passed two
  loops but failed closed when the third loop's final right landing re-latched
  only three supports. The operator subsequently reported that the choreography
  looked good, so visual acceptance is complete. An x/y-recenter hardening passed
  all seven aarch64 suites but failed the right landing in live cycle 2. The
  replacement returns fully to canonical stand and holds 0.35 seconds between
  paws; it passed all seven aarch64 suites and an authorized three-cycle physical
  run with every landing restoring four supports. Normal Anger selection and
  two consecutive repeated loops now also pass after a bounded left support
  transfer correction. A later Anger-to-Disgust run completed the paw
  placement, exact-neutral return, and observable Neutral fallback. The
  later persistent run exposed a policy regression after a failed unload:
  controlled landing and all four supports were confirmed, but ownership was
  still released. The offline correction now takes the existing canonical
  Neutral recovery and resumes Anger under the same owner only after confirmed
  landing and four-foot support. It changes no threshold, is deployed at
  SHA-256 `e72a2db71d1f569b55d5c5af10b82c6b48a6a850513bb71aa3bacd9a84d4e5cd`,
  and still needs physical revalidation. The
  checked-in allowlist stays neutral-only pending the consolidated operator
  verdict. Affection, curiosity, disgust, and surprise remain Neutral
  fallbacks; all four physical fallback/status checks and the rapid newest-only
  retarget passed.

The remaining emotions must be implemented to the same standard as joy:
recognizable whole-body choreography, explicit phases, IK-generated motion,
measured support/load transfer where a paw is lifted, bounded recovery, live
telemetry, and operator visual acceptance. “Like joy” means the same physical
implementation quality and control path, not reusing the joy paw-tap motion for
every category.

## Per-emotion implementation checklist

A checked item means there is corresponding implementation or recorded test
evidence. An emotion is complete only when every item under it is checked.

### [Neutral](TICKET_NEUTRAL_BREATHING.md)

- [x] Final breathing choreography implemented in the official MotionSDK
  runner.
- [x] Native/offline verification completed.
- [x] Physical initialization, stand, breathing, and release completed without
  a safety fault.
- [x] Operator visually accepted the neutral breathing animation.
- [x] Available as the canonical resting profile and transition waypoint.

### [Joy](TICKET_JOY_FRONT_PAW.md)

- [x] Final alternating front-paw choreography and contact gates implemented.
- [x] IK, force-estimator, support, and MotionSDK tests pass locally and on the
  robot's aarch64 perception computer.
- [x] Five-second physical joy suite confirmed both paw unloads, both landings,
  no safety fault, and clean release.
- [x] Operator visually accepted the joy animation.
- [x] Select the accepted paw trajectory from a normal validated `joy` state in
  the continuous chat-driven runner behind the allowlist.
- [x] Prove offline that a mid-gesture emotion change finishes paw lowering and settling,
  returns to exact neutral for `1.5 + 0.35 s`, and then starts only the newest
  pending emotion.
- [x] Complete live neutral→joy, joy→neutral, and joy→another-emotion transition
  tests through the normal chat path.

### [Affection](TICKET_AFFECTION.md)

- [x] Intended physical choreography specified below.
- [ ] Implement the final phased IK/contact-aware profile.
- [ ] Add native trajectory, bound, recovery, and transition tests.
- [x] Integrate normal emotion-state selection through exact neutral offline.
- [ ] Pass aarch64 build/tests and a bounded physical telemetry run.
- [ ] Receive operator visual acceptance as recognizably affectionate.

### [Curiosity](TICKET_CURIOSITY.md)

- [x] Intended physical choreography specified below.
- [ ] Implement the final phased IK/contact-aware profile.
- [ ] Add native trajectory, bound, recovery, and transition tests.
- [x] Integrate normal emotion-state selection through exact neutral offline.
- [ ] Pass aarch64 build/tests and a bounded physical telemetry run.
- [ ] Receive operator visual acceptance as recognizably curious.

### [Sadness](TICKET_SADNESS.md)

- [x] Intended physical choreography specified below.
- [x] Implement the final phased IK/contact-aware profile.
- [x] Add native trajectory, bound, recovery, and transition tests.
- [x] Integrate normal emotion-state selection through exact neutral.
- [x] Pass local and aarch64 build/tests.
- [x] Complete a bounded planted-bow telemetry run and clean release.
- [x] Redesign without rear extension and pass single and two-cycle physical
  runs.
- [ ] Rework and pass the optional withdrawn-paw unload gate.
- [x] Receive operator visual acceptance as recognizably sad.
- [x] Select the accepted planted bow and validate retargeting offline.
- [x] Complete live Sadness-to-Fear retarget through the exact-neutral contract.

### [Disgust](TICKET_DISGUST.md)

- [x] Intended physical choreography specified below.
- [ ] Implement the final phased IK/contact-aware profile.
- [ ] Add native trajectory, bound, recovery, and transition tests.
- [x] Integrate normal emotion-state neutral fallback through exact neutral.
- [ ] Pass aarch64 build/tests and a bounded physical telemetry run.
- [ ] Receive operator visual acceptance as recognizably disgusted.

### [Fear](TICKET_FEAR.md)

- [x] Intended physical choreography specified below.
- [x] Implement the final phased IK/contact-aware profile.
- [x] Add native trajectory, bound, recovery, and transition tests.
- [x] Integrate normal emotion-state selection through exact neutral.
- [x] Pass local and aarch64 build/tests.
- [x] Pass a bounded physical telemetry run.
- [x] Receive operator visual acceptance as recognizably fearful.
- [x] Select the accepted Fear animation and validate retargeting offline.

### [Surprise](TICKET_SURPRISE.md)

- [x] Intended physical choreography specified below.
- [ ] Implement the final phased IK/contact-aware profile.
- [ ] Add native trajectory, bound, recovery, and transition tests.
- [x] Integrate normal emotion-state neutral fallback through exact neutral.
- [ ] Pass aarch64 build/tests and a bounded physical telemetry run.
- [ ] Receive operator visual acceptance as recognizably surprised.

### [Anger](TICKET_ANGER.md)

- [x] Intended physical choreography specified below.
- [x] Implement the phased contact-confirmed front-paw commissioning profile.
- [x] Add native trajectory, bound, touchdown, recovery, and second-paw gate tests.
- [x] Integrate normal emotion-state selection through exact neutral offline.
- [x] Pass the local and aarch64 build/tests.
- [x] Pass the bounded single-placement and complete alternating physical runs.
- [x] Receive operator visual acceptance as recognizably angry without a
  forceful impact.
- [x] Deploy and physically revalidate the canonical-reset repeated path.
- [x] Physically validate normal Anger selection and repeated loops.
- [x] Physically validate its mid-motion retarget before changing the normal
  allowlist.

### Shared transition engine

- [x] Exact-neutral transition behavior and timing are specified.
- [x] The existing continuous runner supports one pending target and exact
  neutral transitions for the planted profile engine.
- [x] Implement the offline [chat-driven physical emotion integration
  ticket](TICKET_PHYSICAL_EMOTION_CHAT.md), including accepted-profile routing
  and observable neutral fallback for unimplemented categories.
- [x] Extend that coordinator to every accepted final profile, including
  an emotion request received while a paw is raised.
- [x] Pass automated rapid-retarget, same-category, stale-link, STOP, and fault
  recovery cases with the final profiles.
- [x] Pass the telemetry/status portion of a live nine-emotion chat sweep in
  which category changes go through exact neutral before the next animation.
- [ ] Record the operator's consolidated visual verdict for that sweep.

## Non-negotiable transition rule

Every category change passes through exact neutral, including
neutral-to-emotion and emotion-to-emotion changes:

1. Finish lowering any raised paw; never abandon a limb in a lifted phase.
2. Cancel the old repeating choreography.
3. Quintically return from the current command to exact canonical stand over
   1.5 seconds.
4. Hold exact canonical stand for 0.35 seconds.
5. Start only the newest pending emotion from its entrance phase.

Rapid requests replace one pending target without restarting the neutral
return. Same-category updates may change the next loop's affect parameters but
must not restart an in-progress loop. A safety fault, STOP, invalid robot state,
dead feedback, or tracking violation still bypasses conversational transition
timing and enters the immediate verified hold/release path.

## Runtime and interfaces

- Preserve `/emotion_bot/state` schema `1.1`, loopback NDJSON schema `1.0`,
  sequence/replay validation, turn correlation, and the existing chat window.
- Keep one ROS-independent `motion_sdk_expression_runner` as the sole 1 kHz
  official MotionSDK owner for the entire operator-started session.
- Require robot state `1` before acquisition and use the proven
  `RobotStateInit → PreStandUp → StandUp → neutral hold` sequence only once.
- Preserve the ownership marker, independent watchdog, `0x0901` state/error
  interlock, `0x0906` freshness/pause handling, battery and attitude bounds,
  tracking-error limit, authenticated STOP, and deterministic release.
- Keep emotional reasoning on the development computer. The robot-side runner
  alone converts validated state into joint trajectories.
- Publish requested/active emotion, phase, cycle, pending target, link age,
  ownership, feedback state, contact estimate, last fault, and release state on
  read-only `/emotion_bot/hardware/expression_status` JSON `1.0`.
- Keep Retroid posture control and legacy senders in diagnostic-only launches;
  they must never run concurrently with the official expression owner.

## Choreography requirements

All final profiles are declarative, deadline-bounded state machines with
quintic phase interpolation and exact-neutral endpoints. HipX may now be used
inside the tested IK workspace for controlled lateral support transfer; planar
locomotion, yaw, gait/action primitives, torque feed-forward, and external
wrenches remain zero/unused.

### Accepted profiles

| Emotion | Required physical expression | Status |
| --- | --- | --- |
| neutral | Slow animal-like breathing with all four paws planted | Accepted |
| joy | Rear/lateral support transfer, 50 mm front-left paw lift and gentle landing, then front-right, within one five-second loop | Accepted |
| sadness | Front-only 48 mm planted bow with slow heaves and exact recovery | Accepted |
| fear | Planted flinch, recoil, lateral cower, guarded freeze, and exact recovery | Accepted |
| anger | Canonical-reset alternating controlled front-paw placements | Accepted |

The joy implementation is the reference pattern for all remaining work:
baseline in four-foot stand, transfer load, command a tested IK trajectory,
confirm the expected contact change, complete the phase even if a new emotion
arrives, return to four-foot support, and recover through exact neutral.

### Remaining emotion designs

These are implementation targets; their exact amplitudes and timing are not
commissioned until live evidence and operator acceptance are recorded.

- **Affection:** a gentle forward bow/lean with one soft front-paw offer, slow
  hold, gentle landing, and relaxed recovery. It must read as inviting rather
  than excited and must not reuse joy's alternating tempo.
- **Curiosity:** an asymmetric side lean with a single inquisitive front-paw
  hover, brief hold, side change on the next loop, and slow recovery. With no
  actuated head, asymmetry and timing must carry the expression.
- **Disgust:** a clear rearward/sideways recoil followed by one front-paw
  withdrawal and guarded return. It must remain stationary in world position.
- **Surprise:** a quick compression/rise with a conspicuous front-body lift or
  sequential front-paw reaction, followed by a short freeze and controlled
  landing. No airborne four-foot jump in this ticket.

Do not accept a profile merely because joints moved. Each must be visually
recognizable as its intended emotion and measurably distinct from neutral and
joy while keeping the robot stable.

## Contact-gated lifted-paw pattern

Any profile that lifts a paw must reuse the proven joy machinery rather than an
open-loop joint offset:

1. Establish a stable four-foot torque-derived load baseline.
2. Transfer body load inside the tested Cartesian/IK workspace.
3. Lift the selected paw along a bounded trajectory.
4. Confirm target-paw unload over consecutive fresh feedback frames.
5. Require at least two strong non-target supports and at least 70 N aggregate
   non-target support during the accepted diagonal-support phase.
6. Lower with bounded velocity and acceleration; never command an impact.
7. Confirm landing and restored four-foot support before another paw or the
   neutral transition.

A missed unload or landing still executes the lowering/recovery path before
reporting failure. Emotion retargeting cannot skip that recovery.

## Verification and rollout

- Keep deterministic native tests for all profile samples, IK/workspace limits,
  finite values, zero-velocity seams, joint velocity/acceleration, contact
  hysteresis, support sufficiency, left/right behavior, exact-neutral recovery,
  and every abort path.
- Test neutral→emotion, emotion→emotion, rapid retargeting, same-category
  updates, stale-link teardown, malformed/replayed state, shutdown, state `8`,
  feedback pause/recovery, dead feedback, STOP, and watchdog release.
- The fake-Sender integration harness must continue proving one SDK acquisition
  per session, uninterrupted ownership, exact transition timing, status
  publication, and no concurrent sender.
- Build and run the native suite on both the development container and the
  robot's aarch64 perception computer before every physical profile change.
- Commission one remaining emotion at a time under explicit current-task
  authorization. Record the command, source revision/binary checksum, initial
  state, battery, baseline loads, phase durations, unload/landing evidence,
  tracking and feedback maxima, fault result, release result, and operator
  visual verdict.
- A complete emotion passes only after its full bounded loop, exact-neutral
  transition, STOP behavior, and clean end-of-session release all pass and the
  operator says the motion reads correctly.
- Finish with a chat-driven nine-emotion sweep. Direct non-neutral transitions
  must visibly and telemetrically pass through the 1.5-second neutral return and
  0.35-second exact-neutral hold before the new emotion begins.

## Scope boundary

- Neutral, Joy, Sadness, Fear, and Anger have accepted physical choreography.
  Their normal physical selection and live retarget paths have passed. Only
  Neutral remains enabled by default until the consolidated operator verdict
  closes. Do not describe Affection, Curiosity, Disgust, or Surprise as
  implemented choreography; they remain physically verified Neutral
  fallbacks.
- True airborne hops, gait/action jump primitives, world-frame locomotion, and
  forceful impacts remain out of scope. Low-clearance, contact-confirmed paw
  lifts and controlled placements are in scope for the remaining emotions.
- The configured 25% battery abort floor remains the current project default.
- Physical commissioning is never implied by software or offline tests; every
  future live run still needs explicit authorization in that task.
- Preserve current nested-repository work and do not advance submodules or
  wrapper gitlinks implicitly.
