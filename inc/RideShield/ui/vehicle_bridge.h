#pragma once

#include <QObject>
#include <QVariantList>

namespace RideShield::ui {

/// 车辆数据桥接层 —— 将 RideShield 核心数据以 Q_PROPERTY 暴露给 QML。
/// 完全去耦合：不直接引用推理/感知类，仅传递 POD 级数据。
class VehicleBridge : public QObject {
    Q_OBJECT

    // ── 融合决策 ──
    Q_PROPERTY(float riskScore READ riskScore NOTIFY riskChanged)
    Q_PROPERTY(int   riskLevel READ riskLevel NOTIFY riskChanged)   // 0-3
    Q_PROPERTY(QString riskText READ riskText NOTIFY riskChanged)
    Q_PROPERTY(bool  brakeAllowed READ brakeAllowed NOTIFY riskChanged)

    // ── 前向感知 ──
    Q_PROPERTY(float ttcSeconds READ ttcSeconds NOTIFY frontChanged)
    Q_PROPERTY(float approachRate READ approachRate NOTIFY frontChanged)
    Q_PROPERTY(int   frontDetectionCount READ frontDetectionCount NOTIFY frontChanged)
    Q_PROPERTY(QVariantList frontDetections READ frontDetections NOTIFY frontChanged)

    // ── 后向感知 ──
    Q_PROPERTY(bool leftRearOccupied  READ leftRearOccupied  NOTIFY rearChanged)
    Q_PROPERTY(bool centerRearOccupied READ centerRearOccupied NOTIFY rearChanged)
    Q_PROPERTY(bool rightRearOccupied READ rightRearOccupied NOTIFY rearChanged)

    // ── 驾驶员 / IMU / 心率 ──
    Q_PROPERTY(float driverFatigueScore READ driverFatigueScore NOTIFY driverChanged)
    Q_PROPERTY(float imuLateralG READ imuLateralG NOTIFY imuChanged)
    Q_PROPERTY(int   heartRate READ heartRate NOTIFY hrChanged)

    // ── 车速 ──
    Q_PROPERTY(float speed READ speed NOTIFY speedChanged)

    // ── 模块可用性（未实现的模块显示为"无"）──
    Q_PROPERTY(bool driverAvailable READ driverAvailable NOTIFY driverChanged)
    Q_PROPERTY(bool imuAvailable    READ imuAvailable    NOTIFY imuChanged)
    Q_PROPERTY(bool hrAvailable     READ hrAvailable     NOTIFY hrChanged)
    Q_PROPERTY(bool rearAvailable   READ rearAvailable   NOTIFY rearChanged)
    Q_PROPERTY(bool speedAvailable  READ speedAvailable  NOTIFY speedChanged)

public:
    explicit VehicleBridge(QObject* parent = nullptr);

    // ---------- 读取器 ----------
    [[nodiscard]] float   riskScore()      const { return risk_score_; }
    [[nodiscard]] int     riskLevel()      const { return risk_level_; }
    [[nodiscard]] QString riskText()       const;
    [[nodiscard]] bool    brakeAllowed()   const { return brake_allowed_; }

    [[nodiscard]] float   ttcSeconds()     const { return ttc_seconds_; }
    [[nodiscard]] float   approachRate()   const { return approach_rate_; }
    [[nodiscard]] int     frontDetectionCount() const { return front_det_count_; }
    [[nodiscard]] QVariantList frontDetections() const { return front_dets_; }

    [[nodiscard]] bool leftRearOccupied()   const { return left_rear_; }
    [[nodiscard]] bool centerRearOccupied() const { return center_rear_; }
    [[nodiscard]] bool rightRearOccupied()  const { return right_rear_; }

    [[nodiscard]] float driverFatigueScore() const { return driver_fatigue_; }
    [[nodiscard]] float imuLateralG()       const { return imu_lat_g_; }
    [[nodiscard]] int   heartRate()         const { return heart_rate_; }
    [[nodiscard]] float speed()             const { return speed_; }

    [[nodiscard]] bool driverAvailable() const { return driver_available_; }
    [[nodiscard]] bool imuAvailable()    const { return imu_available_; }
    [[nodiscard]] bool hrAvailable()     const { return hr_available_; }
    [[nodiscard]] bool rearAvailable()   const { return rear_available_; }
    [[nodiscard]] bool speedAvailable()  const { return speed_available_; }

    // ---------- 逻辑层调用的写入接口 ----------
    void updateFusion(float score, int level, bool brake);
    void updateFront(float ttc, float rate, int count, const QVariantList& dets);
    void updateRear(bool left, bool center, bool right);
    void updateDriver(float fatigue);
    void updateImu(float latG);
    void updateHr(int bpm);
    void updateSpeed(float kmh);

signals:
    void riskChanged();
    void frontChanged();
    void rearChanged();
    void driverChanged();
    void imuChanged();
    void hrChanged();
    void speedChanged();

private:
    float risk_score_{};
    int   risk_level_{};
    bool  brake_allowed_{};

    float ttc_seconds_{99.f};
    float approach_rate_{};
    int   front_det_count_{};
    QVariantList front_dets_;

    bool left_rear_{};
    bool center_rear_{};
    bool right_rear_{};

    float driver_fatigue_{};
    float imu_lat_g_{};
    int   heart_rate_{72};
    float speed_{};

    bool driver_available_{};
    bool imu_available_{};
    bool hr_available_{};
    bool rear_available_{};
    bool speed_available_{};
};

}  // namespace RideShield::ui
