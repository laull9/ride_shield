#pragma once

#include "RideShield/core/types.h"

#ifdef RIDESHIELD_HAS_ONNXRUNTIME
#include "RideShield/inference/yolo_detector.h"
#endif

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace RideShield::perception {

#ifdef RIDESHIELD_HAS_ONNXRUNTIME

/// 前向环境感知模块
/// - YOLO 目标检测 (车辆、行人、非机动车等)
/// - 时序跟踪 + 动态 TTC 估计 (基于 bbox 增长率)
/// - 基于距离 + 运动趋势的分级风险 (正常/提示/预警/紧急)
class FrontPerception {
public:
    struct Config {
        float ttc_warn_seconds      = 3.0f;  // TTC < 此值 → 预警
        float ttc_emergency_seconds = 1.5f;  // TTC < 此值 → 紧急
        float near_bbox_ratio       = 0.35f; // bbox 底边占画面比例 > 此值 → 视为较近
        float danger_bbox_ratio     = 0.55f; // bbox 底边占画面比例 > 此值 → 视为危险近

        // 运动趋势阈值 (bottom_ratio 变化率, 单位: ratio/s)
        float stationary_rate_threshold  = 0.02f; // |rate| < 此值视为静止
        float closing_fast_rate_threshold = 0.15f; // rate > 此值视为快速接近

        // 跟踪参数
        float iou_match_threshold   = 0.20f; // IoU > 此值视为同一目标
        int   max_lost_frames       = 5;     // 目标丢失超过此帧数则删除
        int   min_track_frames      = 3;     // 至少跟踪此帧数才启用动态 TTC
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
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    struct TrackedObject {
        uint32_t id{};
        std::size_t class_id{};
        core::BoundingBox bbox;
        float bottom_ratio{};       // 当前帧 bbox.bottom / image_height
        float approach_rate{};      // bottom_ratio 增长率 (ratio/s)
        core::MotionTrend motion{core::MotionTrend::kUnknown};
        int lost_frames{};          // 连续未匹配帧数
        int total_frames{1};        // 总跟踪帧数

        // 滑动窗口: 最近 N 帧的 (timestamp, bottom_ratio)
        static constexpr int kHistorySize = 8;
        struct Sample { TimePoint time; float bottom_ratio; };
        std::vector<Sample> history;
    };

    Config config_;
    inference::YoloDetector detector_;
    std::unordered_map<uint32_t, TrackedObject> tracks_;
    uint32_t next_track_id_{1};

    struct RiskAssessment {
        core::RiskLevel risk;
        float ttc;
        float approach_rate;
        core::MotionTrend motion;
        float closest_distance_ratio;
    };

    auto evaluate_risk(const core::DetectionReport& report, int image_height) -> RiskAssessment;
    void update_tracks(const core::DetectionReport& report, int image_height, TimePoint now);
    void purge_lost_tracks();
    static auto compute_iou(const core::BoundingBox& a, const core::BoundingBox& b) -> float;
    void compute_approach_rate(TrackedObject& track);
    auto compute_dynamic_ttc(const TrackedObject& track) -> float;
    auto assess_object_risk(const TrackedObject& track) -> core::RiskLevel;
};

#endif

}  // namespace RideShield::perception
