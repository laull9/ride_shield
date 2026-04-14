#include "RideShield/inference/yolo_preprocess.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#ifdef RIDESHIELD_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#endif

namespace RideShield::inference {

YoloPreprocessContext::YoloPreprocessContext(int input_size)
    : input_size_(input_size),
      tensor_buffer_(static_cast<std::size_t>(3) * input_size * input_size, kLetterboxPadValue) {
    if (input_size <= 0) {
        throw std::invalid_argument("YoloPreprocessContext: input_size must be positive");
    }
}

auto YoloPreprocessContext::prepare(const core::ImageView& image) -> PreparedFrame {
    if (image.format() != core::PixelFormat::kBgr8) {
        throw std::invalid_argument("YoloPreprocessContext::prepare expects BGR8 input");
    }

    const int orig_w = image.width();
    const int orig_h = image.height();

    const float scale = std::min(
        static_cast<float>(input_size_) / static_cast<float>(orig_w),
        static_cast<float>(input_size_) / static_cast<float>(orig_h)
    );

    const int resized_w = static_cast<int>(std::round(static_cast<float>(orig_w) * scale));
    const int resized_h = static_cast<int>(std::round(static_cast<float>(orig_h) * scale));
    const int pad_x = (input_size_ - resized_w) / 2;
    const int pad_y = (input_size_ - resized_h) / 2;

    // 填充默认值
    std::fill(tensor_buffer_.begin(), tensor_buffer_.end(), kLetterboxPadValue);

#ifdef RIDESHIELD_HAS_OPENCV
    // 用 OpenCV resize
    cv::Mat src_mat = core::borrow_cv_mat(image);
    cv::resize(src_mat, resized_, cv::Size(resized_w, resized_h), 0, 0, cv::INTER_LINEAR);

    pack_bgr8_into_chw(
        reinterpret_cast<const std::byte*>(resized_.data),
        resized_w, resized_h,
        static_cast<std::ptrdiff_t>(resized_.step[0]),
        pad_x, pad_y
    );
#else
    // 无 OpenCV 时仅支持与 input_size 相同尺寸的图像 (直接拷贝，无缩放)
    if (orig_w != input_size_ || orig_h != input_size_) {
        throw std::runtime_error("Without OpenCV, image must match input_size exactly");
    }

    pack_bgr8_into_chw(
        image.data(), orig_w, orig_h,
        image.row_stride_bytes(),
        0, 0
    );
#endif

    core::TensorView tensor(
        tensor_buffer_.data(),
        core::TensorElementType::kFloat32,
        {1, 3, input_size_, input_size_}
    );

    return PreparedFrame{
        .tensor = tensor,
        .original_width = orig_w,
        .original_height = orig_h,
        .scale = scale,
        .pad_x = static_cast<float>(pad_x),
        .pad_y = static_cast<float>(pad_y),
    };
}

#ifdef RIDESHIELD_HAS_OPENCV
auto YoloPreprocessContext::prepare(const cv::Mat& bgr_frame) -> PreparedFrame {
    auto view = core::borrow_image(const_cast<cv::Mat&>(bgr_frame));
    return prepare(view);
}
#endif

void YoloPreprocessContext::pack_bgr8_into_chw(
    const std::byte* src_data,
    int src_width,
    int src_height,
    std::ptrdiff_t src_row_stride,
    int dst_offset_x,
    int dst_offset_y
) {
    const auto plane_area = static_cast<std::size_t>(input_size_) * input_size_;
    constexpr float inv_255 = 1.f / 255.f;

    float* r_plane = tensor_buffer_.data();
    float* g_plane = tensor_buffer_.data() + plane_area;
    float* b_plane = tensor_buffer_.data() + plane_area * 2;

    for (int y = 0; y < src_height; ++y) {
        const auto* row = src_data + static_cast<std::ptrdiff_t>(y) * src_row_stride;
        const auto dst_row = static_cast<std::size_t>(y + dst_offset_y) * input_size_ + dst_offset_x;

        for (int x = 0; x < src_width; ++x) {
            const auto* pixel = row + x * 3; // BGR8
            const auto dst_idx = dst_row + x;

            // BGR → RGB, 归一化到 [0, 1]
            r_plane[dst_idx] = static_cast<float>(static_cast<std::uint8_t>(pixel[2])) * inv_255;
            g_plane[dst_idx] = static_cast<float>(static_cast<std::uint8_t>(pixel[1])) * inv_255;
            b_plane[dst_idx] = static_cast<float>(static_cast<std::uint8_t>(pixel[0])) * inv_255;
        }
    }
}

}  // namespace RideShield::inference
