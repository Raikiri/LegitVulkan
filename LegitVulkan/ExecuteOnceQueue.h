namespace legit
{
  struct ExecuteOnceQueue
  {
    ExecuteOnceQueue(legit::Core *core)
    {
      this->core = core;
      commandBuffer = std::move(core->AllocateCommandBuffers(1)[0]);
    }

    vk::CommandBuffer BeginCommandBuffer() const
    {
      auto bufferBeginInfo = vk::CommandBufferBeginInfo()
        .setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
      commandBuffer->begin(bufferBeginInfo);
      return commandBuffer.get();
    }

    void EndCommandBuffer() const
    {
      commandBuffer->end();
      vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eAllCommands };

      auto submitInfo = vk::SubmitInfo()
        .setWaitSemaphoreCount(0)
        .setPWaitDstStageMask(waitStages)
        .setCommandBufferCount(1)
        .setPCommandBuffers(&commandBuffer.get())
        .setSignalSemaphoreCount(0);

      core->GetGraphicsQueue().submit({ submitInfo }, nullptr);
      core->GetGraphicsQueue().waitIdle();
    }
  private:
    legit::Core * core;
    vk::UniqueCommandBuffer commandBuffer;
  };
}