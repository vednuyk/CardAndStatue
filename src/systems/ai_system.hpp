#pragma once
#include <entt/entt.hpp>
#include <SFML/System/Vector2.hpp>
#include <cmath>
#include <algorithm>
#include "../components/unit_components.hpp"
#include "proximity_grid.hpp"

class AISystem {
public:
    static void update(entt::registry& registry, float deltaTime, ProximityGrid& enemyGrid) {
        auto view = registry.view<component::Transform, component::UnitStats, component::Velocity, component::AIBehavior>();
        auto* globalTarget = registry.ctx().find<component::GlobalTargetLocation>();

        view.each([&](auto entity, auto& trans, auto& stats, auto& vel, auto& ai) {
            // [COMMON] Stun Logic
            if (registry.any_of<component::Stun>(entity)) {
                vel.value = {0.f, 0.f};
                return;
            }

            switch (ai.type) {
                case component::AIBehavior::Type::SeekStatue:
                    if (globalTarget) {
                        handleSeekStatue(entity, trans, stats, vel, registry, *globalTarget);
                    }
                    break;
                case component::AIBehavior::Type::DefendStatue:
                    handleDefendStatue(entity, trans, stats, vel, registry, enemyGrid);
                    break;
                case component::AIBehavior::Type::Berserker:
                    handleBerserker(entity, trans, stats, vel, registry, enemyGrid);
                    break;
                case component::AIBehavior::Type::Passive:
                    handlePassive(entity, trans, stats, vel, registry, enemyGrid);
                    break;
                default:
                    vel.value = {0.f, 0.f};
                    break;
            }

            // [VISUAL] Flip sprite based on movement
            if (auto* sprite = registry.try_get<component::SpriteData>(entity)) {
                if (std::abs(vel.value.x) > 0.1f) {
                    sprite->flipX = (vel.value.x < 0);
                }
            }
        });
    }

private:
    static void handleSeekStatue(entt::entity entity, component::Transform& trans, component::UnitStats& stats, component::Velocity& vel, entt::registry& registry, const component::GlobalTargetLocation& globalTarget) {
        sf::Vector2f moveDiff = globalTarget.position - trans.position;
        
        // Clamping logic for box boundary
        float closestX = std::max(globalTarget.bounds.position.x, std::min(trans.position.x, globalTarget.bounds.position.x + globalTarget.bounds.size.x));
        float closestY = std::max(globalTarget.bounds.position.y, std::min(trans.position.y, globalTarget.bounds.position.y + globalTarget.bounds.size.y));
        sf::Vector2f diffToBox = sf::Vector2f(closestX, closestY) - trans.position;
        float distSq = diffToBox.x * diffToBox.x + diffToBox.y * diffToBox.y;

        if (distSq <= 100.f) { 
            vel.value = {0.f, 0.f};
        } else {
            float moveDist = std::sqrt(moveDiff.x * moveDiff.x + moveDiff.y * moveDiff.y);
            if (moveDist > 0.001f) {
                vel.value = (moveDiff / moveDist) * stats.speed;
            }
        }
    }

    static void handleDefendStatue(entt::entity entity, component::Transform& trans, component::UnitStats& stats, component::Velocity& vel, entt::registry& registry, ProximityGrid& enemyGrid) {
        // [STRATEGY] DefendStatue: If enemies near statue, target them. Otherwise stay near statue.
        auto* globalTarget = registry.ctx().find<component::GlobalTargetLocation>();
        if (!globalTarget) return;

        entt::entity nearestEnemy = entt::null;
        float minDistSq = stats.attackRange * stats.attackRange * 9.f; // Look a bit further than range

        // Query enemies near the Statue
        enemyGrid.queryNearby(globalTarget->position, [&](entt::entity enemy) {
            if (!registry.valid(enemy)) return;
            auto& et = registry.get<component::Transform>(enemy);
            sf::Vector2f diff = et.position - trans.position;
            float dSq = diff.x * diff.x + diff.y * diff.y;
            if (dSq < minDistSq) {
                minDistSq = dSq;
                nearestEnemy = enemy;
            }
        });

        if (registry.valid(nearestEnemy)) {
            moveToTarget(trans, stats, vel, registry, nearestEnemy);
        } else {
            // No enemies near statue, move to a "defensive position" (e.g., slightly in front of statue)
            sf::Vector2f targetPos = globalTarget->position;
            sf::Vector2f diff = targetPos - trans.position;
            float distSq = diff.x * diff.x + diff.y * diff.y;
            if (distSq > 150.f * 150.f) {
                float dist = std::sqrt(distSq);
                vel.value = (diff / dist) * stats.speed;
            } else {
                vel.value = {0.f, 0.f};
            }
        }
    }

    static void handleBerserker(entt::entity entity, component::Transform& trans, component::UnitStats& stats, component::Velocity& vel, entt::registry& registry, ProximityGrid& grid) {
        entt::entity nearestTarget = entt::null;
        float minDistSq = 1000.f * 1000.f; // Large search radius

        grid.queryNearby(trans.position, [&](entt::entity potential) {
            if (!registry.valid(potential)) return;
            auto& pt = registry.get<component::Transform>(potential);
            sf::Vector2f diff = pt.position - trans.position;
            float dSq = diff.x * diff.x + diff.y * diff.y;
            if (dSq < minDistSq) {
                minDistSq = dSq;
                nearestTarget = potential;
            }
        });

        if (registry.valid(nearestTarget)) {
            moveToTarget(trans, stats, vel, registry, nearestTarget);
        } else {
            vel.value = {0.f, 0.f};
        }
    }

    static void handlePassive(entt::entity entity, component::Transform& trans, component::UnitStats& stats, component::Velocity& vel, entt::registry& registry, ProximityGrid& grid) {
        // Just stands still
        vel.value = {0.f, 0.f};
    }

    static void moveToTarget(component::Transform& trans, component::UnitStats& stats, component::Velocity& vel, entt::registry& registry, entt::entity target) {
        auto& tt = registry.get<component::Transform>(target);
        sf::Vector2f diff = tt.position - trans.position;
        float distSq = diff.x * diff.x + diff.y * diff.y;

        if (distSq <= stats.attackRange * stats.attackRange) {
            vel.value = {0.f, 0.f};
        } else {
            float dist = std::sqrt(distSq);
            vel.value = (diff / dist) * stats.speed;
        }
    }
};
