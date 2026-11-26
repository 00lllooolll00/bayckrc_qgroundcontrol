#include "KeyboardNudgeController.h"

#include <cmath>

#include <QtCore/QEvent>
#include <QtCore/QtMath>
#include <QtGui/QKeyEvent>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>

#include "FlyViewSettings.h"
#include "MultiVehicleManager.h"
#include "QGCLoggingCategory.h"
#include "SettingsManager.h"
#include "Vehicle.h"

QGC_LOGGING_CATEGORY(KeyboardNudgeControllerLog, "QMLControls.KeyboardNudgeController")

namespace {
constexpr int kMergeDebounceMs = 220;
constexpr int kMinIntervalFloorMs = 250;
constexpr int kChainWindowMs = 2500;
constexpr double kMinStepMeters = 1.0;
constexpr double kMaxStepMeters = 100.0;
}  // namespace

KeyboardNudgeController::KeyboardNudgeController(QObject* parent) : QObject(parent)
{
    _flushTimer.setSingleShot(true);
    connect(&_flushTimer, &QTimer::timeout, this, &KeyboardNudgeController::_flushNudge);
}

KeyboardNudgeController::~KeyboardNudgeController()
{
    _flushTimer.stop();
    if (_window) {
        _window->removeEventFilter(this);
    }
}

void KeyboardNudgeController::setWindow(QQuickWindow* window)
{
    if (_window == window) {
        return;
    }
    if (_window) {
        _window->removeEventFilter(this);
    }

    _resetPending();
    _window = window;

    if (_window) {
        _window->installEventFilter(this);
    }
    emit windowChanged();
}

void KeyboardNudgeController::setEnabled(bool enabled)
{
    if (_enabled == enabled) {
        return;
    }

    _enabled = enabled;
    if (!_enabled) {
        _resetPending();
    }
    emit enabledChanged();
}

void KeyboardNudgeController::setHeadingRelative(bool headingRelative)
{
    if (_headingRelative == headingRelative) {
        return;
    }
    _headingRelative = headingRelative;
    emit headingRelativeChanged();
}

void KeyboardNudgeController::nudge(int forwardSign, int rightSign)
{
    if (!_enabled) {
        return;
    }
    _enqueueNudge(qBound(-1, forwardSign, 1), qBound(-1, rightSign, 1), false);
}

bool KeyboardNudgeController::eventFilter(QObject* watched, QEvent* event)
{
    if (!event || watched != _window.data()) {
        return QObject::eventFilter(watched, event);
    }

    switch (event->type()) {
        case QEvent::FocusOut:
        case QEvent::WindowDeactivate:
            _resetPending();
            return false;
        case QEvent::KeyPress: {
            if (!_enabled) {
                return false;
            }
            auto* const keyEvent = static_cast<QKeyEvent*>(event);
            const Qt::KeyboardModifiers mods = keyEvent->modifiers() & ~Qt::KeypadModifier;
            if (mods != Qt::NoModifier) {
                return false;
            }

            int forwardSign = 0;
            int rightSign = 0;
            switch (keyEvent->key()) {
                case Qt::Key_W:
                case Qt::Key_Up:
                    forwardSign = 1;
                    break;
                case Qt::Key_S:
                case Qt::Key_Down:
                    forwardSign = -1;
                    break;
                case Qt::Key_A:
                case Qt::Key_Left:
                    rightSign = -1;
                    break;
                case Qt::Key_D:
                case Qt::Key_Right:
                    rightSign = 1;
                    break;
                default:
                    return false;
            }

            if (_isTextOrProtectedInputFocused()) {
                return false;
            }

            _enqueueNudge(forwardSign, rightSign, keyEvent->isAutoRepeat());
            return true;
        }
        default:
            break;
    }

    return QObject::eventFilter(watched, event);
}

bool KeyboardNudgeController::_isTextOrProtectedInputFocused() const
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

double KeyboardNudgeController::_stepMeters() const
{
    FlyViewSettings* const settings = SettingsManager::instance()->flyViewSettings();
    const double raw = settings ? settings->keyboardNudgeStepMeters()->rawValue().toDouble() : 5.0;
    if (!qIsFinite(raw)) {
        return 5.0;
    }
    return qBound(kMinStepMeters, raw, kMaxStepMeters);
}

int KeyboardNudgeController::_minIntervalMs() const
{
    FlyViewSettings* const settings = SettingsManager::instance()->flyViewSettings();
    const int raw = settings ? settings->keyboardNudgeIntervalMs()->rawValue().toInt() : 500;
    return qMax(kMinIntervalFloorMs, raw);
}

void KeyboardNudgeController::_resetPending()
{
    _flushTimer.stop();
    if (!qFuzzyIsNull(_pendingForwardM) || !qFuzzyIsNull(_pendingRightM)) {
        _pendingForwardM = 0.0;
        _pendingRightM = 0.0;
        emit pendingOffsetChanged();
    }
}

void KeyboardNudgeController::_enqueueNudge(int forwardSign, int rightSign, bool isAutoRepeat)
{
    if (forwardSign == 0 && rightSign == 0) {
        return;
    }

    const int minIntervalMs = _minIntervalMs();
    if (isAutoRepeat && _repeatGate.isValid() && _repeatGate.elapsed() < minIntervalMs) {
        return;
    }
    _repeatGate.restart();

    const double step = _stepMeters();
    _pendingForwardM += static_cast<double>(forwardSign) * step;
    _pendingRightM += static_cast<double>(rightSign) * step;
    emit pendingOffsetChanged();

    int waitMs = kMergeDebounceMs;
    if (_sendCooldown.isValid()) {
        const qint64 remaining = static_cast<qint64>(minIntervalMs) - _sendCooldown.elapsed();
        if (remaining > waitMs) {
            waitMs = static_cast<int>(remaining);
        }
    }
    _flushTimer.start(waitMs);
}

void KeyboardNudgeController::_flushNudge()
{
    const double forwardM = _pendingForwardM;
    const double rightM = _pendingRightM;
    _pendingForwardM = 0.0;
    _pendingRightM = 0.0;
    emit pendingOffsetChanged();

    const double distanceM = std::hypot(forwardM, rightM);
    if (distanceM < 0.1) {
        emit nudgeFeedback(tr("移动偏移已抵消"), QGeoCoordinate());
        return;
    }

    MultiVehicleManager* const manager = MultiVehicleManager::instance();
    Vehicle* const activeVehicle = manager ? manager->activeVehicle() : nullptr;
    if (!activeVehicle || !activeVehicle->coordinate().isValid()) {
        _lastSentCoord = QGeoCoordinate();
        emit nudgeFeedback(tr("未连接有效载具"), QGeoCoordinate());
        return;
    }

    QGeoCoordinate baseCoord = activeVehicle->coordinate();
    if (_lastSentCoord.isValid() && _sendCooldown.isValid() && _sendCooldown.elapsed() < kChainWindowMs) {
        baseCoord = _lastSentCoord;
    }

    double baseHeadingDeg = 0.0;
    if (_headingRelative) {
        const double hdg = activeVehicle->heading()->rawValue().toDouble();
        if (!qIsNaN(hdg) && qIsFinite(hdg)) {
            baseHeadingDeg = hdg;
        }
    }

    const double relAzimuthDeg = qRadiansToDegrees(std::atan2(rightM, forwardM));
    double azimuthDeg = std::fmod(baseHeadingDeg + relAzimuthDeg, 360.0);
    if (azimuthDeg < 0.0) {
        azimuthDeg += 360.0;
    }

    QGeoCoordinate target = baseCoord.atDistanceAndAzimuth(distanceM, azimuthDeg);
    target.setAltitude(0.0);  // param7=0 由飞控端保持当前高度

    if (activeVehicle->guidedModeGotoLocation(target, 0.0)) {
        _lastSentCoord = target;
        _sendCooldown.restart();
        const QString frameLabel = _headingRelative ? tr("机头向") : tr("正北向");
        const QString msg = tr("微调 #%1：偏 %2° 走 %3 m (%4)")
                                .arg(activeVehicle->id())
                                .arg(qRound(relAzimuthDeg))
                                .arg(distanceM, 0, 'f', 1)
                                .arg(frameLabel);
        qCInfo(KeyboardNudgeControllerLog) << msg << target;
        emit nudgeFeedback(msg, target);
    }
}
