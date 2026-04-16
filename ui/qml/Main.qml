import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import RideShield.UI

ApplicationWindow {
    id: root
    width: 1280
    height: 720
    visible: true
    title: "RideShield"
    color: Theme.bgPrimary
    flags: Qt.Window

    // ── 全局字体 ──
    font.family: "Noto Sans SC, Source Han Sans CN, Microsoft YaHei, sans-serif"

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        StatusBar {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
        }

        // ── 主内容区 ──
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // 左侧 — 仪表盘
            DashboardPanel {
                Layout.fillHeight: true
                Layout.preferredWidth: root.width * 0.38
            }

            // 分割线
            Rectangle {
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                color: Theme.divider
            }

            // 右侧 — 感知信息
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 1

                DetectionPanel {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: Theme.divider
                }

                RearViewPanel {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 200
                }
            }
        }
    }
}
