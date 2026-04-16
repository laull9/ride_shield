import QtQuick
import QtQuick.Layouts
import RideShield.UI

/// 前向感知面板 —— 检测目标列表
Rectangle {
    id: panel
    color: Theme.bgPrimary

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10

        // 标题行
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: "前向感知"
                font.pixelSize: Theme.fontMedium
                font.bold: true
                color: Theme.textPrimary
            }

            Rectangle {
                width: countText.implicitWidth + 12
                height: 22
                radius: 11
                color: Theme.accentBlue

                Text {
                    id: countText
                    anchors.centerIn: parent
                    text: VehicleBridge.frontDetectionCount
                    font.pixelSize: Theme.fontTiny
                    font.bold: true
                    color: "#ffffff"
                }
            }

            Item { Layout.fillWidth: true }

            Text {
                text: "接近速率 " + VehicleBridge.approachRate.toFixed(2)
                font.pixelSize: Theme.fontSmall
                color: VehicleBridge.approachRate > 0.05 ? Theme.accentOrange : Theme.textSecondary
            }
        }

        // 目标列表
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: VehicleBridge.frontDetections
            spacing: 4

            delegate: Rectangle {
                width: ListView.view.width
                height: 44
                radius: Theme.radiusSmall
                color: Theme.bgSecondary

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 10

                    // 类别色块
                    Rectangle {
                        width: 4; height: 28
                        radius: 2
                        color: {
                            var conf = modelData.confidence || 0
                            return conf > 0.7 ? Theme.accentGreen
                                 : conf > 0.4 ? Theme.accentYellow
                                 : Theme.accentRed
                        }
                    }

                    Text {
                        text: modelData.className || "obj"
                        font.pixelSize: Theme.fontSmall
                        font.bold: true
                        color: Theme.textPrimary
                        Layout.preferredWidth: 80
                    }

                    Text {
                        text: ((modelData.confidence || 0) * 100).toFixed(0) + "%"
                        font.pixelSize: Theme.fontSmall
                        color: Theme.textSecondary
                    }

                    Item { Layout.fillWidth: true }

                    Text {
                        text: "bbox " + (modelData.left || 0).toFixed(0) + "," +
                              (modelData.top || 0).toFixed(0)
                        font.pixelSize: Theme.fontTiny
                        color: Theme.textMuted
                    }
                }
            }

            // 空状态
            Text {
                anchors.centerIn: parent
                visible: VehicleBridge.frontDetectionCount === 0
                text: "暂无前方目标"
                font.pixelSize: Theme.fontSmall
                color: Theme.textMuted
            }
        }
    }
}
