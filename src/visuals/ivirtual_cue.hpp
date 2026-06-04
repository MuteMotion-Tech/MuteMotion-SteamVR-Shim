#pragma once

#include "../vr_sensor_bridge.hpp" // For MotionOffset Definition

// renderer params sent by the control panel ui via ipc
struct CueParams {
    float intensity;    // vibe multiplier (0.0 = boring, 1.0 = normal, 2.0 = bro chill)
    float opacity;      // alpha (0.0 = ghosted, 1.0 = thicc)
    bool invertAxis;    // true = inverted controls (skill issue), false = chad
    int skinId = 0;     // 0 = default ez, 1 = nyan cat premium swag
};

class IVisualCue {
public:
    virtual ~IVisualCue() = default;

    // called right before the render frame.
    // used to update physics and math shenanigans
    virtual void Update(MotionOffset current_offset, float dt, const CueParams& params) = 0;

    // called in the middle of the texture generation.
    // put your raylib spaghetti code here
    virtual void Draw(int overlay_width, int overlay_height) = 0;
};
