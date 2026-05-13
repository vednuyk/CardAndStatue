#pragma once
#include <entt/entt.hpp>
#include <string>
#include <vector>
#include "../components/unit_components.hpp"
#include "config_manager.hpp"
#include "statue_skill_system.hpp"

class StatuePassiveSystem {
public:
    struct PassiveState {
        std::string skillKey;
        float timer{0.f};
        float duration{10.f};
        float cooldown{5.f};
        bool isActive{false};
    };

    static void update(entt::registry& registry, float deltaTime, const config::ConfigManager& configMgr) {
        auto& passiveStates = getPassiveStates();
        
        auto statueView = registry.view<component::StatueTag>();
        if (statueView.begin() == statueView.end()) return;
        auto statue = statueView.front();

        for (auto& state : passiveStates) {
            state.timer += deltaTime;
            float cycleTime = state.duration + state.cooldown;

            if (state.timer >= cycleTime) {
                state.timer = 0.f;
                // Trigger skill
                StatueSkillSystem::createSkillInstance(registry, statue, state.skillKey, configMgr);
            }
        }
    }

    static void addPassive(const std::string& skillKey, const config::ConfigManager& configMgr) {
        auto& states = getPassiveStates();
        
        PassiveState newState;
        newState.skillKey = skillKey;
        
        // Data-driven values from JSON
        if (skillKey == "CARD_ROSARY") {
            newState.duration = configMgr.skills.rosary.duration;
            newState.cooldown = configMgr.skills.rosary.passiveCooldown;
        } else if (skillKey == "CARD_GOD_RAY") {
            newState.duration = configMgr.skills.godRay.duration;
            newState.cooldown = configMgr.skills.godRay.passiveCooldown;
        }
        
        newState.timer = newState.duration + newState.cooldown; // Trigger immediately on first update
        states.push_back(newState);
    }

    static std::vector<PassiveState>& getPassiveStates() {
        static std::vector<PassiveState> states;
        return states;
    }
};
