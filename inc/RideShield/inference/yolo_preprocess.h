#pragma once

#include "RideShield/core/image_view.h"
#include "RideShield/core/tensor_view.h"

#include <vector>

#ifdef RIDESHIELD_HAS_OPENCV
#include <opencv2/core.hpp>
#endif

namespace RideShield::inference {

struct PreparedFrame {
    core::TensorView tensor;    // 非拥有视图，指向 PreprocessContext 内部缓冲区
    int original_width{};
    int original_height{};
    float scale{};
    float pad_x{};
    float pad_y{};
};

/// YOLO 前处理上下文 —— 拥有预分配的 NCHW float32 张量缓冲区
/// 调用 prepare() 对图像做 letterbox 缩放 + BGR→RGB→CHW 归一化
/// 返回的 PreparedFrame::tensor 零拷贝指向内部缓冲区
class YoloPreprocessContext {
public:
    explicit YoloPreprocessContext(int input_size);

    [[nodiscard]] auto input_size() const noexcept -> int { return input_size_; }

    /// 对输入 BGR8 图像执行 letterbox + 归一化，结果写入内部缓冲区
    /// 返回的 PreparedFrame::tensor 指向内部缓冲区 (不拷贝)
    auto prepare(const core::ImageView& image) -> PreparedFrame;

#ifdef RIDESHIELD_HAS_OPENCV
    auto prepare(const cv::Mat& bgr_frame) -> PreparedFrame;
#endif

private:
    static constexpr float kLetterboxPadValue = 114.f / 255.f;

    int input_size_;
    std::vector<float> tensor_buffer_;  // [1, 3, input_size, input_size]

#ifdef RIDESHIELD_HAS_OPENCV
    cv::Mat resized_;  // 暂存缩放后的中间结果
#endif

    void pack_bgr8_into_chw(
        const std::byte* src_data,
        int src_width,
        int src_height,
        std::ptrdiff_t src_row_stride,
        int dst_offset_x,
        int dst_offset_y
    );
};

}  // namespace RideShield::inference
