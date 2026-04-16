import QtQuick
import RideShield.UI

/// 后向盲区单区域指示器
Rectangle {
    id: indicator

    property bool occupied: false
    property string label: ""

    width: 72; height: 72
    radius: Theme.radiusSmall
    color: occupied
           ? Qt.rgba(Theme.accentRed.r, Theme.accentRed.g, Theme.accentRed.b, 0.20)
           : Qt.rgba(Theme.accentGreen.r, Theme.accentGreen.g, Theme.accentGreen.b, 0.10)

    border.color: occupied ? Theme.accentRed : Theme.accentGreen
    border.width: 2

    Column {
        anchors.centerIn: parent
        spacing: 4

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 12; height: 12; radius: 6
            color: indicator.occupied ? Theme.accentRed : Theme.accentGreen

            SequentialAnimation on opacity {
                running: indicator.occupied
                loops: Animation.Infinite
                NumberAnimation { to: 0.3; duration: 500; easing.type: Easing.InOutQuad }
                NumberAnimation { to: 1.0; duration: 500; easing.type: Easing.InOutQuad }
            }
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: indicator.label
            font.pixelSize: Theme.fontTiny
            color: indicator.occupied ? Theme.accentRed : Theme.textSecondary
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: indicator.occupied ? "占用" : "空闲"
            font.pixelSize: Theme.fontTiny
            font.bold: true
            color: indicator.occupied ? Theme.accentRed : Theme.accentGreen
        }
    }
}
