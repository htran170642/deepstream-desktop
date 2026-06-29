#pragma once

#include <functional>
#include <memory>

#include "pipeline/Pipeline.hpp"

namespace dsd {

// Builds the single multi-source Pipeline. Injectable so PipelineManager can be
// unit-tested with a fake factory instead of the compile-time-selected one.
using PipelineFactory = std::function<std::unique_ptr<Pipeline>()>;

// Default factory: a DeepStreamPipeline when built with ENABLE_DEEPSTREAM,
// a FakePipeline otherwise.
std::unique_ptr<Pipeline> createPipeline();

}  // namespace dsd
