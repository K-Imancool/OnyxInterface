# Можно подключать из BackEnd.pri и из updateclient.pri без дублирования.
isEmpty(APPPATHS_PRI_INCLUDED) {
    APPPATHS_PRI_INCLUDED = 1
    INCLUDEPATH += $$PWD
    HEADERS += $$PWD/apppaths.h
    SOURCES += $$PWD/apppaths.cpp
}
