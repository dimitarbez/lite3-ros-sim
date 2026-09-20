# Physical Surprise: Rapid Rise, Paw Reaction, and Freeze

[← Emotion ticket index](README.md) ·
[Reference: accepted joy machinery](TICKET_JOY_FRONT_PAW.md) ·
[Previous: Fear](TICKET_FEAR.md) ·
[Next: Anger](TICKET_ANGER.md)

## Status — planned

The intended non-airborne surprise gesture is documented. Final trajectory
implementation, tests, chat selection, physical commissioning, and operator
acceptance remain open. The previous planted hop proxy is not the final profile.

## Goal and emotional read

Surprise should read as a sudden whole-body reaction: quick compression, rapid
rise, a conspicuous but supported front-paw response, a short freeze, and a
controlled settle. It should be expansive and brief, unlike fear's guarded low
posture or joy's rhythmic alternating taps.

This ticket does not authorize an airborne jump. At least the accepted support
set remains loaded throughout every paw reaction.

## Proposed four-second loop

Initial targets, subject to offline and physical tuning:

1. **Preload — 0.40 s:** bounded symmetric compression with all feet loaded.
2. **Rise — 0.45 s:** rapid but quintic extension to a taller supported posture.
3. **React — 0.60 s:** transfer rearward/laterally and lift one front paw
   approximately 35–45 mm.
4. **Freeze — 0.65 s:** hold the tall asymmetric pose nearly still.
5. **Place — 0.60 s:** lower and confirm landing.
6. **Settle — 1.30 s:** restore four-foot support and return to the neutral
   entrance pose without a rebound loop.

The selected paw alternates between completed loops. If live visual review does
not read as surprise, tune rise/freeze contrast before adding more lift.

## Contact and motion requirements

- Reuse joy's four-foot baseline, unload, strong-support/aggregate-load,
  landing, and restored-contact gates.
- Test preload plus rapid rise plus paw lift as one bounded Cartesian trajectory.
- Apply explicit velocity and acceleration ceilings to the rise and landing.
- Require all feet loaded during preload/rise; only the selected paw may unload
  during react/freeze/place.
- Keep world-frame velocity, yaw, action/gait, torque feed-forward, and external
  wrench commands zero.

## Transition behavior

Retargeting during react/freeze/place completes safe landing and settle before
the global 1.5-second canonical return and 0.35-second exact-neutral hold. A
retarget during preload/rise still returns through neutral; it never branches
directly into another emotion pose. STOP and faults remain immediate.

## Implementation checklist

- [x] Emotional intent and candidate phase order documented.
- [ ] Implement preload, rise, supported paw reaction, freeze, place, and settle.
- [ ] Add configurable rise height, lift, support transfer, and timing bounds.
- [ ] Reuse contact/load gates and exclusive MotionSDK ownership.
- [ ] Test both sides, combined workspace, finite samples, joint bounds,
  velocity/acceleration, seams, and exact recovery.
- [ ] Prove no frame commands or implies an airborne four-foot phase.
- [ ] Add same-category and mid-reaction retargeting tests.
- [ ] Wire validated `surprise` state through exact neutral.
- [ ] Pass local and aarch64 suites.
- [ ] Complete bounded live telemetry with clean landing and release.
- [ ] Obtain operator acceptance as surprise, distinct from joy and fear.

## Acceptance criteria

- Compression-to-rise contrast is visibly sharp but remains bounded.
- Exactly one selected paw unloads and lands with valid support evidence.
- The freeze is readable and stable without oscillation.
- No flight, hop, impact, slip, or gait/action command occurs.
- Any category change passes visibly through exact neutral.

## Non-goals

- A Gazebo-style airborne hop.
- Simultaneously lifting both front paws without separate commissioning.
- Increasing lift when timing/rise contrast is the visual issue.
- Reusing fear's low guarded posture at higher speed.

