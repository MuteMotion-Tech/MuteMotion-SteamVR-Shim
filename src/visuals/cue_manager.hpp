#pragma once

#include <vector>
#include <memory>
#include "ivirtual_cue.hpp"

// declaration of all the cues the user can pick
enum class CueType {
    UNINITIALIZED = -1,
    SINGLE_DOT = 0,
    GRID,
    HORIZON,
    LIQUID,
    NYAN_CAT,    // premium stuff
    DVD_BOUNCE   // early 2000s vibes
};

class CueManager {
public:
    CueManager();
    ~CueManager();

    // switches the whole vibe to the new cue
    void SwitchCue(CueType type);

    // lifecycle triggers sent straight from the main loop
    void UpdateActiveCue(MotionOffset current_offset, float dt, const CueParams& params);
    void DrawActiveCue(int width, int height);

private:
    std::unique_ptr<IVisualCue> m_activeCue;
    CueType m_activeCueType = CueType::UNINITIALIZED;
};
