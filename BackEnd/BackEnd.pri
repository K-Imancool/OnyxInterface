QT += core quick qml
QT += serialport multimedia sql concurrent

CONFIG += c++17

HEADERS += \
    $$PWD/controlcenter.h \
    $$PWD/databasereader.h \
    $$PWD/halfsocket.h \
    $$PWD/instrimageprovider.h \
    $$PWD/instrument.h \
    $$PWD/jsonstorage.h \
    $$PWD/linkstm.h \
    $$PWD/loggingcategories.h \
    $$PWD/pedal.h \
    $$PWD/proghandle.h \
    $$PWD/socket.h \
    $$PWD/socketmodeeditor.h \
    $$PWD/socketmodel.h \
    $$PWD/stmupdater.h \
    $$PWD/surgicalmode.h \
    $$PWD/uartqmlbridge.h

SOURCES += \
    $$PWD/controlcenter.cpp \
    $$PWD/databasereader.cpp \
    $$PWD/halfsocket.cpp \
    $$PWD/instrimageprovider.cpp \
    $$PWD/instrument.cpp \
    $$PWD/jsonstorage.cpp \
    $$PWD/linkstm.cpp \
    $$PWD/loggingcategories.cpp \
    $$PWD/pedal.cpp \
    $$PWD/proghandle.cpp \
    $$PWD/socket.cpp \
    $$PWD/socketmodeeditor.cpp \
    $$PWD/socketmodel.cpp \
    $$PWD/stmupdater.cpp \
    $$PWD/surgicalmode.cpp \
    $$PWD/uartqmlbridge.cpp

RESOURCES += \
    $$PWD/backend.qrc
