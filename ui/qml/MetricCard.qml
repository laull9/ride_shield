import QtQuick
import RideShield.UI

/// 单个指标卡片 —— 扁平化样式
Rectangle {
    id: card

    property string label: ""
    property string value: ""
    property color accent: Theme.accentBlue

    implicitHeight: 64
    radius: Theme.radiusSmall
    color: Theme.bgSecondary

    Column {
        anchors.centerIn: parent
        spacing: 2

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: card.value
            font.pixelSize: Theme.fontLarge
            font.bold: true
            color: card.accent
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: card.label
            font.pixelSize: Theme.fontTiny
            color: Theme.textSecondary
        }
    }
}
