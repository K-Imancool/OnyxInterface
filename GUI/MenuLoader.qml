import QtQuick 2.15

Item {
    id: menuLoadRoot

    property alias source : menuLoader.source
    property bool shortcut : false
    property alias item : menuLoader.item
    readonly property var loader : menuLoader
    focus: true

    function hideKeyboardAndDropFocus() {
        Qt.inputMethod.commit()
        Qt.inputMethod.hide()
        menuLoadRoot.focus = true
    }

    function navigateTo(sourcePath) {
        hideKeyboardAndDropFocus()
        menuLoader.source = sourcePath
        Qt.callLater(function() {
            hideKeyboardAndDropFocus()
            if (menuLoader.item && menuLoader.item.forceActiveFocus) {
                menuLoader.item.forceActiveFocus()
            }
        })
    }

    function loaderSourceString() {
        var s = menuLoader.source
        if (s === undefined || s === null)
            return ""
        return (typeof s === "string") ? s : s.toString()
    }

    function loaderSourceBaseName() {
        var src = loaderSourceString()
        var slash = src.lastIndexOf("/")
        return slash >= 0 ? src.substring(slash + 1) : src
    }

    function isServiceFlowScreenBaseName(base) {
        return base === "ServiceMenu.qml"
                || base === "SerialNumberSettings.qml"
                || base === "updateWindow.qml"
                || base === "ServiceNetworkSettings.qml"
                || base === "WifiFileReceive.qml"
                || base === "WiFiConnector.qml"
                || base === "AboutScreen.qml"
    }

    signal returnButtonPressed()
    signal closeMe()
    signal programSelected(string scopeName, string progName)
    signal freeSettingsModeActivated()
    signal deleteAllUserProgsRequested()
    Loader {
        id: menuLoader
        anchors.fill: parent
        source: "qrc:/MainMenu.qml"
        
        onItemChanged: {
            // Отключаем все предыдущие подключения
            /// TODO: даже учитывая пользу от коннекта таким образом, в виде отсутствия варнингов
            /// необходимо вернуться к коннектам как обхъектам т.к. у нас много динамики и коннект как функция может уронить приложение
            /// т.к. может оставаться жив после удаление объекта на который использовался

            if (menuLoader.item) {
                try {
                    if (menuLoader.item.recommendButtonPressed) {
                        menuLoader.item.recommendButtonPressed.disconnect()
                    }
                    if (menuLoader.item.settingsButtonPressed) {
                        menuLoader.item.settingsButtonPressed.disconnect()
                    }
                    if (menuLoader.item.clickedButton) {
                        menuLoader.item.clickedButton.disconnect()
                    }
                    if (menuLoader.item.userButtonPressed) {
                        menuLoader.item.userButtonPressed.disconnect()
                    }
                } catch(e) {
                    // Игнорируем ошибки отключения
                }
                
                // Подключаемся к сигналам, которые есть во всех компонентах
                try {
                    if (menuLoader.item.recommendButtonPressed) {
                        menuLoader.item.recommendButtonPressed.connect(function() {

                             menuLoader.setSource("qrc:/ProgItemList.qml", {"recommended" : true})
                        })
                    }
                    if (menuLoader.item.settingsButtonPressed) {
                        menuLoader.item.settingsButtonPressed.connect(function() {
                            navigateTo("qrc:/ServiceMenu.qml")
                        })
                    }
                    if (menuLoader.item.secretKeysButtonPressed) {
                        menuLoader.item.secretKeysButtonPressed.connect(function() {
                            navigateTo("qrc:/SecretKeysWindow.qml")
                        })
                    }
                    if (menuLoader.item.exitButtonPressed) {
                        menuLoader.item.exitButtonPressed.connect(function() {
                            closeMe()
                        })
                    }
                    if (menuLoader.item.userButtonPressed) {
                        menuLoader.item.userButtonPressed.connect(function() {
                            menuLoader.setSource("qrc:/ProgItemList.qml", {"recommended" : false,
                                                                            "editable" : true})
                        })
                    }
                    if (menuLoader.item.freeSettingsButtonPressed) {
                        menuLoader.item.freeSettingsButtonPressed.connect(function() {
                            freeSettingsModeActivated()
                            closeMe()
                        })
                    }
                    if (menuLoader.item.serviceMenuButtonPressed) {
                        menuLoader.item.serviceMenuButtonPressed.connect(function() {
                            navigateTo("qrc:/ServiceMenu.qml")
                        })
                    }
                    if (menuLoader.item.serialNumberButtonPressed) {
                        menuLoader.item.serialNumberButtonPressed.connect(function() {
                            navigateTo("qrc:/SerialNumberSettings.qml")
                        })
                    }
                    if (menuLoader.item.softwareUpdateButtonPressed) {
                        menuLoader.item.softwareUpdateButtonPressed.connect(function() {
                            navigateTo("qrc:/updateWindow.qml")
                        })
                    }
                    if (menuLoader.item.networkSettingsButtonPressed) {
                        menuLoader.item.networkSettingsButtonPressed.connect(function() {
                            navigateTo("qrc:/ServiceNetworkSettings.qml")
                        })
                    }
                    if (menuLoader.item.wifiFileReceiveButtonPressed) {
                        menuLoader.item.wifiFileReceiveButtonPressed.connect(function() {
                            navigateTo("qrc:/WifiFileReceive.qml")
                        })
                    }
                    if (menuLoader.item.wifiSettingsButtonPressed) {
                        menuLoader.item.wifiSettingsButtonPressed.connect(function() {
                            navigateTo("qrc:/WiFiConnector.qml")
                        })
                    }
                    if (menuLoader.item.aboutButtonPressed) {
                        menuLoader.item.aboutButtonPressed.connect(function() {
                            navigateTo("qrc:/AboutScreen.qml")
                        })
                    }
                    if (menuLoader.item.deleteAllUserProgsRequested) {
                        menuLoader.item.deleteAllUserProgsRequested.connect(function() {
                            deleteAllUserProgsRequested()
                        })
                    }
                } catch(e) {
                    // Игнорируем ошибки подключения
                }

                // Подключаемся к сигналам, которых нет в MainMenu
                if (menuLoader.source !== "qrc:/MainMenu.qml") {
                    try {
                        if (menuLoader.item.clickedButton) {
                            menuLoader.item.clickedButton.connect(function(progId) {
                                closeMe()
                                navigateTo("qrc:/MainMenu.qml")
                            })
                        }
                        if (menuLoader.item.programSelected) {
                            menuLoader.item.programSelected.connect(function(scopeName, progName) {
                                menuLoadRoot.programSelected(scopeName, progName)
                            })
                        }
                    } catch(e) {
                        // Игнорируем ошибки подключения
                    }
                }
            }
        }
    }

    Connections {
        target: menuLoader.item
        ignoreUnknownSignals: true
        enabled: loaderSourceBaseName() !== "MainMenu.qml"
        function onReturnButtonPressed() {
            var base = loaderSourceBaseName()
            var inServiceFlow = isServiceFlowScreenBaseName(base)
            if (inServiceFlow && base !== "ServiceMenu.qml") {
                navigateTo("qrc:/ServiceMenu.qml")
            } else if (inServiceFlow && base === "ServiceMenu.qml") {
                navigateTo("qrc:/MainMenu.qml")
            } else if (shortcut) {
                shortcut = false
                closeMe()
                if (menuLoader.item && menuLoader.item.loadClear !== undefined) {
                    menuLoader.item.loadClear = true
                }
                navigateTo("qrc:/MainMenu.qml")
            } else {
                navigateTo("qrc:/MainMenu.qml")
            }
        }
    }
}
