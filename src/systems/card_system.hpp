#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <algorithm>
#include <cmath>
#include <limits>
#include "ui_system.hpp"
#include "config_manager.hpp"
#include "localization_manager.hpp"
#include "card_ui_layout_system.hpp"
#include "statue_passive_system.hpp"
#include "input_manager.hpp"
#include "ui_manager.hpp"

namespace ui {

struct Card {
    std::string nameKey;
    sf::Vector2f position;
    sf::Vector2f size{80.f, 112.f};
    float rotation = 0.f;
    bool isDragging = false;
    float hoverProgress = 0.f;
    bool isHovered = false;
    float dimProgress = 0.f; // New: 0.0 (bright) to 1.0 (dimmed)
};

struct PassiveSlot {
    std::string cardNameKey;
    sf::Vector2f position;
    sf::Vector2f size{100.f, 140.f};
    bool isOccupied = false;
    float duration = 10.0f;
    float cooldown = 5.0f;
    float timer = 0.0f;
};

class CardSystem {
public:
    CardSystem(sf::Vector2f logicalRes, const config::ConfigManager& configMgr) 
        : m_logicalRes(logicalRes), m_configMgr(configMgr) {
        for (int i = 0; i < 5; ++i) {
            m_cards.push_back({"CARD_ROSARY", {0.f, 0.f}});
        }
        for (int i = 0; i < 3; ++i) {
            m_cards.push_back({"CARD_GOD_RAY", {0.f, 0.f}});
        }
        for (int i = 0; i < 8; ++i) {
            m_passiveSlots.push_back(PassiveSlot{});
        }
        updateLayout();
    }

    void updateInput(const ui::UIState currentUIState, const input::InputState& input, const sf::RenderWindow& window, const sf::View& gameView, const sf::View& uiView, const entt::registry& registry) {
        if (currentUIState != ui::UIState::HUD) return;

        if (input.isLeftPressed) {
            for (auto& card : m_cards) {
                if (card.isHovered) {
                    // [CENTER-PICKING] Snap visual center to mouse immediately
                    float lift = card.hoverProgress * 140.f;
                    card.position = input.mouseUIPos - card.size / 2.f + sf::Vector2f(0.f, lift);
                    
                    card.isHovered = false;
                    card.isDragging = true;
                    m_draggedCard = &card;
                    // No need for m_dragOffset as we always snap to center now
                    break;
                }
            }
        }

        if (input.isLeftReleased && m_draggedCard) {
            bool droppedInPassive = false;
            for (auto& slot : m_passiveSlots) {
                sf::FloatRect slotBounds(slot.position, slot.size);
                if (slotBounds.contains(input.mouseUIPos) && !slot.isOccupied) {
                    slot.cardNameKey = m_draggedCard->nameKey;
                    slot.isOccupied = true;
                    
                    StatuePassiveSystem::addPassive(slot.cardNameKey, m_configMgr);
                    
                    if (slot.cardNameKey == "CARD_ROSARY") {
                        slot.duration = m_configMgr.skills.rosary.duration;
                        slot.cooldown = m_configMgr.skills.rosary.passiveCooldown;
                    } else if (slot.cardNameKey == "CARD_GOD_RAY") {
                        slot.duration = m_configMgr.skills.godRay.duration;
                        slot.cooldown = m_configMgr.skills.godRay.passiveCooldown;
                    }
                    
                    slot.timer = 0.f;
                    m_pendingPassiveDrop = true;
                    droppedInPassive = true;
                    break;
                }
            }

            if (!droppedInPassive) {
                m_draggedCard->isDragging = false;
                updateLayout();
            }
            m_draggedCard = nullptr;
        }

        if (m_draggedCard && input.isLeftDown) {
            // [CENTER-PICKING] Keep visual center at mouse position, accounting for dynamic lift
            float lift = m_draggedCard->hoverProgress * 140.f;
            m_draggedCard->position = input.mouseUIPos - m_draggedCard->size / 2.f + sf::Vector2f(0.f, lift);
        }
    }

    void update(float deltaTime, const input::InputState& input) {
        for (auto& slot : m_passiveSlots) {
            if (slot.isOccupied) {
                slot.timer += deltaTime;
                if (slot.timer >= (slot.duration + slot.cooldown)) {
                    slot.timer = 0.f;
                }
            }
        }

        Card* topHoveredCard = nullptr;
        if (!m_draggedCard) {
            float minScore = std::numeric_limits<float>::max();
            for (int i = (int)m_cards.size() - 1; i >= 0; --i) {
                auto& c = m_cards[i];
                // [OPTIMIZED-HOVER] Performance: O(N) where N is small (max ~10-15 cards)
                float liftAmount = 140.f; 
                float scaleFactor = 1.6f;
                float extraWidth = (c.size.x * (scaleFactor - 1.0f)) / 2.f;
                float extraHeight = c.size.y * (scaleFactor - 1.0f);

                sf::FloatRect baseBox(c.position, c.size);
                sf::FloatRect stableBox(
                    sf::Vector2f(c.position.x - extraWidth, c.position.y - liftAmount - extraHeight),
                    sf::Vector2f(c.size.x + (extraWidth * 2.f), c.size.y + liftAmount + extraHeight)
                );
                
                bool isInsideBase = baseBox.contains(input.mouseUIPos);
                bool isInsideStable = stableBox.contains(input.mouseUIPos);
                bool isInHitbox = c.isHovered ? isInsideStable : isInsideBase;

                if (isInHitbox) {
                    sf::Vector2f baseCenter = c.position + c.size / 2.f;
                    sf::Vector2f diff = input.mouseUIPos - baseCenter;
                    float distSq = diff.x * diff.x + diff.y * diff.y;
                    
                    // [PRIORITY-BOOST] If mouse is inside the actual card body, give it a massive advantage
                    float score = distSq;
                    if (isInsideBase) score *= 0.1f; 
                    
                    if (c.isHovered) score *= 0.8f; // Relaxed hysteresis (0.5 -> 0.8) for snappier switching
                    
                    if (score < minScore) {
                        minScore = score;
                        topHoveredCard = &c;
                    }
                }
            }
        }

        // [FIX-DIM-LOGIC] Determine if any card is active (hovered or dragged)
        m_anyCardHovered = (topHoveredCard != nullptr || m_draggedCard != nullptr);

        for (auto& card : m_cards) {
            card.isHovered = (&card == topHoveredCard);
            
            // [SCALE-LOGIC] 1.0 (normal), 1.6 (hover: 1.0), 1.2 (drag: 0.333)
            float targetProgress = 0.f;
            if (card.isHovered) targetProgress = 1.f;
            else if (card.isDragging) targetProgress = 0.333f; // 1.0 + (0.333 * 0.6) = ~1.2x

            // [SILKY-POP] Tuned Lerp speed to 10.5f
            card.hoverProgress += (targetProgress - card.hoverProgress) * deltaTime * 10.5f;

            // [SMOOTH-DIM]
            bool isThisCardActive = card.isHovered || card.isDragging;
            float dimTarget = (m_anyCardHovered && !isThisCardActive) ? 1.f : 0.f;
            
            if (isThisCardActive) {
                card.dimProgress = 0.f; // Instant recovery for active card
            } else {
                card.dimProgress += (dimTarget - card.dimProgress) * deltaTime * 8.f;
            }
        }
    }

    void render(entt::registry& registry, sf::RenderWindow& window, const sf::View& gameView, const sf::View& uiView, const sf::Font& font, float totalTime) {
        window.setView(uiView);
        std::vector<UISystem::CardInfo> cardInfos;
        
        sf::Vector2i mPos = sf::Mouse::getPosition(window);
        sf::Vector2f uiMPos = window.mapPixelToCoords(mPos, uiView);
        
        auto& l10n = LocalizationManager::getInstance();
        for (int i = 0; i < (int)m_cards.size(); ++i) {
            const auto& c = m_cards[i];
            bool isInsideTarget = false;
            if (c.isDragging) {
                // Check against Passive Slots
                for (const auto& slot : m_passiveSlots) {
                    if (!slot.isOccupied && sf::FloatRect(slot.position, slot.size).contains(uiMPos)) {
                        isInsideTarget = true;
                        break;
                    }
                }
            }
            cardInfos.push_back({l10n.get(c.nameKey), c.position, c.size, c.rotation, c.isDragging, c.hoverProgress, c.isHovered, i, isInsideTarget, c.dimProgress});
        }

        std::vector<UISystem::PassiveSlotInfo> passiveInfos;
        for (const auto& slot : m_passiveSlots) {
            bool isActive = false;
            float progress = 0.f;
            if (slot.isOccupied) {
                if (slot.timer < slot.duration) {
                    isActive = true;
                    progress = slot.timer / slot.duration;
                } else {
                    isActive = false;
                    progress = (slot.timer - slot.duration) / slot.cooldown;
                }
            }
            
            bool isHighlighted = false;
            if (m_draggedCard && !slot.isOccupied) {
                if (sf::FloatRect(slot.position, slot.size).contains(uiMPos)) {
                    isHighlighted = true;
                }
            }

            passiveInfos.push_back({
                slot.isOccupied ? l10n.get(slot.cardNameKey) : sf::String(""),
                slot.position, slot.size, slot.isOccupied, isActive, progress,
                isHighlighted
            });
        }
        UISystem::renderAll(
            registry, 
            window, 
            gameView, 
            cardInfos, 
            passiveInfos, 
            font, 
            totalTime, 
            m_anyCardHovered
        );
    }

    bool consumePendingPassiveDrop() {
        bool val = m_pendingPassiveDrop;
        m_pendingPassiveDrop = false;
        return val;
    }

    std::vector<std::string> popTriggeredPassiveSkills() {
        auto val = std::move(m_triggeredPassiveSkills);
        m_triggeredPassiveSkills.clear();
        return val;
    }

    void removeCardUnderMouse() {
        m_cards.erase(std::remove_if(m_cards.begin(), m_cards.end(), [](const Card& c){ return c.isDragging; }), m_cards.end());
        updateLayout();
    }

    void updateLayout() {
        int n = static_cast<int>(m_cards.size());
        if (n > 0) {
            float t = std::clamp((static_cast<float>(n) - 2.f) / 8.f, 0.f, 1.f);
            float arcRadius = 2200.f; // [GOLDEN] Natural curve
            float angleStep = 2.2f - t * 0.4f; // [GOLDEN] Clean overlap
            sf::Vector2f handAnchor(m_logicalRes.x / 2.f, m_logicalRes.y + 45.f); // [GOLDEN] ~40% visibility (approx 47px)
            sf::Vector2f pivot(handAnchor.x, handAnchor.y + arcRadius);
            float totalAngleRange = (n > 1 ? static_cast<float>(n - 1) * angleStep : 0.f);
            float startAngle = -totalAngleRange / 2.f;
            for (int i = 0; i < n; ++i) {
                if (!m_cards[i].isDragging) {
                    float angleDeg = startAngle + i * angleStep;
                    float angleRad = angleDeg * (3.14159f / 180.0f);
                    float x = pivot.x + arcRadius * std::sin(angleRad);
                    float y = pivot.y - arcRadius * std::cos(angleRad);
                    m_cards[i].position = {x - m_cards[i].size.x / 2.f, y - m_cards[i].size.y};
                    m_cards[i].rotation = angleDeg;
                } else {
                    m_cards[i].rotation = 0.f;
                }
            }
        }
        float cardW = 80.f;
        float cardH = 112.f;
        float spacing = 20.f;
        float totalPanelH = (4 * cardH) + (3 * spacing);
        float marginTop = (m_logicalRes.y - totalPanelH) / 2.f;
        float marginRight = 15.f;

        for (int i = 0; i < (int)m_passiveSlots.size(); ++i) {
            int row = i / 2;
            int col = i % 2;
            float x = m_logicalRes.x - marginRight - (2 - col) * (cardW + spacing);
            float y = marginTop + row * (cardH + spacing);
            m_passiveSlots[i].position = {x, y};
            m_passiveSlots[i].size = {cardW, cardH};
        }
    }

private:
    sf::Vector2f m_logicalRes;
    const config::ConfigManager& m_configMgr; // Keep member
    std::vector<Card> m_cards;
    std::vector<PassiveSlot> m_passiveSlots;
    Card* m_draggedCard = nullptr;
    sf::Vector2f m_dragOffset;
    bool m_pendingPassiveDrop = false;
    std::vector<std::string> m_triggeredPassiveSkills;
    bool m_anyCardHovered = false;
};

} // namespace ui
