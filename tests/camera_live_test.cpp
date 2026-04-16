/// 实时摄像头 YOLO 标注测试
/// 使用 OpenCV 打开设备摄像头，通过 FrontPerception 检测行人、车辆等目标，
/// 并在画面上绘制 bounding box + 标签 + 风险等级。
/// 按 ESC / q 退出。

#include "resources.h"

#include "RideShield/core/types.h"
#include "RideShield/inference/coco_labels.h"

#if defined(RIDESHIELD_HAS_ONNXRUNTIME) && defined(RIDESHIELD_HAS_OPENCV)

#include "RideShield/perception/front_perception.h"
#include "RideShield/inference/yolo_detector.h"

#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fmt/core.h>
#include <string>


// 测试视频路径
const auto TestVideoPath = 
    std::filesystem::path(PROJECT_SOURCE_DIR) 
    / "res_test" 
    / "test1.mp4"
    ;


using namespace RideShield;

// ============================================================
//  绘制工具
// ============================================================

namespace {

/// 根据风险等级返回颜色 (BGR)
cv::Scalar risk_color(core::RiskLevel level) {
    switch (level) {
        case core::RiskLevel::kNormal:    return {0, 200, 0};     // 绿
        case core::RiskLevel::kHint:      return {0, 200, 200};   // 黄
        case core::RiskLevel::kWarning:   return {0, 128, 255};   // 橙
        case core::RiskLevel::kEmergency: return {0, 0, 255};     // 红
    }
    return {200, 200, 200};
}

const char* risk_text(core::RiskLevel level) {
    switch (level) {
        case core::RiskLevel::kNormal:    return "Normal";
        case core::RiskLevel::kHint:      return "Hint";
        case core::RiskLevel::kWarning:   return "Warning";
        case core::RiskLevel::kEmergency: return "EMERGENCY";
    }
    return "?";
}

/// 在帧上绘制检测结果，颜色随 bbox 底边位置变化（越靠近底部越红）
void draw_detections(cv::Mat& frame, const core::DetectionReport& report) {
    const float h = static_cast<float>(frame.rows);
    for (const auto& det : report.detections) {
        // 根据 bbox 底边与画面底部的接近程度选色
        const float bottom_ratio = det.bbox.bottom / h;
        cv::Scalar color;
        if (bottom_ratio > 0.85f) {
            color = cv::Scalar(0, 0, 255);       // 红色 — 极近
        } else if (bottom_ratio > 0.65f) {
            color = cv::Scalar(0, 128, 255);      // 橙色 — 较近
        } else if (bottom_ratio > 0.45f) {
            color = cv::Scalar(0, 220, 220);      // 黄色 — 中等
        } else {
            color = cv::Scalar(0, 200, 0);        // 绿色 — 远处
        }
        const cv::Rect rect(
            static_cast<int>(det.bbox.left),
            static_cast<int>(det.bbox.top),
            static_cast<int>(det.bbox.width()),
            static_cast<int>(det.bbox.height())
        );
        cv::rectangle(frame, rect, color, 2);

        auto label = fmt::format("{} {:.0f}%",
            inference::coco80_label_en(det.class_id),
            det.confidence * 100.f);

        const int baseline = 0;
        const auto text_size = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, nullptr);

        // 标签背景
        cv::rectangle(frame,
            cv::Point(rect.x, rect.y - text_size.height - 6),
            cv::Point(rect.x + text_size.width + 4, rect.y),
            color, cv::FILLED);

        cv::putText(frame, label,
            cv::Point(rect.x + 2, rect.y - 4),
            cv::FONT_HERSHEY_SIMPLEX, 0.5,
            cv::Scalar(0, 0, 0), 1);
    }
}

/// 在帧上绘制 HUD 信息（风险等级、TTC、推理耗时）
void draw_hud(cv::Mat& frame,
              core::RiskLevel risk,
              float ttc_seconds,
              float inference_ms,
              int det_count) {
    const auto color = risk_color(risk);

    // 顶部状态栏背景
    cv::rectangle(frame, cv::Point(0, 0), cv::Point(frame.cols, 36), cv::Scalar(0, 0, 0), cv::FILLED);

    auto status = fmt::format("Risk: {}  TTC: {:.1f}s  Infer: {:.1f}ms  Det: {}",
        risk_text(risk), ttc_seconds > 100.f ? 99.9f : ttc_seconds,
        inference_ms, det_count);

    cv::putText(frame, status, cv::Point(8, 26),
        cv::FONT_HERSHEY_SIMPLEX, 0.65, color, 2);
}

void draw_playback_status(cv::Mat& frame,
                          double video_seconds,
                          int displayed_frames,
                          int dropped_frames) {
    cv::rectangle(frame, cv::Point(0, 36), cv::Point(frame.cols, 68), cv::Scalar(0, 0, 0), cv::FILLED);

    auto status = fmt::format("Video: {:.1f}s  Displayed: {}  Dropped: {}",
        video_seconds,
        displayed_frames,
        dropped_frames);

    cv::putText(frame, status, cv::Point(8, 59),
        cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(220, 220, 220), 1);
}

}  // namespace

// ============================================================
//  实时摄像头标注测试（前摄场景）
// ============================================================

class CameraLiveTest : public ::testing::Test {
protected:
    static inference::YoloDetectorConfig make_config(float score_threshold = 0.25f,
                                                      std::size_t threads = 2) {
        inference::YoloDetectorConfig cfg{
            .input_size = 640,
            .score_threshold = score_threshold,
            .intra_threads = threads,
        };
        cfg.model_data = RideShield::resources::resource_yolo26n_onnx();
        return cfg;
    }

    static bool model_available() {
        return !RideShield::resources::resource_yolo26n_onnx().empty();
    }

    static auto make_perception_config() -> perception::FrontPerception::Config {
        return {
            .ttc_warn_seconds = 2.0f,
            .ttc_emergency_seconds = 1.0f,
            .near_bbox_ratio = 0.65f,
            .danger_bbox_ratio = 0.85f,
        };
    }
};

/// 打开设备默认摄像头，实时推理并标注
TEST_F(CameraLiveTest, FrontCameraRealtimeAnnotation) {
    if (!model_available()) GTEST_SKIP() << "Model not available";

    // 打开摄像头 (设备 0)
    cv::VideoCapture cap(0);
    if (!cap.isOpened()) {
        GTEST_SKIP() << "No camera device available (index 0)";
    }

    // 设置分辨率
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);

    auto detector_config = make_config();

    // 放宽风险阈值：bbox 底边需要更靠近画面底部才触发预警
    // 避免远处行人/车辆被误判为高风险
    auto perception_config = make_perception_config();

    perception::FrontPerception front(detector_config, perception_config);

    const std::string window_name = "RideShield - Front Camera Live";
    cv::namedWindow(window_name, cv::WINDOW_AUTOSIZE);

    fmt::println("=== 实时前向摄像头标注 ===");
    fmt::println("按 ESC 或 q 退出");

    int frame_count = 0;

    while (true) {
        cv::Mat frame;
        if (!cap.read(frame) || frame.empty()) {
            break;
        }

        auto result = front.process(frame);

        draw_detections(frame, result.report);
        draw_hud(frame, result.risk, result.ttc_seconds,
                 result.report.inference_ms,
                 static_cast<int>(result.report.detections.size()));

        cv::imshow(window_name, frame);

        const int key = cv::waitKey(1) & 0xFF;
        if (key == 27 || key == 'q' || key == 'Q') {
            break;
        }

        ++frame_count;
    }

    cap.release();
    cv::destroyAllWindows();

    fmt::println("处理帧数: {}", frame_count);
    EXPECT_GE(frame_count, 0);
}

/// 指定摄像头索引测试（从环境变量 CAMERA_INDEX 获取）
TEST_F(CameraLiveTest, CustomCameraIndex) {
    if (!model_available()) GTEST_SKIP() << "Model not available";

    const char* env_idx = std::getenv("CAMERA_INDEX");
    if (!env_idx) {
        GTEST_SKIP() << "Set CAMERA_INDEX env var to specify camera device";
    }

    const int cam_index = std::stoi(env_idx);
    cv::VideoCapture cap(cam_index);
    if (!cap.isOpened()) {
        GTEST_SKIP() << "Cannot open camera device " << cam_index;
    }

    cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);

    inference::YoloDetectorConfig detector_config = make_config();

    auto perception_config = make_perception_config();

    perception::FrontPerception front(detector_config, perception_config);

    const std::string window_name = fmt::format("RideShield - Camera {}", cam_index);
    cv::namedWindow(window_name, cv::WINDOW_AUTOSIZE);

    while (true) {
        cv::Mat frame;
        if (!cap.read(frame) || frame.empty()) break;

        auto result = front.process(frame);

        draw_detections(frame, result.report);
        draw_hud(frame, result.risk, result.ttc_seconds,
                 result.report.inference_ms,
                 static_cast<int>(result.report.detections.size()));

        cv::imshow(window_name, frame);

        const int key = cv::waitKey(1) & 0xFF;
        if (key == 27 || key == 'q' || key == 'Q') break;
    }

    cap.release();
    cv::destroyAllWindows();
}

/// 验证摄像头能正常打开并抓取至少一帧（无 GUI 模式）
TEST_F(CameraLiveTest, CameraGrabSingleFrame) {
    if (!model_available()) GTEST_SKIP() << "Model not available";

    cv::VideoCapture cap(0);
    if (!cap.isOpened()) {
        GTEST_SKIP() << "No camera device available (index 0)";
    }

    cv::Mat frame;
    ASSERT_TRUE(cap.read(frame));
    ASSERT_FALSE(frame.empty());
    EXPECT_GT(frame.cols, 0);
    EXPECT_GT(frame.rows, 0);

    // 跑一次前向感知
    perception::FrontPerception front(make_config());
    auto result = front.process(frame);

    EXPECT_GE(result.report.inference_ms, 0.f);
    fmt::println("Camera frame: {}x{}, detections: {}, risk: {}, ttc: {:.2f}s, infer: {:.1f}ms",
        frame.cols, frame.rows,
        result.report.detections.size(),
        risk_text(result.risk),
        result.ttc_seconds > 100.f ? 99.9f : result.ttc_seconds,
        result.report.inference_ms);

    cap.release();
}

/// 播放测试视频并实时显示逐帧标注结果，必要时主动丢帧以保持时间轴对齐。
TEST_F(CameraLiveTest, TestVideoRealtimeAnnotation) {
    if (!model_available()) GTEST_SKIP() << "Model not available";

    if (!std::filesystem::exists(TestVideoPath)) {
        GTEST_SKIP() << "Test video not found: " << TestVideoPath.string();
    }

    cv::VideoCapture cap(TestVideoPath.string());
    if (!cap.isOpened()) {
        GTEST_SKIP() << "Cannot open test video: " << TestVideoPath.string();
    }

    double fps = cap.get(cv::CAP_PROP_FPS);
    if (fps <= 1.0) {
        fps = 30.0;
    }
    const double frame_duration_ms = 1000.0 / fps;

    perception::FrontPerception front(make_config(), make_perception_config());

    const std::string window_name = "RideShield - Test Video Realtime";
    cv::namedWindow(window_name, cv::WINDOW_AUTOSIZE);

    fmt::println("=== 测试视频实时标注 ===");
    fmt::println("视频: {}", TestVideoPath.string());
    fmt::println("按 ESC 或 q 跳过退出");

    const auto playback_start = std::chrono::steady_clock::now();
    std::size_t next_frame_index = 0;
    int displayed_frames = 0;
    int dropped_frames = 0;
    bool skipped_by_user = false;
    bool end_of_stream = false;

    while (true) {
        cv::Mat frame;
        if (!cap.read(frame) || frame.empty()) {
            break;
        }

        const double frame_timestamp_ms = static_cast<double>(next_frame_index) * frame_duration_ms;
        ++next_frame_index;

        auto result = front.process(frame);

        draw_detections(frame, result.report);
        draw_hud(frame, result.risk, result.ttc_seconds,
                 result.report.inference_ms,
                 static_cast<int>(result.report.detections.size()));
        draw_playback_status(frame,
                             frame_timestamp_ms / 1000.0,
                             displayed_frames + 1,
                             dropped_frames);

        cv::imshow(window_name, frame);
        ++displayed_frames;

        const double next_frame_timestamp_ms = static_cast<double>(next_frame_index) * frame_duration_ms;
        const auto after_infer = std::chrono::steady_clock::now();
        const double elapsed_ms = std::chrono::duration<double, std::milli>(after_infer - playback_start).count();
        const double remaining_ms = next_frame_timestamp_ms - elapsed_ms;

        const int key = cv::waitKey(remaining_ms > 1.0 ? static_cast<int>(remaining_ms) : 1) & 0xFF;
        if (key == 27 || key == 'q' || key == 'Q') {
            skipped_by_user = true;
            break;
        }

        const auto after_wait = std::chrono::steady_clock::now();
        const double synced_elapsed_ms = std::chrono::duration<double, std::milli>(after_wait - playback_start).count();
        while (static_cast<double>(next_frame_index) * frame_duration_ms < synced_elapsed_ms) {
            cv::Mat dropped;
            if (!cap.read(dropped) || dropped.empty()) {
                end_of_stream = true;
                break;
            }
            ++next_frame_index;
            ++dropped_frames;
        }

        if (end_of_stream) {
            break;
        }
    }

    cap.release();
    cv::destroyAllWindows();

    if (skipped_by_user) {
        GTEST_SKIP() << "User skipped video playback";
    }

    fmt::println("视频回放完成: 显示 {} 帧, 丢弃 {} 帧, fps={:.2f}",
        displayed_frames,
        dropped_frames,
        fps);
    EXPECT_GT(displayed_frames, 0);
}

#else

#include <gtest/gtest.h>

TEST(CameraLiveTest, SkippedNoDependencies) {
    GTEST_SKIP() << "Requires RIDESHIELD_HAS_ONNXRUNTIME and RIDESHIELD_HAS_OPENCV";
}

#endif // RIDESHIELD_HAS_ONNXRUNTIME && RIDESHIELD_HAS_OPENCV
