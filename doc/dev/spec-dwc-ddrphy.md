# Specification: DWC DDR PHY driver (`src/drivers/ddr/dwc_ddrphy.c`)

Behavioural specification for a clean-room implementation of the PHY driver. It says what the
driver must do at the register level and what it returns; it says nothing about how the code is
organised. Written 2026-10-28 by the spec side (who has read other implementations); the
implementation side works from this document and the headers named below only.

## Inputs the implementer may use

- This document.
- `include/drivers/ddr/dwc_ddrphy.h`: the API, `struct dwc_ddrphy`, `struct dwc_ddrphy_fw` and the
  CSR numbers and bit names used below.
- `include/drivers/ddr/dram_cfg.h`: `struct dram_cfg_param`, `struct dram_fsp_msg`, `enum fw_type`.
- punchboot's general headers: `include/pb/mmio.h`, `include/pb/pb.h`, `include/pb/errors.h`,
  `include/pb/log_def.h`, and `CLAUDE.md` for conventions.
- Building and running the tests (`tests/unit/test_imx8m_ddr`, which compares every register write
  against a recorded trace) and the firmware build (`scripts/pbdev firmware imx8mevk`).

## Conventions

- C99, freestanding (no libc beyond what punchboot's headers provide), clang-format via
  `pre-commit`. Functions that can fail return `PB_OK` or a negative `-PB_ERR_*`.
- All register access goes through `mmio_read_16/32` and `mmio_write_16/32` from `<pb/mmio.h>`.
- **CSR addressing.** The PHY's registers (CSRs) are numbered. CSR number `n` is at address
  `phy->base + phy->csr_stride * n`. Unless stated otherwise a CSR is accessed with a 32 bit access
  at that address, and its value is in the low 16 bits.
- Logging (`LOG_ERR/INFO/DBG`) is free form; it is not part of the checked behaviour. Errors
  should be logged with `LOG_ERR`.

## The hardware, as far as the driver is concerned

- The PHY contains a microcontroller (PMU) that runs a training firmware. The firmware has two
  parts: code for the PMU's instruction memory (IMEM) and data for its data memory (DMEM). An image
  is given as `struct dwc_ddrphy_fw`: arrays of 32 bit words, `[imem, imem_end)` and
  `[dmem, dmem_end)`. `phy->fw[FW_1D_IMAGE]` and `phy->fw[FW_2D_IMAGE]` are the two images.
- IMEM starts at CSR `DDRPHY_IMEM`, DMEM at CSR `DDRPHY_DMEM`. Each memory location is one 16 bit
  CSR. Firmware word `i` of a part goes into two consecutive CSRs: its low 16 bits into CSR
  `start + 2*i`, its high 16 bits into CSR `start + 2*i + 1`. These memory CSRs are written and read
  with **16 bit** accesses.
- `DDRPHY_MICROCONTMUXSEL` selects who may access the PHY's CSRs and the PMU memories:
  `MICROCONTMUXSEL_APB` (0) the driver, `MICROCONTMUXSEL_PMU` (1) the PMU.
- `DDRPHY_MICRORESET` controls the PMU: `MICRORESET_HOLD` (reset and stall, 0x9),
  `MICRORESET_STALL` (0x1), `MICRORESET_RUN` (0x0).
- **Mailbox.** While running, the PMU posts messages one at a time:
  - A message is pending when bit `UCTSHADOWREGS_UCTWRITEPROTSHADOW` of `DDRPHY_UCTSHADOWREGS`
    reads **0**.
  - The message is read from `DDRPHY_UCTWRITEONLYSHADOW` (low 16 bits). For a 32 bit message the
    high 16 bits are then read from `DDRPHY_UCTDATWRITEONLYSHADOW`: read the low half first.
  - After reading, the driver acknowledges: write `DCTWRITEPROT_ACK` (0) to `DDRPHY_DCTWRITEPROT`,
    wait until the `UCTWRITEPROTSHADOW` bit reads **1**, then write `DCTWRITEPROT_RELEASE` (1) to
    `DDRPHY_DCTWRITEPROT`. Only then may the next message be waited for.
- **Messages during training.** The PMU sends a sequence of 16 bit *major* messages:
  - `DDRPHY_MAIL_TRAINING_DONE` (0x07): training finished successfully. Last message.
  - `DDRPHY_MAIL_TRAINING_FAILED` (0xff): training failed. Last message.
  - `DDRPHY_MAIL_STREAMING_MSG` (0x08): a *streaming message* follows, as 32 bit messages: first a
    header word whose low 16 bits are the number N of argument words, then N argument words. Each
    of these words is a separate mailbox message (wait, read low then high, acknowledge). The
    driver may log them; it must consume all N + 1 words.
  - Any other value is progress information: consume it and wait for the next message.
- `DDRPHY_CALBUSY`: bit `CALBUSY_BUSY` is 1 while the PHY's impedance calibration runs.
- `DDRPHY_UCCLKHCLKENABLES`: clocks for CSR access. `UCCLKHCLKENABLES_UCCLKEN |
  UCCLKHCLKENABLES_HCLKEN` (0x3) enables what is needed to read the PHY's CSRs from the APB;
  `UCCLKHCLKENABLES_HCLKEN` (0x2) is the normal state.

## Functions

The required register writes are listed in order. Reads may be done as needed, but the writes, their
order, their widths and their values must be exactly these: the unit test checks them.

### `void dwc_ddrphy_write(phy, csr, val)` / `uint32_t dwc_ddrphy_read(phy, csr)`

One 32 bit write of `val` / one 32 bit read at CSR `csr`'s address.

### `void dwc_ddrphy_write_cfg(phy, cfg, n)`

For `i = 0 .. n-1` in order: write `cfg[i].val` to CSR `cfg[i].reg` (32 bit).

### `int dwc_ddrphy_train(phy, fsp)`

One training run with image `phy->fw[fsp->fw_type]` and message block `fsp->fsp_cfg[0 ..
fsp->fsp_cfg_num-1]`. The caller has already set the DRAM clock for `fsp->drate`.

1. Write `MICROCONTMUXSEL_APB` to `DDRPHY_MICROCONTMUXSEL`.
2. Load IMEM: write all IMEM words (16 bit writes, low half then high half of each word, words in
   order), then read all of them back (16 bit reads) and compare with the image. On any difference:
   return `-PB_ERR_IO` at once, with no further writes.
3. Load DMEM the same way. Same failure behaviour.
4. Write the message block: each `fsp->fsp_cfg[i]` in order, as `dwc_ddrphy_write_cfg`.
5. Write `MICROCONTMUXSEL_PMU` to `DDRPHY_MICROCONTMUXSEL`, then to `DDRPHY_MICRORESET` in turn
   `MICRORESET_HOLD`, `MICRORESET_STALL`, `MICRORESET_RUN`.
6. Process mailbox messages as described above until TRAINING_DONE or TRAINING_FAILED. On
   TRAINING_FAILED return `-PB_ERR_IO` (after its acknowledge, no further writes).
7. On TRAINING_DONE: write `MICRORESET_STALL` to `DDRPHY_MICRORESET`, then `MICROCONTMUXSEL_APB`
   and then `MICROCONTMUXSEL_PMU` to `DDRPHY_MICROCONTMUXSEL`. Return `PB_OK`.

### `void dwc_ddrphy_wait_calibration(phy)`

Read `DDRPHY_CALBUSY` until `CALBUSY_BUSY` is 0. No writes.

### `void dwc_ddrphy_read_csrs(phy, csrs, n)`

1. Write `MICROCONTMUXSEL_APB` to `DDRPHY_MICROCONTMUXSEL`.
2. Write `UCCLKHCLKENABLES_HCLKEN | UCCLKHCLKENABLES_UCCLKEN` to `DDRPHY_UCCLKHCLKENABLES`.
3. For `i = 0 .. n-1`: `csrs[i].val` = 32 bit read of CSR `csrs[i].reg`.
4. Write `UCCLKHCLKENABLES_HCLKEN` to `DDRPHY_UCCLKHCLKENABLES`.
5. Write `MICROCONTMUXSEL_PMU` to `DDRPHY_MICROCONTMUXSEL`.

## Not required

Timeouts on the polls (open as F-102), decoding streaming messages into text, reading training
results from the message block (F-104).
