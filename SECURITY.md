# Security

## Reporting a vulnerability

Send it to Jonas Blixt <jonas@blxt.se>, and not to the public issue tracker.

What helps:

- The version: the release, or the commit, and for the host tools the `punchboot` package's
  version.
- The board or the SoC, and the device's security life cycle state.
- What an attacker needs (physical access, the USB port, an image, a token) and what they gain.
- The steps, an image or a transcript that shows it.

## What pb protects

`doc/system/security.md` is the security model: what pb protects, against whom in each life
cycle state, and what it leaves to others. A report is easiest to place against its rules.

The releases' known limits are in `CHANGELOG.md`, under "Known".
