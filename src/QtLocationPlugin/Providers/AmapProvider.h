#pragma once

#include <QtCore/QObject>

#include "MapProvider.h"

static constexpr const quint32 AVERAGE_AMAP_STREET_MAP = 1297;
static constexpr const quint32 AVERAGE_AMAP_SAT_MAP    = 19597;

class AmapProvider : public MapProvider
{
protected:
    AmapProvider(const QString &mapName, const QString &imageFormat, quint32 averageSize,
                 MapProvider::MapStyle mapType, const QString &mapUrl)
        : MapProvider(mapName, QStringLiteral("https://www.amap.com/"), imageFormat, averageSize, mapType)
        , _mapUrl(mapUrl) {}

private:
    QString _getURL(int x, int y, int zoom) const final;

    const QString _mapUrl;
};

class AmapRoadProvider : public AmapProvider
{
public:
    AmapRoadProvider()
        : AmapProvider(
            QObject::tr("Amap Road"),
            QStringLiteral("png"),
            AVERAGE_AMAP_STREET_MAP,
            MapProvider::StreetMap,
            QStringLiteral("https://webrd0%1.is.autonavi.com/appmaptile?lang=zh_cn&size=1&scale=1&style=8&x=%2&y=%3&z=%4")) {}
};

class AmapSatelliteProvider : public AmapProvider
{
public:
    AmapSatelliteProvider()
        : AmapProvider(
            QObject::tr("Amap Satellite"),
            QStringLiteral("jpg"),
            AVERAGE_AMAP_SAT_MAP,
            MapProvider::SatelliteMapDay,
            QStringLiteral("https://webst0%1.is.autonavi.com/appmaptile?style=6&x=%2&y=%3&z=%4")) {}
};
