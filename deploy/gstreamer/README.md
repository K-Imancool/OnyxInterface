# qmlglsink для ROC-RK3566

`libgstqmlgl.so` собран из `gst-plugins-good 1.22.0` для AArch64 против
пользовательского Qt 5.15.8 проекта. Поддержка QtWayland исключена; вывод
использует Qt EGLFS.

Плагин устанавливается в:

```text
/usr/share/qtpr/gstreamer-1.0/libgstqmlgl.so
```

Проверка зависимостей на устройстве:

```bash
ldd /usr/share/qtpr/gstreamer-1.0/libgstqmlgl.so
```

В списке зависимостей не должно быть `libQt5WaylandClient.so.5`.
