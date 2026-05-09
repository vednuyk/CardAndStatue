#pragma once
#include <entt/entt.hpp>
#include <SFML/System/Vector2.hpp>
#include <cmath>
#include <vector>
#include <functional>
#include <random>
#include <algorithm>
#include "../components/unit_components.hpp"
#include "entity_factory.hpp"
#include "proximity_grid.hpp"

class StatueSkillSystem {
public:
    using SkillUpdateFunc = std::function<void(entt::registry&, float, ProximityGrid&)>;

    static void update(entt::registry& registry, float deltaTime, ProximityGrid& enemyGrid) {
        // 1. Statue general stats update (Core logic)
        auto statueView = registry.view<component::StatueTag, component::Transform, component::StatueStats>();
        statueView.each([&](auto statueEntity, auto& transform, auto& stats) {
            stats.currentHealth = std::min(stats.maxHealth, stats.currentHealth + stats.hpRegen * deltaTime);
        });

        // 2. Execute all registered skill logic
        for (auto& skillFunc : getSkillLogics()) {
            skillFunc(registry, deltaTime, enemyGrid);
        }

        // 3. Effects Lifecycle
        updateEffectsLifecycle(registry, deltaTime);
        
        // 4. Rosary Lifecycle (Special case for orbital spheres)
        updateRosaryLifecycle(registry, deltaTime, enemyGrid);
    }

    // [OCP] Register new skill logic without modifying this class
    static void registerSkillLogic(SkillUpdateFunc func) {
        getSkillLogics().push_back(func);
    }

    // [OCP] Pre-defined Skill Logics
    static void updateHolyAttack(entt::registry& registry, float dt, ProximityGrid& enemyGrid) {
        auto view = registry.view<component::StatueTag, component::Transform, component::HolyAttackSkill>();
        view.each([&](auto entity, auto& trans, auto& skill) {
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
                        es.hitFlashTimer = 0.1f;
                        hitAny = true;
                    }
                });
                if (hitAny) skill.timer = 0.f;
            }
        });
    }

    static void updateGodRay(entt::registry& registry, float dt, ProximityGrid& enemyGrid) {
        auto view = registry.view<component::StatueTag, component::GodRaySkill>();
        view.each([&](auto entity, auto& skill) {
            skill.timer += dt;
            if (skill.timer >= skill.cooldown) {
                auto& enemyStorage = registry.storage<component::EnemyTag>();
                if (enemyStorage.empty()) return;

                // [OPTIMIZED] O(1) Random Targeting
                static std::mt19937 gen(std::random_device{}());
                std::uniform_int_distribution<size_t> dist(0, enemyStorage.size() - 1);
                entt::entity target = enemyStorage.data()[dist(gen)];

                if (!registry.valid(target)) return;

                auto& targetTrans = registry.get<component::Transform>(target);
                auto& targetStats = registry.get<component::UnitStats>(target);

                targetStats.currentHealth -= skill.damage;
                targetStats.hitFlashTimer = 0.1f;

                float splashRadiusSq = skill.splashRadius * skill.splashRadius;
                float splashDamage = skill.damage * skill.splashRatio;

                enemyGrid.queryNearby(targetTrans.position, [&](entt::entity nearbyEnemy) {
                    if (nearbyEnemy == target || !registry.valid(nearbyEnemy)) return;
                    auto& nt = registry.get<component::Transform>(nearbyEnemy);
                    sf::Vector2f diff = nt.position - targetTrans.position;
                    if (diff.x * diff.x + diff.y * diff.y <= splashRadiusSq) {
                        auto& ns = registry.get<component::UnitStats>(nearbyEnemy);
                        ns.currentHealth -= splashDamage;
                        ns.hitFlashTimer = 0.1f;
                    }
                });

                EntityFactory::createGodRayEffect(registry, targetTrans.position);
                skill.timer = 0.f;
            }
        });
    }

    static void updateSpawnKnight(entt::registry& registry, float dt, ProximityGrid& enemyGrid) {
        auto view = registry.view<component::StatueTag, component::Transform, component::SpawnKnightSkill>();
        view.each([&](auto entity, auto& trans, auto& skill) {
            skill.timer += dt;
            if (skill.timer >= skill.cooldown) {
                for (int i = 0; i < skill.spawnCount; ++i) {
                    float angle = (2.f * 3.14159f / skill.spawnCount) * i;
                    sf::Vector2f spawnPos = trans.position + sf::Vector2f(std::cos(angle), std::sin(angle)) * 60.f;
                    EntityFactory::createPlayerUnit(registry, spawnPos, "Knight");
                }
                skill.timer = 0.f;
            }
        });
    }

    static void applyRosary(entt::registry& registry, entt::entity statue, const component::RosarySkill& config) {
        auto instance = registry.create();
        auto& rosary = registry.emplace<component::RosarySkill>(instance, config);
        rosary.owner = statue;
    }

private:
    // [COMPATIBILITY] Use static method with local static to avoid inline static initialization issues
    static std::vector<SkillUpdateFunc>& getSkillLogics() {
        static std::vector<SkillUpdateFunc> logics;
        return logics;
    }

    static void updateEffectsLifecycle(entt::registry& registry, float deltaTime) {
        auto effectView = registry.view<component::GodRayEffect>();
        static std::vector<entt::entity> expiredEffects;
        expiredEffects.clear();
        effectView.each([&](auto entity, auto& effect) {
            effect.timer -= deltaTime;
            if (effect.timer <= 0.f) expiredEffects.push_back(entity);
        });
        if (!expiredEffects.empty()) registry.destroy(expiredEffects.begin(), expiredEffects.end());
    }

    static void updateRosaryLifecycle(entt::registry& registry, float deltaTime, ProximityGrid& enemyGrid) {
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
                if (rosary.remainingTime <= 0.f) expiredInstances.push_back(instanceEntity);
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
                    if (orbital.state == component::OrbitalSphere::State::Expanding && pushDist < currentRadius) {
                        et.position = parentTrans.position + (pushDir / pushDist) * currentRadius;
                    } else {
                        et.position += (pushDir / pushDist) * orbital.knockbackForce * dt;
                    }
                }
            }
        });
    }
};
