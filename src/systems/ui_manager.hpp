#pragma once
#include <vector>
#include <memory>
#include <string>

namespace ui {

enum class UIState {
    HUD,        // Base game UI (Cards, Passive Slots)
    PauseMenu,  // Overlay menu
    Drafting,   // Full-screen card selection
    Count
};

class UIManager {
public:
    UIManager() {
        m_stateStack.push_back(UIState::HUD);
    }

    void pushState(UIState state) {
        m_stateStack.push_back(state);
    }

    void popState() {
        if (m_stateStack.size() > 1) {
            m_stateStack.pop_back();
        }
    }

    UIState getCurrentState() const {
        return m_stateStack.back();
    }

    bool isStateActive(UIState state) const {
        for (auto s : m_stateStack) {
            if (s == state) return true;
        }
        return false;
    }

    // Check if a specific state is allowed to consume input (must be at the top)
    bool canConsumeInput(UIState state) const {
        return getCurrentState() == state;
    }

private:
    std::vector<UIState> m_stateStack;
};

} // namespace ui
