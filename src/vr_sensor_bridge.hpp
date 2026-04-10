#pragma once

#include <openvr.h>
#include "mutemotion_core.h" // Propitiatory core engine

struct MotionOffset {
    double x;
    double y;
};

class VRSensorBridge {
public:
    VRSensorBridge();
    ~VRSensorBridge();

    bool Initialize();
    MotionOffset ProcessFrame(vr::IVRSystem* vrSystem, double dt);
    void Cleanup();

private:
    void* m_pCoreEngine;
};
