#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <random>
#include <iostream>
#include <array>
#include <cstdint>

// SFML 3.0 기반 고성능 멀티 텍스처 타일맵 시스템
class TileMap : public sf::Drawable, public sf::Transformable {
public:
    enum TileType : uint8_t {
        Grass = 0,
        Flower = 1
    };

    TileMap() 
        : m_grassTexture(nullptr), 
          m_flowerTexture(nullptr), 
          m_tileSize(0, 0), 
          m_width(0), 
          m_height(0) 
    {
        m_grassVertices.setPrimitiveType(sf::PrimitiveType::Triangles);
        m_flowerVertices.setPrimitiveType(sf::PrimitiveType::Triangles);
    }

    bool load(const sf::Texture& grassTex, const sf::Texture& flowerTex, sf::Vector2u tileSize, unsigned int width, unsigned int height) {
        m_grassTexture = &grassTex;
        m_flowerTexture = &flowerTex;
        m_tileSize = tileSize;
        m_width = width;
        m_height = height;

        m_tiles.assign(static_cast<size_t>(width) * height, Grass);
        
        m_grassVertices.setPrimitiveType(sf::PrimitiveType::Triangles);
        m_flowerVertices.setPrimitiveType(sf::PrimitiveType::Triangles);
        
        m_grassVertices.resize(static_cast<size_t>(width) * height * 6);
        m_flowerVertices.clear(); 

        generateInitialTerrain();
        updateAllVertices();

        return true;
    }

private:
    void generateInitialTerrain() {
        std::mt19937 gen(12345);
        std::uniform_int_distribution<int> dist(0, 100);

        for (size_t i = 0; i < m_tiles.size(); ++i) {
            // Flower 배치 확률 (약 8%)
            if (dist(gen) < 8) {
                m_tiles[i] = Flower;
            } else {
                m_tiles[i] = Grass;
            }
        }
    }

    void updateAllVertices() {
        m_grassVertices.clear();
        m_flowerVertices.clear();

        for (unsigned int y = 0; y < m_height; ++y) {
            for (unsigned int x = 0; x < m_width; ++x) {
                TileType type = static_cast<TileType>(m_tiles[x + y * m_width]);
                
                // 각 타일셋(128x128) 내의 4x4 타일 중 하나를 랜덤하게 선택 (Variation)
                uint32_t hash = static_cast<uint32_t>(x * 73856093 ^ y * 19349663);
                sf::Vector2i texCoords((hash % 4) * 32, ((hash / 4) % 4) * 32);

                sf::VertexArray& targetVA = (type == Grass) ? m_grassVertices : m_flowerVertices;

                float left   = static_cast<float>(x * m_tileSize.x);
                float right  = static_cast<float>((x + 1) * m_tileSize.x);
                float top    = static_cast<float>(y * m_tileSize.y);
                float bottom = static_cast<float>((y + 1) * m_tileSize.y);

                float u1 = static_cast<float>(texCoords.x);
                float v1 = static_cast<float>(texCoords.y);
                float u2 = static_cast<float>(texCoords.x + m_tileSize.x);
                float v2 = static_cast<float>(texCoords.y + m_tileSize.y);

                targetVA.append(sf::Vertex({left, top},     sf::Color::White, {u1, v1}));
                targetVA.append(sf::Vertex({right, top},    sf::Color::White, {u2, v1}));
                targetVA.append(sf::Vertex({right, bottom}, sf::Color::White, {u2, v2}));

                targetVA.append(sf::Vertex({left, top},     sf::Color::White, {u1, v1}));
                targetVA.append(sf::Vertex({right, bottom}, sf::Color::White, {u2, v2}));
                targetVA.append(sf::Vertex({left, bottom},  sf::Color::White, {u1, v2}));
            }
        }
    }

    virtual void draw(sf::RenderTarget& target, sf::RenderStates states) const override {
        states.transform *= getTransform();
        
        // Grass 먼저 그리기
        if (m_grassTexture) {
            states.texture = m_grassTexture;
            target.draw(m_grassVertices, states);
        }

        // Flower 나중에 그리기 (위에 덮어씀)
        if (m_flowerTexture) {
            states.texture = m_flowerTexture;
            target.draw(m_flowerVertices, states);
        }
    }

    sf::VertexArray m_grassVertices;
    sf::VertexArray m_flowerVertices;
    const sf::Texture* m_grassTexture;
    const sf::Texture* m_flowerTexture;
    sf::Vector2u m_tileSize;
    unsigned int m_width;
    unsigned int m_height;
    std::vector<uint8_t> m_tiles;
};
