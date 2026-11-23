#include "SpaceArmDisarmController.h"

#include <QtCore/QEvent>
#include <QtCore/QStringList>
#include <QtGui/QKeyEvent>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>

#include "MultiVehicleManager.h"
#include "QGCLoggingCategory.h"
#include "QmlObjectListModel.h"
#include "Vehicle.h"

QGC_LOGGING_CATEGORY(SpaceArmDisarmControllerLog, "QMLControls.SpaceArmDisarmController")

namespace {
constexpr int kHoldTickIntervalMs = 16;
constexpr int kMinHoldDurationMs = 50;
}  // namespace

SpaceArmDisarmController::SpaceArmDisarmController(QObject* parent) : QObject(parent)
{
    _holdTickTimer.setInterval(kHoldTickIntervalMs);
    connect(&_holdTickTimer, &QTimer::timeout, this, &SpaceArmDisarmController::_onHoldTick);
}

SpaceArmDisarmController::~SpaceArmDisarmController()
{
    _holdTickTimer.stop();
    if (_window) {
        _window->removeEventFilter(this);
    }
}

void SpaceArmDisarmController::setWindow(QQuickWindow* window)
{
    if (_window == window) {
        return;
    }

    if (_window) {
        _window->removeEventFilter(this);
    }

    _cancelHold();
    _latched = false;
    _window = window;

    if (_window) {
        _window->installEventFilter(this);
    }

    emit windowChanged();
}

void SpaceArmDisarmController::setEnabled(bool enabled)
{
    if (_enabled == enabled) {
        return;
    }

    _enabled = enabled;
    if (!_enabled) {
        _cancelHold();
        _latched = false;
    }

    emit enabledChanged();
}

void SpaceArmDisarmController::setHoldDurationMs(int holdDurationMs)
{
    const int clamped = qMax(kMinHoldDurationMs, holdDurationMs);
    if (_holdDurationMs == clamped) {
        return;
    }

    _holdDurationMs = clamped;
    emit holdDurationMsChanged();
}

bool SpaceArmDisarmController::eventFilter(QObject* watched, QEvent* event)
{
    if (!event || watched != _window.data()) {
        return QObject::eventFilter(watched, event);
    }

    switch (event->type()) {
        case QEvent::FocusOut:
        case QEvent::WindowDeactivate:
            _cancelHold();
            _latched = false;
            return false;
        case QEvent::KeyPress: {
            auto* const keyEvent = static_cast<QKeyEvent*>(event);
            if (!_enabled || keyEvent->key() != Qt::Key_Space) {
                return false;
            }

            const Qt::KeyboardModifiers mods = keyEvent->modifiers() & ~Qt::KeypadModifier;
            if (mods != Qt::NoModifier && mods != Qt::ShiftModifier) {
                return false;
            }

            if (keyEvent->isAutoRepeat()) {
                return _holding || _latched;
            }

            if (_isTextOrProtectedInputFocused()) {
                return false;
            }

            return _handleSpacePressed(mods == Qt::ShiftModifier);
        }
        case QEvent::KeyRelease: {
            auto* const keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->key() != Qt::Key_Space || keyEvent->isAutoRepeat()) {
                return false;
            }

            if (_holding || _latched) {
                _cancelHold();
                _latched = false;
                return true;
            }
            return false;
        }
        default:
            break;
    }

    return QObject::eventFilter(watched, event);
}

bool SpaceArmDisarmController::_isTextOrProtectedInputFocused() const
{
    if (!_window) {
        return false;
    }

    QQuickItem* focusItem = _window->activeFocusItem();
    if (!focusItem) {
        return false;
    }

    if (focusItem->flags().testFlag(QQuickItem::ItemAcceptsInputMethod)) {
        return true;
    }

    for (QQuickItem* item = focusItem; item != nullptr; item = item->parentItem()) {
        if (item->inherits("QQuickTextInput") || item->inherits("QQuickTextEdit") || item->inherits("QQuickComboBox") ||
            item->inherits("QQuickSpinBox")) {
            return true;
        }
        const QString className = QString::fromLatin1(item->metaObject()->className());
        if (className.contains(QLatin1String("SliderSwitch"))) {
            return true;
        }
    }

    return false;
}

QList<Vehicle*> SpaceArmDisarmController::_collectSelectedVehicles() const
{
    QList<Vehicle*> result;
    MultiVehicleManager* const manager = MultiVehicleManager::instance();
    if (!manager || !manager->selectedVehicles()) {
        return result;
    }

    QmlObjectListModel* const selected = manager->selectedVehicles();
    for (int i = 0; i < selected->count(); ++i) {
        Vehicle* const vehicle = qobject_cast<Vehicle*>(selected->get(i));
        if (vehicle) {
            result.append(vehicle);
        }
    }
    return result;
}

QString SpaceArmDisarmController::_formatVehicleIds(const QList<Vehicle*>& vehicles)
{
    QStringList ids;
    ids.reserve(vehicles.size());
    for (Vehicle* const vehicle : vehicles) {
        if (vehicle) {
            ids.append(QStringLiteral("#%1").arg(vehicle->id()));
        }
    }
    return ids.join(QStringLiteral(", "));
}

bool SpaceArmDisarmController::_handleSpacePressed(bool isMulti)
{
    if (_holding || _latched) {
        return true;
    }

    if (isMulti) {
        const QList<Vehicle*> selected = _collectSelectedVehicles();
        if (selected.isEmpty()) {
            _latched = true;
            emit actionFeedback(tr("未选中任何载具"), false);
            return true;
        }

        QList<Vehicle*> armedVehicles;
        for (Vehicle* const vehicle : selected) {
            if (vehicle && vehicle->armed()) {
                armedVehicles.append(vehicle);
            }
        }

        if (!armedVehicles.isEmpty()) {
            _startHold(true, true,
                       tr("长按 Shift+空格 紧急上锁 (%1台)：%2")
                           .arg(armedVehicles.size())
                           .arg(_formatVehicleIds(armedVehicles)));
            return true;
        }

        _startHold(true, false,
                   tr("长按 Shift+空格 批量解锁 (%1台)：%2").arg(selected.size()).arg(_formatVehicleIds(selected)));
        return true;
    }

    MultiVehicleManager* const manager = MultiVehicleManager::instance();
    Vehicle* const activeVehicle = manager ? manager->activeVehicle() : nullptr;
    if (!activeVehicle) {
        return false;
    }

    if (activeVehicle->armed()) {
        _startHold(false, true, tr("长按空格紧急上锁：#%1").arg(activeVehicle->id()));
        return true;
    }

    _startHold(false, false, tr("长按空格解锁：#%1").arg(activeVehicle->id()));
    return true;
}

void SpaceArmDisarmController::_startHold(bool isMulti, bool isEmergencyDisarm, const QString& promptText)
{
    _holdElapsed.start();
    _setHoldState(true, isMulti, isEmergencyDisarm, 0.0, promptText);
    _holdTickTimer.start();
}

void SpaceArmDisarmController::_cancelHold()
{
    if (_holdTickTimer.isActive()) {
        _holdTickTimer.stop();
    }
    _setHoldState(false, false, false, 0.0, QString());
}

void SpaceArmDisarmController::_setHoldState(bool holding, bool multiVehicleHold, bool holdingEmergencyDisarm,
                                             qreal progress, const QString& promptText)
{
    const bool holdingDidChange = (_holding != holding);
    const bool multiChanged = (_multiVehicleHold != multiVehicleHold);
    const bool emergencyChanged = (_holdingEmergencyDisarm != holdingEmergencyDisarm);
    const bool progressChanged = !qFuzzyCompare(1.0 + _holdProgress, 1.0 + progress);
    const bool promptChanged = (_holdPromptText != promptText);

    _holding = holding;
    _multiVehicleHold = multiVehicleHold;
    _holdingEmergencyDisarm = holdingEmergencyDisarm;
    _holdProgress = progress;
    _holdPromptText = promptText;

    if (holdingDidChange) {
        emit holdingChanged();
    }
    if (multiChanged) {
        emit multiVehicleHoldChanged();
    }
    if (emergencyChanged) {
        emit holdingEmergencyDisarmChanged();
    }
    if (progressChanged) {
        emit holdProgressChanged();
    }
    if (promptChanged) {
        emit holdPromptTextChanged();
    }
}

void SpaceArmDisarmController::_onHoldTick()
{
    if (!_holding || !_enabled) {
        _cancelHold();
        return;
    }

    const qint64 elapsed = _holdElapsed.elapsed();
    if (elapsed < _holdDurationMs) {
        const qreal progress = qBound(0.0, static_cast<qreal>(elapsed) / static_cast<qreal>(_holdDurationMs), 1.0);
        _setHoldState(true, _multiVehicleHold, _holdingEmergencyDisarm, progress, _holdPromptText);
        return;
    }

    const bool isMulti = _multiVehicleHold;
    const bool isEmergencyDisarm = _holdingEmergencyDisarm;
    _holdTickTimer.stop();
    _latched = true;
    _setHoldState(false, false, false, 0.0, QString());

    if (isMulti) {
        const QList<Vehicle*> selected = _collectSelectedVehicles();
        QList<Vehicle*> targets;
        for (Vehicle* const vehicle : selected) {
            if (!vehicle) {
                continue;
            }
            if (isEmergencyDisarm && vehicle->armed()) {
                vehicle->emergencyStop();
                targets.append(vehicle);
            } else if (!isEmergencyDisarm && !vehicle->armed()) {
                vehicle->setArmedShowError(true);
                targets.append(vehicle);
            }
        }
        if (!targets.isEmpty()) {
            const QString ids = _formatVehicleIds(targets);
            if (isEmergencyDisarm) {
                qCInfo(SpaceArmDisarmControllerLog) << "Multi-vehicle emergency stop:" << ids;
                emit actionFeedback(tr("紧急上锁 %1 台：%2").arg(targets.size()).arg(ids), true);
            } else {
                qCInfo(SpaceArmDisarmControllerLog) << "Multi-vehicle arm:" << ids;
                emit actionFeedback(tr("已发送批量解锁 %1 台：%2").arg(targets.size()).arg(ids), false);
            }
        }
        return;
    }

    MultiVehicleManager* const manager = MultiVehicleManager::instance();
    Vehicle* const activeVehicle = manager ? manager->activeVehicle() : nullptr;
    if (!activeVehicle) {
        return;
    }

    if (isEmergencyDisarm && activeVehicle->armed()) {
        activeVehicle->emergencyStop();
        qCInfo(SpaceArmDisarmControllerLog) << "Single-vehicle emergency stop: #" << activeVehicle->id();
        emit actionFeedback(tr("紧急上锁：#%1").arg(activeVehicle->id()), true);
    } else if (!isEmergencyDisarm && !activeVehicle->armed()) {
        activeVehicle->setArmedShowError(true);
        qCInfo(SpaceArmDisarmControllerLog) << "Single-vehicle arm: #" << activeVehicle->id();
        emit actionFeedback(tr("已发送解锁：#%1").arg(activeVehicle->id()), false);
    }
}
