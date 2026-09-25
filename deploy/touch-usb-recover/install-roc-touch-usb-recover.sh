#!/usr/bin/env bash
# Install privileged USB-touch port power-cycle helper for UserInterface.
# Monitoring and logging live in the app (TouchUsbMonitor).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_SH="${SCRIPT_DIR}/roc-touch-usb-recover.sh"
SRC_SUDOERS="${SCRIPT_DIR}/99-roc-touch-usb-recover-sudoers"

DST_SH="/usr/local/sbin/roc-touch-usb-recover"
DST_SUDOERS="/etc/sudoers.d/99-roc-touch-usb-recover"

if [[ "${EUID:-$(id -u)}" -ne 0 ]]; then
  echo "Запустите: sudo $0" >&2
  exit 1
fi

for f in "$SRC_SH" "$SRC_SUDOERS"; do
  if [[ ! -f "$f" ]]; then
    echo "Нет файла: $f" >&2
    exit 1
  fi
done

install -d /usr/local/sbin
install -m 0755 "$SRC_SH" "$DST_SH"
install -m 0440 "$SRC_SUDOERS" "$DST_SUDOERS"
visudo -cf "$DST_SUDOERS"

# Убрать прежний systemd-watchdog, если ставили раньше.
systemctl disable --now roc-touch-usb-recover.timer 2>/dev/null || true
systemctl disable roc-touch-usb-recover.service 2>/dev/null || true
rm -f /etc/systemd/system/roc-touch-usb-recover.service \
      /etc/systemd/system/roc-touch-usb-recover.timer \
      /etc/tmpfiles.d/onyx-run.conf
systemctl daemon-reload 2>/dev/null || true

echo
echo "Установлено:"
echo "  $DST_SH"
echo "  $DST_SUDOERS"
echo
echo "Проверка от kikorik:"
echo "  sudo -n $DST_SH"
echo "  # в UI: touchUsbMonitor.state / journalctl -t roc-touch-usb-recover"
echo
echo "Удаление:"
echo "  sudo rm -f $DST_SH $DST_SUDOERS"
