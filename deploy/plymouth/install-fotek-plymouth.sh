#!/usr/bin/env bash
# Install FOTEK Plymouth theme and rebuild initramfs for the running kernel.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
THEME_SRC="${SCRIPT_DIR}/fotek-theme"
THEME_DST="/usr/share/plymouth/themes/fotek-theme"
PLYMOUTH_CONF="/etc/plymouth/plymouthd.conf"
KERNEL="$(uname -r)"

if [[ "${EUID:-$(id -u)}" -ne 0 ]]; then
  echo "Run as root: sudo $0 [path/to/start.png]" >&2
  exit 1
fi

START_SRC="${1:-}"
if [[ -z "$START_SRC" ]]; then
  for candidate in \
      "${HOME}/FOTEK/Images/start/start.png" \
      "/home/kikorik/FOTEK/Images/start/start.png"; do
    if [[ -f "$candidate" ]]; then
      START_SRC="$candidate"
      break
    fi
  done
fi

if [[ -z "$START_SRC" || ! -f "$START_SRC" ]]; then
  echo "Fullscreen splash not found. Pass path explicitly:" >&2
  echo "  sudo $0 /home/kikorik/FOTEK/Images/start/start.png" >&2
  exit 1
fi

if ! command -v update-initramfs >/dev/null 2>&1; then
  echo "update-initramfs not found" >&2
  exit 1
fi

install -d "$THEME_DST"
install -m 0644 "${THEME_SRC}/fotek-theme.plymouth" "${THEME_SRC}/fotek-theme.script" "$THEME_DST/"

# Prefer exact 1280x800 for the panel; fall back to copy if convert is missing.
if command -v convert >/dev/null 2>&1; then
  convert "$START_SRC" -resize '1280x800!' -strip PNG32:"${THEME_DST}/start.png"
  convert -size 18x18 xc:none -fill '#264093' -draw 'circle 9,9 9,2' \
    PNG32:"${THEME_DST}/progress-dot.png"
else
  echo "ImageMagick (convert) not found; copying start.png as-is." >&2
  install -m 0644 "$START_SRC" "${THEME_DST}/start.png"
  if [[ ! -f "${THEME_DST}/progress-dot.png" ]]; then
    echo "Install imagemagick for progress-dot.png or place it in ${THEME_DST}/" >&2
    exit 1
  fi
fi

if [[ -f "$PLYMOUTH_CONF" ]]; then
  if grep -q '^Theme=' "$PLYMOUTH_CONF"; then
    sed -i 's/^Theme=.*/Theme=fotek-theme/' "$PLYMOUTH_CONF"
  else
    printf '\nTheme=fotek-theme\n' >>"$PLYMOUTH_CONF"
  fi
else
  install -d /etc/plymouth
  cat >"$PLYMOUTH_CONF" <<'EOF'
[Daemon]
Theme=fotek-theme
ThemeDir=/usr/share/plymouth/themes
DeviceTimeout=10
EOF
fi

if [[ -L /boot/uInitrd || -e /boot/uInitrd ]]; then
  ln -sfn "uInitrd-${KERNEL}" /boot/uInitrd
fi

update-initramfs -u -k "$KERNEL"

echo "Installed ${THEME_DST}"
echo "Splash: ${START_SRC} -> ${THEME_DST}/start.png"
echo "Active theme: fotek-theme (kernel ${KERNEL})"
echo "Reboot to verify: sudo reboot"
