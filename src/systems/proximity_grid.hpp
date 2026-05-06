#pragma once
#include <vector>
#include <entt/entt.hpp>
#include <SFML/System/Vector2.hpp>
#include <array>

// [OPTIMIZED] 초경량 고정 그리드 시스템
// 복잡한 해시 없이 단순 인덱싱으로 주변 유닛을 빠르게 탐색
class ProximityGrid {
public:
    static constexpr int CELL_SIZE = 64; // 사거리보다 약간 큰 사이즈
    static constexpr int GRID_SIZE = 128; // -4096 ~ 4096 영역 커버
    static constexpr int OFFSET = GRID_SIZE / 2;

    void clear() {
        for (auto& cell : m_cells) {
            cell.clear();
        }
    }

    void insert(entt::entity entity, sf::Vector2f pos) {
        int x = static_cast<int>(pos.x) / CELL_SIZE + OFFSET;
        int y = static_cast<int>(pos.y) / CELL_SIZE + OFFSET;
        
        if (x >= 0 && x < GRID_SIZE && y >= 0 && y < GRID_SIZE) {
            m_cells[y * GRID_SIZE + x].push_back(entity);
        }
    }

    // 주변 3x3 셀 탐색
    template <typename Func>
    void queryNearby(sf::Vector2f pos, Func func) const {
        int x = static_cast<int>(pos.x) / CELL_SIZE + OFFSET;
        int y = static_cast<int>(pos.y) / CELL_SIZE + OFFSET;

        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                int nx = x + dx;
                int ny = y + dy;
                if (nx >= 0 && nx < GRID_SIZE && ny >= 0 && ny < GRID_SIZE) {
                    const auto& cell = m_cells[ny * GRID_SIZE + nx];
                    for (auto entity : cell) {
                        func(entity);
                    }
                }
            }
        }
    }

private:
    std::vector<std::vector<entt::entity>> m_cells;

public:
    ProximityGrid() {
        m_cells.resize(GRID_SIZE * GRID_SIZE);
    }
};
