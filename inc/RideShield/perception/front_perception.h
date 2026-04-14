#pragma once

#include "RideShield/core/types.h"

#ifdef RIDESHIELD_HAS_ONNXRUNTIME
#include "RideShield/inference/yolo_detector.h"
#endif

#include <memory>
#include <optional>

namespace RideShield::perception {

#ifdef RIDESHIELD_HAS_ONNXRUNTIME

/// 前向环境感知模块
/// - YOLO 目标检测 (车辆、行人、非机动车等)
/// - 简易 TTC 估计 (基于 bbox 底边位置→距离代理)
/// - 输出四级风险 (正常/提示/预警/紧急)
class FrontPerception {
public:
    struct Config {
        float ttc_warn_seconds      = 3.0f; // TTC < 此值 → 预警
        float ttc_emergency_seconds = 1.5f; // TTC < 此值 → 紧急
        float near_bbox_ratio       = 0.35f; // bbox 底边占画面比例 > 此值 → 视为较近
        float danger_bbox_ratio     = 0.55f; // bbox 底边占画面比例 > 此值 → 视为危险近
    };

    FrontPerception(inference::YoloDetectorConfig detector_config, Config config);
    explicit FrontPerception(inference::YoloDetectorConfig detector_config);

    auto process(const core::ImageView& image) -> core::FrontPerceptionResult;

#ifdef RIDESHIELD_HAS_OPENCV
    auto process(const cv::Mat& bgr_frame) -> core::FrontPerceptionResult;
#endif

    [[nodiscard]] auto config() const -> const Config& { return config_; }
    [[nodiscard]] auto detector() -> inference::YoloDetector& { return detector_; }

private:
    Config config_;
    inference::YoloDetector detector_;

    auto evaluate_risk(const core::DetectionReport& report, int image_height) -> std::pair<core::RiskLevel, float>;
};

#endif

}  // namespace RideShield::perception
