#pragma once
#include <entt/entt.hpp>
#include "../components/unit_components.hpp"

class EntityPhysicsSystem {
public:
    static void update(entt::registry& registry) {
        // Handle pending physics requests
        auto view = registry.view<component::UnitStats, component::PhysicsRequest>();
        
        view.each([&](auto entity, auto& stats, auto& request) {
            switch (request.type) {
                case component::PhysicsRequest::Type::KnockbackEffect:
                    if (stats.isPushable) {
                        registry.emplace_or_replace<component::Knockback>(entity, request.velocity, request.duration);
                    }
                    break;
                    
                case component::PhysicsRequest::Type::StunEffect:
                    if (stats.isStunnable) {
                        registry.emplace_or_replace<component::Stun>(entity, request.duration);
                    }
                    break;
                    
                case component::PhysicsRequest::Type::BothEffects:
                    if (stats.isPushable) {
                        registry.emplace_or_replace<component::Knockback>(entity, request.velocity, 0.05f); 
                    }
                    if (stats.isStunnable) {
                        registry.emplace_or_replace<component::Stun>(entity, request.duration);
                    }
                    break;
            }
        });
        
        // Clear requests after processing
        registry.clear<component::PhysicsRequest>();
    }
};