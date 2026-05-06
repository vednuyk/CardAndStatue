#pragma once
#include <SFML/Graphics.hpp>
#include <entt/entt.hpp>
#include <cmath>
#include <algorithm>
#include <random>
#include <cstdint>
#include <map>
#include <vector>
#include "../components/unit_components.hpp"

#include "tilemap_system.hpp"

class RenderSystem {
public:
    static void render(entt::registry& registry, sf::RenderTarget& target, const std::map<component::TextureID, const sf::Texture*>& textureMap, const TileMap& tileMap) {
        target.draw(tileMap); // 1. 지형 렌더링
        
        renderEntities(registry, target, textureMap); // 2. 모든 엔티티 통합 배치 렌더링
    }

private:
    static void renderEntities(entt::registry& registry, sf::RenderTarget& target, const std::map<component::TextureID, const sf::Texture*>& textureMap) {
        struct RenderNode {
            entt::entity entity;
            float depth;
            const sf::Texture* texture;
            sf::FloatRect texRect;
            sf::Vector2f halfSize;
        };
        static std::vector<RenderNode> renderNodes;
        renderNodes.clear();

        auto view = registry.view<component::Transform, component::SpriteData>();
        renderNodes.reserve(view.size_hint());

        for (auto entity : view) {
            auto& transform = view.get<component::Transform>(entity);
            auto& spriteData = view.get<component::SpriteData>(entity);

            auto it = textureMap.find(spriteData.textureID);
            if (it == textureMap.end()) continue;

            const sf::Texture* tex = it->second;

            // [NEW] Hit Flash Override
            if (auto* stats = registry.try_get<component::UnitStats>(entity)) {
                if (stats->hitFlashTimer > 0.f) {
                    auto flashIt = textureMap.find(component::TextureID::WhiteFlash);
                    if (flashIt != textureMap.end()) {
                        tex = flashIt->second;
                    }
                }
            }

            sf::FloatRect texRect = spriteData.textureRect;
            if (texRect.size.x == 0) texRect = sf::FloatRect({0.f, 0.f}, sf::Vector2f(tex->getSize()));

            float hw = (texRect.size.x * spriteData.scale.x) / 2.f;
            float hh = (texRect.size.y * spriteData.scale.y) / 2.f;
            
            // Depth = Foot Y position by default
            float depth = transform.position.y + hh;

            // [NEW] Rendering Priority for Skills
            if (registry.any_of<component::OrbitalSphere>(entity)) {
                depth += 100000.f; // Force to top
            }

            renderNodes.push_back({entity, depth, tex, texRect, {hw, hh}});
        }

        // 1. 전역 정렬 (Foot Y)
        std::sort(renderNodes.begin(), renderNodes.end(), [](const auto& a, const auto& b) {
            return a.depth < b.depth;
        });

        const sf::Texture* currentTexture = nullptr;
        static std::vector<sf::Vertex> vertexBuffer;
        vertexBuffer.clear();
        vertexBuffer.reserve(renderNodes.size() * 6); // 1 Sprite = 2 Triangles (6 Vertices)

        auto flush = [&]() {
            if (currentTexture != nullptr && !vertexBuffer.empty()) {
                target.draw(vertexBuffer.data(), vertexBuffer.size(), sf::PrimitiveType::Triangles, currentTexture);
                vertexBuffer.clear();
            }
        };

        for (const auto& node : renderNodes) {
            auto& transform = registry.get<component::Transform>(node.entity);
            auto& spriteData = registry.get<component::SpriteData>(node.entity);

            // Texture가 바뀌면 이전 배치를 그리고 새로 시작
            if (node.texture != currentTexture) {
                flush();
                currentTexture = node.texture;
            }

            float hw = node.halfSize.x;
            float hh = node.halfSize.y;
            if (spriteData.flipX) hw = -hw; 

            sf::Transform combinedTransform;
            combinedTransform.translate(transform.position).rotate(sf::degrees(transform.rotation));

            sf::Vector2f p1 = combinedTransform.transformPoint({-hw, -hh});
            sf::Vector2f p2 = combinedTransform.transformPoint({hw, -hh});
            sf::Vector2f p3 = combinedTransform.transformPoint({hw, hh});
            sf::Vector2f p4 = combinedTransform.transformPoint({-hw, hh});

            const auto& tr = node.texRect;
            vertexBuffer.push_back(sf::Vertex(p1, sf::Color::White, {tr.position.x, tr.position.y}));
            vertexBuffer.push_back(sf::Vertex(p2, sf::Color::White, {tr.position.x + tr.size.x, tr.position.y}));
            vertexBuffer.push_back(sf::Vertex(p3, sf::Color::White, {tr.position.x + tr.size.x, tr.position.y + tr.size.y}));
            
            vertexBuffer.push_back(sf::Vertex(p1, sf::Color::White, {tr.position.x, tr.position.y}));
            vertexBuffer.push_back(sf::Vertex(p3, sf::Color::White, {tr.position.x + tr.size.x, tr.position.y + tr.size.y}));
            vertexBuffer.push_back(sf::Vertex(p4, sf::Color::White, {tr.position.x, tr.position.y + tr.size.y}));
        }

        flush();
    }

private:
};
