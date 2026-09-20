# Physical Affection: Bow and Gentle Paw Offer

[← Emotion ticket index](README.md) ·
[Reference: accepted joy machinery](TICKET_JOY_FRONT_PAW.md) ·
[Previous: Joy](TICKET_JOY_FRONT_PAW.md) ·
[Next: Curiosity](TICKET_CURIOSITY.md)

## Status — planned

The intended choreography is specified, but the final MotionSDK profile,
normal emotion-state integration, aarch64 verification, physical commissioning,
and operator acceptance remain incomplete. Existing planted affection motion is
a prototype/fallback and is not completion evidence for this ticket.

## Goal and emotional read

Affection should read as a calm dog inviting contact: a gentle forward bow,
soft lateral weight transfer, one offered front paw, a warm pause, and an
unhurried return. It must be slower and softer than joy, without joy's quick
left-right alternation.

Because Lite3 has no actuated head or tail, the emotional signal must come from
body pitch, low amplitude roll, a deliberate single-paw offer, and timing.

## Proposed five-second loop

These timings and amplitudes are initial implementation targets, not physically
commissioned values:

1. **Invite — 0.60 s:** begin from exact neutral and add a small forward bow.
2. **Transfer — 0.80 s:** shift rearward and away from the selected front paw
   while all four contacts remain established.
3. **Offer — 0.80 s:** lift that paw approximately 25–35 mm with quintic IK.
4. **Warm hold — 0.80 s:** keep the paw offered and add only a very slow,
   low-amplitude body sway.
5. **Return paw — 0.80 s:** lower gently and confirm landing.
6. **Relax — 1.20 s:** restore four-foot support and blend back to the neutral
   entrance pose.

Alternate the offered side only between completed loops. Never swap sides while
a paw is raised.

## Contact and motion requirements

- Reuse joy's torque-derived baseline, unload hysteresis, two-strong-support /
  70 N aggregate support gate, landing confirmation, and four-foot recovery.
- Generate the bow and paw path through tested Cartesian leg IK; do not emulate
  a bow by sending locomotion or a vendor action.
- Keep lift, body transfer, joint velocity, acceleration, and tracking limits
  individually configurable until physical tuning is accepted.
- A missed unload still lowers and settles the paw before reporting failure.
- Maintain zero world-frame velocity, yaw command, and torque feed-forward.

## Transition behavior

On any category request during the offer or hold, store only the newest target,
finish lowering and settling, return to canonical stand over 1.5 seconds, hold
exact neutral for 0.35 seconds, then start the pending emotion. STOP and safety
faults retain immediate hold/release behavior.

## Implementation checklist

- [x] Emotional intent and candidate phase order documented.
- [ ] Add an `affection` phased profile to the official runner.
- [ ] Add configurable bow, transfer, lift, hold, lower, and relax parameters.
- [ ] Reuse the contact estimator and support/landing gates without a second
  sender.
- [ ] Add left/right IK, workspace, finite sample, velocity, acceleration, seam,
  and exact-recovery tests.
- [ ] Add same-emotion loop and mid-offer retargeting state-machine tests.
- [ ] Wire validated normal `affection` state to this profile after the neutral
  transition.
- [ ] Pass local and aarch64 native test suites.
- [ ] Complete one bounded physical loop with correlated state, joint, IMU,
  contact estimate, STOP, tracking, and ownership telemetry.
- [ ] Verify clean landing, exact-neutral recovery, and clean release.
- [ ] Obtain operator confirmation that it reads as affection, not joy or
  curiosity.

## Acceptance criteria

- Exactly one paw is offered per loop and its unload/landing are confirmed.
- The motion is visibly gentle, sustained, and distinct from joy's alternating
  rhythm.
- Body pitch contributes a recognizable bow without planar displacement.
- The robot remains stable on accepted supports and restores all four contacts.
- A direct transition to any other emotion visibly passes through exact neutral.
- No fault, residual movement, stale continuation, or owner conflict remains.

## Non-goals

- Simulating petting through repeated paw strikes.
- Reusing joy at a lower speed without an affectionate bow/hold.
- Walking forward, kneeling, sitting, or using a vendor trick/action.
- Interpreting implementation or offline tests as physical acceptance.
