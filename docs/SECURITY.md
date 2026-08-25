# Security

## Supported versions

Security fixes are considered for the latest released **v0.1.x** Windows build of 3DS Studio.

## Reporting a vulnerability

Please **do not** open a public GitHub Issue for security bugs.

Use **GitHub Security Advisories** (private vulnerability reporting) on this repository:

1. Open the repo on GitHub.
2. **Security** tab → **Advisories** → **Report a vulnerability**  
   (or **Settings → Code security** if reporting is enabled there).

Include:

- Studio version (e.g. `0.1.0`)
- OS version
- Steps to reproduce
- Impact (what an attacker could do)

We will acknowledge privately and coordinate disclosure. There is no bug bounty.

## Scope notes

- Studio is a local Windows editor that runs shell builds against a user-installed **devkitPro**. Treat untrusted project folders carefully.
- Do not use Issues to request console exploit chains, piracy, or firmware redistribution — those are out of scope. See [LEGAL.md](LEGAL.md).

## Non-security bugs

Use GitHub Issues with the **bug** or **toolchain** templates.
