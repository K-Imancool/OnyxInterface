import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: control
    property var editorRoot: null
    property bool isCoagSide: false
    property var sideState: ({})
    property color accentColor: "#F4D13D"
    property color accentTextColor: "black"

    readonly property bool modeSelected: editorRoot && editorRoot.modeSelectedInSide(isCoagSide)

    implicitHeight: editorRoot ? editorRoot.modeRowHeight : 76
    implicitWidth: 200

    Rectangle {
        anchors.fill: parent
        radius: editorRoot ? editorRoot.panelCornerRadius : 20
        color: accentColor
        border.width: 2
        border.color: isCoagSide ? "#083D8C" : "#D4B82E"
    }

    MouseArea {
        anchors.fill: parent
        onClicked: editorRoot.openModePicker(isCoagSide)
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 8
        // Для резания иконка справа, для коагуляции — слева.
        layoutDirection: isCoagSide ? Qt.LeftToRight : Qt.RightToLeft

        Image {
            Layout.fillHeight: true
            Layout.preferredWidth: control.height - 16
            Layout.maximumWidth: control.height - 16
            visible: modeSelected
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            source: modeSelected
                    ? ("image://modes/" + editorRoot.modeImagePrefix() + "%1").arg(sideState.modeId) : ""
        }

        Label {
            Layout.fillWidth: true
            Layout.fillHeight: true
            text: modeSelected ? sideState.modeName : qsTr("Выберите режим")
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            font.pixelSize: 38
            font.bold: true
            color: accentTextColor
        }
    }
}
