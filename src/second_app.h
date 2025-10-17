#pragma once

#include "lve_descriptors.hpp"
#include "lve_device.hpp"
#include "lve_game_object.hpp"
#include "lve_renderer.hpp"
#include "lve_window.hpp"


#include "systems/simple_render_system.hpp"

#include "systems/gravity_system.hpp"
#include "systems/vec2_field_system.hpp"

// std
#include <functional>
#include <memory>
#include <vector>

namespace lve {
class SecondApp {
public:
	static constexpr int WIDTH = 800;
	static constexpr int HEIGHT = 600;
        
	SecondApp();
    ~SecondApp();

	SecondApp(const SecondApp&) = delete;
    SecondApp& operator=(const SecondApp&) = delete;

	void run();

private:
	void loadGameObjects();
	
	LveWindow lveWindow{WIDTH, HEIGHT, "Second Vulkan App"};
	LveDevice lveDevice{lveWindow};
	LveRenderer lveRenderer{lveWindow, lveDevice};

	std::unique_ptr<LveDescriptorPool> globalPool{};
    std::unique_ptr<LveDescriptorSetLayout> globalSetLayout{};
    std::vector<std::unique_ptr<LveDescriptorPool>> framePools{};

	LveGameObjectManager gameObjectManager{lveDevice};

	//LveGameObject::Map gameObjects;

	std::vector<std::reference_wrapper<LveGameObject>> physicsObjects{};
    // create vector field
    std::vector<std::reference_wrapper<LveGameObject>> vectorField{};

    GravityPhysicsSystem gravitySystem{0.81f};
    Vec2FieldSystem vecFieldSystem{};
};
}
