#include "vr_sensor_bridge.hpp"
#include <iostream>

#ifdef _WIN32
#include <windows.h>
typedef void* (*InitCoreFn)(double);
typedef double (*ProcessMotionFn)(void*, SensorData, SensorData, double);
typedef void (*CleanupCoreFn)(void*);

static InitCoreFn win_init_core = nullptr;
static ProcessMotionFn win_process_motion = nullptr;
static CleanupCoreFn win_cleanup_core = nullptr;
static HMODULE hCoreLib = nullptr;
#endif

VRSensorBridge::VRSensorBridge() : m_pCoreEngine(nullptr) {
}

VRSensorBridge::~VRSensorBridge() {
    Cleanup();
}

bool VRSensorBridge::Initialize() {
#ifdef _WIN32
    hCoreLib = LoadLibraryA("mutemotion_core.dll");
    if (!hCoreLib) {
        std::cerr << "[SensorBridge] WARNING: mutemotion_core.dll missing. bros pc has no dll 💀. falling back to mock (0.0)." << std::endl;
        return true; // graceful fallback so we dont take steamvr down with us
    }
    win_init_core = (InitCoreFn)GetProcAddress(hCoreLib, "init_core");
    win_process_motion = (ProcessMotionFn)GetProcAddress(hCoreLib, "process_motion");
    win_cleanup_core = (CleanupCoreFn)GetProcAddress(hCoreLib, "cleanup_core");
    
    if (!win_init_core || !win_process_motion || !win_cleanup_core) {
        std::cerr << "[SensorBridge] WARNING: mutemotion_core.dll invalid symbols. falling back to mock (0.0)." << std::endl;
        FreeLibrary(hCoreLib);
        hCoreLib = nullptr;
        return true;
    }
    
    // boot the black box. hope it doesnt segfault.
    m_pCoreEngine = win_init_core(1.0);
#else
    // boot the black box (linux static link edition). hope it doesnt segfault.
    m_pCoreEngine = init_core(1.0);
#endif
    if (!m_pCoreEngine) {
        std::cerr << "[SensorBridge] FATAL: Failed to init mutemotion_core engine. RIP." << std::endl;
        return false;
    }
    return true;
}

MotionOffset VRSensorBridge::ProcessFrame(vr::IVRSystem* vrSystem, double dt) {
    MotionOffset out = {0.0, 0.0};
#ifndef _WIN32
    if (!m_pCoreEngine || !vrSystem) return out;
#else
    // if windows dll didnt load, just return zeroes so we dont crash the whole compositor
    if (!vrSystem) return out;
#endif

    vr::TrackedDevicePose_t trackedDevicePoses[vr::k_unMaxTrackedDeviceCount];
    // we only care about exactly where the user is looking right now (fPredictedSecondsToPhotonsFromNow = 0.0)
    vrSystem->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, 0.0f, trackedDevicePoses, vr::k_unMaxTrackedDeviceCount);

    const vr::TrackedDevicePose_t& hmdPose = trackedDevicePoses[vr::k_unTrackedDeviceIndex_Hmd];
    
    if (hmdPose.bPoseIsValid) {
        // raw gyro data coming in hot (Pitch, Yaw, Roll)
        double gyro_x = static_cast<double>(hmdPose.vAngularVelocity.v[0]);
        double gyro_y = static_cast<double>(hmdPose.vAngularVelocity.v[1]);
        double gyro_z = static_cast<double>(hmdPose.vAngularVelocity.v[2]);

        // OpenVR gives us linear velocity, we pretend it's raw accel bc the filter eats it anyway
        double accel_x = static_cast<double>(hmdPose.vVelocity.v[0]);
        double accel_y = static_cast<double>(hmdPose.vVelocity.v[1]);
        double accel_z = static_cast<double>(hmdPose.vVelocity.v[2]);

        SensorData gData1 = { gyro_x, gyro_y, gyro_z };
        SensorData aData1 = { accel_x, accel_y, accel_z };
        
        // the cursed axis swap strikes again (simulating Y motion)
        SensorData gData2 = { gyro_y, gyro_x, gyro_z };
        SensorData aData2 = { accel_y, accel_x, accel_z };

#ifdef _WIN32
        if (win_process_motion && m_pCoreEngine) {
            // feed the beast. let the proprietary math do its thing.
            double coreOutX = win_process_motion(m_pCoreEngine, gData1, aData1, dt);
            double coreOutY = win_process_motion(m_pCoreEngine, gData2, aData2, dt);
            out.x = coreOutX;
            out.y = coreOutY;
        } else {
            // crickets 🦗
            out.x = 0.0;
            out.y = 0.0;
        }
#else
        // feed the beast. let the proprietary math do its thing.
        double coreOutX = process_motion(m_pCoreEngine, gData1, aData1, dt);
        double coreOutY = process_motion(m_pCoreEngine, gData2, aData2, dt);
        out.x = coreOutX;
        out.y = coreOutY;
#endif
    }

    return out;
}

void VRSensorBridge::Cleanup() {
    if (m_pCoreEngine) {
#ifdef _WIN32
        if (win_cleanup_core) {
            win_cleanup_core(m_pCoreEngine);
        }
        if (hCoreLib) {
            FreeLibrary(hCoreLib);
            hCoreLib = nullptr;
        }
#else
        cleanup_core(m_pCoreEngine);
#endif
        m_pCoreEngine = nullptr;
    }
}
