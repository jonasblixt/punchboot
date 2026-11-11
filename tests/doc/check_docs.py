#!/usr/bin/env python3
"""Check that the documents agree with each other and with the code (doc/process.md 7).

The code follows its documents, so a document that drifts is a bug. This checks,
without building anything:

  states     doc/README.md lists every document of a level, and a document's
             "State:" line says what the map says.
  names      A document that is cited (`platform.md`) exists, and file names are
             unique across doc/.
  sections   A cited section (`platform.md` 2.3) exists. "n.m" is a heading, or
             item m of the list in section n.
  ids        Every F-nnn is a finding and every D-nnn a decision.
  markers    Every finding in a "Not yet in the code (F-nnn)" marker is open.
  paths      Every doc/... path named in the tree exists.
  contracts  The entries of platform.md and board.md are in their headers and
             the other way round; an entry that isn't there yet has a marker.

The log, the decisions and the closed findings are records: they keep the names
and numbers of their day and are not checked.

  check_docs.py [<repository root>]
"""

from __future__ import annotations

import dataclasses
import pathlib
import re
import sys

RECORDS = ("dev/log/", "dev/decisions.md", "dev/findings-closed.md")
NOT_LEVELS = ("dev/", "guides/", "README.md")
STATES = ("draft", "reviewed", "in force")
CODE_DIRS = ("include", "src", "tools", "tests", "scripts", ".github")
CODE_FILES = ("CLAUDE.md", "README.md", "Makefile", "CMakeLists.txt")
VENDORED = (
    "scripts/kconfiglib.py",
    "src/lib/",
    "src/plat/imx8x/sci/",
    "src/boot/atf-interface/",
    "include/xlat_tables/",
)
CODE_SUFFIXES = {".c", ".h", ".S", ".py", ".sh", ".yml", ".yaml", ".txt", ".md", ".cmake", ""}

NAME_RE = re.compile(r"`([A-Za-z0-9_./-]+\.md)`")
NUMBER = r"\d+(?:\.\d+)*"
SECTION_RE = re.compile(rf"(?:,? |\n *)({NUMBER})(?![\w%-])")
MORE_RE = re.compile(rf"(?:, and |, | and | to |,\n *| and\n *| to\n *)({NUMBER})(?![\w%-])")
HEADING_RE = re.compile(rf"^(#+) +(?:({NUMBER})\.? +)?(.*)$")
ITEM_RE = re.compile(r"^(\d+)\. ")
BOLD_RE = re.compile(rf"^\*\*({NUMBER}) ")
MARKER_RE = re.compile(r"Not\s+yet\s+in\s+the\s+code\s*\(([^)]*)\)")
FINDING_RE = re.compile(r"\bF-\d{3}\b")
DECISION_RE = re.compile(r"\bD-\d{3}\b")
PATH_RE = re.compile(r"\bdoc/[A-Za-z0-9_./-]+\.(?:md|txt|png)\b")
ENTRY_RE = re.compile(r"(?:Functions?|Data) *: *(.*?)(?: *\[.*)?$")
STATE_RE = re.compile(r"\*\*State: ([a-z ]+)")


@dataclasses.dataclass
class Section:
    """A numbered section of a document."""

    number: str
    title: str
    line: int
    intro: str = ""  # Its text up to the first sub-heading
    body: str = ""  # Its text with its sub-sections


@dataclasses.dataclass
class Document:
    """A document and what can be cited in it."""

    path: pathlib.Path
    text: str
    sections: list[Section]
    anchors: set[str]

    def section(self, number: str) -> Section | None:
        """Return the section with this number."""
        return next((s for s in self.sections if s.number == number), None)

    def parent(self, section: Section) -> Section | None:
        """Return the section one level up."""
        return self.section(section.number.rpartition(".")[0])


@dataclasses.dataclass
class Report:
    """The problems found."""

    root: pathlib.Path
    problems: list[str] = dataclasses.field(default_factory=list)

    def add(self, path: pathlib.Path, text: str, position: int, problem: str) -> None:
        """Record a problem at a position in a file's text."""
        line = text.count("\n", 0, position) + 1
        self.problems.append(f"{path.relative_to(self.root)}:{line}: {problem}")


def without_code_blocks(text: str) -> str:
    """Blank out fenced code blocks, keeping the line numbers."""
    out = []
    fenced = False
    for line in text.split("\n"):
        if line.lstrip().startswith("```"):
            fenced = not fenced
            out.append("")
        else:
            out.append("" if fenced else line)
    return "\n".join(out)


def parse(path: pathlib.Path) -> Document:
    """Read a document: its numbered sections and the numbers that can be cited."""
    text = without_code_blocks(path.read_text(encoding="utf-8"))
    sections: list[Section] = []
    items: list[tuple[str, str]] = []  # (section number, item number)
    numbered: set[str] = set()
    current = ""
    for number, line in enumerate(text.split("\n"), 1):
        heading = HEADING_RE.match(line)
        if heading:
            current = heading.group(2) or ""
            if current:
                sections.append(Section(current, heading.group(3), number))
            for section in sections:
                if current.startswith(section.number + "."):
                    section.intro += "\0"  # A sub-heading ends the intro
            continue
        item = ITEM_RE.match(line)
        if item:
            items.append((current, item.group(1)))
        bold = BOLD_RE.match(line)
        if bold:
            numbered.add(bold.group(1))  # A rule: "**4.1 Only verified code runs next.**"
        for section in sections:
            if section.number == current or current.startswith(section.number + "."):
                section.body += line + "\n"
                if "\0" not in section.intro:
                    section.intro += line + "\n"
    anchors = {section.number for section in sections} | numbered
    if sections:
        anchors |= {f"{section}.{item}" for section, item in items if section}
    else:
        anchors |= {item for _, item in items}
    return Document(path, text, sections, anchors)


def is_record(doc: pathlib.Path, path: pathlib.Path) -> bool:
    """Return whether a file under doc/ is a record of its day."""
    name = path.relative_to(doc).as_posix()
    return name.startswith(RECORDS)


def load(doc: pathlib.Path, report: Report) -> dict[str, Document]:
    """Read every document under doc/ that is checked, by file name."""
    documents: dict[str, Document] = {}
    for path in sorted(doc.rglob("*.md")):
        if "/log/" in path.as_posix():
            continue
        if path.name in documents:
            other = documents[path.name].path.relative_to(report.root)
            report.add(path, "", 0, f"the file name is also {other}'s: names are unique")
            continue
        documents[path.name] = parse(path)
    return documents


def table_rows(text: str, key: int) -> list[tuple[int, list[str]]]:
    """Return the rows of every table: the position and the cells.

    A row whose cell in the key column is empty continues the row before it.
    """
    rows: list[tuple[int, list[str]]] = []
    position = 0
    for line in text.split("\n"):
        if line.startswith("|") and not re.match(r"\|[ :-]*-", line):
            cells = [cell.strip() for cell in line.strip().strip("|").split("|")]
            if rows and len(cells) == len(rows[-1][1]) and len(cells) > key and not cells[key]:
                rows[-1] = (rows[-1][0], [f"{a} {b}".strip() for a, b in zip(rows[-1][1], cells)])
            else:
                rows.append((position, cells))
        elif rows and rows[-1][1]:
            rows.append((position, []))  # A table ends
        position += len(line) + 1
    return [row for row in rows if row[1]]


def state_of(document: Document) -> str | None:
    """Return the state a document's first lines give it."""
    head = "\n".join(document.text.split("\n")[:12])
    match = STATE_RE.search(head)
    if not match:
        return None
    return next((state for state in STATES if match.group(1).startswith(state)), match.group(1))


def check_states(doc: pathlib.Path, documents: dict[str, Document], report: Report) -> set[str]:
    """Check the map in README.md against the documents. Return the ones not written yet."""
    readme = documents["README.md"]
    listed: dict[str, str] = {}
    last = ["", ""]
    for position, cells in table_rows(readme.text, 2):
        if len(cells) != 4 or cells[0] == "Level":  # noqa: PLR2004
            continue
        last = [cells[0] or last[0], cells[1] or last[1]]
        for name in NAME_RE.findall(cells[2]):
            listed[name] = cells[3]
            wanted = next((s for s in STATES if cells[3].lower().startswith(s)), None)
            if cells[3].startswith("Missing"):
                if name in documents:
                    report.add(readme.path, readme.text, position, f"{name} exists, not missing")
                continue
            if name not in documents:
                report.add(readme.path, readme.text, position, f"{name} doesn't exist")
            elif state_of(documents[name]) != wanted:
                found = state_of(documents[name]) or "no state"
                problem = f"{name} is '{cells[3]}' here and says '{found}' in its first lines"
                report.add(readme.path, readme.text, position, problem)
    for name, document in documents.items():
        relative = document.path.relative_to(doc).as_posix()
        if not relative.startswith(NOT_LEVELS) and name not in listed:
            report.add(document.path, "", 0, "not in doc/README.md's table of the levels")
    return {name for name, state in listed.items() if state.startswith("Missing")}


def resolve(name: str, root: pathlib.Path, documents: dict[str, Document]) -> bool:
    """Return whether a cited name is a document or a file in the tree."""
    if name.startswith("log/"):
        return (root / "doc" / "dev" / name).exists()
    if "/" in name:
        return (root / name).exists() or (root / "doc" / name).exists()
    return name in documents or (root / name).exists()


def cited_sections(text: str, end: int) -> list[str]:
    """Return the section numbers that follow a cited name."""
    first = SECTION_RE.match(text, end)
    if not first:
        return []
    after = text[first.end() : first.end() + 2]
    if "\n" in first.group(0) and after.startswith("."):
        return []  # The next line is a list item, not a section
    numbers = [first.group(1)]
    position = first.end()
    while more := MORE_RE.match(text, position):
        numbers.append(more.group(1))
        position = more.end()
    return numbers


def check_citations(
    document: Document, documents: dict[str, Document], missing: set[str], report: Report
) -> None:
    """Check the documents and the sections a document cites."""
    text = document.text
    for match in NAME_RE.finditer(text):
        name = match.group(1)
        if name in missing or "<" in name or "*" in name:
            continue
        if not resolve(name, report.root, documents):
            report.add(document.path, text, match.start(), f"`{name}`: no such document")
            continue
        target = documents.get(name)
        if target is None or "/" in name:
            continue
        for number in cited_sections(text, match.end()):
            if number not in target.anchors:
                problem = f"`{name}` {number}: no such section"
                report.add(document.path, text, match.start(), problem)


def identifiers(path: pathlib.Path, pattern: re.Pattern[str], row: bool) -> set[str]:
    """Return the ids a record file defines: its headings, or the first cell of its rows."""
    text = path.read_text(encoding="utf-8") if path.exists() else ""
    start = r"^\| " if row else r"^## "
    return set(re.findall(start + r"(" + pattern.pattern.strip(r"\b") + r")\b", text, re.MULTILINE))


def check_ids(document: Document, known: dict[str, set[str]], report: Report) -> None:
    """Check that every finding and decision a document names exists, and the markers."""
    text = document.text
    for pattern, kind in ((FINDING_RE, "finding"), (DECISION_RE, "decision")):
        for match in pattern.finditer(text):
            if match.group(0) not in known[kind]:
                report.add(document.path, text, match.start(), f"{match.group(0)}: no such {kind}")
    marked = [(m.start(), f) for m in MARKER_RE.finditer(text) for f in FINDING_RE.findall(m[1])]
    for section in document.sections:
        if "Not yet in the code" in section.title:
            position = len("\n".join(text.split("\n")[: section.line - 1])) + 1
            rows = table_rows(section.body, 0)
            marked += [(position, f) for _, cells in rows for f in FINDING_RE.findall(cells[0])]
    for position, finding in marked:
        if finding in known["finding"] and finding not in known["open"]:
            problem = f"{finding} is closed and still has a 'Not yet in the code' marker"
            report.add(document.path, text, position, problem)


def code_files(root: pathlib.Path) -> list[pathlib.Path]:
    """Return the files outside doc/ that may name a document."""
    files = [root / name for name in CODE_FILES if (root / name).exists()]
    for name in CODE_DIRS:
        for path in sorted((root / name).rglob("*")):
            relative = path.relative_to(root).as_posix()
            if relative.startswith(VENDORED) or path == pathlib.Path(__file__).resolve():
                continue
            if path.is_file() and path.suffix in CODE_SUFFIXES:
                files.append(path)
    return files


def check_paths(path: pathlib.Path, report: Report) -> None:
    """Check that every doc/... path a file names exists."""
    try:
        text = path.read_text(encoding="utf-8")
    except UnicodeDecodeError:
        return
    for match in PATH_RE.finditer(text):
        if "YYYY" not in match.group(0) and not (report.root / match.group(0)).exists():
            report.add(path, text, match.start(), f"{match.group(0)}: no such file")


def entries(document: Document) -> list[tuple[Section, list[str]]]:
    """Return a contract's entries: the section and the names it declares."""
    found = []
    for section in document.sections:
        match = ENTRY_RE.match(section.title)
        if match:
            names = [name.strip().removesuffix("()") for name in match.group(1).split(",")]
            found.append((section, names))
    return found


def has_marker(document: Document, section: Section) -> bool:
    """Return whether an entry, or the intro of the section it is in, has a marker."""
    parent = document.parent(section)
    return bool(MARKER_RE.search(section.body) or (parent and MARKER_RE.search(parent.intro)))


def without_comments(text: str) -> str:
    """Remove C comments."""
    return re.sub(r"/\*.*?\*/|//[^\n]*", "", text, flags=re.DOTALL)


def struct_members(body: str) -> set[str]:
    """Return the names of a struct's members, function pointers too."""
    members = set()
    for statement in body.split(";"):
        pointer = re.search(r"\(\*(\w+)\)\s*\(", statement)
        plain = re.search(r"(\w+)\s*(?:\[[^\]]*\])?\s*$", statement)
        if pointer:
            members.add(pointer.group(1))
        elif plain:
            members.add(plain.group(1))
    return members


def check_entries(document: Document, declared: set[str], header: str, report: Report) -> None:
    """Check that a contract's entries are declared, or marked as not yet in the code."""
    for section, names in entries(document):
        for name in names:
            if name not in declared and not has_marker(document, section):
                position = len("\n".join(document.text.split("\n")[: section.line - 1])) + 1
                problem = f"{name}: not in {header} and no 'Not yet in the code' marker"
                report.add(document.path, document.text, position, problem)


def check_declared(
    document: Document, declared: set[str], header: pathlib.Path, report: Report
) -> None:
    """Check that the contract names everything its header declares."""
    for name in sorted(declared):
        if not re.search(rf"\b{re.escape(name)}\b", document.text):
            report.add(header, "", 0, f"{name}: not in {document.path.name}")


def check_contracts(root: pathlib.Path, documents: dict[str, Document], report: Report) -> None:
    """Check the platform's and the board's contracts against their headers."""
    plat_h = root / "include" / "pb" / "plat.h"
    functions = set(re.findall(r"\b(plat_\w+)\s*\(", without_comments(plat_h.read_text())))
    data = set(re.findall(r"\bextern\b[^;]*?\b(\w+)\s*(?:\[[^\]]*\])?;", plat_h.read_text()))
    platform = documents["platform.md"]
    check_entries(platform, functions | data, "include/pb/plat.h", report)
    check_declared(platform, functions | data, plat_h, report)

    board_h = root / "include" / "pb" / "board.h"
    body = re.search(r"struct board \{(.*?)\n\};", without_comments(board_h.read_text()), re.DOTALL)
    members = struct_members(body.group(1) if body else "")
    board = documents["board.md"]
    check_entries(board, members, "struct board (include/pb/board.h)", report)
    check_declared(board, members, board_h, report)

    headers = [root / "include" / "pb" / "boot.h", *sorted((root / "include" / "boot").glob("*.h"))]
    text = "".join(without_comments(h.read_text()) for h in headers if h.exists())
    check_entries(documents["boot.md"], set(re.findall(r"\b(\w+)\s*\(", text)), "include/boot/", report)


def main() -> int:
    """Check the documents of the repository and print what is wrong."""
    root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else __file__).resolve()
    if len(sys.argv) <= 1:
        root = root.parents[2]
    doc = root / "doc"
    report = Report(root)
    documents = load(doc, report)
    missing = check_states(doc, documents, report)
    known = {
        "decision": identifiers(doc / "dev" / "decisions.md", DECISION_RE, row=False),
        "open": identifiers(doc / "dev" / "findings.md", FINDING_RE, row=True),
    }
    closed = identifiers(doc / "dev" / "findings-closed.md", FINDING_RE, row=True)
    known["finding"] = known["open"] | closed
    for document in documents.values():
        if not is_record(doc, document.path):
            check_citations(document, documents, missing, report)
            check_ids(document, known, report)
            check_paths(document.path, report)
    for path in code_files(root):
        check_paths(path, report)
    check_contracts(root, documents, report)
    for problem in report.problems:
        sys.stdout.write(problem + "\n")
    sys.stdout.write(f"{len(documents)} documents, {len(report.problems)} problems\n")
    return 1 if report.problems else 0


if __name__ == "__main__":
    sys.exit(main())
