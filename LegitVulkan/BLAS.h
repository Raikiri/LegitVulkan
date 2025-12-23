namespace legit
{
  class BLAS
  {
  public:
    BLAS(vk::PhysicalDevice physicalDevice, vk::Device logicalDevice, const legit::ExecuteOnceQueue &queue, legit::Buffer *vertexBuffer, size_t maxVertex, size_t vertexStride, legit::Buffer *indexBuffer, uint32_t trianglesCount)
    {
      this->transformBuffer = CreateTransformBuffer(physicalDevice, logicalDevice, glm::translate(glm::vec3(0.0f, 0.0f, 0.0f)));

      auto accelerationStructureGeometry = vk::AccelerationStructureGeometryKHR()
        .setFlags(vk::GeometryFlagBitsKHR::eOpaque)
        .setGeometryType(vk::GeometryTypeKHR::eTriangles);
        
      accelerationStructureGeometry.geometry = vk::AccelerationStructureGeometryTrianglesDataKHR()
        .setVertexFormat(vk::Format::eR32G32B32Sfloat)
        .setVertexData(vertexBuffer->GetDeviceAddress())
        .setMaxVertex(maxVertex)
        .setVertexStride(vertexStride)
        .setIndexType(vk::IndexType::eUint32)
        .setIndexData(indexBuffer->GetDeviceAddress())
        .setTransformData(transformBuffer->GetDeviceAddress());
      
      auto buildGeomInfo = vk::AccelerationStructureBuildGeometryInfoKHR()
        .setType(vk::AccelerationStructureTypeKHR::eBottomLevel)
        .setFlags(vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace)
        .setMode(vk::BuildAccelerationStructureModeKHR::eBuild)
        .setGeometryCount(1)
        .setPGeometries(&accelerationStructureGeometry);

      std::vector<uint32_t> triangleCounts;
      triangleCounts.push_back(trianglesCount);
      
      auto buildSizesInfo = logicalDevice.getAccelerationStructureBuildSizesKHR(
        vk::AccelerationStructureBuildTypeKHR::eDevice,
        buildGeomInfo,
        triangleCounts);
        
      accelerationStructure.reset(new legit::AccelerationStructure(physicalDevice, logicalDevice, vk::AccelerationStructureTypeKHR::eBottomLevel, buildSizesInfo));

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
        .setPrimitiveCount(trianglesCount);

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
    std::unique_ptr<legit::Buffer> transformBuffer;
  };
}