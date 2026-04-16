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

#ifdef RIDESHIELD_HAS_EMBEDDED_MODEL
extern const unsigned char rideshield_yolo_model_data[];
extern const unsigned char rideshield_yolo_model_end[];
#endif

#ifdef RIDESHIELD_HAS_OPENCV
#include <opencv2/imgcodecs.hpp>
#include <opencv2/videoio.hpp>
#endif

#ifdef RIDESHIELD_HAS_QT_UI
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTimer>
#include "RideShield/ui/vehicle_bridge.h"
#endif

#include <fmt/core.h>

#include <array>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <span>
#include <vector>

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

bool has_flag(int argc, char* argv[], const char* flag) {
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], flag) == 0) return true;
    return false;
}

#ifdef RIDESHIELD_HAS_QT_UI

/// 将 FrontPerceptionResult 转为 bridge 调用
void pushFrontResult(RideShield::ui::VehicleBridge* bridge,
                     const RideShield::core::FrontPerceptionResult& r) {
    QVariantList dets;
    for (const auto& d : r.report.detections) {
        QVariantMap m;
        m["className"]  = QString::fromStdString(d.class_name);
        m["confidence"] = d.confidence;
        m["left"]       = d.bbox.left;
        m["top"]        = d.bbox.top;
        m["right"]      = d.bbox.right;
        m["bottom"]     = d.bbox.bottom;
        dets.append(m);
    }
    bridge->updateFront(r.ttc_seconds, r.approach_rate,
                        static_cast<int>(r.report.detections.size()), dets);

    // 融合决策（仅前向可用时）
    RideShield::decision::FusionEngine fusion;
    RideShield::decision::FusionEngine::Input input{};
    input.front = r;
    auto result = fusion.evaluate(input);
    bridge->updateFusion(result.risk_score,
                         static_cast<int>(result.overall_risk),
                         result.should_brake);
}

int runUi(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    app.setApplicationName("RideShield");
    app.setOrganizationName("RideShield");

    QQmlApplicationEngine engine;

    auto* bridge = new RideShield::ui::VehicleBridge(&app);
    qmlRegisterSingletonInstance("RideShield.UI", 1, 0, "VehicleBridge", bridge);

    engine.loadFromModule("RideShield.UI", "Main");
    if (engine.rootObjects().isEmpty())
        return -1;

    // ── 尝试打开前向摄像头 + ONNX 模型 ──
#if defined(RIDESHIELD_HAS_ONNXRUNTIME) && defined(RIDESHIELD_HAS_OPENCV)
    std::unique_ptr<RideShield::perception::FrontPerception> front;
    std::unique_ptr<cv::VideoCapture> cap;

    [&]() {
        RideShield::inference::YoloDetectorConfig cfg{
            .input_size = 640,
            .score_threshold = 0.25f,
            .intra_threads = 4,
        };
#ifdef RIDESHIELD_HAS_EMBEDDED_MODEL
        cfg.model_data = {rideshield_yolo_model_data,
                          static_cast<std::size_t>(rideshield_yolo_model_end - rideshield_yolo_model_data)};
#else
        std::filesystem::path model_path = "res/yolo26n.onnx";
        for (int i = 1; i < argc; ++i)
            if (std::strncmp(argv[i], "--", 2) != 0) { model_path = argv[i]; break; }
        if (!std::filesystem::exists(model_path)) {
            fmt::print("[RideShield UI] 模型 {} 不存在, 前向感知不可用\n", model_path.string());
            return;
        }
        cfg.model_path = model_path;
#endif
        try {
            front = std::make_unique<RideShield::perception::FrontPerception>(cfg);
            cap   = std::make_unique<cv::VideoCapture>(0);
            if (!cap->isOpened()) {
                fmt::print("[RideShield UI] 无法打开摄像头, 前向感知仅显示空状态\n");
                cap.reset();
            } else {
                fmt::print("[RideShield UI] 前向摄像头 + ONNX 模型已就绪\n");
            }
        } catch (const std::exception& e) {
            fmt::print("[RideShield UI] 前向感知初始化失败: {}\n", e.what());
            front.reset();
            cap.reset();
        }
    }();

    // 定时器驱动摄像头帧采集 + 推理
    auto* timer = new QTimer(bridge);
    QObject::connect(timer, &QTimer::timeout, bridge, [bridge, &front, &cap]() {
        if (front && cap && cap->isOpened()) {
            cv::Mat frame;
            if (cap->read(frame) && !frame.empty()) {
                auto result = front->process(frame);
                pushFrontResult(bridge, result);
                return;
            }
        }
        // 无摄像头时保持空状态
        bridge->updateFront(99.f, 0.f, 0, {});
        bridge->updateFusion(0.f, 0, false);
    });
    timer->start(100);  // ~10 fps
#else
    // 无 ONNX/OpenCV，全部空状态
    bridge->updateFront(99.f, 0.f, 0, {});
    bridge->updateFusion(0.f, 0, false);
#endif

    return app.exec();
}
#endif  // RIDESHIELD_HAS_QT_UI

int runHeadless(int argc, char* argv[]) {
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
    RideShield::inference::YoloDetectorConfig front_config{
        .input_size = 640,
        .score_threshold = 0.25f,
        .intra_threads = 4,
    };
    RideShield::inference::YoloDetectorConfig rear_config{
        .input_size = 640,
        .score_threshold = 0.30f,
        .intra_threads = 4,
    };

#ifdef RIDESHIELD_HAS_EMBEDDED_MODEL
    auto model_span = std::span<const unsigned char>{
        rideshield_yolo_model_data,
        static_cast<std::size_t>(rideshield_yolo_model_end - rideshield_yolo_model_data)};
    front_config.model_data = model_span;
    rear_config.model_data  = model_span;
    fmt::print("[RideShield] 使用嵌入式 ONNX 模型 ({} bytes)\n", model_span.size());
#else
    std::filesystem::path model_path = "res/yolo26n.onnx";

    // 收集非 -- 开头的位置参数
    std::vector<std::string> pos_args;
    for (int i = 1; i < argc; ++i)
        if (std::strncmp(argv[i], "--", 2) != 0) pos_args.emplace_back(argv[i]);

    if (!pos_args.empty()) {
        model_path = pos_args[0];
    }

    if (!std::filesystem::exists(model_path)) {
        fmt::print("[RideShield] 模型文件不存在: {}\n", model_path.string());
        fmt::print("[RideShield] 用法: {} [model.onnx] [image_or_video]\n", argv[0]);
        return 0;
    }
    front_config.model_path = model_path;
    rear_config.model_path  = model_path;
#endif

    RideShield::perception::FrontPerception front_perception(front_config);
    fmt::print("[RideShield] 前向感知模块已初始化\n");

    RideShield::perception::RearPerception rear_perception(rear_config);
    fmt::print("[RideShield] 后向感知模块已初始化\n");

    // 如果提供了图像或视频路径，执行检测演示
#ifndef RIDESHIELD_HAS_EMBEDDED_MODEL
    if (pos_args.size() > 1) {
        std::string input_path = pos_args[1];
#else
    std::vector<std::string> pos_args;
    for (int i = 1; i < argc; ++i)
        if (std::strncmp(argv[i], "--", 2) != 0) pos_args.emplace_back(argv[i]);
    if (!pos_args.empty()) {
        std::string input_path = pos_args[0];
#endif
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
        fmt::print("[RideShield] 提供图像路径以运行检测: {} <image>\n", argv[0]);
    }
#else
    fmt::print("[RideShield] ONNX Runtime 或 OpenCV 未启用，跳过推理管线\n");
#endif

    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (has_flag(argc, argv, "--headless")) {
        return runHeadless(argc, argv);
    }

#ifdef RIDESHIELD_HAS_QT_UI
    return runUi(argc, argv);
#else
    fmt::print("[RideShield] Qt UI 未启用，以 headless 模式运行\n");
    return runHeadless(argc, argv);
#endif
}