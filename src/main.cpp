#include <iostream>
#include <thread>
#include <chrono>

#include <raylib.h>
#include <openvr.h>

// Forward declarations of our modules (to be implemented)
#include "vr_overlay_manager.hpp"
#include "vr_sensor_bridge.hpp"
#include "visuals/cue_manager.hpp"

#define FPS 90

int main(int argc, char** argv) {
    std::cout << "[MuteMotionVR] Booting VR Background Daemon..." << std::endl;

    // 1. Initialize OpenVR as an Overlay Application
    vr::EVRInitError eError = vr::VRInitError_None;
    vr::IVRSystem* pVRSystem = vr::VR_Init(&eError, vr::VRApplication_Overlay);

    if (eError != vr::VRInitError_None) {
        std::cerr << "[MuteMotionVR] FATAL: Unable to init VR runtime: " 
                  << vr::VR_GetVRInitErrorAsEnglishDescription(eError) << std::endl;
        return -1;
    }

    std::cout << "[MuteMotionVR] OpenVR Initialized successfully." << std::endl;

    // 2. Initialize Raylib (Headless / Invisible Window for GPU Context)
    // We make it 1x1 invisible because we only care about rendering to a Framebuffer Object (FBO)
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(1, 1, "MuteMotion VR Render Context");
    SetTargetFPS(FPS);

    // The actual resolution of the overlay texture rendered into the eye
    const int OVERLAY_WIDTH = 1024;
    const int OVERLAY_HEIGHT = 1024;
    RenderTexture2D targetTexture = LoadRenderTexture(OVERLAY_WIDTH, OVERLAY_HEIGHT);

    // 3. Initialize our MuteMotion Modules
    VROverlayManager overlayManager;
    VRSensorBridge sensorBridge;
    CueManager cueManager;

    // Use Liquid Wave as our baseline beautiful render
    cueManager.SwitchCue(CueType::LIQUID);

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

    std::cout << "[MuteMotionVR] Engine running. Entering render loop." << std::endl;

    // 4. Main Event Loop
    while (!WindowShouldClose()) {
        // Handle OpenVR Events (like quitting SteamVR)
        vr::VREvent_t event;
        while (pVRSystem->PollNextEvent(&event, sizeof(event))) {
            if (event.eventType == vr::VREvent_Quit) {
                std::cout << "[MuteMotionVR] Received Quit event from SteamVR." << std::endl;
                goto cleanup;
            }
        }

        // A. Extract Angular Velocity from Headset to MuteMotion Core
        double dt = 1.0 / FPS;
        MotionOffset offset = sensorBridge.ProcessFrame(pVRSystem, dt);

        // B. Update Cue State
        cueManager.UpdateActiveCue(offset, dt);

        // C. Draw our visuals based on the mitigated output into the Raylib FBO
        BeginTextureMode(targetTexture);
            ClearBackground(BLANK); // Transparent background

            // Let the polymorphic manager draw whichever geometry is currently bound
            cueManager.DrawActiveCue(OVERLAY_WIDTH, OVERLAY_HEIGHT);
            
        EndTextureMode();

        // C. Pass the rendered texture ID directly into SteamVR's Overlay System
        // Raylib uses OpenGL backend by default on Linux
        overlayManager.SubmitTexture(targetTexture.texture.id);

        // Minimal sleep to let raylib sync
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

cleanup:
    std::cout << "[MuteMotionVR] Shutting down gracefully..." << std::endl;
    
    UnloadRenderTexture(targetTexture);
    CloseWindow();
    
    overlayManager.Cleanup();
    sensorBridge.Cleanup();
    vr::VR_Shutdown();

    return 0;
}
