#pragma once

// libs
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

// std
#include <array>
#include <cassert>
#include <functional>
#include <stdexcept>

namespace lve {

class GravityPhysicsSystem {
 private:
  void stepSimulation(std::vector<std::reference_wrapper<LveGameObject>>& physicsObjs, float dt) {
    // Loops through all pairs of objects and applies attractive force between them
    for (auto iterA = physicsObjs.begin(); iterA != physicsObjs.end(); ++iterA) {
      auto& objA = iterA->get();
      for (auto iterB = iterA; iterB != physicsObjs.end(); ++iterB) {
        if (iterA == iterB) continue;
        auto& objB = iterB->get();

        auto force = computeForce(objA, objB);
        objA.rigidBody.velocity2d += dt * -force / objA.rigidBody.mass;
        objB.rigidBody.velocity2d += dt * force / objB.rigidBody.mass;
      }
    }

    // update each objects position based on its final velocity
    for (auto& objRef : physicsObjs) {
      auto& obj = objRef.get();
      obj.transform.translation += dt * obj.rigidBody.velocity;
    }
  }

 public:
  GravityPhysicsSystem(float strength) : strengthGravity{strength} {}

  const float strengthGravity;

  // dt stands for delta time, and specifies the amount of time to advance the simulation
  // substeps is how many intervals to divide the forward time step in. More substeps result in a
  // more stable simulation, but takes longer to compute
  void update(std::vector<std::reference_wrapper<LveGameObject>>& objs, float dt, unsigned int substeps = 1) {
    const float stepDelta = dt / substeps;
    for (unsigned int i = 0; i < substeps; i++) {
      stepSimulation(objs, stepDelta);
    }
  }

  glm::vec2 computeForce(LveGameObject& fromObj, LveGameObject& toObj) const {
    auto offset = fromObj.transform.translation - toObj.transform.translation;
    float distanceSquared = glm::dot(offset, offset);

    // return 0 if objects are too clost together...
    if (glm::abs(distanceSquared) < 1e-10f) {
      return {.0f, .0f};
    }

    float force =
        strengthGravity * toObj.rigidBody.mass * fromObj.rigidBody.mass / distanceSquared;
    return force * offset / glm::sqrt(distanceSquared);
  }
};
}  // namespace lve