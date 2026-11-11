# Guiding principles

How punchboot is designed and reviewed, and what wins when goals conflict. A decision that departs
from one says so and why (`decisions.md`). New principles are added here, not only in a decision.

Order: 1 comes first and 8 last. Security (2, 3) comes before boot time (4), and both before
little code (Jonas, 2026-11-11). 5 to 7 aren't ranked against 2 to 4 yet; such a conflict is
decided case by case and recorded.

1. **Never break compatibility** (D-008). The wire, the bpak format, the device identity and what
   a product in the field relies on (`compatibility.md`, `platform.md` 3.7, `board.md` 8). The hard
   rule, above everything else.
2. **Fail closed, never silently** (D-027, D-030). When a security check can't be made, the answer
   is no. When something the boot's security depends on failed, pb doesn't boot; logging it is not
   enough (`platform.md` 2.3).
3. **Defence in depth** (Jonas, 2026-11-11). A check stays when another one already covers the
   case, and each layer checks its own inputs. A signed image is trusted, yet pb still checks where
   its parts land, because unverified bytes reach memory before the payload hash is compared.
4. **Keep the boot path short** (D-023). Boot time is critical. What only command mode needs is
   set up in command mode, hooks on the boot path start slow hardware and don't wait for it
   (`board.md` 5).
5. **Lean on standards where they apply** (Jonas, 2026-11-11). Arm's Firmware Handoff for the
   generic handover, the Linux boot protocol, TF-A's `bl_params`, the devicetree specification;
   TF-A's porting guide for the style of the contracts. Where one is inspiration, not a compliance
   target, the document says so (D-030: TBBR, PSA, FWU).
6. **Cohesion, flexibility, readability, reviewability.** One responsibility per module and a
   written contract per interface (`platform.md`, `board.md`). The core owns the rules, the
   platforms and boards provide facts and hardware (D-027); a board is data (D-026). Contracts are
   vendor neutral, vendor facts live in their own documents (D-030).
7. **Out-of-tree users exist** (Jonas, 2026-11-11). A feature without a caller in the tree is used
   by projects that aren't visible here (the CUSTOM and in-memory boot sources, speculative
   rollback, `set_dtb_boot_arg`). "Unused in the tree" is not a reason to remove anything; a major
   release (D-018) may change how a feature is used, never drop it without asking.
8. **Little code.** The lowest priority (Jonas, 2026-11-11): less code is better when nothing above
   is lost, and the design deviates from it for better cohesion, flexibility, readability or
   reviewability.
