QT += core quick qml network
QT += serialport multimedia sql concurrent

CONFIG += c++17

# Линковка с математической библиотекой
LIBS += -lm

HEADERS += \
    $$PWD/EshfProgStringBuilder.h \
    $$PWD/Structures.h \
    $$PWD/controlcenter.h \
    $$PWD/HttpUploadController.h \
    $$PWD/databasereader.h \
    $$PWD/halfsocket.h \
    $$PWD/instrimageprovider.h \
    $$PWD/instrument.h \
    $$PWD/jsonstorage.h \
    $$PWD/McFirmwareVersionsBridge.h \
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
    $$PWD/systemmonitor.h 

SOURCES += \
    $$PWD/EshfProgStringBuilder.cpp \
    $$PWD/controlcenter.cpp \
    $$PWD/HttpUploadController.cpp \
    $$PWD/databasereader.cpp \
    $$PWD/halfsocket.cpp \
    $$PWD/instrimageprovider.cpp \
    $$PWD/instrument.cpp \
    $$PWD/jsonstorage.cpp \
    $$PWD/McFirmwareVersionsBridge.cpp \
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
    $$PWD/systemmonitor.cpp 

RESOURCES += \
    $$PWD/backend.qrc
