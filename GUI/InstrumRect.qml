import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle {
    id: instrumRoot
    color: "transparent"

    property bool isCoag
    property string instrumName
    property int instrumNum
    property int modeId
    property bool isEndo: false

    signal instrumEditDialogRequest()

    Canvas {
        id: instrum
        anchors {
            top: parent.top
            bottom: parent.bottom
            right: parent.right
            left: parent.left
        }

        onPaint: {
            var ctx = getContext("2d");
            ctx.clearRect(0, 0, width, height);

            var radius = 20;
            var x = 0;
            var y = 0;
            var w = width;
            var h = height;

            ctx.beginPath();

            if (isCoag) {
                // Правый сокет - закруглен только верхний правый угол
                ctx.moveTo(x, y);
                ctx.lineTo(w - radius, y);
                ctx.arcTo(w, y, w, radius, radius);
                ctx.lineTo(w, h);
                ctx.lineTo(x, h);
                ctx.lineTo(x, y);
            } else {
                // Левый сокет - закруглен только верхний левый угол
                ctx.moveTo(x + radius, y);
                ctx.lineTo(w, y);
                ctx.lineTo(w, h);
                ctx.lineTo(x, h);
                ctx.arcTo(x, y, x + radius, y, radius);
            }

            ctx.closePath();
            ctx.fillStyle = "black";
            ctx.fill();
        }

        Image {
            id: instrImage
            asynchronous: true
            width: 120
            height: 120
            fillMode: Image.PreserveAspectFit
            source: "image://instruments/minstr" + instrumNum
            visible: (instrumNum != 1000)
            anchors {
                top: parent.top
                topMargin: 10
                bottom: undefined
                right: isCoag ? parent.right : undefined
                rightMargin: isCoag ? 10 : undefined
                left: isCoag ? undefined : parent.left
                leftMargin: isCoag ? undefined : 10
            }
        }

        Label {
            text: (instrumNum != 1000) ? instrumName : qsTr("Другой инструмент")
            font.pixelSize: 20
            font.bold: true
            wrapMode: Text.Wrap
            color: (instrumNum != 1000) ? "white" : "grey"
            horizontalAlignment: Qt.AlignHCenter
            verticalAlignment: Qt.AlignVCenter
            anchors.left: {
                if (isCoag || instrumNum === 1000)
                    return parent.left
                else
                    return instrImage.right
            }
            anchors.right: {
                if (!isCoag || instrumNum === 1000)
                    return parent.right
                else
                    return instrImage.left
            }
//            anchors.bottom: parent.bottom
            anchors.verticalCenter: parent.verticalCenter
            anchors.margins: 20
        }

        MouseArea {
            anchors.fill: parent
            anchors.leftMargin: 40
            anchors.rightMargin: 40
            anchors.topMargin: 20
            anchors.bottomMargin: 20
            enabled: modeId !== 1000  // Игнорируем клики, если режим не выбран
            onClicked: instrumRoot.instrumEditDialogRequest()
        }
    }
}
