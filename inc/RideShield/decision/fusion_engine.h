#pragma once

#include "RideShield/core/types.h"

namespace RideShield::decision {

/// 多模态融合决策引擎
/// R = w1*R_front + w2*R_rear + w3*R_driver + w4*R_imu + w5*R_hr
class FusionEngine {
public:
    struct Weights {
        float front  = 0.30f;
        float rear   = 0.20f;
        float driver = 0.25f;
        float imu    = 0.15f;
        float hr     = 0.10f;
    };

    struct Thresholds {
        float hint_threshold      = 0.20f; // R > 此值 → L1
        float warning_threshold   = 0.45f; // R > 此值 → L2
        float emergency_threshold = 0.70f; // R > 此值 → L3
        float brake_threshold     = 0.85f; // R > 此值 → 允许受控制动
    };

    FusionEngine();
    FusionEngine(Weights weights, Thresholds thresholds);

    struct Input {
        core::FrontPerceptionResult front;
        core::RearPerceptionResult rear;
        core::DriverState driver;
        core::ImuState imu;
        core::HeartRateState hr;
    };

    auto evaluate(const Input& input) -> core::FusionResult;

    void set_weights(Weights weights) { weights_ = weights; }
    void set_thresholds(Thresholds thresholds) { thresholds_ = thresholds; }
    [[nodiscard]] auto weights() const -> const Weights& { return weights_; }
    [[nodiscard]] auto thresholds() const -> const Thresholds& { return thresholds_; }

private:
    Weights weights_;
    Thresholds thresholds_;

    static auto risk_to_score(core::RiskLevel level) -> float;
};

}  // namespace RideShield::decision
