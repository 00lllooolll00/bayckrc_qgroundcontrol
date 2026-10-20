#include "BayckrcMAPLRSFirmwarePlugin.h"

#include <QtCore/QSet>
#include <QtCore/QTimer>
#include <functional>

#include "AppMessages.h"
#include "Vehicle.h"

// 在途解锁等待（按机地址区分）：连点时不重复解锁，也避免同一台机并发两个等待。
static QSet<const Vehicle*> s_arming;

// 解锁态到 QGC 要多绕三跳：AIR 从飞控心跳摘 SAFETY_ARMED → 无线遥测回传 → GND 的 1Hz
// 心跳才转发给 QGC，最坏约 2 秒。基类 _armVehicleAndValidate 只等 1.5 秒（按原生数传
// 时序定的），会误报 "failed to arm" —— 命令其实已生效，只是 QGC 还没看到回报。
// 等待由 armedChanged 信号 + 超时定时器驱动，不阻塞 UI 线程；结果通过 done 回调返回。
static void _armAndWait(Vehicle* vehicle, int timeoutMs, std::function<void(bool)> done)
{
    if (vehicle->armed()) {
        done(true);
        return;
    }
    if (s_arming.contains(vehicle)) {
        return;  // 已有一个解锁等待在途中，忽略重复请求。
    }
    s_arming.insert(vehicle);
    QObject::connect(vehicle, &QObject::destroyed, vehicle, [vehicle]() { s_arming.remove(vehicle); });

    // 只尝试解锁一次：带延迟重试解锁会有安全风险（与基类保持一致）。
    vehicle->setArmed(true, false /* showError */);

    auto* timer = new QTimer(vehicle);
    timer->setSingleShot(true);
    timer->setInterval(timeoutMs);

    // 连接以 timer 为 context：timer 销毁时连接自动断开；isActive() 兼作"等待未结束"判据。
    QObject::connect(vehicle, &Vehicle::armedChanged, timer, [vehicle, timer, done](bool armed) {
        if (!armed || !timer->isActive()) {
            return;  // 上锁事件或等待已结束（成功/超时）都不重复回调。
        }
        timer->stop();
        timer->deleteLater();
        s_arming.remove(vehicle);
        done(true);
    });
    QObject::connect(timer, &QTimer::timeout, timer, [vehicle, timer, done]() {
        timer->deleteLater();
        s_arming.remove(vehicle);
        done(vehicle->armed());
    });

    timer->start();
}

bool BayckrcMAPLRSFirmwarePlugin::isCapable(const Vehicle* vehicle, FirmwareCapabilities capabilities) const
{
    switch (capabilities) {
        case FirmwarePlugin::GuidedModeCapability:
        case FirmwarePlugin::PauseVehicleCapability:
        case FirmwarePlugin::ChangeHeadingCapability:
        case FirmwarePlugin::TakeoffVehicleCapability:
            return true;
        default:
            return FirmwarePlugin::isCapable(vehicle, capabilities);
    }
}

void BayckrcMAPLRSFirmwarePlugin::initializeVehicle(Vehicle* vehicle)
{
    Q_UNUSED(vehicle);
}

QString BayckrcMAPLRSFirmwarePlugin::flightMode(uint8_t base_mode, uint32_t custom_mode) const
{
    // custom_mode 是飞控真实模式号：INAV 把自己的模式映射成 ArduPilot 编号回填
    // （inav `mavlink_modes.c` 的 inavToArduCopterMap），这里翻回显示名。
    // 机型按多旋翼编号解释（GND 合成心跳固定回 QUADROTOR）；将来支持固定翼要按机型分表，
    // 但 flightMode() 拿不到机型，得先改 Vehicle 侧接口。
    if (base_mode & MAV_MODE_FLAG_CUSTOM_MODE_ENABLED) {
        switch (custom_mode) {
            case 0:
                return QStringLiteral("STABILIZE");
            case 1:
                return QStringLiteral("ACRO");
            case 2:
                return QStringLiteral("ALT HOLD");
            case 3:
                return QStringLiteral("AUTO");
            case 4:
                return QStringLiteral("GUIDED");
            case 5:
                return QStringLiteral("LOITER");
            case 6:
                return QStringLiteral("RTL");
            case 9:
                return QStringLiteral("LAND");
            case 16:
                return QStringLiteral("POSHOLD");
            case 17:
                return QStringLiteral("BRAKE");
            case 18:
                return QStringLiteral("THROW");
            default:
                break;
        }
        return tr("Mode %1").arg(custom_mode);
    }

    // 飞控还没上报过模式（custom_mode 位未置）时退回基类的通用命名。
    return FirmwarePlugin::flightMode(base_mode, custom_mode);
}

bool BayckrcMAPLRSFirmwarePlugin::setFlightMode(const QString& flightMode, uint8_t* base_mode,
                                                uint32_t* custom_mode) const
{
    Q_UNUSED(flightMode);
    Q_UNUSED(base_mode);
    Q_UNUSED(custom_mode);

    // MAV_CMD_DO_SET_MODE is not forwarded by the MAPLRS bridge. startMission()
    // below never calls _setFlightModeAndValidate(), so nothing depends on this.
    return false;
}

QList<MAV_CMD> BayckrcMAPLRSFirmwarePlugin::supportedMissionCommands(
    QGCMAVLinkTypes::VehicleClass_t /*vehicleClass*/) const
{
    // INAV mavlink_mission.c 实际支持、且 QGC 命令表里有 UI 定义的任务项。
    // NAV_TAKEOFF(22) 由 GND 侧 gcs_link 改写成同序号航点后再转发，所以任务规划里
    // 可以保留起飞项；VTOL 起降和样条航点没有改写，INAV 会拒，不列出。
    static const QList<MAV_CMD> commands = {
        MAV_CMD_NAV_WAYPOINT,
        MAV_CMD_NAV_LOITER_TIME,
        MAV_CMD_NAV_RETURN_TO_LAUNCH,
        MAV_CMD_NAV_LAND,
        MAV_CMD_NAV_TAKEOFF,
        MAV_CMD_CONDITION_DELAY,
        MAV_CMD_CONDITION_CHANGE_ALT,
        MAV_CMD_CONDITION_YAW,
        MAV_CMD_DO_JUMP,
        MAV_CMD_DO_CHANGE_SPEED,
        MAV_CMD_DO_SET_ROI,
    };

    return commands;
}

void BayckrcMAPLRSFirmwarePlugin::startTakeoff(Vehicle* vehicle) const
{
    // Same command order as startMission: arm (and wait for the armed heartbeat) first, then
    // MAV_CMD_NAV_TAKEOFF — INAV refuses to arm while the throttle channel is high, and the GND
    // command slot is single-slot (a later COMMAND_LONG overwrites an earlier one), so the two
    // commands must never be sent back-to-back.
    _armAndWait(vehicle, 3000, [vehicle](bool armedOk) {
        if (!armedOk) {
            QGC::showAppMessage(BayckrcMAPLRSFirmwarePlugin::tr("Unable to start takeoff: Vehicle failed to arm."));
            return;
        }
        // AIR 把起飞翻译成油门档位，参数不参与执行，回执由 AIR 自造 ACCEPTED。
        vehicle->sendMavCommand(vehicle->defaultComponentId(), MAV_CMD_NAV_TAKEOFF, true /* showError */,
                                0.0f /* pitch */, 0.0f, 0.0f, 0.0f /* yaw */, 0.0, 0.0, 0.0 /* lat/lon/alt 未使用 */);
    });
}

void BayckrcMAPLRSFirmwarePlugin::startMission(Vehicle* vehicle) const
{
    // Same command order as APMFirmwarePlugin::startMission for multirotors on the
    // ground: arm (and wait for the armed heartbeat) first, then MAV_CMD_MISSION_START,
    // minus the DO_SET_MODE/Guided switch which the MAPLRS bridge does not forward.
    // The GND command slot is single-slot (a later COMMAND_LONG overwrites an earlier
    // one), so ARM_DISARM and MISSION_START must never be sent back-to-back.
    _armAndWait(vehicle, 5000, [vehicle](bool armedOk) {
        if (!armedOk) {
            QGC::showAppMessage(BayckrcMAPLRSFirmwarePlugin::tr("Unable to start mission: Vehicle failed to arm."));
            return;
        }
        vehicle->sendMavCommand(vehicle->defaultComponentId(), MAV_CMD_MISSION_START, true /* showError */);
    });
}

void BayckrcMAPLRSFirmwarePlugin::guidedModeRTL(Vehicle* vehicle, bool smartRTL) const
{
    Q_UNUSED(smartRTL);

    vehicle->sendMavCommand(vehicle->defaultComponentId(), MAV_CMD_NAV_RETURN_TO_LAUNCH, true /* showError */);
}

void BayckrcMAPLRSFirmwarePlugin::pauseVehicle(Vehicle* vehicle) const
{
    vehicle->sendMavCommand(vehicle->defaultComponentId(), MAV_CMD_DO_PAUSE_CONTINUE, true /* showError */,
                            0.0f /* pause */);
}

bool BayckrcMAPLRSFirmwarePlugin::guidedModeGotoLocation(Vehicle* vehicle, const QGeoCoordinate& gotoCoord,
                                                         double forwardFlightLoiterRadius) const
{
    // COMMAND_LONG 没有 frame 字段，INAV 按 MAV_FRAME_GLOBAL 解释 param7：绝对高度（MSL）。
    // 高度给 0 时 INAV 保持当前高度（navigation.c 的 wp255 分支），不会下潜。
    // INAV 对负的盘旋半径直接回 DENIED（不支持逆时针），这里钳到 0 降级为默认/顺时针。
    const float loiterRadius = (forwardFlightLoiterRadius > 0.0) ? static_cast<float>(forwardFlightLoiterRadius) : 0.0f;

    vehicle->sendMavCommand(vehicle->defaultComponentId(), MAV_CMD_DO_REPOSITION, true /* showError */,
                            0.0f,          // param1 速度，0 = 默认
                            0.0f,          // param2 flags
                            loiterRadius,  // param3 盘旋半径，必须 >= 0
                            0.0f,          // param4 yaw，0 = 不指定朝向
                            static_cast<float>(gotoCoord.latitude()), static_cast<float>(gotoCoord.longitude()),
                            static_cast<float>(gotoCoord.altitude()));
    return true;
}

void BayckrcMAPLRSFirmwarePlugin::guidedModeLand(Vehicle* vehicle) const
{
    // INAV 收到 NAV_LAND 走 activateForcedLanding()，不经过 GCS NAV 闸门；
    // 只要求已解锁且位置/高度/航向估计可用。GND 白名单已放行该命令（gcs_link.c:104-112）。
    vehicle->sendMavCommand(vehicle->defaultComponentId(), MAV_CMD_NAV_LAND, true /* showError */);
}

void BayckrcMAPLRSFirmwarePlugin::guidedModeChangeAltitude(Vehicle* vehicle, double altitudeChange, bool pauseVehicle)
{
    // QGC 传进来的是"相对当前高度的增量"，INAV 的 DO_CHANGE_ALTITUDE 收的是目标高度本身；
    // param2 给 MAV_FRAME_GLOBAL_RELATIVE_ALT，让它按"相对起飞点"解释，不依赖 MSL/gpsOrigin。
    // INAV 侧只要求当前导航状态具备高度控制（POSHOLD/ALTHOLD 等），不经过 GCS NAV 闸门。
    const double currentAltitude = vehicle->altitudeRelative()->rawValue().toDouble();
    if (qIsNaN(currentAltitude)) {
        QGC::showAppMessage(tr("Unable to change altitude, vehicle altitude not known."));
        return;
    }

    // 滑条没动就只做暂停（QGC 的 Pause 动作也走这个入口），避免发无意义的改高命令。
    if (qAbs(altitudeChange) < 0.01) {
        if (pauseVehicle)
            this->pauseVehicle(vehicle);
        return;
    }

    // 先悬停再改高：QGC 的发送队列逐条等 ACK，两条命令不会互相覆盖。
    if (pauseVehicle)
        this->pauseVehicle(vehicle);

    vehicle->sendMavCommand(vehicle->defaultComponentId(), MAV_CMD_DO_CHANGE_ALTITUDE, true /* showError */,
                            static_cast<float>(currentAltitude + altitudeChange),  // param1 目标高度
                            static_cast<float>(MAV_FRAME_GLOBAL_RELATIVE_ALT));    // param2 参考系：相对起飞点
}

void BayckrcMAPLRSFirmwarePlugin::guidedModeChangeHeading(Vehicle* vehicle, const QGeoCoordinate& headingCoord) const
{
    // INAV 的 CONDITION_YAW：param1 是目标航向角（度），param4=0 表示绝对角而非增量。
    // 不经过 GCS NAV 闸门，但要求当前导航状态具备航向控制（POSHOLD/CRUISE 等），否则回 DENIED。
    const double heading = vehicle->coordinate().azimuthTo(headingCoord);

    vehicle->sendMavCommand(vehicle->defaultComponentId(), MAV_CMD_CONDITION_YAW, true /* showError */,
                            static_cast<float>(heading),  // param1 目标航向角
                            0.0f,                         // param2 角速度，INAV 未使用
                            1.0f,                         // param3 旋转方向，仅相对模式使用
                            0.0f);                        // param4 0 = 绝对航向
}
