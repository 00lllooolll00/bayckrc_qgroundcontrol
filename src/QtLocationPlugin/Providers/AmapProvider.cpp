#include "AmapProvider.h"

QString AmapProvider::_getURL(int x, int y, int zoom) const
{
    // Tiles are served from webrd01-04 / webst01-04; rotate over them by tile position
    // the same way the other providers spread load (see MapProvider::_getServerNum).
    return _mapUrl
        .arg(_getServerNum(x, y, 4) + 1)
        .arg(x)
        .arg(y)
        .arg(zoom);
}
