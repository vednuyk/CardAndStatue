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
#include "systems/card_system.hpp"
#include "systems/ai_system.hpp"
#include "systems/localization_manager.hpp"

static void updateView(sf::RenderWindow& window, sf::View& view) {
    sf::Vector2u size = window.getSize();
    float windowRatio = static_cast<float>(size.x) / static_cast<float>(size.y);
    float viewRatio = 1280.f / 720.f; 
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
    sf::View gameView(sf::FloatRect({0.f, 0.f}, logicalRes));
    sf::View uiView(sf::FloatRect({0.f, 0.f}, logicalRes));

    sf::ContextSettings settings;
    settings.antiAliasingLevel = 8; 

    sf::RenderWindow window(sf::VideoMode::getDesktopMode(), "Earth Defense - Mega Optimized", sf::Style::Default, sf::State::Fullscreen, settings);
    window.setVerticalSyncEnabled(true);
    
    auto syncViews = [&](sf::RenderWindow& win) {
        updateView(win, gameView);
        updateView(win, uiView);
    };
    syncViews(window);

    std::map<component::TextureID, const sf::Texture*> textureMap;
    std::vector<std::unique_ptr<sf::Texture>> textureStorage;

    auto loadTex = [&](component::TextureID id, const std::string& path) {
        auto tex = std::make_unique<sf::Texture>();
        if (tex->loadFromFile(path)) {
            textureMap[id] = tex.get();
            textureStorage.push_back(std::move(tex));
        }
    };

    loadTex(component::TextureID::Statue, "assets/IMG/Player/Statue.png");
    loadTex(component::TextureID::Enemy1, "assets/IMG/Enemy/Enemy1.png");
    loadTex(component::TextureID::Grass, "assets/IMG/BG/Grass.png");
    loadTex(component::TextureID::Flower, "assets/IMG/BG/Flower.png");

    auto createColoredTex = [&](component::TextureID id, sf::Vector2u size, sf::Color color, bool isCircle = false) {
        sf::Image img(size, color);
        if (isCircle) {
            sf::Vector2f center(static_cast<float>(size.x) / 2.f, static_cast<float>(size.y) / 2.f);
            float radius = std::min(static_cast<float>(size.x), static_cast<float>(size.y)) / 2.f;
            for (unsigned int y = 0; y < size.y; ++y) {
                for (unsigned int x = 0; x < size.x; ++x) {
                    sf::Vector2f pos(static_cast<float>(x), static_cast<float>(y));
                    if ((pos.x - center.x) * (pos.x - center.x) + (pos.y - center.y) * (pos.y - center.y) > radius * radius) {
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
    createColoredTex(component::TextureID::WhiteFlash, {32, 32}, sf::Color::White);

    sf::Font font;
    if (!font.openFromFile("C:/Windows/Fonts/malgun.ttf")) {
        std::cerr << "Warning: Could not load malgun.ttf" << std::endl;
    }

    entt::registry registry;
    ui::CardSystem cardSystem(logicalRes);
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
    float totalTime = 0.f;

    // [L10N] Setup Toggle Button
    sf::Vector2f btnSize(150.f, 40.f);
    sf::RectangleShape langBtn(btnSize);
    langBtn.setFillColor(sf::Color(50, 50, 50, 200));
    langBtn.setOutlineColor(sf::Color::White);
    langBtn.setOutlineThickness(2.f);
    langBtn.setPosition({logicalRes.x - btnSize.x - 20.f, logicalRes.y - btnSize.y - 20.f});

    while (window.isOpen()) {
        float deltaTime = deltaClock.restart().asSeconds();
        if (deltaTime > 0.05f) deltaTime = 0.05f;
        totalTime += deltaTime;

        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) window.close();
            if (event->getIf<sf::Event::Resized>()) syncViews(window);

            if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                if (keyPressed->code == sf::Keyboard::Key::F11) {
                    isFullscreen = !isFullscreen;
                    window.create(isFullscreen ? sf::VideoMode::getDesktopMode() : sf::VideoMode({1280, 720}), 
                                 "Earth Defense", sf::Style::Default, 
                                 isFullscreen ? sf::State::Fullscreen : sf::State::Windowed, settings);
                    window.setVerticalSyncEnabled(true);
                    syncViews(window);
                }
            }

            if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (mousePressed->button == sf::Mouse::Button::Left) {
                    // [FIX] Convert pixel position to UI coordinates explicitly for button
                    sf::Vector2f uiMousePos = window.mapPixelToCoords(mousePressed->position, uiView);
                    if (sf::FloatRect(langBtn.getPosition(), langBtn.getSize()).contains(uiMousePos)) {
                        LocalizationManager::getInstance().toggleLanguage();
                    }
                }
            }

            sf::Vector2f dummyDropZone;
            cardSystem.handleEvent(*event, window, uiView, gameView, registry, dummyDropZone);
        }

        // Logic Updates
        enemyGrid->clear();
        auto enemyView = registry.view<component::EnemyTag, component::Transform>();
        enemyView.each([&](auto entity, auto& trans) {
            enemyGrid->insert(entity, trans.position);
        });

        waveSystem.update(registry, deltaTime, center, configMgr);
        float targetZoom = waveSystem.getCurrentWaveZoom();
        currentZoom += (targetZoom - currentZoom) * deltaTime * 2.0f;
        gameView.setSize(logicalRes * currentZoom);

        StatueSkillSystem::update(registry, deltaTime, *enemyGrid);
        AISystem::update(registry, deltaTime, *enemyGrid); 
        UnitCombatSystem::update(registry, deltaTime, *enemyGrid); 
        UnitMovementSystem::update(registry, deltaTime);
        fpsDisplay.update(deltaTime);
        cardSystem.update(deltaTime, window, uiView);

        if (cardSystem.consumePendingUse()) {
            auto cfg = configMgr.skills.rosary;
            // [FIX] C2397 Narrowing conversion fix: provide all arguments with correct types
            registry.emplace_or_replace<component::RosarySkill>(
                statue, 
                false, // initialized
                false, // isClosing
                cfg.damage, 
                cfg.knockbackForce, 
                cfg.rotationSpeed, 
                cfg.radius, 
                cfg.duration, 
                0.f    // remainingTime
            );
            cardSystem.removeCardUnderMouse();
        }

        // Rendering
        window.clear(sf::Color::Black);
        window.setView(gameView);
        RenderSystem::render(registry, window, textureMap, tileMap);
        
        cardSystem.render(registry, window, gameView, uiView, font, totalTime);
        
        // [L10N] Render Toggle Button
        window.setView(uiView);
        window.draw(langBtn);
        sf::Text btnText(font);
        btnText.setString(LocalizationManager::getInstance().get("LANG_TOGGLE_BTN"));
        btnText.setCharacterSize(16);
        btnText.setFillColor(sf::Color::White);
        sf::FloatRect textBounds = btnText.getLocalBounds();
        btnText.setOrigin({textBounds.position.x + textBounds.size.x / 2.f, textBounds.position.y + textBounds.size.y / 2.f});
        btnText.setPosition(langBtn.getPosition() + btnSize / 2.f);
        window.draw(btnText);

        window.setView(window.getDefaultView());
        fpsDisplay.draw(window);
        window.display();
    }

    return 0;
}
