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

        for (auto statueEntity : statueView) {
            auto& transform = registry.get<component::Transform>(statueEntity);
            auto& stats = registry.get<component::StatueStats>(statueEntity);

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
                        // [MODIFIED] Spawning with expansionProgress = 0 and targetSpreadOffset
                        float targetOffset = (2.f * 3.14159f / 16.f) * i;
                        registry.emplace<component::OrbitalSphere>(sphere, statueEntity, 0.f, targetOffset, rosary->radius, rosary->rotationSpeed, rosary->knockbackForce, rosary->damage, 0.f);
                        
                        auto& sd = registry.emplace<component::SpriteData>(sphere);
                        sd.textureID = component::TextureID::RosarySphere;
                        sd.scale = {0.5f, 0.5f};
                    }
                    rosary->initialized = true;
                    rosary->remainingTime = rosary->duration;
                } else {
                    rosary->remainingTime -= deltaTime;
                    if (rosary->remainingTime <= 0.f) {
                        auto sphereView = registry.view<component::OrbitalSphere>();
                        std::vector<entt::entity> toDestroy;
                        for (auto entity : sphereView) {
                            if (sphereView.get<component::OrbitalSphere>(entity).parent == statueEntity) {
                                toDestroy.push_back(entity);
                            }
                        }
                        registry.destroy(toDestroy.begin(), toDestroy.end());
                        registry.remove<component::RosarySkill>(statueEntity);
                    }
                }
            }
        }

        updateOrbitalSpheres(registry, deltaTime, enemyGrid);
    }

private:
    static void updateOrbitalSpheres(entt::registry& registry, float deltaTime, ProximityGrid& enemyGrid) {
        auto view = registry.view<component::OrbitalSphere, component::Transform>();
        for (auto entity : view) {
            auto& orbital = view.get<component::OrbitalSphere>(entity);
            auto& trans = view.get<component::Transform>(entity);

            if (!registry.valid(orbital.parent)) {
                registry.destroy(entity);
                continue;
            }

            auto& parentTrans = registry.get<component::Transform>(orbital.parent);
            
            // [NEW] Spiraling Launch Logic -> Changed to Radial Expansion
            if (orbital.expansionProgress < 1.0f) {
                // Expand over 1.5 seconds with cubic easing for "acceleration" feel
                orbital.expansionProgress = std::min(1.0f, orbital.expansionProgress + deltaTime / 1.5f);
            }

            // Cubic easing for a snappy acceleration feel: t^3
            float easeT = orbital.expansionProgress * orbital.expansionProgress * orbital.expansionProgress;
            
            float currentRadius = easeT * orbital.radius;
            // [MODIFIED] No longer interpolate spread; maintain circle from start
            float finalAngle = orbital.orbitAngle + orbital.targetSpreadOffset;

            // Continuous rotation
            orbital.orbitAngle += orbital.rotationSpeed * deltaTime;

            trans.position.x = parentTrans.position.x + std::cos(finalAngle) * currentRadius;
            trans.position.y = parentTrans.position.y + std::sin(finalAngle) * currentRadius;

            // Collision Check
            enemyGrid.queryNearby(trans.position, [&](entt::entity enemy) {
                if (!registry.valid(enemy)) return;
                auto& et = registry.get<component::Transform>(enemy);
                sf::Vector2f diff = et.position - trans.position;
                float distSq = diff.x * diff.x + diff.y * diff.y;
                float combinedRadius = 20.f; // Sphere radius + enemy radius (approx)

                if (distSq < combinedRadius * combinedRadius) {
                    auto& es = registry.get<component::UnitStats>(enemy);
                    es.currentHealth -= orbital.damage * deltaTime;
                    es.hitFlashTimer = 0.1f; // [NEW] Hit Flash Effect

                    // Knockback: Push enemy away from Statue center
                    sf::Vector2f pushDir = et.position - parentTrans.position;
                    float pushDist = std::sqrt(pushDir.x * pushDir.x + pushDir.y * pushDir.y);
                    if (pushDist > 0.001f) {
                        et.position += (pushDir / pushDist) * orbital.knockbackForce * deltaTime;
                    }
                }
            });
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

    static entt::entity findNearestEnemy(entt::registry& registry, sf::Vector2f position, float range, ProximityGrid& enemyGrid) {
        entt::entity nearest = entt::null;
        float minDistSq = range * range;

        enemyGrid.queryNearby(position, [&](entt::entity enemy) {
            if (!registry.valid(enemy)) return;
            auto& es = registry.get<component::UnitStats>(enemy);
            if (es.currentHealth <= 0.f) return;

            auto& et = registry.get<component::Transform>(enemy);
            sf::Vector2f diff = et.position - position;
            float distSq = diff.x * diff.x + diff.y * diff.y;

            if (distSq < minDistSq) {
                minDistSq = distSq;
                nearest = enemy;
            }
        });

        return nearest;
    }
};
