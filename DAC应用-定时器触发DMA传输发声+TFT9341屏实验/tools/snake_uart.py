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
    print("WASD = move, J = OK/pause/retry, K = menu (game over). Ctrl+C to quit.")
    print("2P VS: board K1-K4 = P1 (green), UART WASD = P2 (blue).")
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
