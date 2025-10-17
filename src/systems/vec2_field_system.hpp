#pragma once

#include "lve_game_object.hpp"
#include "gravity_system.hpp"

// libs
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

// std
#include <functional>
#include <vector>

namespace lve {
class Vec2FieldSystem {
 public:
  void update(
      const GravityPhysicsSystem& physicsSystem,
      std::vector<std::reference_wrapper<LveGameObject>>& physicsObjs,
      std::vector<std::reference_wrapper<LveGameObject>>& vectorField) {
    // For each field line we calculate the net gravitational force for that point in space
    for (auto& vfRef : vectorField) {
      auto& vf = vfRef.get();
      glm::vec2 direction{};
      for (auto& objRef : physicsObjs) {
        auto& obj = objRef.get();
        direction += physicsSystem.computeForce(obj, vf);
      }

      // This scales the length of the field line based on the log of the length
      // values were chosen through trial and error based on what looks good
      // and then the field line is rotate to point in the direction of the field
      vf.transform.scale.x =
          0.005f + 0.045f * glm::clamp(glm::log(glm::length(direction) + 1) / 3.f, 0.f, 1.f);
      vf.transform.rotation2d = atan2(direction.y, direction.x);
    }
  }
};

}  // namespace lve
