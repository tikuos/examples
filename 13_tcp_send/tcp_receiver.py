#!/usr/bin/env python3
"""
TikuOS TCP Receiver over SLIP

Accepts a TCP connection from TikuOS Example 13 (tcp_send) over a
SLIP serial link and displays the received data.

The script implements a minimal TCP stack in Python: it responds to
SYN with SYN+ACK, ACKs data segments, and handles FIN close.  All
communication happens over raw SLIP frames on the serial port.

Usage:
    python3 tcp_receiver.py /dev/ttyUSB0
    python3 tcp_receiver.py /dev/ttyUSB0 --baud 9600
    python3 tcp_receiver.py COM3

Requirements:
    pip install pyserial

Setup:
    1. Connect an FTDI/CP2102 USB-UART adapter to MSP430 pins:
       - Adapter TX -> MSP430 P2.1 (RXD)
       - Adapter RX -> MSP430 P2.0 (TXD)
       - Adapter GND -> MSP430 GND
    2. Flash the TikuOS tcp_send example
    3. Run this script pointing to the FTDI/CP2102 serial port

Note: The eZ-FET backchannel UART cannot handle TCP's multi-exchange
      protocol.  An external FTDI/CP2102 adapter is required.

Network:
    TikuOS (172.16.7.2:3000) --SLIP/UART--> Host (this script, port 5000)
"""

import argparse
import signal
import struct
import sys
import time

from slip_decoder import SlipSerial, SlipDecoder, parse_ipv4

# ---------------------------------------------------------------------------
# SLIP constants and encoder
# ---------------------------------------------------------------------------

SLIP_END     = 0xC0
SLIP_ESC     = 0xDB
SLIP_ESC_END = 0xDC
SLIP_ESC_ESC = 0xDD
SLIP_ESC_NUL = 0xDE


def slip_encode(payload):
    """SLIP-encode a packet with eZ-FET NUL escaping."""
    out = bytearray([SLIP_END])
    for b in payload:
        if b == SLIP_END:
            out += bytes([SLIP_ESC, SLIP_ESC_END])
        elif b == SLIP_ESC:
            out += bytes([SLIP_ESC, SLIP_ESC_ESC])
        elif b == 0x00:
            out += bytes([SLIP_ESC, SLIP_ESC_NUL])
        else:
            out.append(b)
    out.append(SLIP_END)
    return bytes(out)

# ---------------------------------------------------------------------------
# Checksum and packet builders
# ---------------------------------------------------------------------------

SRC_IP = bytes([172, 16, 7, 1])   # Us (host)
DST_IP = bytes([172, 16, 7, 2])   # Device


def inet_cksum(data):
    if len(data) % 2:
        data = data + b'\x00'
    s = sum((data[i] << 8) | data[i + 1] for i in range(0, len(data), 2))
    while s >> 16:
        s = (s & 0xFFFF) + (s >> 16)
    return (~s) & 0xFFFF


def tcp_checksum(src_ip, dst_ip, tcp_seg):
    pseudo = src_ip + dst_ip + struct.pack("!BBH", 0, 6, len(tcp_seg))
    return inet_cksum(pseudo + tcp_seg)


def build_tcp_reply(sport, dport, seq, ack, flags, window=4096):
    """Build a complete IPv4/TCP packet (no payload)."""
    tcp = struct.pack("!HHIIBBHHH",
                      sport, dport, seq, ack,
                      0x50, flags, window, 0, 0)
    total = 20 + len(tcp)

    ip = struct.pack("!BBHHHBBH4s4s",
                     0x45, 0, total, 1, 0, 64, 6, 0, SRC_IP, DST_IP)
    ip_ck = inet_cksum(ip)
    ip = struct.pack("!BBHHHBBH4s4s",
                     0x45, 0, total, 1, 0, 64, 6, ip_ck, SRC_IP, DST_IP)

    tcp_ck = tcp_checksum(SRC_IP, DST_IP, tcp)
    tcp = bytearray(tcp)
    struct.pack_into("!H", tcp, 16, tcp_ck)

    return ip + bytes(tcp)


def parse_tcp(data):
    """Parse IPv4/TCP packet.  Returns dict or None."""
    if len(data) < 40 or (data[0] >> 4) != 4 or data[9] != 6:
        return None
    ihl = (data[0] & 0x0F) * 4
    t = data[ihl:]
    if len(t) < 20:
        return None
    doff = (t[12] >> 4) * 4
    return {
        'sport':   (t[0] << 8) | t[1],
        'dport':   (t[2] << 8) | t[3],
        'seq':     struct.unpack("!I", t[4:8])[0],
        'ack':     struct.unpack("!I", t[8:12])[0],
        'flags':   t[13],
        'window':  (t[14] << 8) | t[15],
        'payload': bytes(t[doff:]) if doff <= len(t) else b'',
    }

# ---------------------------------------------------------------------------
# TCP flags
# ---------------------------------------------------------------------------

FIN = 0x01
SYN = 0x02
RST = 0x04
PSH = 0x08
ACK = 0x10


def flags_str(f):
    parts = []
    if f & SYN: parts.append("SYN")
    if f & ACK: parts.append("ACK")
    if f & FIN: parts.append("FIN")
    if f & RST: parts.append("RST")
    if f & PSH: parts.append("PSH")
    return "+".join(parts) or "-"

# ---------------------------------------------------------------------------
# Main TCP receiver loop
# ---------------------------------------------------------------------------


def main():
    parser = argparse.ArgumentParser(
        description="Receive TCP data from TikuOS over SLIP/serial")
    parser.add_argument("port",
        help="Serial port (e.g. /dev/ttyUSB0, COM3)")
    parser.add_argument("--baud", type=int, default=9600)
    parser.add_argument("--listen-port", type=int, default=5000)
    args = parser.parse_args()

    print("=" * 60)
    print("  TikuOS TCP Receiver")
    print("  Serial: {} @ {} baud".format(args.port, args.baud))
    print("  Listen: port {}".format(args.listen_port))
    print("=" * 60)
    print("  Waiting for SYN from device... (Ctrl+C to quit)\n")

    slip = SlipSerial(args.port, args.baud)

    # Connection state
    our_seq = 0x8000
    their_seq = 0
    connected = False
    pkt_count = 0

    def send_reply(sport, dport, seq, ack, flags):
        pkt = build_tcp_reply(sport, dport, seq, ack, flags)
        slip.ser.write(slip_encode(pkt))
        slip.ser.flush()

    def sigint_handler(sig, frame):
        print("\n\n  {} packets received. Goodbye!".format(pkt_count))
        slip.close()
        sys.exit(0)
    signal.signal(signal.SIGINT, sigint_handler)

    try:
        for frame in slip.frames():
            tcp = parse_tcp(frame)
            if tcp is None:
                continue

            # Only handle packets to our listen port
            if tcp['dport'] != args.listen_port:
                continue

            ts = time.strftime("%H:%M:%S")

            # --- SYN (new connection) ---
            if tcp['flags'] & SYN and not (tcp['flags'] & ACK):
                their_seq = tcp['seq'] + 1
                print("[{}]  SYN from port {} (seq={:#x})".format(
                    ts, tcp['sport'], tcp['seq']))

                # Send SYN+ACK
                send_reply(args.listen_port, tcp['sport'],
                           our_seq, their_seq, SYN | ACK)
                our_seq += 1
                connected = True
                print("       -> SYN+ACK sent (our_seq={:#x})".format(
                    our_seq))
                continue

            # --- ACK (handshake completion or data ACK) ---
            if tcp['flags'] & ACK and not (tcp['flags'] & (SYN | FIN)):
                data = tcp['payload']
                if len(data) > 0:
                    pkt_count += 1
                    their_seq = tcp['seq'] + len(data)

                    try:
                        text = data.decode('utf-8')
                        print("[{}]  DATA #{}: \"{}\" ({} bytes)".format(
                            ts, pkt_count, text, len(data)))
                    except UnicodeDecodeError:
                        hex_str = " ".join("{:02X}".format(b)
                                           for b in data[:32])
                        print("[{}]  DATA #{}: {} ({} bytes)".format(
                            ts, pkt_count, hex_str, len(data)))

                    # ACK the data
                    send_reply(args.listen_port, tcp['sport'],
                               our_seq, their_seq, ACK)
                    print("       -> ACK sent (ack={:#x})".format(
                        their_seq))
                else:
                    # Pure ACK (handshake completion)
                    if connected:
                        print("[{}]  ACK (handshake complete)".format(ts))
                continue

            # --- FIN (close) ---
            if tcp['flags'] & FIN:
                their_seq = tcp['seq'] + 1
                print("[{}]  FIN received".format(ts))

                # ACK the FIN
                send_reply(args.listen_port, tcp['sport'],
                           our_seq, their_seq, ACK)

                # Send our FIN
                send_reply(args.listen_port, tcp['sport'],
                           our_seq, their_seq, FIN | ACK)
                our_seq += 1
                connected = False
                print("       -> FIN+ACK sent (connection closed)")
                print("\n  Session complete: {} messages received.\n".format(
                    pkt_count))
                pkt_count = 0
                print("  Waiting for next connection...\n")
                continue

    except Exception as e:
        print("\nError: {}".format(e))
        slip.close()
        sys.exit(1)


if __name__ == "__main__":
    main()
