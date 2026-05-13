#pragma once
#include <entt/entt.hpp>
#include <SFML/System/Vector2.hpp>
#include <cmath>
#include <algorithm>
#include <numbers>
#include "../components/unit_components.hpp"
#include "proximity_grid.hpp"

class AISystem {
public:
    static void update(entt::registry& registry, float deltaTime, ProximityGrid& enemyGrid, ProximityGrid& playerGrid) {
        auto view = registry.view<component::Transform, component::UnitStats, component::Velocity, component::AIBehavior>();
        auto* globalTarget = registry.ctx().find<component::GlobalTargetLocation>();

        view.each([&](auto entity, auto& trans, auto& stats, auto& vel, auto& ai) {
            // [COMMON] Stun Logic
            if (registry.any_of<component::Stun>(entity)) {
                vel.value = {0.f, 0.f};
                return;
            }

            // --- LAYER 1: TARGETING ---
            findTarget(entity, trans, stats, ai, registry, enemyGrid, playerGrid, globalTarget);

            // --- LAYER 2: MOVEMENT & DISTANCE CHECK ---
            if (ai.targetEntity != entt::null || ai.targeting == component::AIBehavior::Targeting::ToStatue) {
                sf::Vector2f targetPos = getTargetPosition(ai, registry, globalTarget);
                
                // [PHYSICAL-ACCURACY] Distance check accounts for own radius
                float distSq = calculateDistanceSq(trans.position, targetPos, ai, registry, globalTarget);
                float combinedReach = stats.attackRange + trans.radius;

                if (distSq <= combinedReach * combinedReach) {
                    // In Range: Stop and Attack
                    vel.value = {0.f, 0.f};
                } else {
                    // Out of Range: Move using Pattern
                    applyMovementPattern(entity, trans, stats, vel, ai, targetPos, deltaTime);
                }
            } else {
                vel.value = {0.f, 0.f};
            }

            // --- LAYER 3: ATTACK PATTERN (Currently handled by UnitCombatSystem) ---
            // We can add logic here if specific patterns need prep work

            // [VISUAL] Flip sprite based on movement
            if (auto* sprite = registry.try_get<component::SpriteData>(entity)) {
                if (std::abs(vel.value.x) > 0.1f) {
                    sprite->flipX = (vel.value.x < 0);
                }
            }
        });
    }

private:
    static void findTarget(entt::entity entity, const component::Transform& trans, const component::UnitStats& stats, component::AIBehavior& ai, entt::registry& registry, ProximityGrid& enemyGrid, ProximityGrid& playerGrid, const component::GlobalTargetLocation* globalTarget) {
        bool isEnemy = registry.any_of<component::EnemyTag>(entity);
        
        if (ai.targeting == component::AIBehavior::Targeting::ToStatue) {
            // Primarily target Statue, but check for nearby PlayerUnits to "interact" with
            entt::entity nearestKnight = entt::null;
            float minDistSq = stats.attackRange * stats.attackRange * 1.5f; // Detection range slightly larger than attack range

            playerGrid.queryNearby(trans.position, [&](entt::entity knight) {
                if (!registry.valid(knight)) return;
                auto& kt = registry.get<component::Transform>(knight);
                float dSq = distSq(kt.position, trans.position);
                if (dSq < minDistSq) {
                    minDistSq = dSq;
                    nearestKnight = knight;
                }
            });

            ai.targetEntity = nearestKnight; // If no knight, targetEntity is null, will move to Statue by default
        } 
        else if (ai.targeting == component::AIBehavior::Targeting::ToNearestEnemy) {
            // Target the closest enemy unit
            entt::entity nearestEnemy = entt::null;
            float minDistSq = 2000.f * 2000.f; // Search wide

            enemyGrid.queryNearby(trans.position, [&](entt::entity enemy) {
                if (!registry.valid(enemy)) return;
                auto& et = registry.get<component::Transform>(enemy);
                float dSq = distSq(et.position, trans.position);
                if (dSq < minDistSq) {
                    minDistSq = dSq;
                    nearestEnemy = enemy;
                }
            });
            ai.targetEntity = nearestEnemy;
        }
    }

    static sf::Vector2f getTargetPosition(const component::AIBehavior& ai, entt::registry& registry, const component::GlobalTargetLocation* globalTarget) {
        if (registry.valid(ai.targetEntity)) {
            return registry.get<component::Transform>(ai.targetEntity).position;
        }
        if (ai.targeting == component::AIBehavior::Targeting::ToStatue && globalTarget) {
            return globalTarget->position;
        }
        return {0.f, 0.f};
    }

    static float calculateDistanceSq(sf::Vector2f pos, sf::Vector2f targetPos, const component::AIBehavior& ai, entt::registry& registry, const component::GlobalTargetLocation* globalTarget) {
        // If targeting Statue, check against its BoxCollider bounds for better precision
        if (!registry.valid(ai.targetEntity) && ai.targeting == component::AIBehavior::Targeting::ToStatue && globalTarget) {
            float closestX = std::max(globalTarget->bounds.position.x, std::min(pos.x, globalTarget->bounds.position.x + globalTarget->bounds.size.x));
            float closestY = std::max(globalTarget->bounds.position.y, std::min(pos.y, globalTarget->bounds.position.y + globalTarget->bounds.size.y));
            sf::Vector2f diff = sf::Vector2f(closestX, closestY) - pos;
            return diff.x * diff.x + diff.y * diff.y;
        }
        // Otherwise standard point-to-point
        sf::Vector2f diff = targetPos - pos;
        return diff.x * diff.x + diff.y * diff.y;
    }

    static void applyMovementPattern(entt::entity entity, const component::Transform& trans, const component::UnitStats& stats, component::Velocity& vel, component::AIBehavior& ai, sf::Vector2f targetPos, float dt) {
        sf::Vector2f dir = targetPos - trans.position;
        float dist = std::sqrt(dir.x * dir.x + dir.y * dir.y);
        if (dist < 0.001f) {
            vel.value = {0.f, 0.f};
            return;
        }
        dir /= dist;

        ai.movementTimer += dt;

        switch (ai.movement) {
            case component::AIBehavior::Movement::Linear:
                vel.value = dir * stats.speed;
                break;

            case component::AIBehavior::Movement::SineWave: {
                sf::Vector2f perp(-dir.y, dir.x);
                float wave = std::sin(ai.movementTimer * 5.f) * 30.f;
                vel.value = (dir * stats.speed) + (perp * wave);
                break;
            }

            case component::AIBehavior::Movement::Dash: {
                // Cycle: 1s slow approach, 0.5s dash
                float cycle = std::fmod(ai.movementTimer, 1.5f);
                if (cycle < 1.0f) {
                    vel.value = dir * (stats.speed * 0.5f);
                } else {
                    vel.value = dir * (stats.speed * 3.0f);
                }
                break;
            }
        }
    }

    static float distSq(sf::Vector2f a, sf::Vector2f b) {
        sf::Vector2f d = a - b;
        return d.x * d.x + d.y * d.y;
    }
};
