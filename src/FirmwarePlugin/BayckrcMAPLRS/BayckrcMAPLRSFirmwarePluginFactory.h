#pragma once

#include "FirmwarePluginFactory.h"
#include "QGCMAVLink.h"

class BayckrcMAPLRSFirmwarePlugin;
class FirmwarePlugin;

class BayckrcMAPLRSFirmwarePluginFactory : public FirmwarePluginFactory
{
    Q_OBJECT

public:
    BayckrcMAPLRSFirmwarePluginFactory() = default;

    QList<QGCMAVLink::FirmwareClass_t> supportedFirmwareClasses() const final;
    FirmwarePlugin *firmwarePluginForAutopilot(MAV_AUTOPILOT autopilotType, MAV_TYPE vehicleType) final;

private:
    BayckrcMAPLRSFirmwarePlugin *_pluginInstance = nullptr;
};

extern BayckrcMAPLRSFirmwarePluginFactory BayckrcMAPLRSFirmwarePluginFactoryImp;
