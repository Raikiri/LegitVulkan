#include <optional>
namespace legit
{
  class AccelerationStructure
  {
  public:
    AccelerationStructure(vk::PhysicalDevice physicalDevice, vk::Device logicalDevice, vk::AccelerationStructureTypeKHR type, vk::AccelerationStructureBuildSizesInfoKHR buildSizeInfo)
    {
      this->buffer.reset(new legit::Buffer(
        physicalDevice,
        logicalDevice,
        buildSizeInfo.accelerationStructureSize,
        vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        vk::MemoryPropertyFlagBits::eDeviceLocal));
      auto accelerationStructureCreateInfo = vk::AccelerationStructureCreateInfoKHR()
        .setBuffer(this->buffer->GetHandle())
        .setType(type)
        .setSize(buildSizeInfo.accelerationStructureSize);
      this->accelerationStructure = logicalDevice.createAccelerationStructureKHRUnique(accelerationStructureCreateInfo);
      auto deviceAddressInfo = vk::AccelerationStructureDeviceAddressInfoKHR()
        .setAccelerationStructure(this->accelerationStructure.get());
      this->deviceAddress = logicalDevice.getAccelerationStructureAddressKHR(deviceAddressInfo);
    }
    vk::AccelerationStructureKHR GetHandle() const
    {
      return accelerationStructure.get();
    }
    uint64_t GetDeviceAddress()
    {
      return deviceAddress;
    }
  private:
    vk::UniqueAccelerationStructureKHR accelerationStructure;
    uint64_t deviceAddress = 0;
    std::unique_ptr<legit::Buffer> buffer;
  };
  
  vk::TransformMatrixKHR GlmMat4ToTransformMatrixKHR(glm::mat4 transform)
  {
    vk::TransformMatrixKHR resTransform;
    for(uint32_t col = 0; col < 3; col++)
    {
      for(uint32_t row = 0; row < 4; row++)
      {
        resTransform.matrix[col][row] = transform[row][col];
      }
    }
    return resTransform;
  }
  std::unique_ptr<legit::Buffer> CreateTransformBuffer(vk::PhysicalDevice physicalDevice, vk::Device logicalDevice, glm::mat4 transform)
  {
    std::unique_ptr<legit::Buffer> transformBuffer;
    transformBuffer.reset(new legit::Buffer(
      physicalDevice,
      logicalDevice,
      sizeof(vk::TransformMatrixKHR),
      vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent));

    auto *transformData = (vk::TransformMatrixKHR*)transformBuffer->Map();
    {
      *transformData = GlmMat4ToTransformMatrixKHR(transform);
    }
    transformBuffer->Unmap();
    return transformBuffer;
  }
}