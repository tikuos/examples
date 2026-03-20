#!/usr/bin/env python3
"""
SLIP (Serial Line Internet Protocol) decoder for TikuOS.

Implements RFC 1055 SLIP framing with the TikuOS NUL-escape extension
(eZ-FET backchannel firmware sends a target reset on two consecutive
0x00 bytes, so TikuOS escapes NUL as ESC 0xDE).

Provides:
    - SlipDecoder:  byte-at-a-time frame reassembly
    - SlipSerial:   reads SLIP frames from a pyserial port
    - parse_ipv4(): minimal IPv4 header parser
    - parse_udp():  UDP header parser

Usage as a library:
    from slip_decoder import SlipSerial, parse_ipv4, parse_udp

    slip = SlipSerial("/dev/ttyACM0", baud=9600)
    for frame in slip.frames():
        ip = parse_ipv4(frame)
        ...

Usage standalone (hex dump of SLIP frames):
    python3 slip_decoder.py /dev/ttyACM0
"""

import argparse
import struct
import sys
import time

try:
    import serial
except ImportError:
    serial = None  # Allow import as library without pyserial installed

# ---------------------------------------------------------------------------
# SLIP constants (RFC 1055)
# ---------------------------------------------------------------------------

SLIP_END     = 0xC0
SLIP_ESC     = 0xDB
SLIP_ESC_END = 0xDC
SLIP_ESC_ESC = 0xDD
SLIP_ESC_NUL = 0xDE  # TikuOS extension: escape NUL (eZ-FET workaround)

# ---------------------------------------------------------------------------
# SLIP decoder
# ---------------------------------------------------------------------------

class SlipDecoder:
    """Byte-at-a-time SLIP frame decoder.

    Feed individual bytes via feed(). When a complete frame has been
    reassembled, feed() returns it as a bytes object; otherwise it
    returns None.

    Example:
        decoder = SlipDecoder()
        for b in raw_bytes:
            frame = decoder.feed(b)
            if frame is not None:
                process(frame)
    """

    def __init__(self):
        self.buf = bytearray()
        self.in_escape = False

    def reset(self):
        """Discard any partially received frame."""
        self.buf = bytearray()
        self.in_escape = False

    def feed(self, byte_val):
        """Feed a single byte.

        Returns a complete frame (bytes) when a SLIP END delimiter
        is received, or None if more data is needed.
        """
        if byte_val == SLIP_END:
            if len(self.buf) > 0:
                frame = bytes(self.buf)
                self.buf = bytearray()
                self.in_escape = False
                return frame
            return None

        if byte_val == SLIP_ESC:
            self.in_escape = True
            return None

        if self.in_escape:
            self.in_escape = False
            if byte_val == SLIP_ESC_END:
                self.buf.append(SLIP_END)
            elif byte_val == SLIP_ESC_ESC:
                self.buf.append(SLIP_ESC)
            elif byte_val == SLIP_ESC_NUL:
                self.buf.append(0x00)
            else:
                self.buf.append(byte_val)
            return None

        self.buf.append(byte_val)
        return None


# ---------------------------------------------------------------------------
# SLIP serial reader
# ---------------------------------------------------------------------------

class SlipSerial:
    """Read SLIP frames from a serial port.

    Wraps a pyserial Serial object and yields complete decoded frames.

    Example:
        slip = SlipSerial("/dev/ttyACM0")
        for frame in slip.frames():
            print(len(frame), "bytes")
    """

    def __init__(self, port, baud=9600, timeout=0.1):
        if serial is None:
            raise ImportError(
                "pyserial is required: pip install pyserial")
        self.ser = serial.Serial(port, baud, timeout=timeout)
        self.decoder = SlipDecoder()

    def close(self):
        """Close the serial port."""
        self.ser.close()

    def frames(self):
        """Generator that yields complete SLIP frames (bytes).

        Blocks between frames. Yields forever until the serial port
        is closed or an error occurs.
        """
        while True:
            chunk = self.ser.read(256)
            if not chunk:
                continue
            for byte_val in chunk:
                frame = self.decoder.feed(byte_val)
                if frame is not None:
                    yield frame


# ---------------------------------------------------------------------------
# IPv4 parser
# ---------------------------------------------------------------------------

def parse_ipv4(frame):
    """Parse a minimal IPv4 header.

    Returns a dict with header fields and the transport-layer payload,
    or None if the frame is not a valid IPv4 packet.

    Fields returned:
        version, ihl, total_len, protocol, src_ip, dst_ip, payload
    """
    if len(frame) < 20:
        return None

    ver_ihl = frame[0]
    version = (ver_ihl >> 4) & 0x0F
    ihl = (ver_ihl & 0x0F) * 4

    if version != 4 or ihl < 20 or ihl > len(frame):
        return None

    total_len = struct.unpack("!H", frame[2:4])[0]
    if total_len > len(frame):
        return None

    protocol = frame[9]
    src_ip = "{}.{}.{}.{}".format(frame[12], frame[13], frame[14], frame[15])
    dst_ip = "{}.{}.{}.{}".format(frame[16], frame[17], frame[18], frame[19])

    return {
        "version": version,
        "ihl": ihl,
        "total_len": total_len,
        "protocol": protocol,
        "src_ip": src_ip,
        "dst_ip": dst_ip,
        "payload": frame[ihl:total_len],
    }


# ---------------------------------------------------------------------------
# UDP parser
# ---------------------------------------------------------------------------

def parse_udp(data):
    """Parse a UDP header from the IPv4 payload.

    Returns a dict with src_port, dst_port, length, checksum, and
    payload bytes, or None if the data is too short.
    """
    if len(data) < 8:
        return None

    src_port, dst_port, udp_len, checksum = struct.unpack("!HHHH", data[:8])

    if udp_len < 8 or udp_len > len(data):
        return None

    return {
        "src_port": src_port,
        "dst_port": dst_port,
        "length": udp_len,
        "checksum": checksum,
        "payload": data[8:udp_len],
    }


# ---------------------------------------------------------------------------
# Standalone mode: hex dump of SLIP frames
# ---------------------------------------------------------------------------

def main():
    if serial is None:
        print("Error: pyserial is required. Install with: pip install pyserial")
        sys.exit(1)

    parser = argparse.ArgumentParser(
        description="Decode and hex-dump SLIP frames from a serial port")
    parser.add_argument("port",
        help="Serial port (e.g. /dev/ttyACM0, COM3)")
    parser.add_argument("--baud", type=int, default=9600,
        help="Baud rate (default: 9600)")
    args = parser.parse_args()

    print("SLIP decoder: {} @ {} baud".format(args.port, args.baud))
    print("Waiting for frames... (Ctrl+C to quit)\n")

    slip = SlipSerial(args.port, args.baud)
    count = 0

    try:
        for frame in slip.frames():
            count += 1
            ts = time.strftime("%H:%M:%S")
            hex_str = " ".join("{:02X}".format(b) for b in frame[:64])
            suffix = " ..." if len(frame) > 64 else ""
            print("[{}] Frame #{}: {} bytes".format(ts, count, len(frame)))
            print("  {}{}".format(hex_str, suffix))

            ip = parse_ipv4(frame)
            if ip:
                print("  IPv4: {} -> {} proto={}".format(
                    ip["src_ip"], ip["dst_ip"], ip["protocol"]))
            print()
    except KeyboardInterrupt:
        print("\n{} frames received.".format(count))
    finally:
        slip.close()


if __name__ == "__main__":
    main()
