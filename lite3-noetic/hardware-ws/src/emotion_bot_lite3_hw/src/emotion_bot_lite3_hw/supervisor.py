"""Exclusive fail-closed hardware supervisor state machine."""

from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
class State(str, Enum):
    DISARMED = "DISARMED"
    POSTURE_ARMED = "POSTURE_ARMED"
    ACTION_PENDING = "ACTION_PENDING"
    SDK_TAKEOVER = "SDK_TAKEOVER"
    ACTION_ACTIVE = "ACTION_ACTIVE"
    RECOVERY = "RECOVERY"
    FAULT = "FAULT"


@dataclass
class Gates:
    stable_state_6: bool = False
    telemetry_fresh: bool = False
    retroid_axes_zero: bool = False
    joints_healthy: bool = False
    imu_healthy: bool = False
    sdk_joints_healthy: bool = False
    sdk_imu_healthy: bool = False
    ai_link_fresh: bool = False
    sdk_layout_compatible: bool = False
    stop_preemption_verified: bool = False
    trajectories_commissioned: bool = False

    def posture_ready(self) -> bool:
        return all((self.stable_state_6, self.telemetry_fresh, self.retroid_axes_zero,
                    self.joints_healthy, self.imu_healthy, self.ai_link_fresh))

    def dynamic_ready(self) -> bool:
        return self.posture_ready() and all((self.sdk_layout_compatible,
                                             self.sdk_joints_healthy,
                                             self.sdk_imu_healthy,
                                             self.stop_preemption_verified,
                                             self.trajectories_commissioned))


class Supervisor:
    def __init__(self, transmit_enabled: bool = False, dynamic_default: bool = False):
        self.transmit_enabled = bool(transmit_enabled)
        self.dynamic_requested = bool(dynamic_default)
        self.state = State.DISARMED
        self.fault_reason = ""
        self.current_action = None
        self.release_required = False

    def set_armed(self, enable: bool, gates: Gates) -> bool:
        if not enable:
            if self.state in (State.SDK_TAKEOVER, State.ACTION_ACTIVE):
                self.state = State.RECOVERY
                self.release_required = True
            else:
                self.state = State.DISARMED
            return True
        if not self.transmit_enabled or not gates.posture_ready() or self.state == State.FAULT:
            return False
        self.state = State.POSTURE_ARMED
        return True

    def set_dynamic(self, enable: bool, gates: Gates) -> bool:
        if not enable:
            self.dynamic_requested = False
            return True
        if self.state != State.POSTURE_ARMED or not gates.dynamic_ready():
            return False
        self.dynamic_requested = True
        return True

    def request_action(self, action: str, gates: Gates) -> bool:
        if not self.dynamic_requested or self.state != State.POSTURE_ARMED or not gates.dynamic_ready():
            return False
        self.current_action = action
        self.state = State.ACTION_PENDING
        return True

    def posture_neutral(self):
        if self.state == State.ACTION_PENDING:
            self.state = State.SDK_TAKEOVER
            self.release_required = True

    def sdk_acquired(self):
        if self.state == State.SDK_TAKEOVER:
            self.state = State.ACTION_ACTIVE

    def action_finished(self):
        if self.state == State.ACTION_ACTIVE:
            self.state = State.RECOVERY

    def recovered(self, standing: bool):
        if self.state != State.RECOVERY:
            return
        self.release_required = False
        self.current_action = None
        self.state = State.POSTURE_ARMED if standing else State.DISARMED

    def sdk_released(self, standing: bool):
        self.release_required = False
        if self.state == State.FAULT:
            self.current_action = None
            return
        if self.state == State.RECOVERY and standing:
            self.recovered(True)

    def ai_link_lost(self):
        self.dynamic_requested = False
        if self.state == State.ACTION_PENDING:
            self.current_action = None
            self.state = State.POSTURE_ARMED
        elif self.state == State.ACTION_ACTIVE:
            self.state = State.RECOVERY
        elif self.state == State.POSTURE_ARMED:
            self.state = State.DISARMED

    def immediate_fault(self, reason: str):
        self.fault_reason = reason
        self.dynamic_requested = False
        self.release_required = self.release_required or self.state in (State.SDK_TAKEOVER, State.ACTION_ACTIVE, State.RECOVERY)
        self.state = State.FAULT

    def restore_after_crash(self, ownership_marker: bool):
        self.dynamic_requested = False
        if ownership_marker:
            self.state = State.RECOVERY
            self.release_required = True
        else:
            self.state = State.DISARMED
