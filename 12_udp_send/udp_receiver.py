#!/usr/bin/env python3
"""
TikuOS UDP Receiver over SLIP

Receives UDP packets sent by the TikuOS Example 12 (udp_send) over a
SLIP serial link and prints the payload.

Usage:
    python3 udp_receiver.py /dev/ttyACM0
    python3 udp_receiver.py /dev/ttyACM0 --baud 9600
    python3 udp_receiver.py /dev/ttyACM0 --listen-port 5000
    python3 udp_receiver.py COM3           # Windows

Requirements:
    pip install pyserial

Setup:
    1. Flash the TikuOS udp_send example onto the MSP430FR5969
    2. Connect the LaunchPad via USB
    3. Run this script pointing to the eZ-FET backchannel UART port

Network:
    TikuOS (172.16.7.2) --SLIP/UART--> Host (this script)
    Source port: 3000, Destination port: 5000
"""

import argparse
import signal
import sys
import time

from slip_decoder import SlipSerial, parse_ipv4, parse_udp

# ---------------------------------------------------------------------------
# Display
# ---------------------------------------------------------------------------

def format_hex(data, max_bytes=32):
    """Format bytes as a hex string with an ASCII sidebar."""
    hex_part = " ".join("{:02X}".format(b) for b in data[:max_bytes])
    ascii_part = "".join(
        chr(b) if 0x20 <= b < 0x7F else "." for b in data[:max_bytes])
    suffix = " ..." if len(data) > max_bytes else ""
    return "{} |{}|{}".format(hex_part, ascii_part, suffix)


def print_packet(pkt_num, ip, udp):
    """Pretty-print a received UDP packet."""
    ts = time.strftime("%H:%M:%S")
    print("─" * 60)
    print("  Packet #{:<5d}  {}".format(pkt_num, ts))
    print("  IPv4:  {} -> {}  (proto={})".format(
        ip["src_ip"], ip["dst_ip"], ip["protocol"]))
    print("  UDP:   port {} -> port {}  ({} bytes payload)".format(
        udp["src_port"], udp["dst_port"], len(udp["payload"])))

    payload = udp["payload"]
    try:
        text = payload.decode("utf-8")
        print("  Data:  \"{}\"".format(text))
    except (UnicodeDecodeError, ValueError):
        print("  Data:  {}".format(format_hex(payload)))

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Receive UDP packets from TikuOS over SLIP/serial")
    parser.add_argument("port",
        help="Serial port (e.g. /dev/ttyACM0, COM3)")
    parser.add_argument("--baud", type=int, default=9600,
        help="Baud rate (default: 9600)")
    parser.add_argument("--listen-port", type=int, default=None,
        help="Only show packets to this UDP port (default: show all)")
    args = parser.parse_args()

    print("=" * 60)
    print("  TikuOS UDP Receiver")
    print("  Serial port: {}  @ {} baud".format(args.port, args.baud))
    if args.listen_port:
        print("  Filter:      UDP port {}".format(args.listen_port))
    else:
        print("  Filter:      all UDP packets")
    print("=" * 60)
    print("  Waiting for packets... (Ctrl+C to quit)\n")

    slip = SlipSerial(args.port, args.baud)
    pkt_count = 0

    def sigint_handler(sig, frame):
        print("\n\nReceived {} packets total. Goodbye!".format(pkt_count))
        slip.close()
        sys.exit(0)
    signal.signal(signal.SIGINT, sigint_handler)

    try:
        for frame in slip.frames():
            ip = parse_ipv4(frame)
            if ip is None:
                continue

            if ip["protocol"] != 17:
                continue

            udp = parse_udp(ip["payload"])
            if udp is None:
                continue

            if args.listen_port and udp["dst_port"] != args.listen_port:
                continue

            pkt_count += 1
            print_packet(pkt_count, ip, udp)

    except Exception as e:
        print("\nError: {}".format(e))
        slip.close()
        sys.exit(1)


if __name__ == "__main__":
    main()
