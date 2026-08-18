#!/usr/bin/env bash
# Build libfprint (goodix5xx fork) RPMs inside a Fedora container.
#
# Meant for immutable Fedora hosts (secureblue / Silverblue / Bluefin), where
# you cannot install build dependencies on the host itself.
#
# Usage:
#   packaging/fedora/build-rpm.sh [fedora-release]
#
# Output: packaging/fedora/out/*.rpm  (binary) and out/*.src.rpm
set -euo pipefail

here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo=$(cd -- "$here/../.." && pwd)
outdir="$here/out"

# Match the Fedora release of the host image so the RPM links against the same
# glib/opencv/openssl sonames the deployment actually has.
detect_release() {
    # shellcheck disable=SC1091
    . /usr/lib/os-release 2>/dev/null || return
    [ "${ID:-}" = fedora ] && echo "${VERSION_ID:-}"
}
fedora_release=${1:-$(detect_release || true)}
fedora_release=${fedora_release:-42}
image="registry.fedoraproject.org/fedora:${fedora_release}"

version=$(grep -m1 -oP "version:\s*'\K[0-9.]+" "$repo/meson.build")
name=libfprint

# ENGINE may carry extra flags, e.g. ENGINE="podman --root=/var/tmp/podman"
# if your home partition is short on space.
read -r -a engine <<<"${ENGINE:-podman}"
command -v "${engine[0]}" >/dev/null || { echo "need podman (or set ENGINE=docker)" >&2; exit 1; }

echo ">> building ${name}-${version} for fedora:${fedora_release}"

rm -rf "$outdir"
mkdir -p "$outdir/src"

# Snapshot the working tree (tracked files, including uncommitted staged state
# is *not* included -- commit first if you want your local changes in).
git -C "$repo" archive --format=tar.gz \
    --prefix="${name}-${version}/" \
    -o "$outdir/src/${name}-${version}.tar.gz" HEAD

cp "$here/${name}.spec" "$outdir/src/"

"${engine[@]}" run --rm \
    -v "$outdir:/out:z" \
    "$image" \
    bash -euxo pipefail -c '
        dnf -y install rpm-build rpmdevtools dnf-plugins-core
        rpmdev-setuptree
        cp /out/src/*.tar.gz ~/rpmbuild/SOURCES/
        cp /out/src/*.spec  ~/rpmbuild/SPECS/
        dnf -y builddep ~/rpmbuild/SPECS/libfprint.spec
        rpmbuild -ba ~/rpmbuild/SPECS/libfprint.spec
        cp ~/rpmbuild/RPMS/*/*.rpm ~/rpmbuild/SRPMS/*.rpm /out/
        chmod 0644 /out/*.rpm
    '

rm -rf "$outdir/src"
echo
echo ">> RPMs in $outdir:"
ls -1 "$outdir"
echo
echo "Install on an rpm-ostree host with:"
echo "  sudo rpm-ostree override replace $outdir/${name}-${version}-*.rpm"
echo "  systemctl reboot"
