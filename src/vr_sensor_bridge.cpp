#include "vr_sensor_bridge.hpp"
#include <iostream>

VRSensorBridge::VRSensorBridge() : m_pCoreEngine(nullptr) {
}

VRSensorBridge::~VRSensorBridge() {
    Cleanup();
}

bool VRSensorBridge::Initialize() {
    // Spin up the proprietary core math engine
    m_pCoreEngine = init_core(1.0);
    if (!m_pCoreEngine) {
        std::cerr << "[SensorBridge] FATAL: Failed to init libmutemotion_core.so" << std::endl;
        return false;
    }
    return true;
}

MotionOffset VRSensorBridge::ProcessFrame(vr::IVRSystem* vrSystem, double dt) {
    MotionOffset out = {0.0, 0.0};
    if (!m_pCoreEngine || !vrSystem) return out;

    vr::TrackedDevicePose_t trackedDevicePoses[vr::k_unMaxTrackedDeviceCount];
    // We only care about the simulated velocity at the current exact time (fPredictedSecondsToPhotonsFromNow = 0.0)
    vrSystem->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, 0.0f, trackedDevicePoses, vr::k_unMaxTrackedDeviceCount);

    const vr::TrackedDevicePose_t& hmdPose = trackedDevicePoses[vr::k_unTrackedDeviceIndex_Hmd];
    
    if (hmdPose.bPoseIsValid) {
        // vAngularVelocity is a 3-dimensional vector (Pitch, Yaw, Roll) in radians per second
        double gyro_x = static_cast<double>(hmdPose.vAngularVelocity.v[0]); // Pitch
        double gyro_y = static_cast<double>(hmdPose.vAngularVelocity.v[1]); // Yaw
        double gyro_z = static_cast<double>(hmdPose.vAngularVelocity.v[2]); // Roll

        // OpenVR also provides linear velocity, which we substitute for raw accel
        // in our Complementary Filter implementation since the Core expects it
        double accel_x = static_cast<double>(hmdPose.vVelocity.v[0]);
        double accel_y = static_cast<double>(hmdPose.vVelocity.v[1]);
        double accel_z = static_cast<double>(hmdPose.vVelocity.v[2]);

        SensorData gData1 = { gyro_x, gyro_y, gyro_z };
        SensorData aData1 = { accel_x, accel_y, accel_z };
        
        SensorData gData2 = { gyro_y, gyro_x, gyro_z }; // Swapped for Y simulation
        SensorData aData2 = { accel_y, accel_x, accel_z };

        // Route directly into the proprietary math engine
        double coreOutX = process_motion(m_pCoreEngine, gData1, aData1, dt);
        double coreOutY = process_motion(m_pCoreEngine, gData2, aData2, dt); // Y mapping example

        out.x = coreOutX;
        out.y = coreOutY;
    }

    return out;
}

void VRSensorBridge::Cleanup() {
    if (m_pCoreEngine) {
        cleanup_core(m_pCoreEngine);
        m_pCoreEngine = nullptr;
    }
}
