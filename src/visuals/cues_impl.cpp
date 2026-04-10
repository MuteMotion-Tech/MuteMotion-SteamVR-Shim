#include "cues_impl.hpp"
#include <raylib.h>
#include <rlgl.h>
#include <math.h>

// ------------------------------------
// SINGLE DOT (The simplest implementation)
// ------------------------------------
void SingleDotCue::Update(MotionOffset offset, float dt) {
    // Map the mathematically tiny core offset to display pixel space (e.g. 50x multiplier)
    m_offsetX = static_cast<float>(offset.x) * 50.0f;
    m_offsetY = static_cast<float>(offset.y) * 50.0f;
}

void SingleDotCue::Draw(int width, int height) {
    float cx = (width / 2.0f) + m_offsetX;
    float cy = (height / 2.0f) + m_offsetY;
    
    // Draw a fading edge circle typical of UI motion anchors
    DrawCircleGradient(static_cast<int>(cx), static_cast<int>(cy), 25.0f, Fade(SKYBLUE, 0.9f), Fade(SKYBLUE, 0.0f));
}

// ------------------------------------
// GRID CUE 
// ------------------------------------
void GridCue::Update(MotionOffset offset, float dt) {
    // Larger multiplier for the grid feeling
    m_offsetX = static_cast<float>(offset.x) * 100.0f;
    m_offsetY = static_cast<float>(offset.y) * 100.0f;
}

void GridCue::Draw(int width, int height) {
    const int dotSpacing = 120;
    
    // Using modulo to allow infinite wrapping of the grid out of frame
    float startX = fmod(m_offsetX, dotSpacing) - dotSpacing;
    float startY = fmod(m_offsetY, dotSpacing) - dotSpacing;
    
    for (float x = startX; x < width + dotSpacing; x += dotSpacing) {
        for (float y = startY; y < height + dotSpacing; y += dotSpacing) {
            DrawCircle(static_cast<int>(x), static_cast<int>(y), 8.0f, Fade(WHITE, 0.4f));
        }
    }
}

// ------------------------------------
// HORIZON BAR
// ------------------------------------
void ArtificialHorizonCue::Update(MotionOffset offset, float dt) {
    // In VR, the offset roughly maps to radians, we'll magnify it for UI scale
    // We bind offset.y to roll/rotation
    m_targetAngle = static_cast<float>(offset.y) * 80.0f; // 80 deg multiplier
    
    // Simple LERP to smooth the bar 
    m_angle += (m_targetAngle - m_angle) * (15.0f * dt);
}

void ArtificialHorizonCue::Draw(int width, int height) {
    float cx = width / 2.0f;
    float cy = height / 2.0f;
    
    rlPushMatrix();
        rlTranslatef(cx, cy, 0.0f);
        rlRotatef(m_angle, 0.0f, 0.0f, 1.0f);
        
        // Draw the horizon line (long rectangle rotating around center)
        // 800px wide, 10px tall
        Rectangle bar = { -400.0f, -5.0f, 800.0f, 10.0f };
        DrawRectangleRec(bar, Fade(LIME, 0.8f));
        
        // Add subtle tick marks like an aircraft ADI indicator
        DrawRectangle(-50, -30, 2, 20, Fade(WHITE, 0.5f));
        DrawRectangle( 50, -30, 2, 20, Fade(WHITE, 0.5f));
        
    rlPopMatrix();
}

// ------------------------------------
// LIQUID WAVE CUE 
// ------------------------------------
LiquidWaveCue::LiquidWaveCue() : m_baseOffset(0), m_timeAcc(0) {}

void LiquidWaveCue::Update(MotionOffset offset, float dt) {
    // Height maps to offset.y, keeping it bounded.
    m_baseOffset = static_cast<float>(offset.y) * 150.0f; 
    
    // Advance the internal fluid clock
    m_timeAcc += dt * 3.0f; // 3.0 frequency multiplier for the wave
}

void LiquidWaveCue::Draw(int width, int height) {
    float centerY = (height / 2.0f) + m_baseOffset;
    
    int numPoints = 100;
    float step = static_cast<float>(width) / (numPoints - 1);
    
    // Draw the fluid using Triangles forming a strip down to the bottom
    // Just like a vertex shader but manually crafted for CPU simplicity
    Vector2* strip = new Vector2[numPoints * 2];
    
    for (int i = 0; i < numPoints; ++i) {
        float x = i * step;
        
        // Standard superimposed sine waves for organic fluid movement
        float wave1 = sinf(x * 0.01f + m_timeAcc) * 20.0f;
        float wave2 = cosf(x * 0.02f + m_timeAcc * 0.8f) * 15.0f;
        
        float y = centerY + wave1 + wave2;
        
        // Top vertex of the strip (the actual wave line)
        strip[i * 2] = { x, y };
        // Bottom vertex of the strip (anchored to the bottom of the texture)
        strip[i * 2 + 1] = { x, static_cast<float>(height) };
    }
    
    DrawTriangleStrip(strip, numPoints * 2, Fade(SKYBLUE, 0.6f));
    
    // Cleanup dynamic allocation
    delete[] strip;
}
