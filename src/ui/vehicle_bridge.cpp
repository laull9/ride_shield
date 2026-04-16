#include "RideShield/ui/vehicle_bridge.h"

namespace RideShield::ui {

VehicleBridge::VehicleBridge(QObject* parent) : QObject(parent) {}

QString VehicleBridge::riskText() const {
    switch (risk_level_) {
    case 0:  return QStringLiteral("正常");
    case 1:  return QStringLiteral("提示");
    case 2:  return QStringLiteral("预警");
    case 3:  return QStringLiteral("紧急");
    default: return QStringLiteral("未知");
    }
}

void VehicleBridge::updateFusion(float score, int level, bool brake) {
    risk_score_   = score;
    risk_level_   = level;
    brake_allowed_ = brake;
    emit riskChanged();
}

void VehicleBridge::updateFront(float ttc, float rate, int count, const QVariantList& dets) {
    ttc_seconds_    = ttc;
    approach_rate_  = rate;
    front_det_count_ = count;
    front_dets_     = dets;
    emit frontChanged();
}

void VehicleBridge::updateRear(bool left, bool center, bool right) {
    left_rear_   = left;
    center_rear_ = center;
    right_rear_  = right;
    emit rearChanged();
}

void VehicleBridge::updateDriver(float fatigue) {
    driver_fatigue_ = fatigue;
    emit driverChanged();
}

void VehicleBridge::updateImu(float latG) {
    imu_lat_g_ = latG;
    emit imuChanged();
}

void VehicleBridge::updateHr(int bpm) {
    heart_rate_ = bpm;
    emit hrChanged();
}

void VehicleBridge::updateSpeed(float kmh) {
    speed_ = kmh;
    emit speedChanged();
}

}  // namespace RideShield::ui
