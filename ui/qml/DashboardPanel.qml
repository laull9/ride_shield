import QtQuick
import QtQuick.Layouts
import QtQuick.Shapes
import RideShield.UI

/// 左侧仪表盘面板 —— 速度环 + 风险弧 + 关键数值
Rectangle {
    id: panel
    color: Theme.bgPrimary

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 20

        // ── 速度表盘 ──
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.alignment: Qt.AlignHCenter

            // 弧形背景
            Shape {
                anchors.centerIn: parent
                width: 260; height: 260

                ShapePath {
                    fillColor: "transparent"
                    strokeColor: Theme.divider
                    strokeWidth: 10
                    capStyle: ShapePath.RoundCap

                    PathAngleArc {
                        centerX: 130; centerY: 130
                        radiusX: 110; radiusY: 110
                        startAngle: 135; sweepAngle: 270
                    }
                }

                // 风险弧（按 riskScore 0..1 填充）
                ShapePath {
                    fillColor: "transparent"
                    strokeColor: Theme.riskColor(VehicleBridge.riskLevel)
                    strokeWidth: 10
                    capStyle: ShapePath.RoundCap

                    PathAngleArc {
                        centerX: 130; centerY: 130
                        radiusX: 110; radiusY: 110
                        startAngle: 135
                        sweepAngle: 270 * Math.min(VehicleBridge.riskScore, 1.0)
                    }
                }
            }

            // 速度数值
            Column {
                anchors.centerIn: parent
                spacing: 2

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: VehicleBridge.speedAvailable ? Math.round(VehicleBridge.speed).toString() : "—"
                    font.pixelSize: Theme.fontHuge
                    font.bold: true
                    color: Theme.textPrimary
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "km/h"
                    font.pixelSize: Theme.fontSmall
                    color: Theme.textSecondary
                }
            }
        }

        // ── 关键指标网格 ──
        GridLayout {
            Layout.fillWidth: true
            columns: 2
            rowSpacing: 12
            columnSpacing: 16

            MetricCard {
                Layout.fillWidth: true
                label: "风险评分"
                value: VehicleBridge.riskScore.toFixed(2)
                accent: Theme.riskColor(VehicleBridge.riskLevel)
            }

            MetricCard {
                Layout.fillWidth: true
                label: "TTC (秒)"
                value: VehicleBridge.ttcSeconds < 50 ? VehicleBridge.ttcSeconds.toFixed(1) : "—"
                accent: VehicleBridge.ttcSeconds < 2.0 ? Theme.accentRed
                      : VehicleBridge.ttcSeconds < 4.0 ? Theme.accentOrange
                      : Theme.accentGreen
            }

            MetricCard {
                Layout.fillWidth: true
                label: "驾驶员疲劳"
                value: VehicleBridge.driverAvailable ? (VehicleBridge.driverFatigueScore * 100).toFixed(0) + "%" : "—"
                accent: !VehicleBridge.driverAvailable ? Theme.textMuted
                      : VehicleBridge.driverFatigueScore > 0.6 ? Theme.accentRed : Theme.accentGreen
            }

            MetricCard {
                Layout.fillWidth: true
                label: "横向 G"
                value: VehicleBridge.imuAvailable ? VehicleBridge.imuLateralG.toFixed(2) + " g" : "—"
                accent: !VehicleBridge.imuAvailable ? Theme.textMuted
                      : Math.abs(VehicleBridge.imuLateralG) > 0.4 ? Theme.accentOrange : Theme.accentBlue
            }
        }

        // ── 制动许可 ──
        Rectangle {
            Layout.fillWidth: true
            height: 40
            radius: Theme.radiusSmall
            color: VehicleBridge.brakeAllowed
                   ? Qt.rgba(Theme.accentRed.r, Theme.accentRed.g, Theme.accentRed.b, 0.15)
                   : Qt.rgba(Theme.accentGreen.r, Theme.accentGreen.g, Theme.accentGreen.b, 0.10)

            Text {
                anchors.centerIn: parent
                text: VehicleBridge.brakeAllowed ? "⚠ 受控制动已启用" : "✓ 常规行驶"
                font.pixelSize: Theme.fontSmall
                font.bold: true
                color: VehicleBridge.brakeAllowed ? Theme.accentRed : Theme.accentGreen
            }
        }
    }
}
