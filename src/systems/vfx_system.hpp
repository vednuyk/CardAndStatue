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
                
                // [DYNAMIC-OFFSET] Handle both UnitStats and StatueStats
                float heightOffset = 20.f; // Fallback
                sf::Color textColor = sf::Color::White;

                if (auto* stats = registry.try_get<component::UnitStats>(entity)) {
                    heightOffset = stats->vfxHeight * 0.8f;
                    textColor = registry.any_of<component::EnemyTag>(entity) ? sf::Color::White : sf::Color::Red;
                } else if (auto* statueStats = registry.try_get<component::StatueStats>(entity)) {
                    heightOffset = statueStats->vfxHeight * 0.8f;
                    textColor = sf::Color::Red; // Statue taking damage is always alarming
                }
                
                sf::Vector2f spawnPos = trans.position - sf::Vector2f(0.f, heightOffset);
                registry.emplace<component::Transform>(textEntity, spawnPos);
                
                auto& ft = registry.emplace<component::FloatingText>(textEntity);
                ft.text = std::to_string(static_cast<int>(event.damage));
                ft.color = textColor;
                ft.duration = 0.5f; // [QUICKER] Shorter duration (0.8s -> 0.5s)
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
        
        // [VISUAL] Re-enable smoothing for clean, high-quality text as pixel-style was too blurry/broken
        const_cast<sf::Texture&>(font.getTexture(16)).setSmooth(true);

        view.each([&](auto entity, auto& ft, auto& trans) {
            sf::Text text(font);
            text.setString(ft.text);
            text.setCharacterSize(16); // Smaller, cleaner size
            
            // Reverted: No outline for a minimalist pixel feel
            
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
