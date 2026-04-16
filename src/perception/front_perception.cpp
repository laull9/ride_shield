#include "RideShield/perception/front_perception.h"

#ifdef RIDESHIELD_HAS_ONNXRUNTIME

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

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
    auto assessment = evaluate_risk(report, image.height());

    return core::FrontPerceptionResult{
        .report = std::move(report),
        .risk = assessment.risk,
        .ttc_seconds = assessment.ttc,
        .approach_rate = assessment.approach_rate,
        .motion = assessment.motion,
        .closest_distance_ratio = assessment.closest_distance_ratio,
    };
}

#ifdef RIDESHIELD_HAS_OPENCV
auto FrontPerception::process(const cv::Mat& bgr_frame) -> core::FrontPerceptionResult {
    auto report = detector_.detect(bgr_frame);
    auto assessment = evaluate_risk(report, bgr_frame.rows);

    return core::FrontPerceptionResult{
        .report = std::move(report),
        .risk = assessment.risk,
        .ttc_seconds = assessment.ttc,
        .approach_rate = assessment.approach_rate,
        .motion = assessment.motion,
        .closest_distance_ratio = assessment.closest_distance_ratio,
    };
}
#endif

// ---- IoU 计算 ----

auto FrontPerception::compute_iou(const core::BoundingBox& a, const core::BoundingBox& b) -> float {
    float inter_left   = std::max(a.left, b.left);
    float inter_top    = std::max(a.top, b.top);
    float inter_right  = std::min(a.right, b.right);
    float inter_bottom = std::min(a.bottom, b.bottom);

    float inter_w = std::max(0.f, inter_right - inter_left);
    float inter_h = std::max(0.f, inter_bottom - inter_top);
    float inter_area = inter_w * inter_h;

    float union_area = a.area() + b.area() - inter_area;
    if (union_area <= 0.f) return 0.f;
    return inter_area / union_area;
}

// ---- 跟踪更新 ----

void FrontPerception::update_tracks(const core::DetectionReport& report, int image_height, TimePoint now) {
    const float h = static_cast<float>(image_height);

    // 标记所有现有 track 为本帧未匹配
    for (auto& [id, track] : tracks_) {
        track.lost_frames++;
    }

    // 对每个新检测, 找 IoU 最高的已有 track 进行匹配
    std::vector<bool> det_matched(report.detections.size(), false);

    for (std::size_t i = 0; i < report.detections.size(); ++i) {
        const auto& det = report.detections[i];
        if (!is_front_target(det.class_id)) continue;

        float best_iou = 0.f;
        uint32_t best_id = 0;

        for (auto& [id, track] : tracks_) {
            if (track.class_id != det.class_id) continue;
            float iou = compute_iou(det.bbox, track.bbox);
            if (iou > best_iou) {
                best_iou = iou;
                best_id = id;
            }
        }

        if (best_iou >= config_.iou_match_threshold && best_id != 0) {
            // 匹配成功 → 更新已有 track
            auto& track = tracks_[best_id];
            track.bbox = det.bbox;
            track.bottom_ratio = det.bbox.bottom / h;
            track.lost_frames = 0;
            track.total_frames++;

            track.history.push_back({now, track.bottom_ratio});
            if (static_cast<int>(track.history.size()) > TrackedObject::kHistorySize) {
                track.history.erase(track.history.begin());
            }

            compute_approach_rate(track);
            det_matched[i] = true;
        }
    }

    // 未匹配的检测 → 创建新 track
    for (std::size_t i = 0; i < report.detections.size(); ++i) {
        if (det_matched[i]) continue;
        const auto& det = report.detections[i];
        if (!is_front_target(det.class_id)) continue;

        TrackedObject track;
        track.id = next_track_id_++;
        track.class_id = det.class_id;
        track.bbox = det.bbox;
        track.bottom_ratio = det.bbox.bottom / h;
        track.lost_frames = 0;
        track.total_frames = 1;
        track.motion = core::MotionTrend::kUnknown;
        track.approach_rate = 0.f;
        track.history.push_back({now, track.bottom_ratio});

        tracks_[track.id] = std::move(track);
    }
}

void FrontPerception::purge_lost_tracks() {
    for (auto it = tracks_.begin(); it != tracks_.end(); ) {
        if (it->second.lost_frames > config_.max_lost_frames) {
            it = tracks_.erase(it);
        } else {
            ++it;
        }
    }
}

// ---- 运动速率计算 (线性回归 bottom_ratio vs time) ----

void FrontPerception::compute_approach_rate(TrackedObject& track) {
    if (track.history.size() < 2) {
        track.approach_rate = 0.f;
        track.motion = core::MotionTrend::kUnknown;
        return;
    }

    // 使用最近的样本做线性回归: bottom_ratio = a*t + b
    // approach_rate = a (ratio/s)
    const auto& samples = track.history;
    const auto t0 = samples.front().time;
    int n = static_cast<int>(samples.size());

    float sum_t = 0, sum_r = 0, sum_tt = 0, sum_tr = 0;
    for (const auto& s : samples) {
        float t = std::chrono::duration<float>(s.time - t0).count();
        sum_t  += t;
        sum_r  += s.bottom_ratio;
        sum_tt += t * t;
        sum_tr += t * s.bottom_ratio;
    }

    float denom = static_cast<float>(n) * sum_tt - sum_t * sum_t;
    if (std::abs(denom) < 1e-9f) {
        track.approach_rate = 0.f;
        track.motion = core::MotionTrend::kStationary;
        return;
    }

    float slope = (static_cast<float>(n) * sum_tr - sum_t * sum_r) / denom;
    track.approach_rate = slope;

    // 判定运动趋势
    if (std::abs(slope) < config_.stationary_rate_threshold) {
        track.motion = core::MotionTrend::kStationary;
    } else if (slope < 0) {
        track.motion = core::MotionTrend::kReceding;
    } else if (slope >= config_.closing_fast_rate_threshold) {
        track.motion = core::MotionTrend::kClosingFast;
    } else {
        track.motion = core::MotionTrend::kApproaching;
    }
}

// ---- 动态 TTC ----

auto FrontPerception::compute_dynamic_ttc(const TrackedObject& track) -> float {
    // TTC = 剩余距离 / 接近速率
    // 剩余距离用 (1.0 - bottom_ratio) 近似 (bottom_ratio=1 表示已到画面底部)
    if (track.approach_rate <= config_.stationary_rate_threshold) {
        return 1e9f; // 静止或远离 → 无碰撞风险
    }

    float remaining = 1.0f - track.bottom_ratio;
    if (remaining <= 0.f) return 0.f;

    return remaining / track.approach_rate;
}

// ---- 单个目标风险评估 (距离 × 运动趋势矩阵) ----

auto FrontPerception::assess_object_risk(const TrackedObject& track) -> core::RiskLevel {
    float br = track.bottom_ratio;
    auto motion = track.motion;
    bool has_enough_history = track.total_frames >= config_.min_track_frames;

    // 如果跟踪帧数不足, 仅按距离给保守评估 (不直接 emergency)
    if (!has_enough_history) {
        if (br >= config_.danger_bbox_ratio) return core::RiskLevel::kWarning;
        if (br >= config_.near_bbox_ratio)   return core::RiskLevel::kHint;
        return core::RiskLevel::kNormal;
    }

    // 动态 TTC 优先判断
    float ttc = compute_dynamic_ttc(track);
    if (ttc < config_.ttc_emergency_seconds) return core::RiskLevel::kEmergency;
    if (ttc < config_.ttc_warn_seconds)      return core::RiskLevel::kWarning;

    //  距离 × 运动趋势矩阵
    //
    //              远离/静止    低速接近     快速接近
    //  远距离      Normal      Hint        Warning
    //  中距离      Hint        Warning     Emergency
    //  近距离      Warning     Emergency   Emergency

    enum class DistZone { kFar, kMid, kNear };
    DistZone dz = DistZone::kFar;
    if (br >= config_.danger_bbox_ratio)     dz = DistZone::kNear;
    else if (br >= config_.near_bbox_ratio)  dz = DistZone::kMid;

    enum class MotionCat { kSafeOrStill, kApproaching, kClosingFast };
    MotionCat mc = MotionCat::kSafeOrStill;
    if (motion == core::MotionTrend::kClosingFast)     mc = MotionCat::kClosingFast;
    else if (motion == core::MotionTrend::kApproaching) mc = MotionCat::kApproaching;

    // 查表
    static constexpr core::RiskLevel table[3][3] = {
        // Far:   Safe/Still,        Approaching,        ClosingFast
        { core::RiskLevel::kNormal,  core::RiskLevel::kHint,      core::RiskLevel::kWarning   },
        // Mid:
        { core::RiskLevel::kHint,    core::RiskLevel::kWarning,   core::RiskLevel::kEmergency },
        // Near:
        { core::RiskLevel::kWarning, core::RiskLevel::kEmergency, core::RiskLevel::kEmergency },
    };

    return table[static_cast<int>(dz)][static_cast<int>(mc)];
}

// ---- 主风险评估入口 ----

auto FrontPerception::evaluate_risk(const core::DetectionReport& report, int image_height)
    -> RiskAssessment {
    if (report.detections.empty() || image_height <= 0) {
        // 无检测时也要更新 tracks (让 lost_frames 递增)
        auto now = Clock::now();
        for (auto& [id, track] : tracks_) track.lost_frames++;
        purge_lost_tracks();
        return {core::RiskLevel::kNormal, 1e9f, 0.f, core::MotionTrend::kUnknown, 0.f};
    }

    auto now = Clock::now();
    update_tracks(report, image_height, now);
    purge_lost_tracks();

    // 从所有活跃 track 中找最高风险
    core::RiskLevel worst_risk = core::RiskLevel::kNormal;
    float min_ttc = 1e9f;
    float max_approach_rate = 0.f;
    core::MotionTrend worst_motion = core::MotionTrend::kUnknown;
    float closest_ratio = 0.f;

    for (const auto& [id, track] : tracks_) {
        if (track.lost_frames > 0) continue; // 本帧未匹配的不参与评估

        auto risk = assess_object_risk(track);
        float ttc = compute_dynamic_ttc(track);

        if (static_cast<int>(risk) > static_cast<int>(worst_risk)) {
            worst_risk = risk;
            worst_motion = track.motion;
        }

        min_ttc = std::min(min_ttc, ttc);
        if (track.approach_rate > max_approach_rate) {
            max_approach_rate = track.approach_rate;
        }
        if (track.bottom_ratio > closest_ratio) {
            closest_ratio = track.bottom_ratio;
        }
    }

    return {worst_risk, min_ttc, max_approach_rate, worst_motion, closest_ratio};
}

}  // namespace RideShield::perception

#endif
