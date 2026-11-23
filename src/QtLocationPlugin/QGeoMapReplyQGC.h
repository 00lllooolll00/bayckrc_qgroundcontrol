#pragma once

#include <QtLocation/private/qgeotiledmapreply_p.h>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include "QGCMapTaskBase.h"

struct QGCCacheTile;
class QNetworkAccessManager;
class QSslError;

class QGeoTiledMapReplyQGC : public QGeoTiledMapReply
{
    Q_OBJECT

public:
    explicit QGeoTiledMapReplyQGC(QNetworkAccessManager *networkManager,
                                  const QNetworkRequest &request,
                                  const QGeoTileSpec &spec,
                                  QObject *parent = nullptr);
    explicit QGeoTiledMapReplyQGC(QNetworkAccessManager *networkManager,
                                  const QNetworkRequest &request,
                                  const QGeoTileSpec &spec,
                                  int fetchX,
                                  int fetchY,
                                  int fetchZoom,
                                  QObject *parent = nullptr);
    ~QGeoTiledMapReplyQGC();

    bool init();
    void abort() final;

    static QByteArray cropOverzoomTile(const QByteArray &parentImageData,
                                       int x, int y, int zoom,
                                       int parentZoom,
                                       QString *outFormat = nullptr,
                                       int outputTileSize = 256);

private slots:
    void _networkReplyFinished();
    void _networkReplyError(QNetworkReply::NetworkError error);
    void _networkReplySslErrors(const QList<QSslError> &errors);
    void _cacheReply(QGCCacheTile *tile);
    void _cacheError(QGCMapTask::TaskType type, QStringView errorString);

private:
    static void _initDataFromResources();

    QNetworkAccessManager *_networkManager = nullptr;
    QNetworkRequest _request;
    int _fetchX = 0;
    int _fetchY = 0;
    int _fetchZoom = 0;
    bool _isOverzoom = false;
    bool m_initialized = false;

    static QByteArray _bingNoTileImage;
    static QByteArray _badTile;
};
