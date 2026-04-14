#include "main.h"

#include "RideShield/core/image_view.h"
#include "RideShield/core/tensor_view.h"
#include "RideShield/core/types.h"
#include "RideShield/decision/fusion_engine.h"

#ifdef RIDESHIELD_HAS_ONNXRUNTIME
#include "RideShield/inference/yolo_detector.h"
#include "RideShield/perception/front_perception.h"
#include "RideShield/perception/rear_perception.h"
#endif

#ifdef RIDESHIELD_HAS_OPENCV
#include <opencv2/imgcodecs.hpp>
#include <opencv2/videoio.hpp>
#endif

#include <fmt/core.h>

#include <array>
#include <cstdlib>
#include <filesystem>

namespace {

const char* risk_level_name(RideShield::core::RiskLevel level) {
    switch (level) {
    case RideShield::core::RiskLevel::kNormal:    return "L0_正常";
    case RideShield::core::RiskLevel::kHint:      return "L1_提示";
    case RideShield::core::RiskLevel::kWarning:   return "L2_预警";
    case RideShield::core::RiskLevel::kEmergency: return "L3_紧急";
    }
    return "unknown";
}

}  // namespace

int main(int argc, char* argv[]) {
    // ---------- 基础 zero-copy 视图验证 ----------
    std::array<std::byte, 4 * 4 * 3> image_storage{};
    std::array<float, 1 * 3 * 4 * 4> tensor_storage{};

    auto image = RideShield::core::ImageView(
        image_storage.data(), 4, 4, RideShield::core::PixelFormat::kBgr8);
    auto tensor = RideShield::core::TensorView(
        tensor_storage.data(), RideShield::core::TensorElementType::kFloat32, {1, 3, 4, 4});

    fmt::print("[RideShield] ImageView: {}x{}, TensorView: {} elements\n",
        image.width(), image.height(), tensor.element_count());

    // ---------- 融合决策引擎验证 ----------
    RideShield::decision::FusionEngine fusion;
    RideShield::decision::FusionEngine::Input fusion_input{};
    auto fusion_result = fusion.evaluate(fusion_input);
    fmt::print("[RideShield] FusionEngine OK, risk_score={:.2f}, level={}\n",
        fusion_result.risk_score, risk_level_name(fusion_result.overall_risk));

#if defined(RIDESHIELD_HAS_ONNXRUNTIME) && defined(RIDESHIELD_HAS_OPENCV)
    // ---------- ONNX 零拷贝推理管线 ----------
    std::filesystem::path model_path = "res/yolo26n.onnx";

    if (argc > 1) {
        model_path = argv[1];
    }

    if (!std::filesystem::exists(model_path)) {
        fmt::print("[RideShield] 模型文件不存在: {}\n", model_path.string());
        fmt::print("[RideShield] 用法: {} [model.onnx] [image_or_video]\n", argv[0]);
        return 0;
    }

    // 创建前向感知 (YOLO 检测 + TTC 评估)
    RideShield::inference::YoloDetectorConfig front_config{
        .model_path = model_path,
        .input_size = 640,
        .score_threshold = 0.25f,
        .intra_threads = 4,
    };

    RideShield::perception::FrontPerception front_perception(front_config);
    fmt::print("[RideShield] 前向感知模块已初始化 (model={})\n", model_path.string());

    // 创建后向感知 (共享同一模型)
    RideShield::inference::YoloDetectorConfig rear_config{
        .model_path = model_path,
        .input_size = 640,
        .score_threshold = 0.30f,
        .intra_threads = 4,
    };

    RideShield::perception::RearPerception rear_perception(rear_config);
    fmt::print("[RideShield] 后向感知模块已初始化\n");

    // 如果提供了图像或视频路径，执行检测演示
    if (argc > 2) {
        std::string input_path = argv[2];
        cv::Mat frame = cv::imread(input_path);

        if (frame.empty()) {
            // 尝试作为视频
            cv::VideoCapture cap(input_path);
            if (cap.isOpened()) {
                cap >> frame;
            }
        }

        if (!frame.empty()) {
            fmt::print("[RideShield] 输入图像: {}x{}\n", frame.cols, frame.rows);

            // 前向检测
            auto front_result = front_perception.process(frame);
            fmt::print("[RideShield] 前向检测: {} 个目标, 推理 {:.1f}ms, 风险={}, TTC={:.1f}s\n",
                front_result.report.detections.size(),
                front_result.report.inference_ms,
                risk_level_name(front_result.risk),
                front_result.ttc_seconds);

            for (const auto& det : front_result.report.detections) {
                fmt::print("  - {} (conf={:.2f}) [{:.0f},{:.0f},{:.0f},{:.0f}]\n",
                    det.class_name, det.confidence,
                    det.bbox.left, det.bbox.top, det.bbox.right, det.bbox.bottom);
            }

            // 后向检测 (用同一图像演示)
            auto rear_result = rear_perception.process(frame);
            fmt::print("[RideShield] 后向检测: {} 个目标, 推理 {:.1f}ms, 风险={}\n",
                rear_result.report.detections.size(),
                rear_result.report.inference_ms,
                risk_level_name(rear_result.risk));
            fmt::print("  盲区占用: 左后={} 正后={} 右后={}\n",
                rear_result.left_rear_occupied,
                rear_result.center_rear_occupied,
                rear_result.right_rear_occupied);

            // 融合决策
            RideShield::decision::FusionEngine::Input full_input{
                .front = front_result,
                .rear = rear_result,
            };

            auto decision = fusion.evaluate(full_input);
            fmt::print("[RideShield] 融合决策: score={:.2f}, level={}, voice={}, vibrate={}, brake={}\n",
                decision.risk_score,
                risk_level_name(decision.overall_risk),
                decision.should_warn_voice,
                decision.should_warn_vibrate,
                decision.should_brake);
        } else {
            fmt::print("[RideShield] 无法加载输入: {}\n", input_path);
        }
    } else {
        fmt::print("[RideShield] 提供图像路径以运行检测: {} {} <image>\n", argv[0], model_path.string());
    }
#else
    fmt::print("[RideShield] ONNX Runtime 或 OpenCV 未启用，跳过推理管线\n");
#endif

    return 0;
}