#!/usr/bin/env python3

import sys
import uuid

if __name__ == "__main__":
    if (len(sys.argv) < 2):
        print(f"Usage: {sys.argv[0]} <UUID>")
        sys.exit(1)
    uu = uuid.UUID(sys.argv[1])
    byte_string = "".join(f"\\x{b:02x}" for b in uu.bytes)
    print("#define UUID_" + str(uu).replace("-", "_") + f" \"{byte_string}\"")
