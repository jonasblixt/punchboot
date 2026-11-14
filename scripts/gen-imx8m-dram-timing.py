#!/usr/bin/env python3
"""Write an i.MX8M board's LPDDR4 timing from NXP's DDR tool's output.

  gen-imx8m-dram-timing.py --board <board> --out-dir <dir> [--description <text>]
                           <file> | <name>=<file> [<name>=<file> ...]

Each input is a C file as NXP's DDR tool writes it ("Init Code"): the tables
of struct dram_cfg_param, the training runs (struct dram_fsp_msg) and a
struct dram_timing_info. A board timing file of pb's from before this script
is read the same way.

The output is pb's struct imx8m_dram_timing (plat/imx8m/dram.h), with the
data as the tool generated it. The PHY's tables, its configuration, the CSRs
read back after the training and the PIE image, are packed (struct
dram_phy_table in drivers/ddr/dram_cfg.h): runs of consecutive CSRs with
16-bit values. Each packed table is unpacked again and checked against the
tool's pairs before anything is written.

- One input, without a name: <dir>/lpddr4_timing.c, imx8m_dram_timing, which
  the platform reads by itself (a board with one DRAM).
- Several, <name>=<file> each (one per DRAM density): <dir>/lpddr4_timing_<name>.c
  with dram_timing_<name>, <dir>/lpddr4_timing_common.c with every table the
  inputs have the same, and <dir>/lpddr4_timing.h. The board chooses one in
  its imx8m_board_dram_timing().

The files are written whole: run the script again for a new output of the
tool, never edit them (imx8m-ddr.md).
"""

from __future__ import annotations

import argparse
import pathlib
import re
import sys
import textwrap

SCRIPT = "scripts/gen-imx8m-dram-timing.py"

# The tables of struct dram_timing_info, by their field
PHY_TABLES = ("ddrphy_cfg", "ddrphy_trained_csr", "ddrphy_pie")

# What the PHY tables are, for their comments
PHY_COMMENT = {
    "ddrphy_cfg": "PHY CSRs written before the training",
    "ddrphy_trained_csr": "PHY CSRs read back after the training (no values)",
    "ddrphy_pie": "PHY Init Engine image",
}

# Lines of the input's first comment that say where the data comes from
PROVENANCE = re.compile(
    r"(Copyright \d{4}(-\d{4})? NXP|Code generated|Generated code|DDR PHY FW|Part number|"
    r"Align with|DDR Tool)",
    re.IGNORECASE,
)

MAX_RUN = 0xFFFF


class TimingError(Exception):
    """An input that isn't a timing as the tool writes it."""


Pairs = list[tuple[int, int]]


class Timing:
    """One input: its tables, training runs and struct dram_timing_info."""

    def __init__(self, path: pathlib.Path) -> None:
        """Read the tool's C file at 'path'."""
        self.path = path
        text = path.read_text()
        self.provenance = provenance(text)
        self.tables: dict[str, Pairs] = {}
        for m in re.finditer(
            r"(?:static\s+)?struct\s+dram_cfg_param\s+(\w+)\s*\[\s*\]\s*=\s*\{(.*?)\n\s*\};",
            text,
            re.DOTALL,
        ):
            self.tables[m.group(1)] = pairs(m.group(2))
        self.fsp_msgs = fsp_msgs(text)
        self.info = timing_info(text)
        for field in ("ddrc_cfg", *PHY_TABLES):
            if self.info.get(field) not in self.tables:
                msg = f"{path}: .{field} names no table of the file"
                raise TimingError(msg)
        for msg_ in self.fsp_msgs:
            if msg_["fsp_cfg"] not in self.tables:
                msg = f"{path}: a training run's .fsp_cfg names no table of the file"
                raise TimingError(msg)

    def table(self, field: str) -> Pairs:
        """Return the pairs of struct dram_timing_info's table 'field'."""
        return self.tables[self.info[field]]


def provenance(text: str) -> list[str]:
    """Return the lines of the file's first comment that say where it comes from."""
    m = re.search(r"/\*(.*?)\*/", text, re.DOTALL)
    if not m:
        return []
    lines = [re.sub(r"^\s*\*\s?", "", line).strip() for line in m.group(1).splitlines()]
    return [line for line in lines if PROVENANCE.search(line) and "Jonas" not in line]


def pairs(body: str) -> Pairs:
    """Return a table's (register, value) pairs, comments left out."""
    body = re.sub(r"/\*.*?\*/", "", body, flags=re.DOTALL)
    body = re.sub(r"//[^\n]*", "", body)
    found = re.findall(r"\{\s*(0x[0-9a-fA-F]+|\d+)\s*,\s*(0x[0-9a-fA-F]+|\d+)\s*\}", body)
    if not found:
        msg = "a table without entries"
        raise TimingError(msg)
    return [(int(a, 0), int(v, 0)) for a, v in found]


def fsp_msgs(text: str) -> list[dict[str, str]]:
    """Return the training runs: drate, fw_type, fsp_cfg of each."""
    m = re.search(r"struct\s+dram_fsp_msg\s+(\w+)\s*\[\s*\]\s*=\s*\{(.*?)\n\s*\};", text, re.DOTALL)
    if not m:
        msg = "no struct dram_fsp_msg table"
        raise TimingError(msg)
    runs = []
    body = re.sub(r"/\*.*?\*/", "", m.group(2), flags=re.DOTALL)
    for entry in re.findall(r"\{(.*?)\}", body, re.DOTALL):
        fields = dict(re.findall(r"\.(\w+)\s*=\s*([^,]+?)\s*,", entry + ","))
        if not {"drate", "fw_type", "fsp_cfg"} <= fields.keys():
            msg = f"a training run without .drate, .fw_type or .fsp_cfg: {entry.strip()}"
            raise TimingError(msg)
        runs.append({k: fields[k] for k in ("drate", "fw_type", "fsp_cfg")})
    if not runs:
        msg = "struct dram_fsp_msg without training runs"
        raise TimingError(msg)
    return runs


def timing_info(text: str) -> dict[str, str]:
    """Return struct dram_timing_info's fields: its tables' names and fsp_table."""
    m = re.search(r"struct\s+dram_timing_info\s+\w+\s*=\s*\{(.*?)\n\s*\};", text, re.DOTALL)
    if not m:
        msg = "no struct dram_timing_info: give the tool's output, not a file of this script's"
        raise TimingError(msg)
    body = m.group(1)
    info = dict(re.findall(r"\.(\w+)\s*=\s*(\w+)\s*,", body))
    table = re.search(r"\.fsp_table\s*=\s*\{([^}]*)\}", body)
    if not table:
        msg = "struct dram_timing_info without .fsp_table"
        raise TimingError(msg)
    info["fsp_table"] = ", ".join(v.strip() for v in table.group(1).split(",") if v.strip())
    return info


def pack(table: Pairs, values: bool) -> list[list[int]]:
    """Return the table as runs: [csr >> 16, csr & 0xffff, n, values...] each."""
    runs: list[list[int]] = []
    for csr, val in table:
        if csr >> 32 or (values and val >> 16):
            msg = f"CSR 0x{csr:x}, value 0x{val:x}: wider than the packed table's"
            raise TimingError(msg)
        if not values and val:
            msg = f"CSR 0x{csr:x} read back after the training has a value, 0x{val:x}"
            raise TimingError(msg)
        last = runs[-1] if runs else None
        if last and (last[0] << 16 | last[1]) + last[2] == csr and last[2] < MAX_RUN:
            last[2] += 1
        else:
            last = [csr >> 16, csr & 0xFFFF, 1]
            runs.append(last)
        if values:
            last.append(val)
    return runs


def unpack(runs: list[list[int]], values: bool) -> Pairs:
    """Return a packed table's pairs, as pb's dram_phy_iter_next() reads them."""
    out = []
    for run in runs:
        csr = run[0] << 16 | run[1]
        for i in range(run[2]):
            out.append((csr + i, run[3 + i] if values else 0))
    return out


def c_pairs(name: str, table: Pairs, static: bool) -> str:
    """Return a struct dram_cfg_param table as C."""
    lines = [f"{'static ' if static else ''}struct dram_cfg_param {name}[] = {{"]
    lines += [f"    {{ 0x{a:x}, 0x{v:x} }}," for a, v in table]
    lines.append("};")
    return "\n".join(lines)


def c_runs(name: str, field: str, runs: list[list[int]], static: bool) -> str:
    """Return a packed table's words as C, a run a line."""
    lines = [
        f"/* {PHY_COMMENT[field]}: [csr >> 16, csr & 0xffff, n, values] a run */",
        f"{'static ' if static else ''}const uint16_t {name}[] = {{",
    ]
    lines += ["    " + ", ".join(f"0x{w:x}" for w in run) + "," for run in runs]
    lines.append("};")
    return "\n".join(lines)


def header(lines: list[str], what: str) -> str:
    """Return a file's comment: the tool's provenance, pb's notice and 'what'."""
    copyrights = sorted({line for line in lines if line.lower().startswith("copyright")})
    others = []
    for line in lines:
        if not line.lower().startswith("copyright") and line not in others:
            others.append(line)
    out = ["/**", " * Punch BOOT", " *"]
    out += [f" * {c}" for c in copyrights]
    out += [" * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>", " *"]
    out += [" * SPDX-License-Identifier: BSD-3-Clause", " *"]
    out += [f" * {line}" for line in others]
    if others:
        out.append(" *")
    for para in what.split("\n"):
        out += [f" * {line}" for line in textwrap.wrap(para, 76)] or [" *"]
    out += [" *", f" * Generated by {SCRIPT} from the tool's output: don't edit,", " * run it again."]
    out.append(" */")
    return "\n".join(out)


class Output:
    """The files for the inputs, with the tables they share once."""

    def __init__(self, board: str, timings: dict[str, Timing]) -> None:
        """Work out which tables are common to all 'timings'."""
        self.board = board
        self.timings = timings
        first = next(iter(timings.values()))
        multi = len(timings) > 1
        # A table the inputs have the same goes into the common file, under
        # the board's name: by struct dram_timing_info's field, and the
        # training runs' message blocks by their name
        self.common: dict[str, str] = {}
        if multi:
            for field in ("ddrc_cfg", *PHY_TABLES):
                if all(t.table(field) == first.table(field) for t in timings.values()):
                    self.common[field] = f"{board}_{field}"
            for msg in first.fsp_msgs:
                name = msg["fsp_cfg"]
                if all(t.tables.get(name) == first.tables[name] for t in timings.values()):
                    self.common[name] = f"{board}_{name.removeprefix('ddr_')}"

    def symbol(self, field: str, static_name: str) -> str:
        """Return a table's C name: the common one, or the file's own."""
        return self.common.get(field, static_name)

    def phy(self, timing: Timing, field: str) -> tuple[list[list[int]], int]:
        """Return a PHY table packed, checked against the tool's pairs."""
        values = field != "ddrphy_trained_csr"
        table = timing.table(field)
        runs = pack(table, values)
        if unpack(runs, values) != [(a, v if values else 0) for a, v in table]:
            msg = f"{timing.path}: .{field} doesn't unpack to the tool's pairs"
            raise TimingError(msg)
        return runs, len(table)

    def phy_ref(self, timing: Timing, field: str, name: str) -> str:
        """Return the DRAM_PHY_RUNS() of a PHY table."""
        _, num = self.phy(timing, field)
        values = "false" if field == "ddrphy_trained_csr" else "true"
        return f"DRAM_PHY_RUNS({name}, {num}, {values})"

    def timing_file(self, name: str | None, timing: Timing, description: str) -> str:
        """Return the file of one input: its own tables and its struct."""
        parts = [header(timing.provenance, description), "", "#include <pb/pb.h>"]
        parts += ["#include <plat/imx8m/dram.h>"]
        if name:
            parts += ["", '#include "lpddr4_timing.h"']
        parts.append("")
        own: dict[str, str] = {}
        if "ddrc_cfg" not in self.common:
            own["ddrc_cfg"] = "ddr_ddrc_cfg"
            parts += [c_pairs("ddr_ddrc_cfg", timing.table("ddrc_cfg"), True), ""]
        for field in PHY_TABLES:
            if field not in self.common:
                own[field] = f"ddr_{field}"
                runs, _ = self.phy(timing, field)
                parts += [c_runs(f"ddr_{field}", field, runs, True), ""]
        for msg in timing.fsp_msgs:
            cfg = msg["fsp_cfg"]
            if cfg not in self.common and cfg not in own:
                own[cfg] = cfg
                parts += [c_pairs(cfg, timing.tables[cfg], True), ""]
        parts.append("static const struct dram_fsp_msg ddr_dram_fsp_msg[] = {")
        for msg in timing.fsp_msgs:
            cfg = self.symbol(msg["fsp_cfg"], msg["fsp_cfg"])
            num = len(timing.tables[msg["fsp_cfg"]])
            parts += [
                "    {",
                f"        .drate = {msg['drate']},",
                f"        .fw_type = {msg['fw_type']},",
                f"        .fsp_cfg = {cfg},",
                f"        .fsp_cfg_num = {num},",
                "    },",
            ]
        parts += ["};", ""]
        symbol = f"dram_timing_{name}" if name else "imx8m_dram_timing"
        ddrc = self.symbol("ddrc_cfg", "ddr_ddrc_cfg")
        parts += [
            f"const struct imx8m_dram_timing {symbol} = {{",
            f"    .ddrc_cfg = {ddrc},",
            f"    .ddrc_cfg_num = {len(timing.table('ddrc_cfg'))},",
        ]
        for field in PHY_TABLES:
            if field == "ddrphy_trained_csr":
                parts += [
                    "    .fsp_msg = ddr_dram_fsp_msg,",
                    f"    .fsp_msg_num = {len(timing.fsp_msgs)},",
                ]
            ref = self.phy_ref(timing, field, self.symbol(field, f"ddr_{field}"))
            parts.append(f"    .{field} = {ref},")
        parts += [f"    .fsp_table = {{ {timing.info['fsp_table']} }},", "};", ""]
        return "\n".join(parts)

    def common_file(self, description: str) -> str:
        """Return the file of the tables all inputs have the same."""
        first = next(iter(self.timings.values()))
        lines = []
        for t in self.timings.values():
            lines += [line for line in t.provenance if line not in lines]
        parts = [header(lines, description), "", "#include <pb/pb.h>"]
        parts += ["#include <plat/imx8m/dram.h>", "", '#include "lpddr4_timing.h"', ""]
        for field, name in self.common.items():
            if field in PHY_TABLES:
                runs, _ = self.phy(first, field)
                parts += [c_runs(name, field, runs, False), ""]
            elif field == "ddrc_cfg":
                parts += [c_pairs(name, first.table(field), False), ""]
            else:
                parts += [c_pairs(name, first.tables[field], False), ""]
        return "\n".join(parts)

    def header_file(self) -> str:
        """Return lpddr4_timing.h: the common tables and the timings."""
        guard = f"BOARD_{self.board.upper()}_LPDDR4_TIMING_H"
        names = ", ".join(self.timings)
        parts = [
            header([], f"The {self.board}'s LPDDR4 timings: {names}."),
            "",
            f"#ifndef {guard}",
            f"#define {guard}",
            "",
            "#include <plat/imx8m/dram.h>",
            "#include <stdint.h>",
            "",
        ]
        first = next(iter(self.timings.values()))
        for field, name in self.common.items():
            if field in PHY_TABLES:
                runs, _ = self.phy(first, field)
                words = sum(len(r) for r in runs)
                parts.append(f"extern const uint16_t {name}[{words}];")
            else:
                table = first.table(field) if field == "ddrc_cfg" else first.tables[field]
                parts.append(f"extern struct dram_cfg_param {name}[{len(table)}];")
        if self.common:
            parts.append("")
        parts += [f"extern const struct imx8m_dram_timing dram_timing_{n};" for n in self.timings]
        parts += ["", f"#endif /* {guard} */", ""]
        return "\n".join(parts)


def rates(timing: Timing) -> str:
    """Return the timing's set points, e.g. '3200 and 667 MT/s'."""
    mts = [v for v in timing.info["fsp_table"].split(", ") if v != "0"]
    return (", ".join(mts[:-1]) + " and " + mts[-1] if len(mts) > 1 else mts[0]) + " MT/s"


def main() -> int:
    """Read the inputs, write the board's timing files."""
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--board", required=True, help="the board's directory name, e.g. pico8ml")
    parser.add_argument("--out-dir", required=True, type=pathlib.Path)
    parser.add_argument("--description", default="", help="what the timing is, for the comments")
    parser.add_argument("inputs", nargs="+", help="<file>, or <name>=<file> for each density")
    args = parser.parse_args()

    named = [i.split("=", 1) for i in args.inputs]
    if not (len(named) == 1 and len(named[0]) == 1) and not all(len(n) == 2 for n in named):  # noqa: PLR2004
        parser.error("one <file>, or a <name>=<file> each")

    try:
        if len(named) == 1 and len(named[0]) == 1:
            timings: dict[str | None, Timing] = {None: Timing(pathlib.Path(named[0][0]))}
        else:
            timings = {n: Timing(pathlib.Path(f)) for n, f in named}
        out = Output(args.board, {k or "": v for k, v in timings.items()})
        files: dict[str, str] = {}
        for name, timing in timings.items():
            what = f"The {args.board}'s LPDDR4 timing"
            what += f" {name}" if name else ""
            what += f": {rates(timing)}."
            if args.description:
                what += "\n" + args.description
            file = f"lpddr4_timing_{name}.c" if name else "lpddr4_timing.c"
            files[file] = out.timing_file(name, timing, what)
        if len(timings) > 1:
            files["lpddr4_timing_common.c"] = out.common_file(
                f"The tables the {args.board}'s timings ({', '.join(n for n in timings if n)}) have the same, "
                "once."
            )
            files["lpddr4_timing.h"] = out.header_file()
    except TimingError as e:
        sys.stderr.write(f"{SCRIPT}: {e}\n")
        return 1

    for file, text in files.items():
        (args.out_dir / file).write_text(text)
        sys.stdout.write(f"{args.out_dir / file}\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
