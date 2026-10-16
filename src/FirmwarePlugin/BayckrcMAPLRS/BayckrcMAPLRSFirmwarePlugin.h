#pragma once

#include "FirmwarePlugin.h"

class Vehicle;

/// Firmware plugin for BayckRC MAPLRS ground station bridge vehicles.
/// The AIR side reports MAV_AUTOPILOT 18 (MAV_AUTOPILOT_SMARTAP) in its heartbeat;
/// the bridge forwards COMMAND_LONG directly to the flight controller:
/// ARM_DISARM(400), NAV_RETURN_TO_LAUNCH(20), DO_PAUSE_CONTINUE(193), MISSION_START(300).
class BayckrcMAPLRSFirmwarePlugin : public FirmwarePlugin
{
    Q_OBJECT

public:
    // FirmwarePlugin overrides
    bool isCapable(const Vehicle *vehicle, FirmwareCapabilities capabilities) const override;
    void initializeVehicle(Vehicle *vehicle) override;
    QString flightMode(uint8_t base_mode, uint32_t custom_mode) const override;
    bool setFlightMode(const QString &flightMode, uint8_t *base_mode, uint32_t *custom_mode) const override;
    QList<MAV_CMD> supportedMissionCommands(QGCMAVLinkTypes::VehicleClass_t vehicleClass) const override;
    bool sendHomePositionToVehicle() const override { return false; }
    void startMission(Vehicle *vehicle) const override;
    void startTakeoff(Vehicle *vehicle) const override;
    void guidedModeRTL(Vehicle *vehicle, bool smartRTL) const override;
    void pauseVehicle(Vehicle *vehicle) const override;
    bool guidedModeGotoLocation(Vehicle *vehicle, const QGeoCoordinate &gotoCoord, double forwardFlightLoiterRadius) const override;
    void guidedModeLand(Vehicle *vehicle) const override;
    void guidedModeChangeAltitude(Vehicle *vehicle, double altitudeChange, bool pauseVehicle) override;
    void guidedModeChangeHeading(Vehicle *vehicle, const QGeoCoordinate &headingCoord) const override;
};
