#pragma once

#include <openvr.h>
#include <string>

class VROverlayManager {
public:
    VROverlayManager();
    ~VROverlayManager();

    bool Initialize(const std::string& key, const std::string& name);
    void SubmitTexture(unsigned int glTextureId);
    void Cleanup();

private:
    vr::VROverlayHandle_t m_ulOverlayHandle;
};
