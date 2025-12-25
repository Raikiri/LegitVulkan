#include <map>
#include <set>
#include "spirv_cross.hpp"
#include <algorithm>

namespace legit
{
  class DescriptorSetLayoutKey;
  class Shader;
  class Sampler;
  class ImageView;
  class AccelerationStructure;
  class Buffer;

  template<typename Base>
  struct ShaderResourceId
  {
    ShaderResourceId() :
      id(size_t(-1))
    {
    }

    bool operator ==(const ShaderResourceId<Base> &other) const
    {
      return this->id == other.id;
    }
    bool IsValid()
    {
      return id != size_t(-1);
    }
  private:
    explicit ShaderResourceId(size_t _id) : id(_id) {}
    size_t id;
    friend class DescriptorSetLayoutKey;
    friend class Shader;
  };

  struct ImageSamplerBinding
  {
    ImageSamplerBinding() : imageView(nullptr), sampler(nullptr) {}
    ImageSamplerBinding(legit::ImageView *_imageView, legit::Sampler *_sampler, uint32_t _shaderBindingId) : imageView(_imageView), sampler(_sampler), shaderBindingId(_shaderBindingId)
    {
      assert(_imageView);
      assert(_sampler);
    }
    bool operator < (const ImageSamplerBinding &other) const
    {
      return std::tie(imageView, sampler, shaderBindingId) < std::tie(other.imageView, other.sampler, other.shaderBindingId);
    }
    legit::ImageView *imageView;
    legit::Sampler *sampler;
    uint32_t shaderBindingId;
  };

  struct TextureBinding
  {
    TextureBinding() : imageView(nullptr) {}
    TextureBinding(legit::ImageView *_imageView, uint32_t _shaderBindingId) : imageView(_imageView), shaderBindingId(_shaderBindingId)
    {
      assert(_imageView);
    }
    bool operator < (const ImageSamplerBinding &other) const
    {
      return std::tie(imageView, shaderBindingId) < std::tie(other.imageView, other.shaderBindingId);
    }
    legit::ImageView *imageView;
    uint32_t shaderBindingId;
  };

  struct SamplerBinding
  {
    SamplerBinding() : sampler(nullptr) {}
    SamplerBinding(legit::Sampler *_sampler, uint32_t _shaderBindingId) : sampler(_sampler), shaderBindingId(_shaderBindingId)
    {
      assert(_sampler);
    }
    bool operator < (const ImageSamplerBinding &other) const
    {
      return std::tie(sampler, shaderBindingId) < std::tie(other.sampler, other.shaderBindingId);
    }
    legit::Sampler *sampler;
    uint32_t shaderBindingId;
  };

  struct UniformBufferBinding
  {
    UniformBufferBinding() : buffer(nullptr), offset(-1), size(-1) {}
    UniformBufferBinding(legit::Buffer *_buffer, uint32_t _shaderBindingId, vk::DeviceSize _offset, vk::DeviceSize _size) : buffer(_buffer), shaderBindingId(_shaderBindingId), offset(_offset), size(_size)
    {
      assert(_buffer);
    }
    bool operator < (const UniformBufferBinding &other) const
    {
      return std::tie(buffer, shaderBindingId, offset, size) < std::tie(other.buffer, other.shaderBindingId, other.offset, other.size);
    }
    legit::Buffer *buffer;
    uint32_t shaderBindingId;
    vk::DeviceSize offset;
    vk::DeviceSize size;
  };

  struct StorageBufferBinding
  {
    struct Descriptor
    {
      legit::Buffer *buffer = nullptr;
      vk::DeviceSize offset = vk::DeviceSize(-1);
      vk::DeviceSize size = vk::DeviceSize(-1);
      bool operator < (const Descriptor &other) const
      {
        return std::tie(buffer, offset, size) < std::tie(other.buffer, other.offset, other.size);
      }
    };
    
    StorageBufferBinding() {}
    StorageBufferBinding(legit::Buffer *_buffer, uint32_t _shaderBindingId, vk::DeviceSize _offset, vk::DeviceSize _size) : shaderBindingId(_shaderBindingId)
    {
      assert(_buffer);
      Descriptor desc;
      desc.buffer = _buffer;
      desc.offset = _offset;
      desc.size = _size;
      descriptors.push_back(desc);
    }
    
    StorageBufferBinding(uint32_t _shaderBindingId, std::vector<Descriptor> _descriptors) : shaderBindingId(_shaderBindingId), descriptors(_descriptors)
    {
    }

    bool operator < (const StorageBufferBinding &other) const
    {
      return std::tie(descriptors, shaderBindingId) < std::tie(other.descriptors, other.shaderBindingId);
    }

    std::vector<Descriptor> descriptors;
    uint32_t shaderBindingId;
  };

  struct StorageImageBinding
  {
    StorageImageBinding() : imageView(nullptr) {}
    StorageImageBinding(legit::ImageView *_imageView, uint32_t _shaderBindingId) : imageView(_imageView), shaderBindingId(_shaderBindingId)
    {
      assert(_imageView);
    }
    bool operator < (const StorageImageBinding &other) const
    {
      return std::tie(imageView, shaderBindingId) < std::tie(other.imageView, other.shaderBindingId);
    }
    legit::ImageView *imageView;
    uint32_t shaderBindingId;
  };

  struct AccelerationStructureBinding
  {
    AccelerationStructureBinding() : accelerationStructure(nullptr) {}
    AccelerationStructureBinding(legit::AccelerationStructure *_accelerationStructure, uint32_t _shaderBindingId) : accelerationStructure(_accelerationStructure), shaderBindingId(_shaderBindingId)
    {
      assert(_accelerationStructure);
    }
    bool operator < (const AccelerationStructureBinding &other) const
    {
      return std::tie(accelerationStructure, shaderBindingId) < std::tie(other.accelerationStructure, other.shaderBindingId);
    }
    legit::AccelerationStructure *accelerationStructure;
    uint32_t shaderBindingId;
  };

  
  class DescriptorSetLayoutKey
  {
  public:
    struct UniformBase;
    using UniformId = ShaderResourceId<UniformBase>;
    struct UniformBufferBase;
    using UniformBufferId = ShaderResourceId<UniformBufferBase>;
    struct ImageSamplerBase;
    using ImageSamplerId = ShaderResourceId<ImageSamplerBase>;
    struct TextureBase;
    using TextureId = ShaderResourceId<TextureBase>;
    struct SamplerBase;
    using SamplerId = ShaderResourceId<SamplerBase>;
    struct StorageBufferBase;
    using StorageBufferId = ShaderResourceId<StorageBufferBase>;
    struct StorageImageBase;
    using StorageImageId = ShaderResourceId<StorageImageBase>;
    struct AccelerationStructureBase;
    using AccelerationStructureId = ShaderResourceId<AccelerationStructureBase>;


    struct UniformData
    {
      bool operator<(const UniformData &other) const
      {
        return std::tie(name, offsetInBinding, size) < std::tie(other.name, other.offsetInBinding, other.size);
      }
      std::string name;
      uint32_t offsetInBinding;
      uint32_t size;
      UniformBufferId uniformBufferId;
    };
    struct UniformBufferData
    {
      bool operator<(const UniformBufferData &other) const
      {
        return std::tie(name, shaderBindingIndex, size, stageFlags) < std::tie(other.name, other.shaderBindingIndex, other.size, other.stageFlags);
      }

      std::string name;
      uint32_t shaderBindingIndex;
      vk::ShaderStageFlags stageFlags;

      uint32_t size;
      uint32_t offsetInSet;
      std::vector<UniformId> uniformIds;
    };
    struct ImageSamplerData
    {
      bool operator<(const ImageSamplerData &other) const
      {
        return std::tie(name, shaderBindingIndex) < std::tie(other.name, other.shaderBindingIndex);
      }

      std::string name;
      uint32_t shaderBindingIndex;
      vk::ShaderStageFlags stageFlags;
    };
    struct TextureData
    {
      bool operator<(const TextureData &other) const
      {
        return std::tie(name, shaderBindingIndex) < std::tie(other.name, other.shaderBindingIndex);
      }

      std::string name;
      uint32_t shaderBindingIndex;
      vk::ShaderStageFlags stageFlags;
    };
    struct SamplerData
    {
      bool operator<(const SamplerData &other) const
      {
        return std::tie(name, shaderBindingIndex) < std::tie(other.name, other.shaderBindingIndex);
      }

      std::string name;
      uint32_t shaderBindingIndex;
      vk::ShaderStageFlags stageFlags;
    };
    struct StorageBufferData
    {
      bool operator<(const StorageBufferData &other) const
      {
        return std::tie(name, count, shaderBindingIndex, podPartSize, arrayMemberSize) < std::tie(other.name, other.count, other.shaderBindingIndex, other.podPartSize, other.arrayMemberSize);
      }

      std::string name;
      uint32_t count;
      uint32_t shaderBindingIndex;
      vk::ShaderStageFlags stageFlags;

      uint32_t podPartSize;
      uint32_t arrayMemberSize;
      uint32_t offsetInSet;
    };
    struct StorageImageData
    {
      bool operator<(const StorageImageData &other) const
      {
        return std::tie(name, shaderBindingIndex) < std::tie(other.name, other.shaderBindingIndex);
      }

      std::string name;
      uint32_t shaderBindingIndex;
      vk::ShaderStageFlags stageFlags;
    };
    struct AccelerationStructureData
    {
      bool operator<(const AccelerationStructureData &other) const
      {
        return std::tie(name, shaderBindingIndex) < std::tie(other.name, other.shaderBindingIndex);
      }

      std::string name;
      uint32_t shaderBindingIndex;
      vk::ShaderStageFlags stageFlags;
    };

    size_t GetUniformBuffersCount() const
    {
      return uniformBufferDatum.size();
    }
    void GetUniformBufferIds(UniformBufferId *dstUniformBufferIds, size_t count = -1, size_t offset = 0) const
    {
      if (count == -1)
        count = uniformBufferDatum.size();
      assert(count + offset <= uniformBufferDatum.size());
      for (size_t index = offset; index < offset + count; index++)
        dstUniformBufferIds[index] = UniformBufferId(index);
    }
    UniformBufferId GetUniformBufferId(std::string bufferName) const
    {
      auto it = uniformBufferNameToIds.find(bufferName);
      if (it == uniformBufferNameToIds.end())
        return UniformBufferId();
      return it->second;
    }
    UniformBufferId GetUniformBufferId(uint32_t bufferBindingId) const
    {
      auto it = uniformBufferBindingToIds.find(bufferBindingId);
      if (it == uniformBufferBindingToIds.end())
        return UniformBufferId();
      return it->second;
    }    
    UniformBufferData GetUniformBufferInfo(UniformBufferId uniformBufferId) const
    {
      return uniformBufferDatum[uniformBufferId.id];
    }
    UniformBufferBinding MakeUniformBufferBinding(std::string bufferName, legit::Buffer *_buffer, vk::DeviceSize _offset = 0, vk::DeviceSize _size = VK_WHOLE_SIZE) const
    {
      auto uniformBufferId = GetUniformBufferId(bufferName);
      assert(uniformBufferId.IsValid());
      auto uniformBufferInfo = GetUniformBufferInfo(uniformBufferId);
      return UniformBufferBinding(_buffer, uniformBufferInfo.shaderBindingIndex, _offset, _size);
    }


    size_t GetStorageBuffersCount() const
    {
      return storageBufferDatum.size();
    }
    void GetStorageBufferIds(StorageBufferId *dstStorageBufferIds, size_t count = -1, size_t offset = 0) const
    {
      if (count == -1)
        count = storageBufferDatum.size();
      assert(count + offset <= storageBufferDatum.size());
      for (size_t index = offset; index < offset + count; index++)
        dstStorageBufferIds[index] = StorageBufferId(index);
    }
    StorageBufferId GetStorageBufferId(std::string storageBufferName) const
    {
      auto it = storageBufferNameToIds.find(storageBufferName);
      if (it == storageBufferNameToIds.end())
        return StorageBufferId();
      return it->second;
    }
    StorageBufferId GetStorageBufferId(uint32_t bufferBindingId) const
    {
      auto it = storageBufferBindingToIds.find(bufferBindingId);
      if (it == storageBufferBindingToIds.end())
        return StorageBufferId();
      return it->second;
    }
    StorageBufferData GetStorageBufferInfo(StorageBufferId storageBufferId) const
    {
      return storageBufferDatum[storageBufferId.id];
    }

    StorageBufferBinding MakeStorageBufferBinding(std::string bufferName, legit::Buffer *_buffer, vk::DeviceSize _offset = 0, vk::DeviceSize _size = VK_WHOLE_SIZE) const
    {
      auto storageBufferId = GetStorageBufferId(bufferName);
      assert(storageBufferId.IsValid());
      auto storageBufferInfo = GetStorageBufferInfo(storageBufferId);
      return StorageBufferBinding(_buffer, storageBufferInfo.shaderBindingIndex, _offset, _size);
    }

    StorageBufferBinding MakeStorageBufferBinding(std::string bufferName, std::vector<legit::Buffer*> _buffers) const
    {
      auto storageBufferId = GetStorageBufferId(bufferName);
      assert(storageBufferId.IsValid());
      auto storageBufferInfo = GetStorageBufferInfo(storageBufferId);
      
      assert(storageBufferInfo.count == 0 || storageBufferInfo.count == _buffers.size());
      std::vector<StorageBufferBinding::Descriptor> descs;
      for(auto buffer : _buffers)
      {
        StorageBufferBinding::Descriptor desc;
        desc.buffer = buffer;
        desc.offset = 0;
        desc.size = VK_WHOLE_SIZE;
        descs.push_back(desc);
      }
      return StorageBufferBinding(storageBufferInfo.shaderBindingIndex, descs);
    }
    
    template<typename MemberType>
    StorageBufferBinding MakeCheckedStorageBufferBinding(std::string bufferName, legit::Buffer* _buffer, vk::DeviceSize _offset = 0, vk::DeviceSize _size = VK_WHOLE_SIZE) const
    {
      auto storageBufferId = GetStorageBufferId(bufferName);
      assert(storageBufferId.IsValid());
      auto storageBufferInfo = GetStorageBufferInfo(storageBufferId);
      assert(storageBufferInfo.arrayMemberSize == sizeof(MemberType));
      return StorageBufferBinding(_buffer, storageBufferInfo.shaderBindingIndex, _offset, _size);
    }


    size_t GetUniformsCount() const
    {
      return uniformDatum.size();
    }
    void GetUniformIds(UniformId *dstUniformIds, size_t count = -1, size_t offset = 0) const
    {
      if (count == -1)
        count = uniformDatum.size();
      assert(count + offset <= uniformDatum.size());
      for (size_t index = offset; index < offset + count; index++)
        dstUniformIds[index] = UniformId(index);
    }
    UniformData GetUniformInfo(UniformId uniformId) const
    {
      return uniformDatum[uniformId.id];
    }
    UniformId GetUniformId(std::string name) const
    {
      auto it = uniformNameToIds.find(name);
      if (it == uniformNameToIds.end())
        return UniformId();
      return it->second;
    }

    size_t GetImageSamplersCount() const
    {
      return imageSamplerDatum.size();
    }
    void GetImageSamplerIds(ImageSamplerId *dstImageSamplerIds, size_t count = -1, size_t offset = 0) const
    {
      if (count == -1)
        count = imageSamplerDatum.size();
      assert(count + offset <= imageSamplerDatum.size());
      for (size_t index = offset; index < offset + count; index++)
        dstImageSamplerIds[index] = ImageSamplerId(index);
    }
    ImageSamplerData GetImageSamplerInfo(ImageSamplerId imageSamplerId) const
    {
      return imageSamplerDatum[imageSamplerId.id];
    }
    ImageSamplerId GetImageSamplerId(std::string imageSamplerName) const
    {
      auto it = imageSamplerNameToIds.find(imageSamplerName);
      if (it == imageSamplerNameToIds.end())
        return ImageSamplerId();
      return it->second;
    }
    ImageSamplerId GetImageSamplerId(uint32_t shaderBindingIndex) const
    {
      auto it = imageSamplerBindingToIds.find(shaderBindingIndex);
      if (it == imageSamplerBindingToIds.end())
        return ImageSamplerId();
      return it->second;
    }
    
    size_t GetTexturesCount() const
    {
      return textureDatum.size();
    }
    void GetTextureIds(TextureId *dstTextureIds, size_t count = -1, size_t offset = 0) const
    {
      if (count == -1)
        count = textureDatum.size();
      assert(count + offset <= textureDatum.size());
      for (size_t index = offset; index < offset + count; index++)
        dstTextureIds[index] = TextureId(index);
    }
    TextureData GetTextureInfo(TextureId textureId) const
    {
      return textureDatum[textureId.id];
    }
    TextureId GetTextureId(std::string textureName) const
    {
      auto it = textureNameToIds.find(textureName);
      if (it == textureNameToIds.end())
        return TextureId();
      return it->second;
    }
    TextureId GetTextureId(uint32_t shaderBindingIndex) const
    {
      auto it = textureBindingToIds.find(shaderBindingIndex);
      if (it == textureBindingToIds.end())
        return TextureId();
      return it->second;
    }
    
    size_t GetSamplersCount() const
    {
      return samplerDatum.size();
    }
    void GetSamplerIds(SamplerId *dstSamplerIds, size_t count = -1, size_t offset = 0) const
    {
      if (count == -1)
        count = samplerDatum.size();
      assert(count + offset <= samplerDatum.size());
      for (size_t index = offset; index < offset + count; index++)
        dstSamplerIds[index] = SamplerId(index);
    }
    SamplerData GetSamplerInfo(SamplerId samplerId) const
    {
      return samplerDatum[samplerId.id];
    }
    SamplerId GetSamplerId(std::string samplerName) const
    {
      auto it = samplerNameToIds.find(samplerName);
      if (it == samplerNameToIds.end())
        return SamplerId();
      return it->second;
    }
    SamplerId GetSamplerId(uint32_t shaderBindingIndex) const
    {
      auto it = samplerBindingToIds.find(shaderBindingIndex);
      if (it == samplerBindingToIds.end())
        return SamplerId();
      return it->second;
    }
    
    ImageSamplerBinding MakeImageSamplerBinding(std::string imageSamplerName, legit::ImageView *_imageView, legit::Sampler *_sampler) const
    {
      auto imageSamplerId = GetImageSamplerId(imageSamplerName);
      assert(imageSamplerId.IsValid());
      auto imageSamplerInfo = GetImageSamplerInfo(imageSamplerId);
      return ImageSamplerBinding(_imageView, _sampler, imageSamplerInfo.shaderBindingIndex);
    }
    TextureBinding MakeTextureBinding(std::string textureName, legit::ImageView *_imageView) const
    {
      auto textureId = GetTextureId(textureName);
      assert(textureId.IsValid());
      auto textureInfo = GetTextureInfo(textureId);
      return TextureBinding(_imageView, textureInfo.shaderBindingIndex);
    }
    SamplerBinding MakeSamplerBinding(std::string samplerName, legit::Sampler *_sampler) const
    {
      auto samplerId = GetSamplerId(samplerName);
      assert(samplerId.IsValid());
      auto samplerInfo = GetSamplerInfo(samplerId);
      return SamplerBinding(_sampler, samplerInfo.shaderBindingIndex);
    }

    size_t GetStorageImagesCount() const
    {
      return storageImageDatum.size();
    }
    void GetStorageImageIds(StorageImageId *dstStorageImageIds, size_t count = -1, size_t offset = 0) const
    {
      if (count == -1)
        count = storageImageDatum.size();
      assert(count + offset <= storageImageDatum.size());
      for (size_t index = offset; index < offset + count; index++)
        dstStorageImageIds[index] = StorageImageId(index);
    }
    StorageImageId GetStorageImageId(std::string storageImageName) const
    {
      auto it = storageImageNameToIds.find(storageImageName);
      if (it == storageImageNameToIds.end())
        return StorageImageId();
      return it->second;
    }
    StorageImageId GetStorageImageId(uint32_t bufferBindingId) const
    {
      auto it = storageImageBindingToIds.find(bufferBindingId);
      if (it == storageImageBindingToIds.end())
        return StorageImageId();
      return it->second;
    }
    StorageImageData GetStorageImageInfo(StorageImageId storageImageId) const
    {
      return storageImageDatum[storageImageId.id];
    }
    StorageImageBinding MakeStorageImageBinding(std::string imageName, legit::ImageView *_imageView) const
    {
      auto imageId = GetStorageImageId(imageName);
      assert(imageId.IsValid());
      auto storageImageInfo = GetStorageImageInfo(imageId);
      return StorageImageBinding(_imageView, storageImageInfo.shaderBindingIndex);
    }
    
    size_t GetAccelerationStructuresCount() const
    {
      return accelerationStructureDatum.size();
    }
    void GetAccelerationStructureIds(AccelerationStructureId *dstAccelerationStructureIds, size_t count = -1, size_t offset = 0) const
    {
      if (count == -1)
        count = accelerationStructureDatum.size();
      assert(count + offset <= accelerationStructureDatum.size());
      for (size_t index = offset; index < offset + count; index++)
        dstAccelerationStructureIds[index] = AccelerationStructureId(index);
    }
    AccelerationStructureId GetAccelerationStructureId(std::string accelerationStructureName) const
    {
      auto it = accelerationStructureNameToIds.find(accelerationStructureName);
      if (it == accelerationStructureNameToIds.end())
        return AccelerationStructureId();
      return it->second;
    }
    AccelerationStructureId GetAccelerationStructureId(uint32_t bufferBindingId) const
    {
      auto it = accelerationStructureBindingToIds.find(bufferBindingId);
      if (it == accelerationStructureBindingToIds.end())
        return AccelerationStructureId();
      return it->second;
    }
    AccelerationStructureData GetAccelerationStructureInfo(AccelerationStructureId accelerationStructureId) const
    {
      return accelerationStructureDatum[accelerationStructureId.id];
    }
    AccelerationStructureBinding MakeAccelerationStructureBinding(std::string accelerationStructureName, legit::AccelerationStructure *_accelerationStructure) const
    {
      auto accelerationStructureId = GetAccelerationStructureId(accelerationStructureName);
      assert(accelerationStructureId.IsValid());
      auto accelerationStructureInfo = GetAccelerationStructureInfo(accelerationStructureId);
      return AccelerationStructureBinding(_accelerationStructure, accelerationStructureInfo.shaderBindingIndex);
    }

    uint32_t GetTotalConstantBufferSize() const
    {
      return size;
    }

    bool IsEmpty() const
    {
      return GetImageSamplersCount() == 0 && GetTexturesCount() == 0 && GetSamplersCount() == 0 && GetUniformBuffersCount() == 0 && GetStorageImagesCount() == 0 && GetStorageBuffersCount() == 0;
    }

    bool operator<(const DescriptorSetLayoutKey &other) const
    {
      return 
        std::tie(uniformDatum, uniformBufferDatum, imageSamplerDatum, textureDatum, samplerDatum, storageBufferDatum, storageImageDatum, accelerationStructureDatum) < 
        std::tie(other.uniformDatum, other.uniformBufferDatum, other.imageSamplerDatum, other.textureDatum, other.samplerDatum, other.storageBufferDatum, other.storageImageDatum, other.accelerationStructureDatum);
    }

    static DescriptorSetLayoutKey Merge(DescriptorSetLayoutKey *setLayouts, size_t setsCount)
    {
      DescriptorSetLayoutKey res;
      if (setsCount <= 0) return res;

      res.setShaderId = setLayouts[0].setShaderId;
      for (size_t layoutIndex = 0; layoutIndex < setsCount; layoutIndex++)
        assert(setLayouts[layoutIndex].setShaderId == res.setShaderId);

      std::set<uint32_t> uniformBufferBindings;
      std::set<uint32_t> imageSamplerBindings;
      std::set<uint32_t> textureBindings;
      std::set<uint32_t> samplerBindings;
      std::set<uint32_t> storageBufferBindings;
      std::set<uint32_t> storageImageBindings;
      std::set<uint32_t> accelerationStructureBindings;

      for (size_t setIndex = 0; setIndex < setsCount; setIndex++)
      {
        auto &setLayout = setLayouts[setIndex];
        for (auto &uniformBufferData : setLayout.uniformBufferDatum)
        {
          uniformBufferBindings.insert(uniformBufferData.shaderBindingIndex);
        }
        for (auto &imageSamplerData : setLayout.imageSamplerDatum)
        {
          imageSamplerBindings.insert(imageSamplerData.shaderBindingIndex);
        }
        for (auto &textureData : setLayout.textureDatum)
        {
          textureBindings.insert(textureData.shaderBindingIndex);
        }
        for (auto &samplerData : setLayout.samplerDatum)
        {
          samplerBindings.insert(samplerData.shaderBindingIndex);
        }
        for (auto &storageBufferData : setLayout.storageBufferDatum)
        {
          storageBufferBindings.insert(storageBufferData.shaderBindingIndex);
        }
        for (auto &storageImageData : setLayout.storageImageDatum)
        {
          storageImageBindings.insert(storageImageData.shaderBindingIndex);
        }
        for (auto &accelerationStructureData : setLayout.accelerationStructureDatum)
        {
          accelerationStructureBindings.insert(accelerationStructureData.shaderBindingIndex);
        }
      }

      for (auto &uniformBufferBinding : uniformBufferBindings)
      {
        UniformBufferId dstUniformBufferId;
        for (size_t setIndex = 0; setIndex < setsCount; setIndex++)
        {
          auto &srcLayout = setLayouts[setIndex];
          auto srcUniformBufferId = srcLayout.GetUniformBufferId(uniformBufferBinding);
          if (!srcUniformBufferId.IsValid()) continue;
          const auto &srcUniformBuffer = srcLayout.uniformBufferDatum[srcUniformBufferId.id];
          assert(srcUniformBuffer.shaderBindingIndex == uniformBufferBinding);

          if (!dstUniformBufferId.IsValid())
          {
            dstUniformBufferId = UniformBufferId(res.uniformBufferDatum.size());
            res.uniformBufferDatum.push_back(UniformBufferData());
            auto &dstUniformBuffer = res.uniformBufferDatum.back();

            dstUniformBuffer.shaderBindingIndex = srcUniformBuffer.shaderBindingIndex;
            dstUniformBuffer.name = srcUniformBuffer.name;
            dstUniformBuffer.size = srcUniformBuffer.size;
            dstUniformBuffer.stageFlags = srcUniformBuffer.stageFlags;

            for (auto srcUniformId : srcUniformBuffer.uniformIds)
            {
              auto srcUniformData = srcLayout.GetUniformInfo(srcUniformId);

              DescriptorSetLayoutKey::UniformId dstUniformId = DescriptorSetLayoutKey::UniformId(res.uniformDatum.size());
              res.uniformDatum.push_back(DescriptorSetLayoutKey::UniformData());
              DescriptorSetLayoutKey::UniformData &dstUniformData = res.uniformDatum.back();

              dstUniformData.uniformBufferId = dstUniformBufferId;
              dstUniformData.offsetInBinding = srcUniformData.offsetInBinding;
              dstUniformData.size = srcUniformData.size;
              dstUniformData.name = srcUniformData.name;

              //memberData.
              dstUniformBuffer.uniformIds.push_back(dstUniformId);
            }
          }
          else
          {
            auto &dstUniformBuffer = res.uniformBufferDatum[dstUniformBufferId.id];
            dstUniformBuffer.stageFlags |= srcUniformBuffer.stageFlags;
            assert(srcUniformBuffer.shaderBindingIndex == dstUniformBuffer.shaderBindingIndex);
            assert(srcUniformBuffer.name == dstUniformBuffer.name);
            assert(srcUniformBuffer.size == dstUniformBuffer.size);
          }
        }
      }

      for (auto &imageSamplerBinding : imageSamplerBindings)
      {
        ImageSamplerId dstImageSamplerId;
        for (size_t setIndex = 0; setIndex < setsCount; setIndex++)
        {
          auto &srcLayout = setLayouts[setIndex];
          auto srcImageSamplerId = srcLayout.GetImageSamplerId(imageSamplerBinding);
          if (!srcImageSamplerId.IsValid()) continue;
          const auto &srcImageSampler = srcLayout.imageSamplerDatum[srcImageSamplerId.id];
          assert(srcImageSampler.shaderBindingIndex == imageSamplerBinding);

          if (!dstImageSamplerId.IsValid())
          {
            dstImageSamplerId = ImageSamplerId(res.imageSamplerDatum.size());
            res.imageSamplerDatum.push_back(ImageSamplerData());
            auto &dstImageSampler = res.imageSamplerDatum.back();

            dstImageSampler.shaderBindingIndex = srcImageSampler.shaderBindingIndex;
            dstImageSampler.name = srcImageSampler.name;
            dstImageSampler.stageFlags = srcImageSampler.stageFlags;
          }
          else
          {
            auto &dstImageSampler = res.imageSamplerDatum[dstImageSamplerId.id];
            dstImageSampler.stageFlags |= srcImageSampler.stageFlags;
            assert(srcImageSampler.shaderBindingIndex == dstImageSampler.shaderBindingIndex);
            assert(srcImageSampler.name == dstImageSampler.name);
          }
        }
      }

      for (auto &textureBinding : textureBindings)
      {
        TextureId dstTextureId;
        for (size_t setIndex = 0; setIndex < setsCount; setIndex++)
        {
          auto &srcLayout = setLayouts[setIndex];
          auto srcTextureId = srcLayout.GetTextureId(textureBinding);
          if (!srcTextureId.IsValid()) continue;
          const auto &srcTexture = srcLayout.textureDatum[srcTextureId.id];
          assert(srcTexture.shaderBindingIndex == textureBinding);

          if (!dstTextureId.IsValid())
          {
            dstTextureId = TextureId(res.textureDatum.size());
            res.textureDatum.push_back(TextureData());
            auto &dstTexture = res.textureDatum.back();

            dstTexture.shaderBindingIndex = srcTexture.shaderBindingIndex;
            dstTexture.name = srcTexture.name;
            dstTexture.stageFlags = srcTexture.stageFlags;
          }
          else
          {
            auto &dstTexture = res.textureDatum[dstTextureId.id];
            dstTexture.stageFlags |= srcTexture.stageFlags;
            assert(srcTexture.shaderBindingIndex == dstTexture.shaderBindingIndex);
            assert(srcTexture.name == dstTexture.name);
          }
        }
      }
      
      for (auto &samplerBinding : samplerBindings)
      {
        SamplerId dstSamplerId;
        for (size_t setIndex = 0; setIndex < setsCount; setIndex++)
        {
          auto &srcLayout = setLayouts[setIndex];
          auto srcSamplerId = srcLayout.GetSamplerId(samplerBinding);
          if (!srcSamplerId.IsValid()) continue;
          const auto &srcSampler = srcLayout.samplerDatum[srcSamplerId.id];
          assert(srcSampler.shaderBindingIndex == samplerBinding);

          if (!dstSamplerId.IsValid())
          {
            dstSamplerId = SamplerId(res.samplerDatum.size());
            res.samplerDatum.push_back(SamplerData());
            auto &dstSampler = res.samplerDatum.back();

            dstSampler.shaderBindingIndex = srcSampler.shaderBindingIndex;
            dstSampler.name = srcSampler.name;
            dstSampler.stageFlags = srcSampler.stageFlags;
          }
          else
          {
            auto &dstSampler = res.samplerDatum[dstSamplerId.id];
            dstSampler.stageFlags |= srcSampler.stageFlags;
            assert(srcSampler.shaderBindingIndex == dstSampler.shaderBindingIndex);
            assert(srcSampler.name == dstSampler.name);
          }
        }
      }

      for (auto &storageBufferBinding : storageBufferBindings)
      {
        StorageBufferId dstStorageBufferId;
        for (size_t setIndex = 0; setIndex < setsCount; setIndex++)
        {
          auto &srcLayout = setLayouts[setIndex];
          auto srcStorageBufferId = srcLayout.GetStorageBufferId(storageBufferBinding);
          if (!srcStorageBufferId.IsValid()) continue;
          const auto &srcStorageBuffer = srcLayout.storageBufferDatum[srcStorageBufferId.id];
          assert(srcStorageBuffer.shaderBindingIndex == storageBufferBinding);

          if (!dstStorageBufferId.IsValid())
          {
            dstStorageBufferId = StorageBufferId(res.storageBufferDatum.size());
            res.storageBufferDatum.push_back(StorageBufferData());
            auto &dstStorageBuffer = res.storageBufferDatum.back();

            dstStorageBuffer.shaderBindingIndex = srcStorageBuffer.shaderBindingIndex;
            dstStorageBuffer.name = srcStorageBuffer.name;
            dstStorageBuffer.count = srcStorageBuffer.count;
            dstStorageBuffer.arrayMemberSize = srcStorageBuffer.arrayMemberSize;
            dstStorageBuffer.podPartSize = srcStorageBuffer.podPartSize;
            dstStorageBuffer.stageFlags = srcStorageBuffer.stageFlags;
          }
          else
          {
            auto &dstStorageBuffer = res.storageBufferDatum[dstStorageBufferId.id];
            dstStorageBuffer.stageFlags |= srcStorageBuffer.stageFlags;
            assert(srcStorageBuffer.shaderBindingIndex == dstStorageBuffer.shaderBindingIndex);
            assert(srcStorageBuffer.name == dstStorageBuffer.name);
            assert(srcStorageBuffer.count == dstStorageBuffer.count);
            assert(srcStorageBuffer.podPartSize == dstStorageBuffer.podPartSize);
            assert(srcStorageBuffer.arrayMemberSize == dstStorageBuffer.arrayMemberSize);
          }
        }
      }
      
      for (auto &storageImageBinding : storageImageBindings)
      {
        StorageImageId dstStorageImageId;
        for (size_t setIndex = 0; setIndex < setsCount; setIndex++)
        {
          auto &srcLayout = setLayouts[setIndex];
          auto srcStorageImageId = srcLayout.GetStorageImageId(storageImageBinding);
          if (!srcStorageImageId.IsValid()) continue;
          const auto &srcStorageImage = srcLayout.storageImageDatum[srcStorageImageId.id];
          assert(srcStorageImage.shaderBindingIndex == storageImageBinding);

          if (!dstStorageImageId.IsValid())
          {
            dstStorageImageId = StorageImageId(res.storageImageDatum.size());
            res.storageImageDatum.push_back(StorageImageData());
            auto &dstStorageImage = res.storageImageDatum.back();

            dstStorageImage.shaderBindingIndex = srcStorageImage.shaderBindingIndex;
            dstStorageImage.name = srcStorageImage.name;
            dstStorageImage.stageFlags = srcStorageImage.stageFlags;
          }
          else
          {
            auto &dstStorageImage = res.storageImageDatum[dstStorageImageId.id];
            dstStorageImage.stageFlags |= srcStorageImage.stageFlags;
            assert(srcStorageImage.shaderBindingIndex == dstStorageImage.shaderBindingIndex);
            assert(srcStorageImage.name == dstStorageImage.name);
          }
        }
      }
      
      for (auto &accelerationStructureBinding : accelerationStructureBindings)
      {
        AccelerationStructureId dstAccelerationStructureId;
        for (size_t setIndex = 0; setIndex < setsCount; setIndex++)
        {
          auto &srcLayout = setLayouts[setIndex];
          auto srcAccelerationStructureId = srcLayout.GetAccelerationStructureId(accelerationStructureBinding);
          if (!srcAccelerationStructureId.IsValid()) continue;
          const auto &srcAccelerationStructure = srcLayout.accelerationStructureDatum[srcAccelerationStructureId.id];
          assert(srcAccelerationStructure.shaderBindingIndex == accelerationStructureBinding);

          if (!dstAccelerationStructureId.IsValid())
          {
            dstAccelerationStructureId = AccelerationStructureId(res.accelerationStructureDatum.size());
            res.accelerationStructureDatum.push_back(AccelerationStructureData());
            auto &dstAccelerationStructure = res.accelerationStructureDatum.back();

            dstAccelerationStructure.shaderBindingIndex = srcAccelerationStructure.shaderBindingIndex;
            dstAccelerationStructure.name = srcAccelerationStructure.name;
            dstAccelerationStructure.stageFlags = srcAccelerationStructure.stageFlags;
          }
          else
          {
            auto &dstAccelerationStructure = res.accelerationStructureDatum[dstAccelerationStructureId.id];
            dstAccelerationStructure.stageFlags |= srcAccelerationStructure.stageFlags;
            assert(srcAccelerationStructure.shaderBindingIndex == dstAccelerationStructure.shaderBindingIndex);
            assert(srcAccelerationStructure.name == dstAccelerationStructure.name);
          }
        }
      }


      res.RebuildIndex();
      return res;
    }
  private:
    void RebuildIndex()
    {
      uniformNameToIds.clear();
      for(size_t uniformIndex = 0; uniformIndex < uniformDatum.size(); uniformIndex++)
      {
        UniformId uniformId = UniformId(uniformIndex);
        auto &uniformData = uniformDatum[uniformIndex];
        uniformNameToIds[uniformData.name] = uniformId;
      }

      uniformBufferNameToIds.clear();
      uniformBufferBindingToIds.clear();
      this->size = 0;
      for (size_t uniformBufferIndex = 0; uniformBufferIndex < uniformBufferDatum.size(); uniformBufferIndex++)
      {
        UniformBufferId uniformBufferId = UniformBufferId(uniformBufferIndex);
        auto &uniformBufferData = uniformBufferDatum[uniformBufferIndex];
        uniformBufferNameToIds[uniformBufferData.name] = uniformBufferId;
        uniformBufferBindingToIds[uniformBufferData.shaderBindingIndex] = uniformBufferId;
        this->size += uniformBufferData.size;
      }

      imageSamplerNameToIds.clear();
      imageSamplerBindingToIds.clear();
      for (size_t imageSamplerIndex = 0; imageSamplerIndex < imageSamplerDatum.size(); imageSamplerIndex++)
      {
        ImageSamplerId imageSamplerId = ImageSamplerId(imageSamplerIndex);
        auto &imageSamplerData = imageSamplerDatum[imageSamplerIndex];
        imageSamplerNameToIds[imageSamplerData.name] = imageSamplerId;
        imageSamplerBindingToIds[imageSamplerData.shaderBindingIndex] = imageSamplerId;
      }
      
      textureNameToIds.clear();
      textureBindingToIds.clear();
      for (size_t textureIndex = 0; textureIndex < textureDatum.size(); textureIndex++)
      {
        TextureId textureId = TextureId(textureIndex);
        auto &textureData = textureDatum[textureIndex];
        textureNameToIds[textureData.name] = textureId;
        textureBindingToIds[textureData.shaderBindingIndex] = textureId;
      }

      samplerNameToIds.clear();
      samplerBindingToIds.clear();
      for (size_t samplerIndex = 0; samplerIndex < samplerDatum.size(); samplerIndex++)
      {
        SamplerId samplerId = SamplerId(samplerIndex);
        auto &samplerData = samplerDatum[samplerIndex];
        samplerNameToIds[samplerData.name] = samplerId;
        samplerBindingToIds[samplerData.shaderBindingIndex] = samplerId;
      }

      storageBufferNameToIds.clear();
      storageBufferBindingToIds.clear();
      for (size_t storageBufferIndex = 0; storageBufferIndex < storageBufferDatum.size(); storageBufferIndex++)
      {
        StorageBufferId storageBufferId = StorageBufferId(storageBufferIndex);
        auto &storageBufferData = storageBufferDatum[storageBufferIndex];
        storageBufferNameToIds[storageBufferData.name] = storageBufferId;
        storageBufferBindingToIds[storageBufferData.shaderBindingIndex] = storageBufferId;
      }

      storageImageNameToIds.clear();
      storageImageBindingToIds.clear();
      for (size_t storageImageIndex = 0; storageImageIndex < storageImageDatum.size(); storageImageIndex++)
      {
        StorageImageId storageImageId = StorageImageId(storageImageIndex);
        auto &storageImageData = storageImageDatum[storageImageIndex];
        storageImageNameToIds[storageImageData.name] = storageImageId;
        storageImageBindingToIds[storageImageData.shaderBindingIndex] = storageImageId;
      }
      
      accelerationStructureNameToIds.clear();
      accelerationStructureBindingToIds.clear();
      for (size_t accelerationStructureIndex = 0; accelerationStructureIndex < accelerationStructureDatum.size(); accelerationStructureIndex++)
      {
        AccelerationStructureId accelerationStructureId = AccelerationStructureId(accelerationStructureIndex);
        auto &accelerationStructureData = accelerationStructureDatum[accelerationStructureIndex];
        accelerationStructureNameToIds[accelerationStructureData.name] = accelerationStructureId;
        accelerationStructureBindingToIds[accelerationStructureData.shaderBindingIndex] = accelerationStructureId;
      }
    }


    friend class Shader;
    uint32_t setShaderId;
    uint32_t size;

    std::vector<UniformData> uniformDatum;
    std::vector<UniformBufferData> uniformBufferDatum;
    std::vector<ImageSamplerData> imageSamplerDatum;
    std::vector<TextureData> textureDatum;
    std::vector<SamplerData> samplerDatum;
    std::vector<StorageBufferData> storageBufferDatum;
    std::vector<StorageImageData> storageImageDatum;
    std::vector<AccelerationStructureData> accelerationStructureDatum;

    std::map<std::string, UniformId> uniformNameToIds;
    std::map<std::string, UniformBufferId> uniformBufferNameToIds;
    std::map<uint32_t, UniformBufferId> uniformBufferBindingToIds;
    std::map<std::string, ImageSamplerId> imageSamplerNameToIds;
    std::map<uint32_t, ImageSamplerId> imageSamplerBindingToIds;
    std::map<std::string, TextureId> textureNameToIds;
    std::map<uint32_t, TextureId> textureBindingToIds;
    std::map<std::string, SamplerId> samplerNameToIds;
    std::map<uint32_t, SamplerId> samplerBindingToIds;
    std::map<std::string, StorageBufferId> storageBufferNameToIds;
    std::map<uint32_t, StorageBufferId> storageBufferBindingToIds;
    std::map<std::string, StorageImageId> storageImageNameToIds;
    std::map<uint32_t, StorageImageId> storageImageBindingToIds;
    std::map<std::string, AccelerationStructureId> accelerationStructureNameToIds;
    std::map<uint32_t, AccelerationStructureId> accelerationStructureBindingToIds;
  };
  class Shader
  {
  public:
    Shader(vk::Device logicalDevice, std::string shaderFile)
    {
      auto bytecode = GetBytecode(shaderFile);
      Init(logicalDevice, bytecode);
    }
    Shader(vk::Device logicalDevice, const std::vector<uint32_t> &bytecode)
    {
      Init(logicalDevice, bytecode);
    }
    static const std::vector<uint32_t> GetBytecode(std::string filename)
    {
      std::ifstream file(filename, std::ios::ate | std::ios::binary);

      if (!file.is_open())
        throw std::runtime_error("failed to open file + " + filename);

      size_t fileSize = (size_t)file.tellg();
      std::vector<uint32_t> bytecode(fileSize / sizeof(uint32_t));

      file.seekg(0);
      file.read((char*)bytecode.data(), bytecode.size() * sizeof(uint32_t));
      file.close();
      return bytecode;
    }

    legit::ShaderModule *GetModule()
    {
      return shaderModule.get();
    }


    size_t GetSetsCount()
    {
      return descriptorSetLayoutKeys.size();
    }
    const DescriptorSetLayoutKey *GetSetInfo(size_t setIndex)
    {
      return &descriptorSetLayoutKeys[setIndex];
    }
    glm::uvec3 GetLocalSize()
    {
      return localSize;
    }
  private:
    void Init(vk::Device logicalDevice, const std::vector<uint32_t> &bytecode)
    {
      shaderModule.reset(new ShaderModule(logicalDevice, bytecode));
      localSize = glm::uvec3(0);
      spirv_cross::Compiler compiler(bytecode.data(), bytecode.size());

      std::vector<spirv_cross::EntryPoint> entryPoints(compiler.get_entry_points_and_stages());
      assert(entryPoints.size() == 1);
      vk::ShaderStageFlags stageFlags;
      switch (entryPoints[0].execution_model)
      {
        case spv::ExecutionModel::ExecutionModelVertex:
        {
          stageFlags |= vk::ShaderStageFlagBits::eVertex;
        }break;
        case spv::ExecutionModel::ExecutionModelFragment:
        {
          stageFlags |= vk::ShaderStageFlagBits::eFragment;
        }break;
        case spv::ExecutionModel::ExecutionModelGLCompute:
        {
          stageFlags |= vk::ShaderStageFlagBits::eCompute;
          localSize.x = compiler.get_execution_mode_argument(spv::ExecutionMode::ExecutionModeLocalSize, 0);
          localSize.y = compiler.get_execution_mode_argument(spv::ExecutionMode::ExecutionModeLocalSize, 1);
          localSize.z = compiler.get_execution_mode_argument(spv::ExecutionMode::ExecutionModeLocalSize, 2);
        }break;
        default:{}
      }

      spirv_cross::ShaderResources resources = compiler.get_shader_resources();


      struct SetResources
      {
        std::vector<spirv_cross::Resource> uniformBuffers;
        std::vector<spirv_cross::Resource> imageSamplers;
        std::vector<spirv_cross::Resource> textures;
        std::vector<spirv_cross::Resource> samplers;
        std::vector<spirv_cross::Resource> storageBuffers;
        std::vector<spirv_cross::Resource> storageImages;
        std::vector<spirv_cross::Resource> accelerationStructures;
      };
      std::vector<SetResources> setResources;
      for (const auto &buffer : resources.uniform_buffers)
      {
        uint32_t setShaderId = compiler.get_decoration(buffer.id, spv::DecorationDescriptorSet);
        if (setShaderId >= setResources.size())
          setResources.resize(setShaderId + 1);
        setResources[setShaderId].uniformBuffers.push_back(buffer);
      }

      for (const auto &imageSampler : resources.sampled_images)
      {
        uint32_t setShaderId = compiler.get_decoration(imageSampler.id, spv::DecorationDescriptorSet);
        if (setShaderId >= setResources.size())
          setResources.resize(setShaderId + 1);
        setResources[setShaderId].imageSamplers.push_back(imageSampler);
      }
      
      for (const auto &texture : resources.separate_images)
      {
        uint32_t setShaderId = compiler.get_decoration(texture.id, spv::DecorationDescriptorSet);
        if (setShaderId >= setResources.size())
          setResources.resize(setShaderId + 1);
        setResources[setShaderId].textures.push_back(texture);
      }

      for (const auto &sampler : resources.separate_samplers)
      {
        uint32_t setShaderId = compiler.get_decoration(sampler.id, spv::DecorationDescriptorSet);
        if (setShaderId >= setResources.size())
          setResources.resize(setShaderId + 1);
        setResources[setShaderId].samplers.push_back(sampler);
      }
      
      for (const auto &buffer : resources.storage_buffers)
      {
        uint32_t setShaderId = compiler.get_decoration(buffer.id, spv::DecorationDescriptorSet);
        if (setShaderId >= setResources.size())
          setResources.resize(setShaderId + 1);
        setResources[setShaderId].storageBuffers.push_back(buffer);
      }

      for (const auto &image : resources.storage_images)
      {
        uint32_t setShaderId = compiler.get_decoration(image.id, spv::DecorationDescriptorSet);
        if (setShaderId >= setResources.size())
          setResources.resize(setShaderId + 1);
        setResources[setShaderId].storageImages.push_back(image);
      }

      for (const auto &accelerationStructure : resources.acceleration_structures)
      {
        uint32_t setShaderId = compiler.get_decoration(accelerationStructure.id, spv::DecorationDescriptorSet);
        if (setShaderId >= setResources.size())
          setResources.resize(setShaderId + 1);
        setResources[setShaderId].accelerationStructures.push_back(accelerationStructure);
      }

      this->descriptorSetLayoutKeys.resize(setResources.size());
      for (size_t setIndex = 0; setIndex < setResources.size(); setIndex++)
      {
        auto &descriptorSetLayoutKey = descriptorSetLayoutKeys[setIndex];
        descriptorSetLayoutKey = DescriptorSetLayoutKey();
        descriptorSetLayoutKey.setShaderId = uint32_t(setIndex);

        for (auto buffer : setResources[setIndex].uniformBuffers)
        {
          uint32_t shaderBindingIndex = compiler.get_decoration(buffer.id, spv::DecorationBinding);

          auto bufferType = compiler.get_type(buffer.type_id);

          if (bufferType.basetype == spirv_cross::SPIRType::BaseType::Struct)
          {
            auto uniformBufferId = DescriptorSetLayoutKey::UniformBufferId(descriptorSetLayoutKey.uniformBufferDatum.size());
            descriptorSetLayoutKey.uniformBufferDatum.emplace_back(DescriptorSetLayoutKey::UniformBufferData());
            auto &bufferData = descriptorSetLayoutKey.uniformBufferDatum.back();

            bufferData.shaderBindingIndex = shaderBindingIndex;
            //https://github.com/KhronosGroup/SPIRV-Cross/issues/753            
            if(buffer.name.find("ConstantBuffer.") != std::string::npos)
            {
              bufferData.name = compiler.get_name(buffer.id);
            }else
            {
              bufferData.name = buffer.name;
            }
            bufferData.stageFlags = stageFlags;

            uint32_t declaredSize = uint32_t(compiler.get_declared_struct_size(bufferType));

            uint32_t currOffset = 0;
            for (uint32_t memberIndex = 0; memberIndex < bufferType.member_types.size(); memberIndex++)
            {
              uint32_t memberSize = uint32_t(compiler.get_declared_struct_member_size(bufferType, memberIndex));
              //auto memberType = compiler.get_type(bufferType.member_types[memberIndex]);
              auto memberName = compiler.get_member_name(buffer.base_type_id, memberIndex);

              //memberData.size = compiler.get_declared_struct_size(memeberType);
              DescriptorSetLayoutKey::UniformId uniformId = DescriptorSetLayoutKey::UniformId(descriptorSetLayoutKey.uniformDatum.size());
              descriptorSetLayoutKey.uniformDatum.push_back(DescriptorSetLayoutKey::UniformData());
              DescriptorSetLayoutKey::UniformData &uniformData = descriptorSetLayoutKey.uniformDatum.back();

              uniformData.uniformBufferId = uniformBufferId;
              uniformData.offsetInBinding = uint32_t(currOffset);
              uniformData.size = uint32_t(memberSize);
              uniformData.name = memberName;

              //memberData.
              bufferData.uniformIds.push_back(uniformId);

              currOffset += memberSize;
            }
            assert(currOffset == declaredSize); //alignment is wrong. avoid using smaller types before larger ones. completely avoid vec2/vec3
            bufferData.size = declaredSize;
            bufferData.offsetInSet = descriptorSetLayoutKey.size;

            descriptorSetLayoutKey.size += bufferData.size;
          }
        }

        for (auto imageSampler : setResources[setIndex].imageSamplers)
        {
          auto imageSamplerId = DescriptorSetLayoutKey::ImageSamplerId(descriptorSetLayoutKey.imageSamplerDatum.size());

          uint32_t shaderBindingIndex = compiler.get_decoration(imageSampler.id, spv::DecorationBinding);
          descriptorSetLayoutKey.imageSamplerDatum.push_back(DescriptorSetLayoutKey::ImageSamplerData());
          auto &imageSamplerData = descriptorSetLayoutKey.imageSamplerDatum.back();
          imageSamplerData.shaderBindingIndex = shaderBindingIndex;
          imageSamplerData.stageFlags = stageFlags;
          imageSamplerData.name = imageSampler.name;
        }
        
        for (auto texture : setResources[setIndex].textures)
        {
          auto textureId = DescriptorSetLayoutKey::TextureId(descriptorSetLayoutKey.textureDatum.size());

          uint32_t shaderBindingIndex = compiler.get_decoration(texture.id, spv::DecorationBinding);
          descriptorSetLayoutKey.textureDatum.push_back(DescriptorSetLayoutKey::TextureData());
          auto &textureData = descriptorSetLayoutKey.textureDatum.back();
          textureData.shaderBindingIndex = shaderBindingIndex;
          textureData.stageFlags = stageFlags;
          textureData.name = texture.name;
        }
        
        for (auto sampler : setResources[setIndex].samplers)
        {
          auto samplerId = DescriptorSetLayoutKey::SamplerId(descriptorSetLayoutKey.samplerDatum.size());

          uint32_t shaderBindingIndex = compiler.get_decoration(sampler.id, spv::DecorationBinding);
          descriptorSetLayoutKey.samplerDatum.push_back(DescriptorSetLayoutKey::SamplerData());
          auto &samplerData = descriptorSetLayoutKey.samplerDatum.back();
          samplerData.shaderBindingIndex = shaderBindingIndex;
          samplerData.stageFlags = stageFlags;
          samplerData.name = sampler.name;
        }

        for (auto buffer : setResources[setIndex].storageBuffers)
        {
          uint32_t shaderBindingIndex = compiler.get_decoration(buffer.id, spv::DecorationBinding);

          auto bufferType = compiler.get_type(buffer.type_id);

          if (bufferType.basetype == spirv_cross::SPIRType::BaseType::Struct)
          {
            auto storageBufferId = DescriptorSetLayoutKey::StorageBufferId(descriptorSetLayoutKey.storageBufferDatum.size());
            if(buffer.name.find("counter.var.") != std::string::npos) continue;
            descriptorSetLayoutKey.storageBufferDatum.emplace_back(DescriptorSetLayoutKey::StorageBufferData());
            auto &bufferData = descriptorSetLayoutKey.storageBufferDatum.back();
            bufferData.shaderBindingIndex = shaderBindingIndex;
            bufferData.stageFlags = stageFlags;
            bufferData.name = buffer.name;
            assert(bufferType.array.size() == 1 || bufferType.array.size() == 0);
            bufferData.count = bufferType.array.size() == 1 ? bufferType.array[0] : 1u;
            bufferData.podPartSize = 0;
            bufferData.arrayMemberSize = 0;
            bufferData.offsetInSet = 0; //should not be used
            size_t declaredSize = compiler.get_declared_struct_size(bufferType);

            uint32_t currOffset = 0;
            bufferData.podPartSize = uint32_t(compiler.get_declared_struct_size(bufferType));

            //taken from get_declared_struct_size_runtime_array implementation
            auto& lastType = compiler.get_type(bufferType.member_types.back());
            if (!lastType.array.empty() && lastType.array_size_literal[0] && lastType.array[0] == 0) // Runtime array
              bufferData.arrayMemberSize = compiler.type_struct_member_array_stride(bufferType, uint32_t(bufferType.member_types.size() - 1));
            else
              bufferData.arrayMemberSize = 0;
          }
        }

        for (auto image : setResources[setIndex].storageImages)
        {
          auto imageSamplerId = DescriptorSetLayoutKey::ImageSamplerId(descriptorSetLayoutKey.imageSamplerDatum.size());

          uint32_t shaderBindingIndex = compiler.get_decoration(image.id, spv::DecorationBinding);
          descriptorSetLayoutKey.storageImageDatum.push_back(DescriptorSetLayoutKey::StorageImageData());
          auto &storageImageData = descriptorSetLayoutKey.storageImageDatum.back();
          storageImageData.shaderBindingIndex = shaderBindingIndex;
          storageImageData.stageFlags = stageFlags;
          storageImageData.name = image.name;
          //type?
        }
        
        for (auto accelerationStructure : setResources[setIndex].accelerationStructures)
        {
          auto accelerationStructureId = DescriptorSetLayoutKey::AccelerationStructureId(descriptorSetLayoutKey.accelerationStructureDatum.size());

          uint32_t shaderBindingIndex = compiler.get_decoration(accelerationStructure.id, spv::DecorationBinding);
          descriptorSetLayoutKey.accelerationStructureDatum.push_back(DescriptorSetLayoutKey::AccelerationStructureData());
          auto &accelerationStructureData = descriptorSetLayoutKey.accelerationStructureDatum.back();
          accelerationStructureData.shaderBindingIndex = shaderBindingIndex;
          accelerationStructureData.stageFlags = stageFlags;
          accelerationStructureData.name = accelerationStructure.name;
        }
        
        descriptorSetLayoutKey.RebuildIndex();
      }
    }


    std::vector<DescriptorSetLayoutKey> descriptorSetLayoutKeys;

    std::unique_ptr<legit::ShaderModule> shaderModule;
    glm::uvec3 localSize;
  };

  class ShaderProgram
  {
  public:
    ShaderProgram(Shader *_vertexShader, Shader *_fragmentShader)
      : vertexShader(_vertexShader), fragmentShader(_fragmentShader)
    {
      combinedDescriptorSetLayoutKeys.resize(std::max(vertexShader->GetSetsCount(), fragmentShader->GetSetsCount()));
      for (size_t setIndex = 0; setIndex < combinedDescriptorSetLayoutKeys.size(); setIndex++)
      {

        vk::DescriptorSetLayout setLayoutHandle = nullptr;
        std::vector<legit::DescriptorSetLayoutKey> setLayoutStageKeys;

        if (setIndex < vertexShader->GetSetsCount())
        {
          auto vertexSetInfo = vertexShader->GetSetInfo(setIndex);
          if (!vertexSetInfo->IsEmpty())
          {
            setLayoutStageKeys.push_back(*vertexSetInfo);
          }
        }
        if (setIndex < fragmentShader->GetSetsCount())
        {
          auto fragmentSetInfo = fragmentShader->GetSetInfo(setIndex);
          if (!fragmentSetInfo->IsEmpty())
          {
            setLayoutStageKeys.push_back(*fragmentSetInfo);
          }
        }
        this->combinedDescriptorSetLayoutKeys[setIndex] = DescriptorSetLayoutKey::Merge(setLayoutStageKeys.data(), setLayoutStageKeys.size());
      }
    }

    size_t GetSetsCount()
    {
      return combinedDescriptorSetLayoutKeys.size();
    }
    const DescriptorSetLayoutKey *GetSetInfo(size_t setIndex) const
    {
      return &combinedDescriptorSetLayoutKeys[setIndex];
    }

    std::vector<DescriptorSetLayoutKey> combinedDescriptorSetLayoutKeys;
    Shader *vertexShader;
    Shader *fragmentShader;
  };
}