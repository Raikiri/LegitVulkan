namespace legit
{
  struct BLASInstance
  {
    legit::BLAS* blas;
    glm::mat4 transform;
  };
  std::unique_ptr<legit::Buffer> CreateInstanceBuffer(vk::PhysicalDevice physicalDevice, vk::Device logicalDevice, std::vector<BLASInstance> instances)
  {
    std::unique_ptr<legit::Buffer> instanceBuffer;
    instanceBuffer.reset(new legit::Buffer(
      physicalDevice,
      logicalDevice,
      sizeof(vk::AccelerationStructureInstanceKHR) * instances.size(),
      vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent));

    auto *instanceData = (vk::AccelerationStructureInstanceKHR*)instanceBuffer->Map();
    {
      for(size_t instanceIdx = 0; instanceIdx < instances.size(); instanceIdx++)
      {
        instanceData[instanceIdx] = vk::AccelerationStructureInstanceKHR()
          .setTransform(GlmMat4ToTransformMatrixKHR(instances[instanceIdx].transform))
          .setMask(0xFF)
          .setFlags(vk::GeometryInstanceFlagBitsKHR::eTriangleFacingCullDisable)
          .setAccelerationStructureReference(instances[instanceIdx].blas->GetAccelerationStructure()->GetDeviceAddress());
      }
    }
    instanceBuffer->Unmap();
    return instanceBuffer;
  }
  
  class TLAS
  {
  public:
    TLAS(vk::PhysicalDevice physicalDevice, vk::Device logicalDevice, const legit::ExecuteOnceQueue &queue, std::vector<BLASInstance> instances)
    {
      auto instanceBuffer = CreateInstanceBuffer(physicalDevice, logicalDevice, instances);

      auto accelerationStructureGeometry = vk::AccelerationStructureGeometryKHR()
        .setGeometryType(vk::GeometryTypeKHR::eInstances)
        .setFlags(vk::GeometryFlagBitsKHR::eOpaque);
      accelerationStructureGeometry.geometry = vk::AccelerationStructureGeometryInstancesDataKHR()
        .setArrayOfPointers(vk::False)
        .setData(instanceBuffer->GetDeviceAddress());
        

      auto buildGeomInfo = vk::AccelerationStructureBuildGeometryInfoKHR()
        .setType(vk::AccelerationStructureTypeKHR::eTopLevel)
        .setFlags(vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace)
        .setMode(vk::BuildAccelerationStructureModeKHR::eBuild)
        .setGeometryCount(1)
        .setPGeometries(&accelerationStructureGeometry);

      std::vector<uint32_t> primitiveCounts;
      primitiveCounts.push_back(instances.size());

      auto buildSizesInfo = logicalDevice.getAccelerationStructureBuildSizesKHR(
        vk::AccelerationStructureBuildTypeKHR::eDevice,
        buildGeomInfo,
        primitiveCounts);

      accelerationStructure.reset(new legit::AccelerationStructure(physicalDevice, logicalDevice, vk::AccelerationStructureTypeKHR::eTopLevel, buildSizesInfo));
      
      legit::Buffer scratchBuffer(
        physicalDevice,
        logicalDevice,
        buildSizesInfo.buildScratchSize,
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        vk::MemoryPropertyFlagBits::eDeviceLocal);

      buildGeomInfo
        .setScratchData(scratchBuffer.GetDeviceAddress())
        .setDstAccelerationStructure(accelerationStructure->GetHandle());

      auto buildRange = vk::AccelerationStructureBuildRangeInfoKHR()
        .setPrimitiveCount(instances.size());

      std::vector<vk::AccelerationStructureBuildRangeInfoKHR*> buildRanges;
      buildRanges.push_back(&buildRange);

      auto commandBuffer = queue.BeginCommandBuffer();
      {
        commandBuffer.buildAccelerationStructuresKHR(
          1,
          &buildGeomInfo,
          buildRanges.data());
      }
      queue.EndCommandBuffer();
    }
    legit::AccelerationStructure *GetAccelerationStructure()
    {
      return accelerationStructure.get();
    }
  private:
    std::unique_ptr<legit::AccelerationStructure> accelerationStructure;
    std::unique_ptr<legit::Buffer> instanceBuffer;
  };
}