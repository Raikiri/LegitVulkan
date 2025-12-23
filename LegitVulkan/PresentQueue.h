namespace legit
{
  //This follows the sync pattern described in https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html
  //acquireToSubmitSemaphore == acquire_semaphore
  //submitToPresentSemaphore == submit_semaphore
  struct PresentQueue
  {
    PresentQueue(legit::Core *core, legit::WindowDesc windowDesc, glm::uvec2 defaultSize, uint32_t desiredImagesCount, vk::PresentModeKHR preferredMode)
    {
      this->core = core;
      this->swapchain = core->CreateSwapchain(windowDesc, defaultSize, desiredImagesCount, preferredMode);

      for (size_t imageIndex = 0; imageIndex < swapchain->GetImagesCount(); imageIndex++)
      {
        auto swapchainImageView = swapchain->GetImageView(imageIndex);
        SwapchainImageResources res;
        res.submitToPresentSemaphore = core->CreateVulkanSemaphore();
        res.imageViewProxy = core->GetRenderGraph()->AddExternalImageView(swapchainImageView);
        swapchainImageResources.emplace_back(std::move(res));
      }
      this->swapchainRect = vk::Rect2D(vk::Offset2D(), swapchain->GetSize());
    }
    struct AcquiredSwapchainImage
    {
      legit::RenderGraph::ImageViewProxyId imageViewProxyId;
      legit::ImageView *imageView;
      vk::Semaphore submitToPresentSemaphore;
    };
    AcquiredSwapchainImage AcquireImage(vk::Semaphore acquireToSubmitSempahores)
    {
      assert(acquiredImageIndex == uint32_t(-1));
      this->acquiredImageIndex = swapchain->AcquireNextImage(acquireToSubmitSempahores).value;
      AcquiredSwapchainImage acquiredImage;
      acquiredImage.imageViewProxyId = swapchainImageResources[acquiredImageIndex].imageViewProxy->Id();
      acquiredImage.imageView = swapchain->GetImageView(acquiredImageIndex);
      acquiredImage.submitToPresentSemaphore = swapchainImageResources[acquiredImageIndex].submitToPresentSemaphore.get();
      return acquiredImage;
    }
    void PresentAcquiredImage()
    {
      assert(acquiredImageIndex != uint32_t(-1));
      vk::SwapchainKHR swapchains[] = { swapchain->GetHandle() };
      vk::Semaphore waitSemaphores[] = { swapchainImageResources[acquiredImageIndex].submitToPresentSemaphore.get() };
      auto presentInfo = vk::PresentInfoKHR()
        .setSwapchainCount(1)
        .setPSwapchains(swapchains)
        .setPImageIndices(&acquiredImageIndex)
        .setPResults(nullptr)
        .setWaitSemaphoreCount(1)
        .setPWaitSemaphores(waitSemaphores);
      auto res = core->GetPresentQueue().presentKHR(presentInfo);
      acquiredImageIndex = uint32_t(-1);
    }
    vk::Extent2D GetImageSize()
    {
      return swapchain->GetSize();
    }
  private:
    uint32_t acquiredImageIndex = uint32_t (-1);
    legit::Core *core;
    vk::Rect2D swapchainRect;
    
    std::unique_ptr<legit::Swapchain> swapchain;

    struct SwapchainImageResources
    {
      vk::UniqueSemaphore submitToPresentSemaphore;
      RenderGraph::ImageViewProxyUnique imageViewProxy;
    };
    std::vector<SwapchainImageResources> swapchainImageResources;
  };
  
  struct InFlightQueue
  {
    InFlightQueue(legit::Core *core, legit::WindowDesc windowDesc, glm::uvec2 defaultSize, uint32_t inFlightCount, uint32_t desiredSwapchainImageCount, vk::PresentModeKHR preferredMode, bool waitForPreviousFrame = false)
    {
      this->core = core;
      this->memoryPool = std::make_unique<legit::ShaderMemoryPool>(core->GetDynamicMemoryAlignment());
      this->waitForPreviousFrame = waitForPreviousFrame;
      presentQueue.reset(new PresentQueue(core, windowDesc, defaultSize, desiredSwapchainImageCount, preferredMode));

      for (size_t frameIndex = 0; frameIndex < inFlightCount; frameIndex++)
      {
        FrameResources frame;
        frame.submitToRecordFence = core->CreateFence(true);
        frame.acquireToSubmitSemaphore = core->CreateVulkanSemaphore();
        if(waitForPreviousFrame)
          frame.thisToNextFrameSemaphore = core->CreateVulkanSemaphore();

        frame.commandBuffer = std::move(core->AllocateCommandBuffers(1)[0]);
        core->SetObjectDebugName(frame.commandBuffer.get(), std::string("Frame") + std::to_string(frameIndex) + " command buffer");
        frame.shaderMemoryBuffer = std::unique_ptr<legit::Buffer>(new legit::Buffer(core->GetPhysicalDevice(), core->GetLogicalDevice(), 100000000, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostCoherent));
        frame.gpuProfiler = std::unique_ptr<legit::GpuProfiler>(new legit::GpuProfiler(core->GetPhysicalDevice(), core->GetLogicalDevice(), 4096));
        frames.push_back(std::move(frame));
      }

      frameIndex = 0;
    }
    vk::Extent2D GetImageSize()
    {
      return presentQueue->GetImageSize();
    }
    size_t GetInFlightFramesCount()
    {
      return frames.size();
    }
    struct FrameInfo
    {
      legit::ShaderMemoryPool *memoryPool;
      size_t frameIndex;
      legit::RenderGraph::ImageViewProxyId swapchainImageViewProxyId;
      legit::ImageView *swapchainImageView;
    };

    FrameInfo BeginFrame()
    {
      this->profilerFrameId = cpuProfiler.StartFrame();

      auto &currFrame = frames[frameIndex];
      {
        auto fenceTask = cpuProfiler.StartScopedTask("WaitForFence", legit::Colors::pomegranate);
        core->WaitForFence(currFrame.submitToRecordFence.get());
        core->ResetFence(currFrame.submitToRecordFence.get());
      }

      {
        auto imageAcquireTask = cpuProfiler.StartScopedTask("ImageAcquire", legit::Colors::emerald);
        this->acquiredSwapchainImage = presentQueue->AcquireImage(currFrame.acquireToSubmitSemaphore.get());
      }

      {
        auto gpuGatheringTask = cpuProfiler.StartScopedTask("GpuPrfGathering", legit::Colors::amethyst);
        currFrame.gpuProfiler->GatherTimestamps();
      }

      currFrame.transientCommandPool.reset();
      auto commandPoolInfo = vk::CommandPoolCreateInfo()
        .setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer | vk::CommandPoolCreateFlagBits::eTransient)
        .setQueueFamilyIndex(core->GetQueueFamilyIndices().graphicsFamilyIndex);
      currFrame.transientCommandPool = core->GetLogicalDevice().createCommandPoolUnique(commandPoolInfo);
      
      core->GetRenderGraph()->AddPass(legit::RenderGraph::FrameSyncBeginPassDesc());

      memoryPool->MapBuffer(currFrame.shaderMemoryBuffer.get());

      FrameInfo frameInfo;
      frameInfo.memoryPool = memoryPool.get();
      frameInfo.frameIndex = frameIndex;
      frameInfo.swapchainImageViewProxyId = acquiredSwapchainImage.imageViewProxyId;
      frameInfo.swapchainImageView = acquiredSwapchainImage.imageView;

      return frameInfo;
    }
    void EndFrame()
    {
      const auto &currFrame = frames[frameIndex];

      core->GetRenderGraph()->AddImagePresent(acquiredSwapchainImage.imageViewProxyId);
      core->GetRenderGraph()->AddPass(legit::RenderGraph::FrameSyncEndPassDesc());

      auto bufferBeginInfo = vk::CommandBufferBeginInfo()
        .setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse);
      currFrame.commandBuffer->begin(bufferBeginInfo);
      {
        auto gpuFrame = currFrame.gpuProfiler->StartScopedFrame(currFrame.commandBuffer.get());
        core->GetRenderGraph()->Execute(core->GetLogicalDevice(), currFrame.transientCommandPool.get(), core->GetDescriptorSetCache(), memoryPool.get(), currFrame.commandBuffer.get(), &cpuProfiler, currFrame.gpuProfiler.get());
      }
      currFrame.commandBuffer->end();

      memoryPool->UnmapBuffer();

      {
        auto presentTask = cpuProfiler.StartScopedTask("Submit", legit::Colors::amethyst);
        std::vector<vk::Semaphore> waitSemaphores = { currFrame.acquireToSubmitSemaphore.get() };
        std::vector<vk::PipelineStageFlags> waitStages = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
        std::vector<vk::Semaphore> signalSemaphores = { acquiredSwapchainImage.submitToPresentSemaphore };

        if (this->waitForPreviousFrame)
        {
          if (previousFrameIndex != size_t(-1))
          {
            const auto& prevFrame = frames[previousFrameIndex];
            waitSemaphores.push_back(prevFrame.thisToNextFrameSemaphore.get());
            waitStages.push_back(vk::PipelineStageFlagBits::eAllCommands);
          }
          signalSemaphores.push_back(currFrame.thisToNextFrameSemaphore.get());
        }


        auto submitInfo = vk::SubmitInfo()
          .setWaitSemaphores(waitSemaphores)
          .setWaitDstStageMask(waitStages)
          .setCommandBuffers({ currFrame.commandBuffer.get() })
          .setSignalSemaphores(signalSemaphores);

        core->GetGraphicsQueue().submit({ submitInfo }, currFrame.submitToRecordFence.get());
      }

      {
        auto presentTask = cpuProfiler.StartScopedTask("Present", legit::Colors::alizarin);
        presentQueue->PresentAcquiredImage();
      }
      previousFrameIndex = frameIndex;
      frameIndex = (frameIndex + 1) % frames.size();

      cpuProfiler.EndFrame(profilerFrameId);
      lastFrameCpuProfilerTasks = cpuProfiler.GetProfilerTasks();
    }
    const std::vector<legit::ProfilerTask> &GetLastFrameCpuProfilerData()
    {
      return lastFrameCpuProfilerTasks;
    }
    const std::vector<legit::ProfilerTask> &GetLastFrameGpuProfilerData()
    {
      return frames[frameIndex].gpuProfiler->GetProfilerTasks();
    }
    CpuProfiler &GetCpuProfiler()
    {
      return cpuProfiler;
    }
  private:
    std::unique_ptr<legit::ShaderMemoryPool> memoryPool;
    std::unique_ptr<PresentQueue> presentQueue;
    bool waitForPreviousFrame = false;

    std::map<legit::ImageView *, legit::RenderGraph::ImageViewProxyUnique> swapchainImageViewProxies;

    struct FrameResources
    {
      vk::UniqueSemaphore thisToNextFrameSemaphore;
      vk::UniqueSemaphore acquireToSubmitSemaphore;
      vk::UniqueFence submitToRecordFence;

      vk::UniqueCommandBuffer commandBuffer;
      vk::UniqueCommandPool transientCommandPool;
      std::unique_ptr<legit::Buffer> shaderMemoryBuffer;
      std::unique_ptr<legit::GpuProfiler> gpuProfiler;
    };
    std::vector<FrameResources> frames;
    size_t frameIndex = 0;
    size_t previousFrameIndex = size_t(-1);

    legit::Core *core;
    PresentQueue::AcquiredSwapchainImage acquiredSwapchainImage;
    legit::CpuProfiler cpuProfiler;
    std::vector<legit::ProfilerTask> lastFrameCpuProfilerTasks;

    size_t profilerFrameId;
  };
}