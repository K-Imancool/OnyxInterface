QT += quick multimedia
CONFIG += c++17 link_pkgconfig disable-desktop

RESOURCES += $$PWD/userinterface.qrc

DISTFILES += \
    $$PWD/Activation.qml \
    $$PWD/ArgCylinder.qml \
    $$PWD/Argon.qml \
    $$PWD/GradientBack.qml \
    $$PWD/HalfSocket.qml \
    $$PWD/InstrumEditor.qml \
    $$PWD/InstrumRect.qml \
    $$PWD/ItemList.qml \
    $$PWD/MainMenu.qml \
    $$PWD/MenuLoader.qml \
    $$PWD/ModeEditor.qml \
    $$PWD/ModePowerRect.qml \
    $$PWD/NeutralButton.qml \
    $$PWD/NeutralEl.qml \
    $$PWD/SerialNumberSettings.qml \
    $$PWD/Pedal.qml \
    $$PWD/PedalContainer.qml \
    $$PWD/PedalDrawer.qml \
    $$PWD/PedalEditor.qml \
    $$PWD/PedalRepeater.qml \
    $$PWD/PeripheryDrawer.qml \
    $$PWD/PeripheryPanel.qml \
    $$PWD/PowerRect.qml \
    $$PWD/ProgAdditionPop.qml \
    $$PWD/ProgItemList.qml \
    $$PWD/ProgSaveDialog.qml \
    $$PWD/SocketRepeater.qml \
    $$PWD/StatesSocket.qml \
    $$PWD/SocketContainerV2.qml \
    $$PWD/StatusBar.qml
    $$PWD/VideoPlayer.qml

HEADERS +=

SOURCES +=
