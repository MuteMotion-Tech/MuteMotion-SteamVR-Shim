#pragma once
#include "ivirtual_cue.hpp"
#include <vector>
#include <cmath>
#include <raylib.h>

// cursed spring math with exact ODE solution
// because lagging frames is for noobs
// this thing is stable no matter your fps, trust
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

// cute ghost trail point
struct TrailPoint {
    float x, y;
};

enum class SkinType {
    Default,
    NyanCat
};

// 1. BALL VIEW: just a dot with springs and ghost trails. clean and full of rizz.
class SingleDotCue : public IVisualCue {
public:
    SingleDotCue();
    ~SingleDotCue();
    void Update(MotionOffset current_offset, float dt, const CueParams& params) override;
    void Draw(int width, int height) override;
private:
    static constexpr float OMEGA = 16.0f; // big stiffness to snap back faster
    static constexpr float GAIN_X = 80.0f;
    static constexpr float GAIN_Y = 120.0f;
    static constexpr float MAX_TRAVEL_X = 600.0f;
    static constexpr float MAX_TRAVEL_Y = 400.0f;
    static constexpr float DOT_RADIUS = 12.0f;
    static constexpr int TRAIL_LENGTH = 12;
    static constexpr float TRAIL_ALPHA_MAX = 1.0f; // alpha so it's not invisible
    static constexpr float NYAN_SCALE_MULTIPLIER = 2.0f; // big cat
    static constexpr float TRAIL_SCALE_MULTIPLIER = 1.0f; 

    float m_posX, m_posY;
    float m_velX, m_velY;
    float m_targetX, m_targetY;
    float m_opacity;
    std::vector<TrailPoint> m_trail;
    bool m_initialized;
    
    // skins kkk
    SkinType m_activeSkin;
    Texture2D m_texNyanCat;
    Texture2D m_texRainbow;
    bool m_texturesLoaded;
};

// 2. DOT GRID VIEW: peripheral dots with spring trails
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
    // math that nobody understands but if you change it rebuild the app
    static constexpr float GAIN_X = 75.0f;
    static constexpr float GAIN_Y = 125.0f;
    static constexpr float MAX_TRAVEL_X = 120.0f;
    static constexpr float MAX_TRAVEL_Y = 200.0f;
    
    // spring stiffness. make it higher and it returns fast af
    static constexpr float OMEGA = 16.0f; 
    
    static constexpr float DOT_RADIUS = 12.0f;
    static constexpr float MARGIN_PERCENT = 0.25f;
    static constexpr int DOT_COUNT = 3;       // per side
    static constexpr int TRAIL_LENGTH = 12;

    DotSpring m_springs[6]; // 3 left + 3 right
    float m_targetX, m_targetY;
    float m_opacity;
    float m_lastDt;
    bool m_initialized;
};

// 3. HORIZON BAR: literally an airplane dashboard bro
struct HorizonTrailPoint {
    float roll, pitch;
};

class ArtificialHorizonCue : public IVisualCue {
public:
    ArtificialHorizonCue();
    void Update(MotionOffset current_offset, float dt, const CueParams& params) override;
    void Draw(int width, int height) override;
private:
    static constexpr float OMEGA = 10.0f;
    static constexpr float GAIN_ROLL = 8.0f;
    static constexpr float GAIN_PITCH = 40.0f;
    static constexpr float MAX_ROLL = 45.0f;
    static constexpr float MAX_PITCH = 350.0f;
    static constexpr int TRAIL_LENGTH = 6;

    float m_posRoll, m_velRoll;
    float m_posPitch, m_velPitch;
    float m_targetRoll, m_targetPitch;
    float m_opacity;
    std::vector<HorizonTrailPoint> m_trail;
};

// 4. LIQUID WAVE: neon water that reacts to how fast you move your head
class LiquidWaveCue : public IVisualCue {
public:
    LiquidWaveCue();
    void Update(MotionOffset current_offset, float dt, const CueParams& params) override;
    void Draw(int width, int height) override;
private:
    static constexpr float OMEGA = 18.0f;  // softer spring to feel like water
    static constexpr float GAIN_ROLL = 8.0f;
    static constexpr float GAIN_PITCH = 50.0f;
    static constexpr float MAX_ROLL = 60.0f;
    static constexpr float MAX_PITCH = 400.0f;

    float m_posRoll, m_velRoll;
    float m_posPitch, m_velPitch;
    float m_targetRoll, m_targetPitch;
    float m_timeAcc;
    float m_opacity;
};

// 5. PREMIUM: NYAN CAT GRID (still cooking)
class NyanCatCue : public IVisualCue {
public:
    NyanCatCue();
    void Update(MotionOffset current_offset, float dt, const CueParams& params) override;
    void Draw(int width, int height) override;
private:
    float m_posX, m_posY;
};

// 6. PREMIUM: DVD BOUNCE (early 2000s vibes)
class DVDCue : public IVisualCue {
public:
    DVDCue();
    void Update(MotionOffset current_offset, float dt, const CueParams& params) override;
    void Draw(int width, int height) override;
private:
    float m_posX, m_posY;
};
