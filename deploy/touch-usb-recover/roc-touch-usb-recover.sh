#!/usr/bin/env bash
# Privileged helper: restore ILITEK touch (222a:0001) for UserInterface.
# Called as: sudo -n /usr/local/sbin/roc-touch-usb-recover
#
# Здоровье = есть "WINSTARTP Mouse" (ABS/BTN для Qt). Одного "WINSTARTP"
# (hid-multitouch) недостаточно — на ROC события тача идут с Mouse-интерфейса.
#
# Exit 0 — WINSTARTP Mouse есть.
# Exit 1 — не удалось восстановить.
set -euo pipefail

VID="${TOUCH_USB_VID:-222a}"
PID="${TOUCH_USB_PID:-0001}"
OFF_SEC="${TOUCH_USB_PORT_OFF_SEC:-2}"
ON_SEC="${TOUCH_USB_PORT_ON_SEC:-2}"
SETTLE_SEC="${TOUCH_USB_SETTLE_SEC:-8}"
# Запасные порты ROC (шина тача бывает usb5 или usb6 в зависимости от xhci).
FALLBACK_DISABLE_PATHS="${TOUCH_USB_FALLBACK_DISABLE_PATHS:-/sys/bus/usb/devices/usb5/5-0:1.0/usb5-port1/disable /sys/bus/usb/devices/usb6/6-0:1.0/usb6-port1/disable}"

log() {
  logger -t roc-touch-usb-recover -- "$@" || true
  printf '%s\n' "$*"
}

# Нужен именно Mouse-узел — как в рабочем evtest.
input_ok() {
  awk '
    BEGIN { RS=""; FS="\n"; found=0 }
    {
      name=""; handlers=""
      for (i=1; i<=NF; i++) {
        if ($i ~ /^N: Name=/) name=$i
        if ($i ~ /^H: Handlers=/) handlers=$i
      }
      if (name ~ /WINSTARTP Mouse/) { found=1; exit }
    }
    END { exit found ? 0 : 1 }
  ' /proc/bus/input/devices 2>/dev/null
}

find_touch_sysfs() {
  local d
  for d in /sys/bus/usb/devices/*; do
    [[ -f "${d}/idVendor" && -f "${d}/idProduct" ]] || continue
    [[ "$(tr '[:upper:]' '[:lower:]' <"${d}/idVendor")" == "${VID}" ]] || continue
    [[ "$(tr '[:upper:]' '[:lower:]' <"${d}/idProduct")" == "${PID}" ]] || continue
    [[ "$(basename "${d}")" == *:* ]] && continue
    printf '%s\n' "${d}"
    return 0
  done
  return 1
}

resolve_disable_path() {
  local dev port p
  if dev="$(find_touch_sysfs)"; then
    if [[ -L "${dev}/port" ]]; then
      port="$(readlink -f "${dev}/port")"
      if [[ -e "${port}/disable" ]]; then
        printf '%s\n' "${port}/disable"
        return 0
      fi
    fi
  fi
  # shellcheck disable=SC2086
  for p in ${FALLBACK_DISABLE_PATHS}; do
    if [[ -e "${p}" ]]; then
      printf '%s\n' "${p}"
      return 0
    fi
  done
  return 1
}

try_bind_mouse_iface() {
  local dev="$1"
  local iface="${dev}:1.1"
  local name
  name="$(basename "${iface}")"
  if [[ ! -d "${iface}" ]]; then
    return 1
  fi
  if [[ -e "${iface}/driver" ]]; then
    log "iface ${name} already has driver"
    return 0
  fi
  log "bind usbhid to ${name}"
  if printf '%s\n' "${name}" > /sys/bus/usb/drivers/usbhid/bind 2>/dev/null; then
    return 0
  fi
  log "bind ${name} failed"
  return 1
}

power_cycle() {
  local disable_path="$1"
  log "power-cycle via ${disable_path}"
  printf '1\n' >"${disable_path}"
  sleep "${OFF_SEC}"
  printf '0\n' >"${disable_path}"
  sleep "${ON_SEC}"
}

wait_for_mouse() {
  local i
  local steps=$((SETTLE_SEC * 2))
  for ((i = 0; i < steps; i++)); do
    if input_ok; then
      return 0
    fi
    sleep 0.5
  done
  return 1
}

if [[ "${EUID:-$(id -u)}" -ne 0 ]]; then
  echo "Run as root: sudo -n $0" >&2
  exit 1
fi

if input_ok; then
  log "ok: WINSTARTP Mouse already present"
  exit 0
fi

dev=""
dev="$(find_touch_sysfs || true)"
if [[ -n "${dev}" ]]; then
  log "device at ${dev}, WINSTARTP Mouse missing"
  # Сначала мягко: привязать второй HID-интерфейс (Mouse).
  try_bind_mouse_iface "${dev}" || true
  if wait_for_mouse; then
    log "recovered: WINSTARTP Mouse after usbhid bind"
    exit 0
  fi
fi

disable_path=""
if ! disable_path="$(resolve_disable_path)"; then
  log "error: no USB port disable node for ${VID}:${PID}"
  exit 1
fi

try=1
while (( try <= 2 )); do
  power_cycle "${disable_path}"
  # После перечисления путь устройства может смениться (usb5 <-> usb6).
  dev="$(find_touch_sysfs || true)"
  if [[ -n "${dev}" ]]; then
    try_bind_mouse_iface "${dev}" || true
  fi
  if wait_for_mouse; then
    log "recovered: WINSTARTP Mouse after try ${try}"
    exit 0
  fi
  log "try ${try}: WINSTARTP Mouse still missing"
  # На следующем круге снова резолвим порт — устройство могло сменить шину.
  disable_path="$(resolve_disable_path || true)"
  if [[ -z "${disable_path}" ]]; then
    log "error: port disable path disappeared"
    exit 1
  fi
  try=$((try + 1))
done

log "failed: no WINSTARTP Mouse after recover"
exit 1
