#pragma once

#include "RideShield/core/tensor_view.h"

#include <string>

#ifdef RIDESHIELD_HAS_ONNXRUNTIME
#include <filesystem>
#include <span>
#include <vector>
#include <onnxruntime_cxx_api.h>
#endif

namespace RideShield::inference {

struct NamedTensorView {
    std::string name;
    RideShield::core::TensorView view;
};

#ifdef RIDESHIELD_HAS_ONNXRUNTIME

auto borrow_ort_value(const RideShield::core::TensorView& tensor, Ort::MemoryInfo& memory_info) -> Ort::Value;

class OnnxSession {
public:
    struct Config {
        std::size_t intra_op_threads = 1;
        bool enable_cpu_mem_arena = true;
    };

    OnnxSession(const std::filesystem::path& model_path, Config config);
    explicit OnnxSession(const std::filesystem::path& model_path);

    [[nodiscard]] auto input_names() const -> const std::vector<std::string>&;
    [[nodiscard]] auto output_names() const -> const std::vector<std::string>&;
    auto run(std::span<const NamedTensorView> inputs) -> std::vector<Ort::Value>;

private:
    Ort::Env env_;
    Ort::SessionOptions session_options_;
    Ort::Session session_;
    std::vector<std::string> input_names_;
    std::vector<std::string> output_names_;
};

#endif

}  // namespace RideShield::inference