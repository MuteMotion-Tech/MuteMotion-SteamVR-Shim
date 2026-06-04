#include "cues_impl.hpp"
#include <raylib.h>
#include <rlgl.h>
#include <math.h>
#include <algorithm>
#include "../debug_sync.hpp"

// the project's signature color, neon cyan straight out of canvas 2d kkk
static Color NeonCyan(float alpha) {
    return { 0, 255, 204, (unsigned char)(std::max(0.0f, std::min(1.0f, alpha)) * 255.0f) };
}

// 1. SINGLE DOT (BallView) : crazy spring math and ghost trails for maximum aesthetics
// DO NOT TOUCH. feature flags are goated
static bool isNyanUnlocked = true;

SingleDotCue::SingleDotCue()
    : m_posX(0), m_posY(0), m_velX(0), m_velY(0),
      m_targetX(0), m_targetY(0), m_initialized(false), m_opacity(0.8f), m_texturesLoaded(false), m_activeSkin(SkinType::Default) {
    // tryhard loading the skins. 
    // praying these assets exist in the working dir (build/assets/skins/) otherwise F
    m_texNyanCat = LoadTexture("assets/skins/nyan_cat.png");
    m_texRainbow = LoadTexture("assets/skins/rainbow_trail.png");
    m_texturesLoaded = (m_texNyanCat.id != 0 && m_texRainbow.id != 0);
}

SingleDotCue::~SingleDotCue() {
    if (m_texNyanCat.id != 0) UnloadTexture(m_texNyanCat);
    if (m_texRainbow.id != 0) UnloadTexture(m_texRainbow);
}

void SingleDotCue::Update(MotionOffset offset, float dt, const CueParams& params) {
    float inv = params.invertAxis ? -1.0f : 1.0f;
    float intensity = params.intensity;
    m_opacity = params.opacity;
    
    // skin toggle from the IPC panel
    if (params.skinId == 1) {
        m_activeSkin = SkinType::NyanCat;
    } else {
        m_activeSkin = SkinType::Default;
    }

    // clamping the target coming from the black box engine so it doesn't fly off screen fr
    m_targetX = std::max(-MAX_TRAVEL_X, std::min(MAX_TRAVEL_X, (float)offset.x * GAIN_X * intensity * inv));
    m_targetY = std::max(-MAX_TRAVEL_Y, std::min(MAX_TRAVEL_Y, (float)offset.y * GAIN_Y * intensity * inv));

    // crazy spring integral math
    auto sx = updateSpring(m_posX, m_velX, m_targetX, dt, OMEGA);
    auto sy = updateSpring(m_posY, m_velY, m_targetY, dt, OMEGA);
    m_posX = sx.pos; m_velX = sx.vel;
    m_posY = sy.pos; m_velY = sy.vel;

    // push it to the trail history
    m_trail.push_back({m_posX, m_posY});
    if ((int)m_trail.size() > TRAIL_LENGTH) m_trail.erase(m_trail.begin());
}

void SingleDotCue::Draw(int width, int height) {
    float cx = (width / 2.0f) + m_posX;
    float cy = (height / 2.0f) + m_posY;

    // check which skin to use, immaculate vibes
    SkinType skinToDraw = SkinType::Default;
    if (m_activeSkin == SkinType::NyanCat && isNyanUnlocked && m_texturesLoaded) {
        skinToDraw = SkinType::NyanCat;
    }

    if (skinToDraw == SkinType::NyanCat) {
        // NYAN CAT SKIN LETS GO
        // draw the rainbow trail
        for (int t = 0; t < (int)m_trail.size(); t++) {
            float progress = (float)(t + 1) / (float)m_trail.size();
            float trailAlpha = m_opacity * progress * TRAIL_ALPHA_MAX;
            float tx = (width / 2.0f) + m_trail[t].x;
            float ty = (height / 2.0f) + m_trail[t].y;
            
            Color tint = WHITE;
            tint.a = (unsigned char)(trailAlpha * 255.0f);
            
            Rectangle source = { 0.0f, 0.0f, (float)m_texRainbow.width, (float)m_texRainbow.height };
            // scale the trail based on tuning constants
            float scale = (DOT_RADIUS * TRAIL_SCALE_MULTIPLIER) / (float)m_texRainbow.height; 
            Rectangle dest = { tx, ty, (float)m_texRainbow.width * scale, (float)m_texRainbow.height * scale };
            Vector2 origin = { dest.width / 2.0f, dest.height / 2.0f }; // center that thing
            
            DrawTexturePro(m_texRainbow, source, dest, origin, 0.0f, tint);
        }

        // draw the actual cat
        Rectangle source = { 0.0f, 0.0f, (float)m_texNyanCat.width, (float)m_texNyanCat.height };
        float scale = (DOT_RADIUS * NYAN_SCALE_MULTIPLIER) / (float)m_texNyanCat.height;
        Rectangle dest = { cx, cy, (float)m_texNyanCat.width * scale, (float)m_texNyanCat.height * scale };
        Vector2 origin = { dest.width / 2.0f, dest.height / 2.0f };
        
        Color tint = WHITE;
        tint.a = (unsigned char)(m_opacity * 255.0f);
        DrawTexturePro(m_texNyanCat, source, dest, origin, 0.0f, tint);

    } else {
        // DEFAULT SKIN
        // cute ghost trail (the oldest ones get super transparent)
        for (int t = 0; t < (int)m_trail.size(); t++) {
            float progress = (float)(t + 1) / (float)m_trail.size();
            float trailAlpha = m_opacity * progress * 0.4f;
            float trailRadius = DOT_RADIUS * progress;
            float tx = (width / 2.0f) + m_trail[t].x;
            float ty = (height / 2.0f) + m_trail[t].y;
            DrawCircle((int)tx, (int)ty, trailRadius, NeonCyan(trailAlpha));
        }

        // shiny main dot
        DrawCircle((int)cx, (int)cy, DOT_RADIUS, NeonCyan(m_opacity));
        // glow ring to fake shadowBlur (since raylib doesn't have it, big sad)
        DrawCircleGradient((int)cx, (int)cy, DOT_RADIUS * 2.5f, NeonCyan(m_opacity * 0.3f), NeonCyan(0.0f));
    }
}

// 2. DOT GRID (DotGridView) : peripheral dots that follow you with springs
GridCue::GridCue() : m_targetX(0), m_targetY(0), m_initialized(false), m_opacity(0.8f), m_lastDt(1.0f/60.0f) {
    for (int i = 0; i < 6; i++) {
        m_springs[i] = {0, 0, 0, 0, {}};
    }
}

void GridCue::Update(MotionOffset offset, float dt, const CueParams& params) {
    float inv = params.invertAxis ? -1.0f : 1.0f;
    float intensity = params.intensity;
    m_opacity = params.opacity;

    m_targetX = std::max(-MAX_TRAVEL_X, std::min(MAX_TRAVEL_X, (float)offset.x * GAIN_X * intensity * inv));
    m_targetY = std::max(-MAX_TRAVEL_Y, std::min(MAX_TRAVEL_Y, (float)offset.y * GAIN_Y * intensity * inv));
    m_lastDt = dt;
}

void GridCue::Draw(int width, int height) {
    float w = (float)width;
    float h = (float)height;
    float leftX = w * MARGIN_PERCENT;
    float rightX = w * (1.0f - MARGIN_PERCENT);
    float dt = m_lastDt;
    if (dt <= 0 || dt > 0.5f) dt = 1.0f / 60.0f;

    // set up the spring anchors on frame 1 otherwise they spawn at 0,0 and fly across the screen
    if (!m_initialized) {
        for (int i = 0; i < DOT_COUNT; i++) {
            float anchorY = h * (0.35f + (i * 0.30f) / (DOT_COUNT - 1));
            m_springs[i].posX = leftX;
            m_springs[i].posY = anchorY;
            m_springs[DOT_COUNT + i].posX = rightX;
            m_springs[DOT_COUNT + i].posY = anchorY;
        }
        m_initialized = true;
    }

    for (int i = 0; i < DOT_COUNT; i++) {
        float anchorY = h * (0.35f + (i * 0.30f) / (DOT_COUNT - 1));

        // LEFT DOT
        DotSpring& ls = m_springs[i];
        float ltx = leftX + m_targetX;
        float lty = anchorY + m_targetY;
        auto lsx = updateSpring(ls.posX, ls.velX, ltx, dt, OMEGA);
        auto lsy = updateSpring(ls.posY, ls.velY, lty, dt, OMEGA);
        ls.posX = lsx.pos; ls.velX = lsx.vel;
        ls.posY = lsy.pos; ls.velY = lsy.vel;

        if (i == 0 && g_debugLogFile) {
            float firstX = ls.trail.empty() ? 0 : ls.trail.front().x;
            float lastX = ls.trail.empty() ? 0 : ls.trail.back().x;
            fprintf(g_debugLogFile, "[FRAME %u] CUE_DRAW: abs_x=%.4f abs_y=%.4f trail_len=%zu tail_x=%.1f head_x=%.1f\n", 
                    g_debugFrameCounter, ls.posX, ls.posY, ls.trail.size(), firstX, lastX);
        }

        ls.trail.push_back({ls.posX, ls.posY});
        if ((int)ls.trail.size() > TRAIL_LENGTH) ls.trail.erase(ls.trail.begin());

        // draw the trail ya
        for (int t = 0; t < (int)ls.trail.size(); t++) {
            float progress = (float)(t + 1) / (float)ls.trail.size();
            float trailAlpha = m_opacity * progress * 0.4f;
            float trailRadius = DOT_RADIUS * progress;
            DrawCircle((int)ls.trail[t].x, (int)ls.trail[t].y, trailRadius, NeonCyan(trailAlpha));
        }
        // draw main left ball
        DrawCircle((int)ls.posX, (int)ls.posY, DOT_RADIUS, NeonCyan(m_opacity));

        // RIGHT DOT
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
        // glow ring on the dots to make them lit
        DrawCircleGradient((int)ls.posX, (int)ls.posY, DOT_RADIUS * 2.5f, NeonCyan(m_opacity * 0.25f), NeonCyan(0.0f));
        DrawCircleGradient((int)rs.posX, (int)rs.posY, DOT_RADIUS * 2.5f, NeonCyan(m_opacity * 0.25f), NeonCyan(0.0f));
    }

    // no connecting lines drawn because it made the view way too cluttered, trust me fr
}

// 3. HORIZON BAR : attitude indicator with even more springs lol
ArtificialHorizonCue::ArtificialHorizonCue()
    : m_posRoll(0), m_velRoll(0), m_posPitch(0), m_velPitch(0),
      m_targetRoll(0), m_targetPitch(0), m_opacity(0.8f) {}

void ArtificialHorizonCue::Update(MotionOffset offset, float dt, const CueParams& params) {
    float inv = params.invertAxis ? -1.0f : 1.0f;
    float intensity = params.intensity;
    m_opacity = params.opacity;

    m_targetRoll = std::max(-MAX_ROLL, std::min(MAX_ROLL, (float)offset.z * GAIN_ROLL * intensity * inv));
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

    // render the trail to the screen
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

    // highly aesthetic main bar
    rlPushMatrix();
        rlTranslatef(cx, cy, 0.0f);
        rlRotatef(m_posRoll, 0.0f, 0.0f, 1.0f);
        rlTranslatef(0, m_posPitch, 0.0f);

        // the actual bar
        DrawRectangle((int)(-barWidth / 2), (int)(-barHeight / 2), (int)barWidth, (int)barHeight, NeonCyan(m_opacity));
        // the glow behind it
        DrawRectangle((int)(-barWidth / 2), (int)(-barHeight), (int)barWidth, (int)(barHeight * 2), NeonCyan(m_opacity * 0.15f));
    rlPopMatrix();
}

// 4. LIQUID WAVE : tiny wave that reacts to how fast you shake your head
LiquidWaveCue::LiquidWaveCue()
    : m_posRoll(0), m_velRoll(0), m_posPitch(0), m_velPitch(0),
      m_targetRoll(0), m_targetPitch(0), m_timeAcc(0), m_opacity(0.8f) {}

void LiquidWaveCue::Update(MotionOffset offset, float dt, const CueParams& params) {
    float inv = params.invertAxis ? -1.0f : 1.0f;
    float intensity = params.intensity;
    m_opacity = params.opacity;

    // for water: roll spins it, pitch changes height, yaw splashes the sides
    m_targetRoll = std::max(-MAX_ROLL, std::min(MAX_ROLL, (float)offset.z * GAIN_ROLL * intensity * inv));
    m_targetPitch = std::max(-MAX_PITCH, std::min(MAX_PITCH, (float)offset.y * GAIN_PITCH * intensity * inv));
    // slosh things around, trust me it looks cool
    float slosh = std::max(-50.0f, std::min(50.0f, (float)offset.x * 2.0f * intensity * inv));
    m_velRoll += slosh * dt * 5.0f; // inject random velocity for the wave feeling

    auto sr = updateSpring(m_posRoll, m_velRoll, m_targetRoll, dt, OMEGA);
    auto sp = updateSpring(m_posPitch, m_velPitch, m_targetPitch, dt, OMEGA);
    m_posRoll = sr.pos; m_velRoll = sr.vel;
    m_posPitch = sp.pos; m_velPitch = sp.vel;

    m_timeAcc += dt;
}

void LiquidWaveCue::Draw(int width, int height) {
    float cx = width / 2.0f;
    float cy = height / 2.0f;

    // wave gets furious when you move super fast
    float waveAmp = std::min(25.0f, fabsf(m_velRoll) * 0.6f) + 3.0f;
    float phase = m_timeAcc * 4.0f + m_posRoll * 0.08f;

    int numPoints = 80;

    rlPushMatrix();
        rlTranslatef(cx, cy, 0.0f);
        rlRotatef(m_posRoll, 0.0f, 0.0f, 1.0f);
        rlTranslatef(0, m_posPitch, 0.0f);

        // the water body (we use a cute triangle strip)
        float extent = (float)width * 1.5f;
        float halfExt = extent / 2.0f;
        float bodyStep = extent / (numPoints - 1);

        Vector2* strip = new Vector2[numPoints * 2];
        for (int i = 0; i < numPoints; i++) {
            float x = -halfExt + i * bodyStep;
            float y = sinf(x * 0.015f + phase) * waveAmp;
            strip[i * 2]     = { x, y };
            strip[i * 2 + 1] = { x, halfExt }; // drips to the bottom
        }
        DrawTriangleStrip(strip, numPoints * 2, NeonCyan(m_opacity * 0.35f));
        delete[] strip;

        // top highlight line so it looks super vibey
        Vector2 prev = { -halfExt, sinf(-halfExt * 0.015f + phase) * waveAmp };
        for (int i = 1; i < numPoints; i++) {
            float x = -halfExt + i * bodyStep;
            float y = sinf(x * 0.015f + phase) * waveAmp;
            Vector2 cur = { x, y };
            DrawLineEx(prev, cur, 4.0f, NeonCyan(m_opacity * 0.9f));
            prev = cur;
        }

        // and the glow obviously
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

// 5. PREMIUM: NYAN CAT GRID (still under construction)
NyanCatCue::NyanCatCue() : m_posX(0), m_posY(0) {}

void NyanCatCue::Update(MotionOffset current_offset, float dt, const CueParams& params) {
    // will make it more dynamic in the future
}

void NyanCatCue::Draw(int width, int height) {
    // no clue how to draw the rainbow trail here yet lmaoo
}

// 6. PREMIUM: DVD BOUNCE (because otherwise there's no fun)
DVDCue::DVDCue() : m_posX(0), m_posY(0) {}

void DVDCue::Update(MotionOffset current_offset, float dt, const CueParams& params) {
    // scaffold for the crazy dvd
}

void DVDCue::Draw(int width, int height) {
    // will make it more dynamic in the future
}
