#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>

#ifdef RIDESHIELD_HAS_OPENCV
#include <opencv2/core.hpp>
#endif

namespace RideShield::core {

enum class PixelFormat {
    kGray8,
    kRgb8,
    kBgr8,
    kFloat32C1,
    kFloat32C3,
};

constexpr std::size_t channel_count(PixelFormat format) {
    switch (format) {
    case PixelFormat::kGray8:
    case PixelFormat::kFloat32C1:
        return 1;
    case PixelFormat::kRgb8:
    case PixelFormat::kBgr8:
    case PixelFormat::kFloat32C3:
        return 3;
    }

    return 0;
}

constexpr std::size_t element_size_bytes(PixelFormat format) {
    switch (format) {
    case PixelFormat::kGray8:
    case PixelFormat::kRgb8:
    case PixelFormat::kBgr8:
        return sizeof(std::uint8_t);
    case PixelFormat::kFloat32C1:
    case PixelFormat::kFloat32C3:
        return sizeof(float);
    }

    return 0;
}

class ImageView {
public:
    ImageView() = default;

    ImageView(void* data, int width, int height, PixelFormat format, std::ptrdiff_t row_stride_bytes = 0)
        : data_(static_cast<std::byte*>(data)),
          width_(width),
          height_(height),
          format_(format) {
        if (width_ <= 0 || height_ <= 0) {
            throw std::invalid_argument("ImageView expects positive width and height");
        }

        const auto minimum_row_stride = static_cast<std::ptrdiff_t>(pixel_stride_bytes() * static_cast<std::size_t>(width_));
        row_stride_bytes_ = row_stride_bytes == 0 ? minimum_row_stride : row_stride_bytes;

        if (row_stride_bytes_ < minimum_row_stride) {
            throw std::invalid_argument("row_stride_bytes is smaller than a packed image row");
        }
    }

    [[nodiscard]] auto data() noexcept -> std::byte* { return data_; }
    [[nodiscard]] auto data() const noexcept -> const std::byte* { return data_; }
    [[nodiscard]] auto width() const noexcept -> int { return width_; }
    [[nodiscard]] auto height() const noexcept -> int { return height_; }
    [[nodiscard]] auto format() const noexcept -> PixelFormat { return format_; }
    [[nodiscard]] auto channels() const noexcept -> int { return static_cast<int>(channel_count(format_)); }
    [[nodiscard]] auto row_stride_bytes() const noexcept -> std::ptrdiff_t { return row_stride_bytes_; }
    [[nodiscard]] auto pixel_stride_bytes() const noexcept -> std::size_t {
        return channel_count(format_) * element_size_bytes(format_);
    }
    [[nodiscard]] auto is_contiguous() const noexcept -> bool {
        return row_stride_bytes_ == static_cast<std::ptrdiff_t>(pixel_stride_bytes() * static_cast<std::size_t>(width_));
    }
    [[nodiscard]] auto byte_size() const noexcept -> std::size_t {
        return static_cast<std::size_t>(row_stride_bytes_) * static_cast<std::size_t>(height_);
    }

    [[nodiscard]] auto subview(int x, int y, int sub_width, int sub_height) const -> ImageView {
        if (x < 0 || y < 0 || sub_width <= 0 || sub_height <= 0 || x + sub_width > width_ || y + sub_height > height_) {
            throw std::out_of_range("Requested subview is outside the image bounds");
        }

        auto* sub_data = data_ + static_cast<std::ptrdiff_t>(y) * row_stride_bytes_ + static_cast<std::ptrdiff_t>(x) * static_cast<std::ptrdiff_t>(pixel_stride_bytes());
        return {sub_data, sub_width, sub_height, format_, row_stride_bytes_};
    }

    template <typename T>
    [[nodiscard]] auto span() -> std::span<T> {
        if (!is_contiguous()) {
            throw std::logic_error("span() requires contiguous image storage");
        }

        return {reinterpret_cast<T*>(data_), byte_size() / sizeof(T)};
    }

    template <typename T>
    [[nodiscard]] auto span() const -> std::span<const T> {
        if (!is_contiguous()) {
            throw std::logic_error("span() requires contiguous image storage");
        }

        return {reinterpret_cast<const T*>(data_), byte_size() / sizeof(T)};
    }

private:
    std::byte* data_{};
    int width_{};
    int height_{};
    PixelFormat format_{PixelFormat::kBgr8};
    std::ptrdiff_t row_stride_bytes_{};
};

#ifdef RIDESHIELD_HAS_OPENCV
inline auto borrow_image(cv::Mat& mat) -> ImageView {
    PixelFormat format{};
    switch (mat.type()) {
    case CV_8UC1:
        format = PixelFormat::kGray8;
        break;
    case CV_8UC3:
        format = PixelFormat::kBgr8;
        break;
    case CV_32FC1:
        format = PixelFormat::kFloat32C1;
        break;
    case CV_32FC3:
        format = PixelFormat::kFloat32C3;
        break;
    default:
        throw std::invalid_argument("Unsupported cv::Mat type for zero-copy ImageView");
    }

    return {mat.data, mat.cols, mat.rows, format, static_cast<std::ptrdiff_t>(mat.step[0])};
}

inline auto borrow_cv_mat(const ImageView& image) -> cv::Mat {
    int cv_type = 0;
    switch (image.format()) {
    case PixelFormat::kGray8:
        cv_type = CV_8UC1;
        break;
    case PixelFormat::kRgb8:
    case PixelFormat::kBgr8:
        cv_type = CV_8UC3;
        break;
    case PixelFormat::kFloat32C1:
        cv_type = CV_32FC1;
        break;
    case PixelFormat::kFloat32C3:
        cv_type = CV_32FC3;
        break;
    }

    return {image.height(), image.width(), cv_type, const_cast<std::byte*>(image.data()), static_cast<std::size_t>(image.row_stride_bytes())};
}
#endif

}  // namespace RideShield::core