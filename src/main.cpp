#include <algorithm>
#include <optional>
#include "systems/fps_display.hpp"
#include "systems/wave_system.hpp"
#include "systems/unit_movement_system.hpp"
#include "systems/render_system.hpp"
#include "systems/config_manager.hpp"
#include "components/unit_components.hpp"
#include "systems/tilemap_system.hpp"
#include "systems/entity_factory.hpp"
#include "systems/statue_skill_system.hpp"
#include "systems/unit_combat_system.hpp"
#include "systems/proximity_grid.hpp"
#include "systems/ui_system.hpp"

// [FIX] 뷰포트 비율 유지 로직 개선
static void updateView(sf::RenderWindow& window, sf::View& view) {
    sf::Vector2u size = window.getSize();
    float windowRatio = static_cast<float>(size.x) / static_cast<float>(size.y);
    float viewRatio = 1280.f / 720.f; // 기준 비율 16:9
    float sizeX = 1.f, sizeY = 1.f, posX = 0.f, posY = 0.f;

    if (windowRatio > viewRatio) {
        sizeX = viewRatio / windowRatio;
        posX = (1.f - sizeX) / 2.f;
    } else {
        sizeY = windowRatio / viewRatio;
        posY = (1.f - sizeY) / 2.f;
    }
    view.setViewport(sf::FloatRect({posX, posY}, {sizeX, sizeY}));
}

int main() {
    config::ConfigManager configMgr;
    if (!configMgr.loadAll()) {
        std::cerr << "Critical Error: Failed to load configurations!" << std::endl;
    }

    const sf::Vector2f logicalRes(1280.f, 720.f);
    sf::View gameView;
    gameView.setSize(logicalRes);
    gameView.setCenter(logicalRes / 2.f); 

    sf::View uiView; // [NEW] Dedicated UI View
    uiView.setSize(logicalRes);
    uiView.setCenter(logicalRes / 2.f);

    sf::ContextSettings settings;
    settings.antiAliasingLevel = 8; 

    sf::RenderWindow window(sf::VideoMode::getDesktopMode(), "Earth Defense - Mega Optimized", sf::Style::Default, sf::State::Fullscreen, settings);
    window.setVerticalSyncEnabled(true);
    
    auto syncViews = [&](sf::RenderWindow& win) {
        updateView(win, gameView);
        updateView(win, uiView); // Sync UI viewport too
    };
    syncViews(window);

    // [OPTIMIZED] TextureID 기반 맵핑
    std::map<component::TextureID, const sf::Texture*> textureMap;
    std::vector<std::unique_ptr<sf::Texture>> textureStorage;

    auto loadTex = [&](component::TextureID id, const std::string& path) {
        auto tex = std::make_unique<sf::Texture>();
        if (tex->loadFromFile(path)) {
            tex->setSmooth(false);
            textureMap[id] = tex.get();
            textureStorage.push_back(std::move(tex));
        }
    };

    loadTex(component::TextureID::Statue, "assets/IMG/Player/Statue.png");
    loadTex(component::TextureID::Enemy1, "assets/IMG/Enemy/Enemy1.png");
    loadTex(component::TextureID::Grass, "assets/IMG/BG/Grass.png");
    loadTex(component::TextureID::Flower, "assets/IMG/BG/Flower.png");

    // [NEW] Programmatic Texture Generation
    auto createColoredTex = [&](component::TextureID id, sf::Vector2u size, sf::Color color, bool isCircle = false) {
        sf::Image img(size, color);
        if (isCircle) {
            sf::Vector2f center(static_cast<float>(size.x) / 2.f, static_cast<float>(size.y) / 2.f);
            float radius = std::min(static_cast<float>(size.x), static_cast<float>(size.y)) / 2.f;
            for (unsigned int y = 0; y < size.y; ++y) {
                for (unsigned int x = 0; x < size.x; ++x) {
                    sf::Vector2f pos(static_cast<float>(x), static_cast<float>(y));
                    float distSq = (pos.x - center.x) * (pos.x - center.x) + (pos.y - center.y) * (pos.y - center.y);
                    if (distSq > radius * radius) {
                        img.setPixel({x, y}, sf::Color::Transparent);
                    }
                }
            }
        }
        auto tex = std::make_unique<sf::Texture>();
        if (tex->loadFromImage(img)) {
            textureMap[id] = tex.get();
            textureStorage.push_back(std::move(tex));
        }
    };

    createColoredTex(component::TextureID::FallbackRedSquare, {32, 32}, sf::Color::Red);
    createColoredTex(component::TextureID::RosarySphere, {32, 32}, sf::Color::White, true);
    createColoredTex(component::TextureID::WhiteFlash, {32, 32}, sf::Color::White); // [NEW] White Flash Texture

    sf::Font font;
    if (!font.openFromFile("C:/Windows/Fonts/malgun.ttf")) {
        std::cerr << "Warning: Could not load malgun.ttf" << std::endl;
    }

    struct Card {
        sf::String name;
        sf::Vector2f position;
        sf::Vector2f size{100.f, 140.f};
        float rotation = 0.f; 
        bool isDragging = false;
        
        // [NEW] Hover Animation State
        float hoverProgress = 0.f; // 0.0 to 1.0
        bool isHovered = false;
    };

    std::vector<Card> cards;
    for (int i = 0; i < 8; ++i) {
        cards.push_back({L"수녀의 묵주", {0.f, 0.f}});
    }

    auto updateCardLayout = [&]() {
        int n = static_cast<int>(cards.size());
        if (n == 0) return;

        // [FINAL MICRO-ADJUSTED HEARTHSTONE STYLE]
        float t = std::clamp((static_cast<float>(n) - 2.f) / 8.f, 0.f, 1.f); 
        float arcRadius = 800.f; 
        float angleStep = 5.8f - t * 1.0f; 

        sf::Vector2f handAnchor(logicalRes.x / 2.f, logicalRes.y + 20.f); 
        sf::Vector2f pivot(handAnchor.x, handAnchor.y + arcRadius); 
        
        float totalAngleRange = (n > 1 ? static_cast<float>(n - 1) * angleStep : 0.f);
        float startAngle = -totalAngleRange / 2.f;

        for (int i = 0; i < n; ++i) {
            if (!cards[i].isDragging) {
                float angleDeg = startAngle + i * angleStep;
                float angleRad = angleDeg * (3.14159f / 180.0f);

                float x = pivot.x + arcRadius * std::sin(angleRad);
                float y = pivot.y - arcRadius * std::cos(angleRad);

                cards[i].position = {x - cards[i].size.x / 2.f, y - cards[i].size.y};
                cards[i].rotation = angleDeg;
            } else {
                cards[i].rotation = 0.f; 
            }
        }
    };
    updateCardLayout();

    auto draggedCard = static_cast<Card*>(nullptr);
    sf::Vector2f dragOffset;

    entt::registry registry;
    FPSDisplay fpsDisplay;
    WaveSystem waveSystem(configMgr.waves);
    auto enemyGrid = std::make_unique<ProximityGrid>();
    sf::Clock deltaClock;
    sf::Vector2f center = logicalRes / 2.f; 
    
    TileMap tileMap;
    if (textureMap.count(component::TextureID::Grass) && textureMap.count(component::TextureID::Flower)) {
        tileMap.load(*textureMap.at(component::TextureID::Grass), *textureMap.at(component::TextureID::Flower), {32, 32}, 128, 128);
        tileMap.setPosition({-2048.f + center.x, -2048.f + center.y}); 
    }

    auto statue = EntityFactory::createStatue(registry, center, configMgr.statue);

    float currentZoom = 1.0f;
    bool isFullscreen = true; 

    while (window.isOpen()) {
        float deltaTime = deltaClock.restart().asSeconds();
        if (deltaTime > 0.05f) deltaTime = 0.05f;

        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) window.close();
            if (event->getIf<sf::Event::Resized>()) syncViews(window);

            if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                if (keyPressed->code == sf::Keyboard::Key::F11) {
                    isFullscreen = !isFullscreen;
                    if (isFullscreen) {
                        window.create(sf::VideoMode::getDesktopMode(), "Earth Defense - Mega Optimized", 
                                     sf::Style::Default, sf::State::Fullscreen, settings);
                    } else {
                        window.create(sf::VideoMode({1280, 720}), "Earth Defense - Mega Optimized", 
                                     sf::Style::Default, sf::State::Windowed, settings);
                    }
                    window.setVerticalSyncEnabled(true);
                    syncViews(window);
                }
            }

            // [NEW] Mouse Interaction
            if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (mousePressed->button == sf::Mouse::Button::Left) {
                    // [FIX] Use uiView for UI elements interaction
                    sf::Vector2f uiMousePos = window.mapPixelToCoords(mousePressed->position, uiView);
                    // [FIX] Use dynamic bounding box (consistent with hover detection) to pick card
                    for (int i = static_cast<int>(cards.size()) - 1; i >= 0; --i) {
                        auto& card = cards[i];
                        
                        // Calculate the same animated box as used in the hover logic
                        float scale = 1.0f + (card.hoverProgress * 0.4f);
                        float lift = card.hoverProgress * 80.f;
                        sf::Vector2f animatedSize = card.size * scale;
                        sf::Vector2f animatedPos = {
                            card.position.x + card.size.x / 2.f - animatedSize.x / 2.f,
                            card.position.y - lift + card.size.y - animatedSize.y
                        };

                        if (sf::FloatRect(animatedPos, animatedSize).contains(uiMousePos)) {
                            card.isDragging = true;
                            draggedCard = &card;
                            dragOffset = card.position - uiMousePos;
                            break;
                        }
                    }
                }
            }

            if (const auto* mouseReleased = event->getIf<sf::Event::MouseButtonReleased>()) {
                if (mouseReleased->button == sf::Mouse::Button::Left && draggedCard) {
                    sf::Vector2f uiMousePos = window.mapPixelToCoords(mouseReleased->position, uiView);
                    // Check if dropped in the center area (relative to logical resolution)
                    sf::Vector2f distVec = uiMousePos - (logicalRes / 2.f);
                    if (distVec.x * distVec.x + distVec.y * distVec.y < 150.f * 150.f) {
                        auto cfg = configMgr.skills.rosary;
                        // [SOLID] main.cpp handles only the intent (adding the component)
                        // [NEW] Added duration and remainingTime initialization
                        registry.emplace_or_replace<component::RosarySkill>(statue, false, cfg.damage, cfg.knockbackForce, cfg.rotationSpeed, cfg.radius, cfg.duration, 0.f);

                        cards.erase(std::remove_if(cards.begin(), cards.end(), [&](const Card& c){ return &c == draggedCard; }), cards.end());
                        updateCardLayout();
                    } else {
                        draggedCard->isDragging = false;
                        updateCardLayout();
                    }
                    draggedCard = nullptr;
                }
            }

            if (const auto* mouseMoved = event->getIf<sf::Event::MouseMoved>()) {
                if (draggedCard) {
                    sf::Vector2f uiMousePos = window.mapPixelToCoords(mouseMoved->position, uiView);
                    draggedCard->position = uiMousePos + dragOffset;
                }
            }
        }

        // [OPTIMIZED] 그리드 갱신
        enemyGrid->clear();
        auto enemyView = registry.view<component::EnemyTag, component::Transform>();
        for (auto entity : enemyView) {
            enemyGrid->insert(entity, registry.get<component::Transform>(entity).position);
        }

        waveSystem.update(registry, deltaTime, center, configMgr);
        
        float targetZoom = waveSystem.getCurrentWaveZoom();
        currentZoom += (targetZoom - currentZoom) * deltaTime * 2.0f;
        gameView.setSize(logicalRes * currentZoom);

        StatueSkillSystem::update(registry, deltaTime, *enemyGrid);
        UnitCombatSystem::update(registry, deltaTime, *enemyGrid);
        UnitMovementSystem::update(registry, deltaTime);
        fpsDisplay.update(deltaTime);

        // [FIX] Real-time Hover Detection with Dynamic Bounding Boxes
        sf::Vector2i mousePos = sf::Mouse::getPosition(window);
        sf::Vector2f uiMousePos = window.mapPixelToCoords(mousePos, uiView);
        
        Card* topHoveredCard = nullptr;
        if (!draggedCard) {
            // Find the top-most card under mouse, considering its animated position/size
            for (int i = static_cast<int>(cards.size()) - 1; i >= 0; --i) {
                auto& c = cards[i];
                
                // Mirror the rendering logic for collision
                float scale = 1.0f + (c.hoverProgress * 0.4f);
                float lift = c.hoverProgress * 80.f;
                
                sf::Vector2f animatedSize = c.size * scale;
                // Center the box horizontally around the x-pivot
                sf::Vector2f animatedPos = {
                    c.position.x + c.size.x / 2.f - animatedSize.x / 2.f,
                    c.position.y - lift + c.size.y - animatedSize.y
                };

                if (sf::FloatRect(animatedPos, animatedSize).contains(uiMousePos)) {
                    topHoveredCard = &c;
                    break;
                }
            }
        }

        for (auto& card : cards) {
            card.isHovered = (&card == topHoveredCard);
            
            // Smoothly update hoverProgress
            float targetProgress = card.isHovered ? 1.f : 0.f;
            float animSpeed = 10.f; 
            card.hoverProgress += (targetProgress - card.hoverProgress) * deltaTime * animSpeed;
        }

        window.clear(sf::Color::Black);
        window.setView(gameView);
        RenderSystem::render(registry, window, textureMap, tileMap);
        
        // [NEW] UI 시스템 렌더링 (Screen Space Cards)
        window.setView(uiView); // [FIX] Switch to UI View
        std::vector<UISystem::CardInfo> cardInfos;
        for (int i = 0; i < cards.size(); ++i) {
            const auto& c = cards[i];
            cardInfos.push_back({c.name, c.position, c.size, c.rotation, c.isDragging, c.hoverProgress, c.isHovered, i});
        }
        UISystem::render(registry, window, gameView, cardInfos, font);
        
        window.setView(window.getDefaultView());
        fpsDisplay.draw(window);
        window.display();
    }

    return 0;
}
