#include "BayckrcMAPLRSFirmwarePluginFactory.h"
#include "BayckrcMAPLRSFirmwarePlugin.h"

// Global instance self-registers with FirmwarePluginFactoryRegister via the
// FirmwarePluginFactory base class constructor.
BayckrcMAPLRSFirmwarePluginFactory BayckrcMAPLRSFirmwarePluginFactoryImp;

QList<QGCMAVLink::FirmwareClass_t> BayckrcMAPLRSFirmwarePluginFactory::supportedFirmwareClasses() const
{
    QList<QGCMAVLink::FirmwareClass_t> firmwareClasses;
    firmwareClasses.append(static_cast<QGCMAVLink::FirmwareClass_t>(MAV_AUTOPILOT_SMARTAP));
    return firmwareClasses;
}

FirmwarePlugin *BayckrcMAPLRSFirmwarePluginFactory::firmwarePluginForAutopilot(MAV_AUTOPILOT autopilotType, MAV_TYPE /*vehicleType*/)
{
    if (autopilotType == MAV_AUTOPILOT_SMARTAP) {
        if (!_pluginInstance) {
            _pluginInstance = new BayckrcMAPLRSFirmwarePlugin();
        }
        return _pluginInstance;
    }

    return nullptr;
}
