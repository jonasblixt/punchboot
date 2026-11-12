# The documents

The code follows its documents, and each document follows the one above it (`process.md`). The
folders are the levels: where a document sits says what it may decide and what it must cite.

Documents cite each other by file name and section (`platform.md` 2.3), a decision as D-nnn
(`dev/decisions.md`), a finding as F-nnn (`dev/findings.md`), a day's log as `log/<date>.md`
(`dev/log/`). File names are unique across the folders and don't change when a document moves.

| Level          | Folder          | Document                             | State (2026-11-12)         |
| -------------- | --------------- | ------------------------------------ | -------------------------- |
| 0 Purpose      |                 | `principles.md`                      | Reviewed (D-035, D-036)    |
|                |                 | `process.md`                         | Reviewed (D-036)           |
| 1 System       | `system/`       | `system.md`                          | Reviewed (D-036)           |
|                |                 | `security.md`                        | Reviewed (D-036)           |
|                |                 | `compatibility.md`                   | In force (the guards)      |
| 2 Architecture | `architecture/` | `boot.md`                            | Reviewed (D-041)           |
|                |                 | `command-mode.md`                    | Reviewed (D-059 to D-061)  |
| 3 Interfaces   | `interfaces/`   | `platform.md`                        | Reviewed (D-056)           |
|                |                 | `board.md`                           | Reviewed (D-052)           |
|                |                 | The drivers' contracts               | Missing                    |
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
|           | `release-checklist.txt` | The steps of a release                                   |
| `dev/`    | `status.md`             | Where the work is: read first                            |
|           | `roadmap.md`            | What comes next, and what is to be discussed             |
|           | `decisions.md`          | Why: D-nnn, appended, never edited                       |
|           | `findings.md`           | The gaps between the documents and the code: F-nnn, open |
|           | `findings-closed.md`    | The fixed and rejected findings                          |
|           | `log/`                  | What was done, per day                                   |

Still to write: the drivers' contracts, a porting guide and a migration guide for 3.0.0
(`guides/`).
