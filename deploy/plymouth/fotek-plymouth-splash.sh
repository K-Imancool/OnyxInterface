#!/usr/bin/env bash
# Replace only start.png in the installed Plymouth theme and refresh initramfs.
set -euo pipefail

SRC="${1:-}"
THEME_DIR="/usr/share/plymouth/themes/fotek-theme"
THEME_DST="${THEME_DIR}/start.png"
KERNEL="$(uname -r)"

if [[ "${EUID:-$(id -u)}" -ne 0 ]]; then
  echo "Run as root: sudo $0 /path/to/start.png" >&2
  exit 1
fi

if [[ -z "$SRC" || ! -f "$SRC" ]]; then
  echo "Usage: $0 /path/to/start.png" >&2
  exit 1
fi

if [[ ! -d "$THEME_DIR" ]]; then
  echo "Plymouth theme not installed: $THEME_DIR" >&2
  exit 1
fi

install -m 0644 "$SRC" "$THEME_DST"

# Boot always loads /boot/uInitrd — keep it on the running kernel.
ln -sfn "uInitrd-${KERNEL}" /boot/uInitrd

update-initramfs -u -k "$KERNEL"

# Confirm the new file is inside the initrd that this kernel will use.
if ! lsinitramfs "/boot/initrd.img-${KERNEL}" | grep -q 'themes/fotek-theme/start.png'; then
  echo "WARN: start.png not found in initrd.img-${KERNEL}" >&2
  exit 2
fi

echo "OK: ${THEME_DST} and initrd.img-${KERNEL} updated"
