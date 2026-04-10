#pragma once
#include "ivirtual_cue.hpp"

// 1. BALL VIEW
class SingleDotCue : public IVisualCue {
public:
    SingleDotCue() : m_offsetX(0), m_offsetY(0) {}
    void Update(MotionOffset current_offset, float dt) override;
    void Draw(int width, int height) override;
private:
    float m_offsetX;
    float m_offsetY;
};

// 2. DOT GRID VIEW
class GridCue : public IVisualCue {
public:
    GridCue() : m_offsetX(0), m_offsetY(0) {}
    void Update(MotionOffset current_offset, float dt) override;
    void Draw(int width, int height) override;
private:
    float m_offsetX;
    float m_offsetY;
};

// 3. HORIZON BAR
class ArtificialHorizonCue : public IVisualCue {
public:
    ArtificialHorizonCue() : m_angle(0.0f), m_targetAngle(0.0f) {}
    void Update(MotionOffset current_offset, float dt) override;
    void Draw(int width, int height) override;
private:
    float m_angle;      // Smooth interpolation state
    float m_targetAngle; // Mapped directly from Core Y output
};

// 4. LIQUID WAVE
class LiquidWaveCue : public IVisualCue {
public:
    LiquidWaveCue();
    void Update(MotionOffset current_offset, float dt) override;
    void Draw(int width, int height) override;
private:
    float m_baseOffset;
    float m_timeAcc; // Sine wave animation runner
};
