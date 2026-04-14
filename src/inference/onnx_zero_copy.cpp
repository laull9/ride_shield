#include "RideShield/inference/onnx_zero_copy.h"

#ifdef RIDESHIELD_HAS_ONNXRUNTIME

#include <stdexcept>

namespace RideShield::inference {

namespace {

auto to_onnx_type(const RideShield::core::TensorElementType type) -> ONNXTensorElementDataType {
    switch (type) {
    case RideShield::core::TensorElementType::kFloat32:
        return ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT;
    case RideShield::core::TensorElementType::kUint8:
        return ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8;
    case RideShield::core::TensorElementType::kInt64:
        return ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64;
    }

    throw std::invalid_argument("Unsupported tensor element type");
}

}  // namespace

auto make_session_options(const OnnxSession::Config& config) -> Ort::SessionOptions {
    Ort::SessionOptions options;
    options.SetIntraOpNumThreads(static_cast<int>(config.intra_op_threads));
    options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);

    if (config.enable_cpu_mem_arena) {
        options.EnableCpuMemArena();
    } else {
        options.DisableCpuMemArena();
    }

    return options;
}

auto borrow_ort_value(const RideShield::core::TensorView& tensor, Ort::MemoryInfo& memory_info) -> Ort::Value {
    return Ort::Value::CreateTensor(
        memory_info,
        const_cast<void*>(tensor.data()),
        tensor.byte_size(),
        tensor.shape().data(),
        tensor.shape().size(),
        to_onnx_type(tensor.type())
    );
}

OnnxSession::OnnxSession(const std::filesystem::path& model_path)
    : OnnxSession(model_path, Config{}) {}

OnnxSession::OnnxSession(const std::filesystem::path& model_path, Config config)
    : env_(ORT_LOGGING_LEVEL_WARNING, "RideShield"),
      session_options_(make_session_options(config)),
      session_(env_, model_path.c_str(), session_options_) {
    Ort::AllocatorWithDefaultOptions allocator;
    const auto input_count = session_.GetInputCount();
    input_names_.reserve(input_count);
    for (std::size_t index = 0; index < input_count; ++index) {
        auto name = session_.GetInputNameAllocated(index, allocator);
        input_names_.emplace_back(name.get());
    }

    const auto output_count = session_.GetOutputCount();
    output_names_.reserve(output_count);
    for (std::size_t index = 0; index < output_count; ++index) {
        auto name = session_.GetOutputNameAllocated(index, allocator);
        output_names_.emplace_back(name.get());
    }
}

auto OnnxSession::input_names() const -> const std::vector<std::string>& {
    return input_names_;
}

auto OnnxSession::output_names() const -> const std::vector<std::string>& {
    return output_names_;
}

auto OnnxSession::run(std::span<const NamedTensorView> inputs) -> std::vector<Ort::Value> {
    Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

    std::vector<const char*> input_name_views;
    input_name_views.reserve(inputs.size());

    std::vector<Ort::Value> input_values;
    input_values.reserve(inputs.size());

    for (const auto& input : inputs) {
        input_name_views.push_back(input.name.c_str());
        input_values.emplace_back(borrow_ort_value(input.view, memory_info));
    }

    std::vector<const char*> output_name_views;
    output_name_views.reserve(output_names_.size());
    for (const auto& output_name : output_names_) {
        output_name_views.push_back(output_name.c_str());
    }

    Ort::RunOptions run_options;
    return session_.Run(
        run_options,
        input_name_views.data(),
        input_values.data(),
        input_values.size(),
        output_name_views.data(),
        output_name_views.size()
    );
}

}  // namespace RideShield::inference

#endif