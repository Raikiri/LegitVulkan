namespace legit
{
  
  struct StateTracker
  {
    struct ImageSubresource
    {
      const legit::ImageData *imageData;
      uint32_t mipLevel;
      uint32_t arrayLayer;
      bool operator < (const ImageSubresource &other) const
      {
        return std::tie(imageData, mipLevel, arrayLayer) < std::tie(other.imageData, other.mipLevel, other.arrayLayer);
      }
    };
    
    struct ImageBarrier
    {
      vk::ImageMemoryBarrier imageMemoryBarrier;
      vk::PipelineStageFlags srcStage;
      vk::PipelineStageFlags dstStage;
    };
    static std::optional<ImageBarrier> CreateImageBarrierIfNeeded(
      const legit::ImageData *imageData,
      vk::ImageSubresourceRange range,
      ImageUsageTypes srcUsageType,
      ImageUsageTypes dstUsageType)
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
        ImageBarrier barrier;
        barrier.srcStage = srcImageAccessPattern.stage;
        barrier.dstStage = dstImageAccessPattern.stage;
        barrier.imageMemoryBarrier = imageBarrier;
        return barrier;
      }
      return {};
    }
    
    legit::ImageUsageTypes TransitionImageSubresource(ImageSubresource imgSubresource, ImageUsageTypes newUsageType)
    {
      auto it = imgSubresourceToCurrUsage.find(imgSubresource);
      if(it != imgSubresourceToCurrUsage.end())
      {
        auto lastUsage = it->second;
        it->second = newUsageType;
        return lastUsage;
      }
      
      // VUID-VkImageMemoryBarrier-oldLayout-01197
      // If srcQueueFamilyIndex and dstQueueFamilyIndex define a queue family ownership transfer or oldLayout and newLayout define an image layout transition,
      // oldLayout must be VK_IMAGE_LAYOUT_UNDEFINED or the current layout of the image subresources affected by the barrier


      auto baseUsage = imgSubresource.imageData->GetSubresourceBaseUsageType(imgSubresource.mipLevel, imgSubresource.arrayLayer);
      imgSubresourceToCurrUsage[imgSubresource] = newUsageType;
      return baseUsage;
    }
    
    std::vector<ImageBarrier> TransitionImageAndCreateBarriers(const legit::ImageView *imageView, ImageUsageTypes dstUsageType)
    {
      auto range = vk::ImageSubresourceRange()
        .setAspectMask(imageView->GetImageData()->GetAspectFlags());
      const legit::ImageData *imageData = imageView->GetImageData();
      std::vector<ImageBarrier> barriers;
      
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
          auto lastUsageType = TransitionImageSubresource({imageView->GetImageData(), mipLevel, arrayLayer}, dstUsageType);
          if (prevSubresourceUsageType != lastUsageType)
          {
            if(auto maybeBarrier = CreateImageBarrierIfNeeded(imageView->GetImageData(), range, prevSubresourceUsageType, dstUsageType))
            {
              barriers.push_back(*maybeBarrier);
            }
            range
              .setBaseMipLevel(mipLevel)
              .setLevelCount(0);
            prevSubresourceUsageType = lastUsageType;
          }
          range.levelCount++;
        }
        if(auto maybeBarrier = CreateImageBarrierIfNeeded(imageView->GetImageData(), range, prevSubresourceUsageType, dstUsageType))
        {
          barriers.push_back(*maybeBarrier);
        }
      }
      
      return barriers;
    }
    
    struct BufferBarrier
    {
      vk::BufferMemoryBarrier bufferMemoryBarrier;
      vk::PipelineStageFlags srcStage;
      vk::PipelineStageFlags dstStage;
    };
    
    static std::optional<BufferBarrier> CreateBufferBufferBarrierIfNeeded(const legit::Buffer *buffer, BufferUsageTypes srcUsageType, BufferUsageTypes dstUsageType)
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
        BufferBarrier barrier;
        barrier.srcStage = srcBufferAccessPattern.stage;
        barrier.dstStage = dstBufferAccessPattern.stage;
        barrier.bufferMemoryBarrier = bufferBarrier;
        return barrier;
      }
      return {};
    }
    
    legit::BufferUsageTypes TransitionBuffer(const legit::Buffer *buffer, BufferUsageTypes newUsageType)
    {
      auto it = bufferToCurrUsage.find(buffer);
      if(it != bufferToCurrUsage.end())
      {
        auto lastUsage = it->second;
        it->second = newUsageType;
        return lastUsage;
      }
      auto baseUsage = BufferUsageTypes::None;
      bufferToCurrUsage[buffer] = newUsageType;
      return baseUsage;
    }

    std::vector<BufferBarrier> TransitionBufferAndCreateBarriers(const legit::Buffer *buffer, BufferUsageTypes dstUsageType)
    {
      auto lastUsageType = TransitionBuffer(buffer, dstUsageType);
      std::vector<BufferBarrier> bufferBarriers;
      if(auto maybeBarrier = CreateBufferBufferBarrierIfNeeded(buffer, lastUsageType, dstUsageType))
      {
        bufferBarriers.push_back(*maybeBarrier);
      }
      return bufferBarriers;
    }

    
    std::map<ImageSubresource, legit::ImageUsageTypes> imgSubresourceToCurrUsage;
    std::map<const legit::Buffer*, legit::BufferUsageTypes> bufferToCurrUsage;
  };
}