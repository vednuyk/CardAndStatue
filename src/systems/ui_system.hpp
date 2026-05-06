#pragma once
#include <SFML/Graphics.hpp>
#include <entt/entt.hpp>
#include <algorithm>
#include <vector>
#include <cstdint>
#include <cmath>
#include "../components/unit_components.hpp"

class UISystem {
public:
    enum class UIType {
        WorldSpace,
        ScreenSpace
    };

    struct CardInfo {
        sf::String name;
        sf::Vector2f position;
        sf::Vector2f size;
        float rotation;
        bool isDragging;
        float hoverProgress; 
        bool isHovered; 
        int index;      
        bool isInsideTarget; 
    };

    static void render(entt::registry& registry, sf::RenderTarget& target, const sf::View& worldView, const std::vector<CardInfo>& cards, const sf::Font& font, float totalTime) {
        renderWorldUI(registry, target);
        renderScreenUI(target, cards, font, totalTime);
    }

private:
    static void renderScreenUI(sf::RenderTarget& target, const std::vector<CardInfo>& cards, const sf::Font& font, float totalTime) {
        std::vector<const CardInfo*> renderOrder;
        for (const auto& card : cards) renderOrder.push_back(&card);

        std::sort(renderOrder.begin(), renderOrder.end(), [](const auto* a, const auto* b) {
            if (a->isDragging != b->isDragging) return !a->isDragging; 
            if (a->isHovered != b->isHovered) return !a->isHovered;
            return a->index < b->index;
        });

        for (const auto* cardPtr : renderOrder) {
            const auto& card = *cardPtr;
            
            float scale = 1.0f + (card.hoverProgress * 0.4f);
            float lift = card.hoverProgress * 80.f;
            float animatedRotation = card.rotation * (1.0f - card.hoverProgress);

            sf::RectangleShape shape(card.size);
            shape.setOrigin({card.size.x / 2.f, card.size.y});
            
            sf::Vector2f renderPos = card.position + sf::Vector2f(card.size.x / 2.f, card.size.y);
            renderPos.y -= lift;
            
            shape.setPosition(renderPos);
            shape.setScale({scale, scale});
            shape.setRotation(sf::degrees(animatedRotation));
            
            // [FIXED] Symmetrical, Uniform Glow & Reverted Color
            sf::Color baseColor = card.isDragging ? sf::Color(100, 100, 250) : sf::Color(220, 220, 220);
            
            if (card.isInsideTarget) {
                float pulse = (std::sin(totalTime * 8.f) + 1.f) * 0.5f;
                
                // [FIX] Transparent Blue (Reverted from Golden)
                baseColor.a = 150; 
                
                // [FIX] Calculate Center for Symmetrical Glow
                // Point {size.x/2, size.y/2} in local space is the exact center
                sf::Vector2f visualCenter = shape.getTransform().transformPoint({card.size.x / 2.f, card.size.y / 2.f});

                int layers = 10; // Fewer layers for a tighter, cleaner glow
                float maxPadding = 12.f + (pulse * 5.f); // Much thinner padding
                
                for (int i = 0; i < layers; ++i) {
                    float layerProgress = static_cast<float>(i + 1) / layers;
                    
                    // [FIX] Uniform thickness by adding pixels to size
                    float padding = maxPadding * layerProgress;
                    sf::Vector2f glowSize = card.size + sf::Vector2f(padding * 2.f, padding * 2.f);
                    
                    sf::RectangleShape glow(glowSize);
                    glow.setOrigin(glowSize / 2.f); 
                    glow.setPosition(visualCenter);
                    glow.setRotation(shape.getRotation());
                    glow.setScale(shape.getScale());
                    
                    // Cleaner Alpha falloff (faster dissipation)
                    float alpha = 100.f * std::pow(1.f - layerProgress, 1.5f) * (0.8f + 0.2f * pulse);
                    glow.setFillColor(sf::Color(255, 255, 255, static_cast<std::uint8_t>(alpha)));
                    
                    target.draw(glow);
                }
            } else {
                baseColor.a = 255;
            }

            shape.setFillColor(baseColor);
            shape.setOutlineThickness(2.f / scale);
            shape.setOutlineColor(sf::Color(0, 0, 0, baseColor.a));
            target.draw(shape);

            sf::Text text(font);
            text.setString(card.name);
            text.setCharacterSize(static_cast<unsigned int>(12 * scale));
            sf::Color textColor = sf::Color::Black;
            textColor.a = baseColor.a;
            text.setFillColor(textColor);
            
            sf::FloatRect textBounds = text.getLocalBounds();
            text.setOrigin({textBounds.position.x + textBounds.size.x / 2.f, textBounds.position.y + textBounds.size.y / 2.f});
            
            sf::Transform cardTransform;
            cardTransform.translate(shape.getPosition()).rotate(sf::degrees(animatedRotation));
            sf::Vector2f textLocalPos = {0.f, (-card.size.y + 25.f) * scale};
            text.setPosition(cardTransform.transformPoint(textLocalPos));
            text.setRotation(sf::degrees(animatedRotation));
            
            target.draw(text);
        }
    }

    static void renderWorldUI(entt::registry& registry, sf::RenderTarget& target) {
        auto statueView = registry.view<component::Transform, component::StatueStats>();
        statueView.each([&](auto entity, auto& trans, auto& stats) {
            if (stats.currentHealth < stats.maxHealth) {
                drawHealthBar(target, trans.position, stats.currentHealth, stats.maxHealth, {120.f, 8.f}, -80.f);
            }
        });

        auto unitView = registry.view<component::Transform, component::UnitStats>();
        unitView.each([&](auto entity, auto& trans, auto& stats) {
            if (stats.currentHealth < stats.maxHealth) {
                drawHealthBar(target, trans.position, stats.currentHealth, stats.maxHealth, {40.f, 5.f}, -35.f);
            }
        });
    }

    static void drawHealthBar(sf::RenderTarget& target, sf::Vector2f position, float current, float max, sf::Vector2f size, float offsetY) {
        sf::RectangleShape bg(size);
        bg.setOrigin({size.x / 2.f, size.y / 2.f});
        bg.setPosition({position.x, position.y + offsetY});
        bg.setFillColor(sf::Color(60, 0, 0, 200)); 
        target.draw(bg);

        float percentage = std::max(0.f, std::min(1.f, current / max));
        sf::RectangleShape fg({size.x * percentage, size.y});
        fg.setOrigin({0.f, size.y / 2.f}); 
        fg.setPosition({position.x - size.x / 2.f, position.y + offsetY});
        
        if (percentage > 0.5f) fg.setFillColor(sf::Color::Green);
        else if (percentage > 0.2f) fg.setFillColor(sf::Color::Yellow);
        else fg.setFillColor(sf::Color::Red);

        target.draw(fg);
    }
};
