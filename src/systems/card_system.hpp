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

namespace ui {

struct Card {
    std::string nameKey;
    sf::Vector2f position;
    sf::Vector2f size{100.f, 140.f};
    float rotation = 0.f;
    bool isDragging = false;
    float hoverProgress = 0.f;
    bool isHovered = false;
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
        for (int i = 0; i < 4; ++i) {
            m_passiveSlots.push_back(PassiveSlot{});
        }
        updateLayout();
    }

    void updateInput(const ui::UIState currentUIState, const input::InputState& input, const sf::RenderWindow& window, const sf::View& gameView, const entt::registry& registry, sf::Vector2f& dropZoneOut) {
        if (currentUIState != ui::UIState::HUD) return;

        if (input.isLeftPressed) {
            for (auto& card : m_cards) {
                if (card.isHovered) {
                    card.position = input.mouseUIPos - card.size / 2.f;
                    card.hoverProgress = 0.f;
                    card.isHovered = false;
                    card.isDragging = true;
                    m_draggedCard = &card;
                    m_dragOffset = card.position - input.mouseUIPos;
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
                sf::Vector2f dropZoneCenter = m_logicalRes / 2.f;
                auto statueView = registry.view<component::StatueTag, component::Transform>();
                if (statueView.begin() != statueView.end()) {
                    auto worldPos = registry.get<component::Transform>(statueView.front()).position;
                    // Use a const-safe way to map coords if possible, or keep as is if window is non-const in main
                    dropZoneCenter = const_cast<sf::RenderWindow&>(window).mapPixelToCoords(const_cast<sf::RenderWindow&>(window).mapCoordsToPixel(worldPos, gameView), window.getView());
                }
                dropZoneOut = dropZoneCenter;
                sf::Vector2f distVec = input.mouseUIPos - dropZoneCenter;
                if (distVec.x * distVec.x + distVec.y * distVec.y < 150.f * 150.f) {
                    m_pendingUse = true;
                    m_lastUsedCardKey = m_draggedCard->nameKey;
                } else {
                    m_draggedCard->isDragging = false;
                    updateLayout();
                }
            }
            m_draggedCard = nullptr;
        }

        if (m_draggedCard && input.isLeftDown) {
            m_draggedCard->position = input.mouseUIPos + m_dragOffset;
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
            for (auto& c : m_cards) {
                ui::LayoutInfo baseLayout = { c.position, c.rotation };
                ui::LayoutInfo hoveredLayout = ui::CardUILayoutSystem::calculateHoverEffect(baseLayout, c.size, c.hoverProgress);
                
                float scale = 1.0f + (c.hoverProgress * 0.4f);
                sf::Vector2f animatedSize = c.size * scale;
                sf::FloatRect animatedBox(hoveredLayout.position, animatedSize);
                sf::FloatRect baseBox(c.position, c.size);
                
                if (animatedBox.contains(input.mouseUIPos) || baseBox.contains(input.mouseUIPos)) {
                    sf::Vector2f baseCenter = c.position + c.size / 2.f;
                    sf::Vector2f diff = input.mouseUIPos - baseCenter;
                    float distSq = diff.x * diff.x + diff.y * diff.y;
                    float score = distSq;
                    if (c.isHovered) score *= 0.5f;
                    if (score < minScore) {
                        minScore = score;
                        topHoveredCard = &c;
                    }
                }
            }
        }
        for (auto& card : m_cards) {
            card.isHovered = (&card == topHoveredCard);
            float targetProgress = card.isHovered ? 1.f : 0.f;
            card.hoverProgress += (targetProgress - card.hoverProgress) * deltaTime * 10.f;
        }
    }

    void render(entt::registry& registry, sf::RenderWindow& window, const sf::View& gameView, const sf::View& uiView, const sf::Font& font, float totalTime) {
        window.setView(uiView);
        std::vector<UISystem::CardInfo> cardInfos;
        sf::Vector2f dropZoneCenter = m_logicalRes / 2.f;
        auto statueView = registry.view<component::StatueTag, component::Transform>();
        if (statueView.begin() != statueView.end()) {
            auto worldPos = registry.get<component::Transform>(statueView.front()).position;
            dropZoneCenter = window.mapPixelToCoords(window.mapCoordsToPixel(worldPos, gameView), uiView);      
        }
        auto& l10n = LocalizationManager::getInstance();
        for (int i = 0; i < (int)m_cards.size(); ++i) {
            const auto& c = m_cards[i];
            bool isInsideTarget = false;
            if (c.isDragging) {
                sf::Vector2i mPos = sf::Mouse::getPosition(window);
                sf::Vector2f uiMPos = window.mapPixelToCoords(mPos, uiView);
                sf::Vector2f distVec = uiMPos - dropZoneCenter;
                if (distVec.x * distVec.x + distVec.y * distVec.y < 150.f * 150.f) {
                    isInsideTarget = true;
                }
                for (const auto& slot : m_passiveSlots) {
                    if (!slot.isOccupied && sf::FloatRect(slot.position, slot.size).contains(uiMPos)) {
                        isInsideTarget = true;
                        break;
                    }
                }
            }
            cardInfos.push_back({l10n.get(c.nameKey), c.position, c.size, c.rotation, c.isDragging, c.hoverProgress, c.isHovered, i, isInsideTarget});
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
            passiveInfos.push_back({
                slot.isOccupied ? l10n.get(slot.cardNameKey) : sf::String(""),
                slot.position, slot.size, slot.isOccupied, isActive, progress
            });
        }
        UISystem::render(registry, window, gameView, cardInfos, passiveInfos, font, totalTime);
    }

    bool consumePendingUse() {
        bool val = m_pendingUse;
        m_pendingUse = false;
        return val;
    }

    std::string getLastUsedCardKey() const {
        return m_lastUsedCardKey;
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
            float arcRadius = 1200.f; // [TWEAKED] Slightly flatter arc
            float angleStep = 4.0f - t * 0.5f; // [TWEAKED] More overlap than original 5.8f
            sf::Vector2f handAnchor(m_logicalRes.x / 2.f, m_logicalRes.y + 5.f); // [TWEAKED] Raised up (Original +20f -> +5f)
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
        float margin = 20.f;
        float spacing = 10.f;
        float cardW = 100.f;
        float cardH = 140.f;
        for (int i = 0; i < (int)m_passiveSlots.size(); ++i) {
            int row = i / 2;
            int col = i % 2;
            float x = m_logicalRes.x - margin - (2 - col) * (cardW + spacing);
            float y = margin + row * (cardH + spacing);
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
    bool m_pendingUse = false;
    std::string m_lastUsedCardKey;
    bool m_pendingPassiveDrop = false;
    std::vector<std::string> m_triggeredPassiveSkills;
};

} // namespace ui
