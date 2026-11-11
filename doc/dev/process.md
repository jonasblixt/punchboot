# The process: documents first, top down

**State: reviewed (Jonas, 2026-11-11), D-033, D-036.**

How punchboot is developed. The code follows its documents, and each document follows the one
above it. Read this before writing or changing a contract, and to find where a change starts.

## 1. The rule

- **Documents first.** A change to what pb does at a boundary is written in the document that
  owns it, reviewed, and coded after that. A bug is code that doesn't do what its document says:
  it is fixed in the code (`bugfix:`). When the document doesn't say, the document comes first.
- **Top down.** A document decides only what its level owns. What it needs from above it cites;
  what it can't answer from above goes up as an open item of the level that owns the question, and
  is never decided below.
- **Facts come from below, decisions from above.** The code, the hardware and the vendors'
  manuals give facts: a finding (`findings.md`) or a port's document (level 4). They can show that
  a document above is wrong. Then that document changes first, and the code after it.

Why top down: the platform contract was written first, and it now waits for `board.md` and
`boot.md` (its "follows" markers, six sections). Their open items in turn ask what pb defends
against and what it promises, which no document says yet.

## 2. The levels

| Level        | Answers                                              | Documents                                |
| ------------ | ---------------------------------------------------- | ---------------------------------------- |
| 0 Purpose    | What wins when goals conflict                        | `principles.md`                          |
| 1 System     | What pb promises to the outside, and against whom    | `system.md`, `security.md`,              |
|              |                                                      | `compatibility.md`                       |
| 2 Flows      | What happens, in which order, and who owns each step | `boot.md`, `command-mode.md` (new),      |
|              |                                                      | `arch-firmware.md`, `arch-host-tools.md` |
| 3 Interfaces | What a layer provides and may rely on                | `platform.md`, `board.md`,               |
|              |                                                      | the drivers' contracts (new)             |
| 4 Ports      | How an implementation meets its contract;            | `plat-imx.md`, `imx8m-ddr.md`,           |
|              | the SoC's facts and their sources                    | `spec-dwc-ddrphy.md`                     |
| 5 Code       |                                                      | The headers, the tests, the guards       |

- **`system.md`** (draft, D-034): pb's place in the boot chain and its outside interfaces: the ROM before
  it, the host on the wire, the image, the next stage (the handover), the OS (`pbstate`, the DT
  properties), the factory (provisioning, fuses). For each: what pb guarantees and what it
  expects. The security life cycle's states and what each allows.
- **`security.md`** (draft, D-034): the threat model. What is protected, the attackers per life cycle
  state (physical access, the wire, the storage, a compromised OS), what pb defends and what it
  leaves to the ROM, the next stage or the product. The security clauses below cite it.
- **`command-mode.md`** (new): each command's meaning, in which state and with which
  authentication it runs, what a failure leaves behind (F-186..F-190). `compatibility.md` freezes
  the bytes, this the behaviour.
- **The drivers' contracts** (new): per class with several implementations (block devices, the
  crypto backends, the USB device controllers, DMA and caches; `log/2026-11-11.md`).

Beside the levels, not normative: `decisions.md` (why), `findings.md` (the gaps between a document
and the code), `status.md` and `log/` (where the work is), `roadmap.md` (the order), `testing.md`
and `build-and-test.md` (how to). The guides (the README, a porting guide, a migration guide for
3.0.0) explain and cite the contracts; they decide nothing.

What gets a contract: an interface with several implementations, with users outside the tree or
the release, or one that security rests on. Elsewhere the header's comments are enough.

## 3. A contract's form

As `platform.md` and `board.md`:

- The first lines: what it covers, its decision, its **state** (4), what to read with it.
- Entries tagged `[mandatory]`, `[optional]` or `[mandatory when X]`. Each says what it does,
  from when it is valid, what it may rely on and what a failure means.
- A clause that follows from a level above cites it (`security.md` 3.2). A clause that waits for
  another draft says "follows `<document>`".
- **Open (n)**: a proposal that isn't confirmed, listed at the top and marked in the text.
- **Not yet in the code (F-nnn)**: what the code doesn't do yet. The commit that fixes the
  finding removes the marker.
- A section for what is fixed like the wire, listed in `compatibility.md`, "Fixed elsewhere".
- A review checklist for a change or a new implementation.
- A conformance table per implementation, here or in the port's document, with how each row is
  verified: a test, the bench, or review only.

## 4. States

| State    | Means                                                     | To change it           |
| -------- | --------------------------------------------------------- | ---------------------- |
| Draft    | Under discussion, open items at its top                   | Edit                   |
| Reviewed | Jonas went through it, no open items. The code follows it | Edit, noted in the log |
| In force | One implementation meets every mandatory entry            | A decision entry first |

- A document is reviewed when the ones above it are. A draft below may exist earlier, with its
  "follows" markers.
- A draft may have a spike: code on a branch that shows whether the text holds, redone after the
  review.
- "In force" is its own state. Until an implementation conforms, a clause can still
  turn out to be wrong, so a change is cheap; after it, a change is a decision. What out-of-tree
  users build on (`struct board`, the hooks, the build interface) changes only in a major release
  in either state (D-031).
- When a document above changes, the "follows" markers and citations below name what to revisit.

## 5. The path of a change

1. Find the highest level the change touches. Start there.
2. Write it into the documents, top down. A change of direction gets a decision entry.
3. Jonas reviews.
4. Every gap to the code becomes a finding and a marker.
5. The code: a commit per finding. It removes the marker, updates the conformance table and adds
   the test, named after the clause where one can be written.
6. `status.md` and the log.

## 6. Where the documents are (2026-11-11)

| Document               | Level | State                                                         |
| ---------------------- | ----- | ------------------------------------------------------------- |
| `principles.md`        | 0     | Reviewed (Jonas, 2026-11-11: security first, D-035)           |
| `security.md`          | 1     | Draft, second round: 3 answers to confirm                     |
| `system.md`            | 1     | Draft, second round: 1 open (EOL)                             |
| `compatibility.md`     | 1     | In force (the guards)                                         |
| `boot.md`              | 2     | Draft, 15 open                                                |
| `command-mode.md`      | 2     | Missing                                                       |
| `arch-firmware.md`     | 2     | Behind the code (waits for the board work's review)           |
| `board.md`             | 3     | Draft, waits for the review                                   |
| `platform.md`          | 3     | Reviewed once, 7 open again, follows `boot.md` and `board.md` |
| The drivers' contracts | 3     | Missing                                                       |
| `plat-imx.md`          | 4     | Follows `platform.md`                                         |

Level 1 was written after three drafts below it. Their open items that it owns moved up and
were reviewed there (D-035):

| Below                                        | In level 1                                             |
| -------------------------------------------- | ------------------------------------------------------ |
| `platform.md` Open (1), the bypassed filter  | `security.md` Open (2): always, to confirm             |
| `platform.md` 2.3, the preconditions' policy | `security.md` 4.5, accepted                            |
| `platform.md` Open (2), a failed lockdown    | `security.md` 4.6: no jump; what follows is the flow's |
| `boot.md` Open (7), `dont-hash`              | Decided: refused (`security.md` 4.2)                   |
| `boot.md` Open (11), the custom source       | Decided: removed (`security.md` 4.1)                   |
| `boot.md` Open (3), (9), speculative mode    | `security.md` 2.5: availability ranks below integrity  |

The review order: the second round of `security.md` and `system.md`, then `boot.md`, `board.md`,
`platform.md`.

## 7. Guards

Not yet:

- **A document lint** in `scripts/pbdev test`: every `Function :` and `Data :` entry of a
  contract is in its header (`plat.h`, `pb/board.h`) and the other way round; every F-nnn in a
  marker is an open finding; every document states its state.
- **Conformance suites**: one per contract that can run on the host, against every
  implementation and the fakes (F-182 hid behind a fake stricter than the driver); on the bench
  through the simple target (D-025).
- **A test record per release**: what ran on which board and what never did (fuse writes, the 8X
  closed state), next to `doc/release-checklist.txt`.
