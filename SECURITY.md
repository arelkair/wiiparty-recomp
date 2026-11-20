# Security policy

## Supported versions

Only the latest release receives fixes. Update the launcher and the game before you report a problem.

## Reporting a vulnerability

Please do not open a public issue for a security problem. Use the private report of this repository instead: open the Security tab and press **Report a vulnerability**.

Include:

- The launcher version and build, shown at the bottom left of the launcher.
- What the problem is and what it could allow someone to do.
- The steps to reproduce it, and the file that triggers it if there is one (a Mii file, a texture pack, a save backup, a disc image).

You can expect an answer within a few days. Once the problem is fixed and a release is published, the report can be made public with credit to you, if you want it.

## What counts

- The launcher or the game running code or writing files outside the install folder because of a crafted file or a download.
- A crash or memory corruption caused by a file the game or the launcher loads from disk.
- A weakness in how the launcher downloads and verifies its build tools.

Windows Defender detections of the launcher are not vulnerabilities. They are false positives of an unsigned program; see the warning in the README and the open issue about it.
