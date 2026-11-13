#include "FormationModelTest.h"

#include <QtTest/QSignalSpy>

#include "FormationModel.h"
#include "LinkManager.h"
#include "MockConfiguration.h"
#include "MockLink.h"
#include "MultiVehicleManager.h"
#include "QmlObjectListModel.h"
#include "Vehicle.h"

void FormationModelTest::cleanup()
{
    dumpFailureContextIfTestFailed(QStringLiteral("before FormationModelTest teardown"));
    LinkManager::instance()->disconnectAll();
    QTRY_COMPARE_WITH_TIMEOUT(MultiVehicleManager::instance()->vehicles()->count(), 0, TestTimeout::mediumMs());
    UnitTest::settleEventLoopForCleanup();
    VehicleTestManualConnect::cleanup();
}

bool FormationModelTest::_startSmartApVehicle()
{
    auto* mockConfig = new MockConfiguration(QStringLiteral("SmartAP MockLink"));
    mockConfig->setFirmwareType(MAV_AUTOPILOT_SMARTAP);
    mockConfig->setVehicleType(MAV_TYPE_QUADROTOR);
    mockConfig->setIncrementVehicleId(true);
    mockConfig->setDynamic(true);

    SharedLinkConfigurationPtr config = LinkManager::instance()->addConfiguration(mockConfig);
    return LinkManager::instance()->createConnectedLink(config);
}

void FormationModelTest::_positionUpdateEmitsConfigChanged()
{
    MultiVehicleManager* manager = MultiVehicleManager::instance();
    QVERIFY(_startSmartApVehicle());
    QVERIFY(_startSmartApVehicle());
    QVERIFY(_startSmartApVehicle());
    QTRY_COMPARE_WITH_TIMEOUT(manager->vehicles()->count(), 3, TestTimeout::mediumMs());

    FormationModel model;
    model.matchOnline(manager->vehicles());
    QCOMPARE(model.rowCount(), 3);

    const int id1 = model.data(model.index(0, 0), FormationModel::SysidRole).toInt();
    const int id2 = model.data(model.index(1, 0), FormationModel::SysidRole).toInt();
    const int id3 = model.data(model.index(2, 0), FormationModel::SysidRole).toInt();

    // 初始已全部在线（wasOnline == true）：仅报出 0 和 1，第 3 架还在 -1（未分配）。
    model.updateStatus(id1, 0, 2, id1, 0x0C, 1);
    model.updateStatus(id2, 1, 2, id1, 0x08, 1);

    // 当第 3 架后续拿到槽位 3（模拟机上分配 0/1/3，4 槽布局）时，必须发 configChanged()
    // 通知绑定了 positions 的编队小地图重算，不能因为已在线就静默吞掉。
    QSignalSpy configSpy(&model, &FormationModel::configChanged);
    model.updateStatus(id3, 3, 2, id1, 0x08, 1);

    QVERIFY(configSpy.count() >= 1);
    QCOMPARE(model.positions().count(), 4);
}

void FormationModelTest::_unassignedFollowerKeepsSlotCount()
{
    MultiVehicleManager* manager = MultiVehicleManager::instance();
    QVERIFY(_startSmartApVehicle());
    QVERIFY(_startSmartApVehicle());
    QVERIFY(_startSmartApVehicle());
    QTRY_COMPARE_WITH_TIMEOUT(manager->vehicles()->count(), 3, TestTimeout::mediumMs());

    FormationModel model;
    model.matchOnline(manager->vehicles());
    const int id1 = model.data(model.index(0, 0), FormationModel::SysidRole).toInt();
    const int id2 = model.data(model.index(1, 0), FormationModel::SysidRole).toInt();
    const int id3 = model.data(model.index(2, 0), FormationModel::SysidRole).toInt();

    // 第 3 架在线但暂未分配槽位（position == -1），小地图仍必须绘出全部 3 个槽位，不能少画一个从机槽。
    model.updateStatus(id1, 0, 2, id1, 0x0C, 1);
    model.updateStatus(id2, 1, 2, id1, 0x08, 1);
    model.updateStatus(id3, -1, 1, id1, 0x08, 1);
    QCOMPARE(model.positions().count(), 3);

    // 即使两从机暂时抢在同一个槽位 1，小地图也不能缩成只有 2 个槽。
    model.updateStatus(id3, 1, 2, id1, 0x08, 1);
    QCOMPARE(model.positions().count(), 3);
}

void FormationModelTest::_leaderSwitchNotifiesLeaderChanged()
{
    MultiVehicleManager* manager = MultiVehicleManager::instance();
    QVERIFY(_startSmartApVehicle());
    QVERIFY(_startSmartApVehicle());
    QTRY_COMPARE_WITH_TIMEOUT(manager->vehicles()->count(), 2, TestTimeout::mediumMs());

    FormationModel model;
    model.matchOnline(manager->vehicles());
    const int id1 = model.data(model.index(0, 0), FormationModel::SysidRole).toInt();
    const int id2 = model.data(model.index(1, 0), FormationModel::SysidRole).toInt();

    model.updateStatus(id1, 0, 2, id1, 0x0C, 1);
    model.updateStatus(id2, 1, 2, id1, 0x08, 1);
    QCOMPARE(model.leaderSysid(), id1);

    QSignalSpy leaderSpy(&model, &FormationModel::leaderChanged);
    // 领航机从 id1 切到 id2：必须通知右栏与地图刷新领航机标识。
    model.updateStatus(id1, 1, 2, id2, 0x08, 1);
    model.updateStatus(id2, 0, 2, id2, 0x0C, 1);

    QCOMPARE(model.leaderSysid(), id2);
    QVERIFY(leaderSpy.count() >= 1);
}

UT_REGISTER_TEST(FormationModelTest, TestLabel::Integration, TestLabel::Vehicle)
