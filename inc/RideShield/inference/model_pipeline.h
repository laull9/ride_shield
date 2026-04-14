#pragma once

#include "RideShield/core/image_view.h"
#include "RideShield/core/tensor_view.h"
#include "RideShield/inference/onnx_zero_copy.h"

#include <memory>
#include <string>
#include <vector>

#ifdef RIDESHIELD_HAS_ONNXRUNTIME
#include <filesystem>
#include <span>
#endif

namespace RideShield::inference {

struct ModelTensorSpec {
    std::string name;
    RideShield::core::TensorElementType element_type;
    std::vector<std::int64_t> shape;
};

struct ModelPipelineSpec {
    std::string name;
    std::vector<ModelTensorSpec> inputs;
    std::vector<std::string> outputs;
};

class IImagePreprocessor {
public:
    virtual ~IImagePreprocessor() = default;
    virtual auto prepare(const RideShield::core::ImageView& image) -> std::vector<NamedTensorView> = 0;
};

#ifdef RIDESHIELD_HAS_ONNXRUNTIME

class IOnnxPostprocessor {
public:
    virtual ~IOnnxPostprocessor() = default;
    virtual auto decode(std::span<const Ort::Value> outputs) -> std::vector<std::string> = 0;
};

class OnnxModelPipeline {
public:
    OnnxModelPipeline(
        std::filesystem::path model_path,
        ModelPipelineSpec spec,
        std::unique_ptr<IImagePreprocessor> preprocessor,
        std::unique_ptr<IOnnxPostprocessor> postprocessor,
        OnnxSession::Config session_config = {}
    );

    [[nodiscard]] auto spec() const -> const ModelPipelineSpec&;
    auto run(const RideShield::core::ImageView& image) -> std::vector<std::string>;

private:
    ModelPipelineSpec spec_;
    std::unique_ptr<IImagePreprocessor> preprocessor_;
    std::unique_ptr<IOnnxPostprocessor> postprocessor_;
    OnnxSession session_;
};

#endif

}  // namespace RideShield::inference