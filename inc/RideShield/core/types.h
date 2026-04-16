#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace RideShield::core {

// ---------- 检测结果 ----------

struct BoundingBox {
    float left{};
    float top{};
    float right{};
    float bottom{};

    [[nodiscard]] constexpr float width() const noexcept { return (right - left > 0.f) ? right - left : 0.f; }
    [[nodiscard]] constexpr float height() const noexcept { return (bottom - top > 0.f) ? bottom - top : 0.f; }
    [[nodiscard]] constexpr float center_x() const noexcept { return (left + right) * 0.5f; }
    [[nodiscard]] constexpr float center_y() const noexcept { return (top + bottom) * 0.5f; }
    [[nodiscard]] constexpr float area() const noexcept { return width() * height(); }
};

struct Detection {
    std::size_t class_id{};
    std::string class_name;
    float confidence{};
    BoundingBox bbox;
};

struct DetectionReport {
    std::vector<Detection> detections;
    float inference_ms{};
};

// ---------- 风险等级 (L0-L3) ----------

enum class RiskLevel {
    kNormal,    // L0 正常
    kHint,      // L1 提示
    kWarning,   // L2 预警
    kEmergency, // L3 紧急
};

// ---------- 运动趋势 ----------

enum class MotionTrend {
    kUnknown,      // 首帧/无法判断
    kReceding,     // 远离
    kStationary,   // 静止
    kApproaching,  // 接近 (低速)
    kClosingFast,  // 快速接近
};

// ---------- 后向区域 ----------

enum class RearZone {
    kLeftRear,
    kCenterRear,
    kRightRear,
};

// ---------- 感知输出 ----------

struct FrontPerceptionResult {
    DetectionReport report;
    RiskLevel risk{RiskLevel::kNormal};
    float ttc_seconds{1e9f};            // 碰撞时间估计 (seconds), 初值极大表示安全
    float approach_rate{0.f};           // bbox 底边增长率 (ratio/s), >0=接近
    MotionTrend motion{MotionTrend::kUnknown};
    float closest_distance_ratio{0.f};  // 最近目标的 bottom_ratio
};

struct RearPerceptionResult {
    DetectionReport report;
    RiskLevel risk{RiskLevel::kNormal};
    bool left_rear_occupied{};
    bool center_rear_occupied{};
    bool right_rear_occupied{};
};

struct DriverState {
    float fatigue_score{};
    float attention_score{1.f};
    bool eyes_closed{};
    bool yawning{};
    RiskLevel risk{RiskLevel::kNormal};
};

struct ImuState {
    float roll_deg{};
    float pitch_deg{};
    float angular_velocity_dps{};
    bool abnormal{};
    RiskLevel risk{RiskLevel::kNormal};
};

struct HeartRateState {
    float bpm{};
    bool abnormal{};
    RiskLevel risk{RiskLevel::kNormal};
};

// ---------- 融合决策输出 ----------

struct FusionResult {
    RiskLevel overall_risk{RiskLevel::kNormal};
    float risk_score{};

    float front_risk_score{};
    float rear_risk_score{};
    float driver_risk_score{};
    float imu_risk_score{};
    float hr_risk_score{};

    bool should_warn_voice{};
    bool should_warn_vibrate{};
    bool should_brake{};
};

}  // namespace RideShield::core
