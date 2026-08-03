import QtQuick 2.15

Item {
    id: menuLoadRoot

    property alias source : menuLoader.source
    property bool shortcut : false
    property bool closeOnServiceRootReturn: false
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

    function navigateToWithProperties(sourcePath, properties) {
        hideKeyboardAndDropFocus()
        menuLoader.setSource(sourcePath, properties)
        Qt.callLater(function() {
            hideKeyboardAndDropFocus()
            if (menuLoader.item && menuLoader.item.forceActiveFocus) {
                menuLoader.item.forceActiveFocus()
            }
        })
    }

    function savedJsonString(key) {
        if (typeof savedJson === "undefined" || !savedJson) {
            return ""
        }
        return savedJson.readString(key, "")
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

    property string serviceMenuAccessLevel: "full"

    function navigateToServiceMenu(accessLevel) {
        serviceMenuAccessLevel = accessLevel || "full"
        navigateToWithProperties("qrc:/ServiceMenu.qml", {
            "accessLevel": serviceMenuAccessLevel
        })
    }

    function isServiceMenuScreenAllowed(base) {
        if (serviceMenuAccessLevel !== "limited") {
            return true
        }
        return base === "ServiceMenu.qml"
                || base === "updateWindow.qml"
                || base === "WifiFileReceive.qml"
                || base === "AboutScreen.qml"
                || base === "logUpdate.qml"
    }

    function isServiceMenuChildScreenBaseName(base) {
        return base === "SerialNumberSettings.qml"
                || base === "updateWindow.qml"
                || base === "WifiFileReceive.qml"
                || base === "WiFiConnector.qml"
                || base === "TouchScreenTest.qml"
                || base === "AboutScreen.qml"
                || base === "SpecialCommands.qml"
                || base === "SecretKeysWindow.qml"
                || base === "FeatureOptionsManager.qml"
                || base === "IsnCorrection.qml"
                || base === "logUpdate.qml"
    }

    function isSettingsMenuChildScreenBaseName(base) {
        return base === "AdditionalSettingsMenu.qml"
                || base === "StartupInfoScreen.qml"
    }

    function isAdditionalSettingsChildScreenBaseName(base) {
        return base === "ServiceMenu.qml"
                || base === "LogFileScreen.qml"
                || base === "DateTimeSettings.qml"
    }

    signal returnButtonPressed()
    signal closeMe()
    signal programSelected(string scopeName, string progName, int scopeId, int progId)
    signal freeSettingsModeActivated()
    signal deleteAllUserProgsRequested()
    signal saveSettingsButtonPressed()

    // Перехват касаний по пустым зонам; дочерние кнопки/поля — через propagateComposedEvents
    MouseArea {
        id: menuTouchShield
        anchors.fill: parent
        propagateComposedEvents: true
        z: 0

        Rectangle {
            anchors.fill: parent
            color: "darkslategray"
            z: -1
        }

        Loader {
            id: menuLoader
            anchors.fill: parent
            // Пусто по умолчанию: MainMenu задаёт вызывающий код (startup / drawer).
            // Иначе WorkScreen при создании снова тянет тяжёлое меню.
            source: ""

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
                    if (menuLoader.item.secretKeysButtonPressed) {
                        menuLoader.item.secretKeysButtonPressed.disconnect()
                    }
                    if (menuLoader.item.userButtonPressed) {
                        menuLoader.item.userButtonPressed.disconnect()
                    }
                    if (menuLoader.item.freeSettingsButtonPressed) {
                        menuLoader.item.freeSettingsButtonPressed.disconnect()
                    }
                    if (menuLoader.item.saveSettingsButtonPressed) {
                        menuLoader.item.saveSettingsButtonPressed.disconnect()
                    }
                    if (menuLoader.item.infoButtonPressed) {
                        menuLoader.item.infoButtonPressed.disconnect()
                    }
                    if (menuLoader.item.logFileButtonPressed) {
                        menuLoader.item.logFileButtonPressed.disconnect()
                    }
                    if (menuLoader.item.additionalSettingsButtonPressed) {
                        menuLoader.item.additionalSettingsButtonPressed.disconnect()
                    }
                    if (menuLoader.item.dateTimeSettingsButtonPressed) {
                        menuLoader.item.dateTimeSettingsButtonPressed.disconnect()
                    }
                    if (menuLoader.item.logUpdateButtonPressed) {
                        menuLoader.item.logUpdateButtonPressed.disconnect()
                    }
                    if (menuLoader.item.specialCommandsButtonPressed) {
                        menuLoader.item.specialCommandsButtonPressed.disconnect()
                    }
                    if (menuLoader.item.featureOptionsButtonPressed) {
                        menuLoader.item.featureOptionsButtonPressed.disconnect()
                    }
                    if (menuLoader.item.isnCorrectionButtonPressed) {
                        menuLoader.item.isnCorrectionButtonPressed.disconnect()
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
                            navigateTo("qrc:/SettingsMenu.qml")
                        })
                    }
                    if (menuLoader.item.secretKeysButtonPressed) {
                        menuLoader.item.secretKeysButtonPressed.connect(function() {
                            if (!isServiceMenuScreenAllowed("SecretKeysWindow.qml")) {
                                return
                            }
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
                    if (menuLoader.item.saveSettingsButtonPressed) {
                        menuLoader.item.saveSettingsButtonPressed.connect(function() {
                            saveSettingsButtonPressed()
                            closeMe()
                        })
                    }
                    if (menuLoader.item.serviceMenuButtonPressed) {
                        menuLoader.item.serviceMenuButtonPressed.connect(function(accessLevel) {
                            if (loaderSourceBaseName() === "MainMenu.qml") {
                                navigateTo("qrc:/SettingsMenu.qml")
                            } else {
                                navigateToServiceMenu(accessLevel)
                            }
                        })
                    }
                    if (menuLoader.item.additionalSettingsButtonPressed) {
                        menuLoader.item.additionalSettingsButtonPressed.connect(function() {
                            navigateTo("qrc:/AdditionalSettingsMenu.qml")
                        })
                    }
                    if (menuLoader.item.infoButtonPressed) {
                        menuLoader.item.infoButtonPressed.connect(function() {
                            navigateTo("qrc:/StartupInfoScreen.qml")
                        })
                    }
                    if (menuLoader.item.serialNumberButtonPressed) {
                        menuLoader.item.serialNumberButtonPressed.connect(function() {
                            if (!isServiceMenuScreenAllowed("SerialNumberSettings.qml")) {
                                return
                            }
                            navigateTo("qrc:/SerialNumberSettings.qml")
                        })
                    }
                    if (menuLoader.item.softwareUpdateButtonPressed) {
                        menuLoader.item.softwareUpdateButtonPressed.connect(function() {
                            if (!isServiceMenuScreenAllowed("updateWindow.qml")) {
                                return
                            }
                            navigateTo("qrc:/updateWindow.qml")
                        })
                    }
                    if (menuLoader.item.wifiFileReceiveButtonPressed) {
                        menuLoader.item.wifiFileReceiveButtonPressed.connect(function() {
                            if (!isServiceMenuScreenAllowed("WifiFileReceive.qml")) {
                                return
                            }
                            navigateTo("qrc:/WifiFileReceive.qml")
                        })
                    }
                    if (menuLoader.item.wifiSettingsButtonPressed) {
                        menuLoader.item.wifiSettingsButtonPressed.connect(function() {
                            if (!isServiceMenuScreenAllowed("WiFiConnector.qml")) {
                                return
                            }
                            navigateTo("qrc:/WiFiConnector.qml")
                        })
                    }
                    if (menuLoader.item.aboutButtonPressed) {
                        menuLoader.item.aboutButtonPressed.connect(function() {
                            if (!isServiceMenuScreenAllowed("AboutScreen.qml")) {
                                return
                            }
                            navigateToWithProperties("qrc:/AboutScreen.qml", {
                                "serialNumber": savedJsonString("serialNumber"),
                                "deviceType": savedJsonString("deviceType"),
                                "featureNotes": savedJsonString("featureNotes")
                            })
                        })
                    }
                    if (menuLoader.item.specialCommandsButtonPressed) {
                        menuLoader.item.specialCommandsButtonPressed.connect(function() {
                            if (!isServiceMenuScreenAllowed("SpecialCommands.qml")) {
                                return
                            }
                            navigateTo("qrc:/SpecialCommands.qml")
                        })
                    }
                    if (menuLoader.item.featureOptionsButtonPressed) {
                        menuLoader.item.featureOptionsButtonPressed.connect(function() {
                            if (!isServiceMenuScreenAllowed("FeatureOptionsManager.qml")) {
                                return
                            }
                            navigateTo("qrc:/FeatureOptionsManager.qml")
                        })
                    }
                    if (menuLoader.item.isnCorrectionButtonPressed) {
                        menuLoader.item.isnCorrectionButtonPressed.connect(function() {
                            if (!isServiceMenuScreenAllowed("IsnCorrection.qml")) {
                                return
                            }
                            navigateTo("qrc:/IsnCorrection.qml")
                        })
                    }
                    if (menuLoader.item.touchScreenTestButtonPressed) {
                        menuLoader.item.touchScreenTestButtonPressed.connect(function() {
                            if (!isServiceMenuScreenAllowed("TouchScreenTest.qml")) {
                                return
                            }
                            navigateTo("qrc:/TouchScreenTest.qml")
                        })
                    }
                    if (menuLoader.item.logFileButtonPressed) {
                        menuLoader.item.logFileButtonPressed.connect(function() {
                            navigateTo("qrc:/LogFileScreen.qml")
                        })
                    }
                    if (menuLoader.item.dateTimeSettingsButtonPressed) {
                        menuLoader.item.dateTimeSettingsButtonPressed.connect(function() {
                            navigateTo("qrc:/DateTimeSettings.qml")
                        })
                    }
                    if (menuLoader.item.logUpdateButtonPressed) {
                        menuLoader.item.logUpdateButtonPressed.connect(function() {
                            if (!isServiceMenuScreenAllowed("logUpdate.qml")) {
                                return
                            }
                            navigateTo("qrc:/logUpdate.qml")
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
                            menuLoader.item.programSelected.connect(function(scopeName, progName, scopeId, progId) {
                                menuLoadRoot.programSelected(scopeName, progName, scopeId, progId)
                            })
                        }
                    } catch(e) {
                        // Игнорируем ошибки подключения
                    }
                }
            }
        }
        }

        onPressed: function(mouse) {
            mouse.accepted = true
        }
        onReleased: function(mouse) {
            mouse.accepted = true
        }
        onClicked: function(mouse) {
            mouse.accepted = true
        }
    }

    Connections {
        target: menuLoader.item
        ignoreUnknownSignals: true
        enabled: loaderSourceBaseName() !== "MainMenu.qml"
        function onReturnButtonPressed() {
            if (typeof container !== "undefined"
                    && container.suppressMenuNavigationForLanguageChange) {
                return
            }
            var base = loaderSourceBaseName()
            if (isServiceMenuChildScreenBaseName(base)) {
                navigateToServiceMenu(serviceMenuAccessLevel)
            } else if (isAdditionalSettingsChildScreenBaseName(base)) {
                navigateTo("qrc:/AdditionalSettingsMenu.qml")
            } else if (isSettingsMenuChildScreenBaseName(base)) {
                navigateTo("qrc:/SettingsMenu.qml")
            } else if (base === "AdditionalSettingsMenu.qml") {
                navigateTo("qrc:/SettingsMenu.qml")
            } else if (base === "ServiceMenu.qml") {
                serviceMenuAccessLevel = "full"
                navigateTo("qrc:/AdditionalSettingsMenu.qml")
            } else if (base === "SettingsMenu.qml") {
                if (closeOnServiceRootReturn) {
                    closeMe()
                } else {
                    navigateTo("qrc:/MainMenu.qml")
                }
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
