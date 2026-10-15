#include "BayckrcMAPLRSFirmwarePlugin.h"

#include "AppMessages.h"
#include "Vehicle.h"

bool BayckrcMAPLRSFirmwarePlugin::isCapable(const Vehicle *vehicle, FirmwareCapabilities capabilities) const
{
    switch (capabilities) {
    case FirmwarePlugin::GuidedModeCapability:
    case FirmwarePlugin::PauseVehicleCapability:
    case FirmwarePlugin::ChangeHeadingCapability:
        return true;
    default:
        return FirmwarePlugin::isCapable(vehicle, capabilities);
    }
}

void BayckrcMAPLRSFirmwarePlugin::initializeVehicle(Vehicle *vehicle)
{
    Q_UNUSED(vehicle);
}

QString BayckrcMAPLRSFirmwarePlugin::flightMode(uint8_t base_mode, uint32_t custom_mode) const
{
    // The bridge does not implement SET_MODE; fall back to the generic base-mode
    // flag naming (Manual/Guided/Auto/...) which is display-only and harmless.
    return FirmwarePlugin::flightMode(base_mode, custom_mode);
}

bool BayckrcMAPLRSFirmwarePlugin::setFlightMode(const QString &flightMode, uint8_t *base_mode, uint32_t *custom_mode) const
{
    Q_UNUSED(flightMode);
    Q_UNUSED(base_mode);
    Q_UNUSED(custom_mode);

    // MAV_CMD_DO_SET_MODE is not forwarded by the MAPLRS bridge. startMission()
    // below never calls _setFlightModeAndValidate(), so nothing depends on this.
    return false;
}

QList<MAV_CMD> BayckrcMAPLRSFirmwarePlugin::supportedMissionCommands(QGCMAVLinkTypes::VehicleClass_t /*vehicleClass*/) const
{
    // Commands forwarded by the GND bridge: NAV_WAYPOINT(16), NAV_LOITER_UNLIM(17),
    // NAV_LOITER_TURNS(18), NAV_LOITER_TIME(19), NAV_RETURN_TO_LAUNCH(20),
    // NAV_LAND(21), NAV_TAKEOFF(22), NAV_DELAY(93), DO_JUMP(177).
    // Anything else is filtered out of the Plan command selector.
    static const QList<MAV_CMD> commands = {
        MAV_CMD_NAV_WAYPOINT,
        MAV_CMD_NAV_LOITER_UNLIM,
        MAV_CMD_NAV_LOITER_TURNS,
        MAV_CMD_NAV_LOITER_TIME,
        MAV_CMD_NAV_RETURN_TO_LAUNCH,
        MAV_CMD_NAV_LAND,
        MAV_CMD_NAV_TAKEOFF,
        MAV_CMD_NAV_DELAY,
        MAV_CMD_DO_JUMP,
    };

    return commands;
}

void BayckrcMAPLRSFirmwarePlugin::startMission(Vehicle *vehicle) const
{
    // Same command order as APMFirmwarePlugin::startMission for multirotors on the
    // ground: arm (and wait for the armed heartbeat) first, then MAV_CMD_MISSION_START,
    // minus the DO_SET_MODE/Guided switch which the MAPLRS bridge does not forward.
    // The GND command slot is single-slot (a later COMMAND_LONG overwrites an earlier
    // one), so ARM_DISARM and MISSION_START must never be sent back-to-back.
    if (!vehicle->armed() && !_armVehicleAndValidate(vehicle)) {
        QGC::showAppMessage(tr("Unable to start mission: Vehicle failed to arm."));
        return;
    }

    vehicle->sendMavCommand(vehicle->defaultComponentId(), MAV_CMD_MISSION_START, true /* showError */);
}

void BayckrcMAPLRSFirmwarePlugin::guidedModeRTL(Vehicle *vehicle, bool smartRTL) const
{
    Q_UNUSED(smartRTL);

    vehicle->sendMavCommand(vehicle->defaultComponentId(), MAV_CMD_NAV_RETURN_TO_LAUNCH, true /* showError */);
}

void BayckrcMAPLRSFirmwarePlugin::pauseVehicle(Vehicle *vehicle) const
{
    vehicle->sendMavCommand(vehicle->defaultComponentId(), MAV_CMD_DO_PAUSE_CONTINUE, true /* showError */, 0.0f /* pause */);
}

bool BayckrcMAPLRSFirmwarePlugin::guidedModeGotoLocation(Vehicle *vehicle, const QGeoCoordinate &gotoCoord, double forwardFlightLoiterRadius) const
{
    // COMMAND_LONG 没有 frame 字段，INAV 按 MAV_FRAME_GLOBAL 解释 param7：绝对高度（MSL）。
    // 高度给 0 时 INAV 保持当前高度（navigation.c 的 wp255 分支），不会下潜。
    // INAV 对负的盘旋半径直接回 DENIED（不支持逆时针），这里钳到 0 降级为默认/顺时针。
    const float loiterRadius = (forwardFlightLoiterRadius > 0.0) ? static_cast<float>(forwardFlightLoiterRadius) : 0.0f;

    vehicle->sendMavCommand(vehicle->defaultComponentId(), MAV_CMD_DO_REPOSITION, true /* showError */,
                            0.0f,                                   // param1 速度，0 = 默认
                            0.0f,                                   // param2 flags
                            loiterRadius,                           // param3 盘旋半径，必须 >= 0
                            0.0f,                                   // param4 yaw，0 = 不指定朝向
                            static_cast<float>(gotoCoord.latitude()),
                            static_cast<float>(gotoCoord.longitude()),
                            static_cast<float>(gotoCoord.altitude()));
    return true;
}

void BayckrcMAPLRSFirmwarePlugin::guidedModeLand(Vehicle *vehicle) const
{
    // INAV 收到 NAV_LAND 走 activateForcedLanding()，不经过 GCS NAV 闸门；
    // 只要求已解锁且位置/高度/航向估计可用。GND 白名单已放行该命令（gcs_link.c:104-112）。
    vehicle->sendMavCommand(vehicle->defaultComponentId(), MAV_CMD_NAV_LAND, true /* showError */);
}

void BayckrcMAPLRSFirmwarePlugin::guidedModeChangeAltitude(Vehicle *vehicle, double altitudeChange, bool pauseVehicle)
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
        if (pauseVehicle) this->pauseVehicle(vehicle);
        return;
    }

    // 先悬停再改高：QGC 的发送队列逐条等 ACK，两条命令不会互相覆盖。
    if (pauseVehicle) this->pauseVehicle(vehicle);

    vehicle->sendMavCommand(vehicle->defaultComponentId(), MAV_CMD_DO_CHANGE_ALTITUDE, true /* showError */,
                            static_cast<float>(currentAltitude + altitudeChange),  // param1 目标高度
                            static_cast<float>(MAV_FRAME_GLOBAL_RELATIVE_ALT));   // param2 参考系：相对起飞点
}

void BayckrcMAPLRSFirmwarePlugin::guidedModeChangeHeading(Vehicle *vehicle, const QGeoCoordinate &headingCoord) const
{
    // INAV 的 CONDITION_YAW：param1 是目标航向角（度），param4=0 表示绝对角而非增量。
    // 不经过 GCS NAV 闸门，但要求当前导航状态具备航向控制（POSHOLD/CRUISE 等），否则回 DENIED。
    const double heading = vehicle->coordinate().azimuthTo(headingCoord);

    vehicle->sendMavCommand(vehicle->defaultComponentId(), MAV_CMD_CONDITION_YAW, true /* showError */,
                            static_cast<float>(heading), // param1 目标航向角
                            0.0f,                        // param2 角速度，INAV 未使用
                            1.0f,                        // param3 旋转方向，仅相对模式使用
                            0.0f);                       // param4 0 = 绝对航向
}
