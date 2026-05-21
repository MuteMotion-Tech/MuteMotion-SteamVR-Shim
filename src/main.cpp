#include <iostream>
#include <thread>
#include <chrono>
#include <fstream>
#include <string>
#include <cstring>
#include <cmath>

#include <raylib.h>
#include <openvr.h>

#include "vr_overlay_manager.hpp"
#include "vr_sensor_bridge.hpp"
#include "visuals/cue_manager.hpp"

#define FPS 90

// ── Desktop Preview Mode ──────────────────────────────────────
// Generates fake sinusoidal sensor data bc we dont have a headset on linux 😭
// so devs can test visual cues without steamvr screaming at them.
static MotionOffset DesktopSimulateSensor(double time, double dt) {
    MotionOffset out;
    // Superimposed sine waves mimicking natural head sway (the drunken sailor algorithm)
    out.x = sin(time * 1.2) * 0.4 + sin(time * 2.7) * 0.15;
    out.y = cos(time * 0.9) * 0.3 + sin(time * 1.8) * 0.2;
    return out;
}

int main(int argc, char** argv) {
    // ── Parse CLI Arguments ───────────────────────────────────
    bool desktopMode = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--desktop") == 0 || strcmp(argv[i], "-d") == 0) {
            desktopMode = true;
        }
    }

    if (desktopMode) {
        std::cout << "[MuteMotionVR] ── DESKTOP PREVIEW MODE ──" << std::endl;
        std::cout << "[MuteMotionVR] No VR headset required. Rendering to desktop window." << std::endl;
        std::cout << "[MuteMotionVR] Press 1-4 to switch cues. Press Q to quit." << std::endl;
    } else {
        std::cout << "[MuteMotionVR] Booting VR Background Daemon..." << std::endl;
    }

    // ── VR-Only Initialization ────────────────────────────────
    vr::IVRSystem* pVRSystem = nullptr;
    VROverlayManager overlayManager;
    VRSensorBridge sensorBridge;

    if (!desktopMode) {
        // 1. the great OpenVR handshake. pls don't crash.
        vr::EVRInitError eError = vr::VRInitError_None;
        pVRSystem = vr::VR_Init(&eError, vr::VRApplication_Overlay);

        if (eError != vr::VRInitError_None) {
            std::cerr << "[MuteMotionVR] FATAL: Unable to init VR runtime: " 
                      << vr::VR_GetVRInitErrorAsEnglishDescription(eError) << " (Valve says no)" << std::endl;
            return -1;
        }

        std::cout << "[MuteMotionVR] OpenVR Initialized successfully." << std::endl;

        if (!overlayManager.Initialize("MuteMotion.Overlay", "MuteMotion Mitigator")) {
            std::cerr << "[MuteMotionVR] Failed to initialize overlay." << std::endl;
            vr::VR_Shutdown();
            return -1;
        }

        if (!sensorBridge.Initialize()) {
            std::cerr << "[MuteMotionVR] Failed to bind to MuteMotion Core Engine." << std::endl;
            vr::VR_Shutdown();
            return -1;
        }
    }

    // ── Raylib Window ─────────────────────────────────────────
    const int OVERLAY_WIDTH = 1024;
    const int OVERLAY_HEIGHT = 1024;

    if (desktopMode) {
        // Visible window so devs can see the math go brrr
        SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
        InitWindow(800, 600, "MuteMotionVR — Desktop Preview (Fake VR)");
    } else {
        // Invisible 1x1 window (VR mode only needs the FBO, don't pollute the desktop)
        SetConfigFlags(FLAG_WINDOW_HIDDEN);
        InitWindow(1, 1, "MuteMotion VR Render Context (Sneaky Mode)");
    }
    SetTargetFPS(FPS);

    RenderTexture2D targetTexture = LoadRenderTexture(OVERLAY_WIDTH, OVERLAY_HEIGHT);

    // ── Cue Manager ───────────────────────────────────────────
    CueManager cueManager;
    cueManager.SwitchCue(CueType::LIQUID);

    // ── Render Parameters (from Control Panel via state file) ─
    CueParams cueParams = { 1.0f, 0.8f, true };  // defaults: intensity=1.0, opacity=0.8, invert=true

    // ── Calibration Offset (nuking the gyro drift) ─────────────────
    MotionOffset calibrationOffset = { 0.0, 0.0 };
    bool calibrationPending = false;

    // ── Idle Detection ────────────────────────────────────────
    int idleFrameCount = 0;
    const int IDLE_THRESHOLD = 60;  // frames of zero-motion before idle indicator

    std::cout << "[MuteMotionVR] Engine running. Entering render loop." << std::endl;

    int frameCounter = 0;
    bool isOverlayActive = true;
    double elapsedTime = 0.0;

    // ── Main Event Loop ───────────────────────────────────────
    while (!WindowShouldClose()) {
        double dt = 1.0 / FPS;
        elapsedTime += dt;

        // ── Desktop Mode: Keyboard Controls ───────────────────
        if (desktopMode) {
            if (IsKeyPressed(KEY_ONE))   cueManager.SwitchCue(CueType::SINGLE_DOT);
            if (IsKeyPressed(KEY_TWO))   cueManager.SwitchCue(CueType::GRID);
            if (IsKeyPressed(KEY_THREE)) cueManager.SwitchCue(CueType::HORIZON);
            if (IsKeyPressed(KEY_FOUR))  cueManager.SwitchCue(CueType::LIQUID);
            if (IsKeyPressed(KEY_Q))     break;
            // Desktop calibration shortcut
            if (IsKeyPressed(KEY_C))     calibrationPending = true;
        }

        // ── VR Mode: Handle OpenVR Events ─────────────────────
        if (!desktopMode) {
            vr::VREvent_t event;
            while (pVRSystem->PollNextEvent(&event, sizeof(event))) {
                if (event.eventType == vr::VREvent_Quit) {
                    std::cout << "[MuteMotionVR] Received Quit event from SteamVR." << std::endl;
                    goto cleanup;
                }
            }
        }

        // ── Poll vr_state.txt at ~3Hz (both modes) ───────────
        frameCounter++;
        if (frameCounter >= 30) {
            frameCounter = 0;
            std::string homePath = "";
#ifdef _WIN32
            const char* userProfile = getenv("USERPROFILE");
            if (userProfile) homePath = std::string(userProfile);
#else
            const char* homeDir = getenv("HOME");
            if (homeDir) homePath = std::string(homeDir);
#endif
            if (!homePath.empty()) {
                std::string path = homePath + "/.mutemotion/vr_state.txt";
                std::ifstream stateFile(path);
                if (stateFile.is_open()) {
                    std::string line;
                    while (std::getline(stateFile, line)) {
                        try {
                            if (line.find("ACTIVE=") == 0) {
                                int active = std::stoi(line.substr(7));
                                if (active == 1 && !isOverlayActive) {
                                    if (!desktopMode) overlayManager.ShowOverlay();
                                    isOverlayActive = true;
                                } else if (active == 0 && isOverlayActive) {
                                    if (!desktopMode) overlayManager.HideOverlay();
                                    isOverlayActive = false;
                                }
                            } else if (line.find("PRESET=") == 0) {
                                int preset = std::stoi(line.substr(7));
                                cueManager.SwitchCue(static_cast<CueType>(preset));
                            } else if (line.find("INTENSITY=") == 0) {
                                cueParams.intensity = std::stof(line.substr(10));
                            } else if (line.find("OPACITY=") == 0) {
                                cueParams.opacity = std::stof(line.substr(8));
                            } else if (line.find("INVERT=") == 0) {
                                cueParams.invertAxis = (std::stoi(line.substr(7)) == 1);
                            } else if (line.find("CALIBRATE=") == 0) {
                                int cal = std::stoi(line.substr(10));
                                if (cal == 1) calibrationPending = true;
                            }
                        } catch (const std::exception& e) {
                            // Malformed line — ignore and continue
                        }
                    }
                }
            }
        }

        // ── Get Motion Offset ─────────────────────────────────
        MotionOffset offset;
        if (desktopMode) {
            offset = DesktopSimulateSensor(elapsedTime, dt);
        } else {
            offset = sensorBridge.ProcessFrame(pVRSystem, dt);
        }

        // ── Calibration: capture current offset as zero point ─
        if (calibrationPending) {
            calibrationOffset.x = offset.x;
            calibrationOffset.y = offset.y;
            calibrationPending = false;
            std::cout << "[MuteMotionVR] Calibrated: zero = (" 
                      << calibrationOffset.x << ", " << calibrationOffset.y << ")" << std::endl;
        }

        // Apply calibration offset
        offset.x -= calibrationOffset.x;
        offset.y -= calibrationOffset.y;

        // ── Idle Detection ────────────────────────────────────
        bool isIdle = false;
        if (std::abs(offset.x) < 0.001 && std::abs(offset.y) < 0.001) {
            idleFrameCount++;
            if (idleFrameCount > IDLE_THRESHOLD) isIdle = true;
        } else {
            idleFrameCount = 0;
        }

        if (!isOverlayActive) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }

        // ── Update & Render Cue ───────────────────────────────
        cueManager.UpdateActiveCue(offset, dt, cueParams);

        BeginTextureMode(targetTexture);
            ClearBackground(BLANK);
            cueManager.DrawActiveCue(OVERLAY_WIDTH, OVERLAY_HEIGHT);
            
            // Idle indicator: pulsing dot in the bottom-right corner
            if (isIdle) {
                float pulse = fabsf(sinf((float)elapsedTime * 3.0f));
                float radius = 6.0f + pulse * 6.0f;
                Color idleColor = { 0, 255, 204, (unsigned char)(pulse * 150.0f) };
                DrawCircle(OVERLAY_WIDTH - 40, OVERLAY_HEIGHT - 40, radius, idleColor);
            }
        EndTextureMode();

        if (desktopMode) {
            // Desktop Preview: draw the FBO texture to the visible window
            BeginDrawing();
                ClearBackground({20, 20, 30, 255});
                
                // Draw the overlay texture scaled to fit the window
                float scale = fmin((float)GetScreenWidth() / OVERLAY_WIDTH, 
                                   (float)GetScreenHeight() / OVERLAY_HEIGHT);
                float drawW = OVERLAY_WIDTH * scale;
                float drawH = OVERLAY_HEIGHT * scale;
                float drawX = (GetScreenWidth() - drawW) / 2.0f;
                float drawY = (GetScreenHeight() - drawH) / 2.0f;
                
                DrawTexturePro(
                    targetTexture.texture,
                    {0, 0, (float)OVERLAY_WIDTH, -(float)OVERLAY_HEIGHT}, // Flip Y
                    {drawX, drawY, drawW, drawH},
                    {0, 0}, 0.0f, WHITE
                );
                
                // HUD
                DrawText("MuteMotionVR — Desktop Preview", 10, 10, 20, LIGHTGRAY);
                DrawText("[1] Dot  [2] Grid  [3] Horizon  [4] Liquid  [C] Calibrate  [Q] Quit", 10, 35, 16, GRAY);
                
                // Live params display
                char paramBuf[128];
                snprintf(paramBuf, sizeof(paramBuf), 
                    "Intensity: %.1f | Opacity: %.1f | Invert: %s | Idle: %s",
                    cueParams.intensity, cueParams.opacity, 
                    cueParams.invertAxis ? "ON" : "OFF",
                    isIdle ? "YES" : "NO");
                DrawText(paramBuf, 10, 55, 14, DARKGRAY);
                
                DrawFPS(GetScreenWidth() - 100, 10);
            EndDrawing();
        } else {
            // VR Mode: submit texture to SteamVR compositor
            overlayManager.SubmitTexture(targetTexture.texture.id);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }

cleanup:
    std::cout << "[MuteMotionVR] Shutting down gracefully..." << std::endl;
    
    UnloadRenderTexture(targetTexture);
    CloseWindow();
    
    if (!desktopMode) {
        overlayManager.Cleanup();
        sensorBridge.Cleanup();
        vr::VR_Shutdown();
    }

    return 0;
}
