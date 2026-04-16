#include "RideShield/inference/yolo_detector.h"

#ifdef RIDESHIELD_HAS_ONNXRUNTIME

#include "RideShield/inference/coco_labels.h"

#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace {
auto make_onnx_session(const RideShield::inference::YoloDetectorConfig& config)
    -> RideShield::inference::OnnxSession {
    RideShield::inference::OnnxSession::Config ort_config{
        .intra_op_threads = config.intra_threads,
        .enable_cpu_mem_arena = true,
    };
    if (!config.model_data.empty()) {
        return RideShield::inference::OnnxSession(config.model_data, ort_config);
    }
    return RideShield::inference::OnnxSession(config.model_path, ort_config);
}
}  // namespace

namespace RideShield::inference {

YoloDetector::YoloDetector(YoloDetectorConfig config)
    : config_(std::move(config)),
      preprocess_(config_.input_size),
      session_(make_onnx_session(config_)) {
    // 获取模型输入名称
    const auto& input_names = session_.input_names();
    if (input_names.empty()) {
        throw std::runtime_error("ONNX model has no input tensors");
    }
    input_name_ = input_names.front();

    // 设置类别名称
    if (config_.class_names.empty()) {
        class_names_.reserve(kCoco80Labels.size());
        for (const auto& label : kCoco80Labels) {
            class_names_.emplace_back(label);
        }
    } else {
        class_names_ = config_.class_names;
    }
}

auto YoloDetector::detect(const core::ImageView& image) -> core::DetectionReport {
    auto prepared = preprocess_.prepare(image);
    return run_inference(prepared);
}

#ifdef RIDESHIELD_HAS_OPENCV
auto YoloDetector::detect(const cv::Mat& bgr_frame) -> core::DetectionReport {
    auto prepared = preprocess_.prepare(bgr_frame);
    return run_inference(prepared);
}
#endif

auto YoloDetector::run_inference(PreparedFrame& prepared) -> core::DetectionReport {
    using Clock = std::chrono::steady_clock;
    const auto started = Clock::now();

    // 零拷贝：TensorView 直接指向 PreprocessContext 的内部缓冲区
    NamedTensorView input{input_name_, prepared.tensor};
    std::array<NamedTensorView, 1> inputs{input};

    auto outputs = session_.run(inputs);

    if (outputs.empty()) {
        return core::DetectionReport{{}, 0.f};
    }

    // 解析输出张量 shape: [1, N, 6] 其中每行 = [x1, y1, x2, y2, confidence, class_id]
    auto& output_val = outputs.front();
    auto type_info = output_val.GetTensorTypeAndShapeInfo();
    auto shape = type_info.GetShape();

    // 支持 [1, N, 6] 和 [1, 6, N] 两种布局
    const float* raw_data = output_val.GetTensorData<float>();
    const bool transposed = (shape.size() == 3 && shape[1] == 6 && shape[2] != 6);

    std::int64_t num_detections = 0;
    std::int64_t row_stride = 6;

    if (shape.size() == 3) {
        if (transposed) {
            // [1, 6, N] → 转置处理
            num_detections = shape[2];
        } else {
            // [1, N, 6]
            num_detections = shape[1];
        }
    } else if (shape.size() == 2) {
        // [N, 6]
        num_detections = shape[0];
    }

    std::vector<core::Detection> detections;
    detections.reserve(static_cast<std::size_t>(num_detections));

    for (std::int64_t i = 0; i < num_detections; ++i) {
        float x1, y1, x2, y2, confidence;
        std::size_t class_id;

        if (transposed) {
            // [1, 6, N] 布局: 第 j 列 = raw_data[j * N + i]
            const auto n = num_detections;
            x1 = raw_data[0 * n + i];
            y1 = raw_data[1 * n + i];
            x2 = raw_data[2 * n + i];
            y2 = raw_data[3 * n + i];
            confidence = raw_data[4 * n + i];
            class_id = static_cast<std::size_t>(std::max(0.f, raw_data[5 * n + i]));
        } else {
            const float* row = raw_data + i * row_stride;
            x1 = row[0];
            y1 = row[1];
            x2 = row[2];
            y2 = row[3];
            confidence = row[4];
            class_id = static_cast<std::size_t>(std::max(0.f, row[5]));
        }

        if (confidence < config_.score_threshold) {
            continue;
        }

        auto bbox = remap_bbox(
            x1, y1, x2, y2,
            prepared.original_width, prepared.original_height,
            prepared.scale, prepared.pad_x, prepared.pad_y
        );

        if (bbox.width() <= 1.f || bbox.height() <= 1.f) {
            continue;
        }

        std::string name;
        if (class_id < class_names_.size()) {
            name = class_names_[class_id];
        } else {
            name = "class_" + std::to_string(class_id);
        }

        detections.push_back(core::Detection{
            .class_id = class_id,
            .class_name = std::move(name),
            .confidence = confidence,
            .bbox = bbox,
        });
    }

    const auto elapsed = std::chrono::duration<float, std::milli>(Clock::now() - started).count();

    return core::DetectionReport{
        .detections = std::move(detections),
        .inference_ms = elapsed,
    };
}

auto YoloDetector::remap_bbox(
    float x1, float y1, float x2, float y2,
    int orig_w, int orig_h,
    float scale, float pad_x, float pad_y
) -> core::BoundingBox {
    const float max_x = static_cast<float>(std::max(orig_w - 1, 0));
    const float max_y = static_cast<float>(std::max(orig_h - 1, 0));

    return core::BoundingBox{
        .left   = std::clamp((x1 - pad_x) / scale, 0.f, max_x),
        .top    = std::clamp((y1 - pad_y) / scale, 0.f, max_y),
        .right  = std::clamp((x2 - pad_x) / scale, 0.f, max_x),
        .bottom = std::clamp((y2 - pad_y) / scale, 0.f, max_y),
    };
}

}  // namespace RideShield::inference

#endif
