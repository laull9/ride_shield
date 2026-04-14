#include "RideShield/perception/rear_perception.h"

#ifdef RIDESHIELD_HAS_ONNXRUNTIME

#include <algorithm>

namespace RideShield::perception {

// 关注的后向目标类别 (COCO class_id)
// 0=人 1=自行车 2=汽车 3=摩托车 5=公交车 7=卡车
static constexpr std::array<std::size_t, 6> kRearTargetClasses = {0, 1, 2, 3, 5, 7};

static bool is_rear_target(std::size_t class_id) {
    return std::ranges::find(kRearTargetClasses, class_id) != kRearTargetClasses.end();
}

RearPerception::RearPerception(inference::YoloDetectorConfig detector_config)
    : RearPerception(std::move(detector_config), Config{}) {}

RearPerception::RearPerception(inference::YoloDetectorConfig detector_config, Config config)
    : config_(config),
      detector_(std::move(detector_config)) {}

auto RearPerception::process(const core::ImageView& image) -> core::RearPerceptionResult {
    auto report = detector_.detect(image);
    auto result = evaluate_risk(report, image.width(), image.height());
    result.report = std::move(report);
    return result;
}

#ifdef RIDESHIELD_HAS_OPENCV
auto RearPerception::process(const cv::Mat& bgr_frame) -> core::RearPerceptionResult {
    auto report = detector_.detect(bgr_frame);
    auto result = evaluate_risk(report, bgr_frame.cols, bgr_frame.rows);
    result.report = std::move(report);
    return result;
}
#endif

auto RearPerception::evaluate_risk(
    const core::DetectionReport& report,
    int image_width, int image_height
) -> core::RearPerceptionResult {
    core::RearPerceptionResult result;

    if (report.detections.empty() || image_width <= 0 || image_height <= 0) {
        return result;
    }

    const float w = static_cast<float>(image_width);
    const float image_area = static_cast<float>(image_width) * static_cast<float>(image_height);
    const float left_boundary = w * config_.zone_left_threshold;
    const float right_boundary = w * config_.zone_right_threshold;

    core::RiskLevel max_risk = core::RiskLevel::kNormal;

    for (const auto& det : report.detections) {
        if (!is_rear_target(det.class_id)) {
            continue;
        }

        if (det.confidence < config_.min_confidence) {
            continue;
        }

        // 区域判断: bbox 中心点所在区域
        const float cx = det.bbox.center_x();
        if (cx < left_boundary) {
            result.left_rear_occupied = true;
        } else if (cx > right_boundary) {
            result.right_rear_occupied = true;
        } else {
            result.center_rear_occupied = true;
        }

        // 面积比 → 近距判断
        const float area_ratio = det.bbox.area() / image_area;
        core::RiskLevel det_risk = core::RiskLevel::kNormal;

        if (area_ratio >= config_.danger_bbox_ratio) {
            det_risk = core::RiskLevel::kEmergency;
        } else if (area_ratio >= config_.close_bbox_ratio) {
            det_risk = core::RiskLevel::kWarning;
        } else if (area_ratio >= config_.close_bbox_ratio * 0.5f) {
            det_risk = core::RiskLevel::kHint;
        }

        if (static_cast<int>(det_risk) > static_cast<int>(max_risk)) {
            max_risk = det_risk;
        }
    }

    result.risk = max_risk;
    return result;
}

}  // namespace RideShield::perception

#endif
