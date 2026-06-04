#pragma once

#include <openvr.h>
// secret sauce fr
#include "mutemotion_core.h" 

struct MotionOffset {
    double x; // yaw
    double y; // pitch
    double z; // roll
};

class VRSensorBridge {
public:
    VRSensorBridge();
    ~VRSensorBridge();

    bool Initialize();
    MotionOffset ProcessFrame(vr::IVRSystem* vrSystem, double dt);
    void Cleanup();

private:
    void* m_pCoreEngine;  // yaw 
    void* m_pCoreEngineY; // pitch 
    void* m_pCoreEngineZ; // roll 
    MotionOffset m_lastValidOffset; // keeps it from dropping to zero and causing a scene
};
