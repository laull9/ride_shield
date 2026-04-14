#include "RideShield/decision/fusion_engine.h"

#include <algorithm>

namespace RideShield::decision {

FusionEngine::FusionEngine()
    : weights_{}, thresholds_{} {}

FusionEngine::FusionEngine(Weights weights, Thresholds thresholds)
    : weights_(weights), thresholds_(thresholds) {}

auto FusionEngine::evaluate(const Input& input) -> core::FusionResult {
    core::FusionResult result;

    result.front_risk_score  = risk_to_score(input.front.risk);
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

}  // namespace RideShield::decision
