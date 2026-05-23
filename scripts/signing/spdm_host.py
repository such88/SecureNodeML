#!/usr/bin/env python3
"""
scripts/signing/spdm_host.py
Day 15 — Python SPDM requester to test STM32 SPDM responder over UART

Sends SPDM GET_VERSION and GET_MEASUREMENTS, prints SHA-384 model hash.
Use this to test firmware/stm32/src/spdm_responder.c without needing ESP32.

Usage:
    python scripts/signing/spdm_host.py /dev/ttyACM0

Requirements:
    pip install pyserial
"""
import sys, time, os, hashlib
try:
    import serial
except ImportError:
    print("ERROR: pyserial not installed. Run: pip install pyserial")
    sys.exit(1)

def send_recv(port, data, timeout=2.0, label=""):
    port.reset_input_buffer()
    port.write(data)
    port.flush()
    time.sleep(0.3)
    resp = port.read(256)
    print(f"\n--- {label} ---")
    print(f"  Sent ({len(data)}B): {data.hex()}")
    print(f"  Recv ({len(resp)}B): {resp.hex()}")
    return resp

def main():
    dev = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyACM0"

    try:
        port = serial.Serial(dev, 115200, timeout=2)
    except serial.SerialException as e:
        print(f"ERROR: cannot open {dev}: {e}")
        print("Check usbipd attach if running in WSL2")
        sys.exit(1)

    print(f"Connected to {dev} at 115200 baud")
    time.sleep(0.5)   # wait for Zephyr boot

    # GET_VERSION request (DSP0274 §10.4)
    # [SPDMVersion=0x10][Code=0x84][P1=0x00][P2=0x00][Reserved:4B]
    get_version = bytes([0x10, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00])
    resp = send_recv(port, get_version, label="GET_VERSION")

    if len(resp) >= 4 and resp[1] == 0x04:
        print("  → VERSION response received OK")
    else:
        print("  → Unexpected response (is SPDM responder running?)")

    # GET_MEASUREMENTS request (DSP0274 §10.11)
    # [Version=0x12][Code=0xE0][Attr=0x01 all measurements][SlotID=0xFF]
    # [Nonce: 32 random bytes]
    nonce = os.urandom(32)
    get_meas = bytes([0x12, 0xE0, 0x01, 0xFF]) + nonce
    resp = send_recv(port, get_meas, label="GET_MEASUREMENTS")

    if len(resp) >= 52:
        digest = resp[4:52]
        print(f"\n  → Model SHA-384: {digest.hex()}")
        print(f"\nVerify against local file:")
        print(f"  sha384sum models/converted/anomaly_int8.tflite")
        print(f"  (hashes should match if OTA was successful)")
    else:
        print(f"  → Short response ({len(resp)}B) — check spdm_responder.c")

    port.close()

if __name__ == "__main__":
    main()
