

<div align="center">

# LibFPrint

This is an experimental libfprint fork adding drivers for Goodix fingerprint sensors.

- **27c6:55a4** (88×108, TLS) &nbsp;—&nbsp; **working**: enrol, verify & identify via `fprintd`
- 27c6:5110 (80×64) &nbsp;—&nbsp; in progress

*LibFPrint is part of the **[FPrint][Website]** project.*

<br/>

[![Button Website]][Website]
[![Button Documentation]][Documentation]

[![Button Supported]][Supported]
[![Button Unsupported]][Unsupported]

[![Button Contribute]][Contribute]
[![Button Contributors]][Contributors]

</div>

## Status — Goodix `27c6:55a4`

The `goodix5xx` driver targets the Goodix `27c6:55a4`, an 88×108 image sensor
that speaks TLS-PSK. It was removed from libfprint's blacklist upstream but had
no driver; this fork provides one. It is **functional end to end** on the
author's hardware:

- ✅ USB transport + message framing (ported from [goodix-fp-dump])
- ✅ OpenSSL PSK-TLS session (the sensor is a TLS 1.2 PSK client; the host is the server)
- ✅ Image capture — finger-down/lift detection, calibration, 12-bit decode
- ✅ **Enrol / Verify / Identify** through `fprintd` and GNOME

Because the sensor is far too small to yield the ≥10 minutiae that libfprint's
default NBIS / Bozorth3 matcher needs, matching uses **SIGFM** — SIFT-based
feature matching via OpenCV — instead. Enrolment stores several SIFT templates
per finger and a probe is accepted on its best per-sample score.

> **Requires OpenCV** (`opencv5` or `opencv4`) at build and runtime. This makes
> the driver **not upstreamable as-is** — it is an out-of-tree, experimental
> fork. Use at your own risk; it may misbehave with your device.

## Installing

- **Arch Linux** — the `PKGBUILD` in the repo root (`makepkg -si`), or the
  `libfprint-goodix5xx-git` AUR package it mirrors.
- **Fedora / secureblue / Silverblue / Kinoite** — see
  [`packaging/fedora/`](packaging/fedora/README.md): an RPM spec plus a
  containerised build script, and a `Containerfile` that bakes the fork into a
  custom bootc image (the recommended route on image-based systems).

## Development & AI assistance ("vibe-coded")

Be upfront: the `goodix5xx` driver was written **almost entirely with an AI
coding agent** — Anthropic's Claude Code, running Claude Opus 4.8. The agent
produced the USB/framing layer, the OpenSSL PSK-TLS handshake, the image
decode/preprocessing, the `FpImageDevice` → `FpDevice` conversion, the SIGFM
integration, and most of the live on-device debugging (TLS silence traced to
USB write chunking, a cross-action stale-callback bug found via fprintd logs,
the finger-lift/calibration-pollution fix, and aligning the image pipeline with
the reference so genuine fingers stopped scoring ~0).

What was **human**: the device and all physical finger-presses / on-hardware
testing, the direction and decisions, and review of the generated code. The
protocol is a faithful port of the [goodix-fp-dump] Python reference, and the
SIFT matcher in `sigfm/` comes from the [goodix-fp-linux-dev] project, which
pioneered SIFT matching for these small Goodix sensors.

Treat it accordingly: it works, but it has had **no upstream libfprint review**.

## History

**LibFPrint** was originally developed as part of an
academic project at the **[University Of Manchester]**.

It aimed to hide the differences between consumer
fingerprint scanners and provide a single uniform
API to application developers.

## Goal

The ultimate goal of the **FPrint** project is to make
fingerprint scanners widely and easily usable under
common Linux environments.

## License

`Section 6` of the license states that for compiled works that use
this library, such works must include **LibFPrint** copyright notices
alongside the copyright notices for the other parts of the work.

**LibFPrint** includes code from **NIST's** **[NBIS]** software distribution.

We include **Bozorth3** from the **[US Export Controlled]**
distribution, which we have determined to be fine
being shipped in an open source project.

<br/>

<div align="right">

[![Badge License]][License]

</div>


<!----------------------------------------------------------------------------->

[Documentation]: https://fprint.freedesktop.org/libfprint-dev/
[Contributors]: https://gitlab.freedesktop.org/libfprint/libfprint/-/graphs/master
[Unsupported]: https://gitlab.freedesktop.org/libfprint/wiki/-/wikis/Unsupported-Devices
[Supported]: https://fprint.freedesktop.org/supported-devices.html
[Website]: https://fprint.freedesktop.org/

[Contribute]: ./HACKING.md
[License]: ./COPYING

[University Of Manchester]: https://www.manchester.ac.uk/
[US Export Controlled]: https://fprint.freedesktop.org/us-export-control.html
[NBIS]: http://fingerprint.nist.gov/NBIS/index.html

[goodix-fp-dump]: https://github.com/goodix-fp-linux-dev/goodix-fp-dump
[goodix-fp-linux-dev]: https://github.com/goodix-fp-linux-dev


<!---------------------------------[ Badges ]---------------------------------->

[Badge License]: https://img.shields.io/badge/License-LGPL2.1-015d93.svg?style=for-the-badge&labelColor=blue


<!---------------------------------[ Buttons ]--------------------------------->

[Button Documentation]: https://img.shields.io/badge/Documentation-04ACE6?style=for-the-badge&logoColor=white&logo=BookStack
[Button Contributors]: https://img.shields.io/badge/Contributors-FF4F8B?style=for-the-badge&logoColor=white&logo=ActiGraph
[Button Unsupported]: https://img.shields.io/badge/Unsupported_Devices-EF2D5E?style=for-the-badge&logoColor=white&logo=AdBlock
[Button Contribute]: https://img.shields.io/badge/Contribute-66459B?style=for-the-badge&logoColor=white&logo=Git
[Button Supported]: https://img.shields.io/badge/Supported_Devices-428813?style=for-the-badge&logoColor=white&logo=AdGuard
[Button Website]: https://img.shields.io/badge/Homepage-3B80AE?style=for-the-badge&logoColor=white&logo=freedesktopDotOrg
