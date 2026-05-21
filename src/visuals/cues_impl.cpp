#include "cues_impl.hpp"
#include <raylib.h>
#include <rlgl.h>
#include <math.h>
#include <algorithm>

// =============================================================================
// The MuteMotion Neon Cyan — rgba(0, 255, 204) ported from Canvas 2D
// =============================================================================
static Color NeonCyan(float alpha) {
    return { 0, 255, 204, (unsigned char)(std::max(0.0f, std::min(1.0f, alpha)) * 255.0f) };
}

// =============================================================================
// 1. SINGLE DOT (BallView) — Critically Damped Spring + Ghost Trail
// =============================================================================
SingleDotCue::SingleDotCue()
    : m_posX(0), m_posY(0), m_velX(0), m_velY(0),
      m_targetX(0), m_targetY(0), m_initialized(false), m_opacity(0.8f) {}

void SingleDotCue::Update(MotionOffset offset, float dt, const CueParams& params) {
    float inv = params.invertAxis ? -1.0f : 1.0f;
    float intensity = params.intensity;
    m_opacity = params.opacity;

    // Clamp target from core engine output, scaled by intensity and inversion
    m_targetX = std::max(-MAX_TRAVEL_X, std::min(MAX_TRAVEL_X, (float)offset.x * GAIN_X * intensity * inv));
    m_targetY = std::max(-MAX_TRAVEL_Y, std::min(MAX_TRAVEL_Y, (float)offset.y * GAIN_Y * intensity * inv));

    // Critically damped spring integration
    auto sx = updateSpring(m_posX, m_velX, m_targetX, dt, OMEGA);
    auto sy = updateSpring(m_posY, m_velY, m_targetY, dt, OMEGA);
    m_posX = sx.pos; m_velX = sx.vel;
    m_posY = sy.pos; m_velY = sy.vel;

    // Push current position into ghost trail buffer
    m_trail.push_back({m_posX, m_posY});
    if ((int)m_trail.size() > TRAIL_LENGTH) m_trail.erase(m_trail.begin());
}

void SingleDotCue::Draw(int width, int height) {
    float cx = (width / 2.0f) + m_posX;
    float cy = (height / 2.0f) + m_posY;

    // Draw ghost trail (oldest = most transparent)
    for (int t = 0; t < (int)m_trail.size(); t++) {
        float progress = (float)(t + 1) / (float)m_trail.size();
        float trailAlpha = m_opacity * progress * 0.4f;
        float trailRadius = DOT_RADIUS * progress;
        float tx = (width / 2.0f) + m_trail[t].x;
        float ty = (height / 2.0f) + m_trail[t].y;
        DrawCircle((int)tx, (int)ty, trailRadius, NeonCyan(trailAlpha));
    }

    // Draw glowing main ball
    DrawCircle((int)cx, (int)cy, DOT_RADIUS, NeonCyan(m_opacity));
    // Glow ring (simulates Canvas shadowBlur)
    DrawCircleGradient((int)cx, (int)cy, DOT_RADIUS * 2.5f, NeonCyan(m_opacity * 0.3f), NeonCyan(0.0f));
}

// =============================================================================
// 2. DOT GRID (DotGridView) — Peripheral Anchors with Spring Trails
// =============================================================================
GridCue::GridCue() : m_targetX(0), m_targetY(0), m_initialized(false), m_opacity(0.8f) {
    for (int i = 0; i < 8; i++) {
        m_springs[i] = {0, 0, 0, 0, {}};
    }
}

void GridCue::Update(MotionOffset offset, float dt, const CueParams& params) {
    float inv = params.invertAxis ? -1.0f : 1.0f;
    float intensity = params.intensity;
    m_opacity = params.opacity;

    m_targetX = std::max(-MAX_TRAVEL_X, std::min(MAX_TRAVEL_X, (float)offset.x * GAIN_X * intensity * inv));
    m_targetY = std::max(-MAX_TRAVEL_Y, std::min(MAX_TRAVEL_Y, (float)offset.y * GAIN_Y * intensity * inv));
}

void GridCue::Draw(int width, int height) {
    float w = (float)width;
    float h = (float)height;
    float leftX = w * MARGIN_PERCENT;
    float rightX = w * (1.0f - MARGIN_PERCENT);
    float dt = GetFrameTime();
    if (dt <= 0 || dt > 0.5f) dt = 1.0f / 60.0f;

    // Initialize spring anchors on first frame
    if (!m_initialized) {
        for (int i = 0; i < DOT_COUNT; i++) {
            float anchorY = h * (0.25f + (i * 0.5f) / (DOT_COUNT - 1));
            m_springs[i].posX = leftX;
            m_springs[i].posY = anchorY;
            m_springs[DOT_COUNT + i].posX = rightX;
            m_springs[DOT_COUNT + i].posY = anchorY;
        }
        m_initialized = true;
    }

    for (int i = 0; i < DOT_COUNT; i++) {
        float anchorY = h * (0.25f + (i * 0.5f) / (DOT_COUNT - 1));

        // --- LEFT DOT ---
        DotSpring& ls = m_springs[i];
        float ltx = leftX + m_targetX;
        float lty = anchorY + m_targetY;
        auto lsx = updateSpring(ls.posX, ls.velX, ltx, dt, OMEGA);
        auto lsy = updateSpring(ls.posY, ls.velY, lty, dt, OMEGA);
        ls.posX = lsx.pos; ls.velX = lsx.vel;
        ls.posY = lsy.pos; ls.velY = lsy.vel;

        ls.trail.push_back({ls.posX, ls.posY});
        if ((int)ls.trail.size() > TRAIL_LENGTH) ls.trail.erase(ls.trail.begin());

        // Draw trail
        for (int t = 0; t < (int)ls.trail.size(); t++) {
            float progress = (float)(t + 1) / (float)ls.trail.size();
            float trailAlpha = m_opacity * progress * 0.4f;
            float trailRadius = DOT_RADIUS * progress;
            DrawCircle((int)ls.trail[t].x, (int)ls.trail[t].y, trailRadius, NeonCyan(trailAlpha));
        }
        // Main dot
        DrawCircle((int)ls.posX, (int)ls.posY, DOT_RADIUS, NeonCyan(m_opacity));

        // --- RIGHT DOT ---
        DotSpring& rs = m_springs[DOT_COUNT + i];
        float rtx = rightX + m_targetX;
        float rty = anchorY + m_targetY;
        auto rsx = updateSpring(rs.posX, rs.velX, rtx, dt, OMEGA);
        auto rsy = updateSpring(rs.posY, rs.velY, rty, dt, OMEGA);
        rs.posX = rsx.pos; rs.velX = rsx.vel;
        rs.posY = rsy.pos; rs.velY = rsy.vel;

        rs.trail.push_back({rs.posX, rs.posY});
        if ((int)rs.trail.size() > TRAIL_LENGTH) rs.trail.erase(rs.trail.begin());

        for (int t = 0; t < (int)rs.trail.size(); t++) {
            float progress = (float)(t + 1) / (float)rs.trail.size();
            float trailAlpha = m_opacity * progress * 0.4f;
            float trailRadius = DOT_RADIUS * progress;
            DrawCircle((int)rs.trail[t].x, (int)rs.trail[t].y, trailRadius, NeonCyan(trailAlpha));
        }
        DrawCircle((int)rs.posX, (int)rs.posY, DOT_RADIUS, NeonCyan(m_opacity));
    }
}

// =============================================================================
// 3. HORIZON BAR — Rotating Attitude Indicator with Spring + Ghost Trails
// =============================================================================
ArtificialHorizonCue::ArtificialHorizonCue()
    : m_posRoll(0), m_velRoll(0), m_posPitch(0), m_velPitch(0),
      m_targetRoll(0), m_targetPitch(0), m_opacity(0.8f) {}

void ArtificialHorizonCue::Update(MotionOffset offset, float dt, const CueParams& params) {
    float inv = params.invertAxis ? -1.0f : 1.0f;
    float intensity = params.intensity;
    m_opacity = params.opacity;

    m_targetRoll = std::max(-MAX_ROLL, std::min(MAX_ROLL, (float)offset.x * GAIN_ROLL * intensity * inv));
    m_targetPitch = std::max(-MAX_PITCH, std::min(MAX_PITCH, (float)offset.y * GAIN_PITCH * intensity * inv));

    auto sr = updateSpring(m_posRoll, m_velRoll, m_targetRoll, dt, OMEGA);
    auto sp = updateSpring(m_posPitch, m_velPitch, m_targetPitch, dt, OMEGA);
    m_posRoll = sr.pos; m_velRoll = sr.vel;
    m_posPitch = sp.pos; m_velPitch = sp.vel;

    m_trail.push_back({m_posRoll, m_posPitch});
    if ((int)m_trail.size() > TRAIL_LENGTH) m_trail.erase(m_trail.begin());
}

void ArtificialHorizonCue::Draw(int width, int height) {
    float cx = width / 2.0f;
    float cy = height / 2.0f;
    float barWidth = width * 0.9f;
    float barHeight = 4.0f;

    // Draw ghost trails
    for (int t = 0; t < (int)m_trail.size(); t++) {
        float progress = (float)(t + 1) / (float)m_trail.size();
        float tAlpha = m_opacity * progress * 0.3f;
        float tWidth = barWidth * (0.8f + 0.2f * progress);

        rlPushMatrix();
            rlTranslatef(cx, cy, 0.0f);
            rlRotatef(m_trail[t].roll, 0.0f, 0.0f, 1.0f);
            rlTranslatef(0, m_trail[t].pitch, 0.0f);
            DrawRectangle((int)(-tWidth / 2), (int)(-barHeight / 2), (int)tWidth, (int)barHeight, NeonCyan(tAlpha));
        rlPopMatrix();
    }

    // Draw main bar
    rlPushMatrix();
        rlTranslatef(cx, cy, 0.0f);
        rlRotatef(m_posRoll, 0.0f, 0.0f, 1.0f);
        rlTranslatef(0, m_posPitch, 0.0f);

        // Main bar
        DrawRectangle((int)(-barWidth / 2), (int)(-barHeight / 2), (int)barWidth, (int)barHeight, NeonCyan(m_opacity));
        // Glow effect (wider, dimmer bar behind)
        DrawRectangle((int)(-barWidth / 2), (int)(-barHeight), (int)barWidth, (int)(barHeight * 2), NeonCyan(m_opacity * 0.15f));
    rlPopMatrix();
}

// =============================================================================
// 4. LIQUID WAVE — Velocity-Reactive Sine Fluid with Neon Meniscus
// =============================================================================
LiquidWaveCue::LiquidWaveCue()
    : m_posRoll(0), m_velRoll(0), m_posPitch(0), m_velPitch(0),
      m_targetRoll(0), m_targetPitch(0), m_timeAcc(0), m_opacity(0.8f) {}

void LiquidWaveCue::Update(MotionOffset offset, float dt, const CueParams& params) {
    float inv = params.invertAxis ? -1.0f : 1.0f;
    float intensity = params.intensity;
    m_opacity = params.opacity;

    m_targetRoll = std::max(-MAX_ROLL, std::min(MAX_ROLL, (float)offset.x * GAIN_ROLL * intensity * inv));
    m_targetPitch = std::max(-MAX_PITCH, std::min(MAX_PITCH, (float)offset.y * GAIN_PITCH * intensity * inv));

    auto sr = updateSpring(m_posRoll, m_velRoll, m_targetRoll, dt, OMEGA);
    auto sp = updateSpring(m_posPitch, m_velPitch, m_targetPitch, dt, OMEGA);
    m_posRoll = sr.pos; m_velRoll = sr.vel;
    m_posPitch = sp.pos; m_velPitch = sp.vel;

    m_timeAcc += dt;
}

void LiquidWaveCue::Draw(int width, int height) {
    float cx = width / 2.0f;
    float cy = height / 2.0f;

    // Wave amplitude reacts to rotation velocity (faster = rougher waves)
    float waveAmp = std::min(25.0f, fabsf(m_velRoll) * 0.6f) + 3.0f;
    float phase = m_timeAcc * 4.0f + m_posRoll * 0.08f;

    int numPoints = 80;

    rlPushMatrix();
        rlTranslatef(cx, cy, 0.0f);
        rlRotatef(m_posRoll, 0.0f, 0.0f, 1.0f);
        rlTranslatef(0, m_posPitch, 0.0f);

        // Fluid body (triangle strip from wave surface to bottom)
        float extent = (float)width * 1.5f;
        float halfExt = extent / 2.0f;
        float bodyStep = extent / (numPoints - 1);

        Vector2* strip = new Vector2[numPoints * 2];
        for (int i = 0; i < numPoints; i++) {
            float x = -halfExt + i * bodyStep;
            float y = sinf(x * 0.015f + phase) * waveAmp;
            strip[i * 2]     = { x, y };
            strip[i * 2 + 1] = { x, halfExt }; // down to bottom
        }
        DrawTriangleStrip(strip, numPoints * 2, NeonCyan(m_opacity * 0.35f));
        delete[] strip;

        // Meniscus highlight line — the bright surface edge
        Vector2 prev = { -halfExt, sinf(-halfExt * 0.015f + phase) * waveAmp };
        for (int i = 1; i < numPoints; i++) {
            float x = -halfExt + i * bodyStep;
            float y = sinf(x * 0.015f + phase) * waveAmp;
            Vector2 cur = { x, y };
            DrawLineEx(prev, cur, 4.0f, NeonCyan(m_opacity * 0.9f));
            prev = cur;
        }

        // Subtle glow line (wider, dimmer)
        prev = { -halfExt, sinf(-halfExt * 0.015f + phase) * waveAmp };
        for (int i = 1; i < numPoints; i++) {
            float x = -halfExt + i * bodyStep;
            float y = sinf(x * 0.015f + phase) * waveAmp;
            Vector2 cur = { x, y };
            DrawLineEx(prev, cur, 12.0f, NeonCyan(m_opacity * 0.15f));
            prev = cur;
        }

    rlPopMatrix();
}
