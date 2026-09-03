#!/usr/bin/env bash
# Предсказуемое выключение ROC: без splash на poweroff, polkit уже запущен,
# сеть/cron не держат shutdown дольше 500 мс.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STOP_DROPIN="${SCRIPT_DIR}/onyx-stop-500ms.conf"
MANAGER_CONF="${SCRIPT_DIR}/onyx-shutdown.conf"

if [[ "${EUID:-$(id -u)}" -ne 0 ]]; then
  echo "Запустите: sudo $0" >&2
  exit 1
fi

if [[ ! -f "$STOP_DROPIN" || ! -f "$MANAGER_CONF" ]]; then
  echo "Нет файлов drop-in рядом со скриптом: $SCRIPT_DIR" >&2
  exit 1
fi

install -d /etc/systemd/system.conf.d
install -m 0644 "$MANAGER_CONF" /etc/systemd/system.conf.d/onyx-shutdown.conf

install_stop_dropin() {
  local unit="$1"
  if ! systemctl list-unit-files "${unit}" >/dev/null 2>&1; then
    echo "нет юнита ${unit}, drop-in пропущен"
    return 0
  fi
  local dir="/etc/systemd/system/${unit}.d"
  install -d "$dir"
  install -m 0644 "$STOP_DROPIN" "${dir}/onyx-stop.conf"
  echo "TimeoutStopSec=500ms: ${unit}"
}

install_stop_dropin NetworkManager.service
install_stop_dropin wpa_supplicant.service
install_stop_dropin networking.service
install_stop_dropin cron.service

mask_if_present() {
  local unit="$1"
  if systemctl list-unit-files "${unit}" >/dev/null 2>&1; then
    systemctl mask "${unit}"
    echo "mask: ${unit}"
  fi
}

# Заставка на shutdown не нужна (HDMI к этому моменту уже мёртв).
# Тема fotek на загрузке не трогается.
mask_if_present plymouth-poweroff.service
mask_if_present plymouth-reboot.service
mask_if_present plymouth-halt.service
mask_if_present plymouth-kexec.service
mask_if_present bootsplash-show-on-shutdown.service

if systemctl list-unit-files polkit.service >/dev/null 2>&1; then
  systemctl unmask polkit.service >/dev/null 2>&1 || true
  systemctl enable --now polkit.service
  echo "polkit: enable --now"
fi

systemctl daemon-reload

echo
echo "Установлено. Штатный poweroff из UI не меняется."
echo "Проверка: sudo systemctl poweroff"
echo "Если система не с SD, лишний umount:"
echo "  systemctl mask media-mmcboot.mount"
