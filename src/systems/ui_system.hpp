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
        float dimProgress; // New: 0.0 to 1.0 smooth dimming
    };

    struct PassiveSlotInfo {
        sf::String cardName;
        sf::Vector2f position;
        sf::Vector2f size;
        bool isOccupied;
        bool isActive;
        float progress; // 0.0 to 1.0
        bool isHighlighted; // New: For drag-over feedback
    };

    static void renderAll(entt::registry& registry, sf::RenderTarget& target, const sf::View& worldView, 
                       const std::vector<CardInfo>& cards, const std::vector<PassiveSlotInfo>& passiveSlots, 
                       const sf::Font& font, float totalTime, bool anyCardHovered) {
        renderWorldUI(registry, target);
        renderScreenUI(target, cards, passiveSlots, font, totalTime, anyCardHovered);
    }

private:
    static void renderScreenUI(sf::RenderTarget& target, const std::vector<CardInfo>& cards, 
                               const std::vector<PassiveSlotInfo>& passiveSlots, 
                               const sf::Font& font, float totalTime, bool anyCardHovered) {
        
        // Render Passive Slots first (background layer)
        for (const auto& slot : passiveSlots) {
            sf::RectangleShape shape(slot.size);
            shape.setPosition(slot.position);
            
            if (!slot.isOccupied) {
                shape.setFillColor(sf::Color(0, 0, 0, 50));
                shape.setOutlineThickness(2.f);
                shape.setOutlineColor(sf::Color(255, 255, 255, 100));
                
                if (slot.isHighlighted) {
                    float pulse = (std::sin(totalTime * 8.f) + 1.f) * 0.5f;
                    sf::Vector2f visualCenter = slot.position + slot.size / 2.f;

                    int layers = 8;
                    float maxPadding = 10.f + (pulse * 5.f);
                    
                    for (int i = 0; i < layers; ++i) {
                        float layerProgress = static_cast<float>(i + 1) / layers;
                        float padding = maxPadding * layerProgress;
                        sf::Vector2f glowSize = slot.size + sf::Vector2f(padding * 2.f, padding * 2.f);
                        
                        sf::RectangleShape glow(glowSize);
                        glow.setOrigin(glowSize / 2.f); 
                        glow.setPosition(visualCenter);
                        
                        float alpha = 120.f * std::pow(1.f - layerProgress, 1.5f) * (0.8f + 0.2f * pulse);
                        glow.setFillColor(sf::Color(255, 255, 255, static_cast<std::uint8_t>(alpha)));
                        
                        target.draw(glow);
                    }
                    shape.setOutlineColor(sf::Color::White);
                    shape.setOutlineThickness(3.f);
                }
                
                target.draw(shape);
            } else {
                sf::Color baseColor = slot.isActive ? sf::Color(150, 255, 150) : sf::Color(180, 180, 180);
                shape.setFillColor(baseColor);
                shape.setOutlineThickness(slot.isActive ? 4.f : 2.f);
                shape.setOutlineColor(slot.isActive ? sf::Color::Green : sf::Color::White);
                target.draw(shape);

                if (!slot.isActive) {
                    // Cooldown overlay (Darkening from top to bottom as it recharges)
                    sf::RectangleShape overlay(slot.size);
                    overlay.setPosition(slot.position);
                    overlay.setFillColor(sf::Color(0, 0, 0, 200));
                    overlay.setScale({1.f, 1.f - slot.progress}); 
                    target.draw(overlay);
                } else {
                    // Active pulse effect
                    float pulse = (std::sin(totalTime * 10.f) + 1.f) * 0.5f;
                    shape.setOutlineColor(sf::Color(0, 255, 0, static_cast<std::uint8_t>(150 + pulse * 105)));
                    target.draw(shape);
                }

                sf::Text text(font);
                text.setString(slot.cardName);
                text.setCharacterSize(12);
                text.setFillColor(sf::Color::Black);
                sf::FloatRect textBounds = text.getLocalBounds();
                text.setOrigin({textBounds.position.x + textBounds.size.x / 2.f, textBounds.position.y + textBounds.size.y / 2.f});
                text.setPosition(slot.position + slot.size / 2.f);
                target.draw(text);
            }
        }

        std::vector<const CardInfo*> renderOrder;
        for (const auto& card : cards) renderOrder.push_back(&card);

        std::sort(renderOrder.begin(), renderOrder.end(), [](const auto* a, const auto* b) {
            if (a->isDragging != b->isDragging) return !a->isDragging; 
            if (a->isHovered != b->isHovered) return !a->isHovered;
            return a->index < b->index;
        });

        for (const auto* cardPtr : renderOrder) {
            const auto& card = *cardPtr;
            
            float scale = 1.0f + (card.hoverProgress * 0.6f); // [SCALE] Increased from 0.4 to 0.6 for 1.6x zoom
            float lift = card.hoverProgress * 120.f; 
            float animatedRotation = card.rotation * (1.0f - card.hoverProgress);

            sf::Vector2f origin = {card.size.x / 2.f, card.size.y};
            sf::Vector2f renderPos = card.position + sf::Vector2f(card.size.x / 2.f, card.size.y);
            renderPos.y -= lift;

            // [SHADOW]
            sf::RectangleShape shadow(card.size);
            shadow.setOrigin(origin);
            shadow.setPosition(renderPos + sf::Vector2f(4.f + card.hoverProgress * 8.f, 4.f + card.hoverProgress * 8.f));
            shadow.setScale({scale, scale});
            shadow.setRotation(sf::degrees(animatedRotation));
            shadow.setFillColor(sf::Color(0, 0, 0, static_cast<std::uint8_t>(60 + card.hoverProgress * 60)));
            target.draw(shadow);

            // [GLOW-OUTLINE] Soft outer glow for the focused or dragged card
            if ((card.isHovered || card.isDragging) && card.hoverProgress > 0.05f) {
                float glowAlpha = 150.f * std::max(card.hoverProgress, 0.5f); 
                float pulse = (std::sin(totalTime * 6.f) + 1.f) * 0.5f;
                float padding = 3.f + (pulse * 2.f);
                
                sf::RectangleShape glow(card.size + sf::Vector2f(padding * 2.f, padding * 2.f));
                glow.setOrigin(origin + sf::Vector2f(padding, padding));
                glow.setPosition(renderPos);
                glow.setScale({scale, scale});
                glow.setRotation(sf::degrees(animatedRotation));
                glow.setFillColor(sf::Color(255, 255, 255, static_cast<std::uint8_t>(glowAlpha * 0.4f)));
                target.draw(glow);
            }

            sf::RectangleShape shape(card.size);
            shape.setOrigin(origin);
            shape.setPosition(renderPos);
            shape.setScale({scale, scale});
            shape.setRotation(sf::degrees(animatedRotation));
            
            sf::Color baseColor = sf::Color(245, 245, 245); // Consistent ivory base
            
            if (card.isInsideTarget) {
                baseColor.a = 120;
            } else {
                baseColor.a = 255;
            }

            shape.setFillColor(baseColor);
            
            // [OUTLINE] Stronger white outline for hovered or dragged card
            bool isActive = card.isHovered || card.isDragging;
            float outlineThickness = isActive ? (3.f / scale) : (2.f / scale);
            shape.setOutlineThickness(outlineThickness);
            shape.setOutlineColor(isActive ? sf::Color::White : sf::Color(0, 0, 0, baseColor.a));
            
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

            // [DIMMING] Individual dimming based on dimProgress (Alpha 0 to 80)
            if (card.dimProgress > 0.01f && !card.isDragging) {
                sf::RectangleShape dimOverlay(card.size);
                dimOverlay.setOrigin(origin);
                dimOverlay.setPosition(renderPos);
                dimOverlay.setScale({scale, scale});
                dimOverlay.setRotation(sf::degrees(animatedRotation));
                dimOverlay.setFillColor(sf::Color(0, 0, 0, static_cast<std::uint8_t>(card.dimProgress * 80.f))); 
                target.draw(dimOverlay);
            }
        }
    }

    static void renderWorldUI(entt::registry& registry, sf::RenderTarget& target) {
        // [FIXED] Only render health bar for the Statue
        // Show bar if health is not full OR if hit recently (using hitFlashTimer as a proxy or health state)
        auto statueView = registry.view<component::Transform, component::StatueStats>();
        statueView.each([&](auto entity, auto& trans, auto& stats) {
            // [VISIBILITY-LOGIC] Show if damaged OR if hit flash is active (indicating recent combat)
            if (stats.currentHealth < stats.maxHealth || stats.hitFlashTimer > 0.f) {
                // [DYNAMIC-POS] Use vfxHeight from config + small padding
                float offsetY = -(stats.vfxHeight + 10.f); 
                drawHealthBar(target, trans.position, stats.currentHealth, stats.maxHealth, {120.f, 8.f}, offsetY);
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
