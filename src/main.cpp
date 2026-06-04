#include <iostream>
#include <thread>
#include <chrono>
#include <fstream>
#include <atomic>
#include <iomanip>
#include <string>
#include <cstring>
#include <cmath>

#include <raylib.h>
#include <openvr.h>

#include "vr_overlay_manager.hpp"
#include "vr_sensor_bridge.hpp"
#include "visuals/cue_manager.hpp"

#define FPS 90

// comfort tuning stuff. gotta rebuild to apply this btw
// unverified baseline so the latency text stays visible and doesn't get clipped by the lens
static constexpr int LATENCY_TEXT_X = 200; // shift inward so it doesn't clip the fov
static constexpr int LATENCY_TEXT_Y = 20;

#include "debug_sync.hpp"
unsigned int g_debugFrameCounter = 0;
FILE* g_debugLogFile = nullptr;

// desktop preview mode
// generates fake sinusoidal sensor data because we don't have a headset on linux rn 😭
// this way devs can test visual cues without steamvr yelling at us
static MotionOffset DesktopSimulateSensor(double time, double dt) {
    MotionOffset out;
    // mix of sines to simulate natural head movement (drunken sailor vibes)
    out.x = sin(time * 1.2) * 0.4 + sin(time * 2.7) * 0.15;
    out.y = cos(time * 0.9) * 0.3 + sin(time * 1.8) * 0.2;
    return out;
}

int main(int argc, char** argv) {
    // checking what arguments were passed in the CLI
    bool desktopMode = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-desktop") == 0 || strcmp(argv[i], "-d") == 0) {
            desktopMode = true;
        }
    }

    if (desktopMode) {
        std::cout << "[MuteMotionVR] DESKTOP PREVIEW MODE" << std::endl;
        std::cout << "[MuteMotionVR] No VR headset required. Rendering to window." << std::endl;
        std::cout << "[MuteMotionVR] Press 1-4 to swap cues. Press Q to quit." << std::endl;
    } else {
        std::cout << "[MuteMotionVR] Starting VR daemon..." << std::endl;
    }

    // inits for when you're actually in VR
    vr::IVRSystem* pVRSystem = nullptr;
    VROverlayManager overlayManager;
    VRSensorBridge sensorBridge;

    if (!desktopMode) {
        // the great openvr handshake. pls don't crash mr valve
        vr::EVRInitError eError = vr::VRInitError_None;
        pVRSystem = vr::VR_Init(&eError, vr::VRApplication_Overlay);

        if (eError != vr::VRInitError_None) {
            std::cerr << "[MuteMotionVR] FATAL: Unable to init VR runtime: " 
                      << vr::VR_GetVRInitErrorAsEnglishDescription(eError) << std::endl;
            return -1;
        }

        std::cout << "[MuteMotionVR] OpenVR init successful." << std::endl;

        // snooping to see which headset the user has
        char hmdModel[256];
        vr::ETrackedPropertyError propError;
        pVRSystem->GetStringTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_ModelNumber_String, hmdModel, sizeof(hmdModel), &propError);
        if (propError == vr::TrackedProp_Success) {
            std::cout << "[MuteMotionVR] DETECTED_HMD: " << hmdModel << std::endl;
        } else {
            std::cout << "[MuteMotionVR] Unknown HMD." << std::endl;
        }

        if (!overlayManager.Initialize("MuteMotion.Overlay", "Mitigation Overlay")) {
            std::cerr << "[MuteMotionVR] FATAL: Failed to init overlay." << std::endl;
            vr::VR_Shutdown();
            return -1;
        }

        if (!sensorBridge.Initialize()) {
            std::cerr << "[MuteMotionVR] FATAL: Failed to init sensor bridge." << std::endl;
            vr::VR_Shutdown();
            return -1;
        }
    }

    // dynamic maths to calculate native resolution of the headset
    uint32_t recWidth = 2560, recHeight = 2560;
    if (!desktopMode && pVRSystem) {
        pVRSystem->GetRecommendedRenderTargetSize(&recWidth, &recHeight);
        std::cout << "[MuteMotionVR] Recommended render target size: " << recWidth << "x" << recHeight << std::endl;
    }
    const int OVERLAY_WIDTH = std::max(1024, (int)recWidth);
    const int OVERLAY_HEIGHT = std::max(1024, (int)recHeight);

    if (desktopMode) {
        // window on sight to see the maths go brrr
        SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
        InitWindow(800, 600, "MuteMotionVR Desktop Preview");
    } else {
        // stealth mode. 1x1 hidden window for VR mode because we just need the FBO.
        // sneaky fr
        SetConfigFlags(FLAG_WINDOW_HIDDEN);
        InitWindow(1, 1, "MuteMotionVR Stealth Context");
    }
    
    // FPS cap so the CPU doesn't catch fire
    // the physics timing is still real-time because we use getframetime()
    SetTargetFPS(FPS);
    RenderTexture2D targetTexture = LoadRenderTexture(OVERLAY_WIDTH, OVERLAY_HEIGHT);
    SetTextureFilter(targetTexture.texture, TEXTURE_FILTER_BILINEAR);

    CueManager cueManager;
    cueManager.SwitchCue(CueType::LIQUID);

    // params from the python panel (shared state goes hard)
    CueParams cueParams = { 1.0f, 0.8f, true };  // defaults

    // offset for calibrate (nukes the drift)
    MotionOffset calibrationOffset = { 0.0, 0.0 };
    bool calibrationPending = false;

    // detect idle
    int idleFrameCount = 0;
    const int IDLE_THRESHOLD = 60;  // framerates until we consider it idle

    std::cout << "[MuteMotionVR] Engine running. Entering render loop." << std::endl;

    int frameCounter = 0;
    bool isOverlayActive = true;
    double elapsedTime = 0.0;
    bool showLatency = false;
    bool telemetry = false;

    // watchdog to detect animation jitter
    std::ofstream watchdog("shim_watchdog.log", std::ios::trunc);
    if (watchdog.is_open()) {
        watchdog << "Shim Watchdog v2\n";
        watchdog << "Columns: time | dt | offset.x | offset.y | offset.z | jitter_x | jitter_y\n";
        watchdog << "jitter = |current - previous| (high values = choppy frames)\n";
        watchdog << "summary of 0.5s shows: avg_jitter, max_jitter, direction_flips\n\n";
    }
    double prevOffX = 0.0, prevOffY = 0.0;
    double summaryTimer = 0.0;
    double sumJitterX = 0.0, sumJitterY = 0.0;
    double maxJitterX = 0.0, maxJitterY = 0.0;
    int jitterSamples = 0;
    int dirFlipsX = 0;  // flips in X
    double prevSignX = 0.0;

    g_debugLogFile = fopen("shim_divergence.log", "w");
    if (g_debugLogFile) {
        fprintf(g_debugLogFile, "DIVERGENCES LOGGED GG\n");
    }

    // the grand loop
    while (!WindowShouldClose()) {
        g_debugFrameCounter++; // sync for everyone
        double dt = GetFrameTime(); // real time delta, so physics don't judder
        if (dt > 0.1) dt = 0.1; // cap in case of lag spike, otherwise the springs explode
        elapsedTime += dt;

        // keyboard for devs
        if (desktopMode) {
            if (IsKeyPressed(KEY_Q))     break;
            // shortcut for devs to calibrate
            if (IsKeyPressed(KEY_C))     calibrationPending = true;
        }

        // openvr events in vr mode
        if (!desktopMode) {
            vr::VREvent_t event;
            while (pVRSystem->PollNextEvent(&event, sizeof(event))) {
                if (event.eventType == vr::VREvent_Quit) {
                    std::cout << "[MuteMotionVR] SteamVR requested quit." << std::endl;
                    goto cleanup;
                }
            }
            // random crash fix
            overlayManager.PumpEvents();
        }

        // poll the python state file every ~3hz
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
                            } else if (line.find("SKIN=") == 0) {
                                cueParams.skinId = std::stoi(line.substr(5));
                            } else if (line.find("LATENCY=") == 0) {
                                showLatency = (std::stoi(line.substr(8)) == 1);
                            } else if (line.find("TELEMETRY=") == 0) {
                                telemetry = (std::stoi(line.substr(10)) == 1);
                            } else if (line.find("CALIBRATE=") == 0) {
                                int cal = std::stoi(line.substr(10));
                                if (cal == 1) calibrationPending = true;
                            }
                        } catch (const std::exception& e) {
                            // ignore garbage strings
                        }
                    }
                }
            }
        }

        // pull the offset
        MotionOffset offset;
        if (desktopMode) {
            offset = DesktopSimulateSensor(elapsedTime, dt);
        } else {
            offset = sensorBridge.ProcessFrame(pVRSystem, dt);

            if (g_debugLogFile) {
                fprintf(g_debugLogFile, "[FRAME %u] DO_MATH: x=%.4f y=%.4f z=%.4f\n", g_debugFrameCounter, offset.x, offset.y, offset.z);
            }

            // watchdog
            if (watchdog.is_open() && (frameCounter % 5 == 0)) {
                double jitterX = fabs(offset.x - prevOffX);
                double jitterY = fabs(offset.y - prevOffY);
                
                // count flips
                double curSignX = (offset.x > 0.001) ? 1.0 : ((offset.x < -0.001) ? -1.0 : 0.0);
                if (curSignX != 0.0 && prevSignX != 0.0 && curSignX != prevSignX) dirFlipsX++;
                if (curSignX != 0.0) prevSignX = curSignX;
                
                sumJitterX += jitterX; sumJitterY += jitterY;
                maxJitterX = fmax(maxJitterX, jitterX);
                maxJitterY = fmax(maxJitterY, jitterY);
                jitterSamples++;
                
                watchdog << std::fixed << std::setprecision(4)
                         << "t=" << elapsedTime 
                         << " dt=" << dt 
                         << " | x=" << offset.x << " y=" << offset.y << " z=" << offset.z
                         << " | jit_x=" << jitterX << " jit_y=" << jitterY << "\n";
                
                prevOffX = offset.x; prevOffY = offset.y;
            }
            
            // summary line in the log for stats
            summaryTimer += dt;
            if (watchdog.is_open() && summaryTimer >= 0.5) {
                double avgJitX = jitterSamples > 0 ? sumJitterX / jitterSamples : 0;
                double avgJitY = jitterSamples > 0 ? sumJitterY / jitterSamples : 0;
                watchdog << "SUM 0.5s: avg_x=" << avgJitX << " max_x=" << maxJitterX
                         << " avg_y=" << avgJitY << " max_y=" << maxJitterY
                         << " flips_x=" << dirFlipsX << " samples=" << jitterSamples << "\n";
                watchdog.flush();
                summaryTimer = 0; sumJitterX = 0; sumJitterY = 0;
                maxJitterX = 0; maxJitterY = 0; jitterSamples = 0; dirFlipsX = 0;
            }
        }

        // calibrate null point to wipe drift
        if (calibrationPending) {
            calibrationOffset.x = offset.x;
            calibrationOffset.y = offset.y;
            calibrationPending = false;
            std::cout << "[MuteMotionVR] Calibrated: zero = (" 
                      << calibrationOffset.x << ", " << calibrationOffset.y << ")" << std::endl;
        }

        // apply the math
        offset.x -= calibrationOffset.x;
        offset.y -= calibrationOffset.y;

        // telemetry
        if (telemetry && (frameCounter % 9 == 0)) {
            if (!desktopMode) {
                vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
                pVRSystem->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, 0, poses, vr::k_unMaxTrackedDeviceCount);
                auto& pose = poses[vr::k_unTrackedDeviceIndex_Hmd];
                if (pose.bPoseIsValid) {
                    float pitch = pose.vAngularVelocity.v[0];
                    float yaw   = pose.vAngularVelocity.v[1];
                    float roll  = pose.vAngularVelocity.v[2];
                    std::cout << "[IMU_DATA] " << pitch << "," << yaw << "," << roll << "|" << offset.x << "," << offset.y << std::endl;
                }
            } else {
                std::cout << "[IMU_DATA] 0.0,0.0,0.0|" << offset.x << "," << offset.y << std::endl;
            }
        }

        // check if user is afk
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

        // no clue how this works tbh but it draws the visual cue overlay lol
        cueManager.UpdateActiveCue(offset, dt, cueParams);

        BeginTextureMode(targetTexture);
            ClearBackground(BLANK);
            
            cueManager.DrawActiveCue(OVERLAY_WIDTH, OVERLAY_HEIGHT);
            
            // pulsing dot to let you know you're afk
            if (isIdle) {
                float pulse = fabsf(sinf((float)elapsedTime * 3.0f));
                float radius = 6.0f + pulse * 6.0f;
                Color idleColor = { 0, 255, 204, (unsigned char)(pulse * 150.0f) };
                DrawCircle(OVERLAY_WIDTH - 40, OVERLAY_HEIGHT - 40, radius, idleColor);
            }

            if (showLatency) {
                DrawText(TextFormat("Ping (ms): %.1f", dt * 1000.0f), LATENCY_TEXT_X, LATENCY_TEXT_Y, 40, GREEN);
            }

        EndTextureMode();

        if (desktopMode) {
            // desktop preview rendering
            BeginDrawing();
                ClearBackground({20, 20, 30, 255});
                
                // scale math so it fits
                float scale = fmin((float)GetScreenWidth() / OVERLAY_WIDTH, 
                                   (float)GetScreenHeight() / OVERLAY_HEIGHT);
                float drawW = OVERLAY_WIDTH * scale;
                float drawH = OVERLAY_HEIGHT * scale;
                float drawX = (GetScreenWidth() - drawW) / 2.0f;
                float drawY = (GetScreenHeight() - drawH) / 2.0f;
                
                DrawTexturePro(
                    targetTexture.texture,
                    {0, 0, (float)OVERLAY_WIDTH, -(float)OVERLAY_HEIGHT}, // invert Y (opengl skill issues fr)
                    {drawX, drawY, drawW, drawH},
                    {0, 0}, 0.0f, WHITE
                );
                
                // HUD
                DrawText("Desktop Preview Shim", 10, 10, 20, LIGHTGRAY);
                DrawText("[1] Single Dot  [2] Grid  [3] Horizon  [4] Liquid  [C] Zero  [Q] Quit", 10, 35, 16, GRAY);
                
                char paramBuf[128];
                snprintf(paramBuf, sizeof(paramBuf), 
                    "Spiciness: %.1f | Alpha: %.1f | Invert: %s | Afk: %s",
                    cueParams.intensity, cueParams.opacity, 
                    cueParams.invertAxis ? "YES" : "NO",
                    isIdle ? "YES" : "NO");
                DrawText(paramBuf, 10, 55, 14, DARKGRAY);
                
                DrawFPS(GetScreenWidth() - 100, 10);
            EndDrawing();
        } else {
            // submit to SteamVR compositor
            overlayManager.SubmitTexture(targetTexture.texture.id);
            
            // raylib needs this to not crash (updates internal clock and dt)
            BeginDrawing();
                ClearBackground(BLACK);
            EndDrawing();
        }
    }

cleanup:
    if (g_debugLogFile) {
        fclose(g_debugLogFile);
        g_debugLogFile = nullptr;
    }
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
