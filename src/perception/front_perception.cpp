#include "RideShield/perception/front_perception.h"

#ifdef RIDESHIELD_HAS_ONNXRUNTIME

#include <algorithm>
#include <cmath>

namespace RideShield::perception {

// 关注的前向目标类别 (COCO class_id)
// 0=人 1=自行车 2=汽车 3=摩托车 5=公交车 7=卡车
static constexpr std::array<std::size_t, 6> kFrontTargetClasses = {0, 1, 2, 3, 5, 7};

static bool is_front_target(std::size_t class_id) {
    return std::ranges::find(kFrontTargetClasses, class_id) != kFrontTargetClasses.end();
}

FrontPerception::FrontPerception(inference::YoloDetectorConfig detector_config)
    : FrontPerception(std::move(detector_config), Config{}) {}

FrontPerception::FrontPerception(inference::YoloDetectorConfig detector_config, Config config)
    : config_(config),
      detector_(std::move(detector_config)) {}

auto FrontPerception::process(const core::ImageView& image) -> core::FrontPerceptionResult {
    auto report = detector_.detect(image);
    auto [risk, ttc] = evaluate_risk(report, image.height());

    return core::FrontPerceptionResult{
        .report = std::move(report),
        .risk = risk,
        .ttc_seconds = ttc,
    };
}

#ifdef RIDESHIELD_HAS_OPENCV
auto FrontPerception::process(const cv::Mat& bgr_frame) -> core::FrontPerceptionResult {
    auto report = detector_.detect(bgr_frame);
    auto [risk, ttc] = evaluate_risk(report, bgr_frame.rows);

    return core::FrontPerceptionResult{
        .report = std::move(report),
        .risk = risk,
        .ttc_seconds = ttc,
    };
}
#endif

auto FrontPerception::evaluate_risk(const core::DetectionReport& report, int image_height)
    -> std::pair<core::RiskLevel, float> {
    if (report.detections.empty() || image_height <= 0) {
        return {core::RiskLevel::kNormal, 1e9f};
    }

    const auto h = static_cast<float>(image_height);
    float min_ttc_proxy = 1e9f;

    for (const auto& det : report.detections) {
        if (!is_front_target(det.class_id)) {
            continue;
        }

        // 简易距离代理: bbox 底边越接近画面底部 → 越近
        // bottom_ratio ∈ [0, 1], 越大越近
        const float bottom_ratio = det.bbox.bottom / h;

        // 简易 TTC 代理 (基于空间位置，非时序跟踪)
        // 未来可接入时序 tracker 用 bbox 增长率估计真实 TTC
        float ttc_proxy;
        if (bottom_ratio >= config_.danger_bbox_ratio) {
            ttc_proxy = config_.ttc_emergency_seconds * 0.8f;
        } else if (bottom_ratio >= config_.near_bbox_ratio) {
            // 线性插值
            const float t = (bottom_ratio - config_.near_bbox_ratio)
                          / (config_.danger_bbox_ratio - config_.near_bbox_ratio);
            ttc_proxy = config_.ttc_warn_seconds * (1.f - t)
                      + config_.ttc_emergency_seconds * t;
        } else {
            ttc_proxy = 1e9f;
        }

        min_ttc_proxy = std::min(min_ttc_proxy, ttc_proxy);
    }

    core::RiskLevel risk = core::RiskLevel::kNormal;
    if (min_ttc_proxy < config_.ttc_emergency_seconds) {
        risk = core::RiskLevel::kEmergency;
    } else if (min_ttc_proxy < config_.ttc_warn_seconds) {
        risk = core::RiskLevel::kWarning;
    } else if (min_ttc_proxy < config_.ttc_warn_seconds * 2.f) {
        risk = core::RiskLevel::kHint;
    }

    return {risk, min_ttc_proxy};
}

}  // namespace RideShield::perception

#endif
