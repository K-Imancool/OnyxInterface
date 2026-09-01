# Дублирование системы ROC-RK3566 через microSD

Как снять рабочую систему (Armbian + Qt + ONYX) с одной платы и развернуть на другие такие же ROC-RK3566.

Нулевая установка с образа — в [roc-rk3566-zero-install.md](roc-rk3566-zero-install.md). Этот документ — только про **клон уже настроенной** платы.

---

## Зачем так, а не `armbian-install` на карту

На рабочей плате два накопителя:

| Устройство | Что это | Роль |
|---|---|---|
| `/dev/mmcblk1` | eMMC (есть `mmcblk1boot0` / `boot1`) | U-Boot и `/boot` |
| `/dev/nvme0n1p1` | NVMe SSD | корень `/` |
| `/dev/mmcblk0` | microSD, если вставлена | появляется только с картой |

RK3566 **не грузится с NVMe напрямую**. BootROM идёт: SPI → eMMC → SD. Корень на SSD работает только потому, что загрузчик уже лежит в eMMC (и/или SPI) **этой** платы.

`armbian-install` умеет копировать систему *с текущего носителя внутрь* (на eMMC / NVMe / USB). Когда корень уже на NVMe, пункта «поставить на SD» в меню нет. Пункт «Boot from — system on» с пустым текстом **не нажимать**: скрипт путает `mmcblk0` и `mmcblk1` и может стереть eMMC рабочей платы.

Поэтому клон собирается вручную: загрузочная SD → ею поднимаем новую плату → уже на ней `armbian-install` в eMMC + NVMe (штатное направление).

Карта должна вместить **занятое** место на корне (`df -h /`), не полный размер SSD. 11 ГБ на 32 ГБ карте — нормально. Сырой `dd` всего NVMe на меньшую карту не подойдёт.

---

## Что должно получиться

1. На исходной плате один раз готовится SD с полной копией системы.
2. Новая плата грузится с этой карты (без установленного eMMC/NVMe — тоже).
3. На новой плате система переносится на её eMMC + NVMe, карта вынимается.
4. Та же карта идёт на следующую плату. Хостнейм, IP, `machine-id` и SSH-ключи на каждой плате свои.

Платы — те же ROC-RK3566 и тот же DTB (`rk3566-firefly-roc-pc.dtb`). Quartz64 / другая ревизия этим клоном не поднимать.

---

## Опасности

- **`mmcblk1` — это eMMC, не карта.** Карта — `mmcblk0`. Любой `dd`/`parted` не на тот диск уничтожит загрузчик рабочей платы.
- Пока карта вставлена, BootROM часто **предпочитает SD**. Недописанная карта = чёрный экран, eMMC даже не пробуется. Готовить карту только **после** загрузки с NVMe, карту вставлять уже в работающую систему. Если плата не стартует с картой — вынуть карту, загрузиться как обычно.
- В `(initramfs)` USB-клавиатура обычно мертва (в initramfs нет USB). HDMI-оболочка бесполезна. Питание снять, править карту с рабочей системы. UART при необходимости: `ttyS2`, **1500000** 8N1.
- Пока на клоне тот же статический IP, что на исходной плате, **не включайте обе в одну сеть**.

---

## A. Исходная плата: подготовка initramfs (один раз)

Initramfs собран под корень на NVMe (`MODULES=dep`). Драйверов SD в нём нет: U-Boot ядро с карты читает, ядро карту уже не видит → долгий Plymouth → чёрный экран `(initramfs)`.

Модули MMC в initramfs рабочей системы безвредны (с NVMe по-прежнему грузится). Сделать **до** rsync, чтобы на карту ушёл уже новый `uInitrd`.

```bash
# target, корень на NVMe, карты в слоте ещё нет (или есть — неважно)
printf '%s\n' \
  mmc_block mmc_core \
  sdhci sdhci_pltfm cqhci sdhci_of_dwcmshc \
  dw_mmc dw_mmc_pltfm dw_mmc_rockchip \
  | sudo tee -a /etc/initramfs-tools/modules

sudo update-initramfs -u -k $(uname -r)
sudo ln -sfn uInitrd-$(uname -r) /boot/uInitrd

# проверка: это сырой initrd, не uInitrd (у uInitrd заголовок U-Boot, lsinitramfs ругается)
lsinitramfs /boot/initrd.img-$(uname -r) | grep -Ei 'mmc_block|sdhci_of_dwcmshc|dw_mmc_rockchip'
```

Должны быть строки с этими модулями. Если `tee -a` гоняли повторно — дубли в файле модулей не страшны; важнее вывод `lsinitramfs`.

---

## B. Исходная плата: сборка загрузочной SD

Загрузиться **без** карты (NVMe + eMMC). Вставить карту. Убедиться:

```bash
lsblk
cat /sys/block/mmcblk0/device/type    # должно быть SD
cat /sys/block/mmcblk1/device/type    # должно быть MMC
ls /dev/mmcblk1boot0                  # есть только у eMMC
```

Дальше только `/dev/mmcblk0`.

### B1. Разметка и U-Boot

`write_uboot_platform` с этой системы на SD недостаточен: BootROM берёт карту и зависает. Рабочий загрузчик — **копия с eMMC**, сектора 64…32767 (16 МиБ до раздела не затираются).

Порядок обязателен: сначала `parted` (затирает начало диска), потом `dd` загрузчика.

```bash
sudo umount /dev/mmcblk0p* 2>/dev/null || true

sudo parted /dev/mmcblk0 --script mklabel gpt
sudo parted /dev/mmcblk0 --script mkpart primary ext4 16MiB 100%
sudo partprobe /dev/mmcblk0
sudo mkfs.ext4 -F /dev/mmcblk0p1

# U-Boot с живого eMMC → на карту, таблица разделов и fs не трогаются
sudo dd if=/dev/mmcblk1 of=/dev/mmcblk0 bs=512 skip=64 seek=64 count=32704 conv=notrunc,fsync
```

### B2. Копирование системы

```bash
sudo mkdir -p /mnt/sd
sudo mount /dev/mmcblk0p1 /mnt/sd

sudo rsync -aAXH --numeric-ids --info=progress2 \
  --exclude='/dev/*' --exclude='/proc/*' --exclude='/sys/*' \
  --exclude='/tmp/*' --exclude='/run/*' --exclude='/mnt/*' \
  --exclude='/media/*' --exclude='/var/log/*' --exclude='/lost+found' \
  / /mnt/sd/
```

`/boot` копируется (это содержимое eMMC, примонтированное в `/boot`). `/media/mmcboot` отброшен специально — на карте boot и root в одной ФС.

11 ГБ на класс 10 — десятки минут.

### B3. UUID карты — и только он

На исходной плате в `fstab` / `extlinux` прописаны UUID **NVMe** и **eMMC**. На карте корень — сама карта. Иначе initramfs 30+ секунд ищет чужой диск и падает в `(initramfs)`.

```bash
SD_UUID=$(sudo blkid -s UUID -o value /dev/mmcblk0p1)
echo "$SD_UUID"
```

`/mnt/sd/etc/fstab` **целиком** заменить на:

```
tmpfs                                           /tmp            tmpfs   defaults,nosuid,nodev  0 0
UUID=<SD_UUID>                                  /               ext4    defaults,noatime,commit=120,errors=remount-ro  0 1
```

Строки с `/media/mmcboot`, bind `/boot` и UUID NVMe (`35796156-…` и подобные) не оставлять.

В `/mnt/sd/boot/extlinux/extlinux.conf` в `append` / `APPEND`:

- `root=UUID=<SD_UUID>`
- сразу после него — `rootwait` (карта может появиться позже NVMe)
- остальная строка как была: `console=ttyS2,1500000`, `video=HDMI-A-1:1280x800M@54.36D`, plymouth, `initcall_blacklist=rk808_rtc_driver_init` и т.д.

Если есть `/mnt/sd/boot/armbianEnv.txt` — тот же UUID в `rootdev=`.

Пути в extlinux должны быть относительно корня карты (`LINUX /boot/Image`, `INITRD /boot/uInitrd`, DTB `rk3566-firefly-roc-pc.dtb`). После rsync с этой системы так и есть — не ужимать в `/Image`.

```bash
# сверка до umount
echo "карта:  $SD_UUID"
grep -E 'root=|append|APPEND|linux|LINUX|initrd|INITRD|FDT|fdtdir' /mnt/sd/boot/extlinux/extlinux.conf
cat /mnt/sd/etc/fstab
ls -l /mnt/sd/boot/Image /mnt/sd/boot/uInitrd /mnt/sd/boot/extlinux/extlinux.conf
ls /mnt/sd/boot/dtb/rockchip/rk3566-firefly-roc-pc.dtb

sudo umount /mnt/sd
sync
```

Выключить, вынуть карту.

---

## C. Проверка карты (желательно на исходной плате)

Вставить карту, включить. Должны быть splash Plymouth и UI, как на SSD.

Если снова `(initramfs)` — вынуть карту, загрузиться с NVMe, сверить `blkid /dev/mmcblk0p1` с `root=` и что на карте лежит **новый** `uInitrd` после шага A.

Если чёрный экран сразу, без Plymouth — снова загрузчик: повторить `dd` U-Boot с eMMC (раздел не трогать).

---

## D. Новая плата: перенос на её eMMC + NVMe

Карта в **новую** плату, питание. Система должна подняться с SD.

Сеть пока не подключать (тот же IP, что у исходной), либо сразу сменить IP.

На новой плате корень уже на SD, `armbian-install` работает в штатную сторону:

```bash
lsblk
sudo armbian-install
```

Нужный пункт: **Boot from eMMC — system on SATA, USB or NVMe** (цель NVMe, ext4). Это тот же сценарий, что раздел F в гайде нулевой установки.

Не выбирать «весь система на eMMC», если корень должен остаться на SSD.

Дождаться конца, выключить, **вынуть SD**, включить. Загрузка с eMMC этой платы, корень на её NVMe.

Та же карта — на следующую плату, с шага D. Карту заново не собирать, пока исходную систему не обновляли.

---

## E. Уникальность каждой платы

До включения в одну сеть с исходной (и с другими клонами):

```bash
# свой номер и адрес
sudo hostnamectl set-hostname onyx-N
sudo nmcli con mod "Wired connection 1" \
  ipv4.addresses 10.7.4.XX/24 \
  ipv4.gateway 10.7.4.254 \
  ipv4.dns "8.8.8.8 1.1.1.1" \
  ipv4.method manual

sudo rm -f /etc/machine-id /var/lib/dbus/machine-id
sudo systemd-machine-id-setup
sudo ssh-keygen -A
sudo reboot
```

Имя соединения NM сверить: `nmcli con show`.

С хоста: новый IP в Qt Creator (Devices) и при необходимости заново `ssh-copy-id` — host keys другие.

---

## Типичные сбои

| Симптом | Почему | Что делать |
|---|---|---|
| С картой плата полностью мертва, без карты — жива | BootROM взял SD, U-Boot на карте битый/чужой | Вынуть карту. Повторить `dd` загрузчика с eMMC |
| Долгий Plymouth → `(initramfs)`, клавиатура молчит | Нет MMC в initramfs или чужой `root=UUID` | Не сидеть в оболочке. Шаг A + сверка UUID + `rootwait` |
| `lsinitramfs /boot/uInitrd` → `cpio: premature end of archive` | Это образ U-Boot, не cpio | Смотреть `lsinitramfs /boot/initrd.img-$(uname -r)` |
| `armbian-install` на исходной плате без пункта «на SD» | Так и задумано | Не жать пункт 2. Собирать карту разделом B |
| После клона две платы с одним IP | Скопирован NM | Раздел E **до** общего коммутатора |
| Новая плата другая (Quartz64 и т.п.) | Другой DTB/U-Boot | Этот клон не подходит, см. нулевую установку |

---

## Скрипт: собрать SD на исходной плате

Запускать **на ROC с корнем на NVMe**, карту вставить **после** загрузки. Скрипт сотрёт только `/dev/mmcblk0`. Перед стартом ещё раз `lsblk`.

Вставить целиком:

```bash
#!/bin/bash
set -euo pipefail

SD=/dev/mmcblk0
EMMC=/dev/mmcblk1
MNT=/mnt/sd
MMC_MODULES=(
  mmc_block mmc_core
  sdhci sdhci_pltfm cqhci sdhci_of_dwcmshc
  dw_mmc dw_mmc_pltfm dw_mmc_rockchip
)

die() { echo "ERROR: $*" >&2; exit 1; }

[[ $EUID -eq 0 ]] || die "запустите через sudo"
[[ -b $SD ]]     || die "$SD нет — вставьте карту после загрузки с NVMe"
[[ -b $EMMC ]]   || die "$EMMC нет — это не та плата"
[[ -b ${EMMC}boot0 ]] || die "$EMMC не похож на eMMC (нет boot0) — стоп"
[[ -e ${SD}boot0 ]] && die "$SD похож на eMMC, это не карта — стоп"

sd_type=$(cat /sys/block/${SD#/dev/}/device/type 2>/dev/null || echo "")
emmc_type=$(cat /sys/block/${EMMC#/dev/}/device/type 2>/dev/null || echo "")
[[ "$sd_type" == SD ]]   || die "$SD type='$sd_type', ожидался SD"
[[ "$emmc_type" == MMC ]] || die "$EMMC type='$emmc_type', ожидался MMC"

root_src=$(findmnt -no SOURCE /)
echo "корень сейчас: $root_src"
echo "карта:  $SD ($sd_type)"
echo "eMMC:   $EMMC ($emmc_type)"
lsblk "$SD" "$EMMC"
echo
echo "Будет ПОЛНОСТЬЮ стёрта $SD. eMMC и NVMe не трогаем."
read -r -p "Введите YES чтобы продолжить: " ans
[[ "$ans" == YES ]] || die "отмена"

# --- A. MMC в initramfs (идемпотентно) ---
mkdir -p /etc/initramfs-tools
touch /etc/initramfs-tools/modules
for m in "${MMC_MODULES[@]}"; do
  grep -qxF "$m" /etc/initramfs-tools/modules || echo "$m" >> /etc/initramfs-tools/modules
done
update-initramfs -u -k "$(uname -r)"
ln -sfn "uInitrd-$(uname -r)" /boot/uInitrd
lsinitramfs "/boot/initrd.img-$(uname -r)" | grep -Eqi 'mmc_block' \
  || die "mmc_block не попал в initramfs"

# --- B1. разметка + U-Boot с eMMC ---
umount ${SD}p* 2>/dev/null || true
parted "$SD" --script mklabel gpt
parted "$SD" --script mkpart primary ext4 16MiB 100%
partprobe "$SD"
sleep 1
[[ -b ${SD}p1 ]] || die "нет ${SD}p1 после разметки"
mkfs.ext4 -F ${SD}p1
dd if="$EMMC" of="$SD" bs=512 skip=64 seek=64 count=32704 conv=notrunc,fsync status=progress

# --- B2. rsync ---
mkdir -p "$MNT"
mount ${SD}p1 "$MNT"
rsync -aAXH --numeric-ids --info=progress2 \
  --exclude='/dev/*' --exclude='/proc/*' --exclude='/sys/*' \
  --exclude='/tmp/*' --exclude='/run/*' --exclude='/mnt/*' \
  --exclude='/media/*' --exclude='/var/log/*' --exclude='/lost+found' \
  / "$MNT/"

# --- B3. UUID ---
SD_UUID=$(blkid -s UUID -o value ${SD}p1)
[[ -n "$SD_UUID" ]] || die "не удалось прочитать UUID ${SD}p1"

cat > "$MNT/etc/fstab" << EOF
tmpfs                                           /tmp            tmpfs   defaults,nosuid,nodev  0 0
UUID=${SD_UUID}       /               ext4    defaults,noatime,commit=120,errors=remount-ro  0 1
EOF

EXTLINUX="$MNT/boot/extlinux/extlinux.conf"
[[ -f "$EXTLINUX" ]] || die "нет $EXTLINUX"
sed -i -E "s/root=UUID=[0-9a-fA-F-]+/root=UUID=${SD_UUID}/g" "$EXTLINUX"
if ! grep -Eq 'root=UUID=[^ ]+ rootwait' "$EXTLINUX"; then
  sed -i -E "s/(root=UUID=${SD_UUID})/\1 rootwait/" "$EXTLINUX"
fi
if [[ -f "$MNT/boot/armbianEnv.txt" ]] && grep -q rootdev "$MNT/boot/armbianEnv.txt"; then
  sed -i -E "s|^rootdev=.*|rootdev=UUID=${SD_UUID}|" "$MNT/boot/armbianEnv.txt"
fi

echo
echo "=== проверка ==="
echo "UUID карты: $SD_UUID"
grep -E 'root=|append|APPEND' "$EXTLINUX"
echo "--- fstab ---"
cat "$MNT/etc/fstab"
ls -l "$MNT/boot/Image" "$MNT/boot/uInitrd" "$EXTLINUX"
ls "$MNT/boot/dtb/rockchip/rk3566-firefly-roc-pc.dtb"

umount "$MNT"
sync
echo
echo "Готово. Выключите плату, выньте карту, грузите ею целевой ROC."
echo "На целевой: sudo armbian-install → Boot from eMMC — system on NVMe."
echo "Затем смените hostname/IP/machine-id/SSH keys (раздел E)."
```
