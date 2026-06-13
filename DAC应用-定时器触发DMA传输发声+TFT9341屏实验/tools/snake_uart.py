#!/usr/bin/env python3
"""Forward PC keyboard WASDJ to the Snake game over UART (115200)."""

import sys

try:
    import serial
except ImportError:
    print("Install pyserial: pip install pyserial")
    sys.exit(1)

try:
    import msvcrt
except ImportError:
    print("This script needs Windows msvcrt (run on Windows).")
    sys.exit(1)


def main():
    port = sys.argv[1] if len(sys.argv) > 1 else "COM3"
    ser = serial.Serial(port, 115200, timeout=0.01)
    print("Connected %s @ 115200" % port)
    print("W/S = menu select, J = change (diff/music/vs), K = start game.")
    print("2P: WASD = P2. VS CPU: WASD = P1 (same as board keys).")
    print("Board also prints: [START] [SCORE] [LEVEL] [OVER] [VS_OVER] [DEMO_OVER]")
    try:
        while True:
            if msvcrt.kbhit():
                ch = msvcrt.getwch()
                if ch in "wWasSdDjJkK":
                    ser.write(ch.lower().encode("ascii"))
            if ser.in_waiting:
                sys.stdout.write(
                    ser.read(ser.in_waiting).decode("ascii", errors="replace")
                )
                sys.stdout.flush()
    except KeyboardInterrupt:
        pass
    finally:
        ser.close()


if __name__ == "__main__":
    main()
