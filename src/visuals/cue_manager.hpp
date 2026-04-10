#pragma once

#include <vector>
#include <memory>
#include "ivirtual_cue.hpp"

// Forward declaration of available cues
enum class CueType {
    SINGLE_DOT,
    GRID,
    HORIZON,
    LIQUID
};

class CueManager {
public:
    CueManager();
    ~CueManager();

    // Swaps the active visual manifestation
    void SwitchCue(CueType type);

    // Lifecycle triggers sent from main.cpp
    void UpdateActiveCue(MotionOffset current_offset, float dt);
    void DrawActiveCue(int width, int height);

private:
    std::unique_ptr<IVisualCue> m_activeCue;
};
