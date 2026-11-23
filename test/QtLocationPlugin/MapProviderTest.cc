#include "MapProviderTest.h"

#include <QtCore/QBuffer>
#include <QtCore/QtMath>
#include <QtGui/QImage>

#include "MapProvider.h"
#include "QGCMapUrlEngine.h"
#include "QGCTileSet.h"
#include "QGeoMapReplyQGC.h"

class TestableMapProvider : public MapProvider
{
public:
    explicit TestableMapProvider(const QString& name = QStringLiteral("TestProvider"),
                                 const QString& imageFormat = QStringLiteral("png"),
                                 quint32 avgSize = QGC_AVERAGE_TILE_SIZE)
        : MapProvider(name, QString(), imageFormat, avgSize, MapProvider::CustomMap)
    {
    }

    using MapProvider::_getServerNum;
    using MapProvider::_tileXYToQuadKey;

private:
    QString _getURL(int, int, int) const override
    {
        return {};
    }
};

// --- Image format detection ---

void MapProviderTest::_testGetImageFormatPng()
{
    TestableMapProvider p;
    const QByteArray png(
        "\x89\x50\x4E\x47\x0D\x0A\x1A\x0A"
        "rest_of_data",
        20);
    QCOMPARE(p.getImageFormat(png), QStringLiteral("png"));
}

void MapProviderTest::_testGetImageFormatJpeg()
{
    TestableMapProvider p;
    const QByteArray jpeg(
        "\xFF\xD8\xFF\xE0"
        "jpeg_data",
        13);
    QCOMPARE(p.getImageFormat(jpeg), QStringLiteral("jpg"));
}

void MapProviderTest::_testGetImageFormatGif()
{
    TestableMapProvider p;
    const QByteArray gif("GIF89a_data", 11);
    QCOMPARE(p.getImageFormat(gif), QStringLiteral("gif"));
}

void MapProviderTest::_testGetImageFormatFallback()
{
    TestableMapProvider p(QStringLiteral("Test"), QStringLiteral("webp"));
    const QByteArray unknown("RIFF____WEBP", 12);
    QCOMPARE(p.getImageFormat(unknown), QStringLiteral("webp"));
}

void MapProviderTest::_testGetImageFormatTooSmall()
{
    TestableMapProvider p;
    QVERIFY(p.getImageFormat(QByteArray("AB")).isEmpty());
    QVERIFY(p.getImageFormat(QByteArray()).isEmpty());
}

// --- Web Mercator coordinate math ---
// Reference: https://wiki.openstreetmap.org/wiki/Slippy_map_tilenames

void MapProviderTest::_testLong2tileXZoom0()
{
    TestableMapProvider p;
    QCOMPARE(p.long2tileX(0.0, 0), 0);
    QCOMPARE(p.long2tileX(-180.0, 0), 0);
}

void MapProviderTest::_testLong2tileXZoom1()
{
    TestableMapProvider p;
    // lon=0 at z=1: (0+180)/360 * 2 = 1.0 → floor = 1
    QCOMPARE(p.long2tileX(0.0, 1), 1);
    // lon=-180 at z=1: (−180+180)/360 * 2 = 0.0 → floor = 0
    QCOMPARE(p.long2tileX(-180.0, 1), 0);
}

void MapProviderTest::_testLong2tileXNegative()
{
    TestableMapProvider p;
    // NYC longitude: floor((-73.9857+180)/360 * 1024) = floor(301.48...) = 301
    QCOMPARE(p.long2tileX(-73.9857, 10), 301);
}

void MapProviderTest::_testLong2tileXBoundary180()
{
    TestableMapProvider p;
    // lon just below 180 at z=10: should be 2^10 - 1 = 1023
    QCOMPARE(p.long2tileX(179.999, 10), 1023);
}

void MapProviderTest::_testLat2tileYEquator()
{
    TestableMapProvider p;
    // lat=0 at z=1: (1 - log(tan(0) + 1/cos(0))/π) / 2 * 2 = (1 - 0)/2 * 2 = 1
    QCOMPARE(p.lat2tileY(0.0, 1), 1);
}

void MapProviderTest::_testLat2tileYNorthern()
{
    TestableMapProvider p;
    // NYC latitude 40.7128 at z=10
    QCOMPARE(p.lat2tileY(40.7128, 10), 385);
}

void MapProviderTest::_testLat2tileYPoles()
{
    TestableMapProvider p;
    // Near north pole at z=1 → tile 0
    QCOMPARE(p.lat2tileY(85.05, 1), 0);
    // Near south pole at z=1 → tile 1
    QCOMPARE(p.lat2tileY(-85.05, 1), 1);
}

// --- Tile count ---

void MapProviderTest::_testGetTileCountSingleTile()
{
    TestableMapProvider p;
    // At z=1, tile (1,1) covers lon=[0,180), lat≈[-85.05,0).
    // Use a bbox entirely within that tile.
    const auto set = p.getTileCount(1, 10.0, -10.0, 20.0, -20.0);
    QCOMPARE(set.tileCount, static_cast<quint64>(1));
}

void MapProviderTest::_testGetTileCountMultipleTiles()
{
    TestableMapProvider p;
    // z=1: full world = 2x2 = 4 tiles
    const auto set = p.getTileCount(1, -179.9, 85.0, 179.9, -85.0);
    QCOMPARE(set.tileCount, static_cast<quint64>(4));
}

void MapProviderTest::_testGetTileCountSizeMatchesAvg()
{
    TestableMapProvider p(QStringLiteral("Test"), QStringLiteral("png"), 5000);
    const auto set = p.getTileCount(1, -179.9, 85.0, 179.9, -85.0);
    QCOMPARE(set.tileSize, set.tileCount * static_cast<quint64>(5000));
}

// --- QuadKey encoding ---

void MapProviderTest::_testQuadKeyLevel1()
{
    TestableMapProvider p;
    QCOMPARE(p._tileXYToQuadKey(0, 0, 1), QStringLiteral("0"));
    QCOMPARE(p._tileXYToQuadKey(1, 0, 1), QStringLiteral("1"));
    QCOMPARE(p._tileXYToQuadKey(0, 1, 1), QStringLiteral("2"));
    QCOMPARE(p._tileXYToQuadKey(1, 1, 1), QStringLiteral("3"));
}

void MapProviderTest::_testQuadKeyLevel3()
{
    TestableMapProvider p;
    // (3,5,3): level 3, mask=4,2,1
    // i=3: mask=4; tileX=3 (011), 3&4=0; tileY=5 (101), 5&4=4≠0 → digit='0'+2='2'
    // i=2: mask=2; 3&2=2≠0 → +1; 5&2=0 → digit='1'
    // i=1: mask=1; 3&1=1≠0 → +1; 5&1=1≠0 → +2; digit='3'
    QCOMPARE(p._tileXYToQuadKey(3, 5, 3), QStringLiteral("213"));
}

void MapProviderTest::_testQuadKeyLevel0()
{
    TestableMapProvider p;
    QCOMPARE(p._tileXYToQuadKey(0, 0, 0), QString());
}

// --- Server number ---

void MapProviderTest::_testGetServerNumBasic()
{
    TestableMapProvider p;
    // (x + 2*y) % max
    QCOMPARE(p._getServerNum(0, 0, 4), 0);
    QCOMPARE(p._getServerNum(1, 0, 4), 1);
    QCOMPARE(p._getServerNum(0, 1, 4), 2);
    QCOMPARE(p._getServerNum(1, 1, 4), 3);
}

void MapProviderTest::_testGetServerNumWrap()
{
    TestableMapProvider p;
    // (5 + 2*3) % 4 = 11 % 4 = 3
    QCOMPARE(p._getServerNum(5, 3, 4), 3);
    // max=1 always returns 0
    QCOMPARE(p._getServerNum(99, 99, 1), 0);
}

// --- Getters ---

void MapProviderTest::_testGettersReturnConstructorValues()
{
    TestableMapProvider p(QStringLiteral("MyMap"), QStringLiteral("jpg"), 7777);
    QCOMPARE(p.getMapName(), QStringLiteral("MyMap"));
    QCOMPARE(p.getAverageSize(), static_cast<quint32>(7777));
    QCOMPARE(p.getMapStyle(), MapProvider::CustomMap);
    QVERIFY(!p.isElevationProvider());
    QVERIFY(!p.isBingProvider());
    QVERIFY(p.getMapId() > 0);
}

// --- Overzoom & Native Zoom Limits ---

namespace {
QByteArray makeQuadrantPng(int size, QImage::Format fmt = QImage::Format_RGBA8888)
{
    QImage img(size, size, fmt);
    const int half = size / 2;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            if (x < half && y < half) {
                img.setPixelColor(x, y, QColor(255, 0, 0, 255));
            } else if (x >= half && y < half) {
                img.setPixelColor(x, y, QColor(0, 255, 0, 255));
            } else if (x < half && y >= half) {
                img.setPixelColor(x, y, QColor(0, 0, 255, 255));
            } else {
                img.setPixelColor(x, y, QColor(255, 255, 0, 255));
            }
        }
    }
    QByteArray out;
    QBuffer buf(&out);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "PNG");
    return out;
}
} // namespace

void MapProviderTest::_testMaxNativeZoomAndAmapScale()
{
    const auto amapRoad = UrlFactory::getMapProviderFromProviderType(QStringLiteral("Amap Road"));
    const auto amapSat = UrlFactory::getMapProviderFromProviderType(QStringLiteral("Amap Satellite"));
    const auto tdtRoad = UrlFactory::getMapProviderFromProviderType(QStringLiteral("TianDiTu Road"));
    const auto tdtSat = UrlFactory::getMapProviderFromProviderType(QStringLiteral("TianDiTu Satellite"));
    const auto tdtTer = UrlFactory::getMapProviderFromProviderType(QStringLiteral("TianDiTu Terrain"));
    const auto bingRoad = UrlFactory::getMapProviderFromProviderType(QStringLiteral("Bing Road"));

    QVERIFY(amapRoad != nullptr);
    QVERIFY(amapSat != nullptr);
    QVERIFY(tdtRoad != nullptr);
    QVERIFY(tdtSat != nullptr);
    QVERIFY(tdtTer != nullptr);
    QVERIFY(bingRoad != nullptr);

    QCOMPARE(amapRoad->maxNativeZoom(), 18);
    QCOMPARE(amapSat->maxNativeZoom(), 18);
    QCOMPARE(tdtRoad->maxNativeZoom(), 18);
    QCOMPARE(tdtSat->maxNativeZoom(), 18);
    QCOMPARE(tdtTer->maxNativeZoom(), 18);
    QCOMPARE(bingRoad->maxNativeZoom(), QGC_MAX_MAP_ZOOM);

    const QUrl amapUrl = amapRoad->getTileURL(100, 200, 18);
    QVERIFY(amapUrl.toString().contains(QStringLiteral("scale=2")));
}

void MapProviderTest::_testOverzoomCrop256Quadrants()
{
    const QByteArray parentPng = makeQuadrantPng(256);
    QVERIFY(!parentPng.isEmpty());

    const int px = 100;
    const int py = 200;
    struct QuadrantExpectation {
        int dx;
        int dy;
        QColor color;
    };
    const QuadrantExpectation expectations[] = {
        {0, 0, QColor(255, 0, 0)},
        {1, 0, QColor(0, 255, 0)},
        {0, 1, QColor(0, 0, 255)},
        {1, 1, QColor(255, 255, 0)},
    };

    for (const auto& exp : expectations) {
        QString fmt;
        const QByteArray tileData = QGeoTiledMapReplyQGC::cropOverzoomTile(
            parentPng, (px << 1) + exp.dx, (py << 1) + exp.dy, 19, 18, &fmt);
        QCOMPARE(fmt, QStringLiteral("png"));
        const QImage out = QImage::fromData(tileData);
        QCOMPARE(out.size(), QSize(256, 256));
        QCOMPARE(out.pixelColor(128, 128), exp.color);
    }

    // dz = 3 (z = 21 from z = 18): bottom-right corner tile maps to yellow quadrant
    QString fmt21;
    const QByteArray corner21 = QGeoTiledMapReplyQGC::cropOverzoomTile(
        parentPng, (px << 3) + 7, (py << 3) + 7, 21, 18, &fmt21);
    QCOMPARE(fmt21, QStringLiteral("png"));
    const QImage out21 = QImage::fromData(corner21);
    QCOMPARE(out21.size(), QSize(256, 256));
    QCOMPARE(out21.pixelColor(128, 128), QColor(255, 255, 0));
}

void MapProviderTest::_testOverzoomCrop512HiDpi()
{
    const QByteArray parent512 = makeQuadrantPng(512);
    QVERIFY(!parent512.isEmpty());

    const int px = 50;
    const int py = 60;
    QString fmt;
    const QByteArray topRight = QGeoTiledMapReplyQGC::cropOverzoomTile(
        parent512, (px << 1) + 1, (py << 1) + 0, 19, 18, &fmt);
    QCOMPARE(fmt, QStringLiteral("png"));
    const QImage out = QImage::fromData(topRight);
    QCOMPARE(out.size(), QSize(256, 256));
    QCOMPARE(out.pixelColor(10, 10), QColor(0, 255, 0));
    QCOMPARE(out.pixelColor(245, 245), QColor(0, 255, 0));
}

void MapProviderTest::_testOverzoomPreservesAlphaAndInvalidInputs()
{
    QImage alphaImg(256, 256, QImage::Format_ARGB32);
    alphaImg.fill(Qt::transparent);
    for (int y = 0; y < 128; ++y) {
        for (int x = 0; x < 128; ++x) {
            alphaImg.setPixelColor(x, y, QColor(200, 50, 50, 255));
        }
    }
    QByteArray alphaPng;
    QBuffer buf(&alphaPng);
    buf.open(QIODevice::WriteOnly);
    QVERIFY(alphaImg.save(&buf, "PNG"));

    const QByteArray bottomRight = QGeoTiledMapReplyQGC::cropOverzoomTile(alphaPng, 1, 1, 19, 18);
    const QImage outBr = QImage::fromData(bottomRight);
    QCOMPARE(outBr.size(), QSize(256, 256));
    QVERIFY(outBr.hasAlphaChannel());
    QCOMPARE(outBr.pixelColor(128, 128).alpha(), 0);

    const QByteArray topLeft = QGeoTiledMapReplyQGC::cropOverzoomTile(alphaPng, 0, 0, 19, 18);
    const QImage outTl = QImage::fromData(topLeft);
    QCOMPARE(outTl.pixelColor(64, 64), QColor(200, 50, 50, 255));

    // Invalid inputs
    QVERIFY(QGeoTiledMapReplyQGC::cropOverzoomTile(QByteArray(), 0, 0, 19, 18).isEmpty());
    QVERIFY(QGeoTiledMapReplyQGC::cropOverzoomTile(QByteArray("not_an_image"), 0, 0, 19, 18).isEmpty());
    QVERIFY(QGeoTiledMapReplyQGC::cropOverzoomTile(alphaPng, 0, 0, 18, 18).isEmpty());
    QVERIFY(QGeoTiledMapReplyQGC::cropOverzoomTile(alphaPng, 0, 0, 17, 18).isEmpty());
    QVERIFY(QGeoTiledMapReplyQGC::cropOverzoomTile(alphaPng, 0, 0, 35, 18).isEmpty());
}

UT_REGISTER_TEST(MapProviderTest, TestLabel::Unit)
