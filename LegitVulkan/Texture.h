namespace legit
{
  struct Texture
  {
    Texture(legit::Core *core, vk::ImageCreateInfo imageCreateInfo, std::string debugName) :
      imageCreateInfo(imageCreateInfo),
      debugName(debugName)
    {
      image.reset(new legit::Image(core->GetPhysicalDevice(), core->GetLogicalDevice(), imageCreateInfo, vk::MemoryPropertyFlagBits::eDeviceLocal));
      CreateImageViews(core, imageCreateInfo, debugName);
      core->SetObjectDebugName(image->GetImageData()->GetHandle(), debugName + "[image]");
    }
    Texture(legit::Core *core, vk::ImageCreateInfo imageCreateInfo, vk::CommandBuffer commandBuffer, legit::ImageUsageTypes baseUsageType, std::string debugName) :
      imageCreateInfo(imageCreateInfo),
      debugName(debugName)
    {
      image.reset(new legit::Image(core->GetPhysicalDevice(), core->GetLogicalDevice(), imageCreateInfo, commandBuffer, baseUsageType, vk::MemoryPropertyFlagBits::eDeviceLocal));
      CreateImageViews(core, imageCreateInfo, debugName);
      core->SetObjectDebugName(image->GetImageData()->GetHandle(), debugName + "[image]");
    }
    glm::uvec3 GetBaseSize() {return glm::uvec3(imageCreateInfo.extent.width, imageCreateInfo.extent.height, imageCreateInfo.extent.depth);}
    legit::Image *GetImage() {return image.get();}
    legit::ImageView *GetImageView() {return imageView.get();}
    legit::ImageView *GetMipImageView(uint32_t mipLevel){return mipImageViews[mipLevel].get();};
    size_t GetMipsCount(){return mipImageViews.size();}
  private:
    void CreateImageViews(legit::Core *core, vk::ImageCreateInfo imageCreateInfo, std::string debugName)
    {
      mipImageViews.resize(imageCreateInfo.mipLevels);
      if(imageCreateInfo.flags & vk::ImageCreateFlagBits::eCubeCompatible)
      {
        assert(imageCreateInfo.imageType == vk::ImageType::e2D);
        assert(imageCreateInfo.arrayLayers == 6);
        imageView.reset(new legit::ImageView(core->GetLogicalDevice(), image->GetImageData(), 0, imageCreateInfo.mipLevels));
        core->SetObjectDebugName(imageView->GetHandle(), debugName + "[cube view complete]");
        for(uint32_t mipLevel = 0; mipLevel < imageCreateInfo.mipLevels; mipLevel++)
        {
          mipImageViews[mipLevel].reset(new legit::ImageView(core->GetLogicalDevice(), image->GetImageData(), mipLevel, 1u));
          core->SetObjectDebugName(mipImageViews[mipLevel]->GetHandle(), debugName + "[cube view mip " + std::to_string(mipLevel) + "]");
        }
      }else
      {
        imageView.reset(new legit::ImageView(core->GetLogicalDevice(), image->GetImageData(), 0, imageCreateInfo.mipLevels, 0u, imageCreateInfo.arrayLayers));
        core->SetObjectDebugName(imageView->GetHandle(), debugName + "[image view complete]");
        for(uint32_t mipLevel = 0; mipLevel < imageCreateInfo.mipLevels; mipLevel++)
        {
          mipImageViews[mipLevel].reset(new legit::ImageView(core->GetLogicalDevice(), image->GetImageData(), mipLevel, 1u, 0u, imageCreateInfo.arrayLayers));
          core->SetObjectDebugName(mipImageViews[mipLevel]->GetHandle(), debugName + "[image view mip " + std::to_string(mipLevel) + "]");
        }
      }
    }
    std::string debugName;
    vk::ImageCreateInfo imageCreateInfo;
    std::unique_ptr<legit::Image> image;
    std::unique_ptr<legit::ImageView> imageView;
    std::vector<std::unique_ptr<legit::ImageView> > mipImageViews;
  };
}