#pragma once
#include "ivirtual_cue.hpp"
#include <vector>
#include <cmath>

// =============================================================================
// Critically Damped Spring — exact ODE solution (ζ=1.0, ω=20 rad/s)
// Ported from the Canvas 2D Steam Deck implementation.
// Unconditionally stable regardless of frame delta.
// =============================================================================
struct SpringState1D {
    float pos;
    float vel;
};

static inline SpringState1D updateSpring(float pos, float vel, float target, float dt, float omega = 20.0f) {
    if (dt > 0.1f) dt = 0.1f;
    float x0 = pos - target;
    float v0 = vel;
    float e = expf(-omega * dt);
    float term = (v0 + omega * x0) * dt;
    return {
        target + (x0 + term) * e,
        (v0 - omega * term) * e,
    };
}

// Trail point for ghost rendering
struct TrailPoint {
    float x, y;
};

// =============================================================================
// 1. BALL VIEW — Single glowing dot with spring physics + ghost trail
// =============================================================================
class SingleDotCue : public IVisualCue {
public:
    SingleDotCue();
    void Update(MotionOffset current_offset, float dt, const CueParams& params) override;
    void Draw(int width, int height) override;
private:
    static constexpr float OMEGA = 20.0f;
    static constexpr float GAIN_X = 16.0f;
    static constexpr float GAIN_Y = 24.0f;
    static constexpr float MAX_TRAVEL_X = 400.0f;
    static constexpr float MAX_TRAVEL_Y = 250.0f;
    static constexpr float DOT_RADIUS = 15.0f;
    static constexpr int TRAIL_LENGTH = 10;

    float m_posX, m_posY;
    float m_velX, m_velY;
    float m_targetX, m_targetY;
    float m_opacity;
    std::vector<TrailPoint> m_trail;
    bool m_initialized;
};

// =============================================================================
// 2. DOT GRID VIEW — Peripheral anchors (4 left + 4 right) with spring trails
// =============================================================================
struct DotSpring {
    float posX, posY;
    float velX, velY;
    std::vector<TrailPoint> trail;
};

class GridCue : public IVisualCue {
public:
    GridCue();
    void Update(MotionOffset current_offset, float dt, const CueParams& params) override;
    void Draw(int width, int height) override;
private:
    static constexpr float OMEGA = 20.0f;
    static constexpr float GAIN_X = 12.0f;
    static constexpr float GAIN_Y = 24.0f;
    static constexpr float MAX_TRAVEL_X = 30.0f;
    static constexpr float MAX_TRAVEL_Y = 90.0f;
    static constexpr float DOT_RADIUS = 5.0f;
    static constexpr float MARGIN_PERCENT = 0.05f;
    static constexpr int DOT_COUNT = 4;       // per side
    static constexpr int TRAIL_LENGTH = 8;

    DotSpring m_springs[8]; // 4 left + 4 right
    float m_targetX, m_targetY;
    float m_opacity;
    bool m_initialized;
};

// =============================================================================
// 3. HORIZON BAR — Rotating attitude indicator with spring + ghost trails
// =============================================================================
struct HorizonTrailPoint {
    float roll, pitch;
};

class ArtificialHorizonCue : public IVisualCue {
public:
    ArtificialHorizonCue();
    void Update(MotionOffset current_offset, float dt, const CueParams& params) override;
    void Draw(int width, int height) override;
private:
    static constexpr float OMEGA = 20.0f;
    static constexpr float GAIN_ROLL = 1.5f;
    static constexpr float GAIN_PITCH = 8.0f;
    static constexpr float MAX_ROLL = 30.0f;
    static constexpr float MAX_PITCH = 240.0f;
    static constexpr int TRAIL_LENGTH = 6;

    float m_posRoll, m_velRoll;
    float m_posPitch, m_velPitch;
    float m_targetRoll, m_targetPitch;
    float m_opacity;
    std::vector<HorizonTrailPoint> m_trail;
};

// =============================================================================
// 4. LIQUID WAVE — Velocity-reactive sine fluid with neon meniscus
// =============================================================================
class LiquidWaveCue : public IVisualCue {
public:
    LiquidWaveCue();
    void Update(MotionOffset current_offset, float dt, const CueParams& params) override;
    void Draw(int width, int height) override;
private:
    static constexpr float OMEGA = 18.0f;  // softer spring for liquid feel
    static constexpr float GAIN_ROLL = 1.5f;
    static constexpr float GAIN_PITCH = 8.0f;
    static constexpr float MAX_ROLL = 45.0f;
    static constexpr float MAX_PITCH = 250.0f;

    float m_posRoll, m_velRoll;
    float m_posPitch, m_velPitch;
    float m_targetRoll, m_targetPitch;
    float m_timeAcc;
    float m_opacity;
};
