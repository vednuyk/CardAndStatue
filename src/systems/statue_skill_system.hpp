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

        // 2. God Ray Instances Update (Enables Stacking and Duration)
        auto godRayView = registry.view<component::GodRaySkill>();
        static std::vector<entt::entity> expiredGodRays;
        expiredGodRays.clear();

        godRayView.each([&](auto entity, auto& skill) {
            if (registry.valid(skill.owner)) {
                skill.remainingTime -= deltaTime;
                if (skill.remainingTime <= 0.f) {
                    expiredGodRays.push_back(entity);
                } else {
                    auto& statueTrans = registry.get<component::Transform>(skill.owner);
                    updateGodRay(registry, entity, skill, statueTrans, deltaTime, enemyGrid);
                }
            } else {
                expiredGodRays.push_back(entity);
            }
        });

        if (!expiredGodRays.empty()) {
            registry.destroy(expiredGodRays.begin(), expiredGodRays.end());
        }

        // 3. God Ray Effects Lifecycle
        updateGodRayEffects(registry, deltaTime);

        // 4. Rosary Skill Instances Lifecycle (Independent timers)
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
                    orbital.ownerStatue = rosary.owner; 
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

        for (auto instance : expiredInstances) {
            auto sphereView = registry.view<component::OrbitalSphere>();
            for (auto entity : sphereView) {
                auto& orbital = sphereView.get<component::OrbitalSphere>(entity);
                if (orbital.parentInstance == instance) {
                    orbital.state = component::OrbitalSphere::State::Shrinking;
                    orbital.parentInstance = entt::null;
                }
            }
            registry.destroy(instance);
        }

        updateOrbitalSpheres(registry, deltaTime, enemyGrid);
    }

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
            if (!registry.valid(orbital.ownerStatue)) {
                toDestroy.push_back(entity);
                return;
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
                    sf::Vector2f pushVelocity = (pushDir / pushDist) * orbital.knockbackForce;
                    
                    if (orbital.state == component::OrbitalSphere::State::Expanding && pushDist < currentRadius) {
                        pushVelocity = (pushDir / pushDist) * (currentRadius - pushDist) * (1.0f / dt);
                    }
                    
                    registry.emplace_or_replace<component::Knockback>(enemy, pushVelocity, 0.05f);
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

    static void updateGodRay(entt::registry& registry, entt::entity instance, component::GodRaySkill& skill, const component::Transform& statueTrans, float dt, ProximityGrid& enemyGrid) {
        skill.timer += dt;
        if (skill.timer >= skill.cooldown) {
            std::vector<entt::entity> potentialTargets;
            
            // [RANDOM TARGETING] Collect all enemies within range
            enemyGrid.queryRange(statueTrans.position, skill.range, [&](entt::entity enemy) {
                if (!registry.valid(enemy)) return;
                potentialTargets.push_back(enemy);
            });

            if (!potentialTargets.empty()) {
                // Pick one at random
                static std::mt19937 rng(std::random_device{}());
                std::uniform_int_distribution<size_t> dist(0, potentialTargets.size() - 1);
                entt::entity target = potentialTargets[dist(rng)];

                auto& es = registry.get<component::UnitStats>(target);
                auto& et = registry.get<component::Transform>(target);
                
                // [NEW] Use Pivot offset for exact alignment at target's feet
                sf::Vector2f pivotPos = et.position;
                if (auto* pivot = registry.try_get<component::Pivot>(target)) {
                    pivotPos += pivot->offset;
                }

                es.currentHealth -= skill.damage;
                es.hitFlashTimer = 0.1f;

                // [REPLACED] Knockback -> Stun/Stiffness
                registry.emplace_or_replace<component::Stun>(target, skill.stunDuration);

                // Create "Baptism" effect above the target
                auto effect = registry.create();
                auto& effectComp = registry.emplace<component::GodRayEffect>(effect, 0.5f, 0.f, target);
                effectComp.lastTargetPos = pivotPos; // Anchor to pivot
                
                // Half-height for scale.y 0.5 is 128
                // Position center so that bottom is exactly at pivotPos
                registry.emplace<component::Transform>(effect, pivotPos - sf::Vector2f(0.f, 128.f)); 
                
                auto& sd = registry.emplace<component::SpriteData>(effect);
                sd.textureID = component::TextureID::GodRay;
                sd.scale = {0.2f, 0.5f}; 
                
                skill.timer = 0.f;
            }
        }
    }

    static void updateGodRayEffects(entt::registry& registry, float dt) {
        auto view = registry.view<component::GodRayEffect, component::Transform, component::SpriteData>();
        static std::vector<entt::entity> toDestroy;
        toDestroy.clear();

        view.each([&](auto entity, auto& effect, auto& trans, auto& sprite) {
            effect.timer += dt;
            if (effect.timer >= effect.duration) {
                toDestroy.push_back(entity);
            } else {
                sf::Vector2f refPos = effect.lastTargetPos;
                if (registry.valid(effect.target)) {
                    auto& et = registry.get<component::Transform>(effect.target);
                    refPos = et.position;
                    // [FIX] Always apply Pivot offset if available to track the specific enemy point
                    if (auto* pivot = registry.try_get<component::Pivot>(effect.target)) {
                        refPos += pivot->offset;
                    }
                    effect.lastTargetPos = refPos;
                }
                
                float progress = effect.timer / effect.duration;
                float targetScaleX = 1.2f;
                float currentScaleY = 0.5f;

                if (progress < 0.2f) {
                    float t = progress / 0.2f;
                    sprite.scale.x = 0.2f + (targetScaleX - 0.2f) * t; 
                } else if (progress < 0.7f) {
                    static std::mt19937 gen(1337);
                    std::uniform_real_distribution<float> dis(0.95f, 1.05f);
                    sprite.scale.x = targetScaleX * dis(gen); 
                } else {
                    float t = (progress - 0.7f) / 0.3f;
                    sprite.scale.x = targetScaleX * (1.0f - t);
                    currentScaleY = 0.5f * (1.0f - t * 0.5f);
                }
                sprite.scale.y = currentScaleY;

                // Anchoring: Keep the bottom of the beam exactly at refPos (the Pivot)
                // Beam height is 512. Visual half-height = (512 * scale.y) / 2
                float hh = (512.f * sprite.scale.y) / 2.f;
                trans.position = refPos - sf::Vector2f(0.f, hh);
            }
        });

        if (!toDestroy.empty()) registry.destroy(toDestroy.begin(), toDestroy.end());
    }
};
