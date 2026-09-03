#!/usr/bin/env bash
# Доступ к /dev/gpiochip* для UserInterface (user-сервис, не root).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RULE_SRC="${SCRIPT_DIR}/99-qtpr-gpio.rules"
RULE_DST="/etc/udev/rules.d/99-qtpr-gpio.rules"

if [[ "${EUID:-$(id -u)}" -ne 0 ]]; then
  echo "Запустите: sudo $0" >&2
  exit 1
fi

if [[ ! -f "$RULE_SRC" ]]; then
  echo "Нет файла правил: $RULE_SRC" >&2
  exit 1
fi

install -m 0644 "$RULE_SRC" "$RULE_DST"
udevadm control --reload-rules
udevadm trigger --action=change --subsystem-match=gpio || true
shopt -s nullglob
for node in /dev/gpiochip*; do
  chmod 666 "$node"
done

echo "Установлено $RULE_DST"
echo "Перезапустите UI:"
echo "  systemctl --user restart Demo1-user.service"
