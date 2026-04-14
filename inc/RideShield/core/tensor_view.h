#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <numeric>
#include <span>
#include <stdexcept>
#include <vector>

namespace RideShield::core {

enum class TensorElementType {
    kFloat32,
    kUint8,
    kInt64,
};

constexpr std::size_t element_size_bytes(TensorElementType type) {
    switch (type) {
    case TensorElementType::kFloat32:
        return sizeof(float);
    case TensorElementType::kUint8:
        return sizeof(std::uint8_t);
    case TensorElementType::kInt64:
        return sizeof(std::int64_t);
    }

    return 0;
}

class TensorView {
public:
    TensorView() = default;

    TensorView(void* data, TensorElementType type, std::initializer_list<std::int64_t> shape)
        : TensorView(data, type, std::vector<std::int64_t>(shape)) {}

    TensorView(void* data, TensorElementType type, std::vector<std::int64_t> shape)
        : data_(data),
          type_(type),
          shape_(std::move(shape)) {
        if (shape_.empty()) {
            throw std::invalid_argument("TensorView shape must not be empty");
        }

        for (const auto dimension : shape_) {
            if (dimension <= 0) {
                throw std::invalid_argument("TensorView shape dimensions must be positive");
            }
        }
    }

    [[nodiscard]] auto data() noexcept -> void* { return data_; }
    [[nodiscard]] auto data() const noexcept -> const void* { return data_; }
    [[nodiscard]] auto type() const noexcept -> TensorElementType { return type_; }
    [[nodiscard]] auto shape() const noexcept -> std::span<const std::int64_t> { return shape_; }
    [[nodiscard]] auto rank() const noexcept -> std::size_t { return shape_.size(); }
    [[nodiscard]] auto element_count() const -> std::size_t {
        return std::accumulate(shape_.begin(), shape_.end(), static_cast<std::size_t>(1), [](std::size_t accumulator, std::int64_t value) {
            return accumulator * static_cast<std::size_t>(value);
        });
    }
    [[nodiscard]] auto byte_size() const -> std::size_t {
        return element_count() * element_size_bytes(type_);
    }

    template <typename T>
    [[nodiscard]] auto span() -> std::span<T> {
        return {reinterpret_cast<T*>(data_), byte_size() / sizeof(T)};
    }

    template <typename T>
    [[nodiscard]] auto span() const -> std::span<const T> {
        return {reinterpret_cast<const T*>(data_), byte_size() / sizeof(T)};
    }

private:
    void* data_{};
    TensorElementType type_{TensorElementType::kFloat32};
    std::vector<std::int64_t> shape_;
};

}  // namespace RideShield::core