#!/usr/bin/env python3

import sys

if __name__ == "__main__":
    if (len(sys.argv) < 2):
        print(f"Usage: {sys.argv[0]} <hex string>")
        sys.exit(1)

    byte_string = "".join(f"\\x{b:02x}" for b in bytearray.fromhex(sys.argv[1]))
    print(f"\"{byte_string}\"")
