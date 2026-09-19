#!/usr/bin/env python3
"""Passive p2p0 Retroid observer for the ROS-less Lite3 motion host."""

import argparse
import os
import secrets
import socket
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.realpath(__file__)), "lib"))

from emotion_bot_lite3_hw.protocol import (  # noqa: E402
    AXIS_CODES, PacketError, filter_datagram, parse_command, parse_ethernet_udp,
    parse_simple_stop, retroid_axes_zero,
)
from emotion_bot_lite3_hw.stop_transport import (  # noqa: E402
    StopRelayFrame, encode_frame, load_key,
)


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--interface", default="p2p0")
    parser.add_argument("--retroid-ip", default="192.168.2.196")
    parser.add_argument("--motion-wifi-ip", default="192.168.2.1")
    parser.add_argument("--command-port", type=int, default=43893)
    parser.add_argument("--source-ip", default="192.168.1.120")
    parser.add_argument("--source-port", type=int, default=43911)
    parser.add_argument("--destination-ip", default="192.168.1.103")
    parser.add_argument("--destination-port", type=int, default=43910)
    parser.add_argument("--key-file", default="/etc/emotion-bot/stop-relay.key")
    parser.add_argument("--rate", type=float, default=20.0)
    parser.add_argument("--retroid-timeout", type=float, default=0.75)
    parser.add_argument("--stop-hold", type=float, default=2.0)
    return parser.parse_args()


def main():
    args = parse_args()
    key = load_key(args.key_file)
    session_id = secrets.token_hex(16)
    sequence = 0
    last_packet = None
    last_nonzero_axis = None
    stop_until = 0.0
    axis_state = {code: None for code in AXIS_CODES}

    raw = socket.socket(socket.AF_PACKET, socket.SOCK_RAW, socket.htons(0x0003))
    raw.bind((args.interface, 0))
    raw.setblocking(False)
    relay = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    relay.bind((args.source_ip, args.source_port))
    relay_destination = (args.destination_ip, args.destination_port)
    period = 1.0 / args.rate
    next_publish = time.monotonic()

    try:
        while True:
            while True:
                try:
                    datagram = parse_ethernet_udp(raw.recv(65535))
                    payload = filter_datagram(
                        datagram, args.retroid_ip, args.command_port, args.motion_wifi_ip,
                    )
                    command = parse_command(payload)
                except BlockingIOError:
                    break
                except PacketError:
                    continue
                now = time.monotonic()
                last_packet = now
                if parse_simple_stop(command):
                    stop_until = max(stop_until, now + args.stop_hold)
                axis_zero = retroid_axes_zero(command)
                if axis_zero is not None:
                    axis_state[command.code] = axis_zero
                    if not axis_zero:
                        last_nonzero_axis = now

            now = time.monotonic()
            if now < next_publish:
                time.sleep(min(next_publish - now, 0.01))
                continue
            observed_fresh = last_packet is not None and now - last_packet <= args.retroid_timeout
            quiet = last_nonzero_axis is None or now - last_nonzero_axis > args.retroid_timeout
            axes_zero = observed_fresh and quiet and all(
                value is not False for value in axis_state.values()
            )
            sequence += 1
            try:
                relay.sendto(encode_frame(key, StopRelayFrame(
                    session_id=session_id,
                    sequence=sequence,
                    stop=now <= stop_until,
                    axes_zero=axes_zero,
                    observed_fresh=observed_fresh,
                )), relay_destination)
            except OSError:
                # The perception receiver may be restarting. Remaining alive is
                # safer: its local supervisor treats the absent relay as stale.
                pass
            next_publish = now + period
    finally:
        raw.close()
        relay.close()


if __name__ == "__main__":
    main()
