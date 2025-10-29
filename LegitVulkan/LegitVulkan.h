#pragma once

#if defined(WIN32)
  #define NOMINMAX
  #define VK_USE_PLATFORM_WIN32_KHR
#else
  #define VK_USE_PLATFORM_WAYLAND_KHR
#endif

//#include <vulkan/vulkan.h>
//#include <vulkan/vk_sdk_platform.h>
#include <vulkan/vulkan.hpp>

//#include <Windows.h>

#include <glm/glm.hpp>
#include "Span.h"
#include "Handles.h"
#include "Pool.h"
#include "CpuProfiler.h"
#include "QueueIndices.h"
#include "WindowDesc.h"
#include "Surface.h"
#include "VertexDeclaration.h"
#include "ShaderModule.h"
#include "ShaderProgram.h"
#include "Synchronization.h"
#include "Buffer.h"
#include "Image.h"
#include "TimestampQuery.h"
#include "GpuProfiler.h"
#include "Sampler.h"
#include "Pipeline.h"
#include "ImageView.h"
#include "Swapchain.h"
#include "RenderPass.h"
#include "Framebuffer.h"
#include "ShaderMemoryPool.h"
#include "DescriptorSetCache.h"
#include "PipelineCache.h"
#include "RenderPassCache.h"

#include "Core.h"
#include "StateTracker.h"
#include "RenderGraph.h"
#include "CoreImpl.h"

#include "PresentQueue.h"

#include "StagedResources.h"
#include "ImageLoader.h"
#include "Texture.h"