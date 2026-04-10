#pragma once

#include "../vr_sensor_bridge.hpp" // For MotionOffset Definition

class IVisualCue {
public:
    virtual ~IVisualCue() = default;

    // Called once per frame BEFORE drawing. 
    // This allows the cue to update its internal physics, wave states, etc.
    virtual void Update(MotionOffset current_offset, float dt) = 0;

    // Called once per frame DURING the texture generation.
    // Must implement pure Raylib drawing API calls internally.
    virtual void Draw(int overlay_width, int overlay_height) = 0;
};
