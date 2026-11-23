#include "SpaceArmDisarmUITest.h"

#include <QtCore/QPointer>
#include <QtCore/QRegularExpression>
#include <QtCore/QScopeGuard>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <QtTest/QTest>

#include "LinkManager.h"
#include "MockConfiguration.h"
#include "MockLink.h"
#include "MultiVehicleManager.h"
#include "QmlObjectListModel.h"
#include "Vehicle.h"

UT_REGISTER_TEST(SpaceArmDisarmUITest, TestLabel::Integration)

void SpaceArmDisarmUITest::_testSingleVehicleSpaceEmergencyDisarmAndHoldArm()
{
    runWithMockLink(
        [] { return MockLink::startPX4MockLink(); },
        [&](QPointer<MockLink> mockLink, Vehicle* vehicle) {
            QVERIFY(mockLink);
            QVERIFY(vehicle);
            QVERIFY2(findVisibleItem(_rootItem, QStringLiteral("mainView_fly"), 3000), "Fly view not visible");

            QQuickItem* overlay = findItem(_rootItem, QStringLiteral("spaceArmDisarmOverlay"));
            QVERIFY2(overlay, "spaceArmDisarmOverlay not found");
            QObject* controller = overlay->findChild<QObject*>(QStringLiteral("spaceArmDisarmController"));
            QVERIFY2(controller, "spaceArmDisarmController not found");

            // 1. 已解锁状态下：短按空格不应上锁，长按 0.8s 才触发强制紧急上锁，且不反转进入解锁
            vehicle->setArmed(true, false);
            QVERIFY_TRUE_WAIT(vehicle->armed(), 3000);

            QTest::keyClick(_window, Qt::Key_Space);
            QVERIFY(!controller->property("holding").toBool());
            QVERIFY(vehicle->armed());

            const int beforeCmdCount = mockLink->receivedMavCommandCount(MAV_CMD_COMPONENT_ARM_DISARM);
            QTest::keyPress(_window, Qt::Key_Space);
            QVERIFY_TRUE_WAIT(
                controller->property("holding").toBool() && controller->property("holdingEmergencyDisarm").toBool(),
                1000);
            QVERIFY2(findVisibleItem(_rootItem, QStringLiteral("spaceArmHoldIndicator"), 1000),
                     "Hold progress indicator not visible for emergency disarm");
            QVERIFY_TRUE_WAIT(!vehicle->armed(), 3000);
            QVERIFY(mockLink->receivedMavCommandCount(MAV_CMD_COMPONENT_ARM_DISARM) > beforeCmdCount);
            QVERIFY(!controller->property("holding").toBool());
            QVERIFY2(findVisibleItem(_rootItem, QStringLiteral("spaceArmFeedbackToast"), 2000),
                     "Feedback toast not shown after emergency disarm");
            QTest::keyRelease(_window, Qt::Key_Space);

            // 2. 短按空格不触发解锁
            QTest::keyClick(_window, Qt::Key_Space);
            QVERIFY(!controller->property("holding").toBool());
            QVERIFY(!vehicle->armed());

            // 2b. 确认弹窗显示时，全局空格拦截禁用，不触发长按解锁
            QVERIFY(QMetaObject::invokeMethod(_window, "armVehicleRequest"));
            QVERIFY_TRUE_WAIT(!overlay->property("shortcutEnabled").toBool(), 2000);
            QTest::keyPress(_window, Qt::Key_Space);
            QVERIFY(!controller->property("holding").toBool());
            QTest::keyRelease(_window, Qt::Key_Space);
            QQuickItem* flyView = findItem(_rootItem, QStringLiteral("mainView_fly"));
            QVERIFY(flyView);
            QObject* guidedController = flyView->property("guidedController").value<QObject*>();
            QVERIFY(guidedController);
            QObject* confirmDialog = guidedController->property("confirmDialog").value<QObject*>();
            QVERIFY(confirmDialog);
            QVERIFY(QMetaObject::invokeMethod(confirmDialog, "confirmCancelled", Q_ARG(QVariant, QVariant())));
            QVERIFY_TRUE_WAIT(overlay->property("shortcutEnabled").toBool(), 2000);

            // 3. 长按 0.8s 空格触发单机解锁
            QTest::keyPress(_window, Qt::Key_Space);
            QVERIFY_TRUE_WAIT(
                controller->property("holding").toBool() && !controller->property("holdingEmergencyDisarm").toBool(),
                1000);
            QVERIFY2(findVisibleItem(_rootItem, QStringLiteral("spaceArmHoldIndicator"), 1000),
                     "Hold progress indicator not visible");
            QVERIFY_TRUE_WAIT(vehicle->armed(), 3000);
            QVERIFY(!controller->property("holding").toBool());
            QTest::keyRelease(_window, Qt::Key_Space);
        });
}

void SpaceArmDisarmUITest::_testMultiVehicleShiftSpaceCoordination()
{
    ignoreLogMessage("API.QGCApplication.AppMessage", QtDebugMsg,
                     QRegularExpression(QStringLiteral("Connected to \\d+ vehicles")));

    startUI();
    if (QTest::currentTestFailed()) {
        return;
    }

    QVERIFY2(findVisibleItem(_rootItem, QStringLiteral("mainView_fly"), 3000), "Fly view not visible");

    QQuickItem* overlay = findItem(_rootItem, QStringLiteral("spaceArmDisarmOverlay"));
    QVERIFY2(overlay, "spaceArmDisarmOverlay not found");
    QObject* controller = overlay->findChild<QObject*>(QStringLiteral("spaceArmDisarmController"));
    QVERIFY2(controller, "spaceArmDisarmController not found");

    auto startNumberedPX4MockLink = [](int index) -> QPointer<MockLink> {
        auto* config = new MockConfiguration(QStringLiteral("SpaceMultiMock %1").arg(index));
        config->setDynamic(true);
        config->setIncrementVehicleId(true);
        SharedLinkConfigurationPtr sharedConfig(config);
        if (!LinkManager::instance()->createConnectedLink(sharedConfig)) {
            return {};
        }
        return qobject_cast<MockLink*>(config->link());
    };

    QPointer<MockLink> mockLink1 = startNumberedPX4MockLink(1);
    QVERIFY2(mockLink1, "Failed to start first MockLink");
    QPointer<MockLink> mockLink2 = startNumberedPX4MockLink(2);
    QVERIFY2(mockLink2, "Failed to start second MockLink");

    auto cleanupLinks = qScopeGuard([&] {
        if (mockLink1) {
            mockLink1->disconnect();
        }
        if (mockLink2) {
            mockLink2->disconnect();
        }
        (void) QTest::qWaitFor(
            [] {
                return MultiVehicleManager::instance()->vehicles()->count() == 0 &&
                       MultiVehicleManager::instance()->activeVehicle() == nullptr;
            },
            10000);
        closeUIWindow();
        destroyUIEngine();
    });

    MultiVehicleManager* manager = MultiVehicleManager::instance();
    QVERIFY_TRUE_WAIT(manager->vehicles()->count() == 2, 10000);

    Vehicle* v1 = qobject_cast<Vehicle*>(manager->vehicles()->get(0));
    Vehicle* v2 = qobject_cast<Vehicle*>(manager->vehicles()->get(1));
    QVERIFY(v1);
    QVERIFY(v2);
    QVERIFY(v1->id() != v2->id());
    QVERIFY_TRUE_WAIT(v1->isInitialConnectComplete() && v2->isInitialConnectComplete(), 10000);

    // 1. 未选中任何载具时 Shift+空格：只提示，不操作 activeVehicle
    manager->deselectAllVehicles();
    QTest::keyPress(_window, Qt::Key_Space, Qt::ShiftModifier);
    QVERIFY(!controller->property("holding").toBool());
    QVERIFY(!v1->armed());
    QVERIFY(!v2->armed());
    QVERIFY2(findVisibleItem(_rootItem, QStringLiteral("spaceArmFeedbackToast"), 2000),
             "Feedback toast expected when no vehicles are selected");
    QTest::keyRelease(_window, Qt::Key_Space, Qt::ShiftModifier);

    // 2. 选中两台（均未解锁）长按 Shift+空格 0.8s：批量解锁两台
    manager->selectVehicle(v1->id());
    manager->selectVehicle(v2->id());
    QCOMPARE(manager->selectedVehicles()->count(), 2);

    QTest::keyPress(_window, Qt::Key_Space, Qt::ShiftModifier);
    QVERIFY_TRUE_WAIT(controller->property("holding").toBool() && controller->property("multiVehicleHold").toBool() &&
                          !controller->property("holdingEmergencyDisarm").toBool(),
                      1000);
    QVERIFY_TRUE_WAIT(v1->armed() && v2->armed(), 3000);
    QTest::keyRelease(_window, Qt::Key_Space, Qt::ShiftModifier);

    // 3. 混合态协调：一台上锁、一台仍解锁 -> 短按不上锁，长按 Shift+空格 0.8s 优先紧急上锁，不触发解锁
    v1->setArmed(false, false);
    QVERIFY_TRUE_WAIT(!v1->armed(), 3000);
    QVERIFY(v2->armed());

    QTest::keyClick(_window, Qt::Key_Space, Qt::ShiftModifier);
    QVERIFY(!controller->property("holding").toBool());
    QVERIFY(v2->armed());
    QVERIFY(!v1->armed());

    QTest::keyPress(_window, Qt::Key_Space, Qt::ShiftModifier);
    QVERIFY_TRUE_WAIT(controller->property("holding").toBool() && controller->property("multiVehicleHold").toBool() &&
                          controller->property("holdingEmergencyDisarm").toBool(),
                      1000);
    QVERIFY_TRUE_WAIT(!v2->armed(), 3000);
    QVERIFY(!v1->armed());
    QVERIFY(!controller->property("holding").toBool());
    QTest::keyRelease(_window, Qt::Key_Space, Qt::ShiftModifier);

    // 4. 两台均已解锁 -> 长按 Shift+空格 0.8s 全部紧急上锁
    v1->setArmed(true, false);
    v2->setArmed(true, false);
    QVERIFY_TRUE_WAIT(v1->armed() && v2->armed(), 3000);

    QTest::keyPress(_window, Qt::Key_Space, Qt::ShiftModifier);
    QVERIFY_TRUE_WAIT(controller->property("holding").toBool() && controller->property("multiVehicleHold").toBool() &&
                          controller->property("holdingEmergencyDisarm").toBool(),
                      1000);
    QVERIFY_TRUE_WAIT(!v1->armed() && !v2->armed(), 3000);
    QVERIFY(!controller->property("holding").toBool());
    QTest::keyRelease(_window, Qt::Key_Space, Qt::ShiftModifier);
}
