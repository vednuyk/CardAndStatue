#pragma once
#include <entt/entt.hpp>
#include <SFML/System/Vector2.hpp>
#include <cmath>
#include <limits>
#include <random>
#include "../components/unit_components.hpp"
#include "entity_factory.hpp"
#include "proximity_grid.hpp"

class StatueSkillSystem {
public:
    static void update(entt::registry& registry, float deltaTime, ProximityGrid& enemyGrid) {
        // 1. Statue general stats update
        auto statueView = registry.view<component::StatueTag, component::Transform, component::StatueStats>();
        statueView.each([&](auto statueEntity, auto& transform, auto& stats) {
            stats.currentHealth = std::min(stats.maxHealth, stats.currentHealth + stats.hpRegen * deltaTime);

            if (auto* holy = registry.try_get<component::HolyAttackSkill>(statueEntity)) {
                updateHolyAttack(registry, statueEntity, *holy, transform, deltaTime, enemyGrid);
            }

            if (auto* spawn = registry.try_get<component::SpawnKnightSkill>(statueEntity)) {
                updateSpawnKnight(registry, statueEntity, *spawn, transform, deltaTime);
            }
        });

        // 2. Rosary Skill Instances Lifecycle (Independent timers)
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
                for (int i = 0; i < 16; ++i) {
                    auto sphere = registry.create();
                    registry.emplace<component::Transform>(sphere, statueTrans.position);
                    
                    float targetOffset = (2.f * 3.14159f / 16.f) * i;
                    auto& orbital = registry.emplace<component::OrbitalSphere>(sphere);
                    orbital.parentInstance = instanceEntity;
                    orbital.ownerStatue = rosary.owner; // Direct link to statue
                    orbital.state = component::OrbitalSphere::State::Expanding;
                    orbital.targetSpreadOffset = targetOffset;
                    orbital.radius = rosary.radius;
                    orbital.rotationSpeed = rosary.rotationSpeed;
                    orbital.knockbackForce = rosary.knockbackForce;
                    orbital.damage = rosary.damage;
                    
                    auto& sd = registry.emplace<component::SpriteData>(sphere);
                    sd.textureID = component::TextureID::RosarySphere;
                    sd.scale = {0.1f, 0.1f};
                }
                rosary.initialized = true;
                rosary.remainingTime = rosary.duration;
            } else {
                rosary.remainingTime -= deltaTime;
                if (rosary.remainingTime <= 0.f) {
                    expiredInstances.push_back(instanceEntity);
                }
            }
        });

        // Handle expired instances: notify their spheres and destroy the instance entity
        for (auto instance : expiredInstances) {
            auto sphereView = registry.view<component::OrbitalSphere>();
            for (auto entity : sphereView) {
                auto& orbital = sphereView.get<component::OrbitalSphere>(entity);
                if (orbital.parentInstance == instance) {
                    orbital.state = component::OrbitalSphere::State::Shrinking;
                    orbital.parentInstance = entt::null; // Instance is gone
                }
            }
            registry.destroy(instance);
        }

        // 3. Orbital Spheres independent logic
        updateOrbitalSpheres(registry, deltaTime, enemyGrid);
    }

    // Factory method for Rosary (Independent Instance)
    static void applyRosary(entt::registry& registry, entt::entity statue, const component::RosarySkill& config) {
        auto instance = registry.create();
        auto& rosary = registry.emplace<component::RosarySkill>(instance, config);
        rosary.owner = statue;
    }

private:
    static void updateOrbitalSpheres(entt::registry& registry, float deltaTime, ProximityGrid& enemyGrid) {
        auto view = registry.view<component::OrbitalSphere, component::Transform>();
        static std::vector<entt::entity> toDestroy;
        toDestroy.clear();

        view.each([&](auto entity, auto& orbital, auto& trans) {
            // Check if owner statue is still valid
            if (!registry.valid(orbital.ownerStatue)) {
                toDestroy.push_back(entity);
                return;
            }

            // If instance is gone but we aren't shrinking, start shrinking
            if (orbital.parentInstance != entt::null && !registry.valid(orbital.parentInstance)) {
                orbital.state = component::OrbitalSphere::State::Shrinking;
                orbital.parentInstance = entt::null;
            }

            // State management
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
                case component::OrbitalSphere::State::Active:
                    break;
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

            if (currentRadius > 5.0f) {
                handleSphereCollision(registry, entity, orbital, trans, parentTrans, currentRadius, deltaTime, enemyGrid);
            }
        });

        if (!toDestroy.empty()) registry.destroy(toDestroy.begin(), toDestroy.end());
    }

    static void handleSphereCollision(entt::registry& registry, entt::entity sphere, const component::OrbitalSphere& orbital, component::Transform& trans, const component::Transform& parentTrans, float currentRadius, float dt, ProximityGrid& enemyGrid) {
        float collisionRadius = std::max(15.0f, currentRadius * 0.15f + 10.0f);
        
        enemyGrid.queryNearby(trans.position, [&](entt::entity enemy) {
            if (!registry.valid(enemy)) return;
            auto& et = registry.get<component::Transform>(enemy);
            sf::Vector2f diff = et.position - trans.position;
            float distSq = diff.x * diff.x + diff.y * diff.y;

            if (distSq < collisionRadius * collisionRadius) {
                auto& es = registry.get<component::UnitStats>(enemy);
                es.currentHealth -= orbital.damage * dt;
                es.hitFlashTimer = 0.1f;

                sf::Vector2f pushDir = et.position - parentTrans.position;
                float pushDist = std::sqrt(pushDir.x * pushDir.x + pushDir.y * pushDir.y);
                if (pushDist > 0.001f) {
                    if (orbital.state == component::OrbitalSphere::State::Expanding && pushDist < currentRadius) {
                        et.position = parentTrans.position + (pushDir / pushDist) * currentRadius;
                    } else {
                        et.position += (pushDir / pushDist) * orbital.knockbackForce * dt;
                    }
                }
            }
        });
    }

    static void updateHolyAttack(entt::registry& registry, entt::entity statue, component::HolyAttackSkill& skill, const component::Transform& trans, float dt, ProximityGrid& enemyGrid) {
        skill.timer += dt;
        if (skill.timer >= skill.cooldown) {
            bool hitAny = false;
            float radiusSq = skill.radius * skill.radius;

            enemyGrid.queryNearby(trans.position, [&](entt::entity enemy) {
                if (!registry.valid(enemy)) return;
                auto& et = registry.get<component::Transform>(enemy);
                sf::Vector2f diff = et.position - trans.position;
                if (diff.x * diff.x + diff.y * diff.y <= radiusSq) {
                    auto& es = registry.get<component::UnitStats>(enemy);
                    es.currentHealth -= skill.damage;
                    hitAny = true;
                }
            });

            if (hitAny) skill.timer = 0.f;
        }
    }

    static void updateSpawnKnight(entt::registry& registry, entt::entity statue, component::SpawnKnightSkill& skill, const component::Transform& trans, float dt) {
        skill.timer += dt;
        if (skill.timer >= skill.cooldown) {
            for (int i = 0; i < skill.spawnCount; ++i) {
                float angle = (2.f * 3.14159f / skill.spawnCount) * i;
                sf::Vector2f spawnPos = trans.position + sf::Vector2f(std::cos(angle), std::sin(angle)) * 60.f;
                EntityFactory::createPlayerUnit(registry, spawnPos, "Knight");
            }
            skill.timer = 0.f;
        }
    }
};
