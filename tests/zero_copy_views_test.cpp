#include "RideShield/core/image_view.h"
#include "RideShield/core/tensor_view.h"

#include <gtest/gtest.h>

#include <array>

TEST(ZeroCopyViewsTest, TensorViewReportsByteSize) {
    std::array<float, 24> storage{};
    const auto tensor = RideShield::core::TensorView(storage.data(), RideShield::core::TensorElementType::kFloat32, {1, 3, 2, 4});

    EXPECT_EQ(tensor.rank(), 4);
    EXPECT_EQ(tensor.element_count(), 24U);
    EXPECT_EQ(tensor.byte_size(), sizeof(float) * storage.size());
}

TEST(ZeroCopyViewsTest, ImageSubviewSharesParentBuffer) {
    std::array<std::byte, 4 * 4 * 3> storage{};
    auto image = RideShield::core::ImageView(storage.data(), 4, 4, RideShield::core::PixelFormat::kBgr8);
    auto crop = image.subview(1, 1, 2, 2);

    EXPECT_EQ(crop.width(), 2);
    EXPECT_EQ(crop.height(), 2);
    EXPECT_EQ(crop.row_stride_bytes(), image.row_stride_bytes());
    EXPECT_EQ(crop.data(), image.data() + image.row_stride_bytes() + static_cast<std::ptrdiff_t>(image.pixel_stride_bytes()));
}

TEST(ZeroCopyViewsTest, ContiguousImageProducesSpan) {
    std::array<std::byte, 2 * 2 * 3> storage{};
    auto image = RideShield::core::ImageView(storage.data(), 2, 2, RideShield::core::PixelFormat::kBgr8);

    auto bytes = image.span<std::byte>();
    EXPECT_EQ(bytes.size(), storage.size());
}