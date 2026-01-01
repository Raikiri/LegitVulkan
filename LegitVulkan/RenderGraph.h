#include "RenderPassCache.h"

namespace legit
{
  template<typename T>
  void AppendVectors(std::vector<T> &a, const std::vector<T> &b)
  {
    a.insert(a.end(), b.begin(), b.end());
  }
  
  class RenderGraph;

  template<typename Base>
  struct RenderGraphProxyId
  {
    RenderGraphProxyId() :
      id(size_t(-1))
    {
    }

    bool operator ==(const RenderGraphProxyId<Base> &other) const
    {
      return this->id == other.id;
    }
    bool IsValid()
    {
      return id != size_t(-1);
    }
  private:
    explicit RenderGraphProxyId(size_t _id) : id(_id) {}
    size_t id;
    friend class RenderGraph;
  };

  class ImageCache
  {
  public:
    ImageCache(vk::PhysicalDevice _physicalDevice, vk::Device _logicalDevice, vk::detail::DispatchLoaderDynamic _loader) : physicalDevice(_physicalDevice), logicalDevice(_logicalDevice), loader(_loader)
    {}

    struct ImageKey
    {
      bool operator < (const ImageKey &other) const
      {
        auto lUsageFlags = VkMemoryMapFlags(usageFlags);
        auto rUsageFlags = VkMemoryMapFlags(other.usageFlags);
        return std::tie(format, mipsCount, arrayLayersCount, lUsageFlags, size.x, size.y, size.z) < std::tie(other.format, other.mipsCount, other.arrayLayersCount, rUsageFlags, other.size.x, other.size.y, other.size.z);
      }
      vk::Format format;
      vk::ImageUsageFlags usageFlags;
      uint32_t mipsCount;
      uint32_t arrayLayersCount;
      glm::uvec3 size;
      std::string debugName;
    };

    void Release()
    {
      for (auto &cacheEntry : imageCache)
      {
        cacheEntry.second.usedCount = 0;
      }
    }

    void PurgeUnused()
    {
      std::vector<ImageKey> toRemoveKeys;
      for (auto& cacheEntry : imageCache)
      {
        assert(cacheEntry.second.usedCount <= cacheEntry.second.images.size());
        cacheEntry.second.images.resize(cacheEntry.second.usedCount);
        if (cacheEntry.second.usedCount == 0)
          toRemoveKeys.push_back(cacheEntry.first);
      }
      for (const auto& toRemoveKey: toRemoveKeys)
      {
        imageCache.erase(toRemoveKey);
      }
    }

    legit::ImageData *GetImage(ImageKey imageKey)
    {
      auto &cacheEntry = imageCache[imageKey];
      if (cacheEntry.usedCount + 1 > cacheEntry.images.size())
      {
        vk::ImageCreateInfo imageCreateInfo;
        if(imageKey.size.z == glm::u32(-1))
          imageCreateInfo = legit::Image::CreateInfo2d(glm::uvec2(imageKey.size.x, imageKey.size.y), imageKey.mipsCount, imageKey.arrayLayersCount, imageKey.format, imageKey.usageFlags);
        else
          imageCreateInfo = legit::Image::CreateInfoVolume(imageKey.size, imageKey.mipsCount, imageKey.arrayLayersCount, imageKey.format, imageKey.usageFlags);
        auto newImage = std::unique_ptr<legit::Image>(new legit::Image(physicalDevice, logicalDevice, imageCreateInfo));
        Core::SetObjectDebugName(logicalDevice, loader, newImage->GetImageData()->GetHandle(), imageKey.debugName);
        cacheEntry.images.emplace_back(std::move(newImage));
      }
      return cacheEntry.images[cacheEntry.usedCount++]->GetImageData();
    }
  private:
    struct ImageCacheEntry
    {
      ImageCacheEntry() : usedCount(0) {}
      std::vector<std::unique_ptr<legit::Image> > images;
      size_t usedCount;
    };
    std::map<ImageKey, ImageCacheEntry> imageCache;
    vk::PhysicalDevice physicalDevice;
    vk::Device logicalDevice;
    vk::detail::DispatchLoaderDynamic loader;
  };

  class ImageViewCache
  {
  public:
    ImageViewCache(vk::PhysicalDevice _physicalDevice, vk::Device _logicalDevice) : physicalDevice(_physicalDevice), logicalDevice(_logicalDevice)
    {}
    struct ImageViewKey
    {
      legit::ImageData *image;
      ImageSubresourceRange subresourceRange;
      std::string debugName;
      bool operator < (const ImageViewKey &other) const
      {
        return std::tie(image, subresourceRange) < std::tie(other.image, other.subresourceRange);
      }
    };

    legit::ImageView *GetImageView(ImageViewKey imageViewKey)
    {
      auto &imageView = imageViewCache[imageViewKey];
      if (!imageView)
        imageView = std::unique_ptr<legit::ImageView>(new legit::ImageView(
          logicalDevice,
          imageViewKey.image,
          imageViewKey.subresourceRange.baseMipLevel,
          imageViewKey.subresourceRange.mipsCount,
          imageViewKey.subresourceRange.baseArrayLayer,
          imageViewKey.subresourceRange.arrayLayersCount));
      return imageView.get();
    }
  private:
    std::map<ImageViewKey, std::unique_ptr<legit::ImageView> > imageViewCache;
    vk::PhysicalDevice physicalDevice;
    vk::Device logicalDevice;
  };




  class BufferCache
  {
  public:
    BufferCache(vk::PhysicalDevice _physicalDevice, vk::Device _logicalDevice) : physicalDevice(_physicalDevice), logicalDevice(_logicalDevice)
    {}

    struct BufferKey
    {
      uint32_t elementSize;
      uint32_t elementsCount;
      bool operator < (const BufferKey &other) const
      {
        return std::tie(elementSize, elementsCount) < std::tie(other.elementSize, other.elementsCount);
      }
    };

    void Release()
    {
      for (auto &cacheEntry : bufferCache)
      {
        cacheEntry.second.usedCount = 0;
      }
    }

    void PurgeUnused()
    {
      std::vector<BufferKey> toRemoveKeys;
      for (auto& cacheEntry : bufferCache)
      {
        assert(cacheEntry.second.usedCount <= cacheEntry.second.buffers.size());
        cacheEntry.second.buffers.resize(cacheEntry.second.usedCount);
        if (cacheEntry.second.usedCount == 0)
          toRemoveKeys.push_back(cacheEntry.first);
      }
      for (const auto& toRemoveKey : toRemoveKeys)
      {
        bufferCache.erase(toRemoveKey);
      }
    }

    legit::Buffer *GetBuffer(BufferKey bufferKey)
    {
      auto &cacheEntry = bufferCache[bufferKey];
      if (cacheEntry.usedCount + 1 > cacheEntry.buffers.size())
      {
        auto newBuffer = std::unique_ptr<legit::Buffer>(new legit::Buffer(
          physicalDevice,
          logicalDevice,
          bufferKey.elementSize * bufferKey.elementsCount,
          vk::BufferUsageFlagBits::eStorageBuffer,
          vk::MemoryPropertyFlagBits::eDeviceLocal));
        cacheEntry.buffers.emplace_back(std::move(newBuffer));
      }
      return cacheEntry.buffers[cacheEntry.usedCount++].get();
    }
  private:
    struct BufferCacheEntry
    {
      BufferCacheEntry() : usedCount(0) {}
      std::vector<std::unique_ptr<legit::Buffer> > buffers;
      size_t usedCount;
    };
    std::map<BufferKey, BufferCacheEntry> bufferCache;
    vk::PhysicalDevice physicalDevice;
    vk::Device logicalDevice;
  };


  class RenderGraph
  {
  private:
    struct ImageProxy;
    using ImageProxyPool = Utils::Pool<ImageProxy>;

    struct ImageViewProxy;
    using ImageViewProxyPool = Utils::Pool<ImageViewProxy>;

    struct BufferProxy;
    using BufferProxyPool = Utils::Pool<BufferProxy>;
  public:
    using ImageProxyId = ImageProxyPool::Id;
    using ImageViewProxyId = ImageViewProxyPool::Id;
    using BufferProxyId = BufferProxyPool::Id;

    struct ImageHandleInfo
    {
      ImageHandleInfo() {}
      ImageHandleInfo(RenderGraph *renderGraph, ImageProxyId imageProxyId)
      {
        this->renderGraph = renderGraph;
        this->imageProxyId = imageProxyId;
      }
      void Reset()
      {
        renderGraph->DeleteImage(imageProxyId);
      }
      ImageProxyId Id() const
      {
        return imageProxyId;
      }
      void SetDebugName(std::string name) const
      {
        renderGraph->SetImageProxyDebugName(imageProxyId, name);
      }
    private:
      RenderGraph *renderGraph;
      ImageProxyId imageProxyId;
      friend class RenderGraph;
    };

    struct ImageViewHandleInfo
    {
      ImageViewHandleInfo() {}
      ImageViewHandleInfo(RenderGraph *renderGraph, ImageViewProxyId imageViewProxyId)
      {
        this->renderGraph = renderGraph;
        this->imageViewProxyId = imageViewProxyId;
      }
      void Reset()
      {
        renderGraph->DeleteImageView(imageViewProxyId);
      }
      ImageViewProxyId Id() const
      {
        return imageViewProxyId;
      }
      void SetDebugName(std::string name) const
      {
        renderGraph->SetImageViewProxyDebugName(imageViewProxyId, name);
      }
    private:
      RenderGraph *renderGraph;
      ImageViewProxyId imageViewProxyId;
      friend class RenderGraph;
    };
    struct BufferHandleInfo
    {
      BufferHandleInfo() {}
      BufferHandleInfo(RenderGraph *renderGraph, BufferProxyId bufferProxyId)
      {
        this->renderGraph = renderGraph;
        this->bufferProxyId = bufferProxyId;
      }
      void Reset()
      {
        renderGraph->DeleteBuffer(bufferProxyId);
      }
      BufferProxyId Id() const
      {
        return bufferProxyId;
      }
    private:
      RenderGraph *renderGraph;
      BufferProxyId bufferProxyId;
      friend class RenderGraph;
    };

  public:
    RenderGraph(vk::PhysicalDevice _physicalDevice, vk::Device _logicalDevice, vk::detail::DispatchLoaderDynamic _loader) :
      physicalDevice(_physicalDevice),
      logicalDevice(_logicalDevice),
      loader(_loader),
      renderPassCache(_logicalDevice),
      framebufferCache(_logicalDevice),
      imageCache(_physicalDevice, _logicalDevice, _loader),
      imageViewCache(_physicalDevice, _logicalDevice),
      bufferCache(_physicalDevice, _logicalDevice)
    {
    }

    using ImageProxyUnique = UniqueHandle<ImageHandleInfo, RenderGraph>;
    using ImageViewProxyUnique = UniqueHandle<ImageViewHandleInfo, RenderGraph>;
    using BufferProxyUnique = UniqueHandle<BufferHandleInfo, RenderGraph>;


    ImageProxyUnique AddImage(vk::Format format, uint32_t mipsCount, uint32_t arrayLayersCount, glm::uvec2 size, vk::ImageUsageFlags usageFlags)
    {
      return AddImage(format, mipsCount, arrayLayersCount, glm::uvec3(size.x, size.y, -1), usageFlags);
    }
    ImageProxyUnique AddImage(vk::Format format, uint32_t mipsCount, uint32_t arrayLayersCount, glm::uvec3 size, vk::ImageUsageFlags usageFlags)
    {
      ImageProxy imageProxy;
      imageProxy.type = ImageProxy::Types::Transient;
      imageProxy.imageKey.format = format;
      imageProxy.imageKey.usageFlags = usageFlags;
      imageProxy.imageKey.mipsCount = mipsCount;
      imageProxy.imageKey.arrayLayersCount = arrayLayersCount;
      imageProxy.imageKey.size = size;
      
      imageProxy.externalImage = nullptr;
      auto uniqueProxyHandle = ImageProxyUnique(ImageHandleInfo(this, imageProxies.Add(std::move(imageProxy))));
      std::string debugName = std::string("Graph image [") + std::to_string(size.x) + ", " + std::to_string(size.y) + ", Id=" + std::to_string(uniqueProxyHandle->Id().asInt) + "]" + vk::to_string(imageProxy.imageKey.format);
      uniqueProxyHandle->SetDebugName(debugName);
      return uniqueProxyHandle;
    }
    ImageProxyUnique AddExternalImage(legit::ImageData *image)
    {
      ImageProxy imageProxy;
      imageProxy.type = ImageProxy::Types::External;
      imageProxy.externalImage = image;
      imageProxy.imageKey.debugName = "External graph image";
      return ImageProxyUnique(ImageHandleInfo(this, imageProxies.Add(std::move(imageProxy))));
    }
    ImageViewProxyUnique AddImageView(ImageProxyId imageProxyId, uint32_t baseMipLevel, uint32_t mipLevelsCount, uint32_t baseArrayLayer, uint32_t arrayLayersCount)
    {
      ImageViewProxy imageViewProxy;
      imageViewProxy.externalView = nullptr;
      imageViewProxy.type = ImageViewProxy::Types::Transient;
      imageViewProxy.imageProxyId = imageProxyId;
      imageViewProxy.subresourceRange.baseMipLevel = baseMipLevel;
      imageViewProxy.subresourceRange.mipsCount = mipLevelsCount;
      imageViewProxy.subresourceRange.baseArrayLayer = baseArrayLayer;
      imageViewProxy.subresourceRange.arrayLayersCount = arrayLayersCount;
      imageViewProxy.debugName = "View";
      return ImageViewProxyUnique(ImageViewHandleInfo(this, imageViewProxies.Add(std::move(imageViewProxy))));
    }
    ImageViewProxyUnique AddExternalImageView(legit::ImageView *imageView, legit::ImageUsageTypes usageType = legit::ImageUsageTypes::Unknown)
    {
      ImageViewProxy imageViewProxy;
      imageViewProxy.externalView = imageView;
      imageViewProxy.externalUsageType = usageType;
      imageViewProxy.type = ImageViewProxy::Types::External;
      imageViewProxy.imageProxyId = ImageProxyId();
      imageViewProxy.debugName = "External view";
      return ImageViewProxyUnique(ImageViewHandleInfo(this, imageViewProxies.Add(std::move(imageViewProxy))));
    }
  private:
    void DeleteImage(ImageProxyId imageId)
    {
      imageProxies.Release(imageId);
    }

    void SetImageProxyDebugName(ImageProxyId imageId, std::string debugName)
    {
      imageProxies.Get(imageId).imageKey.debugName = debugName;
    }
    void SetImageViewProxyDebugName(ImageViewProxyId imageViewId, std::string debugName)
    {
      imageViewProxies.Get(imageViewId).debugName = debugName;
    }

    void DeleteImageView(ImageViewProxyId imageViewId)
    {
      imageViewProxies.Release(imageViewId);
    }

    void DeleteBuffer(BufferProxyId bufferId)
    {
      bufferProxies.Release(bufferId);
    }
  public:

    glm::uvec2 GetMipSize(ImageProxyId imageProxyId, uint32_t mipLevel)
    {
      auto &imageProxy = imageProxies.Get(imageProxyId);
      switch (imageProxy.type)
      {
        case ImageProxy::Types::External:
        {
          return imageProxy.externalImage->GetMipSize(mipLevel);
        }break;
        case ImageProxy::Types::Transient:
        {
          glm::u32 mipMult = (1 << mipLevel);
          return glm::uvec2(imageProxy.imageKey.size.x / mipMult, imageProxy.imageKey.size.y / mipMult);
        }break;
      }
      return glm::uvec2(-1, -1);
    }

    glm::uvec2 GetMipSize(ImageViewProxyId imageViewProxyId, uint32_t mipOffset)
    {
      auto &imageViewProxy = imageViewProxies.Get(imageViewProxyId);
      switch (imageViewProxy.type)
      {
        case ImageViewProxy::Types::External:
        {
          uint32_t mipLevel = imageViewProxy.externalView->GetBaseMipLevel() + mipOffset;
          return imageViewProxy.externalView->GetImageData()->GetMipSize(mipLevel);
        }break;
        case ImageViewProxy::Types::Transient:
        {
          uint32_t mipLevel = imageViewProxy.subresourceRange.baseMipLevel + mipOffset;
          return GetMipSize(imageViewProxy.imageProxyId, mipLevel);
        }break;
      }
      return glm::uvec2(-1, -1);
    }


    template<typename BufferType>
    BufferProxyUnique AddBuffer(uint32_t count)
    {
      BufferProxy bufferProxy;
      bufferProxy.type = BufferProxy::Types::Transient;
      bufferProxy.bufferKey.elementSize = sizeof(BufferType);
      bufferProxy.bufferKey.elementsCount = count;
      bufferProxy.externalBuffer = nullptr;
      return BufferProxyUnique(BufferHandleInfo(this, bufferProxies.Add(std::move(bufferProxy))));
    }

    BufferProxyUnique AddExternalBuffer(legit::Buffer *buffer)
    {
      BufferProxy bufferProxy;
      bufferProxy.type = BufferProxy::Types::External;
      bufferProxy.bufferKey.elementSize = -1;
      bufferProxy.bufferKey.elementsCount = -1;
      bufferProxy.externalBuffer = buffer;
      return BufferProxyUnique(BufferHandleInfo(this, bufferProxies.Add(std::move(bufferProxy))));
    }

    struct PassContext
    {
      legit::ImageView *GetImageView(ImageViewProxyId imageViewProxyId)
      {
        return resolvedImageViews[imageViewProxyId.asInt];
      }
      legit::Buffer *GetBuffer(BufferProxyId bufferProxy)
      {
        return resolvedBuffers[bufferProxy.asInt];
      }
      vk::CommandBuffer GetCommandBuffer()
      {
        return commandBuffer;
      }
    private:
      std::vector<legit::ImageView *> resolvedImageViews;
      std::vector<legit::Buffer *> resolvedBuffers;
      vk::CommandBuffer commandBuffer;
      friend class RenderGraph;
    };

    struct RenderPassContext : public PassContext
    {
      legit::RenderPass *GetRenderPass()
      {
        return renderPass;
      }
    private:
      legit::RenderPass *renderPass;
      friend class RenderGraph;
    };

    struct RenderPassDesc
    {
      RenderPassDesc()
      {
        profilerTaskName = "RenderPass";
        profilerTaskColor = glm::packUnorm4x8(glm::vec4(1.0f, 0.5f, 0.0f, 1.0f));
      }
      struct Attachment
      {
        ImageViewProxyId imageViewProxyId;
        vk::AttachmentLoadOp loadOp;
        vk::ClearValue clearValue;
      };
      RenderPassDesc &SetColorAttachments(
        const std::vector<ImageViewProxyId> &_colorAttachmentViewProxies, 
        vk::AttachmentLoadOp _loadOp = vk::AttachmentLoadOp::eDontCare, 
        vk::ClearValue _clearValue = vk::ClearColorValue(std::array<float, 4>{1.0f, 0.5f, 0.0f, 1.0f}))
      {
        this->colorAttachments.resize(_colorAttachmentViewProxies.size());
        for (size_t index = 0; index < _colorAttachmentViewProxies.size(); index++)
        {
          this->colorAttachments[index] = { _colorAttachmentViewProxies [index], _loadOp, _clearValue};
        }
        return *this;
      }
      RenderPassDesc &SetColorAttachments(std::vector<Attachment> &&_colorAttachments)
      {
        this->colorAttachments = std::move(_colorAttachments);
        return *this;
      }

      RenderPassDesc &SetDepthAttachment(
        ImageViewProxyId _depthAttachmentViewProxyId,
        vk::AttachmentLoadOp _loadOp = vk::AttachmentLoadOp::eDontCare,
        vk::ClearValue _clearValue = vk::ClearDepthStencilValue(1.0f, 0))
      {
        this->depthAttachment.imageViewProxyId = _depthAttachmentViewProxyId;
        this->depthAttachment.loadOp = _loadOp;
        this->depthAttachment.clearValue = _clearValue;
        return *this;
      }
      RenderPassDesc &SetDepthAttachment(Attachment _depthAttachment)
      {
        this->depthAttachment = _depthAttachment;
        return *this;
      }
      RenderPassDesc& SetVertexBuffers(std::vector<BufferProxyId>&& _vertexBufferProxies)
      {
        this->vertexBufferProxies = std::move(_vertexBufferProxies);
        return *this;
      }

      RenderPassDesc &SetInputImages(std::vector<ImageViewProxyId> &&_inputImageViewProxies)
      {
        this->inputImageViewProxies = std::move(_inputImageViewProxies);
        return *this;
      }
      RenderPassDesc &SetStorageBuffers(std::vector<BufferProxyId> &&_inoutStorageBufferProxies)
      {
        this->inoutStorageBufferProxies = _inoutStorageBufferProxies;
        return *this;
      }
      RenderPassDesc &SetStorageImages(std::vector<ImageViewProxyId> &&_inoutStorageImageProxies)
      {
        this->inoutStorageImageProxies = _inoutStorageImageProxies;
        return *this;
      }
      RenderPassDesc &SetRenderAreaExtent(vk::Extent2D _renderAreaExtent)
      {
        this->renderAreaExtent = _renderAreaExtent;
        return *this;
      }

      RenderPassDesc &SetRecordFunc(std::function<void(RenderPassContext)> _recordFunc)
      {
        this->recordFunc = _recordFunc;
        return *this;
      }
      RenderPassDesc &SetProfilerInfo(uint32_t taskColor, std::string taskName)
      {
        this->profilerTaskColor = taskColor;
        this->profilerTaskName = taskName;
        return *this;
      }

      std::vector<Attachment> colorAttachments;
      Attachment depthAttachment;

      std::vector<ImageViewProxyId> inputImageViewProxies;
      std::vector<BufferProxyId> vertexBufferProxies;
      std::vector<BufferProxyId> inoutStorageBufferProxies;
      std::vector<ImageViewProxyId> inoutStorageImageProxies;

      vk::Extent2D renderAreaExtent;
      std::function<void(RenderPassContext)> recordFunc;

      std::string profilerTaskName;
      uint32_t profilerTaskColor;
    };

    struct PassContext2
    {
      struct DescriptorSetBindings
      {
        DescriptorSetBindings(const legit::PipelineCache::PipelineInfo &pipelineInfo, size_t setIndex) :
          setIndex(setIndex),
          shaderDataSetInfo(pipelineInfo.shaderProgram ? pipelineInfo.shaderProgram->GetSetInfo(setIndex) : pipelineInfo.computeShader->GetSetInfo(setIndex)),
          pipelineLayout(pipelineInfo.pipelineLayout)
        {
        }
        DescriptorSetBindings &AddImageSamplerBinding(std::string name, legit::ImageView *imageView, legit::Sampler *sampler)
        {
          imageSamplerBindings.push_back(shaderDataSetInfo->MakeImageSamplerBinding(name, imageView, sampler));
          return *this;
        }
        DescriptorSetBindings &AddTextureBinding(std::string name, legit::ImageView *imageView)
        {
          textureBindings.push_back(shaderDataSetInfo->MakeTextureBinding(name, imageView));
          return *this;
        }
        DescriptorSetBindings &AddSamplerBinding(std::string name, legit::Sampler *sampler)
        {
          samplerBindings.push_back(shaderDataSetInfo->MakeSamplerBinding(name, sampler));
          return *this;
        }
        DescriptorSetBindings &AddStorageImageBinding(std::string name, legit::ImageView *imageView)
        {
          storageImageBindings.push_back(shaderDataSetInfo->MakeStorageImageBinding(name, imageView));
          return *this;
        }
        DescriptorSetBindings &AddStorageBufferBinding(std::string name, legit::Buffer *buffer)
        {
          storageBufferBindings.push_back(shaderDataSetInfo->MakeStorageBufferBinding(name, buffer));
          return *this;
        }
        DescriptorSetBindings &AddStorageBufferBinding(std::string name, std::vector<legit::Buffer*> buffers)
        {
          storageBufferBindings.push_back(shaderDataSetInfo->MakeStorageBufferBinding(name, buffers));
          return *this;
        }
        DescriptorSetBindings &AddAccelerationStructureBinding(std::string name, legit::AccelerationStructure *accelerationStructure)
        {
          accelerationStructureBindings.push_back(shaderDataSetInfo->MakeAccelerationStructureBinding(name, accelerationStructure));
          return *this;
        }
        
        struct UniformBinding
        {
          std::string name;
          const void *data;
          size_t size;
        };
        
        template<typename T>
        DescriptorSetBindings &AddUniformBinding(std::string name, const T *uniformData)
        {
          UniformBinding uniformBinding;
          uniformBinding.name = name;
          uniformBinding.data = uniformData;
          uniformBinding.size = sizeof(T);
          uniformBindings.emplace_back(std::move(uniformBinding));
          return *this;
        }
        std::vector<legit::ImageSamplerBinding> imageSamplerBindings;
        std::vector<legit::TextureBinding> textureBindings;
        std::vector<legit::SamplerBinding> samplerBindings;
        std::vector<legit::StorageImageBinding> storageImageBindings;
        std::vector<legit::StorageBufferBinding> storageBufferBindings;
        std::vector<legit::AccelerationStructureBinding> accelerationStructureBindings;
        std::vector<UniformBinding> uniformBindings;
        const legit::DescriptorSetLayoutKey *shaderDataSetInfo;
        const vk::PipelineLayout pipelineLayout;
        size_t setIndex;
      };
      using BindDescriptorSetFunc = std::function<void(const DescriptorSetBindings &bindings)>;
      
      PassContext2(BindDescriptorSetFunc bindDescriptorSetFunc) :
        bindDescriptorSetFunc(bindDescriptorSetFunc)
      {
      }
      void BindDescriptorSet(const DescriptorSetBindings &bindings)
      {
        bindDescriptorSetFunc(bindings);
      }
      vk::CommandBuffer GetCommandBuffer()
      {
        return commandBuffer;
      }
    private:
      BindDescriptorSetFunc bindDescriptorSetFunc;
      vk::CommandBuffer commandBuffer;
      friend class RenderGraph;
    };
    
    struct RenderPassContext2 : public PassContext2
    {
      using DrawIndirectFunc = std::function<void(legit::Buffer *buf)>;
      RenderPassContext2(PassContext2::BindDescriptorSetFunc bindDescriptorSetFunc, DrawIndirectFunc drawIndirectFunc) :
        PassContext2(bindDescriptorSetFunc),
        drawIndirectFunc(drawIndirectFunc)
      {
      }
      void DrawIndirect(legit::Buffer *indirectBuf)
      {
        drawIndirectFunc(indirectBuf);
      }
      legit::RenderPass *GetRenderPass()
      {
        return renderPass;
      }
    private:
      DrawIndirectFunc drawIndirectFunc;
      legit::RenderPass *renderPass;
      friend class RenderGraph;
    };
    
    struct ComputePassContext2 : public PassContext2
    {
      using DispatchIndirectFunc = std::function<void(legit::Buffer *buf)>;
      ComputePassContext2(PassContext2::BindDescriptorSetFunc bindDescriptorSetFunc, DispatchIndirectFunc dispatchIndirectFunc) :
        PassContext2(bindDescriptorSetFunc),
        dispatchIndirectFunc(dispatchIndirectFunc)
      {
      }
      void DispatchIndirect(legit::Buffer *indirectBuf)
      {
        dispatchIndirectFunc(indirectBuf);
      }
      void DispatchThreads(glm::uvec3 threads_count, glm::uvec3 workgroup_size)
      {
        this->GetCommandBuffer().dispatch(
          (threads_count.x + workgroup_size.x - 1u) / workgroup_size.x,
          (threads_count.y + workgroup_size.y - 1u) / workgroup_size.y,
          (threads_count.z + workgroup_size.z - 1u) / workgroup_size.z
        );
      }
      legit::RenderPass *GetRenderPass()
      {
        return renderPass;
      }
    private:
      DispatchIndirectFunc dispatchIndirectFunc;
      legit::RenderPass *renderPass;
      friend class RenderGraph;
    };
    
    struct RenderPassDesc2
    {
      RenderPassDesc2()
      {
        profilerTaskName = "RenderPass";
        profilerTaskColor = glm::packUnorm4x8(glm::vec4(1.0f, 0.5f, 0.0f, 1.0f));
      }
      struct Attachment
      {
        ImageView *imageView = nullptr;
        vk::AttachmentLoadOp loadOp;
        vk::ClearValue clearValue;
      };
      RenderPassDesc2 &SetColorAttachments(
        const std::vector<ImageView*> _colorAttachments, 
        vk::AttachmentLoadOp _loadOp = vk::AttachmentLoadOp::eDontCare, 
        vk::ClearValue _clearValue = vk::ClearColorValue(std::array<float, 4>{1.0f, 0.5f, 0.0f, 1.0f}))
      {
        this->colorAttachments.resize(_colorAttachments.size());
        for (size_t index = 0; index < _colorAttachments.size(); index++)
        {
          this->colorAttachments[index] = { _colorAttachments [index], _loadOp, _clearValue};
        }
        return *this;
      }
      RenderPassDesc2 &SetColorAttachments(std::vector<Attachment> &&_colorAttachments)
      {
        this->colorAttachments = std::move(_colorAttachments);
        return *this;
      }

      RenderPassDesc2 &SetDepthAttachment(
        ImageView *_depthAttachmentView,
        vk::AttachmentLoadOp _loadOp = vk::AttachmentLoadOp::eDontCare,
        vk::ClearValue _clearValue = vk::ClearDepthStencilValue(1.0f, 0))
      {
        this->depthAttachment.imageView = _depthAttachmentView;
        this->depthAttachment.loadOp = _loadOp;
        this->depthAttachment.clearValue = _clearValue;
        return *this;
      }
      RenderPassDesc2 &SetDepthAttachment(Attachment _depthAttachment)
      {
        this->depthAttachment = _depthAttachment;
        return *this;
      }
      RenderPassDesc2 &SetRenderAreaExtent(vk::Extent2D _renderAreaExtent)
      {
        this->renderAreaExtent = _renderAreaExtent;
        return *this;
      }

      RenderPassDesc2 &SetRecordFunc(std::function<void(RenderPassContext2)> _recordFunc)
      {
        this->recordFunc = _recordFunc;
        return *this;
      }
      RenderPassDesc2 &SetProfilerInfo(uint32_t taskColor, std::string taskName)
      {
        this->profilerTaskColor = taskColor;
        this->profilerTaskName = taskName;
        return *this;
      }

      std::vector<Attachment> colorAttachments;
      Attachment depthAttachment;


      vk::Extent2D renderAreaExtent = {0, 0};
      std::function<void(RenderPassContext2)> recordFunc;

      std::string profilerTaskName;
      uint32_t profilerTaskColor;
    };

    void AddRenderPass(
      std::vector<ImageViewProxyId> colorAttachmentImageProxies,
      ImageViewProxyId depthAttachmentImageProxy,
      std::vector<ImageViewProxyId> inputImageViewProxies,
      vk::Extent2D renderAreaExtent,
      vk::AttachmentLoadOp loadOp,
      std::function<void(RenderPassContext)> recordFunc)
    {
      RenderPassDesc renderPassDesc;
      for (const auto &proxy : colorAttachmentImageProxies)
      {
        RenderPassDesc::Attachment colorAttachment = { proxy, loadOp, vk::ClearColorValue(std::array<float, 4>{0.03f, 0.03f, 0.03f, 1.0f}) };
        renderPassDesc.colorAttachments.push_back(colorAttachment);
      }
      renderPassDesc.depthAttachment = { depthAttachmentImageProxy, loadOp, vk::ClearDepthStencilValue(1.0f, 0) };
      renderPassDesc.inputImageViewProxies = inputImageViewProxies;
      renderPassDesc.inoutStorageBufferProxies = {};
      renderPassDesc.renderAreaExtent = renderAreaExtent;
      renderPassDesc.recordFunc = recordFunc;

      AddPass(renderPassDesc);
    }

    void AddPass(RenderPassDesc &renderPassDesc)
    {
      Task task;
      task.type = Task::Types::RenderPass;
      task.index = renderPassDescs.size();
      AddTask(task);

      renderPassDescs.emplace_back(renderPassDesc);
    }
    
    void AddPass(RenderPassDesc2 &renderPassDesc2)
    {
      Task task;
      task.type = Task::Types::RenderPass2;
      task.index = renderPassDescs2.size();
      AddTask(task);

      renderPassDescs2.emplace_back(renderPassDesc2);
    }

    void Clear()
    {
      *this = RenderGraph(physicalDevice, logicalDevice, loader);
    }

    struct ComputePassDesc
    {
      ComputePassDesc()
      {
        profilerTaskName = "ComputePass";
        profilerTaskColor = legit::Colors::belizeHole;
      }
      ComputePassDesc &SetInputImages(std::vector<ImageViewProxyId> &&_inputImageViewProxies)
      {
        this->inputImageViewProxies = std::move(_inputImageViewProxies);
        return *this;
      }
      ComputePassDesc &SetStorageBuffers(std::vector<BufferProxyId> &&_inoutStorageBufferProxies)
      {
        this->inoutStorageBufferProxies = _inoutStorageBufferProxies;
        return *this;
      }
      ComputePassDesc &SetStorageImages(std::vector<ImageViewProxyId> &&_inoutStorageImageProxies)
      {
        this->inoutStorageImageProxies = _inoutStorageImageProxies;
        return *this;
      }
      ComputePassDesc &SetRecordFunc(std::function<void(PassContext)> _recordFunc)
      {
        this->recordFunc = _recordFunc;
        return *this;
      }
      ComputePassDesc &SetProfilerInfo(uint32_t taskColor, std::string taskName)
      {
        this->profilerTaskColor = taskColor;
        this->profilerTaskName = taskName;
        return *this;
      }
      std::vector<BufferProxyId> inoutStorageBufferProxies;
      std::vector<ImageViewProxyId> inputImageViewProxies;
      std::vector<ImageViewProxyId> inoutStorageImageProxies;

      std::function<void(PassContext)> recordFunc;

      std::string profilerTaskName;
      uint32_t profilerTaskColor;
    };

    struct ComputePassDesc2
    {
      ComputePassDesc2()
      {
        profilerTaskName = "ComputePass2";
        profilerTaskColor = glm::packUnorm4x8(glm::vec4(0.0f, 0.5f, 1.0f, 1.0f));
      }

      ComputePassDesc2 &SetRecordFunc(std::function<void(ComputePassContext2)> _recordFunc)
      {
        this->recordFunc = _recordFunc;
        return *this;
      }
      ComputePassDesc2 &SetProfilerInfo(uint32_t taskColor, std::string taskName)
      {
        this->profilerTaskColor = taskColor;
        this->profilerTaskName = taskName;
        return *this;
      }

      std::function<void(ComputePassContext2)> recordFunc;

      std::string profilerTaskName;
      uint32_t profilerTaskColor;
    };
    
    void AddPass(ComputePassDesc &computePassDesc)
    {
      Task task;
      task.type = Task::Types::ComputePass;
      task.index = computePassDescs.size();
      AddTask(task);

      computePassDescs.emplace_back(computePassDesc);
    }

    void AddPass(ComputePassDesc2 &computePassDesc2)
    {
      Task task;
      task.type = Task::Types::ComputePass2;
      task.index = computePassDescs2.size();
      AddTask(task);

      computePassDescs2.emplace_back(computePassDesc2);
    }
    
    struct TransferPassDesc
    {
      TransferPassDesc()
      {
        profilerTaskName = "TransferPass";
        profilerTaskColor = legit::Colors::silver;
      }
      TransferPassDesc& SetSrcImages(std::vector<ImageViewProxyId>&& _srcImageViewProxies)
      {
        this->srcImageViewProxies = std::move(_srcImageViewProxies);
        return *this;
      }

      TransferPassDesc& SetDstImages(std::vector<ImageViewProxyId>&& _dstImageViewProxies)
      {
        this->dstImageViewProxies = std::move(_dstImageViewProxies);
        return *this;
      }

      TransferPassDesc& SetSrcBuffers(std::vector<BufferProxyId>&& _srcBufferProxies)
      {
        this->srcBufferProxies = std::move(_srcBufferProxies);
        return *this;
      }

      TransferPassDesc& SetDstBuffers(std::vector<BufferProxyId>&& _dstBufferProxies)
      {
        this->dstBufferProxies = std::move(_dstBufferProxies);
        return *this;
      }

      TransferPassDesc& SetRecordFunc(std::function<void(PassContext)> _recordFunc)
      {
        this->recordFunc = _recordFunc;
        return *this;
      }

      TransferPassDesc& SetProfilerInfo(uint32_t taskColor, std::string taskName)
      {
        this->profilerTaskColor = taskColor;
        this->profilerTaskName = taskName;
        return *this;
      }

      std::vector<BufferProxyId> srcBufferProxies;
      std::vector<ImageViewProxyId> srcImageViewProxies;

      std::vector<BufferProxyId> dstBufferProxies;
      std::vector<ImageViewProxyId> dstImageViewProxies;

      std::function<void(PassContext)> recordFunc;

      std::string profilerTaskName;
      uint32_t profilerTaskColor;
    };

    void AddPass(TransferPassDesc& transferPassDesc)
    {
      Task task;
      task.type = Task::Types::TransferPass;
      task.index = transferPassDescs.size();
      AddTask(task);

      transferPassDescs.emplace_back(transferPassDesc);
    }
    void AddComputePass(
      std::vector<BufferProxyId> inoutBufferProxies,
      std::vector<ImageViewProxyId> inputImageViewProxies,
      std::function<void(PassContext)> recordFunc)
    {
      ComputePassDesc computePassDesc;
      computePassDesc.inoutStorageBufferProxies = inoutBufferProxies;
      computePassDesc.inputImageViewProxies = inputImageViewProxies;
      computePassDesc.recordFunc = recordFunc;

      AddPass(computePassDesc);
    }


    struct ImagePresentPassDesc
    {
      ImagePresentPassDesc &SetImage(ImageViewProxyId _presentImageViewProxyId)
      {
        this->presentImageViewProxyId = _presentImageViewProxyId;
        return *this;
      }
      ImageViewProxyId presentImageViewProxyId;
    };
    void AddPass(ImagePresentPassDesc &&imagePresentDesc)
    {
      Task task;
      task.type = Task::Types::ImagePresent;
      task.index = imagePresentDescs.size();
      AddTask(task);

      imagePresentDescs.push_back(imagePresentDesc);
    }
    void AddImagePresent(ImageViewProxyId presentImageViewProxyId)
    {
      ImagePresentPassDesc imagePresentDesc;
      imagePresentDesc.presentImageViewProxyId = presentImageViewProxyId;

      AddPass(std::move(imagePresentDesc));
    }

    struct FrameSyncBeginPassDesc
    {
    };
    struct FrameSyncEndPassDesc
    {
    };

    void AddPass(FrameSyncBeginPassDesc &&frameSyncBeginDesc)
    {
      Task task;
      task.type = Task::Types::FrameSyncBegin;
      task.index = frameSyncBeginDescs.size();
      AddTask(task);

      frameSyncBeginDescs.push_back(frameSyncBeginDesc);
    }

    void AddPass(FrameSyncEndPassDesc&& frameSyncEndDesc)
    {
      Task task;
      task.type = Task::Types::FrameSyncEnd;
      task.index = frameSyncEndDescs.size();
      AddTask(task);

      frameSyncEndDescs.push_back(frameSyncEndDesc);
    }
    
    static void SubmitBarriers(vk::CommandBuffer commandBuffer, Span<StateTracker::ImageBarrier> imageBarriers, Span<StateTracker::BufferBarrier> bufferBarriers)
    {
      vk::PipelineStageFlags srcStage = {};
      vk::PipelineStageFlags dstStage = {};

      std::vector<vk::ImageMemoryBarrier> vkImageMemoryBarriers;
      for(auto imageBarrier : imageBarriers)
      {
        srcStage |= imageBarrier.srcStage;
        dstStage |= imageBarrier.dstStage;
        vkImageMemoryBarriers.push_back(imageBarrier.imageMemoryBarrier);
      }

      std::vector<vk::BufferMemoryBarrier> vkBufferMemoryBarriers;
      for(auto bufferBarrier : bufferBarriers)
      {
        srcStage |= bufferBarrier.srcStage;
        dstStage |= bufferBarrier.dstStage;
        vkBufferMemoryBarriers.push_back(bufferBarrier.bufferMemoryBarrier);
      }
      
      if (imageBarriers.size() > 0 || bufferBarriers.size() > 0)
        commandBuffer.pipelineBarrier(srcStage, dstStage, vk::DependencyFlags(), {}, vkBufferMemoryBarriers, vkImageMemoryBarriers);
    }

    void Execute(vk::Device logicalDevice, vk::CommandPool transientCommandPool, legit::DescriptorSetCache *descriptorSetCache, legit::ShaderMemoryPool *memoryPool, vk::CommandBuffer commandBuffer, legit::CpuProfiler *cpuProfiler, legit::GpuProfiler *gpuProfiler)
    {
      ResolveImages();
      ResolveImageViews();
      ResolveBuffers();

      StateTracker stateTracker;
      
      for (size_t taskIndex = 0; taskIndex < tasks.size(); taskIndex++)
      {
        auto &task = tasks[taskIndex];
        switch (task.type)
        {
          case Task::Types::RenderPass:
          {
            auto &renderPassDesc = renderPassDescs[task.index];
            auto profilerTask = CreateProfilerTask(renderPassDesc);
            auto gpuTask = gpuProfiler->StartScopedTask(profilerTask.name, profilerTask.color, vk::PipelineStageFlagBits::eBottomOfPipe);
            auto cpuTask = cpuProfiler->StartScopedTask(profilerTask.name, profilerTask.color);

            RenderPassContext passContext;
            passContext.resolvedImageViews.resize(imageViewProxies.GetSize(), nullptr);
            passContext.resolvedBuffers.resize(bufferProxies.GetSize(), nullptr);

            for (auto &inputImageViewProxy : renderPassDesc.inputImageViewProxies)
            {
              passContext.resolvedImageViews[inputImageViewProxy.asInt] = GetResolvedImageView(taskIndex, inputImageViewProxy);
            }

            for (auto &inoutStorageImageProxy : renderPassDesc.inoutStorageImageProxies)
            {
              passContext.resolvedImageViews[inoutStorageImageProxy.asInt] = GetResolvedImageView(taskIndex, inoutStorageImageProxy);
            }

            for (auto &inoutBufferProxy : renderPassDesc.inoutStorageBufferProxies)
            {
              passContext.resolvedBuffers[inoutBufferProxy.asInt] = GetResolvedBuffer(taskIndex, inoutBufferProxy);
            }

            for (auto& vertexBufferProxy : renderPassDesc.vertexBufferProxies)
            {
              passContext.resolvedBuffers[vertexBufferProxy.asInt] = GetResolvedBuffer(taskIndex, vertexBufferProxy);
            }

            std::vector<StateTracker::ImageBarrier> imageBarriers;
            for (auto inputImageViewProxy : renderPassDesc.inputImageViewProxies)
            {
              auto imageView = GetResolvedImageView(taskIndex, inputImageViewProxy);
              AppendVectors(imageBarriers, stateTracker.TransitionImageAndCreateBarriers(imageView, ImageUsageTypes::GraphicsShaderRead));
            }

            for (auto &inoutStorageImageProxy : renderPassDesc.inoutStorageImageProxies)
            {
              auto imageView = GetResolvedImageView(taskIndex, inoutStorageImageProxy);
              AppendVectors(imageBarriers, stateTracker.TransitionImageAndCreateBarriers(imageView, ImageUsageTypes::GraphicsShaderReadWrite));
            }

            for (auto colorAttachment : renderPassDesc.colorAttachments)
            {
              auto imageView = GetResolvedImageView(taskIndex, colorAttachment.imageViewProxyId);
              AppendVectors(imageBarriers, stateTracker.TransitionImageAndCreateBarriers(imageView, ImageUsageTypes::ColorAttachment));
            }

            if(!(renderPassDesc.depthAttachment.imageViewProxyId == ImageViewProxyId()))
            {
              auto imageView = GetResolvedImageView(taskIndex, renderPassDesc.depthAttachment.imageViewProxyId);
              AppendVectors(imageBarriers, stateTracker.TransitionImageAndCreateBarriers(imageView, ImageUsageTypes::DepthAttachment));
            }

            std::vector<StateTracker::BufferBarrier> bufferBarriers;
            for (auto vertexBufferProxy : renderPassDesc.vertexBufferProxies)
            {
              auto storageBuffer = GetResolvedBuffer(taskIndex, vertexBufferProxy);
              AppendVectors(bufferBarriers, stateTracker.TransitionBufferAndCreateBarriers(storageBuffer, BufferUsageTypes::VertexBuffer));
            }

            for (auto inoutBufferProxy : renderPassDesc.inoutStorageBufferProxies)
            {
              auto storageBuffer = GetResolvedBuffer(taskIndex, inoutBufferProxy);
              AppendVectors(bufferBarriers, stateTracker.TransitionBufferAndCreateBarriers(storageBuffer, BufferUsageTypes::GraphicsShaderReadWrite));
            }

            SubmitBarriers(commandBuffer, imageBarriers, bufferBarriers);
            
            std::vector<FramebufferCache::Attachment> colorAttachments;
            FramebufferCache::Attachment depthAttachment;

            legit::RenderPassCache::RenderPassKey renderPassKey;

            for (auto &attachment : renderPassDesc.colorAttachments)
            {
              auto imageView = GetResolvedImageView(taskIndex, attachment.imageViewProxyId);

              renderPassKey.colorAttachmentDescs.push_back({imageView->GetImageData()->GetFormat(), attachment.loadOp, attachment.clearValue });
              colorAttachments.push_back({ imageView, attachment.clearValue });
            }
            bool depthPresent = !(renderPassDesc.depthAttachment.imageViewProxyId == ImageViewProxyId());
            if (depthPresent)
            {
              auto imageView = GetResolvedImageView(taskIndex, renderPassDesc.depthAttachment.imageViewProxyId);

              renderPassKey.depthAttachmentDesc = { imageView->GetImageData()->GetFormat(), renderPassDesc.depthAttachment.loadOp, renderPassDesc.depthAttachment.clearValue };
              depthAttachment = { imageView, renderPassDesc.depthAttachment.clearValue };
            }
            else
            {
              renderPassKey.depthAttachmentDesc.format = vk::Format::eUndefined;
            }

            auto renderPass = renderPassCache.GetRenderPass(renderPassKey);
            passContext.renderPass = renderPass;

            framebufferCache.BeginPass(commandBuffer, colorAttachments, depthPresent ? (&depthAttachment) : nullptr, renderPass, renderPassDesc.renderAreaExtent);
            passContext.commandBuffer = commandBuffer;
            renderPassDesc.recordFunc(passContext);
            framebufferCache.EndPass(commandBuffer);
          }break;
          case Task::Types::RenderPass2:
          {
            auto &renderPassDesc2 = renderPassDescs2[task.index];
            auto profilerTask = CreateProfilerTask(renderPassDesc2);
            auto gpuTask = gpuProfiler->StartScopedTask(profilerTask.name, profilerTask.color, vk::PipelineStageFlagBits::eBottomOfPipe);
            auto cpuTask = cpuProfiler->StartScopedTask(profilerTask.name, profilerTask.color);
            
            auto commandBufferAllocateInfo = vk::CommandBufferAllocateInfo()
              .setCommandPool(transientCommandPool)
              .setLevel(vk::CommandBufferLevel::eSecondary)
              .setCommandBufferCount(1u);
            auto transientCommandBuffer = logicalDevice.allocateCommandBuffers(commandBufferAllocateInfo)[0];

            std::vector<StateTracker::ImageBarrier> imageBarriers;
            std::vector<StateTracker::BufferBarrier> bufferBarriers;

            RenderPassContext2 passContext([&](const PassContext2::DescriptorSetBindings &bindings)
            {
              std::vector<legit::DescriptorSetLayoutKey::UniformBufferId> uniformBufferIds;
              uniformBufferIds.resize(bindings.shaderDataSetInfo->GetUniformBuffersCount());
              bindings.shaderDataSetInfo->GetUniformBufferIds(uniformBufferIds.data());
              assert(uniformBufferIds.size() == bindings.uniformBindings.size());
              
              auto uniforms = memoryPool->BeginSet(bindings.shaderDataSetInfo);
              size_t bufIndex = 0;
              for(auto uniformBinding : bindings.uniformBindings)
              {
                auto uniformBufferInfo = bindings.shaderDataSetInfo->GetUniformBufferInfo(uniformBufferIds[bufIndex]);
                assert(uniformBinding.size == uniformBufferInfo.size);
                void *uniformData = memoryPool->GetUniformBufferData(uniformBinding.name, uniformBinding.size);
                memcpy(uniformData, uniformBinding.data, uniformBinding.size);
                bufIndex++;
              }
              memoryPool->EndSet();

              auto descriptoSetBindings = legit::DescriptorSetBindings()
                .SetUniformBufferBindings(uniforms.uniformBufferBindings)
                .SetImageSamplerBindings(bindings.imageSamplerBindings)
                .SetTextureBindings(bindings.textureBindings)
                .SetSamplerBindings(bindings.samplerBindings)
                .SetStorageImageBindings(bindings.storageImageBindings)
                .SetStorageBufferBindings(bindings.storageBufferBindings)
                .SetAccelerationStructureBindings(bindings.accelerationStructureBindings);

              for (auto binding : bindings.imageSamplerBindings)
              {
                AppendVectors(imageBarriers, stateTracker.TransitionImageAndCreateBarriers(binding.imageView, ImageUsageTypes::GraphicsShaderRead));
              }

              for (auto binding : bindings.textureBindings)
              {
                AppendVectors(imageBarriers, stateTracker.TransitionImageAndCreateBarriers(binding.imageView, ImageUsageTypes::GraphicsShaderRead));
              }

              for (auto &binding : bindings.storageImageBindings)
              {
                AppendVectors(imageBarriers, stateTracker.TransitionImageAndCreateBarriers(binding.imageView, ImageUsageTypes::GraphicsShaderReadWrite));
              }

              for (auto storageBuffer : bindings.storageBufferBindings)
              {
                for(auto desc : storageBuffer.descriptors)
                {
                  AppendVectors(bufferBarriers, stateTracker.TransitionBufferAndCreateBarriers(desc.buffer, BufferUsageTypes::GraphicsShaderReadWrite));
                }
              }

              auto descriptorSet = descriptorSetCache->GetDescriptorSet(*bindings.shaderDataSetInfo, descriptoSetBindings);
              std::vector<uint32_t> dynamicOffsets;
              if(bindings.uniformBindings.size() > 0)
              {
                dynamicOffsets.push_back(uniforms.dynamicOffset);
              }
              transientCommandBuffer.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics,
                bindings.pipelineLayout,
                bindings.setIndex,
                { descriptorSet },
                dynamicOffsets);
            },
            [&](legit::Buffer *indirectBuf)
            {
              AppendVectors(bufferBarriers, stateTracker.TransitionBufferAndCreateBarriers(indirectBuf, BufferUsageTypes::DrawIndirect));
              transientCommandBuffer.drawIndirect(indirectBuf->GetHandle(), 0, 1, sizeof(uint32_t) * 4);
            });
            // for (auto storageBuffer : bindings.vertexBuffers)
            // {
            //   AppendVectors(bufferBarriers, stateTracker.TransitionBufferAndCreateBarriers(storageBuffer, BufferUsageTypes::VertexBuffer));
            // }

            for (auto colorAttachment : renderPassDesc2.colorAttachments)
            {
              auto imageView = colorAttachment.imageView;
              AppendVectors(imageBarriers, stateTracker.TransitionImageAndCreateBarriers(imageView, ImageUsageTypes::ColorAttachment));
            }

            if(auto imageView = renderPassDesc2.depthAttachment.imageView)
            {
              AppendVectors(imageBarriers, stateTracker.TransitionImageAndCreateBarriers(imageView, ImageUsageTypes::DepthAttachment));
            }

            std::vector<FramebufferCache::Attachment> colorAttachments;
            FramebufferCache::Attachment depthAttachment;

            legit::RenderPassCache::RenderPassKey renderPassKey;

            for (auto &attachment : renderPassDesc2.colorAttachments)
            {
              renderPassKey.colorAttachmentDescs.push_back({attachment.imageView->GetImageData()->GetFormat(), attachment.loadOp, attachment.clearValue });
              colorAttachments.push_back({ attachment.imageView, attachment.clearValue });
            }
            if (auto imageView = renderPassDesc2.depthAttachment.imageView)
            {
              renderPassKey.depthAttachmentDesc = { imageView->GetImageData()->GetFormat(), renderPassDesc2.depthAttachment.loadOp, renderPassDesc2.depthAttachment.clearValue };
              depthAttachment = { imageView, renderPassDesc2.depthAttachment.clearValue };
            }
            else
            {
              renderPassKey.depthAttachmentDesc.format = vk::Format::eUndefined;
            }

            auto renderPass = renderPassCache.GetRenderPass(renderPassKey);
            passContext.renderPass = renderPass;
            passContext.commandBuffer = transientCommandBuffer;

            auto renderAreaExtent = renderPassDesc2.renderAreaExtent;
            if(renderAreaExtent.width == 0 && renderAreaExtent.height == 0)
            {
              if(colorAttachments.size() > 0)
              {
                auto mipSize = colorAttachments[0].imageView->GetBaseSize();
                renderAreaExtent = vk::Extent2D(mipSize.x, mipSize.y);
              }else
              {
                assert(depthAttachment.imageView);
                auto mipSize = depthAttachment.imageView->GetBaseSize();
                renderAreaExtent = vk::Extent2D(mipSize.x, mipSize.y);                
              }
            }
            
            auto passInfo = framebufferCache.GetPassInfo(colorAttachments, renderPassDesc2.depthAttachment.imageView ? (&depthAttachment) : nullptr, renderPass, renderAreaExtent);
            auto inheritanceInfo = vk::CommandBufferInheritanceInfo()
              .setRenderPass(renderPass->GetHandle())
              .setFramebuffer(passInfo.framebuffer->GetHandle());
            auto oneTimeBeginInfo = vk::CommandBufferBeginInfo()
              .setFlags(vk::CommandBufferUsageFlagBits::eRenderPassContinue)
              .setPInheritanceInfo(&inheritanceInfo);      
            passContext.commandBuffer.begin(oneTimeBeginInfo);
            {
              passContext.commandBuffer.setViewport(0, { passInfo.viewport });
              passContext.commandBuffer.setScissor(0, { passInfo.scissorRect });                
              renderPassDesc2.recordFunc(passContext);
            }
            passContext.commandBuffer.end();            
            
            SubmitBarriers(commandBuffer, imageBarriers, bufferBarriers);

            framebufferCache.BeginPass(commandBuffer, colorAttachments, renderPassDesc2.depthAttachment.imageView ? (&depthAttachment) : nullptr, renderPass, renderAreaExtent, vk::SubpassContents::eSecondaryCommandBuffers);
            {
              commandBuffer.executeCommands({passContext.commandBuffer});
            }
            framebufferCache.EndPass(commandBuffer);
          }break;
          case Task::Types::ComputePass:
          {
            auto &computePassDesc = computePassDescs[task.index];
            auto profilerTask = CreateProfilerTask(computePassDesc);
            auto gpuTask = gpuProfiler->StartScopedTask(profilerTask.name, profilerTask.color, vk::PipelineStageFlagBits::eBottomOfPipe);
            auto cpuTask = cpuProfiler->StartScopedTask(profilerTask.name, profilerTask.color);

            PassContext passContext;
            passContext.resolvedImageViews.resize(imageViewProxies.GetSize(), nullptr);
            passContext.resolvedBuffers.resize(bufferProxies.GetSize(), nullptr);

            for (auto &inputImageViewProxy : computePassDesc.inputImageViewProxies)
            {
              passContext.resolvedImageViews[inputImageViewProxy.asInt] = GetResolvedImageView(taskIndex, inputImageViewProxy);
            }

            for (auto &inoutBufferProxy : computePassDesc.inoutStorageBufferProxies)
            {
              passContext.resolvedBuffers[inoutBufferProxy.asInt] = GetResolvedBuffer(taskIndex, inoutBufferProxy);
            }

            for (auto &inoutStorageImageProxy : computePassDesc.inoutStorageImageProxies)
            {
              passContext.resolvedImageViews[inoutStorageImageProxy.asInt] = GetResolvedImageView(taskIndex, inoutStorageImageProxy);
            }

            std::vector<StateTracker::ImageBarrier> imageBarriers;
            for (auto inputImageViewProxy : computePassDesc.inputImageViewProxies)
            {
              auto imageView = GetResolvedImageView(taskIndex, inputImageViewProxy);
              AppendVectors(imageBarriers, stateTracker.TransitionImageAndCreateBarriers(imageView, ImageUsageTypes::ComputeShaderRead));
            }

            for (auto &inoutStorageImageProxy : computePassDesc.inoutStorageImageProxies)
            {
              auto imageView = GetResolvedImageView(taskIndex, inoutStorageImageProxy);
              AppendVectors(imageBarriers, stateTracker.TransitionImageAndCreateBarriers(imageView, ImageUsageTypes::ComputeShaderReadWrite));
            }

            std::vector<StateTracker::BufferBarrier> bufferBarriers;
            for (auto inoutBufferProxy : computePassDesc.inoutStorageBufferProxies)
            {
              auto storageBuffer = GetResolvedBuffer(taskIndex, inoutBufferProxy);
              AppendVectors(bufferBarriers, stateTracker.TransitionBufferAndCreateBarriers(storageBuffer, BufferUsageTypes::ComputeShaderReadWrite));
            }

            SubmitBarriers(commandBuffer, imageBarriers, bufferBarriers);

            passContext.commandBuffer = commandBuffer;
            if(computePassDesc.recordFunc)
              computePassDesc.recordFunc(passContext);
          }break;
          case Task::Types::ComputePass2:
          {
            auto &computePassDesc2 = computePassDescs2[task.index];
            auto profilerTask = CreateProfilerTask(computePassDesc2);
            auto gpuTask = gpuProfiler->StartScopedTask(profilerTask.name, profilerTask.color, vk::PipelineStageFlagBits::eBottomOfPipe);
            auto cpuTask = cpuProfiler->StartScopedTask(profilerTask.name, profilerTask.color);
            
            auto commandBufferAllocateInfo = vk::CommandBufferAllocateInfo()
              .setCommandPool(transientCommandPool)
              .setLevel(vk::CommandBufferLevel::eSecondary)
              .setCommandBufferCount(1u);
            auto transientCommandBuffer = logicalDevice.allocateCommandBuffers(commandBufferAllocateInfo)[0];

            std::vector<StateTracker::ImageBarrier> imageBarriers;
            std::vector<StateTracker::BufferBarrier> bufferBarriers;

            ComputePassContext2 passContext([&](const PassContext2::DescriptorSetBindings &bindings)
            {
              auto uniforms = memoryPool->BeginSet(bindings.shaderDataSetInfo);
              for(auto uniformBinding : bindings.uniformBindings)
              {
                void *uniformData = memoryPool->GetUniformBufferData(uniformBinding.name, uniformBinding.size);
                memcpy(uniformData, uniformBinding.data, uniformBinding.size);
              }
              memoryPool->EndSet();

              auto descriptoSetBindings = legit::DescriptorSetBindings()
                .SetUniformBufferBindings(uniforms.uniformBufferBindings)
                .SetImageSamplerBindings(bindings.imageSamplerBindings)
                .SetTextureBindings(bindings.textureBindings)
                .SetSamplerBindings(bindings.samplerBindings)
                .SetStorageImageBindings(bindings.storageImageBindings)
                .SetStorageBufferBindings(bindings.storageBufferBindings)
                .SetAccelerationStructureBindings(bindings.accelerationStructureBindings);


              for (auto binding : bindings.imageSamplerBindings)
              {
                AppendVectors(imageBarriers, stateTracker.TransitionImageAndCreateBarriers(binding.imageView, ImageUsageTypes::ComputeShaderRead));
              }

              for (auto binding : bindings.textureBindings)
              {
                AppendVectors(imageBarriers, stateTracker.TransitionImageAndCreateBarriers(binding.imageView, ImageUsageTypes::ComputeShaderRead));
              }

              for (auto &binding : bindings.storageImageBindings)
              {
                AppendVectors(imageBarriers, stateTracker.TransitionImageAndCreateBarriers(binding.imageView, ImageUsageTypes::ComputeShaderReadWrite));
              }

              for (auto storageBuffer : bindings.storageBufferBindings)
              {
                for(auto desc : storageBuffer.descriptors)
                {
                  AppendVectors(bufferBarriers, stateTracker.TransitionBufferAndCreateBarriers(desc.buffer, BufferUsageTypes::ComputeShaderReadWrite));
                }
              }

              auto descriptorSet = descriptorSetCache->GetDescriptorSet(*bindings.shaderDataSetInfo, descriptoSetBindings);

              std::vector<uint32_t> dynamicOffsets;
              if(bindings.uniformBindings.size() > 0)
              {
                dynamicOffsets.push_back(uniforms.dynamicOffset);
              }
              transientCommandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, bindings.pipelineLayout, bindings.setIndex, { descriptorSet }, dynamicOffsets);
            },
            [&](legit::Buffer *indirectBuf)
            {
              AppendVectors(bufferBarriers, stateTracker.TransitionBufferAndCreateBarriers(indirectBuf, BufferUsageTypes::DispatchIndirect));
              transientCommandBuffer.dispatchIndirect(indirectBuf->GetHandle(), 0);
            });

            passContext.commandBuffer = transientCommandBuffer;

            auto inheritanceInfo = vk::CommandBufferInheritanceInfo();
            auto oneTimeBeginInfo = vk::CommandBufferBeginInfo()
              .setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit)
              .setPInheritanceInfo(&inheritanceInfo);      
            passContext.commandBuffer.begin(oneTimeBeginInfo);
            {
              computePassDesc2.recordFunc(passContext);
            }
            passContext.commandBuffer.end();            
            
            SubmitBarriers(commandBuffer, imageBarriers, bufferBarriers);

            commandBuffer.executeCommands({passContext.commandBuffer});
          }break;
          case Task::Types::TransferPass:
          {
            auto& transferPassDesc = transferPassDescs[task.index];
            auto profilerTask = CreateProfilerTask(transferPassDesc);
            auto gpuTask = gpuProfiler->StartScopedTask(profilerTask.name, profilerTask.color, vk::PipelineStageFlagBits::eBottomOfPipe);
            auto cpuTask = cpuProfiler->StartScopedTask(profilerTask.name, profilerTask.color);

            PassContext passContext;
            passContext.resolvedImageViews.resize(imageViewProxies.GetSize(), nullptr);
            passContext.resolvedBuffers.resize(bufferProxies.GetSize(), nullptr);

            for (auto& srcImageViewProxy : transferPassDesc.srcImageViewProxies)
            {
              passContext.resolvedImageViews[srcImageViewProxy.asInt] = GetResolvedImageView(taskIndex, srcImageViewProxy);
            }
            for (auto& dstImageViewProxy : transferPassDesc.dstImageViewProxies)
            {
              passContext.resolvedImageViews[dstImageViewProxy.asInt] = GetResolvedImageView(taskIndex, dstImageViewProxy);
            }

            for (auto& srcBufferProxy : transferPassDesc.srcBufferProxies)
            {
              passContext.resolvedBuffers[srcBufferProxy.asInt] = GetResolvedBuffer(taskIndex, srcBufferProxy);
            }

            for (auto& dstBufferProxy : transferPassDesc.dstBufferProxies)
            {
              passContext.resolvedBuffers[dstBufferProxy.asInt] = GetResolvedBuffer(taskIndex, dstBufferProxy);
            }

            std::vector<StateTracker::ImageBarrier> imageBarriers;
            for (auto srcImageViewProxy : transferPassDesc.srcImageViewProxies)
            {
              auto imageView = GetResolvedImageView(taskIndex, srcImageViewProxy);
              AppendVectors(imageBarriers, stateTracker.TransitionImageAndCreateBarriers(imageView, ImageUsageTypes::TransferSrc));
            }

            for (auto dstImageViewProxy : transferPassDesc.dstImageViewProxies)
            {
              auto imageView = GetResolvedImageView(taskIndex, dstImageViewProxy);
              AppendVectors(imageBarriers, stateTracker.TransitionImageAndCreateBarriers(imageView, ImageUsageTypes::TransferDst));
            }

            std::vector<StateTracker::BufferBarrier> bufferBarriers;
            for (auto srcBufferProxy : transferPassDesc.srcBufferProxies)
            {
              auto storageBuffer = GetResolvedBuffer(taskIndex, srcBufferProxy);
              AppendVectors(bufferBarriers, stateTracker.TransitionBufferAndCreateBarriers(storageBuffer, BufferUsageTypes::TransferSrc));
            }

            for (auto dstBufferProxy : transferPassDesc.dstBufferProxies)
            {
              auto storageBuffer = GetResolvedBuffer(taskIndex, dstBufferProxy);
              AppendVectors(bufferBarriers, stateTracker.TransitionBufferAndCreateBarriers(storageBuffer, BufferUsageTypes::TransferDst));
            }

            SubmitBarriers(commandBuffer, imageBarriers, bufferBarriers);

            passContext.commandBuffer = commandBuffer;
            if (transferPassDesc.recordFunc)
              transferPassDesc.recordFunc(passContext);
          }break;
          case Task::Types::ImagePresent:
          {
            auto imagePesentDesc = imagePresentDescs[task.index];
            auto profilerTask = CreateProfilerTask(imagePesentDesc);
            auto gpuTask = gpuProfiler->StartScopedTask(profilerTask.name, profilerTask.color, vk::PipelineStageFlagBits::eBottomOfPipe);
            auto cpuTask = cpuProfiler->StartScopedTask(profilerTask.name, profilerTask.color);

            std::vector<StateTracker::ImageBarrier> imageBarriers;
            {
              auto imageView = GetResolvedImageView(taskIndex, imagePesentDesc.presentImageViewProxyId);
              AppendVectors(imageBarriers, stateTracker.TransitionImageAndCreateBarriers(imageView, ImageUsageTypes::Present));
            }

            std::vector<StateTracker::BufferBarrier> bufferBarriers;
            SubmitBarriers(commandBuffer, imageBarriers, bufferBarriers);
          }break;
          case Task::Types::FrameSyncBegin:
          {
            auto frameSyncDesc = frameSyncBeginDescs[task.index];
            auto profilerTask = CreateProfilerTask(frameSyncDesc);
            auto gpuTask = gpuProfiler->StartScopedTask(profilerTask.name, profilerTask.color, vk::PipelineStageFlagBits::eBottomOfPipe);
            auto cpuTask = cpuProfiler->StartScopedTask(profilerTask.name, profilerTask.color);

            std::vector<vk::ImageMemoryBarrier> imageBarriers;
            vk::PipelineStageFlags srcStage = vk::PipelineStageFlagBits::eBottomOfPipe;
            vk::PipelineStageFlags dstStage = vk::PipelineStageFlagBits::eTopOfPipe;

            auto memoryBarrier = vk::MemoryBarrier();
            commandBuffer.pipelineBarrier(srcStage, dstStage, vk::DependencyFlags(), { memoryBarrier }, {}, {});
          }break;
          case Task::Types::FrameSyncEnd:
          {
            auto frameSyncDesc = frameSyncEndDescs[task.index];
            auto profilerTask = CreateProfilerTask(frameSyncDesc);
            auto gpuTask = gpuProfiler->StartScopedTask(profilerTask.name, profilerTask.color, vk::PipelineStageFlagBits::eBottomOfPipe);
            auto cpuTask = cpuProfiler->StartScopedTask(profilerTask.name, profilerTask.color);

            std::vector<StateTracker::ImageBarrier> imageBarriers;

            // Not transitioning any images into Undefined at the end of the frame if they're transitioned from Undefined next frame, because:
            
            // VUID-VkImageMemoryBarrier-oldLayout-01197
            // If srcQueueFamilyIndex and dstQueueFamilyIndex define a queue family ownership transfer or oldLayout and newLayout define an image layout transition,
            // oldLayout must be VK_IMAGE_LAYOUT_UNDEFINED or the current layout of the image subresources affected by the barrier

            for (auto imageViewProxy : imageViewProxies)
            {
              if(imageViewProxy.externalView != nullptr && imageViewProxy.externalUsageType != legit::ImageUsageTypes::Unknown && imageViewProxy.externalUsageType != legit::ImageUsageTypes::None)
              {
                AppendVectors(imageBarriers, stateTracker.TransitionImageAndCreateBarriers(imageViewProxy.externalView, imageViewProxy.externalUsageType));
              }
            }
            
            for(auto &imageBarrier : imageBarriers)
            {
              imageBarrier.srcStage |= vk::PipelineStageFlagBits::eBottomOfPipe;
              imageBarrier.dstStage |= vk::PipelineStageFlagBits::eTopOfPipe;
            }
            std::vector<StateTracker::BufferBarrier> bufferBarriers;
            SubmitBarriers(commandBuffer, imageBarriers, bufferBarriers);
          }break;
        }
      }

      renderPassDescs.clear();
      transferPassDescs.clear();
      imagePresentDescs.clear();
      frameSyncBeginDescs.clear();
      frameSyncEndDescs.clear();
      tasks.clear();
    }
    
  private:

    bool ImageViewContainsSubresource(legit::ImageView *imageView, legit::ImageData *imageData, uint32_t mipLevel, uint32_t arrayLayer)
    {
      return (
        imageView->GetImageData() == imageData &&
        arrayLayer >= imageView->GetBaseArrayLayer() && arrayLayer < imageView->GetBaseArrayLayer() + imageView->GetArrayLayersCount() &&
        mipLevel >= imageView->GetBaseMipLevel() && mipLevel < imageView->GetBaseMipLevel() + imageView->GetMipLevelsCount());
    }

    ImageUsageTypes GetTaskImageSubresourceUsageType(size_t taskIndex, legit::ImageData *imageData, uint32_t mipLevel, uint32_t arrayLayer)
    {
      Task &task = tasks[taskIndex];
      switch (task.type)
      {
        case Task::Types::RenderPass:
        {
          auto &renderPassDesc = renderPassDescs[task.index];
          for (auto colorAttachment : renderPassDesc.colorAttachments)
          {
            auto attachmentImageView = GetResolvedImageView(taskIndex, colorAttachment.imageViewProxyId);
            if(ImageViewContainsSubresource(attachmentImageView, imageData, mipLevel, arrayLayer))
              return ImageUsageTypes::ColorAttachment;
          }
          if (!(renderPassDesc.depthAttachment.imageViewProxyId == ImageViewProxyId()))
          {
            auto attachmentImageView = GetResolvedImageView(taskIndex, renderPassDesc.depthAttachment.imageViewProxyId);
            if (ImageViewContainsSubresource(attachmentImageView, imageData, mipLevel, arrayLayer))
              return ImageUsageTypes::DepthAttachment;
          }
          for (auto imageViewProxy : renderPassDesc.inputImageViewProxies)
          {
            if(ImageViewContainsSubresource(GetResolvedImageView(taskIndex, imageViewProxy), imageData, mipLevel, arrayLayer))
              return ImageUsageTypes::GraphicsShaderRead;
          }
          for (auto imageViewProxy : renderPassDesc.inoutStorageImageProxies)
          {
            if (ImageViewContainsSubresource(GetResolvedImageView(taskIndex, imageViewProxy), imageData, mipLevel, arrayLayer))
              return ImageUsageTypes::GraphicsShaderReadWrite;
          }
        }break;
        case Task::Types::ComputePass:
        {
          auto &computePassDesc = computePassDescs[task.index];
          for (auto imageViewProxy : computePassDesc.inputImageViewProxies)
          {
            if (ImageViewContainsSubresource(GetResolvedImageView(taskIndex, imageViewProxy), imageData, mipLevel, arrayLayer))
              return ImageUsageTypes::ComputeShaderRead;
          }
          for (auto imageViewProxy : computePassDesc.inoutStorageImageProxies)
          {
            if (ImageViewContainsSubresource(GetResolvedImageView(taskIndex, imageViewProxy), imageData, mipLevel, arrayLayer))
              return ImageUsageTypes::ComputeShaderReadWrite;
          }
        }break;
        case Task::Types::TransferPass:
        {
          auto &transferPassDesc = transferPassDescs[task.index];
          for (auto srcImageViewProxy : transferPassDesc.srcImageViewProxies)
          {
            if (ImageViewContainsSubresource(GetResolvedImageView(taskIndex, srcImageViewProxy), imageData, mipLevel, arrayLayer))
              return ImageUsageTypes::TransferSrc;
          }
          for (auto dstImageViewProxy : transferPassDesc.dstImageViewProxies)
          {
            if (ImageViewContainsSubresource(GetResolvedImageView(taskIndex, dstImageViewProxy), imageData, mipLevel, arrayLayer))
              return ImageUsageTypes::TransferDst;
          }
        }break;
        case Task::Types::ImagePresent:
        {
          auto &imagePresentDesc = imagePresentDescs[task.index];
          if (ImageViewContainsSubresource(GetResolvedImageView(taskIndex, imagePresentDesc.presentImageViewProxyId), imageData, mipLevel, arrayLayer))
            return ImageUsageTypes::Present;
        }break;
        default:{}
      }
      return ImageUsageTypes::None;
    }

    
    BufferUsageTypes GetTaskBufferUsageType(size_t taskIndex, legit::Buffer *buffer)
    {
      Task &task = tasks[taskIndex];
      switch (task.type)
      {
        case Task::Types::RenderPass:
        {
          auto &renderPassDesc = renderPassDescs[task.index];
          for (auto storageBufferProxy : renderPassDesc.inoutStorageBufferProxies)
          {
            auto storageBuffer = GetResolvedBuffer(taskIndex, storageBufferProxy);
            if (buffer->GetHandle() == storageBuffer->GetHandle())
              return BufferUsageTypes::GraphicsShaderReadWrite;
          }
          for (auto vertexBufferProxy : renderPassDesc.vertexBufferProxies)
          {
            auto vertexBuffer = GetResolvedBuffer(taskIndex, vertexBufferProxy);
            if (buffer->GetHandle() == vertexBuffer->GetHandle())
              return BufferUsageTypes::VertexBuffer;
          }
        }break;
        case Task::Types::ComputePass:
        {
          auto &computePassDesc = computePassDescs[task.index];
          for (auto storageBufferProxy : computePassDesc.inoutStorageBufferProxies)
          {
            auto storageBuffer = GetResolvedBuffer(taskIndex, storageBufferProxy);
            if (buffer->GetHandle() == storageBuffer->GetHandle())
              return BufferUsageTypes::ComputeShaderReadWrite;
          }
        }break;

        case Task::Types::TransferPass:
        {
          auto& transferPassDesc = transferPassDescs[task.index];
          for (auto srcBufferProxy : transferPassDesc.srcBufferProxies)
          {
            auto srcBuffer = GetResolvedBuffer(taskIndex, srcBufferProxy);
            if (buffer->GetHandle() == srcBuffer->GetHandle())
              return BufferUsageTypes::TransferSrc;
          }
          for (auto dstBufferProxy : transferPassDesc.dstBufferProxies)
          {
            auto dstBuffer = GetResolvedBuffer(taskIndex, dstBufferProxy);
            if (buffer->GetHandle() == dstBuffer->GetHandle())
              return BufferUsageTypes::TransferSrc;
          }
        }break;
        default: {}
      }
      return BufferUsageTypes::None;
    }

    ImageUsageTypes GetLastImageSubresourceUsageType(size_t taskIndex, legit::ImageData *imageData, uint32_t mipLevel, uint32_t arrayLayer)
    {
      for (size_t taskOffset = 0; taskOffset < taskIndex; taskOffset++)
      {
        size_t prevTaskIndex = taskIndex - taskOffset - 1;
        auto usageType = GetTaskImageSubresourceUsageType(prevTaskIndex, imageData, mipLevel, arrayLayer);
        if (usageType != ImageUsageTypes::None)
          return usageType;
      }

      for (auto &imageViewProxy : imageViewProxies)
      {
        if (imageViewProxy.type == ImageViewProxy::Types::External && imageViewProxy.externalView->GetImageData() == imageData)
        {
          return imageViewProxy.externalUsageType;
        }
      }
      return ImageUsageTypes::None;
    }

    /*ImageUsageTypes GetNextImageSubresourceUsageType(size_t taskIndex, legit::ImageData *imageData, uint32_t mipLevel, uint32_t arrayLayer)
    {
      for (size_t nextTaskIndex = taskIndex + 1; nextTaskIndex < tasks.size(); nextTaskIndex++)
      {
        auto usageType = GetTaskImageSubresourceUsageType(nextTaskIndex, imageData, mipLevel, arrayLayer);
        if (usageType != ImageUsageTypes::Undefined)
          return usageType;
      }
      for (auto &imageViewProxy : imageViewProxies)
      {
        if (imageViewProxy.type == ImageViewProxy::Types::External && imageViewProxy.externalView->GetImageData() == imageData)
        {
          return imageViewProxy.externalUsageType;
        }
      }
      return ImageUsageTypes::Undefined;
    }*/

    BufferUsageTypes GetLastBufferUsageType(size_t taskIndex, legit::Buffer *buffer)
    {
      for (size_t taskOffset = 1; taskOffset < taskIndex; taskOffset++)
      {
        size_t prevTaskIndex = taskIndex - taskOffset;
        auto usageType = GetTaskBufferUsageType(prevTaskIndex, buffer);
        if (usageType != BufferUsageTypes::None)
          return usageType;
      }
      return BufferUsageTypes::None;
    }

    /*BufferUsageTypes GetNextBufferUsageType(size_t taskIndex, legit::Buffer *buffer)
    {
      for (size_t nextTaskIndex = taskIndex + 1; nextTaskIndex < tasks.size(); nextTaskIndex++)
      {
        auto usageType = GetTaskBufferUsageType(nextTaskIndex, buffer);
        if (usageType != BufferUsageTypes::Undefined)
          return usageType;
      }
      return BufferUsageTypes::Undefined;
    }*/

    void FlushImageTransitionBarriers(legit::ImageData *imageData, vk::ImageSubresourceRange range, ImageUsageTypes srcUsageType, ImageUsageTypes dstUsageType, vk::PipelineStageFlags &srcStage, vk::PipelineStageFlags &dstStage, std::vector<vk::ImageMemoryBarrier> &imageBarriers)
    {
      if (
        IsImageBarrierNeeded(srcUsageType, dstUsageType) &&
        //srcUsageType != ImageUsageTypes::ColorAttachment && //this is done automatically when constructing render pass
        //srcUsageType != ImageUsageTypes::DepthAttachment &&
        range.layerCount > 0 &&
        range.levelCount > 0) //this is done automatically when constructing render pass
      {
        auto srcImageAccessPattern = GetSrcImageAccessPattern(srcUsageType);
        auto dstImageAccessPattern = GetDstImageAccessPattern(dstUsageType);
        auto imageBarrier = vk::ImageMemoryBarrier()
          .setSrcAccessMask(srcImageAccessPattern.accessMask)
          .setOldLayout(srcImageAccessPattern.layout)
          .setDstAccessMask(dstImageAccessPattern.accessMask)
          .setNewLayout(dstImageAccessPattern.layout)
          .setSubresourceRange(range)
          .setImage(imageData->GetHandle());

        if (srcImageAccessPattern.queueFamilyType == dstImageAccessPattern.queueFamilyType)
        {
          imageBarrier
            .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
            .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
        }
        else
        {
          imageBarrier
            .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
            .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
          /*imageBarrier
            .setSrcQueueFamilyIndex(srcImageAccessPattern.queueFamilyIndex)
            .setDstQueueFamilyIndex(dstImageAccessPattern.queueFamilyIndex);*/
        }
        srcStage |= srcImageAccessPattern.stage;
        dstStage |= dstImageAccessPattern.stage;
        imageBarriers.push_back(imageBarrier);
      }
    }

    void AddImageTransitionBarriers(legit::ImageView *imageView, ImageUsageTypes dstUsageType, size_t dstTaskIndex, vk::PipelineStageFlags &srcStage, vk::PipelineStageFlags &dstStage, std::vector<vk::ImageMemoryBarrier> &imageBarriers)
    {
      auto range = vk::ImageSubresourceRange()
        .setAspectMask(imageView->GetImageData()->GetAspectFlags());

      for (uint32_t arrayLayer = imageView->GetBaseArrayLayer(); arrayLayer < imageView->GetBaseArrayLayer() + imageView->GetArrayLayersCount(); arrayLayer++)
      {
        range
          .setBaseArrayLayer(arrayLayer)
          .setLayerCount(1)
          .setBaseMipLevel(imageView->GetBaseMipLevel())
          .setLevelCount(0);
        ImageUsageTypes prevSubresourceUsageType = ImageUsageTypes::None;

        for (uint32_t mipLevel = imageView->GetBaseMipLevel(); mipLevel < imageView->GetBaseMipLevel() + imageView->GetMipLevelsCount(); mipLevel++)
        {
          auto lastUsageType = GetLastImageSubresourceUsageType(dstTaskIndex, imageView->GetImageData(), mipLevel, arrayLayer);
          if (prevSubresourceUsageType != lastUsageType)
          {
            FlushImageTransitionBarriers(imageView->GetImageData(), range, prevSubresourceUsageType, dstUsageType, srcStage, dstStage, imageBarriers);
            range
              .setBaseMipLevel(mipLevel)
              .setLevelCount(0);
            prevSubresourceUsageType = lastUsageType;
          }
          range.levelCount++;
        }
        FlushImageTransitionBarriers(imageView->GetImageData(), range, prevSubresourceUsageType, dstUsageType, srcStage, dstStage, imageBarriers);
      }
    }


    void FlushBufferTransitionBarriers(legit::Buffer *buffer, BufferUsageTypes srcUsageType, BufferUsageTypes dstUsageType, vk::PipelineStageFlags &srcStage, vk::PipelineStageFlags &dstStage, std::vector<vk::BufferMemoryBarrier> &bufferBarriers)
    {
      if (IsBufferBarrierNeeded(srcUsageType, dstUsageType))
      {
        auto srcBufferAccessPattern = GetSrcBufferAccessPattern(srcUsageType);
        auto dstBufferAccessPattern = GetDstBufferAccessPattern(dstUsageType);
        auto bufferBarrier = vk::BufferMemoryBarrier()
          .setSrcAccessMask(srcBufferAccessPattern.accessMask)
          .setOffset(0)
          .setSize(VK_WHOLE_SIZE)
          .setDstAccessMask(dstBufferAccessPattern.accessMask)
          .setBuffer(buffer->GetHandle());

        if (srcBufferAccessPattern.queueFamilyType == dstBufferAccessPattern.queueFamilyType)
        {
          bufferBarrier
            .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
            .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
        }
        else
        {
          bufferBarrier
            .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
            .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
          /*imageBarrier
          .setSrcQueueFamilyIndex(srcImageAccessPattern.queueFamilyIndex)
          .setDstQueueFamilyIndex(dstImageAccessPattern.queueFamilyIndex);*/
        }
        srcStage |= srcBufferAccessPattern.stage;
        dstStage |= dstBufferAccessPattern.stage;
        bufferBarriers.push_back(bufferBarrier);
      }
    }

    void AddBufferBarriers(legit::Buffer *buffer, BufferUsageTypes dstUsageType, size_t dstTaskIndex, vk::PipelineStageFlags &srcStage, vk::PipelineStageFlags &dstStage, std::vector<vk::BufferMemoryBarrier> &bufferBarriers)
    {
      auto lastUsageType = GetLastBufferUsageType(dstTaskIndex, buffer);
      FlushBufferTransitionBarriers(buffer, lastUsageType, dstUsageType, srcStage, dstStage, bufferBarriers);
    }

    struct ImageProxy
    {
      enum struct Types
      {
        Transient,
        External
      };

      ImageCache::ImageKey imageKey;
      legit::ImageData *externalImage;

      legit::ImageData *resolvedImage;

      Types type;
    };


    ImageCache imageCache;

    ImageProxyPool imageProxies;
    void ResolveImages()
    {
      imageCache.Release();

      for (auto &imageProxy : imageProxies)
      {
        switch (imageProxy.type)
        {
          case ImageProxy::Types::External:
          {
            imageProxy.resolvedImage = imageProxy.externalImage;
          }break;
          case ImageProxy::Types::Transient:
          {
            imageProxy.resolvedImage = imageCache.GetImage(imageProxy.imageKey);
          }break;
        }
      }

      imageCache.PurgeUnused();
    }
    legit::ImageData *GetResolvedImage(size_t taskIndex, ImageProxyId imageProxy)
    {
      return imageProxies.Get(imageProxy).resolvedImage;
    }

    struct ImageViewProxy
    {
      enum struct Types
      {
        Transient,
        External
      };
      bool Contains(const ImageViewProxy &other)
      {
        if (type == Types::Transient && subresourceRange.Contains(other.subresourceRange) && type == other.type && imageProxyId == other.imageProxyId)
        {
          return true;
        }

        if (type == Types::External && externalView == other.externalView)
        {
          return true;
        }
        return false;
      }
      ImageProxyId imageProxyId;
      ImageSubresourceRange subresourceRange;

      legit::ImageView *externalView;
      legit::ImageUsageTypes externalUsageType;

      legit::ImageView *resolvedImageView;
      std::string debugName;

      Types type;
    };

    ImageViewCache imageViewCache;
    ImageViewProxyPool imageViewProxies;
    void ResolveImageViews()
    {
      for (auto &imageViewProxy : imageViewProxies)
      {
        switch (imageViewProxy.type)
        {
          case ImageViewProxy::Types::External:
          {
            imageViewProxy.resolvedImageView = imageViewProxy.externalView;
          }break;
          case ImageViewProxy::Types::Transient:
          {
            ImageViewCache::ImageViewKey imageViewKey;
            imageViewKey.image = GetResolvedImage(0, imageViewProxy.imageProxyId);
            imageViewKey.subresourceRange = imageViewProxy.subresourceRange;
            imageViewKey.debugName = imageViewProxy.debugName + "[img: " + imageProxies.Get(imageViewProxy.imageProxyId).imageKey.debugName + "]";

            imageViewProxy.resolvedImageView = imageViewCache.GetImageView(imageViewKey);
          }break;
        }
      }
    }
    legit::ImageView *GetResolvedImageView(size_t taskIndex, ImageViewProxyId imageViewProxyId)
    {
      return imageViewProxies.Get(imageViewProxyId).resolvedImageView;
    }

    struct BufferProxy
    {
      enum struct Types
      {
        Transient,
        External
      };

      BufferCache::BufferKey bufferKey;
      legit::Buffer *externalBuffer;

      legit::Buffer *resolvedBuffer;

      Types type;
    };

    BufferCache bufferCache;
    BufferProxyPool bufferProxies;
    void ResolveBuffers()
    {
      bufferCache.Release();

      for (auto &bufferProxy : bufferProxies)
      {
        switch (bufferProxy.type)
        {
          case BufferProxy::Types::External:
          {
            bufferProxy.resolvedBuffer = bufferProxy.externalBuffer;
          }break;
          case BufferProxy::Types::Transient:
          {
            bufferProxy.resolvedBuffer = bufferCache.GetBuffer(bufferProxy.bufferKey);
          }break;
        }
      }
      bufferCache.PurgeUnused();
    }
    legit::Buffer *GetResolvedBuffer(size_t taskIndex, BufferProxyId bufferProxyId)
    {
      return bufferProxies.Get(bufferProxyId).resolvedBuffer;
    }



    std::vector<RenderPassDesc> renderPassDescs;
    std::vector<RenderPassDesc2> renderPassDescs2;
    std::vector<ComputePassDesc> computePassDescs;
    std::vector<ComputePassDesc2> computePassDescs2;
    std::vector<TransferPassDesc> transferPassDescs;


    std::vector<ImagePresentPassDesc> imagePresentDescs;
    std::vector<FrameSyncBeginPassDesc> frameSyncBeginDescs;
    std::vector<FrameSyncEndPassDesc> frameSyncEndDescs;

    struct Task
    {
      enum struct Types
      {
        RenderPass,
        RenderPass2,
        ComputePass,
        ComputePass2,
        TransferPass,
        ImagePresent,
        FrameSyncBegin,
        FrameSyncEnd
      };
      Types type;
      size_t index;
    };
    std::vector<Task> tasks;
    void AddTask(Task task)
    {
      tasks.push_back(task);
    }

    legit::ProfilerTask CreateProfilerTask(const RenderPassDesc &renderPassDesc)
    {
      legit::ProfilerTask task;
      task.startTime = -1.0f;
      task.endTime = -1.0f;
      task.name = renderPassDesc.profilerTaskName;
      task.color = renderPassDesc.profilerTaskColor;
      return task;
    }
    legit::ProfilerTask CreateProfilerTask(const RenderPassDesc2 &renderPassDesc2)
    {
      legit::ProfilerTask task;
      task.startTime = -1.0f;
      task.endTime = -1.0f;
      task.name = renderPassDesc2.profilerTaskName;
      task.color = renderPassDesc2.profilerTaskColor;
      return task;
    }    
    legit::ProfilerTask CreateProfilerTask(const ComputePassDesc &computePassDesc)
    {
      legit::ProfilerTask task;
      task.startTime = -1.0f;
      task.endTime = -1.0f;
      task.name = computePassDesc.profilerTaskName;
      task.color = computePassDesc.profilerTaskColor;
      return task;
    }
    legit::ProfilerTask CreateProfilerTask(const ComputePassDesc2 &computePassDesc2)
    {
      legit::ProfilerTask task;
      task.startTime = -1.0f;
      task.endTime = -1.0f;
      task.name = computePassDesc2.profilerTaskName;
      task.color = computePassDesc2.profilerTaskColor;
      return task;
    }
    legit::ProfilerTask CreateProfilerTask(const TransferPassDesc& transferPassDesc)
    {
      legit::ProfilerTask task;
      task.startTime = -1.0f;
      task.endTime = -1.0f;
      task.name = transferPassDesc.profilerTaskName;
      task.color = transferPassDesc.profilerTaskColor;
      return task;
    }
    legit::ProfilerTask CreateProfilerTask(const ImagePresentPassDesc &imagePresentPassDesc)
    {
      legit::ProfilerTask task;
      task.startTime = -1.0f;
      task.endTime = -1.0f;
      task.name = "ImagePresent";
      task.color = glm::packUnorm4x8(glm::vec4(0.0f, 1.0f, 0.5f, 1.0f));
      return task;
    }

    legit::ProfilerTask CreateProfilerTask(const FrameSyncBeginPassDesc& frameSyncBeginPassDesc)
    {
      legit::ProfilerTask task;
      task.startTime = -1.0f;
      task.endTime = -1.0f;
      task.name = "FrameSyncBegin";
      task.color = glm::packUnorm4x8(glm::vec4(0.0f, 0.5f, 1.0f, 1.0f));
      return task;
    }
    legit::ProfilerTask CreateProfilerTask(const FrameSyncEndPassDesc& frameSyncEndPassDesc)
    {
      legit::ProfilerTask task;
      task.startTime = -1.0f;
      task.endTime = -1.0f;
      task.name = "FrameSyncEnd";
      task.color = glm::packUnorm4x8(glm::vec4(0.0f, 0.5f, 1.0f, 1.0f));
      return task;
    }

    RenderPassCache renderPassCache;
    FramebufferCache framebufferCache;

    vk::Device logicalDevice;
    vk::PhysicalDevice physicalDevice;
    vk::detail::DispatchLoaderDynamic loader;
    size_t imageAllocations = 0;
  };

}