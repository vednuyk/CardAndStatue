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
        auto statueView = registry.view<component::StatueTag, component::Transform, component::StatueStats>();
        statueView.each([&](auto statueEntity, auto& transform, auto& stats) {
            stats.currentHealth = std::min(stats.maxHealth, stats.currentHealth + stats.hpRegen * deltaTime);

            if (auto* holy = registry.try_get<component::HolyAttackSkill>(statueEntity)) {
                updateHolyAttack(registry, statueEntity, *holy, transform, deltaTime, enemyGrid);
            }

            if (auto* spawn = registry.try_get<component::SpawnKnightSkill>(statueEntity)) {
                updateSpawnKnight(registry, statueEntity, *spawn, transform, deltaTime);
            }

            if (auto* rosary = registry.try_get<component::RosarySkill>(statueEntity)) {
                if (!rosary->initialized) {
                    for (int i = 0; i < 16; ++i) {
                        auto sphere = registry.create();
                        registry.emplace<component::Transform>(sphere, transform.position);
                        float targetOffset = (2.f * 3.14159f / 16.f) * i;
                        registry.emplace<component::OrbitalSphere>(sphere, statueEntity, 0.f, targetOffset, rosary->radius, rosary->rotationSpeed, rosary->knockbackForce, rosary->damage, 0.f);
                        
                        auto& sd = registry.emplace<component::SpriteData>(sphere);
                        sd.textureID = component::TextureID::RosarySphere;
                        sd.scale = {0.1f, 0.1f};
                    }
                    rosary->initialized = true;
                    rosary->remainingTime = rosary->duration;
                } else {
                    if (!rosary->isClosing) {
                        rosary->remainingTime -= deltaTime;
                        if (rosary->remainingTime <= 0.f) {
                            rosary->isClosing = true;
                        }
                    } else {
                        // [NEW] Check if all spheres have finished contracting
                        bool allDone = true;
                        auto sphereView = registry.view<component::OrbitalSphere>();
                        sphereView.each([&](auto entity, auto& orbital) {
                            if (orbital.parent == statueEntity) {
                                if (orbital.expansionProgress > 0.01f) allDone = false;
                            }
                        });

                        if (allDone) {
                            static std::vector<entt::entity> toDestroy;
                            toDestroy.clear();
                            sphereView.each([&](auto entity, auto& orbital) {
                                if (orbital.parent == statueEntity) toDestroy.push_back(entity);
                            });
                            if (!toDestroy.empty()) registry.destroy(toDestroy.begin(), toDestroy.end());
                            registry.remove<component::RosarySkill>(statueEntity);
                        }
                    }
                }
            }
        });

        updateOrbitalSpheres(registry, deltaTime, enemyGrid);
    }

private:
    static void updateOrbitalSpheres(entt::registry& registry, float deltaTime, ProximityGrid& enemyGrid) {
        auto view = registry.view<component::OrbitalSphere, component::Transform>();
        static std::vector<entt::entity> invalidOrbits;
        invalidOrbits.clear();

        view.each([&](auto entity, auto& orbital, auto& trans) {
            if (!registry.valid(orbital.parent)) {
                invalidOrbits.push_back(entity);
                return;
            }

            bool closing = false;
            if (auto* rosary = registry.try_get<component::RosarySkill>(orbital.parent)) {
                closing = rosary->isClosing;
            }

            auto& parentTrans = registry.get<component::Transform>(orbital.parent);
            
            // [MODIFIED] Handle both expansion and contraction
            if (closing) {
                // Shrink back faster: 0.8 seconds
                orbital.expansionProgress = std::max(0.0f, orbital.expansionProgress - deltaTime / 0.8f);
            } else if (orbital.expansionProgress < 1.0f) {
                orbital.expansionProgress = std::min(1.0f, orbital.expansionProgress + deltaTime / 1.5f);
            }

            float easeT = closing ? (orbital.expansionProgress * orbital.expansionProgress) : (orbital.expansionProgress * orbital.expansionProgress * orbital.expansionProgress);
            float currentRadius = easeT * orbital.radius;
            float finalAngle = orbital.orbitAngle + orbital.targetSpreadOffset;

            orbital.orbitAngle += orbital.rotationSpeed * deltaTime;
            trans.position.x = parentTrans.position.x + std::cos(finalAngle) * currentRadius;
            trans.position.y = parentTrans.position.y + std::sin(finalAngle) * currentRadius;

            // [NEW] Dynamic Scaling: 0.1 -> 0.5 based on expansion
            if (auto* sd = registry.try_get<component::SpriteData>(entity)) {
                float targetScale = 0.1f + (orbital.expansionProgress * 0.4f);
                sd->scale = {targetScale, targetScale};
            }

            // [FIX] Damage Logic: Only active if expansion has started (radius > threshold)
            if (currentRadius > 5.0f) {
                enemyGrid.queryNearby(trans.position, [&](entt::entity enemy) {
                    if (!registry.valid(enemy)) return;
                    auto& et = registry.get<component::Transform>(enemy);
                    sf::Vector2f diff = et.position - trans.position;
                    float distSq = diff.x * diff.x + diff.y * diff.y;
                    float combinedRadius = 20.f;

                    if (distSq < combinedRadius * combinedRadius) {
                        auto& es = registry.get<component::UnitStats>(enemy);
                        es.currentHealth -= orbital.damage * deltaTime;
                        es.hitFlashTimer = 0.1f;

                        sf::Vector2f pushDir = et.position - parentTrans.position;
                        float pushDist = std::sqrt(pushDir.x * pushDir.x + pushDir.y * pushDir.y);
                        if (pushDist > 0.001f) {
                            et.position += (pushDir / pushDist) * orbital.knockbackForce * deltaTime;
                        }
                    }
                });
            }
        });

        if (!invalidOrbits.empty()) {
            registry.destroy(invalidOrbits.begin(), invalidOrbits.end());
        }
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
