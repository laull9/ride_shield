#pragma once

#include "RideShield/core/types.h"

#ifdef RIDESHIELD_HAS_ONNXRUNTIME
#include "RideShield/inference/yolo_detector.h"
#endif

namespace RideShield::perception {

#ifdef RIDESHIELD_HAS_ONNXRUNTIME

/// 后向盲区感知模块
/// - YOLO 目标检测 (后向鱼眼摄像头)
/// - 左后 / 正后 / 右后 三区域占用判断
/// - 输出盲区预警和追尾风险等级
class RearPerception {
public:
    struct Config {
        float zone_left_threshold  = 0.33f; // 画面左 1/3 为左后区域
        float zone_right_threshold = 0.67f; // 画面右 1/3 为右后区域
        float close_bbox_ratio     = 0.25f; // bbox 面积占画面比例 > 此值 → 视为近距来车
        float danger_bbox_ratio    = 0.40f; // bbox 面积占画面比例 > 此值 → 视为紧急
        float min_confidence       = 0.30f; // 后向检测额外置信度阈值
    };

    RearPerception(inference::YoloDetectorConfig detector_config, Config config);
    explicit RearPerception(inference::YoloDetectorConfig detector_config);

    auto process(const core::ImageView& image) -> core::RearPerceptionResult;

#ifdef RIDESHIELD_HAS_OPENCV
    auto process(const cv::Mat& bgr_frame) -> core::RearPerceptionResult;
#endif

    [[nodiscard]] auto config() const -> const Config& { return config_; }
    [[nodiscard]] auto detector() -> inference::YoloDetector& { return detector_; }

private:
    Config config_;
    inference::YoloDetector detector_;

    auto evaluate_risk(const core::DetectionReport& report, int image_width, int image_height)
        -> core::RearPerceptionResult;
};

#endif

}  // namespace RideShield::perception
