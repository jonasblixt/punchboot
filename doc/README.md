# The documents

What pb does at a boundary is decided in its documents before it is coded, and each document
follows the one above it (`process.md`). The folders are the levels: where a document sits says
what it may decide and what it must cite.

Documents cite each other by file name and section (`platform.md` 2.3), and a decision as D-nnn
(`decisions.md`). File names are unique across the folders and don't change when a document
moves.

The working records aren't in the repository (D-098): the maintainers keep them in a repository
of their own. The documents and the code still cite them, a finding as F-nnn (`findings.md`,
`findings-closed.md`), a day's log as `log/<date>.md`, and `status.md` and `roadmap.md` by name.
What a reader needs of a finding is in the text that cites it; the releases' known limits are in
`CHANGELOG.md`.

| Level          | Folder          | Document                             | State (2026-11-12)         |
| -------------- | --------------- | ------------------------------------ | -------------------------- |
| 0 Purpose      |                 | `principles.md`                      | Reviewed (D-035, D-036)    |
|                |                 | `process.md`                         | Reviewed (D-036, D-063)    |
| 1 System       | `system/`       | `system.md`                          | Reviewed (D-036)           |
|                |                 | `security.md`                        | Reviewed (D-036)           |
|                |                 | `compatibility.md`                   | In force (the guards)      |
| 2 Architecture | `architecture/` | `boot.md`                            | Reviewed (D-041)           |
|                |                 | `command-mode.md`                    | Reviewed (D-059 to D-061)  |
| 3 Interfaces   | `interfaces/`   | `platform.md`                        | Reviewed (D-056)           |
|                |                 | `board.md`                           | Reviewed (D-052)           |
|                |                 | `drivers.md`                         | Reviewed (D-066)           |
|                |                 | `bio.md`                             | Reviewed (D-070, D-093)    |
|                |                 | `crypto.md`                          | Reviewed (D-071)           |
|                |                 | `usb.md`                             | Reviewed (D-072)           |
| 4 Ports        | `ports/`        | `plat-imx.md`                        | Follows `platform.md`      |
|                |                 | `imx8m-ddr.md`, `spec-dwc-ddrphy.md` | Records of one port's work |

The states (draft, reviewed, in force) are `process.md` 4. A document also says its state in its
first lines; this table is the overview.

Beside the levels, not normative:

| Folder    | Document                | What                                                     |
| --------- | ----------------------- | -------------------------------------------------------- |
| `guides/` | `build-and-test.md`     | Building firmware, the dev container, CI, releasing      |
|           | `testing.md`            | Writing and running the host tests                       |
|           | `bpak.md`               | The vendored bpak and how to update it                   |
|           | `firmware-handoff.md`   | A brief on Arm's Firmware Handoff, for `boot.md` 5.4     |
|           | `migration-3.0.md`      | What an out-of-tree board, driver or platform changes    |
|           |                         | from 2.x                                                 |
|           | `porting.md`            | A new board, driver or platform: the order, and where    |
|           |                         | each rule is                                             |
|           | `release-checklist.txt` | The steps of a release                                   |
|           | `slc.md`                | The life cycle on a device: configure, lock, revoke      |
|           |                         | (draft)                                                  |
|           | `updates.md`            | A product's A/B update with `pbstate`, the OS's side     |
|           |                         | (draft)                                                  |
|           | `vendored.md`           | The code pb didn't write: upstream, version, licence and |
|           |                         | local changes of each piece                              |
| `doc/`    | `decisions.md`          | Why: D-nnn, appended; the index says what still holds    |
