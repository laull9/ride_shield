import QtQuick
import QtQuick.Layouts
import RideShield.UI

/// 顶部状态栏 —— 时间、连接状态、风险摘要
Rectangle {
    id: bar
    color: Theme.bgSecondary

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 20
        anchors.rightMargin: 20
        spacing: 16

        // 品牌标识
        Text {
            text: "RideShield"
            font.pixelSize: Theme.fontMedium
            font.bold: true
            color: Theme.accentBlue
        }

        Text {
            text: "│"
            color: Theme.divider
            font.pixelSize: Theme.fontMedium
        }

        // 风险概览
        Rectangle {
            width: riskRow.implicitWidth + 20
            height: 28
            radius: Theme.radiusSmall
            color: Qt.rgba(Theme.riskColor(VehicleBridge.riskLevel).r,
                           Theme.riskColor(VehicleBridge.riskLevel).g,
                           Theme.riskColor(VehicleBridge.riskLevel).b, 0.18)

            Row {
                id: riskRow
                anchors.centerIn: parent
                spacing: 6

                Rectangle {
                    width: 8; height: 8; radius: 4
                    anchors.verticalCenter: parent.verticalCenter
                    color: Theme.riskColor(VehicleBridge.riskLevel)
                }

                Text {
                    text: VehicleBridge.riskText
                    font.pixelSize: Theme.fontSmall
                    font.bold: true
                    color: Theme.riskColor(VehicleBridge.riskLevel)
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }

        Item { Layout.fillWidth: true }

        // 心率
        Row {
            spacing: 4
            Text {
                text: "♥"
                font.pixelSize: Theme.fontSmall
                color: VehicleBridge.hrAvailable ? Theme.accentRed : Theme.textMuted
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                text: VehicleBridge.hrAvailable ? VehicleBridge.heartRate + " bpm" : "—"
                font.pixelSize: Theme.fontSmall
                color: Theme.textSecondary
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        // 时间
        Text {
            id: clock
            font.pixelSize: Theme.fontSmall
            color: Theme.textSecondary

            Timer {
                interval: 1000; running: true; repeat: true
                onTriggered: clock.text = Qt.formatDateTime(new Date(), "HH:mm")
            }

            Component.onCompleted: text = Qt.formatDateTime(new Date(), "HH:mm")
        }
    }
}
