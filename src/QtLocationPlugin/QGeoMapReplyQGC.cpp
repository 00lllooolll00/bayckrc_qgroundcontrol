#include "QGeoMapReplyQGC.h"

#include <QtCore/QBuffer>
#include <QtCore/QFile>
#include <QtGui/QImage>
#include <QtLocation/private/qgeotilespec_p.h>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QSslError>
#include <cmath>

#include "ElevationMapProvider.h"
#include "MapProvider.h"
#include "QGCCacheTile.h"
#include "QGCLoggingCategory.h"
#include "QGCMapEngine.h"
#include "QGCMapTasks.h"
#include "QGCMapUrlEngine.h"
#include "QGCNetworkHelper.h"
#include "QGeoFileTileCacheQGC.h"

QGC_LOGGING_CATEGORY(QGeoTiledMapReplyQGCLog, "QtLocationPlugin.QGeoTiledMapReplyQGC")

QByteArray QGeoTiledMapReplyQGC::_bingNoTileImage;
QByteArray QGeoTiledMapReplyQGC::_badTile;

QGeoTiledMapReplyQGC::QGeoTiledMapReplyQGC(QNetworkAccessManager *networkManager,
                                             const QNetworkRequest &request,
                                             const QGeoTileSpec &spec,
                                             QObject *parent)
    : QGeoTiledMapReplyQGC(networkManager, request, spec, spec.x(), spec.y(), spec.zoom(), parent)
{
}

QGeoTiledMapReplyQGC::QGeoTiledMapReplyQGC(QNetworkAccessManager *networkManager,
                                             const QNetworkRequest &request,
                                             const QGeoTileSpec &spec,
                                             int fetchX,
                                             int fetchY,
                                             int fetchZoom,
                                             QObject *parent)
    : QGeoTiledMapReply(spec, parent)
    , _networkManager(networkManager)
    , _request(request)
    , _fetchX((fetchZoom >= 0) ? fetchX : spec.x())
    , _fetchY((fetchZoom >= 0) ? fetchY : spec.y())
    , _fetchZoom((fetchZoom >= 0) ? fetchZoom : spec.zoom())
    , _isOverzoom(_fetchZoom < spec.zoom())
{
    qCDebug(QGeoTiledMapReplyQGCLog) << this;
}

QGeoTiledMapReplyQGC::~QGeoTiledMapReplyQGC()
{
    qCDebug(QGeoTiledMapReplyQGCLog) << this;
}

bool QGeoTiledMapReplyQGC::init()
{
    if (m_initialized) {
        return true;
    }

    m_initialized = true;

    _initDataFromResources();

    (void) connect(this, &QGeoTiledMapReplyQGC::errorOccurred, this, [this](QGeoTiledMapReply::Error error, const QString &errorString) {
        qCWarning(QGeoTiledMapReplyQGCLog) << error << errorString;
        setMapImageData(_badTile);
        setMapImageFormat(QStringLiteral("png"));
        setCached(false);
    }, Qt::AutoConnection);

    QGCFetchTileTask *task = QGeoFileTileCacheQGC::createFetchTileTask(
        UrlFactory::getProviderTypeFromQtMapId(tileSpec().mapId()), _fetchX, _fetchY, _fetchZoom);
    if (!task) {
        qCWarning(QGeoTiledMapReplyQGCLog) << "Failed to create fetch tile task";
        m_initialized = false;
        return false;
    }
    (void) connect(task, &QGCFetchTileTask::tileFetched, this, &QGeoTiledMapReplyQGC::_cacheReply);
    (void) connect(task, &QGCMapTask::error, this, &QGeoTiledMapReplyQGC::_cacheError);
    if (!getQGCMapEngine()->addTask(task)) {
        task->deleteLater();
        m_initialized = false;
        return false;
    }

    return true;
}

void QGeoTiledMapReplyQGC::_initDataFromResources()
{
    if (_bingNoTileImage.isEmpty()) {
        QFile file(":/res/BingNoTileBytes.dat");
        if (file.open(QFile::ReadOnly)) {
            _bingNoTileImage = file.readAll();
            file.close();
        }
    }

    if (_badTile.isEmpty()) {
        QFile file(":/res/images/notile.png");
        if (file.open(QFile::ReadOnly)) {
            _badTile = file.readAll();
            file.close();
        }
    }
}

void QGeoTiledMapReplyQGC::_networkReplyFinished()
{
    QNetworkReply* const reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        setError(QGeoTiledMapReply::UnknownError, tr("Unexpected Error"));
        return;
    }
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        return;
    }

    if (!reply->isOpen()) {
        setError(QGeoTiledMapReply::ParseError, tr("Empty Reply"));
        return;
    }

    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (!QGCNetworkHelper::isHttpSuccess(statusCode)) {
        setError(QGeoTiledMapReply::CommunicationError, reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString());
        return;
    }

    QByteArray image = reply->readAll();
    if (image.isEmpty()) {
        setError(QGeoTiledMapReply::ParseError, tr("Image is Empty"));
        return;
    }

    const SharedMapProvider mapProvider = UrlFactory::getMapProviderFromQtMapId(tileSpec().mapId());
    if (!mapProvider) {
        setError(QGeoTiledMapReply::UnknownError, tr("Invalid Map Provider"));
        return;
    }

    if (mapProvider->isBingProvider() && (image == _bingNoTileImage)) {
        setError(QGeoTiledMapReply::CommunicationError, tr("Bing Tile Above Zoom Level"));
        return;
    }

    if (mapProvider->isElevationProvider()) {
        const SharedElevationProvider elevationProvider = std::dynamic_pointer_cast<const ElevationProvider>(mapProvider);
        image = elevationProvider->serialize(image);
        if (image.isEmpty()) {
            setError(QGeoTiledMapReply::ParseError, tr("Failed to Serialize Terrain Tile"));
            return;
        }
    }
    const QString format = mapProvider->getImageFormat(image);
    if (format.isEmpty()) {
        setError(QGeoTiledMapReply::ParseError, tr("Unknown Format"));
        return;
    }

    QGeoFileTileCacheQGC::cacheTile(mapProvider->getMapName(), _fetchX, _fetchY, _fetchZoom, image, format);

    if (_isOverzoom) {
        QString croppedFormat;
        const QByteArray cropped = cropOverzoomTile(
            image, tileSpec().x(), tileSpec().y(), tileSpec().zoom(), _fetchZoom, &croppedFormat);
        if (cropped.isEmpty()) {
            setError(QGeoTiledMapReply::ParseError, tr("Failed to Overzoom Tile"));
            return;
        }
        setMapImageData(cropped);
        setMapImageFormat(croppedFormat);
        // Mark derived overzoom tiles as cached so Qt does not write duplicate crops to disk.
        setCached(true);
        setFinished(true);
        return;
    }

    setMapImageData(image);
    setMapImageFormat(format);
    setFinished(true);
}

QByteArray QGeoTiledMapReplyQGC::cropOverzoomTile(const QByteArray &parentImageData,
                                                 int x, int y, int zoom,
                                                 int parentZoom,
                                                 QString *outFormat,
                                                 int outputTileSize)
{
    if (parentImageData.isEmpty() || zoom <= parentZoom || parentZoom < 0 ||
        (zoom - parentZoom) > 16 || outputTileSize <= 0) {
        return {};
    }

    const QImage parentImg = QImage::fromData(parentImageData);
    if (parentImg.isNull() || parentImg.width() <= 0 || parentImg.height() <= 0) {
        return {};
    }

    const int dz = zoom - parentZoom;
    const int tilesPerAxis = 1 << dz;
    const int subX = x & (tilesPerAxis - 1);
    const int subY = y & (tilesPerAxis - 1);

    const int w = parentImg.width();
    const int h = parentImg.height();
    const double tileW = static_cast<double>(w) / tilesPerAxis;
    const double tileH = static_cast<double>(h) / tilesPerAxis;

    const int rx = qBound(0, static_cast<int>(std::floor(subX * tileW)), w - 1);
    const int ry = qBound(0, static_cast<int>(std::floor(subY * tileH)), h - 1);
    const int rx2 = qBound(rx + 1, static_cast<int>(std::floor((subX + 1) * tileW)), w);
    const int ry2 = qBound(ry + 1, static_cast<int>(std::floor((subY + 1) * tileH)), h);

    QImage cropped = parentImg.copy(rx, ry, rx2 - rx, ry2 - ry);
    if (cropped.width() != outputTileSize || cropped.height() != outputTileSize) {
        cropped = cropped.scaled(outputTileSize, outputTileSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    QByteArray encoded;
    QBuffer buffer(&encoded);
    if (!buffer.open(QIODevice::WriteOnly) || !cropped.save(&buffer, "PNG")) {
        return {};
    }

    if (outFormat != nullptr) {
        *outFormat = QStringLiteral("png");
    }
    return encoded;
}

void QGeoTiledMapReplyQGC::_networkReplyError(QNetworkReply::NetworkError error)
{
    if (error != QNetworkReply::OperationCanceledError) {
        const QNetworkReply* const reply = qobject_cast<const QNetworkReply*>(sender());
        if (!reply) {
            setError(QGeoTiledMapReply::CommunicationError, tr("Invalid Reply"));
        } else {
            setError(QGeoTiledMapReply::CommunicationError, reply->errorString());
        }
    } else {
        setFinished(true);
    }
}

void QGeoTiledMapReplyQGC::_networkReplySslErrors(const QList<QSslError> &errors)
{
    QString errorString;
    for (const QSslError &error : errors) {
        if (!errorString.isEmpty()) {
            (void) errorString.append('\n');
        }
        (void) errorString.append(error.errorString());
    }

    if (!errorString.isEmpty()) {
        setError(QGeoTiledMapReply::CommunicationError, errorString);
    }
}

void QGeoTiledMapReplyQGC::_cacheReply(QGCCacheTile *tile)
{
    if (tile) {
        if (_isOverzoom) {
            QString croppedFormat;
            const QByteArray cropped = cropOverzoomTile(
                tile->img, tileSpec().x(), tileSpec().y(), tileSpec().zoom(), _fetchZoom, &croppedFormat);
            delete tile;
            if (cropped.isEmpty()) {
                setError(QGeoTiledMapReply::ParseError, tr("Failed to Overzoom Tile"));
                return;
            }
            setMapImageData(cropped);
            setMapImageFormat(croppedFormat);
            setCached(true);
            setFinished(true);
            return;
        }
        setMapImageData(tile->img);
        setMapImageFormat(tile->format);
        setCached(true);
        setFinished(true);
        delete tile;
    } else {
        setError(QGeoTiledMapReply::UnknownError, tr("Invalid Cache Tile"));
    }
}

void QGeoTiledMapReplyQGC::_cacheError(QGCMapTask::TaskType type, QStringView errorString)
{
    Q_UNUSED(errorString);

    Q_ASSERT(type == QGCMapTask::TaskType::taskFetchTile);

    if (!QGCNetworkHelper::isInternetAvailable()) {
        setError(QGeoTiledMapReply::CommunicationError, tr("Network Not Available"));
        return;
    }

    _request.setOriginatingObject(this);

    QNetworkReply* const reply = _networkManager->get(_request);
    reply->setParent(this);
    QGCNetworkHelper::ignoreSslErrorsIfNeeded(reply);

    (void) connect(reply, &QNetworkReply::finished, this, &QGeoTiledMapReplyQGC::_networkReplyFinished);
    (void) connect(reply, &QNetworkReply::errorOccurred, this, &QGeoTiledMapReplyQGC::_networkReplyError);
    (void) connect(reply, &QNetworkReply::sslErrors, this, &QGeoTiledMapReplyQGC::_networkReplySslErrors);
    (void) connect(this, &QGeoTiledMapReplyQGC::aborted, reply, &QNetworkReply::abort);
}

void QGeoTiledMapReplyQGC::abort()
{
    QGeoTiledMapReply::abort();
}
