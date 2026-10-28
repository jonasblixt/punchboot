# Host tools architecture

## C library (`tools/punchboot/`)

- `api.h` / `api.c`: `struct pb_context` is a transport vtable (`init/free/write/read/list/connect/disconnect`, debug log `d`).
- Transports:
  - `usb.c`: libusb, VID 0x1209 / PID 0x2019, device selected by UUID serial.
  - `socket.c`: AF_UNIX, used by the host tests (`pb-host`), Linux only.
- API modules: `api_{device,authentication,slc,partition,stream,boot,board,misc}.c`, `error.c`.
  Each call repeats the same boilerplate (init cmd, write, read result, check magic), about 40 times.
- There is no standalone build: the lib is only compiled into the Python extension by `setup.py`,
  together with firmware sources `src/wire.c` and `src/lib/bpak.c`.

## Python (`tools/punchboot/punchboot/`)

- `_punchboot` C extension (`python_wrapper.c`, `exceptions.c`): `Session` type, `wait_for_device`,
  `list_usb_devices`. Exceptions are mapped from `pb_results`.
- `session.py`: typed wrapper. `partition.py`, `slc.py`: enums/dataclasses mirroring C constants.
  `helpers.py`: `pb_id` (crc32 table), `valid_bpak_magic`.
- `__main__.py`: click CLI `punchboot` with groups `dev`, `auth`, `part`, `board`, `boot`, `slc`,
  plus `log` (the RAM log, `PB_CMD_LOG_READ`), `list` and `completion`. Transport is chosen with `-t usb|socket`, the device with `-u` / `PB_DEVICE_UUID`.

## Wire protocol (`include/pb-tools/wire.h`)

- Fixed 512-byte packed `pb_command` (`magic 'PBL0'`, `command`, `request[504]`) and `pb_result`
  (`magic`, `int8 result_code`, `response[504]`). Larger payloads follow as separate transfers.
- Commands are enum `pb_commands`. The numbering depends on declaration order, so **only append**.
- **Backward compatibility is a hard rule (D-008).** What is frozen and how to extend it: `compatibility.md`.
- The magic is the only version marker. `PB_CMD_DEVICE_READ_CAPS` is the only capability query.
- Commands that don't need auth: `DEVICE_IDENTIFIER_READ`, `AUTHENTICATE`.
- Raw `struct bpak_header` crosses the wire (`read_bpak`, `bpak_boot`, `verify.bpak`).

## pbstate (`tools/pbstate/`)

CMake C lib + CLI that reads and writes the A/B state blob from Linux. It uses the firmware headers
`include/boot/pb_state_blob.h` and `src/lib/crc.c`. Its version (`pbstate -V`, `pbstate.pc`) comes
from `version.txt`; the SONAME is the ABI version and stays `libpbstate.so.0`.
