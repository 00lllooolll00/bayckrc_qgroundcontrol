#pragma once

#include <QtCore/QElapsedTimer>
#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtCore/QTimer>
#include <QtQmlIntegration/QtQmlIntegration>
#include <QtQuick/QQuickWindow>

class Vehicle;

/// @brief Handles Space / Shift+Space keyboard shortcuts for 0.8s hold-to-emergency-disarm
///        and 0.8s hold-to-arm across single and multi-vehicle selections.
class SpaceArmDisarmController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QQuickWindow* window READ window WRITE setWindow NOTIFY windowChanged)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(int holdDurationMs READ holdDurationMs WRITE setHoldDurationMs NOTIFY holdDurationMsChanged)
    Q_PROPERTY(bool holding READ holding NOTIFY holdingChanged)
    Q_PROPERTY(qreal holdProgress READ holdProgress NOTIFY holdProgressChanged)
    Q_PROPERTY(QString holdPromptText READ holdPromptText NOTIFY holdPromptTextChanged)
    Q_PROPERTY(bool multiVehicleHold READ multiVehicleHold NOTIFY multiVehicleHoldChanged)
    Q_PROPERTY(bool holdingEmergencyDisarm READ holdingEmergencyDisarm NOTIFY holdingEmergencyDisarmChanged)

public:
    explicit SpaceArmDisarmController(QObject* parent = nullptr);
    ~SpaceArmDisarmController() override;

    QQuickWindow* window() const { return _window.data(); }

    void setWindow(QQuickWindow* window);

    bool enabled() const { return _enabled; }

    void setEnabled(bool enabled);

    int holdDurationMs() const { return _holdDurationMs; }

    void setHoldDurationMs(int holdDurationMs);

    bool holding() const { return _holding; }

    qreal holdProgress() const { return _holdProgress; }

    QString holdPromptText() const { return _holdPromptText; }

    bool multiVehicleHold() const { return _multiVehicleHold; }

    bool holdingEmergencyDisarm() const { return _holdingEmergencyDisarm; }

    bool eventFilter(QObject* watched, QEvent* event) override;

signals:
    void windowChanged();
    void enabledChanged();
    void holdDurationMsChanged();
    void holdingChanged();
    void holdProgressChanged();
    void holdPromptTextChanged();
    void multiVehicleHoldChanged();
    void holdingEmergencyDisarmChanged();
    void actionFeedback(const QString& message, bool isEmergency);

private slots:
    void _onHoldTick();

private:
    bool _isTextOrProtectedInputFocused() const;
    QList<Vehicle*> _collectSelectedVehicles() const;
    static QString _formatVehicleIds(const QList<Vehicle*>& vehicles);

    bool _handleSpacePressed(bool isMulti);
    void _startHold(bool isMulti, bool isEmergencyDisarm, const QString& promptText);
    void _cancelHold();
    void _setHoldState(bool holding, bool multiVehicleHold, bool holdingEmergencyDisarm, qreal progress,
                       const QString& promptText);

    QPointer<QQuickWindow> _window;
    bool _enabled = true;
    int _holdDurationMs = 800;
    bool _holding = false;
    bool _multiVehicleHold = false;
    bool _holdingEmergencyDisarm = false;
    bool _latched = false;
    qreal _holdProgress = 0.0;
    QString _holdPromptText;
    QTimer _holdTickTimer;
    QElapsedTimer _holdElapsed;
};
