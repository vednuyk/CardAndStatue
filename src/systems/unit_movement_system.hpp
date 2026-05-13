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
        // 1. Unified Lifecycle Update (Remove expired components first)
        // This prevents processing entities that should already be "cured"
        static std::vector<entt::entity> toRemove;
        
        // Stun Lifecycle
        toRemove.clear();
        auto stunView = registry.view<component::Stun>();
        stunView.each([&](auto entity, auto& s) {
            s.duration -= deltaTime;
            if (s.duration <= 0.f) toRemove.push_back(entity);
        });
        if (!toRemove.empty()) registry.remove<component::Stun>(toRemove.begin(), toRemove.end());

        // Knockback Lifecycle
        toRemove.clear();
        auto kbView = registry.view<component::Knockback>();
        kbView.each([&](auto entity, auto& kb) {
            kb.duration -= deltaTime;
            if (kb.duration <= 0.f) toRemove.push_back(entity);
        });
        if (!toRemove.empty()) registry.remove<component::Knockback>(toRemove.begin(), toRemove.end());

        // 2. Optimized Movement Loop
        // We use a view to ensure data locality
        auto moveView = registry.view<component::Transform, component::Velocity>();
        
        // For very large numbers of entities, parallel loop can be beneficial.
        // But we avoid the manual vector copy if possible by using a more efficient pattern.
        moveView.each([&](auto entity, auto& transform, auto& velocity) {
            sf::Vector2f effectiveVel = velocity.value;
            
            // If stunned, self-velocity is negated but external forces still apply
            if (registry.any_of<component::Stun>(entity)) {
                effectiveVel = {0.f, 0.f};
            }

            // Apply base movement
            transform.position += effectiveVel * deltaTime;

            // Apply external forces (Knockback)
            if (auto* kb = registry.try_get<component::Knockback>(entity)) {
                transform.position += kb->force * deltaTime;
                // [DATA-DRIVEN] Friction should ideally be in a config, but keeping 0.85f for now
                kb->force *= 0.85f; 
            }
        });
    }
};
