#pragma once
#include <entt/entt.hpp>
#include <SFML/System/Vector2.hpp>
#include <cmath>
#include <vector>
#include <algorithm>
#include "../../components/unit_components.hpp"
#include "../proximity_grid.hpp"

#include "../unit_combat_system.hpp"

class RosarySystem {
public:
    static void update(entt::registry& registry, float deltaTime, ProximityGrid& enemyGrid) {
        // 1. Manage Rosary Skill Instances
        auto rosaryView = registry.view<component::RosarySkill>();
        static std::vector<entt::entity> expiredInstances;
        expiredInstances.clear();

        rosaryView.each([&](auto instanceEntity, auto& rosary) {
            if (!registry.valid(rosary.owner)) {
                expiredInstances.push_back(instanceEntity);
                return;
            }

            auto& statueTrans = registry.get<component::Transform>(rosary.owner);
            if (!rosary.initialized) {
                initializeRosary(registry, instanceEntity, rosary, statueTrans);
            } else {
                rosary.remainingTime -= deltaTime;
                if (rosary.remainingTime <= 0.f) expiredInstances.push_back(instanceEntity);
            }
        });

        if (!expiredInstances.empty()) {
            cleanupExpiredRosaries(registry, expiredInstances);
            registry.destroy(expiredInstances.begin(), expiredInstances.end());
        }

        // 2. Update Orbital Spheres
        updateOrbitalSpheres(registry, deltaTime, enemyGrid);
    }

private:
    static void initializeRosary(entt::registry& registry, entt::entity instanceEntity, component::RosarySkill& rosary, const component::Transform& statueTrans) {
        for (int i = 0; i < 16; ++i) {
            float targetOffset = (2.f * 3.14159f / 16.f) * i;
            
            // Create Bead Core
            createOrbital(registry, instanceEntity, rosary, statueTrans, targetOffset, component::TextureID::RosarySphere, {1.1f, 1.1f}, true);
            // Create Holy Glow
            createOrbital(registry, instanceEntity, rosary, statueTrans, targetOffset, component::TextureID::RosaryGlow, {1.2f, 1.2f}, false);
        }
        rosary.initialized = true;
        rosary.remainingTime = rosary.duration;
    }

    static void createOrbital(entt::registry& registry, entt::entity instance, const component::RosarySkill& rosary, const component::Transform& statueTrans, float offset, component::TextureID tex, sf::Vector2f scale, bool hasLogic) {
        auto sphere = registry.create();
        registry.emplace<component::Transform>(sphere, statueTrans.position);
        
        auto& orbital = registry.emplace<component::OrbitalSphere>(sphere);
        orbital.parentInstance = instance;
        orbital.ownerStatue = rosary.owner; 
        orbital.state = component::OrbitalSphere::State::Expanding;
        orbital.targetSpreadOffset = offset;
        orbital.radius = rosary.radius;
        orbital.rotationSpeed = rosary.rotationSpeed;
        
        if (hasLogic) {
            orbital.knockbackForce = rosary.knockbackForce;
            orbital.damage = rosary.damage;
        } else {
            orbital.knockbackForce = 0.f;
            orbital.damage = 0.f;
        }
        
        auto& sd = registry.emplace<component::SpriteData>(sphere);
        sd.textureID = tex;
        sd.scale = scale;
        sd.renderLayer = 1;
    }

    static void updateOrbitalSpheres(entt::registry& registry, float deltaTime, ProximityGrid& enemyGrid) {
        auto view = registry.view<component::OrbitalSphere, component::Transform>();
        static std::vector<entt::entity> toDestroy;
        toDestroy.clear();

        view.each([&](auto entity, auto& orbital, auto& trans) {
            if (!registry.valid(orbital.ownerStatue)) {
                toDestroy.push_back(entity);
                return;
            }

            if (orbital.parentInstance != entt::null && registry.valid(orbital.parentInstance)) {
                if (auto* parentSkill = registry.try_get<component::RosarySkill>(orbital.parentInstance)) {
                    // [USER-REQUESTED] Sync stats in real-time from parent skill (which is updated by StatuePassiveSystem)
                    orbital.damage = parentSkill->damage;
                    orbital.knockbackForce = parentSkill->knockbackForce;
                    orbital.radius = parentSkill->radius;
                    orbital.rotationSpeed = parentSkill->rotationSpeed;
                }
            }

            if (orbital.parentInstance != entt::null && !registry.valid(orbital.parentInstance)) {
                orbital.state = component::OrbitalSphere::State::Shrinking;
                orbital.parentInstance = entt::null;
            }

            switch (orbital.state) {
                case component::OrbitalSphere::State::Expanding:
                    orbital.expansionProgress = std::min(1.0f, orbital.expansionProgress + orbital.expansionSpeed * deltaTime);
                    if (orbital.expansionProgress >= 1.0f) orbital.state = component::OrbitalSphere::State::Active;
                    break;
                case component::OrbitalSphere::State::Shrinking:
                    orbital.expansionProgress -= orbital.shrinkSpeed * deltaTime;
                    if (orbital.expansionProgress <= 0.0f) {
                        toDestroy.push_back(entity);
                        return;
                    }
                    break;
                default: break;
            }

            float easeT = (orbital.state == component::OrbitalSphere::State::Shrinking) 
                        ? (orbital.expansionProgress * orbital.expansionProgress) 
                        : (orbital.expansionProgress * orbital.expansionProgress * orbital.expansionProgress);
            
            float currentRadius = easeT * orbital.radius;
            orbital.orbitAngle += orbital.rotationSpeed * deltaTime;
            float finalAngle = orbital.orbitAngle + orbital.targetSpreadOffset;

            auto& parentTrans = registry.get<component::Transform>(orbital.ownerStatue);
            trans.position.x = parentTrans.position.x + std::cos(finalAngle) * currentRadius;
            trans.position.y = parentTrans.position.y + std::sin(finalAngle) * currentRadius;

            if (auto* sd = registry.try_get<component::SpriteData>(entity)) {
                float targetScale = 0.1f + (orbital.expansionProgress * 0.4f);
                sd->scale = {targetScale, targetScale};
            }

            if (currentRadius > 5.0f && (orbital.damage > 0.f || orbital.knockbackForce > 0.f)) {
                handleSphereCollision(registry, trans, parentTrans, currentRadius, orbital.damage, orbital.knockbackForce, deltaTime, enemyGrid);
            }
        });

        if (!toDestroy.empty()) registry.destroy(toDestroy.begin(), toDestroy.end());
    }

    static void handleSphereCollision(entt::registry& registry, const component::Transform& trans, const component::Transform& parentTrans, float currentRadius, float damage, float knockback, float dt, ProximityGrid& enemyGrid) {
        const float beadVisualRadius = 15.0f; 
        enemyGrid.queryNearby(trans.position, [&](entt::entity enemy) {
            if (!registry.valid(enemy)) return;
            auto& et = registry.get<component::Transform>(enemy);
            
            sf::Vector2f diff = et.position - trans.position;
            float distSq = diff.x * diff.x + diff.y * diff.y;
            float totalRadius = beadVisualRadius + et.radius;

            if (distSq < totalRadius * totalRadius) {
                if (UnitCombatSystem::applyDamage(registry, enemy, damage, 0.2f)) {
                    sf::Vector2f pushDir = et.position - parentTrans.position;
                    float pushDistSq = pushDir.x * pushDir.x + pushDir.y * pushDir.y;
                    if (pushDistSq > 0.0001f) {
                        float pushDist = std::sqrt(pushDistSq);
                        sf::Vector2f normPushDir = pushDir / pushDist;
                        sf::Vector2f finalVel;
                        if (pushDist < currentRadius) {
                            float speed = (currentRadius - pushDist) * (1.0f / dt);
                            finalVel = normPushDir * std::max(knockback, speed);
                        } else {
                            finalVel = normPushDir * knockback;
                        }
                        registry.get<component::DamagedEvent>(enemy).addKnockback(finalVel);
                    }
                }
            }
        });
    }

    static void cleanupExpiredRosaries(entt::registry& registry, const std::vector<entt::entity>& expiredInstances) {
        auto sphereView = registry.view<component::OrbitalSphere>();
        for (auto entity : sphereView) {
            auto& orbital = sphereView.get<component::OrbitalSphere>(entity);
            if (orbital.parentInstance != entt::null) {
                bool found = std::any_of(expiredInstances.begin(), expiredInstances.end(), [&](auto inst){ return orbital.parentInstance == inst; });
                if (found) {
                    orbital.state = component::OrbitalSphere::State::Shrinking;
                    orbital.parentInstance = entt::null;
                }
            }
        }
    }
};
