pragma Singleton
import QtQuick

/// 全局主题色彩常量 —— 扁平化深色车机风格
QtObject {
    // ── 背景 ──
    readonly property color bgPrimary:   "#1a1a2e"
    readonly property color bgSecondary: "#16213e"
    readonly property color bgCard:      "#0f3460"

    // ── 前景 ──
    readonly property color textPrimary:   "#e8e8e8"
    readonly property color textSecondary: "#8899aa"
    readonly property color textMuted:     "#556677"

    // ── 强调色 ──
    readonly property color accentBlue:   "#00adb5"
    readonly property color accentGreen:  "#2ecc71"
    readonly property color accentYellow: "#f1c40f"
    readonly property color accentOrange: "#e67e22"
    readonly property color accentRed:    "#e74c3c"

    // ── 分割线 ──
    readonly property color divider: "#2a2a4a"

    // ── 风险等级对应颜色 ──
    function riskColor(level: int): color {
        switch (level) {
        case 0:  return accentGreen
        case 1:  return accentYellow
        case 2:  return accentOrange
        case 3:  return accentRed
        default: return textSecondary
        }
    }

    // ── 字号 ──
    readonly property int fontHuge:   48
    readonly property int fontLarge:  24
    readonly property int fontMedium: 16
    readonly property int fontSmall:  13
    readonly property int fontTiny:   11

    // ── 圆角 ──
    readonly property int radiusSmall:  6
    readonly property int radiusMedium: 12
    readonly property int radiusLarge:  20
}
