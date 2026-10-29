# Host tools architecture

## Python package (`tools/punchboot/`)

Pure Python since 3.0.0 (D-021): one `py3-none-any` wheel. Dependencies: click, semver, pyusb and
libusb-package (libusb binaries for pyusb; without it pyusb uses the system's libusb).

- `wire.py`: the wire protocol, a mirror of `wire.h`/`error.h`. Enums for commands, results,
  auth methods and SLC, and a ctypes `LittleEndianStructure` (`_pack_ = 1`) per wire struct, named
  like the C struct. `test_compat.py` checks every value, size and field offset against
  `tests/compat/abi.txt`, and that every frozen `pb_*` struct and enum value is mirrored.
- `bpak.py`: the bpak header structs and `bpak_valid_header`/`bpak_part_size`/`bpak_part_offset`,
  for RAM boot and for detecting bpak files in `part_write`. Checked against `abi.txt` too.
- `transport.py`: `Transport` (`write`/`read`, one call is one transfer), `SocketTransport`
  (AF_UNIX, pb-host, 60 s timeout) and `UsbTransport` (pyusb, VID 0x1209 / PID 0x2019, device
  selected by its serial number, the UUID; EP 0x02 out, 0x81 in, interface 0, 10 s per transfer;
  a short transfer is a `TransferError`). pyusb is imported only when USB is used.
- `session.py`: `Session`, the command sequences, ported one to one from the 2.x C library
  (`api_*.c`), including its quirks: the stream is finalized after a failed write, the board
  command request and the prepare-buffer data are sent whatever the first result says (F-095).
  `Session(transport=...)` takes any transport (the transcript tests record the write sizes).
- `errors.py`: `Error` and one exception per `pb_results` value; unknown codes are `GenericError`.
- `helpers.py`: `pb_id` (crc32 table), `list_usb_devices`, `wait_for_device`, `valid_bpak_magic`.
  `partition.py`, `slc.py`: `Partition`, `PartitionFlags`, `SLC`.
- `__main__.py`: click CLI `punchboot` with groups `dev`, `auth`, `part`, `board`, `boot`, `slc`,
  plus `log` (the RAM log, `PB_CMD_LOG_READ`), `list` and `completion`. Transport is chosen with `-t usb|socket`, the device with `-u` / `PB_DEVICE_UUID`.

The 2.x host was a C library (`tools/punchboot/*.c` next to the package in
`tools/punchboot/punchboot/`, libusb) compiled into a `_punchboot`
extension; the transcripts in `tests/compat/transcripts` were recorded from it. Performance: a
32 MiB `part_write` to the 8MP EVK's eMMC runs at 27.7-28.6 MiB/s, the same as the 2.7.1 C wheel
(28.0-28.8 MiB/s) within run-to-run noise.

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
