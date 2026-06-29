#include "pipeline/PipelineFactory.hpp"

#include "pipeline/FakePipeline.hpp"
#ifdef DSD_WITH_DEEPSTREAM
#include "pipeline/DeepStreamPipeline.hpp"
#endif

namespace dsd {

std::unique_ptr<Pipeline> createPipeline() {
#ifdef DSD_WITH_DEEPSTREAM
    // Real GPU pipeline (Stage 5b): GStreamer + nvstreammux + nvinfer.
    return std::make_unique<DeepStreamPipeline>();
#else
    // Host build: no SDK — synthetic multi-source pipeline.
    return std::make_unique<FakePipeline>();
#endif
}

}  // namespace dsd
