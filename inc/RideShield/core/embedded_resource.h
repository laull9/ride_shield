#pragma once

#include <cstddef>
#include <span>

namespace RideShield {

/// 兼容旧的静态资源 span 帮助函数。
/// 新代码请直接包含 resources.h 并使用 RideShield::resources 下的访问器。
/// @tparam symbol_data  外部符号 xxx_data（由汇编 .incbin 生成）
/// @tparam symbol_end   外部符号 xxx_end
///
/// 典型用法：
///   extern const unsigned char yolo_model_data[];
///   extern const unsigned char yolo_model_end[];
///   auto model = RideShield::embedded_span(yolo_model_data, yolo_model_end);
inline auto embedded_span(const unsigned char* data, const unsigned char* end)
    -> std::span<const unsigned char> {
    return {data, static_cast<std::size_t>(end - data)};
}

}  // namespace RideShield
