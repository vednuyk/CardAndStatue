#pragma once
#include <entt/entt.hpp>
#include <vector>
#include <algorithm>
#include <execution> 
#include <cmath>
#include "../components/unit_components.hpp"

class UnitMovementSystem {
public:
    static void update(entt::registry& registry, float deltaTime) noexcept {
        auto view = registry.view<component::Transform, component::Velocity>();
        
        static std::vector<entt::entity> entities;
        entities.clear();
        entities.reserve(view.size_hint());
        for(auto entity : view) entities.push_back(entity);

        if (entities.empty()) return;

        std::for_each(std::execution::par, entities.begin(), entities.end(), [&](entt::entity entity) {
            auto& transform = registry.get<component::Transform>(entity);
            auto& velocity = registry.get<component::Velocity>(entity);

            // [NEW] Stun/Stiffness Logic: Zero out self-velocity, but don't return!
            // This allows Knockback (External Force) to still apply below.
            sf::Vector2f effectiveVel = velocity.value;
            if (registry.any_of<component::Stun>(entity)) {
                effectiveVel = {0.f, 0.f};
            }

            // 1. 기본 이동 (AI Velocity or Zero if Stunned)
            transform.position += effectiveVel * deltaTime;

            // 2. 넉백 처리 (External Force - Always applies)
            if (auto* kb = registry.try_get<component::Knockback>(entity)) {
                transform.position += kb->force * deltaTime;
                kb->force *= 0.85f; 
                kb->duration -= deltaTime;
            }
        });

        // Lifecycle updates (Remove expired components)
        static std::vector<entt::entity> toRemove;
        toRemove.clear();

        auto stunView = registry.view<component::Stun>();
        for (auto entity : stunView) {
            auto& s = stunView.get<component::Stun>(entity);
            s.duration -= deltaTime;
            if (s.duration <= 0.f) toRemove.push_back(entity);
        }
        if (!toRemove.empty()) registry.remove<component::Stun>(toRemove.begin(), toRemove.end());
        toRemove.clear();

        auto kbView = registry.view<component::Knockback>();
        for (auto entity : kbView) {
            if (kbView.get<component::Knockback>(entity).duration <= 0.f) {
                toRemove.push_back(entity);
            }
        }
        if (!toRemove.empty()) {
            registry.remove<component::Knockback>(toRemove.begin(), toRemove.end());
        }
    }
};
