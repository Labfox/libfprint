# Fedora / secureblue packaging

RPM packaging for this libfprint fork (Goodix `27c6:55a4`, `goodix5xx` driver),
targeting Fedora and image-based Fedora Atomic variants — **secureblue**,
Silverblue, Kinoite, Bluefin.

The RPM is named `libfprint` on purpose: on an rpm-ostree/bootc system the
replacement package must carry the same name as the one in the base image, so
that `rpm-ostree override replace` can swap it.

| File | What it is |
| --- | --- |
| `libfprint.spec` | RPM spec, builds `libfprint` + `libfprint-devel` |
| `build-rpm.sh` | Builds the RPMs in a throwaway Fedora container |
| `Containerfile` | Bakes the RPMs into a custom secureblue image (recommended) |

## Which route?

- **Custom image** (`Containerfile`) — the fork lives in your image, survives
  every `bootc upgrade`, nothing to re-apply. This is the route secureblue
  itself recommends for persistent changes.
- **Local override** (`build-rpm.sh` + `rpm-ostree override replace`) — quicker
  to try, but it is a pinned local override you carry across updates and must
  refresh whenever Fedora bumps `libfprint`.

## Route A — custom image

```bash
# find the image you're on
rpm-ostree status | grep -m1 -i image     # or: bootc status

podman build -f packaging/fedora/Containerfile \
  --build-arg BASE_IMAGE=ghcr.io/secureblue/silverblue-main-hardened:latest \
  --build-arg FEDORA_RELEASE=42 \
  -t localhost/secureblue-goodix5xx .

sudo bootc switch --transport containers-storage localhost/secureblue-goodix5xx
systemctl reboot
```

Set `FEDORA_RELEASE` to the same Fedora version as the base image, so the
build links against the glib/OpenSSL/OpenCV sonames the deployment ships.

## Route B — local override

```bash
packaging/fedora/build-rpm.sh            # detects the host's Fedora release
sudo rpm-ostree override replace packaging/fedora/out/libfprint-1.94.9-*.rpm
sudo rpm-ostree install fprintd fprintd-pam   # if not already in your image
systemctl reboot
```

`build-rpm.sh` archives **`HEAD`**, not your working tree — commit local changes
first. `ENGINE=docker` works if you prefer Docker, and `ENGINE` may carry flags
(`ENGINE="podman --root=/var/tmp/podman"`) if your home partition is short on
space — the build container needs ~3 GB for the OpenCV/GTK build deps.

To undo:

```bash
sudo rpm-ostree override reset libfprint && systemctl reboot
```

On a traditional (non-atomic) Fedora, the same script works — just install with
`sudo dnf install packaging/fedora/out/libfprint-*.rpm`. Add `libfprint` to
`excludepkgs` in `/etc/dnf/dnf.conf` so a routine update doesn't quietly put
Fedora's build back.

## secureblue-specific notes

- **USBGuard.** secureblue enables USBGuard on several images. An internal
  fingerprint sensor that was present at policy-generation time is already
  allowed; if it was not, `fprintd` will see no device. Check with
  `sudo usbguard list-devices | grep -i 27c6` and allow it permanently:
  `sudo usbguard allow-device <id> -p`.
- **hardened_malloc.** The hardened images preload `libhardened_malloc`. The
  driver's OpenSSL/OpenCV paths are ordinary heap users, but if `fprintd`
  crashes only on this image, test by running it once without the preload
  (`sudo LD_PRELOAD= /usr/libexec/fprintd -t`) before blaming the driver.
- **Fingerprint auth is off by default.** secureblue does not wire fingerprint
  into PAM. After installing `fprintd-pam`, enable it deliberately
  (`sudo authselect enable-feature with-fingerprint`) — and be aware that
  biometrics-as-auth is a security trade-off secureblue intentionally avoids.
- **Rootless podman** is available on secureblue; if user namespaces are
  restricted in your configuration, run the build inside a `toolbox`/`distrobox`
  Fedora container instead — the same `build-rpm.sh` steps apply.

## Version

The fork is rebased on upstream **libfprint 1.94.9** — the same version Fedora
42 ships — so replacing the distribution package costs you nothing but gains
the `goodix5xx` driver. The RPM release carries a `.goodix5xx` suffix
(`1.94.9-1.fc42.goodix5xx`) so it sorts above Fedora's build and is easy to
spot in `rpm -q`.

## Runtime dependencies

All pulled automatically by RPM's soname dependency generator — no manual
`Requires:` needed. On Fedora 42 that resolves to `glib2`, `libgusb`,
`libgudev`, `pixman`, `openssl-libs` and the fine-grained OpenCV
subpackages (`opencv-core`, `opencv-imgproc`, `opencv-features2d`,
`opencv-flann`, `opencv-calib3d`, `opencv-stitching`) — a few tens of MB, not
the full `opencv` stack.

Note there are **no udev rules for the sensor**: 27c6:55a4 is a plain USB
device, and libfprint only generates udev rules for SPI devices. `fprintd` runs
as root and reaches it through libgusb, so nothing extra is required.

## Verified

`build-rpm.sh` was run end to end against `registry.fedoraproject.org/fedora:42`
on 2026-08-16, on the tree rebased onto upstream v1.94.9: the spec builds
`libfprint`, `libfprint-devel` (plus debuginfo) cleanly, the RPM replaces
Fedora's `libfprint-1.94.9-1.fc42` with `1.94.9-1.fc42.goodix5xx`, every soname
resolves, and `fprintd` installs on top of it. The `rpm-ostree` and
secureblue steps above are documented but were not exercised on this machine
(Arch host).
