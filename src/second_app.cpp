#include "second_app.h"

#include "keyboard_movement_controller.hpp"
#include "lve_buffer.hpp"
#include "lve_camera.hpp"

// libs
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

// std
#include <array>
#include <cassert>
#include <chrono>
#include <stdexcept>

namespace lve {
std::unique_ptr<LveModel> createBoxSprite(LveDevice& device, glm::vec3 offset, float depth = 0.1f) {
  std::vector<LveModel::Vertex> vertices = {
      // Front face
      {{-0.5f, -0.5f, depth / 2}},
      {{0.5f, -0.5f, depth / 2}},
      {{0.5f, 0.5f, depth / 2}},
      {{-0.5f, 0.5f, depth / 2}},
      // Back face
      {{-0.5f, -0.5f, -depth / 2}},
      {{0.5f, -0.5f, -depth / 2}},
      {{0.5f, 0.5f, -depth / 2}},
      {{-0.5f, 0.5f, -depth / 2}},
  };
  for (auto& v : vertices) v.position += offset;

  std::vector<uint32_t> indices = {
      // Front
      0,1,2,0,2,3,
      // Back
      4,5,6,4,6,7,
      // Left
      4,0,3,4,3,7,
      // Right
      1,5,6,1,6,2,
      // Top
      3,2,6,3,6,7,
      // Bottom
      4,5,1,4,1,0
  };

  LveModel::Builder builder{};
  builder.vertices = vertices;
  builder.indices = indices;
  return std::make_unique<LveModel>(device, builder);
}

std::unique_ptr<LveModel> createSquareSprite(LveDevice& device, glm::vec3 offset) {
  std::vector<LveModel::Vertex> vertices = {
      {{-0.5f, -0.5f, 0.f}},
      {{0.5f, -0.5f, 0.f}},
      {{0.5f, 0.5f, 0.f}},
      {{-0.5f, 0.5f, 0.f}},
  };
  for (auto& v : vertices) {
    v.position += offset;
  }
  LveModel::Builder spriteBuilder{};
  spriteBuilder.vertices = vertices;
  spriteBuilder.indices.push_back(0);
  spriteBuilder.indices.push_back(1);
  spriteBuilder.indices.push_back(2);
  spriteBuilder.indices.push_back(0);
  spriteBuilder.indices.push_back(2);
  spriteBuilder.indices.push_back(3);
  return std::make_unique<LveModel>(device, spriteBuilder);
}
std::unique_ptr<LveModel> createCircleSprite(LveDevice& device, unsigned int numSides) {
  std::vector<LveModel::Vertex> uniqueVertices{};
  for (unsigned int i = 0; i < numSides; i++) {
    float angle = i * glm::two_pi<float>() / numSides;
    uniqueVertices.push_back({{glm::cos(angle), glm::sin(angle), 0}});
  }
  uniqueVertices.push_back({});  // adds center vertex at 0, 0

  std::vector<LveModel::Vertex> vertices{};
  std::vector<uint32_t> indices{};
  for (unsigned int i = 0; i < numSides; i++) {
    vertices.push_back(uniqueVertices[i]);
    vertices.push_back(uniqueVertices[(i + 1) % numSides]);
    vertices.push_back(uniqueVertices[numSides]);

    indices.push_back(i * 3);
    indices.push_back(i * 3 + 1);
    indices.push_back(i * 3 + 2);
  }

  LveModel::Builder spriteBuilder{};
  spriteBuilder.vertices = vertices;
  spriteBuilder.indices = indices;

  return std::make_unique<LveModel>(device, spriteBuilder);
}

SecondApp::SecondApp() { 
	globalPool = 
		LveDescriptorPool::Builder(lveDevice)
		.setMaxSets(LveSwapChain::MAX_FRAMES_IN_FLIGHT)
		.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, LveSwapChain::MAX_FRAMES_IN_FLIGHT)
          .build();
	loadGameObjects(); 
}

SecondApp::~SecondApp() {}

void SecondApp::run() {
  std::vector<std::unique_ptr<LveBuffer>> uboBuffers(LveSwapChain::MAX_FRAMES_IN_FLIGHT);
  for (size_t i = 0; i < uboBuffers.size(); i++) {
    uboBuffers[i] = std::make_unique<LveBuffer>(
		lveDevice, 
		sizeof(GlobalUbo),
		1, 
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	uboBuffers[i]->map();
  }

  auto globalSetLayout = 
	  LveDescriptorSetLayout::Builder(lveDevice)
		 .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
         .build();

  std::vector<VkDescriptorSet> globalDescriptorSets(LveSwapChain::MAX_FRAMES_IN_FLIGHT);
  for (size_t i = 0; i < globalDescriptorSets.size(); i++) {
	auto bufferInfo = uboBuffers[i]->descriptorInfo();
	LveDescriptorWriter(*globalSetLayout, *globalPool)
		.writeBuffer(0, &bufferInfo)
		.build(globalDescriptorSets[i]);
  }

  // Create per-frame descriptor pools for dynamic allocations
  framePools.resize(LveSwapChain::MAX_FRAMES_IN_FLIGHT);
  for (size_t i = 0; i < framePools.size(); i++) {
    framePools[i] = LveDescriptorPool::Builder(lveDevice)
        .setMaxSets(1000)
        .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000)
        .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000)
        .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000)
        .setPoolFlags(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT)
        .build();
  }

  SimpleRenderSystem simpleRenderSystem{
      lveDevice,
      lveRenderer.getSwapChainRenderPass(),
      globalSetLayout->getDescriptorSetLayout()};
  LveCamera camera{};

  auto& viewerObject = gameObjectManager.createGameObject();
  viewerObject.transform.translation = {0.f, 0.f, -2.5f};
  KeyboardMovementController cameraController{};

  auto currentTime = std::chrono::high_resolution_clock::now();
  while (!lveWindow.shouldClose()) {
    glfwPollEvents();

	auto newTime = std::chrono::high_resolution_clock::now();
	float frameTime =
		std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
	currentTime = newTime;
	cameraController.moveInPlaneXZ(lveWindow.getGLFWwindow(), frameTime, viewerObject);
	camera.setViewYXZ(viewerObject.transform.translation, viewerObject.transform.rotation);
	float aspect = lveRenderer.getAspectRatio();
	camera.setPerspectiveProjection(glm::radians(50.f), aspect, 0.1f, 100.f);
	if (auto commandBuffer = lveRenderer.beginFrame()) {
	  int frameIndex = lveRenderer.getFrameIndex();
      framePools[frameIndex]->resetPool();
	  FrameInfo frameInfo{
		  frameIndex,
		  frameTime,
		  commandBuffer,
		  camera,
		  globalDescriptorSets[frameIndex],
          *framePools[frameIndex],
          gameObjectManager.gameObjects};

		// Update
		GlobalUbo ubo{};
		ubo.projection = camera.getProjection();
		ubo.view = camera.getView();
        ubo.inverseView = camera.getInverseView();
        uboBuffers[frameIndex]->writeToBuffer(&ubo);
        uboBuffers[frameIndex]->flush();

        gravitySystem.update(physicsObjects, 1.f / 60, 5);
        vecFieldSystem.update(gravitySystem, physicsObjects, vectorField);

		// render
        lveRenderer.beginSwapChainRenderPass(commandBuffer);

		// order here matters
        simpleRenderSystem.renderGameObjects(frameInfo);
		lveRenderer.endSwapChainRenderPass(commandBuffer);
		lveRenderer.endFrame();
        }
	  }
  vkDeviceWaitIdle(lveDevice.device());  
  }

void SecondApp::loadGameObjects() {
    std::shared_ptr<LveModel> lveModel =
        LveModel::createModelFromFile(lveDevice, "models/quad.obj");
    auto& floor = gameObjectManager.createGameObject();
    floor.model = lveModel;
    floor.transform.translation = {0.f, .5f, 0.f};
    floor.transform.scale = {3.f, 1.f, 3.f};

	
    // Create some sprites
    std::shared_ptr<LveModel> square = createBoxSprite(
        lveDevice,
        {0.5f,
         1.f,
         0.f});  // offset sprite by .5 so rotation occurs at edge rather than center of square

    std::shared_ptr<LveModel> circle = createCircleSprite(lveDevice, 22);

    // create physics objects
    auto& blue = gameObjectManager.createGameObject();
    blue.transform.scale = {.05f, .05f, 0.f};
    blue.transform.translation = {.5f, .5f, 0.f};
    blue.color = {0.f, 0.f, 1.f};
    blue.rigidBody.velocity = {-.5f, .0f, 0.f};
    blue.model = circle;
    //physicsObjects.push_back(std::ref(blue));

    auto& red = gameObjectManager.createGameObject();
    red.transform.scale = {.05f, .05f, 0.f};
    red.transform.translation = {-.45f, -.25f, 0.f};
    red.color = {1.f, 0.f, 0.f};
    red.rigidBody.velocity = {.5f, .0f, 0.f};
    red.model = circle;
    //physicsObjects.push_back(std::ref(red));

    // create vector field
    const int gridCount = 30;
    for (int i = 0; i < gridCount; i++) {
      for (int j = 0; j < gridCount; j++) {
        auto& vf = gameObjectManager.createGameObject();
        vf.transform.scale = {0.02f, 0.02f, 2.f};
        vf.transform.translation = {
            -1.0f + (i + 0.5f) * 2.0f / gridCount,
            .45f,
            -1.0f + (j + 0.5f) * 2.0f / gridCount
        };
        vf.transform.rotation = {1.5f, 0.f, 0.f};
        vf.color = {
          1.f,
          1.f,
          1.f,
        };
        vf.model = square;
        vectorField.push_back(std::ref(vf));
      }
    }

}

}