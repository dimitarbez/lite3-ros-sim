# Lite3 Physical Emotion Tickets

## Ticket index

| Emotion | Ticket | Current state |
| --- | --- | --- |
| Neutral | [Neutral breathing](TICKET_NEUTRAL_BREATHING.md) | Implemented and operator-accepted |
| Joy | [Alternating front-paw joy](TICKET_JOY_FRONT_PAW.md) | Physical gesture accepted; normal chat integration remains |
| Affection | [Affection bow and paw offer](TICKET_AFFECTION.md) | Planned |
| Curiosity | [Curiosity lean and paw hover](TICKET_CURIOSITY.md) | Planned |
| Sadness | [Sadness lowered posture](TICKET_SADNESS.md) | Planned |
| Disgust | [Disgust recoil](TICKET_DISGUST.md) | Planned |
| Fear | [Fear crouch and guarded hovers](TICKET_FEAR.md) | First candidate rejected; redesign failed first unload gate |
| Surprise | [Surprise rise and freeze](TICKET_SURPRISE.md) | Planned |
| Anger | [Anger controlled front-paw stomps](TICKET_ANGER.md) | Canonical-reset 3-cycle suite passed; live chat retarget remains |

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
  and reported no safety fault.
- **Fear remains unaccepted.** One first-candidate single-hover run passed
  unload/landing, but the full loop failed its first unload on a new baseline,
  and the operator said the motion looked nothing like fear. All trials
  recovered and released safely. A stronger whole-body flinch/recoil redesign
  passed local and aarch64 tests, but its bounded physical run retained
  `7.264 N` on the front-left paw and failed the unload gate. It landed,
  recovered, and released without a safety fault or feedback pause; no second
  paw was attempted. A separate all-feet-planted 15-second visual diagnostic
  subsequently completed three exact five-second cycles, restored four
  supports each time, and released without a safety fault. It does not satisfy
  the paw-unload or operator-acceptance gates. The normal allowlist is unchanged.
- **Anger is implemented and its single-placement and alternating physical
  suites passed.** Both paws produced confirmed unload and four-foot landing,
  with clean release and no safety fault. A later 15-second repeat passed two
  loops but failed closed when the third loop's final right landing re-latched
  only three supports. The operator subsequently reported that the choreography
  looked good, so visual acceptance is complete. An x/y-recenter hardening passed
  all seven aarch64 suites but failed the right landing in live cycle 2. The
  replacement returns fully to canonical stand and holds 0.35 seconds between
  paws; it passed all seven aarch64 suites and an authorized three-cycle physical
  run with every landing restoring four supports. Normal chat selection and
  retargeting remain live-unvalidated, so the checked-in allowlist stays
  neutral-only. Affection, curiosity, sadness, disgust, and surprise remain to
  be implemented; Fear still requires physical telemetry and visual acceptance.

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
- [ ] Select the accepted paw trajectory from a normal validated `joy` state in
  the continuous chat-driven runner; it is currently proven through the
  explicit bounded suite.
- [ ] Prove a mid-gesture emotion change finishes paw lowering and settling,
  returns to exact neutral for `1.5 + 0.35 s`, and then starts only the newest
  pending emotion.
- [ ] Complete live neutral→joy, joy→neutral, and joy→another-emotion transition
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
- [ ] Implement the final phased IK/contact-aware profile.
- [ ] Add native trajectory, bound, recovery, and transition tests.
- [ ] Integrate normal emotion-state selection through exact neutral.
- [ ] Pass aarch64 build/tests and a bounded physical telemetry run.
- [ ] Receive operator visual acceptance as recognizably sad.

### [Disgust](TICKET_DISGUST.md)

- [x] Intended physical choreography specified below.
- [ ] Implement the final phased IK/contact-aware profile.
- [ ] Add native trajectory, bound, recovery, and transition tests.
- [ ] Integrate normal emotion-state selection through exact neutral.
- [ ] Pass aarch64 build/tests and a bounded physical telemetry run.
- [ ] Receive operator visual acceptance as recognizably disgusted.

### [Fear](TICKET_FEAR.md)

- [x] Intended physical choreography specified below.
- [x] Implement the final phased IK/contact-aware profile.
- [x] Add native trajectory, bound, recovery, and transition tests.
- [x] Integrate normal emotion-state selection through exact neutral.
- [x] Pass local and aarch64 build/tests.
- [ ] Pass a bounded physical telemetry run.
- [ ] Receive operator visual acceptance as recognizably fearful.

### [Surprise](TICKET_SURPRISE.md)

- [x] Intended physical choreography specified below.
- [ ] Implement the final phased IK/contact-aware profile.
- [ ] Add native trajectory, bound, recovery, and transition tests.
- [ ] Integrate normal emotion-state selection through exact neutral.
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
- [ ] Physically validate normal chat selection and retargeting before changing
  the normal allowlist.

### Shared transition engine

- [x] Exact-neutral transition behavior and timing are specified.
- [x] The existing continuous runner supports one pending target and exact
  neutral transitions for the planted profile engine.
- [ ] Extend that coordinator to every final IK/contact-aware profile, including
  an emotion request received while a paw is raised.
- [ ] Pass automated rapid-retarget, same-category, stale-link, STOP, and fault
  recovery cases with the final profiles.
- [ ] Pass a live nine-emotion chat sweep in which every category change goes
  through exact neutral before the next animation begins.

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
- **Sadness:** a long, low, slow crouch with reduced body height, subdued roll,
  and a delayed single-paw reposition/hover if contact tests support it. No
  bounce or sharp landing.
- **Disgust:** a clear rearward/sideways recoil followed by one front-paw
  withdrawal and guarded return. It must remain stationary in world position.
- **Fear:** a fast but bounded crouch-and-recoil followed by small alternating
  weight shifts or low paw hovers. No high-frequency joint chatter and no loss
  of the support/load gate.
- **Surprise:** a quick compression/rise with a conspicuous front-body lift or
  sequential front-paw reaction, followed by a short freeze and controlled
  landing. No airborne four-foot jump in this ticket.
- **Anger:** deliberate alternating front-paw stomps: unload one paw, lift it
  with the same contact-gated machinery as joy, lower it firmly but within a
  bounded velocity/acceleration profile, confirm landing, then alternate. This
  is expressive placement, not an impact or torque strike.

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

- Neutral, joy, and the Anger choreography have operator visual acceptance as
  of this update. Only neutral is enabled for normal hardware use; Joy remains
  suite-only, and Anger's canonical-reset commissioning suite is physically
  accepted while its normal chat/retarget path remains disabled and
  live-unvalidated. Do not describe the other six as implemented or
  commissioned.
- True airborne hops, gait/action jump primitives, world-frame locomotion, and
  forceful impacts remain out of scope. Low-clearance, contact-confirmed paw
  lifts and controlled placements are in scope for the remaining emotions.
- The configured 25% battery abort floor remains the current project default.
- Physical commissioning is never implied by software or offline tests; every
  future live run still needs explicit authorization in that task.
- Preserve current nested-repository work and do not advance submodules or
  wrapper gitlinks implicitly.
