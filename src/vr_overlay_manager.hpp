#pragma once

#include <openvr.h>
#include <string>

// cursed UI class to send stuff to SteamVR
class VROverlayManager {
public:
    VROverlayManager();
    ~VROverlayManager();

    bool Initialize(const std::string& key, const std::string& name);
    void SubmitTexture(unsigned int glTextureId);
    void PumpEvents();
    void ShowOverlay();
    void HideOverlay();
    void Cleanup();

private:
    vr::VROverlayHandle_t m_ulOverlayHandle;
};
