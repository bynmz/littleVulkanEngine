#include "simple_render_system.hpp"

namespace lve {

struct SimplePushConstantData {
  glm::mat4 modelMatrix{1.f};
  glm::mat4 normalMatrix{1.f};
};

struct PushConstantData {
  glm::vec2 offset;
  alignas(16) glm::vec3 color;
  glm::mat2 transform{1.f};
};

SimpleRenderSystem::SimpleRenderSystem(
    LveDevice& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout)
    : lveDevice{device} {
  createPipelineLayout(globalSetLayout);
  createPipeline(renderPass);
}

SimpleRenderSystem::~SimpleRenderSystem() {
  vkDestroyPipelineLayout(lveDevice.device(), pipelineLayout, nullptr);
}

void SimpleRenderSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout) {
  VkPushConstantRange pushConstantRange{};
  pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  pushConstantRange.offset = 0;
  pushConstantRange.size = sizeof(SimplePushConstantData);

  renderSystemLayout =
      LveDescriptorSetLayout::Builder(lveDevice)
          .addBinding(
              0, 
              VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 
              VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
          /*.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)*/
          .build();

  std::vector<VkDescriptorSetLayout> descriptorSetLayouts{
      globalSetLayout,
      renderSystemLayout->getDescriptorSetLayout()
  };

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
  pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
  pipelineLayoutInfo.pushConstantRangeCount = 1;
  pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
  if (vkCreatePipelineLayout(lveDevice.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create pipeline layout!");
  }
}

void SimpleRenderSystem::createPipeline(VkRenderPass renderPass) {
  assert(pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");

  PipelineConfigInfo pipelineConfig{};
  LvePipeline::defaultPipelineConfigInfo(pipelineConfig);
  pipelineConfig.renderPass = renderPass;
  pipelineConfig.pipelineLayout = pipelineLayout;
  lvePipeline = std::make_unique<LvePipeline>(
      lveDevice,
      "shaders/simple_shader.vert.spv",
      "shaders/simple_shader.frag.spv",
      pipelineConfig);
}

void SimpleRenderSystem::renderGameObjects(FrameInfo& frameInfo) {
  lvePipeline->bind(frameInfo.commandBuffer);

  vkCmdBindDescriptorSets(
      frameInfo.commandBuffer,
      VK_PIPELINE_BIND_POINT_GRAPHICS,
      pipelineLayout,
      0,
      1,
      &frameInfo.globalDescriptorSet,
      0,
      nullptr);

  // Create a map to cache descriptor sets for each game object
  std::unordered_map<LveGameObject*, VkDescriptorSet> cachedDescriptorSets;

  for (auto& kv : frameInfo.gameObjects) {
    auto& obj = kv.second;
    if (obj.model == nullptr) continue;

    // Check if the descriptor set for this object is already cached
    auto it = cachedDescriptorSets.find(&obj);
    if (it != cachedDescriptorSets.end()) {
      // If cached, bind the cached descriptor set
      vkCmdBindDescriptorSets(
          frameInfo.commandBuffer,
          VK_PIPELINE_BIND_POINT_GRAPHICS,
          pipelineLayout,
          1,
          1,
          &it->second,
          0,
          nullptr);
    } else {
      // If not cached, create a new descriptor set and cache it
      auto bufferInfo = obj.getBufferInfo(frameInfo.frameIndex);
      
      VkDescriptorSet gameObjectDescriptorSet;
      LveDescriptorWriter(*renderSystemLayout, frameInfo.frameDescriptorPool)
          .writeBuffer(0, &bufferInfo)
          /*.writeImage(1, &diffuseMapInfo)*/
          .build(gameObjectDescriptorSet);

      cachedDescriptorSets[&obj] = gameObjectDescriptorSet;

      // Bind the newly created descriptor set
      vkCmdBindDescriptorSets(
          frameInfo.commandBuffer,
          VK_PIPELINE_BIND_POINT_GRAPHICS,
          pipelineLayout,
          1,
          1,
          &gameObjectDescriptorSet,
          0,
          nullptr);
    }
    
    SimplePushConstantData push{};
    push.modelMatrix = obj.transform.mat4();
    push.normalMatrix = obj.transform.normalMatrix();

    vkCmdPushConstants(
        frameInfo.commandBuffer,
        pipelineLayout,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(SimplePushConstantData),
        &push);
    obj.model->bind(frameInfo.commandBuffer);
    obj.model->draw(frameInfo.commandBuffer);
  }
}

}  // namespace lve
