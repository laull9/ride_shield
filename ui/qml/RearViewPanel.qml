import QtQuick
import QtQuick.Layouts
import RideShield.UI

/// 后向感知面板 —— 三区域盲区占用可视化
Rectangle {
    id: panel
    color: Theme.bgPrimary

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        Text {
            text: "后向盲区监测"
            font.pixelSize: Theme.fontMedium
            font.bold: true
            color: Theme.textPrimary
        }

        // 不可用提示
        Text {
            visible: !VehicleBridge.rearAvailable
            Layout.fillWidth: true
            Layout.fillHeight: true
            text: "后向摄像头未接入"
            font.pixelSize: Theme.fontSmall
            color: Theme.textMuted
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        // 俯视示意图
        Item {
            visible: VehicleBridge.rearAvailable
            Layout.fillWidth: true
            Layout.fillHeight: true

            // 车身轮廓
            Rectangle {
                id: carBody
                anchors.centerIn: parent
                width: 80; height: 120
                radius: Theme.radiusMedium
                color: Theme.bgCard
                border.color: Theme.accentBlue
                border.width: 2

                Text {
                    anchors.centerIn: parent
                    text: "车辆"
                    font.pixelSize: Theme.fontTiny
                    color: Theme.textSecondary
                }
            }

            // 左后区域
            ZoneIndicator {
                anchors.right: carBody.left
                anchors.rightMargin: 12
                anchors.verticalCenter: carBody.verticalCenter
                anchors.verticalCenterOffset: 20
                occupied: VehicleBridge.leftRearOccupied
                label: "左后"
            }

            // 正后区域
            ZoneIndicator {
                anchors.horizontalCenter: carBody.horizontalCenter
                anchors.top: carBody.bottom
                anchors.topMargin: 12
                occupied: VehicleBridge.centerRearOccupied
                label: "正后"
            }

            // 右后区域
            ZoneIndicator {
                anchors.left: carBody.right
                anchors.leftMargin: 12
                anchors.verticalCenter: carBody.verticalCenter
                anchors.verticalCenterOffset: 20
                occupied: VehicleBridge.rightRearOccupied
                label: "右后"
            }
        }
    }
}
