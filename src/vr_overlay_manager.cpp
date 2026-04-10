#include "vr_overlay_manager.hpp"
#include <iostream>

VROverlayManager::VROverlayManager() : m_ulOverlayHandle(vr::k_ulOverlayHandleInvalid) {
}

VROverlayManager::~VROverlayManager() {
    Cleanup();
}

bool VROverlayManager::Initialize(const std::string& key, const std::string& name) {
    if (!vr::VROverlay()) {
        std::cerr << "[OverlayManager] ERROR: vr::VROverlay() returned null." << std::endl;
        return false;
    }

    // Create the overlay instance
    vr::EVROverlayError error = vr::VROverlay()->CreateOverlay(key.c_str(), name.c_str(), &m_ulOverlayHandle);
    if (error != vr::VROverlayError_None) {
        std::cerr << "[OverlayManager] ERROR: Could not create overlay: " << vr::VROverlay()->GetOverlayErrorNameFromEnum(error) << std::endl;
        return false;
    }

    // Set properties
    // Width defaults to meters in VR space
    vr::VROverlay()->SetOverlayWidthInMeters(m_ulOverlayHandle, 1.5f);
    
    // We want to lock this overlay relative to the user's head!
    // This makes the anchors stick to the periphery instead of the absolute room coordinates
    vr::HmdMatrix34_t transform = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, -1.0f  // 1 meter straight in front of the face
    };
    
    vr::VROverlay()->SetOverlayTransformTrackedDeviceRelative(
        m_ulOverlayHandle, 
        vr::k_unTrackedDeviceIndex_Hmd, 
        &transform
    );

    // Make sure it doesn't block controller lasers or interact physically
    vr::VROverlay()->SetOverlayInputMethod(m_ulOverlayHandle, vr::VROverlayInputMethod_None);

    // Show the overlay
    vr::VROverlay()->ShowOverlay(m_ulOverlayHandle);
    
    return true;
}

void VROverlayManager::SubmitTexture(unsigned int glTextureId) {
    if (m_ulOverlayHandle == vr::k_ulOverlayHandleInvalid || !vr::VROverlay()) return;

    // Package the OpenGL texture handle into OpenVR's generic struct
    vr::Texture_t texture = { (void*)(uintptr_t)glTextureId, vr::TextureType_OpenGL, vr::ColorSpace_Auto };
    
    // Push it to the compositor
    vr::EVROverlayError error = vr::VROverlay()->SetOverlayTexture(m_ulOverlayHandle, &texture);
    if (error != vr::VROverlayError_None) {
        // Suppressing print on failure to avoid console flooding, but a real app would throttle the log.
    }
}

void VROverlayManager::Cleanup() {
    if (m_ulOverlayHandle != vr::k_ulOverlayHandleInvalid && vr::VROverlay()) {
        vr::VROverlay()->DestroyOverlay(m_ulOverlayHandle);
        m_ulOverlayHandle = vr::k_ulOverlayHandleInvalid;
    }
}
