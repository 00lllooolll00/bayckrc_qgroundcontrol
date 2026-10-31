import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

/// Map provider, type, and elevation provider selection.
/// Wraps a SettingsGroupLayout with cascading comboboxes driven by QGCMapEngineManager.
/// Models carry raw internal names in `value` and localized display text in `text`.
SettingsGroupLayout {
    Layout.fillWidth: true

    property var _mapEngineManager:     QGroundControl.mapEngineManager
    property Fact _mapProviderFact:     QGroundControl.settingsManager.flightMapSettings.mapProvider
    property Fact _mapTypeFact:         QGroundControl.settingsManager.flightMapSettings.mapType
    property Fact _elevationProviderFact: QGroundControl.settingsManager.flightMapSettings.elevationMapProvider

    function _indexOfValue(listModel, value) {
        for (var i = 0; i < listModel.count; i++) {
            if (listModel.get(i).value === value) return i
        }
        return -1
    }

    function _buildProviderModel() {
        providerModel.clear()
        var providers = _mapEngineManager.mapProviderList
        for (var i = 0; i < providers.length; i++) {
            providerModel.append({
                value: providers[i],
                text:  _mapEngineManager.mapProviderDisplayName(providers[i])
            })
        }
    }

    function _buildTypeModel(provider) {
        typeModel.clear()
        var types = _mapEngineManager.mapTypeList(provider)
        for (var i = 0; i < types.length; i++) {
            typeModel.append({
                value: types[i],
                text:  _mapEngineManager.mapTypeDisplayName(types[i])
            })
        }
    }

    function _buildElevationModel() {
        elevationModel.clear()
        var providers = _mapEngineManager.elevationProviderList
        for (var i = 0; i < providers.length; i++) {
            elevationModel.append({
                value: providers[i],
                text:  _mapEngineManager.mapProviderDisplayName(providers[i])
            })
        }
    }

    ListModel { id: providerModel }
    ListModel { id: typeModel }
    ListModel { id: elevationModel }

    LabelledComboBox {
        label: qsTr("Provider")
        model: providerModel

        onActivated: (index) => {
            var provider = providerModel.get(index).value
            _mapProviderFact.rawValue = provider
            _buildTypeModel(provider)
            if (typeModel.count > 0) {
                typeComboBox.currentIndex = 0
                _mapTypeFact.rawValue = typeModel.get(0).value
            }
        }

        Component.onCompleted: {
            _buildProviderModel()
            comboBox.textRole = "text"
            var index = _indexOfValue(providerModel, _mapProviderFact.rawValue)
            comboBox.currentIndex = index < 0 ? 0 : index
        }
    }

    LabelledComboBox {
        id:         typeComboBox
        label:      qsTr("Type")
        model:      typeModel

        onActivated: (index) => { _mapTypeFact.rawValue = typeModel.get(index).value }

        Component.onCompleted: {
            _buildTypeModel(_mapProviderFact.rawValue)
            comboBox.textRole = "text"
            var index = _indexOfValue(typeModel, _mapTypeFact.rawValue)
            comboBox.currentIndex = index < 0 ? 0 : index
        }
    }

    LabelledComboBox {
        label: qsTr("Elevation Provider")
        model: elevationModel

        onActivated: (index) => { _elevationProviderFact.rawValue = elevationModel.get(index).value }

        Component.onCompleted: {
            _buildElevationModel()
            comboBox.textRole = "text"
            var index = _indexOfValue(elevationModel, _elevationProviderFact.rawValue)
            comboBox.currentIndex = index < 0 ? 0 : index
        }
    }
}
