#include "BayckrcMAPLRSFirmwarePlugin.h"

#include "AppMessages.h"
#include "Vehicle.h"

bool BayckrcMAPLRSFirmwarePlugin::isCapable(const Vehicle *vehicle, FirmwareCapabilities capabilities) const
{
    switch (capabilities) {
    case FirmwarePlugin::GuidedModeCapability:
    case FirmwarePlugin::PauseVehicleCapability:
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
