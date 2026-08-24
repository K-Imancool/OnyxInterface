# Установка ONYX / UserInterface на нулевой ROC-RK3566

Гайд для подготовки microSD, первого запуска Armbian, Qt, автозапуска UI и Plymouth FOTEK.

## Обозначения

| Метка | Где | Пример |
|---|---|---|
| **host** | ПК сборки (Ubuntu 22.04 + KDE) | `kikorik@host:~$` |
| **target** | ROC / Station | `kikorik@roc:~$` |

- Дожидайтесь приглашения shell после каждой команды.
- На host в Dolphin: **ПКМ → Actions → Open Terminal** или **Shift+F4**.
- Перед `dd` / записью образа ещё раз проверьте диск: `lsblk` (не затрите SSD хоста).

Подставьте один раз и копируйте дальше:

```bash
# host
export TARGET_IP=10.7.4.51          # свой IP
export IMG="$HOME/Downloads/Station P2/bookworm-6.3.12-demo1-5G-station-p2.img.xz"
export SD_DISK=/dev/sdX               # из lsblk, БЕЗ раздела (не sdd1)
export DTB=rk3566-firefly-roc-pc.dtb  # или rk3568-... / rk3566-quartz64-b.dtb
```

---

## A. Подготовка microSD на host

### A1. Узнать диск

```bash
lsblk
```

### A2. (Опционально) затереть карту

Только если уверены в `$SD_DISK`:

```bash
sudo dd if=/dev/zero of=$SD_DISK bs=1M status=progress
```

Часто достаточно только USBImager — затирание не обязательно.

### A3. Записать образ

1. Открыть **USBImager**.
2. Образ: путь из `$IMG`.
3. Диск = тот же, что в `lsblk`.
4. **Write** → дождаться конца.
5. Если разделы не смонтировались — вынуть/вставить карту.

### A4. Терминал на смонтированной карте

Открыть том root (или boot+root, как смонтировалось) → Shift+F4.  
Дальше команды относительно **корня смонтированной флешки** (видны `boot/`, `usr/` и т.д.).

### A5. Device Tree

```bash
sudo cp ~/additions/rk356* boot/dtb/rockchip/
```

### A6. Только Quartz64 B — U-Boot

```bash
sudo cp ~/PROJECTS/u-boot-quartz64/idbloader.img \
  usr/lib/linux-u-boot-current-station-p2_23.06_arm64/
sudo cp ~/PROJECTS/u-boot-quartz64/u-boot.itb \
  usr/lib/linux-u-boot-current-station-p2_23.06_arm64/
```

Путь `usr/lib/linux-u-boot-…` сверить на карте через `ls`.

### A7. `extlinux.conf`

```bash
sudo nano boot/extlinux/extlinux.conf
```

- Строка **FDT / fdtdir**: нужный `$DTB`, например:
  - `rk3566-firefly-roc-pc.dtb` — ROC-RK3566
  - `rk3568-firefly-roc-pc.dtb`
  - `rk3566-quartz64-b.dtb` — Quartz64 B

- Строка **APPEND**: **сохранить** `root=UUID=…`, дальше **одной строкой**:

```text
console=ttyS2,1500000 console=tty0 rw no_console_suspend consoleblank=0 fsck.fix=yes fsck.repair=yes net.ifnames=0 quiet loglevel=1 video=HDMI-A-1:1280x800M@54.36D vt.global_cursor_default=0 initcall_blacklist=rk808_rtc_driver_init splash plymouth.ignore-serial-consoles fbcon=nodefer
```

`initcall_blacklist=rk808_rtc_driver_init` нужен для сохранения даты/времени на старых платах (вместе с обновлённым DT).

Сохранить: `Ctrl+X` → `Y` → Enter.

Карту безопасно извлечь → вставить в target → питание.

---

## B. Первый запуск target: сеть

Клавиатура на HDMI / UART / временный DHCP — как удобнее.

```bash
sudo nmcli con show
```

Статический Ethernet (подставьте IP `10.7.4.51`–`253`):

```bash
sudo nmcli con mod "Wired connection 1" \
  ipv4.addresses 10.7.4.51/24 \
  ipv4.gateway 10.7.4.254 \
  ipv4.dns "8.8.8.8 1.1.1.1" \
  ipv4.method manual \
  ipv6.method disabled

sudo nmcli con up "Wired connection 1"
# или: sudo systemctl restart NetworkManager
```

Проверка с host:

```bash
ssh kikorik@$TARGET_IP
```

Wi‑Fi (свои SSID/пароль):

```bash
nmcli dev wifi list
nmcli dev wifi connect "SSID" password "PASSWORD"
```

Если после смены IP «залип» старый адрес:

```bash
sudo ip addr del 10.64.61.10/24 dev eth0   # пример
```

В Qt Creator: **Tools → Options → Devices** — новый IP.

SSH без пароля (host):

```bash
ssh-copy-id kikorik@$TARGET_IP
```

---

## C. Пакеты на target

Если `apt update` падает (зеркало РФ / GPG) — сначала раздел [Armbian apt из РФ](#l-armbian-apt-из-рф).

```bash
sudo apt-get update

sudo apt-get install -y \
  gstreamer1.0-alsa gstreamer1.0-gl gstreamer1.0-libav gstreamer1.0-nice \
  gstreamer1.0-pipewire libgstreamer-gl1.0-0 libgstreamer1.0-0 libgstreamer1.0-dev \
  libmd4c0 libdouble-conversion3 \
  unzip zip qrencode dnsmasq-base \
  fonts-noto-color-emoji fonts-symbola \
  openssh-server openssl evtest \
  plymouth plymouth-themes imagemagick

sudo apt install --only-upgrade openssh-server openssl
```

Пакеты `gstreamer1.0-*-pulseeffects` ставить только если есть в репозитории; иначе пропустить.

Назначение части пакетов:

| Пакет | Зачем |
|---|---|
| `unzip` / `zip` | обновления / архив логов |
| `qrencode` | QR для обновления |
| `dnsmasq-base` | точка Wi‑Fi |
| `evtest` | отладка тача |
| `fonts-noto-color-emoji` / `fonts-symbola` | эмодзи в видеоплеере |

---

## D. Qt 5.15 на target

**host:**

```bash
ssh kikorik@$TARGET_IP 'sudo mkdir -p /usr/local/qt5_aarch && sudo chown kikorik:kikorik /usr/local/qt5_aarch'

cd /home/kikorik/QtFolder/Qt5.15.8_anew/
rsync -avz qt5_aarch/ kikorik@$TARGET_IP:/usr/local/qt5_aarch/
```

**target** — только **прямые** кавычки (не «ёлочки»):

```bash
echo /usr/local/qt5_aarch/lib | sudo tee /etc/ld.so.conf.d/qt5_aarch.conf
sudo ldconfig

echo 'QML2_IMPORT_PATH=/usr/local/qt5_aarch/qml' | sudo tee -a /etc/environment
echo 'QT_PLUGIN_PATH=/usr/local/qt5_aarch/plugins' | sudo tee -a /etc/environment
```

Кривые строки с `‘…’”` в `/etc/environment` дают `pam_env: non-alphanumeric key` — удалить и заменить как выше.

---

## E. Getty (autologin) и сервис приложения

### E1. Autologin на tty

```bash
sudo systemctl edit getty@.service
```

После комментариев:

```ini
[Service]
ExecStart=
ExecStart=-/sbin/agetty --skip-login --nonewline --noissue --autologin kikorik --noclear %I $TERM
```

`Ctrl+X` → `Y` → Enter.

### E2. Убрать старый system-сервис

```bash
sudo systemctl disable Demo1.service 2>/dev/null || true
sudo rm -f /etc/systemd/system/Demo1.service
```

### E3. User-сервис (`default.target` + linger)

```bash
sudo loginctl enable-linger kikorik
systemctl --user --force edit --full Demo1-user.service
```

Содержимое:

```ini
[Unit]
Description=OnyxM autostart

[Service]
Type=simple
Environment="QT_QPA_PLATFORM=eglfs"
Environment="QT_QPA_EGLFS_INTEGRATION=eglfs_kms"
Environment="QT_QPA_EGLFS_WIDTH=1280"
Environment="QT_QPA_EGLFS_HEIGHT=800"
Environment="QT_QPA_EGLFS_CURSOR=0"
Environment="QT_QPA_EGLFS_FORCE888=1"
Environment="QT_QPA_GENERIC_PLUGINS=evdevkeyboard"
Environment="QT_QPA_EVDEV_KEYBOARD_PARAMETERS=grab=1"
ExecStart=/usr/share/qtpr/UserInterface
Restart=always
RestartSec=2
KillMode=process

[Install]
WantedBy=default.target
```

```bash
systemctl --user daemon-reload
systemctl --user enable Demo1-user.service
```

Без перезагрузки:

```bash
systemctl --user restart Demo1-user.service
journalctl --user -u Demo1-user.service -n 100 -f
```

**Не** использовать `WantedBy=graphical-session.target` — на headless ROC сервис не стартует.

### E4. Тише консоль

В `/etc/pam.d/login` закомментировать блоки «Prints …» (`pam_motd`, `pam_lastlog`, `pam_mail`).

```bash
echo "export PS1=' :'" >> ~/.bash_profile
. ~/.bash_profile
```

---

## F. Перенос системы на NVMe

```bash
sudo armbian-install
```

- Boot from SD/eMMC — system on eMMC/USB/NVMe
- Цель: `/dev/nvme0n1p1` (или как покажет)
- FS: **ext4** оба раза

Выключить → вынуть SD → включить.

---

## G. Plymouth FOTEK

Старый путь `spinner` / `plymouth-set-default-theme bgrt` **не использовать**.

На target должны быть:

```text
~/FOTEK/Plymouth/          # скрипты + fotek-theme/
~/FOTEK/Images/start/      # start-ru.png, start-en.png, start-es.png
```

Исходники в репозитории UI: `deploy/plymouth/`.

### G1. Ранний DRM

В `/etc/initramfs-tools/modules`:

```text
drm
drm_panel_orientation_quirks
drm_kms_helper
cec
drm_display_helper
drm_dma_helper
display-connector
dw-hdmi
dw_mipi_dsi
analogix_dp
phy-rockchip-inno-hdmi
rockchipdrm
gpu_sched
drm_shmem_helper
panfrost
```

Имена сверить:

```bash
find /lib/modules/$(uname -r) -iname '*hdmi*' -o -iname '*display*connector*' -o -iname '*rockchipdrm*'
```

```bash
echo FRAMEBUFFER=y | sudo tee /etc/initramfs-tools/conf.d/plymouth
sudo update-initramfs -u -k $(uname -r)
sudo ln -sfn uInitrd-$(uname -r) /boot/uInitrd
```

В `extlinux` уже должны быть `splash plymouth.ignore-serial-consoles fbcon=nodefer` и `console=tty0` (шаг A7).

### G2. Установка темы

Под `sudo` путь к картинке — **абсолютный** (иначе ищется `/root/FOTEK/...`):

```bash
chmod +x ~/FOTEK/Plymouth/*.sh
cp ~/FOTEK/Images/start/start-ru.png ~/FOTEK/Images/start/start.png

cd ~/FOTEK/Plymouth
sudo ./install-fotek-plymouth.sh /home/kikorik/FOTEK/Images/start/start.png
```

### G3. Helper смены языка

```bash
sudo install -m 0755 ~/FOTEK/Plymouth/fotek-plymouth-splash.sh /usr/local/sbin/fotek-plymouth-splash
sudo install -m 0440 ~/FOTEK/Plymouth/99-fotek-plymouth-splash-sudoers /etc/sudoers.d/99-fotek-plymouth-splash
sudo visudo -cf /etc/sudoers.d/99-fotek-plymouth-splash

sudo -n /usr/local/sbin/fotek-plymouth-splash /home/kikorik/FOTEK/Images/start/start.png
```

Ожидание: `OK: ... updated`.

При смене языка в UI копируется `start-XX.png` → `start.png`, helper обновляет тему + initramfs; в настройках — плашка ожидания.

### G4. Проверка

```bash
uname -r
ls -l /boot/uInitrd
dmesg | grep -E 'fb0|rockchip-drm'
grep Theme /etc/plymouth/plymouthd.conf
lsinitramfs /boot/initrd.img-$(uname -r) | grep -E 'fotek-theme|rockchipdrm|dw-hdmi'
```

После reboot: `fb0` ~4 с, свой splash, затем UI.  
Ядра **6.2.16-edge-media** и **6.3.12-media** оба ок; критично, чтобы `uInitrd` совпадал с `uname -r`.

### Типичные ошибки Plymouth

| Симптом | Причина |
|---|---|
| Splash с ~9 с | DRM не в initrd / `uInitrd` на другое ядро |
| `update-initramfs` пишет другое ядро | Всегда `-k $(uname -r)` + симлинк `uInitrd` |
| После смены языка splash старый | Helper/sudoers или ушли из меню до конца `update-initramfs` |
| Qt «висит», LLVMpipe | Plymouth не отдал DRM; `sudo plymouth quit` |
| `Fullscreen splash not found` | Под sudo нужен абсолютный путь к `start.png` |

---

## H. Звук (Pulse + динамик)

```bash
pacmd list-sinks | grep -e 'name:' -e 'index:'
# типично: alsa_output.platform-rk809-sound.stereo-fallback
```

В `/etc/pulse/default.pa` задать default sink на этот выход.

```bash
pactl set-default-sink alsa_output.platform-rk809-sound.stereo-fallback

# динамик (SPK), не наушники:
amixer -c 0 cset numid=4 1
paplay /usr/share/sounds/alsa/Front_Center.wav
# наушники: amixer -c 0 cset numid=4 0
```

---

## I. Развёртывание приложения и медиа

```bash
sudo mkdir -p /usr/share/qtpr
sudo chown kikorik:kikorik /usr/share/qtpr
sudo install -d -o kikorik -g kikorik -m 755 /var/lib/qtpr
mkdir -p ~/FOTEK
```

**host** — бинарь (путь build подставьте свой):

```bash
rsync -av \
  /home/kikorik/QtFolder/projects/build-UserInterface-M2-Debug/UserInterface \
  kikorik@$TARGET_IP:/usr/share/qtpr/
```

StratifyLabs:

```bash
rsync -av /usr/lib/x86_64-linux-gnu/qt5/qml/StratifyLabs \
  kikorik@$TARGET_IP:~/
```

**target:**

```bash
sudo mkdir -p /usr/lib/aarch64-linux-gnu/qt5/qml/
sudo mv ~/StratifyLabs /usr/lib/aarch64-linux-gnu/qt5/qml/
```

БД и картинки:

```bash
# host
rsync -avz eshfDb.db Images kikorik@$TARGET_IP:/home/kikorik/FOTEK/
```

В `Images/`: `modes/`, `instruments/`, `scopes/`, плюс `start/start-*.png` для Plymouth.

### Опционально: wrapper от спама курсора

`/usr/share/qtpr/launch_wrapper.sh`:

```bash
#!/bin/bash
"$@" 2>&1 | while IFS= read -r line; do
  if [[ ! "$line" =~ (cursor|Failed to move cursor|Could not set cursor|screen HDMI1) ]]; then
    echo "$line"
  fi
done
```

```bash
sudo chmod 740 /usr/share/qtpr/launch_wrapper.sh
```

В сервисе: `ExecStart=/usr/share/qtpr/launch_wrapper.sh /usr/share/qtpr/UserInterface`.

### Отладка из Qt Creator

Временно убрать `ExecStart` / `Restart` из user-сервиса → `daemon-reload` → потом вернуть.

---

## J. Права: nmcli, питание, firewall upload

### nmcli без пароля

```bash
sudo visudo -f /etc/sudoers.d/onyx-nmcli
```

Одна строка:

```text
kikorik ALL=(root) NOPASSWD: /usr/bin/nmcli
```

### Polkit (power / time / NM)

```bash
sudo nano /etc/polkit-1/rules.d/50-onyx-device-control.rules
```

```javascript
polkit.addRule(function(action, subject) {
    if (subject.user !== "kikorik") {
        return polkit.Result.NOT_HANDLED;
    }
    if (action.id === "org.freedesktop.NetworkManager.network-control" ||
        action.id === "org.freedesktop.login1.power-off" ||
        action.id === "org.freedesktop.login1.power-off-multiple-sessions" ||
        action.id === "org.freedesktop.login1.power-off-ignore-inhibit" ||
        action.id === "org.freedesktop.login1.reboot" ||
        action.id === "org.freedesktop.login1.reboot-multiple-sessions" ||
        action.id === "org.freedesktop.login1.reboot-ignore-inhibit" ||
        action.id === "org.freedesktop.timedate1.set-time" ||
        action.id === "org.freedesktop.timedate1.set-ntp") {
        return polkit.Result.YES;
    }
    return polkit.Result.NOT_HANDLED;
});
```

```bash
sudo chmod 644 /etc/polkit-1/rules.d/50-onyx-device-control.rules
```

### Upload firewall

Файлы: `deploy/firewall/` в репозитории UI.

```bash
sudo install -m 0755 deploy/firewall/upload-fw-guard.sh /usr/local/sbin/upload-fw-guard
sudo install -m 0440 deploy/firewall/99-upload-fw-guard-sudoers /etc/sudoers.d/99-upload-fw-guard
sudo visudo -cf /etc/sudoers.d/99-upload-fw-guard
```

UFW настраивать отдельно и осознанно; SSH сначала по ключам, текущую сессию не закрывать до проверки второго входа.

---

## K. Полезное

**Тач:**

```bash
sudo evtest
sudo evtest /dev/input/event0
```

**Видео для плеера:** H.264, 1280×720, 25 fps, yuv420p, ~5 Mbps:

```bash
ffmpeg -i input.mp4 \
  -c:v libx264 -preset medium \
  -profile:v high -level 4.0 \
  -pix_fmt yuv420p \
  -vf scale=-2:720 \
  -r 25 -b:v 5M \
  -c:a aac -b:a 128k \
  output.mp4
```

**Updater на host:**

```bash
cd ~/QtFolder/projects/Updater-FastAPI/ui-update-server
source .venv/bin/activate
python -m uvicorn app.main:app --host 0.0.0.0 --port 8000
# http://127.0.0.1:8000/admin/releases
```

---

## L. Armbian apt из РФ

Если `apt update` падает на зеркале Armbian / `NO_PUBKEY`.

`/etc/apt/sources.list.d/armbian.list`:

```text
deb [signed-by=/usr/share/keyrings/armbian-archive-keyring.gpg] http://mirror.yandex.ru/mirrors/armbian/apt bookworm main bookworm-utils bookworm-desktop
```

```bash
sudo rm -f /usr/share/keyrings/armbian-archive-keyring.gpg
sudo mkdir -p /usr/share/keyrings

curl -fsSL https://mirror.yandex.ru/mirrors/armbian/apt/armbian.key \
| gpg --dearmor | sudo tee /usr/share/keyrings/armbian-archive-keyring.gpg > /dev/null

curl -fsSL 'https://keyserver.ubuntu.com/pks/lookup?op=get&search=0x93D6889F9F0E78D5' \
| sudo gpg --no-default-keyring \
  --keyring /usr/share/keyrings/armbian-archive-keyring.gpg --import

sudo ln -sf /usr/share/keyrings/armbian-archive-keyring.gpg /usr/share/keyrings/armbian.gpg
sudo apt clean
sudo rm -rf /var/lib/apt/lists/partial/*
sudo apt update --allow-releaseinfo-change
```

---

## Чеклист «плата готова»

1. SD записан, DT + `extlinux` (видео + blacklist RTC + splash).
2. Сеть статическая, SSH по ключу.
3. Qt в `/usr/local/qt5_aarch`, `/etc/environment` без кривых кавычек.
4. `Demo1-user` → `default.target`, linger.
5. Приложение + FOTEK media + StratifyLabs.
6. Plymouth fotek-theme + early DRM + `uInitrd` = `uname -r`.
7. Helper языка + sudoers.
8. nmcli / polkit.
9. Звук на SPK при необходимости.
10. `reboot` → splash ~4 с → UI.

---

## Устаревшее (не повторять)

| Было | Как надо |
|---|---|
| Plymouth `spinner` / `bgrt` + `background-tile` | `fotek-theme` + `~/FOTEK/Plymouth` |
| `WantedBy=graphical-session.target` | `default.target` + `enable-linger` |
| `‘QML2_IMPORT_PATH=”…”’` в environment | прямые кавычки / путь без лишних кавычек |
| `install-fotek-plymouth` без абсолютного пути под sudo | `/home/kikorik/FOTEK/Images/start/start.png` |
| `update-initramfs` без `-k $(uname -r)` | всегда ядро = `uname -r` и симлинк `uInitrd` |
