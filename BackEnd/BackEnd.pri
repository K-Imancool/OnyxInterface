QT += core quick qml network
QT += serialport multimedia sql concurrent

CONFIG += c++17

# Линковка с математической библиотекой
LIBS += -lm

include($$PWD/apppaths.pri)

HEADERS += \
    $$PWD/dblocale.h \
    $$PWD/translationcontroller.h \
    $$PWD/datetimecontroller.h \
    $$PWD/uiclicksound.h \
    $$PWD/DeviceLogManager.h \
    $$PWD/UpdateLogManager.h \
    $$PWD/featureunlockcontroller.h \
    $$PWD/Structures.h \
    $$PWD/controlcenter.h \
    $$PWD/HttpUploadController.h \
    $$PWD/databasereader.h \
    $$PWD/EshfProgStringBuilder.h \
    $$PWD/halfsocket.h \
    $$PWD/instrimageprovider.h \
    $$PWD/instrument.h \
    $$PWD/jsonstorage.h \
    $$PWD/McFirmwareVersionsBridge.h \
    $$PWD/gstreamervideoplayer.h \
    $$PWD/keygenerator.h \
    $$PWD/linkstm.h \
    $$PWD/loggingcategories.h \
    $$PWD/onyxapp.h \
    $$PWD/pedal.h \
    $$PWD/periphhandler.h \
    $$PWD/proghandle.h \
    $$PWD/progloader.h \
    $$PWD/progloaderbase.h \
    $$PWD/recomprogloader.h \
    $$PWD/socket.h \
    $$PWD/socketmodeeditor.h \
    $$PWD/socketmodel.h \
    $$PWD/stmupdater.h \
    $$PWD/surgicalmode.h \
    $$PWD/uartqmlbridge.h \
    $$PWD/userprogloader.h \
    $$PWD/userprogtransfercontroller.h \
    $$PWD/systemmonitor.h \
    $$PWD/gpiomonitor.h 

SOURCES += \
    $$PWD/dblocale.cpp \
    $$PWD/translationcontroller.cpp \
    $$PWD/datetimecontroller.cpp \
    $$PWD/uiclicksound.cpp \
    $$PWD/DeviceLogManager.cpp \
    $$PWD/UpdateLogManager.cpp \
    $$PWD/featureunlockcontroller.cpp \
    $$PWD/controlcenter.cpp \
    $$PWD/HttpUploadController.cpp \
    $$PWD/databasereader.cpp \
    $$PWD/EshfProgStringBuilder.cpp \
    $$PWD/halfsocket.cpp \
    $$PWD/instrimageprovider.cpp \
    $$PWD/instrument.cpp \
    $$PWD/jsonstorage.cpp \
    $$PWD/McFirmwareVersionsBridge.cpp \
    $$PWD/gstreamervideoplayer.cpp \
    $$PWD/keygenerator.cpp \
    $$PWD/linkstm.cpp \
    $$PWD/loggingcategories.cpp \
    $$PWD/onyxapp.cpp \
    $$PWD/pedal.cpp \
    $$PWD/periphhandler.cpp \
    $$PWD/proghandle.cpp \
    $$PWD/progloader.cpp \
    $$PWD/recomprogloader.cpp \
    $$PWD/socket.cpp \
    $$PWD/socketmodeeditor.cpp \
    $$PWD/socketmodel.cpp \
    $$PWD/stmupdater.cpp \
    $$PWD/surgicalmode.cpp \
    $$PWD/uartqmlbridge.cpp \
    $$PWD/userprogloader.cpp \
    $$PWD/userprogtransfercontroller.cpp \
    $$PWD/systemmonitor.cpp \
    $$PWD/gpiomonitor.cpp 

RESOURCES += \
    $$PWD/backend.qrc
