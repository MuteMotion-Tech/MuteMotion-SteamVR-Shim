#include "vr_overlay_manager.hpp"
#include <iostream>
#include <ctime>
#include <string>
#include <cmath>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include <GL/gl.h>
#else
#include <GL/gl.h>
#endif

VROverlayManager::VROverlayManager() : m_ulOverlayHandle(vr::k_ulOverlayHandleInvalid) {
}

VROverlayManager::~VROverlayManager() {
    Cleanup();
}

bool VROverlayManager::Initialize(const std::string& key, const std::string& name) {
    if (!vr::VRSystem() || !vr::VROverlay()) {
        std::cerr << "[MuteMotionVR] ERROR: vr::VRSystem() or vr::VROverlay() returned null." << std::endl;
        return false;
    }

    // spawn the overlay 
    vr::EVROverlayError error = vr::VROverlay()->CreateOverlay(key.c_str(), name.c_str(), &m_ulOverlayHandle);
    
    // sometimes steamvr holds the key hostage when we crash. big yikes. let's fallback to unique keys
    if (error == vr::VROverlayError_KeyInUse) {
        std::string uniqueKey = key + "." + std::to_string(time(nullptr));
        std::cout << "[MuteMotionVR] WARNING: Key in use. Falling back to unique key: " << uniqueKey << std::endl;
        error = vr::VROverlay()->CreateOverlay(uniqueKey.c_str(), name.c_str(), &m_ulOverlayHandle);
    }

    if (error != vr::VROverlayError_None) {
        std::cerr << "[MuteMotionVR] ERROR: Could not create overlay: " << vr::VROverlay()->GetOverlayErrorNameFromEnum(error) << std::endl;
        return false;
    }

    // crazy maths for the VR headset FOV 
    // extract exact tangents so it perfectly aligns to the user's screen
    float l_left, l_right, l_top, l_bottom;
    vr::VRSystem()->GetProjectionRaw(vr::Eye_Left, &l_left, &l_right, &l_top, &l_bottom);
    
    float r_left, r_right, r_top, r_bottom;
    vr::VRSystem()->GetProjectionRaw(vr::Eye_Right, &r_left, &r_right, &r_top, &r_bottom);

    // full scale horizontal tangent to cover binocular vision
    float maxTanX = std::max({ std::abs(l_left), std::abs(l_right), std::abs(r_left), std::abs(r_right) });
    
    // physics: width = 2 * distance (which is 1m) * tangenX + a 10% safety cap for reprojection
    float dynamicWidth = (maxTanX * 2.0f) * 1.0f * 1.1f;
    std::cout << "[MuteMotionVR] HMD Geometry Parsed. Setting dynamic overlay width to: " << dynamicWidth << " meters." << std::endl;

    vr::VROverlay()->SetOverlayWidthInMeters(m_ulOverlayHandle, dynamicWidth);
    
    // glue this to the user's head
    // this way the peripheral dots don't roll with the head and stay locked on the HUD
    vr::HmdMatrix34_t transform = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, -1.0f  // push 1 meter straight in front of the face
    };
    
    vr::VROverlay()->SetOverlayTransformTrackedDeviceRelative(
        m_ulOverlayHandle, 
        vr::k_unTrackedDeviceIndex_Hmd, 
        &transform
    );

    // turn laser input off otherwise it ruins gameplay with ghost clicks
    vr::VROverlay()->SetOverlayInputMethod(m_ulOverlayHandle, vr::VROverlayInputMethod_None);

    // standard alpha blending (our neoncyan is already straight alpha)

    // flip the texture vertically. openvr defaults opengl texture bounds upside down lol
    vr::VRTextureBounds_t bounds = { 0.0f, 0.0f, 1.0f, 1.0f }; // default ez
    vr::VROverlay()->SetOverlayTextureBounds(m_ulOverlayHandle, &bounds);

    // show it
    vr::VROverlay()->ShowOverlay(m_ulOverlayHandle);
    
    return true;
}

void VROverlayManager::SubmitTexture(unsigned int glTextureId) {
    if (m_ulOverlayHandle == vr::k_ulOverlayHandleInvalid || !vr::VROverlay()) return;

    // flush and dip before the gpu chokes on the sync stall. vr compositor gets mad otherwise.
    glFlush();

    // pack the raylib fbo for openvr to eat
    vr::Texture_t texture = { (void*)(uintptr_t)glTextureId, vr::TextureType_OpenGL, vr::ColorSpace_Auto };
    
    // push it
    vr::EVROverlayError error = vr::VROverlay()->SetOverlayTexture(m_ulOverlayHandle, &texture);
    if (error != vr::VROverlayError_None) {
        // stay quiet because otherwise it fills the log with spam if it fails
    }
}

void VROverlayManager::PumpEvents() {
    if (m_ulOverlayHandle == vr::k_ulOverlayHandleInvalid || !vr::VROverlay()) return;
    
    vr::VREvent_t event;
    while (vr::VROverlay()->PollNextOverlayEvent(m_ulOverlayHandle, &event, sizeof(event))) {
        // drain the annoying ipc queue so the vr compositor doesn't crash
    }
}

void VROverlayManager::Cleanup() {
    if (m_ulOverlayHandle != vr::k_ulOverlayHandleInvalid && vr::VROverlay()) {
        vr::VROverlay()->DestroyOverlay(m_ulOverlayHandle);
        m_ulOverlayHandle = vr::k_ulOverlayHandleInvalid;
    }
}

void VROverlayManager::ShowOverlay() {
    if (m_ulOverlayHandle != vr::k_ulOverlayHandleInvalid && vr::VROverlay()) {
        vr::VROverlay()->ShowOverlay(m_ulOverlayHandle);
    }
}

void VROverlayManager::HideOverlay() {
    if (m_ulOverlayHandle != vr::k_ulOverlayHandleInvalid && vr::VROverlay()) {
        vr::VROverlay()->HideOverlay(m_ulOverlayHandle);
    }
}
