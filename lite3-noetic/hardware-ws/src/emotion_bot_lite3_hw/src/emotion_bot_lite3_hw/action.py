"""Deterministic dynamic-action phase machines shared by tests and the SDK runner."""

from __future__ import annotations

from dataclasses import dataclass


HOP_PHASES = ("stable_stance", "crouch", "thrust", "flight", "landing", "recovery")
STOMP_PHASES = ("stable_stance", "support_shift", "unload", "lift", "strike", "contact", "recovery")


@dataclass
class ActionObservation:
    stable: bool = False
    four_contacts: bool = False
    all_unloaded: bool = False
    selected_unloaded: bool = False
    selected_contact: bool = False
    attitude_ok: bool = True
    timing_ok: bool = True
    telemetry_ok: bool = True


class ActionMachine:
    def __init__(self, action: str):
        if action not in ("hop", "stomp"):
            raise ValueError("unsupported action")
        self.action = action
        self.phases = HOP_PHASES if action == "hop" else STOMP_PHASES
        self.index = 0
        self.failed = False
        self.recovery_verified = False

    @property
    def phase(self):
        return self.phases[self.index]

    @property
    def complete(self):
        return self.recovery_verified

    def update(self, observation: ActionObservation):
        if not observation.attitude_ok or not observation.timing_ok or not observation.telemetry_ok:
            self.failed = True
            return "release"
        required = {
            "stable_stance": observation.stable and observation.four_contacts,
            "crouch": True,
            "thrust": True,
            "flight": observation.all_unloaded,
            "landing": observation.four_contacts,
            "support_shift": True,
            "unload": observation.selected_unloaded,
            "lift": observation.selected_unloaded,
            "strike": True,
            "contact": observation.selected_contact,
            "recovery": observation.stable and observation.four_contacts,
        }[self.phase]
        if required and self.index < len(self.phases) - 1:
            self.index += 1
        elif required and self.phase == "recovery":
            self.recovery_verified = True
        return self.phase
