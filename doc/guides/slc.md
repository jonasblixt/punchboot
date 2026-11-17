# The life cycle: configure, lock, revoke

**State: draft** (F-232), not reviewed. How a product takes a device from open to locked with
pb, and revokes a key later. The rules are `system.md` 4, `command-mode.md` 5.2,
`platform.md` 4.1 and 4.2, `board.md` 7 and 8; the SoC's facts are `plat-imx.md` 4.1. This
guide cites them and adds nothing they don't say.

**Every step that writes a fuse can't be undone.** Run none on a board that must stay open:
a development board that isn't yours to close gets a build that writes no fuses
(`CONFIG_SLC_NO_FUSE_WRITES`, `system.md` 4, D-087), as the 8MP EVK's defconfig has.

## 1. What is fused

- **The key hash**: the hash of the keys the ROM verifies pb with (the SRKs on the i.MX), the
  first entries of the board's `slc_fuses`, one per word by `<P>_FUSE_SRK(n)` (D-094). The
  words are srktool's fuse file's (`*_fuse.bin`, little endian, in fuse order), copied into the
  board's file. Nothing checks them against the SRK table pb is signed with: a wrong word
  closes the device to every image once it is locked. Section 3's reset is the check.
- **The board's other configuration fuses**, `slc_fuses` (`board.md` 6): the boot device and
  what else the product fuses, by the platform's fuse index macro. jiffy: boot from the eMMC on
  uSDHC1, and take the boot configuration from the fuses. Check each against the SoC's
  reference manual before the first device.
- **The lock**: the SoC's closed state (on the i.MX the SEC_CONFIG fuse; AHAB's OEM closed
  life cycle on the 8X). From then on the ROM runs only a signed pb, and command mode asks
  for authentication.
- **A key's revocation**: its revoke bit, one per key slot. Key n of the board's
  `VERIFY_KEY<n>` is in slot n (`board.md` 7, D-083).

pb's own keys, the ones it verifies images and tokens with, are in pb's image
(`VERIFY_KEY<n>`), not fused. The ROM's key hash is what makes that image trustworthy.

## 2. Before the first device

1. The keys and their slots (`board.md` 7): a key in a slot the SoC has no revoke bit for
   can't be revoked; the build warns, and on the i.MX8X it stops (`platform.md` 4.1, D-092).
2. The key hash's words in `slc_fuses` against srktool's fuse file of the SRK table the build
   signs pb with.
3. `slc_fuses`, against the reference manual.
4. A token for authentication after the lock: `scripts/createtoken.sh <uuid> file <hash>
   <private key>` signs the device's UUID (`punchboot dev show`) with a key of the board's
   (`command-mode.md` 3.3). A token works for that device only.
5. What is fixed once a product ships: `board.md` 8.

## 3. Configure

On an open device (`NOT_CONFIGURED`), with the product's signed pb booted:

1. `punchboot slc show`: the state and the keys (active, revoked, and which active ones the
   device can't revoke).
2. `punchboot slc configure`: writes the board's configuration fuses, the key hash among
   them, the bits that aren't set yet (`command-mode.md` 5.2). Every word is read first: one
   with a bit its value lacks can never become it, and nothing is written. Each fuse word is read back after its
   write, and a bit that didn't take fails the command (`platform.md` 4.2, F-166; the 8X
   doesn't read back, `plat-imx.md` 4.1). A configuration that failed half way is completed
   by running it again.
3. `punchboot slc show`: `CONFIGURATION`.
4. Reset, and check that the ROM accepted pb with the fused hash: on HABv4 `punchboot log hab`
   has no failure event, on the 8X the SECO's log (`punchboot log seco`, `plat-imx.md` 2). The device is still open: an image that isn't
   signed for it still boots from the ROM, so this is the last chance to find a wrong hash.
5. Boot the product's system once (`punchboot boot partition <System A>`).

## 4. Lock

1. `punchboot slc lock`: from `CONFIGURATION` only (`command-mode.md` 5.2). On the 8M it, and
   a revocation, is refused when HAB reported a failure for this boot: closed, that pb
   wouldn't boot (`imx_hab_no_failure()`). On the other SoCs step 4 of section 3 is the check.
2. Reset. Command mode now needs authentication: `punchboot -u <uuid> auth token
   <uuid>.token <key id>` after every reset, before anything but the device's identifier
   (`command-mode.md` 3.2).
3. `punchboot slc show`: `CONFIGURATION_LOCKED`.

## 5. Revoke a key

`punchboot slc revoke-key <key id>`, in an authenticated session. Which key, and never the
last one, is the product's (`system.md` 6): pb doesn't stop a revocation of the key its own
images are signed with.

- A key the SoC has no revoke bit for answers `NOT_SUPPORTED` (D-085).
- The session the token opened stays open; the next one needs a token of another key
  (`security.md` 4.3).
- On the i.MX8X the SECO revokes what the running image's header names: pb's image is signed
  with the key's bit in its revoke mask, and the command commits it; a header that doesn't
  name the key fails it (`imx8x_revoke_key()`).

## 6. End of life

Optional, the product's choice: `CONFIG_SLC_EOL`, off by default (`system.md` 4). Without it
`punchboot slc eol` answers `NOT_SUPPORTED`. Only the host platform implements it today
(F-263).

## 7. By SoC

| SoC         | Writes                       | Read back           | Revoke bits    | Notes              |
| ----------- | ---------------------------- | ------------------- | -------------- | ------------------ |
| i.MX6UL     | OCOTP                        | The array (F-166)   | 3 (SRK_REVOKE) | jiffy is locked    |
| i.MX8M      | OCOTP                        | The array; on the   | 3 on the 8MQ;  | F-447: the 8M's    |
|             |                              | 8MQ B1 the reloaded | the 8M Plus's  | SLC sources        |
|             |                              | shadows (F-466)     | aren't public  |                    |
| i.MX8X      | The SCFW (SECO)              | None, after a reset | 4 (OEM SRKs)   | Slots 4 and up are |
|             |                              |                     |                | refused (D-092)    |
| i.MX RT1060 | None: no fuse timing (F-148) |                     | 3              | Reads only         |

Never run on a board, as of 2026-11-17: a fuse write with the read back, and the 8X's closed
state (`roadmap.md`, the bench run).
