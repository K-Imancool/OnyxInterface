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

        Image {
            Layout.fillHeight: true
            Layout.preferredWidth: control.height - 16
            Layout.maximumWidth: control.height - 16
            visible: instrSelected
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            source: instrSelected ? editorRoot.instrumentIconSource(isCoagSide) : ""
        }

        Label {
            Layout.fillWidth: true
            Layout.fillHeight: true
            text: instrSelected ? sideState.instrName : qsTr("Выберите инструмент")
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            font.pixelSize: 24
            font.bold: true
            color: editorRoot ? editorRoot.fotekBlue : "#264093"
        }
    }
}
