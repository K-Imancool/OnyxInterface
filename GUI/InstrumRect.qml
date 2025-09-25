import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle {
    id: instrumRoot
    color: "transparent"

    property bool isCoag
    property string instrumName/*: "Биполярный лапороскопический инструмент"*/
    property int instrumId

    signal instrumEditDialogRequest()

    Canvas {
        id: instrumCanvas
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
            source: "image://instrums/miniInstr" + (instrumId+1)
//            fillMode: Image.PreserveAspectFit
//            anchors.fill: parent
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
            text: instrumName
            font.pixelSize: 20
            font.bold: true
            wrapMode: Text.Wrap
            color: "white"
            horizontalAlignment: Qt.AlignHCenter
            verticalAlignment: Qt.AlignVCenter
            anchors.left: isCoag ? parent.left : instrImage.right
            anchors.right: isCoag ? instrImage.left : parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 10
        }

        MouseArea {
            anchors.fill: parent
            onClicked: instrumRoot.instrumEditDialogRequest()
        }
    }

//    Rectangle {
//        id: instrImage
//        color: "black"
//        width: 0.3 * parent.width
//        radius: 8
//        // Закругление только верхних углов в зависимости от позиции
//        topLeftRadius: isCoag ? 0 : 8
//        topRightRadius: isCoag ? 8 : 0
//        bottomLeftRadius: 0
//        bottomRightRadius: 0
        
//        Image {
//            id: theImage
//            asynchronous: true
//            source: "image://instrums/miniInstr" + (instrumId+1)
//            fillMode: Image.PreserveAspectFit
//            anchors.fill: parent
//        }
//        anchors {
//            left: isCoag ? undefined : parent.left
//            right: isCoag ? parent.right : undefined
//            bottom: parent.bottom
//            top: parent.top
//        }
//    }
//    MouseArea {
//        anchors.fill: parent
//        onClicked: instrumRoot.instrumEditDialogRequest()
//    }
}
