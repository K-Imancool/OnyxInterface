import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: control
    property var editorRoot: null
    property bool isCoagSide: false
    property var sideState: ({})

    readonly property bool modeSelected: editorRoot && editorRoot.modeSelectedInSide(isCoagSide)
    readonly property bool instrSelected: editorRoot && editorRoot.instrumentSelectedInSide(isCoagSide)
    readonly property string displayName: instrSelected ? sideState.instrName : qsTr("Другой инструмент")
    readonly property int nameMaxFont: 30
    readonly property int nameMinFont: 16

    implicitHeight: editorRoot ? editorRoot.instrRowHeight : 104
    implicitWidth: 200

    Rectangle {
        anchors.fill: parent
        radius: editorRoot ? editorRoot.panelCornerRadius : 20
        color: "white"
        border.width: 2
        border.color: modeSelected ? editorRoot.fotekOrange : "#C7CEDA"
        opacity: modeSelected ? 1.0 : 0.65
    }

    MouseArea {
        anchors.fill: parent
        enabled: modeSelected
        onClicked: editorRoot.openInstrPicker(isCoagSide)
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 10
        layoutDirection: isCoagSide ? Qt.LeftToRight : Qt.RightToLeft

        Image {
            Layout.fillHeight: true
            Layout.preferredWidth: control.height - 16
            Layout.maximumWidth: control.height - 16
            visible: instrSelected
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            source: instrSelected ? editorRoot.instrumentIconSource(isCoagSide) : ""
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            // Скрытый измеритель: без maximumLineCount/elide, чтобы корректно
            // посчитать, влезает ли текст в 3 строки при данном размере шрифта.
            Text {
                id: nameMeasure
                visible: false
                width: parent.width
                text: control.displayName
                wrapMode: Text.WordWrap
                font.bold: true
            }

            Label {
                id: instrNameLabel
                anchors.fill: parent
                text: control.displayName
                wrapMode: Text.WordWrap
                elide: Text.ElideNone
                maximumLineCount: 3
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: control.nameMaxFont
                font.bold: true
                color: editorRoot ? editorRoot.fotekBlue : "#264093"
                clip: true

                function fitFontToBounds() {
                    if (parent.width <= 1 || parent.height <= 1)
                        return

                    nameMeasure.width = parent.width
                    var size = control.nameMaxFont
                    var fitted = control.nameMinFont

                    for (; size >= control.nameMinFont; --size) {
                        nameMeasure.font.pixelSize = size
                        // Три строки допустимы; уменьшаем шрифт только если
                        // нужно больше трёх строк или не хватает высоты.
                        if (nameMeasure.lineCount <= 3
                                && Math.ceil(nameMeasure.contentHeight) <= parent.height) {
                            fitted = size
                            break
                        }
                    }

                    font.pixelSize = fitted
                }

                onTextChanged: Qt.callLater(fitFontToBounds)
                onWidthChanged: Qt.callLater(fitFontToBounds)
                onHeightChanged: Qt.callLater(fitFontToBounds)
                Component.onCompleted: Qt.callLater(fitFontToBounds)
            }
        }
    }
}
