#include "RideShield/decision/fusion_engine.h"

#include <algorithm>

namespace RideShield::decision {

FusionEngine::FusionEngine()
    : weights_{}, thresholds_{} {}

FusionEngine::FusionEngine(Weights weights, Thresholds thresholds)
    : weights_(weights), thresholds_(thresholds) {}

auto FusionEngine::evaluate(const Input& input) -> core::FusionResult {
    core::FusionResult result;

    // 前向感知: 使用 TTC 驱动的连续风险分, 而非离散等级
    result.front_risk_score  = front_ttc_to_score(input.front);
    result.rear_risk_score   = risk_to_score(input.rear.risk);
    result.driver_risk_score = risk_to_score(input.driver.risk);
    result.imu_risk_score    = risk_to_score(input.imu.risk);
    result.hr_risk_score     = risk_to_score(input.hr.risk);

    // R = w1*R_front + w2*R_rear + w3*R_driver + w4*R_imu + w5*R_hr
    result.risk_score =
        weights_.front  * result.front_risk_score +
        weights_.rear   * result.rear_risk_score +
        weights_.driver * result.driver_risk_score +
        weights_.imu    * result.imu_risk_score +
        weights_.hr     * result.hr_risk_score;

    // 疲劳 + 心率异常同时出现 → 提升权重
    if (input.driver.risk >= core::RiskLevel::kWarning &&
        input.hr.risk >= core::RiskLevel::kWarning) {
        result.risk_score = std::min(1.f, result.risk_score * 1.3f);
    }

    // 分级决策
    if (result.risk_score >= thresholds_.emergency_threshold) {
        result.overall_risk = core::RiskLevel::kEmergency;
    } else if (result.risk_score >= thresholds_.warning_threshold) {
        result.overall_risk = core::RiskLevel::kWarning;
    } else if (result.risk_score >= thresholds_.hint_threshold) {
        result.overall_risk = core::RiskLevel::kHint;
    } else {
        result.overall_risk = core::RiskLevel::kNormal;
    }

    // 干预策略
    result.should_warn_voice  = result.overall_risk >= core::RiskLevel::kHint;
    result.should_warn_vibrate = result.overall_risk >= core::RiskLevel::kWarning;

    // 受控减速条件: 综合分极高 + IMU 无异常侧倾
    result.should_brake = result.risk_score >= thresholds_.brake_threshold
                       && !input.imu.abnormal;

    return result;
}

auto FusionEngine::risk_to_score(core::RiskLevel level) -> float {
    switch (level) {
    case core::RiskLevel::kNormal:    return 0.0f;
    case core::RiskLevel::kHint:      return 0.3f;
    case core::RiskLevel::kWarning:   return 0.6f;
    case core::RiskLevel::kEmergency: return 1.0f;
    }
    return 0.0f;
}

auto FusionEngine::front_ttc_to_score(const core::FrontPerceptionResult& front) -> float {
    // 基础: 离散风险等级分
    float base = risk_to_score(front.risk);

    // TTC 驱动的连续调制:
    // TTC > 6s        → score *= 0.3  (很远, 大幅降低)
    // TTC 3~6s        → score *= 0.5~1.0 线性插值
    // TTC 1.5~3s      → score *= 1.0  (保持)
    // TTC < 1.5s      → score = max(score, 0.9) (紧急兜底)
    float ttc = front.ttc_seconds;

    if (ttc >= 1e8f) {
        // 无碰撞风险 (静止/远离/极远)
        return base * 0.2f;
    }

    if (ttc > 6.f) {
        return base * 0.3f;
    } else if (ttc > 3.f) {
        float t = (ttc - 3.f) / 3.f; // 1.0 at 6s, 0.0 at 3s
        return base * (1.0f - 0.5f * t);
    } else if (ttc > 1.5f) {
        return base;
    } else {
        // 紧急 TTC → 至少 0.9
        return std::max(base, 0.9f);
    }
}

}  // namespace RideShield::decision
