include($$PWD/GUI/userinterface.pri)
include($$PWD/SettingsScreen/settingsscreen.pri)
include($$PWD/BackEnd/BackEnd.pri)

QT += quick multimedia multimediawidgets

# Линковка с математической библиотекой
LIBS += -lm

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

# Для Linux
# Закомментировано - Qt Multimedia уже включает поддержку GStreamer
#unix:!mac {
#    CONFIG += link_pkgconfig
#    PKGCONFIG += gstreamer-1.0 gstreamer-video-1.0
#}

SOURCES += \
        main.cpp

HEADERS += \
        appversion.h

RESOURCES += qml.qrc

TRANSLATIONS += \
    translations/UserInterface_en.ts \
    translations/UserInterface_es.ts

# Additional import path used to resolve QML modules in Qt Creator's code model
# QML_IMPORT_PATH =

# Additional import path used to resolve QML modules just for Qt Quick Designer
# QML_DESIGNER_IMPORT_PATH =

# # Default rules for deployment.
# qnx: target.path = /tmp/$${TARGET}/bin
# else: unix:!android: target.path = /opt/$${TARGET}/bin
# !isEmpty(target.path): INSTALLS += target
QT_QPA_ENABLE_TERMINAL_KEYBOARD = 0


# Default rules for deployment.
# qnx: target.path = /tmp/$${TARGET}/bin
# else: unix:!android: target.path = /opt/$${TARGET}/bin
# !isEmpty(target.path): INSTALLS += target

target.path = /usr/share/qtpr
INSTALLS += target

DISTFILES += \
    main.qml
