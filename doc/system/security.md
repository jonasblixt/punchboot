# The security model

**State: reviewed (Jonas, 2026-11-11), D-034 to D-036.** Level 1 (`process.md`).

What pb protects, against whom, and what it leaves to others. The security clauses of the
documents below cite the rules in section 4 by number.

EOL is in `system.md` 4.

## 1. The chain

```
ROM  ->  pb  ->  the next stage: secure firmware, a secure OS, an operating system,
                 a hypervisor, an application
```

- The ROM is the root: it can't be changed, and on a locked device it runs only a pb whose
  signature verifies against the key hash in the fuses. Where the SoC's vendor firmware runs
  before pb, it belongs to the root as the ROM does.
- pb's trust anchor is its **keystore**: the public keys pb trusts, built into the image the ROM
  verified. pb verifies the next stage's image with it. What the next stage verifies (a root file
  system) is the next stage's.
- pb is the first mutable stage, in BL2's place (`platform.md` 4): it runs with the highest
  privilege the SoC gives a mutable stage, and it leaves the hardware's security state as the
  next stage expects.

Why first, and the cost:

- Everything after pb is an image pb verifies. Secure firmware and the secure OS are updated and
  rolled back with the system (the boot source policy), under one set of keys, and nothing is
  loaded twice. A flow that puts secure firmware before the bootloader leaves that firmware
  outside the system's update.
- The cost: all of pb runs with the highest privilege. Command mode, its transport's stack and
  the parsers are in the trusted computing base, where such a flow runs its large bootloader
  below the secure firmware. What answers it: pb stays simple (`principles.md` 8 is a security
  property too), a locked device exposes almost nothing before authentication (4.7), and pb locks
  what it can before the jump (4.6).
- The first stage's duties are pb's (4.9), and nothing after pb notices when one is missed
  (F-096).
- pb itself is outside the system's update (section 5).

## 2. What is protected

In this order:

1. **What runs next.** Only an image signed with a keystore key that isn't revoked.
2. **The root of trust's state.** Key revocation, the life cycle and the configuration fuses
   change only through pb, for an authorised host.
3. **The secure world.** Its isolation from the normal world, where the SoC provides one and
   setting it up falls to pb.
4. **The device's storage, through command mode.** Reading, writing and erasing partitions.
5. **Availability.** The device boots a verified system when it has one (A/B, the watchdog).
   Below 1 to 4: nothing done for it weakens them.

Not protected today: confidentiality. Images aren't encrypted, and pb holds no private key; the
keystore is public keys. The one secret is the board's password secret (`board.md` 4.4).
Encrypted images are on the roadmap, to discuss.

## 3. Attackers

The model holds on a locked device (`CONFIGURATION_LOCKED`, `system.md` 4). Before the lock the
ROM runs any pb and command mode is open, so whoever reaches the device owns it. pb verifies
images the same way in every state, but that proves nothing when pb itself can be replaced. In
EOL the device is open again; what makes that safe is the SoC's (`system.md` 4).

| Attacker                           | Can                                   | pb's answer                           |
| ---------------------------------- | ------------------------------------- | ------------------------------------- |
| A host on command mode's transport | Send any bytes, at any time pb is in  | 4.7: nothing but the identity         |
| (physical access to the port)      | command mode; force command mode with | before authentication                 |
|                                    | the board's stop or a serial download |                                       |
| Whoever writes the storage (a      | Replace images, the A/B state, the    | 4.1, 4.2: every image verified on     |
| programmer on the eMMC, a          | partition table                       | every boot. The state and the table   |
| compromised OS)                    |                                       | are input: they choose among verified |
|                                    |                                       | images, never an unverified one       |
| A compromised OS, until the next   | Call into what pb left open; reset    | 4.6: revocation and the fuses that    |
| reset                              | into pb                               | reopen the device are locked; 4.9     |
| A signed image                     | Name load addresses and sizes         | 4.2: placement is checked             |
|                                    |                                       | (`principles.md` 2)                   |
| The holder of a revoked key        | Present old images and tokens         | 4.3                                   |

## 4. The rules

Security comes first, also before compatibility (`principles.md`): a rule here may refuse what a
release accepted, with a decision entry that names it.

**4.1 Only verified code runs next.** In every life cycle state and from every source (a
partition, the host's stream, in place). Verification is the core's (D-027): no hook, board or
platform makes pb jump to what it refused, they can only stop a boot. There is no source where a
board authenticates the image itself: the custom source is removed (D-035).

**4.2 Every byte pb loads is authenticated before anything runs or is patched.** The header's
signature and where every part lands are checked before the first payload byte is loaded,
because unverified bytes reach memory before the payload's hash is compared (`boot.md` 4.2,
4.3). An image with a part outside the hash (bpak's `dont-hash`) is refused, before the load:
the flag is for content that is verified at run time (a file system under a hash tree), which pb
doesn't boot.

**4.3 A revoked key verifies nothing**, neither an image nor a token. Revocation is the SoC's,
shared with the ROM (D-027).

**4.4 Fail closed.** A check that can't be made is a no (`principles.md` 1): a key whose status
can't be read is unusable; a life cycle that can't be read requires authentication and doesn't
boot (`system.md` 4).

**4.5 No silent boot.** pb doesn't boot when something the boot's security rests on failed
(`platform.md` 2.3). **Not yet in the code (F-248).** The conditions are sorted by what rests on
them:

- What 2.1 to 2.4 rest on refuses the boot: the secure world's isolation not in effect, a failed
  verification engine without a fallback that verifies the same. pb enters command mode and logs
  the reason.
- What only 2.5 rests on boots and reports: the watchdog that didn't arm. Refusing would leave no
  system at all, which is the availability it was there for. The report is part of pb's
  information in the handover (`system.md` 3), and in the log.
- The same rule in every life cycle state, without a development override: a development board
  is where a missing isolation goes unnoticed (F-096).

Where setting up the secure world's isolation falls to pb, pb always sets it up and always
refuses to boot without it, also for a product with nothing in the secure world. One rule,
no declaration a board can forget (F-096), and one path that every board tests. The cost is a
porting note for 3.0.0: a board whose setup before pb leaves the isolation off must change, or
its new pb refuses to boot. Security before compatibility decides it (`principles.md`).

**4.6 Lock before the jump.** What only pb may change (key revocation, the fuses that reopen a
locked device, the controller settings the SoC can lock) is locked until the next reset before
pb jumps (`platform.md` 4.4). When it can't be locked, pb doesn't jump. What follows then is the
flow's (`boot.md` 2, `platform.md` 4.4).

**4.7 A locked device answers nothing but its identity before authentication** (D-011): the
device and board identifiers, and the authentication command. A life cycle that is unknown counts
as locked; a board without a life cycle is open. A product may build pb without command mode's
authentication (`CONFIG_CM_AUTH` off, on by default): such a build has no gate and its command
mode is open in every state, which is the product's explicit choice (`system.md` 6,
`command-mode.md` 3.2, D-059). Host input is untrusted in every state,
authenticated or not: every size and offset from the host is checked before it is used.
Authentication is by method. Methods are added, never changed (D-008): the token
and the password stay as they are. Today a session is authenticated or not; a new method may
grant less (a token limited in time, or to reading the logs for a return analysis), so the gate
asks the method what the session may do. Roadmap; `command-mode.md` carries the requirement.

**4.8 The core owns the rules.** The order of the life cycle, key revocation, image verification,
the authentication gate and the boot's preconditions are implemented once, in the core. The
platforms and boards provide facts and hardware (D-027).

**4.9 pb completes the SoC's isolation.** Where the SoC separates a secure world from the normal
one and leaves part of the setup to the first mutable stage, pb does that part, checks that it
took effect, and hands the rest over in the state the secure world's owner expects. Which
mechanisms that means is the platform's (`platform.md` 4.3, 4.5). **Not yet in the code (F-241,
F-261, F-246).**

**4.10 Secrets are compared whole and in constant time, and never logged** (`board.md` 4.4).

## 5. Known limits

Not defended today. Each is a decision to keep, or a roadmap item:

- **No anti-rollback.** An older image signed with a key that isn't revoked boots. Only key
  revocation answers it, and the SoC has few slots. On the roadmap: a version floor as a
  verification step (`boot.md` 4.2).
- **The token is static.** It is a signature over the device's UUID, without a challenge: whoever
  holds a device's token holds it until its key is revoked. It stays supported (D-008); better
  methods are added next to it (4.7).
- **pb's own update has no rollback.** pb is outside the boot source policy; replacing it is the
  product's procedure.
- **Physical attacks on the SoC**: fault injection, probing the buses, the debug port. The debug
  port is the SoC's locked state, not pb's.
- **Denial of service** by whoever writes the storage or holds the board's stop: the device
  ends in command mode, locked.
- **The signing keys' custody and the build.** A pb built with the wrong keystore or key hash is
  the product's loss (F-183: a stale build keeps the test keys).
- **A compromised key until it is revoked**, and revoking takes an authenticated session.

## 6. Review checklist

For a change to anything the rules name:

- [ ] Does it add a path to the jump? Then it goes through 4.1, 4.2, 4.5 and 4.6.
- [ ] Does it add a command or an authentication method? Then 4.7, and its bounds checks are in
      the core.
- [ ] Does a failure now only log? Then sort it with 4.5.
- [ ] Does a board or a platform decide something 4.8 gives the core?
- [ ] Does a new limit belong in section 5?
