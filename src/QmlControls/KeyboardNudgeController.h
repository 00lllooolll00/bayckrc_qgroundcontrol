#pragma once

#include <QtCore/QElapsedTimer>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtCore/QTimer>
#include <QtPositioning/QGeoCoordinate>
#include <QtQmlIntegration/QtQmlIntegration>
#include <QtQuick/QQuickWindow>

class Vehicle;

/// @brief 通过 WSAD / 方向键与鼠标微调按键发送 MAV_CMD_DO_REPOSITION（保持当前高度），
///        带合并防抖与最小发送间隔节流，避免长按挤爆 LoRa 超级帧。
class KeyboardNudgeController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QQuickWindow* window READ window WRITE setWindow NOTIFY windowChanged)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool headingRelative READ headingRelative WRITE setHeadingRelative NOTIFY headingRelativeChanged)
    Q_PROPERTY(double pendingForwardMeters READ pendingForwardMeters NOTIFY pendingOffsetChanged)
    Q_PROPERTY(double pendingRightMeters READ pendingRightMeters NOTIFY pendingOffsetChanged)

public:
    explicit KeyboardNudgeController(QObject* parent = nullptr);
    ~KeyboardNudgeController() override;

    QQuickWindow* window() const { return _window.data(); }
    void setWindow(QQuickWindow* window);

    bool enabled() const { return _enabled; }
    void setEnabled(bool enabled);

    bool headingRelative() const { return _headingRelative; }
    void setHeadingRelative(bool headingRelative);

    double pendingForwardMeters() const { return _pendingForwardM; }
    double pendingRightMeters() const { return _pendingRightM; }

    Q_INVOKABLE void nudge(int forwardSign, int rightSign);

    bool eventFilter(QObject* watched, QEvent* event) override;

signals:
    void windowChanged();
    void enabledChanged();
    void headingRelativeChanged();
    void pendingOffsetChanged();
    void nudgeFeedback(const QString& message, const QGeoCoordinate& targetCoord);

private slots:
    void _flushNudge();

private:
    bool _isTextOrProtectedInputFocused() const;
    void _enqueueNudge(int forwardSign, int rightSign, bool isAutoRepeat);
    void _resetPending();
    double _stepMeters() const;
    int _minIntervalMs() const;

    QPointer<QQuickWindow> _window;
    bool _enabled = true;
    bool _headingRelative = true;
    double _pendingForwardM = 0.0;
    double _pendingRightM = 0.0;
    QGeoCoordinate _lastSentCoord;
    QTimer _flushTimer;
    QElapsedTimer _sendCooldown;
    QElapsedTimer _repeatGate;
};
