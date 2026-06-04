#include "vr_sensor_bridge.hpp"
#include <iostream>
#include <cmath>
#include "debug_sync.hpp"
#ifdef _WIN32
#include <windows.h>
// function pointers for the black box DLL exports. don't touch
typedef void* (*InitCoreFn)(double);
typedef double (*ProcessMotionFn)(void*, SensorData, SensorData, double);
typedef double (*ProcessMotionVRFn)(void*, double, double);
typedef void (*CleanupCoreFn)(void*);

static InitCoreFn win_init_core = nullptr;
static ProcessMotionFn win_process_motion = nullptr;
static ProcessMotionVRFn win_process_motion_vr = nullptr;
static CleanupCoreFn win_cleanup_core = nullptr;
static HMODULE hCoreLib = nullptr;
#endif

VRSensorBridge::VRSensorBridge() : m_pCoreEngine(nullptr), m_pCoreEngineY(nullptr), m_pCoreEngineZ(nullptr) {
    m_lastValidOffset = {0.0, 0.0, 0.0};
}

VRSensorBridge::~VRSensorBridge() {
    Cleanup();
}

bool VRSensorBridge::Initialize() {
#ifdef _WIN32
    // load the magic dll that handles the math we shouldn't leak
    hCoreLib = LoadLibraryA("mutemotion_core.dll");
    if (!hCoreLib) {
        std::cerr << "[SensorBridge] WARNING: mutemotion_core.dll missing. falling back to mock (0.0)." << std::endl;
        return true; // fallback so we don't kill steamvr
    }
    // fetch the functions
    win_init_core = (InitCoreFn)GetProcAddress(hCoreLib, "init_core");
    win_process_motion = (ProcessMotionFn)GetProcAddress(hCoreLib, "process_motion");
    win_process_motion_vr = (ProcessMotionVRFn)GetProcAddress(hCoreLib, "process_motion_vr");
    win_cleanup_core = (CleanupCoreFn)GetProcAddress(hCoreLib, "cleanup_core");
    
    if (!win_init_core || !win_process_motion || !win_process_motion_vr || !win_cleanup_core) {
        std::cerr << "[SensorBridge] WARNING: mutemotion_core.dll invalid symbols. falling back to mock (0.0)." << std::endl;
        FreeLibrary(hCoreLib);
        hCoreLib = nullptr;
        return true;
    }
    
    // booting the engine instances. let's hope it doesn't segfault
    // X axis (yaw) gets the first one
    m_pCoreEngine = win_init_core(1.0);
    // Y axis (pitch)
    m_pCoreEngineY = win_init_core(1.0);
    // Z axis (roll) 
    m_pCoreEngineZ = win_init_core(1.0);
#else
    // the black box linux version. ya it should work after compiling
    m_pCoreEngine = init_core(1.0);
    m_pCoreEngineY = init_core(1.0);
    m_pCoreEngineZ = init_core(1.0);
#endif
    if (!m_pCoreEngine || !m_pCoreEngineY || !m_pCoreEngineZ) {
        std::cerr << "[SensorBridge] FATAL: Failed to init mutemotion_core engine. RIP." << std::endl;
        return false;
    }
    return true;
}

MotionOffset VRSensorBridge::ProcessFrame(vr::IVRSystem* vrSystem, double dt) {
    if (!vrSystem) return m_lastValidOffset;

    vr::TrackedDevicePose_t trackedDevicePoses[vr::k_unMaxTrackedDeviceCount];
    
    vrSystem->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, 0.0f, trackedDevicePoses, vr::k_unMaxTrackedDeviceCount);

    const vr::TrackedDevicePose_t& hmdPose = trackedDevicePoses[vr::k_unTrackedDeviceIndex_Hmd];
    
    if (hmdPose.bPoseIsValid) {
        // openvr sketchy values in rad/s.
        // since the overlay is head-locked, the values are already in local-space fr.
        // doing a matrix transform here causes massive axis crosstalk (confirmed in logs).
        double yaw   = static_cast<double>(hmdPose.vAngularVelocity.v[1]);
        double pitch = static_cast<double>(hmdPose.vAngularVelocity.v[0]);
        double roll  = static_cast<double>(hmdPose.vAngularVelocity.v[2]);

        // dead zone nuke: gyros never rest at 0 so just kill this micro noise 
        if (fabs(yaw) < 0.05) yaw = 0.0;
        if (fabs(pitch) < 0.05) pitch = 0.0;
        if (fabs(roll) < 0.05) roll = 0.0;

        if (g_debugLogFile) {
            fprintf(g_debugLogFile, "[FRAME %u] RAW_GYRO_TILT: yaw=%.4f pitch=%.4f roll=%.4f valid=%d\n", g_debugFrameCounter, yaw, pitch, roll, hmdPose.bPoseIsValid);
        }

        // feed the secret sauce engine
        // we use three separate ones so they don't wipe each other's state
#ifdef _WIN32
        if (win_process_motion_vr && m_pCoreEngine && m_pCoreEngineY && m_pCoreEngineZ) {
            m_lastValidOffset.x = win_process_motion_vr(m_pCoreEngine, yaw, dt);
            m_lastValidOffset.y = win_process_motion_vr(m_pCoreEngineY, pitch, dt);
            m_lastValidOffset.z = win_process_motion_vr(m_pCoreEngineZ, roll, dt);
        }
#else
        if (m_pCoreEngine && m_pCoreEngineY && m_pCoreEngineZ) {
            m_lastValidOffset.x = process_motion_vr(m_pCoreEngine, yaw, dt);
            m_lastValidOffset.y = process_motion_vr(m_pCoreEngineY, pitch, dt);
            m_lastValidOffset.z = process_motion_vr(m_pCoreEngineZ, roll, dt);
        }
#endif
    }

    return m_lastValidOffset;
}

void VRSensorBridge::Cleanup() {
    if (m_pCoreEngine) {
#ifdef _WIN32
        // cleanup the garbage
        if (win_cleanup_core) {
            win_cleanup_core(m_pCoreEngine);
            if (m_pCoreEngineY) win_cleanup_core(m_pCoreEngineY);
            if (m_pCoreEngineZ) win_cleanup_core(m_pCoreEngineZ);
        }
        if (hCoreLib) {
            FreeLibrary(hCoreLib);
            hCoreLib = nullptr;
        }
#else
        cleanup_core(m_pCoreEngine);
        if (m_pCoreEngineY) cleanup_core(m_pCoreEngineY);
        if (m_pCoreEngineZ) cleanup_core(m_pCoreEngineZ);
#endif
        m_pCoreEngine = nullptr;
        m_pCoreEngineY = nullptr;
        m_pCoreEngineZ = nullptr;
    }
}
