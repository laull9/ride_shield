#include "RideShield/inference/model_pipeline.h"

#ifdef RIDESHIELD_HAS_ONNXRUNTIME

#include <stdexcept>

namespace RideShield::inference {

OnnxModelPipeline::OnnxModelPipeline(
    std::filesystem::path model_path,
    ModelPipelineSpec spec,
    std::unique_ptr<IImagePreprocessor> preprocessor,
    std::unique_ptr<IOnnxPostprocessor> postprocessor,
    OnnxSession::Config session_config)
    : spec_(std::move(spec)),
      preprocessor_(std::move(preprocessor)),
      postprocessor_(std::move(postprocessor)),
      session_(std::move(model_path), session_config) {
    if (!preprocessor_) {
        throw std::invalid_argument("OnnxModelPipeline requires a preprocessor");
    }

    if (!postprocessor_) {
        throw std::invalid_argument("OnnxModelPipeline requires a postprocessor");
    }
}

auto OnnxModelPipeline::spec() const -> const ModelPipelineSpec& {
    return spec_;
}

auto OnnxModelPipeline::run(const RideShield::core::ImageView& image) -> std::vector<std::string> {
    auto inputs = preprocessor_->prepare(image);

    if (inputs.empty()) {
        throw std::runtime_error("Preprocessor returned no tensors");
    }

    auto outputs = session_.run(inputs);
    return postprocessor_->decode(outputs);
}

}  // namespace RideShield::inference

#endif