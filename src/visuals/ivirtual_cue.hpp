#pragma once

#include "../vr_sensor_bridge.hpp" // For MotionOffset Definition

// =============================================================================
// Overlay render parameters — propagated from the Control Panel via IPC
// =============================================================================
struct CueParams {
    float intensity;    // Motion multiplier (0.0 = frozen, 1.0 = default, 2.0 = exaggerated)
    float opacity;      // Visual alpha (0.0 = invisible, 1.0 = fully opaque)
    bool invertAxis;    // true = opposite-inertia (Apple style), false = direct tracking
};

class IVisualCue {
public:
    virtual ~IVisualCue() = default;

    // Called once per frame BEFORE drawing. 
    // This allows the cue to update its internal physics, wave states, etc.
    virtual void Update(MotionOffset current_offset, float dt, const CueParams& params) = 0;

    // Called once per frame DURING the texture generation.
    // Must implement pure Raylib drawing API calls internally.
    virtual void Draw(int overlay_width, int overlay_height) = 0;
};
