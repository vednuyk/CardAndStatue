#pragma once
#include <entt/entt.hpp>
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>
#include "../components/unit_components.hpp"

class VFXSystem {
public:
    static void update(entt::registry& registry, float deltaTime) {
        // 1. Process DamagedEvents to spawn floating text
        auto damageView = registry.view<component::DamagedEvent, component::Transform>();
        damageView.each([&](auto entity, auto& event, auto& trans) {
            if (event.damage > 0.f) {
                auto textEntity = registry.create();
                registry.emplace<component::Transform>(textEntity, trans.position);
                
                auto& ft = registry.emplace<component::FloatingText>(textEntity);
                ft.text = std::to_string(static_cast<int>(event.damage));
                ft.color = registry.any_of<component::EnemyTag>(entity) ? sf::Color::White : sf::Color::Red;
                ft.duration = 0.8f;
                ft.velocity = { (float)(rand() % 40 - 20), -60.f }; // Slight random spread
            }
        });

        // 2. Update existing FloatingText entities
        auto textView = registry.view<component::FloatingText, component::Transform>();
        static std::vector<entt::entity> toDestroy;
        toDestroy.clear();

        textView.each([&](auto entity, auto& ft, auto& trans) {
            ft.timer += deltaTime;
            if (ft.timer >= ft.duration) {
                toDestroy.push_back(entity);
            } else {
                trans.position += ft.velocity * deltaTime;
                // Add slight gravity to text
                ft.velocity.y += 100.f * deltaTime;
            }
        });

        if (!toDestroy.empty()) registry.destroy(toDestroy.begin(), toDestroy.end());
    }

    static void render(entt::registry& registry, sf::RenderWindow& window, const sf::Font& font) {
        auto view = registry.view<component::FloatingText, component::Transform>();
        view.each([&](auto entity, auto& ft, auto& trans) {
            sf::Text text(font);
            text.setString(ft.text);
            text.setCharacterSize(20);
            
            // Fade out
            float alpha = 1.f - (ft.timer / ft.duration);
            sf::Color c = ft.color;
            c.a = static_cast<uint8_t>(alpha * 255);
            text.setFillColor(c);
            
            sf::FloatRect bounds = text.getLocalBounds();
            text.setOrigin({bounds.position.x + bounds.size.x / 2.f, bounds.position.y + bounds.size.y / 2.f});
            text.setPosition(trans.position);
            
            window.draw(text);
        });
    }
};
