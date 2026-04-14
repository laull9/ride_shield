#pragma once

#include "RideShield/core/types.h"
#include "RideShield/inference/yolo_preprocess.h"

#include <filesystem>
#include <string>
#include <vector>

#ifdef RIDESHIELD_HAS_ONNXRUNTIME
#include "RideShield/inference/onnx_zero_copy.h"
#endif

namespace RideShield::inference {

struct YoloDetectorConfig {
    std::filesystem::path model_path{"yolo26n.onnx"};
    int input_size{640};
    float score_threshold{0.25f};
    std::size_t intra_threads{1};
    std::vector<std::string> class_names; // 空则使用 COCO-80
};

#ifdef RIDESHIELD_HAS_ONNXRUNTIME

/// YOLO 检测器 —— 封装 preprocess → ONNX 推理 → 后处理
/// 输入端零拷贝：预处理结果直接作为 ONNX 输入张量，无额外拷贝
class YoloDetector {
public:
    explicit YoloDetector(YoloDetectorConfig config);

    /// 从 BGR8 ImageView 执行检测
    auto detect(const core::ImageView& image) -> core::DetectionReport;

#ifdef RIDESHIELD_HAS_OPENCV
    /// 从 cv::Mat 执行检测
    auto detect(const cv::Mat& bgr_frame) -> core::DetectionReport;
#endif

    [[nodiscard]] auto config() const -> const YoloDetectorConfig& { return config_; }
    [[nodiscard]] auto class_names() const -> const std::vector<std::string>& { return class_names_; }

private:
    YoloDetectorConfig config_;
    std::vector<std::string> class_names_;
    std::string input_name_;
    YoloPreprocessContext preprocess_;
    OnnxSession session_;

    auto run_inference(PreparedFrame& prepared) -> core::DetectionReport;

    static auto remap_bbox(
        float x1, float y1, float x2, float y2,
        int orig_w, int orig_h,
        float scale, float pad_x, float pad_y
    ) -> core::BoundingBox;
};

#endif

}  // namespace RideShield::inference
