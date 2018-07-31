/*
    Copyright 2016 Benjamin Vedder	benjamin@vedder.se

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
    */

#include <QDebug>
#include <math.h>
#include <qmath.h>
#include <QDir>
#include <QTime>

#include "mapwidget.h"
#include "utility.h"

namespace
{
static void normalizeAngleRad(double &angle)
{
    angle = fmod(angle, 2.0 * M_PI);

    if (angle < 0.0) {
        angle += 2.0 * M_PI;
    }
}

void minMaxEps(double x, double y, double &min, double &max) {
    double eps;

    if (fabs(x) > fabs(y)) {
        eps = fabs(x) / 1e10;
    } else {
        eps = fabs(y) / 1e10;
    }

    if (x > y) {
        max = x + eps;
        min = y - eps;
    } else {
        max = y + eps;
        min = x - eps;
    }
}

bool isPointWithinRect(const QPointF &p, double xStart, double xEnd, double yStart, double yEnd) {
    return p.x() >= xStart && p.x() <= xEnd && p.y() >= yStart && p.y() <= yEnd;
}

// See https://www.topcoder.com/community/data-science/data-science-tutorials/geometry-concepts-line-intersection-and-its-applications/
bool lineSegmentIntersection(const QPointF &p1, const QPointF &p2, const QPointF &q1, const QPointF &q2) {
    bool res = false;

    const double A1 = p2.y() - p1.y();
    const double B1 = p1.x() - p2.x();
    const double C1 = A1 * p1.x() + B1 * p1.y();

    const double A2 = q2.y() - q1.y();
    const double B2 = q1.x() - q2.x();
    const double C2 = A2 * q1.x() + B2 * q1.y();

    const double det = A1 * B2 - A2 * B1;

    if(fabs(det) < 1e-6) {
        //Lines are parallel
    } else {
        double x = (B2 * C1 - B1 * C2) / det;
        double y = (A1 * C2 - A2 * C1) / det;

        // Check if this point is on both line segments.
        double p1XMin, p1XMax, p1YMin, p1YMax;
        minMaxEps(p1.x(), p2.x(), p1XMin, p1XMax);
        minMaxEps(p1.y(), p2.y(), p1YMin, p1YMax);

        double q1XMin, q1XMax, q1YMin, q1YMax;
        minMaxEps(q1.x(), q2.x(), q1XMin, q1XMax);
        minMaxEps(q1.y(), q2.y(), q1YMin, q1YMax);

        if (    x <= p1XMax && x >= p1XMin &&
                y <= p1YMax && y >= p1YMin &&
                x <= q1XMax && x >= q1XMin &&
                y <= q1YMax && y >= q1YMin) {
            res = true;
        }
    }

    return res;
}

bool isLineSegmentWithinRect(const QPointF &p1, const QPointF &p2, double xStart, double xEnd, double yStart, double yEnd) {
    QPointF q1(xStart, yStart);
    QPointF q2(xEnd, yStart);

    bool res = lineSegmentIntersection(p1, p2, q1, q2);

    if (!res) {
        q1.setX(xStart);
        q1.setY(yStart);
        q2.setX(xStart);
        q2.setY(yEnd);
        res = lineSegmentIntersection(p1, p2, q1, q2);
    }

    if (!res) {
        q1.setX(xStart);
        q1.setY(yEnd);
        q2.setX(xEnd);
        q2.setY(yEnd);
        res = lineSegmentIntersection(p1, p2, q1, q2);
    }

    if (!res) {
        q1.setX(xEnd);
        q1.setY(yStart);
        q2.setX(xEnd);
        q2.setY(yEnd);
        res = lineSegmentIntersection(p1, p2, q1, q2);
    }

    return res;
}
}

MapWidget::MapWidget(QWidget *parent) :
    MapWidgetType(parent)
{
    mScaleFactor = 0.1;
    mRotation = 0;
    mXOffset = 0;
    mYOffset = 0;
    mMouseLastX = 1000000;
    mMouseLastY = 1000000;
    mFollowCar = -1;
    mTraceCar = -1;
    mSelectedCar = -1;
    xRealPos = 0;
    yRealPos = 0;
    mCarInfo.clear();
    mCarTrace.clear();
    mCarTraceGps.clear();
    mRoutePointSpeed = 1.0;
    mRoutePointTime = 0.0;
    mAntialiasDrawings = false;
    mAntialiasOsm = true;
    mInfoTraceTextZoom = 0.5;
    mDrawGrid = true;
    mRoutePointSelected = -1;
    mRouteNow = 0;
    mTraceMinSpaceCar = 0.05;
    mTraceMinSpaceGps = 0.05;
    mInfoTraceNow = 0;

    mOsm = new OsmClient(this);
    mDrawOpenStreetmap = true;
    mOsmZoomLevel = 15;
    mOsmRes = 1.0;
    mOsmMaxZoomLevel = 19;
    mDrawOsmStats = false;

    mRoutes.clear();
    QList<LocPoint> l;
    mRoutes.append(l);

    mInfoTraces.clear();
    mInfoTraces.append(l);

    // Set this to the SP base station position for now
    mRefLat = 57.71495867;
    mRefLon = 12.89134921;
    mRefHeight = 219.0;

    // Home
//    mRefLat = 57.57848470;
//    mRefLon = 13.11463205;
//    mRefHeight = 204.626;

    // Hardcoded for now
    mOsm->setCacheDir("osm_tiles");
    //    mOsm->setTileServerUrl("http://tile.openstreetmap.org");
    //    mOsm->setTileServerUrl("http://c.osm.rrze.fau.de/osmhd"); // Also https
    //    mOsm->setTileServerUrl("http://tiles.vedder.se/osm_tiles");
    mOsm->setTileServerUrl("http://tiles.vedder.se/osm_tiles_hd");

    connect(mOsm, SIGNAL(tileReady(OsmTile)),
            this, SLOT(tileReady(OsmTile)));
    connect(mOsm, SIGNAL(errorGetTile(QString)),
            this, SLOT(errorGetTile(QString)));

    setMouseTracking(true);

/* START CHRONOS CODE */

/* Chronos ref point*/
    mRefLat = 57.777360; // LATb
    mRefLon = 12.780472; // LONb
    mRefHeight = 201.934115075;

    mUdpSocket = new QUdpSocket(this);

    bool boResult = mUdpSocket->bind(QHostAddress::Any, 53250);
    if(boResult)
    {
        connect(mUdpSocket, SIGNAL(readyRead()), this, SLOT(readPendingDatagrams()));
        qDebug() << "UDP visualization started on address" << mUdpSocket->localAddress().toString() <<  " port: " << mUdpSocket->localPort();
    }
    else
    {
        qDebug() << "UDP visualization not able to start";
    }

    mWebSocket = new QWebSocket(QString(),QWebSocketProtocol::VersionLatest,this);
    connect(mWebSocket, SIGNAL(connected()), this, SLOT(onConnected()));
    connect(mWebSocket, SIGNAL(disconnected()), mWebSocket, SLOT(close()));
    mWebSocket->open(QUrl(QStringLiteral("ws://10.130.23.14:53251")));

    //QList<QPointF> trajTemp;
    QDir dir("./traj/");

    double ref_llh[3];
    double llh[3];
    double xyz[3];

    qDebug() << "Path: " << dir.absolutePath();

    if ( dir.exists() )
    {
        QFileInfoList entries = dir.entryInfoList( QDir::NoDotAndDotDot | QDir::Files);
        for(int i = 0; i < entries.size(); ++i)
        {
            QList<QString> stringList;
            QString line;
            QFile file(entries[i].absoluteFilePath());
            file.open(QIODevice::ReadOnly);

            /* First already exist so create only for preeciding ones */
            if(i!=0)
            {
                QList<LocPoint> emptyList;
                mInfoTraces.append(emptyList);
            }

            QTextStream in(&file);
            while(!in.atEnd())
            {
                line = in.readLine();
                QStringList list = line.split(';');
                if(list[0].compare("LINE") == 0)
                {
                    //qDebug() << "X: " << list[2] << " Y: " << list[3] << " Z: " << list[4];

                    // Transform  xyz to llh, hardcoded origo
                    ref_llh[0] = mRefLat; // LATb
                    ref_llh[1] = mRefLon; // LONb
                    ref_llh[2] = mRefHeight;

                    xyz[0] = list[2].toDouble();
                    xyz[1] = list[3].toDouble();
                    xyz[2] = list[4].toDouble();
                    //utility::enuToLlh(ref_llh,xyz,llh);
                    //llh[2] = 219.0;

                    // x = (LON - LONb) * (M_PI/180)*R*cos(((LAT - LATb)/2)*(M_PI/180))
                    // y = (LAT - LATb) * (M_PI/180)*R

                    // LAT = y / ((M_PI/180)*R) + LATb
                    // LON = x / ((M_PI/180)*R*cos(((LAT - LATb)/2)*(M_PI/180))) + LONb


                    llh[0] = xyz[1] / ((M_PI/180)*6378137.0) + ref_llh[0];
                    llh[1] = xyz[0] / ((M_PI/180)*6378137.0*cos(((llh[0] + ref_llh[0])/2)*(M_PI/180))) + ref_llh[1];
                    llh[2] = ref_llh[2] + xyz[2];

                    qDebug() << "LLH: " << QString::number(llh[0], 'g', 8) << " " << QString::number(llh[1], 'g', 8) << " " << QString::number(llh[2], 'g', 8);

                    // Transform to x,y,z according to getEnuRef
                    getEnuRef(ref_llh);
                    utility::llhToEnu(ref_llh, llh, xyz);

                    qDebug() << "ref_llh: " << QString::number(ref_llh[0], 'g', 8) << " " << QString::number(ref_llh[1], 'g', 8) << " " << QString::number(ref_llh[2], 'g', 8);

                    qDebug() << "ENU: " << xyz[0] << " " << xyz[1] << " " << xyz[2];

                    //trajTemp.append(QPointF(xyz[0],xyz[1]));
                    //mInfoTrace.append(LocPoint(xyz[0],xyz[1]));
                    mInfoTraces[i].append(LocPoint(xyz[0],xyz[1]));
                }

            }
            file.close();
        }
    }
}

MapWidget::~MapWidget()
{
    mWebSocket->close();
}

void MapWidget::onConnected()
{
    qDebug() << "WebSocket connected";

    connect(mWebSocket, SIGNAL(binaryMessageReceived(const QByteArray &)), this, SLOT(onBinaryMessageReceived(const QByteArray &)));
    connect(mWebSocket, SIGNAL(textMessageReceived(const QString &)), this, SLOT(onTextMessageReceived(const QString &)));
}

void MapWidget::onBinaryMessageReceived(const QByteArray &message)
{
    qDebug() << "Message received:" << message;

    displayMessage(message);
}

void MapWidget::onTextMessageReceived(const QString &message)
{
    (void)message;
    qDebug() << "Message received:" << message;
    QByteArray textTemp(message.toUtf8());
    displayMessage(textTemp);
}


void MapWidget::readPendingDatagrams()
{
    QByteArray datagram;

    while (mUdpSocket->hasPendingDatagrams()) {
        datagram.resize(mUdpSocket->pendingDatagramSize());
        QHostAddress sender;
        quint16 senderPort;

        mUdpSocket->readDatagram(datagram.data(), datagram.size(),&sender, &senderPort);

        displayMessage(datagram);
    }
}

void MapWidget::displayMessage(const QByteArray& message)
{
    /* Handle incoming packet */
    int id = 0;
    double ref_llh[3];
    double llh[3];
    double xyz[3];
    double heading,speed;
    QList<QByteArray> list = message.split(';');

    getEnuRef(ref_llh);

    id = list[0].toInt();

    if(list[3].length()>7)
    {
        llh[0] = list[3].insert(list[3].length()-7,'.').toDouble();
    }
    else
    {
        llh[0] = 0;
    }

    if(list[4].length()>7)
    {
        llh[1] = list[4].insert(list[4].length()-7,'.').toDouble();
    }
    else
    {
        llh[1] = 0;
    }
    llh[2] = 219.0;// list[5].insert(list[5].length()-7,'.').toDouble();
    if(list[6].length()>2)
    {
        speed = list[6].insert(list[6].length()-2,'.').toDouble();
    }
    else
    {
        speed = 0;
    }

    if(list[7].length()>1)
    {
        heading = list[7].insert(list[7].length()-1,'.').toDouble();
    }
    else
    {
        heading = 0;
    }
    heading = (360-heading)*M_PI/180;



    qDebug() << "Parse m: " << id << " " << list[1] << " " << QString::number( llh[0], 'f', 6 )
         << " " << QString::number( llh[1], 'f', 6 ) << " " << message;

    CarInfo* carInfo = getCarInfo(id);

    if(!carInfo)
    {
        addCar(CarInfo(id));
    }
    else
    {
        utility::llhToEnu(ref_llh, llh, xyz);
        LocPoint pos;
        pos.setXY(xyz[0], xyz[1]);
        pos.setXY(xyz[0], xyz[1]);
        pos.setSpeed(speed);
        pos.setAlpha(heading);
        carInfo->setLocation(pos);
        carInfo->setLocationGps(pos);
        update();
    }
/* END CHRONOS CODE */
}

CarInfo *MapWidget::getCarInfo(int car)
{
    for (int i = 0;i < mCarInfo.size();i++) {
        if (mCarInfo[i].getId() == car) {
            return &mCarInfo[i];
        }
    }

    return 0;
}

void MapWidget::addCar(CarInfo car)
{
    mCarInfo.append(car);
    update();
}

bool MapWidget::removeCar(int carId)
{
    QMutableListIterator<CarInfo> i(mCarInfo);
    while (i.hasNext()) {
        if (i.next().getId() == carId) {
            i.remove();
            return true;
        }
    }

    return false;
}

void MapWidget::setScaleFactor(double scale)
{
    double scaleDiff = scale / mScaleFactor;
    mScaleFactor = scale;
    mXOffset *= scaleDiff;
    mYOffset *= scaleDiff;
    update();
}

void MapWidget::setRotation(double rotation)
{
    mRotation = rotation;
    update();
}

void MapWidget::setXOffset(double offset)
{
    mXOffset = offset;
    update();
}

void MapWidget::setYOffset(double offset)
{
    mYOffset = offset;
    update();
}

void MapWidget::clearTrace()
{
    mCarTrace.clear();
    mCarTraceGps.clear();
    update();
}

void MapWidget::addRoutePoint(double px, double py, double speed, qint32 time)
{
    LocPoint pos;
    pos.setXY(px, py);
    pos.setSpeed(speed);
    pos.setTime(time);
    mRoutes[mRouteNow].append(pos);
    update();
}

QList<LocPoint> MapWidget::getRoute()
{
    return mRoutes[mRouteNow];
}

void MapWidget::setRoute(QList<LocPoint> route)
{
    mRoutes[mRouteNow] = route;
    update();
}

void MapWidget::clearRoute()
{
    mRoutes[mRouteNow].clear();
    update();
}

void MapWidget::clearAllRoutes()
{
    for (int i = 0;i < mRoutes.size();i++) {
        mRoutes[i].clear();
    }

    update();
}

void MapWidget::setRoutePointSpeed(double speed)
{
    mRoutePointSpeed = speed;
}

void MapWidget::addInfoPoint(LocPoint &info)
{
    mInfoTraces[mInfoTraceNow].append(info);
    update();
}

void MapWidget::clearInfoTrace()
{
    mInfoTraces[mInfoTraceNow].clear();
    update();
}

void MapWidget::clearAllInfoTraces()
{
    for (int i = 0;i < mInfoTraces.size();i++) {
        mInfoTraces[i].clear();
    }

    update();
}

void MapWidget::addPerspectivePixmap(PerspectivePixmap map)
{
    mPerspectivePixmaps.append(map);
}

void MapWidget::clearPerspectivePixmaps()
{
    mPerspectivePixmaps.clear();
    update();
}

QPoint MapWidget::getMousePosRelative()
{
    QPoint p = this->mapFromGlobal(QCursor::pos());
    p.setX((p.x() - mXOffset - width() / 2) / mScaleFactor);
    p.setY((-p.y() - mYOffset + height() / 2) / mScaleFactor);
    return p;
}

void MapWidget::setAntialiasDrawings(bool antialias)
{
    mAntialiasDrawings = antialias;
    update();
}

void MapWidget::setAntialiasOsm(bool antialias)
{
    mAntialiasOsm = antialias;
    update();
}

void MapWidget::tileReady(OsmTile tile)
{
    (void)tile;
    update();
}

void MapWidget::errorGetTile(QString reason)
{
    qWarning() << "OSM tile error:" << reason;
}

void MapWidget::setFollowCar(int car)
{
    int oldCar = mFollowCar;
    mFollowCar = car;

    if (oldCar != mFollowCar) {
        update();
    }
}

void MapWidget::setTraceCar(int car)
{
    mTraceCar = car;
}

void MapWidget::setSelectedCar(int car)
{
    int oldCar = mSelectedCar;
    mSelectedCar = car;

    if (oldCar != mSelectedCar) {
        update();
    }
}

void MapWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);

    painter.setRenderHint(QPainter::Antialiasing, mAntialiasDrawings);
    painter.setRenderHint(QPainter::TextAntialiasing, mAntialiasDrawings);

    const double scaleMax = 20;
    const double scaleMin = 0.000001;

    // Make sure scale and offsetappend is reasonable
    if (mScaleFactor < scaleMin) {
        double scaleDiff = scaleMin / mScaleFactor;
        mScaleFactor = scaleMin;
        mXOffset *= scaleDiff;
        mYOffset *= scaleDiff;
    } else if (mScaleFactor > scaleMax) {
        double scaleDiff = scaleMax / mScaleFactor;
        mScaleFactor = scaleMax;
        mXOffset *= scaleDiff;
        mYOffset *= scaleDiff;
    }

    // Optionally follow a car
    if (mFollowCar >= 0) {
        for (int i = 0;i < mCarInfo.size();i++) {
            CarInfo &carInfo = mCarInfo[i];
            if (carInfo.getId() == mFollowCar) {
                LocPoint followLoc = carInfo.getLocation();
                mXOffset = -followLoc.getX() * 1000.0 * mScaleFactor;
                mYOffset = -followLoc.getY() * 1000.0 * mScaleFactor;
            }

        }
    }

    // Limit the offset to avoid overflow at 2^31 mm
    double lim = 2000000000.0 * mScaleFactor;
    if (mXOffset > lim) {
        mXOffset = lim;
    } else if (mXOffset < -lim) {
        mXOffset = -lim;
    }

    if (mYOffset > lim) {
        mYOffset = lim;
    } else if (mYOffset < -lim) {
        mYOffset = -lim;
    }

    // Paint begins here
    painter.fillRect(event->rect(), QBrush(Qt::transparent));

    const double car_w = 800;
    const double car_h = 335;
    const double car_corner = 20;
    double angle, x, y;
    QString txt;
    QPointF pt_txt;
    QRectF rect_txt;
    QPen pen;
    QFont font = this->font();

    // Map coordinate transforms
    QTransform drawTrans;
    drawTrans.translate(event->rect().width() / 2 + mXOffset, event->rect().height() / 2 - mYOffset);
    drawTrans.scale(mScaleFactor, -mScaleFactor);
    drawTrans.rotate(mRotation);

    // Text coordinates
    QTransform txtTrans;
    txtTrans.translate(0, 0);
    txtTrans.scale(1, 1);
    txtTrans.rotate(0);

    // Set font
    font.setPointSize(10);
    font.setFamily("Monospace");
    painter.setFont(font);

    // Grid parameters
    double stepGrid = 20.0 * ceil(1.0 / ((mScaleFactor * 10.0) / 50.0));
    bool gridDiv = false;
    if (round(log10(stepGrid)) > log10(stepGrid)) {
        gridDiv = true;
    }
    stepGrid = pow(10.0, round(log10(stepGrid)));
    if (gridDiv) {
        stepGrid /= 2.0;
    }
    const double zeroAxisWidth = 3;
    const QColor zeroAxisColor = Qt::red;
    const QColor firstAxisColor = Qt::gray;
    const QColor secondAxisColor = Qt::blue;
    const QColor textColor = QPalette::Foreground;

    // Grid boundries in mm
    const double xStart = -ceil(width() / stepGrid / mScaleFactor) * stepGrid - ceil(mXOffset / stepGrid / mScaleFactor) * stepGrid;
    const double xEnd = ceil(width() / stepGrid / mScaleFactor) * stepGrid - floor(mXOffset / stepGrid / mScaleFactor) * stepGrid;
    const double yStart = -ceil(height() / stepGrid / mScaleFactor) * stepGrid - ceil(mYOffset / stepGrid / mScaleFactor) * stepGrid;
    const double yEnd = ceil(height() / stepGrid / mScaleFactor) * stepGrid - floor(mYOffset / stepGrid / mScaleFactor) * stepGrid;

    // View center, width and height in m
    const double cx = -mXOffset / mScaleFactor / 1000.0;
    const double cy = -mYOffset / mScaleFactor / 1000.0;
    const double view_w = width() / mScaleFactor / 1000.0;
    const double view_h = height() / mScaleFactor / 1000.0;

    // View boundries in mm
    const double xStart2 = (cx - view_w / 2.0) * 1000.0;
    const double yStart2 = (cy - view_h / 2.0) * 1000.0;
    const double xEnd2 = (cx + view_w / 2.0) * 1000.0;
    const double yEnd2 = (cy + view_h / 2.0) * 1000.0;

    // Draw perspective pixmaps first
    painter.setTransform(drawTrans);
    for(int i = 0;i < mPerspectivePixmaps.size();i++) {
        mPerspectivePixmaps[i].drawUsingPainter(painter);
    }

    // Draw openstreetmap tiles
    if (mDrawOpenStreetmap) {
        double i_llh[3];
        i_llh[0] = mRefLat;
        i_llh[1] = mRefLon;
        i_llh[2] = mRefHeight;

        mOsmZoomLevel = (int)round(log2(mScaleFactor * mOsmRes * 100000000.0 *
                                        cos(i_llh[0] * M_PI / 180.0)));
        if (mOsmZoomLevel > mOsmMaxZoomLevel) {
            mOsmZoomLevel = mOsmMaxZoomLevel;
        } else if (mOsmZoomLevel < 0) {
            mOsmZoomLevel = 0;
        }

        int xt = OsmTile::long2tilex(i_llh[1], mOsmZoomLevel);
        int yt = OsmTile::lat2tiley(i_llh[0], mOsmZoomLevel);

        double llh_t[3];
        llh_t[0] = OsmTile::tiley2lat(yt, mOsmZoomLevel);
        llh_t[1] = OsmTile::tilex2long(xt, mOsmZoomLevel);
        llh_t[2] = 0.0;

        double xyz[3];
        utility::llhToEnu(i_llh, llh_t, xyz);

        // Calculate scale at ENU origin
        double w = OsmTile::lat2width(i_llh[0], mOsmZoomLevel);

        int t_ofs_x = (int)ceil(-(cx - view_w / 2.0) / w);
        int t_ofs_y = (int)ceil((cy + view_h / 2.0) / w);

        painter.setRenderHint(QPainter::SmoothPixmapTransform, mAntialiasOsm);
        QTransform transOld = painter.transform();
        QTransform trans = painter.transform();
        trans.scale(1, -1);
        painter.setTransform(trans);

        for (int j = 0;j < 40;j++) {
            for (int i = 0;i < 40;i++) {
                int xt_i = xt + i - t_ofs_x;
                int yt_i = yt + j - t_ofs_y;
                double ts_x = xyz[0] + w * i - (double)t_ofs_x * w;
                double ts_y = -xyz[1] + w * j - (double)t_ofs_y * w;

                // We are outside the view
                if (ts_x > (cx + view_w / 2.0)) {
                    break;
                } else if ((ts_y - w) > (-cy + view_h / 2.0)) {
                    break;
                }

                int res;
                OsmTile t = mOsm->getTile(mOsmZoomLevel, xt_i, yt_i, res);

                if (w < 0.0) {
                    w = t.getWidthTop();
                }

                painter.drawPixmap(ts_x * 1000.0, ts_y * 1000.0,
                                   w * 1000.0, w * 1000.0, t.pixmap());

                if (res == 0 && !mOsm->downloadQueueFull()) {
                    mOsm->downloadTile(mOsmZoomLevel, xt_i, yt_i);
                }
            }
        }

        // Restore painter
        painter.setTransform(transOld);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, mAntialiasDrawings);
    }

    if (mDrawGrid) {
        painter.setTransform(txtTrans);

        // Draw Y-axis segments
        for (double i = xStart;i < xEnd;i += stepGrid) {
            if (fabs(i) < 1e-3) {
                i = 0.0;
            }

            if ((int)(i / stepGrid) % 2) {
                pen.setWidth(0);
                pen.setColor(firstAxisColor);
                painter.setPen(pen);
            } else {
                txt.sprintf("%.2f m", i / 1000.0);

                pt_txt.setX(i);
                pt_txt.setY(0);
                pt_txt = drawTrans.map(pt_txt);
                pt_txt.setX(pt_txt.x() + 5);
                pt_txt.setY(height() - 10);
                painter.setPen(QPen(textColor));
                painter.drawText(pt_txt, txt);

                if (fabs(i) < 1e-3) {
                    pen.setWidthF(zeroAxisWidth);
                    pen.setColor(zeroAxisColor);
                } else {
                    pen.setWidth(0);
                    pen.setColor(secondAxisColor);
                }
                painter.setPen(pen);
            }

            QPointF pt_start(i, yStart);
            QPointF pt_end(i, yEnd);
            pt_start = drawTrans.map(pt_start);
            pt_end = drawTrans.map(pt_end);
            painter.drawLine(pt_start, pt_end);
        }

        // Draw X-axis segments
        for (double i = yStart;i < yEnd;i += stepGrid) {
            if (fabs(i) < 1e-3) {
                i = 0.0;
            }

            if ((int)(i / stepGrid) % 2) {
                pen.setWidth(0);
                pen.setColor(firstAxisColor);
                painter.setPen(pen);
            } else {
                txt.sprintf("%.2f m", i / 1000.0);
                pt_txt.setY(i);

                pt_txt = drawTrans.map(pt_txt);
                pt_txt.setX(10);
                pt_txt.setY(pt_txt.y() - 5);
                painter.setPen(QPen(textColor));
                painter.drawText(pt_txt, txt);

                if (fabs(i) < 1e-3) {
                    pen.setWidthF(zeroAxisWidth);
                    pen.setColor(zeroAxisColor);
                } else {
                    pen.setWidth(0);
                    pen.setColor(secondAxisColor);
                }
                painter.setPen(pen);
            }

            QPointF pt_start(xStart, i);
            QPointF pt_end(xEnd, i);
            pt_start = drawTrans.map(pt_start);
            pt_end = drawTrans.map(pt_end);
            painter.drawLine(pt_start, pt_end);
        }
    }

    // Store trace for the selected car
    if (mTraceCar >= 0) {
        for (int i = 0;i < mCarInfo.size();i++) {
            CarInfo &carInfo = mCarInfo[i];
            if (carInfo.getId() == mTraceCar) {
                if (mCarTrace.isEmpty()) {
                    mCarTrace.append(carInfo.getLocation());
                }

                if (mCarTrace.last().getDistanceTo(carInfo.getLocation()) > mTraceMinSpaceCar) {
                    mCarTrace.append(carInfo.getLocation());
                }
            }
        }

        // GPS Trace
        for (int i = 0;i < mCarInfo.size();i++) {
            CarInfo &carInfo = mCarInfo[i];
            if (carInfo.getId() == mTraceCar) {
                if (mCarTraceGps.isEmpty()) {
                    mCarTraceGps.append(carInfo.getLocationGps());
                }

                if (mCarTraceGps.last().getDistanceTo(carInfo.getLocationGps()) > mTraceMinSpaceGps) {
                    mCarTraceGps.append(carInfo.getLocationGps());
                }
            }
        }
    }

    // Draw info trace
    int info_segments = 0;
    int info_points = 0;
    mVisibleInfoTracePoints.clear();

    for (int in = 0;in < mInfoTraces.size();in++) {
        QList<LocPoint> &itNow = mInfoTraces[in];

        if (mInfoTraceNow == in) {
            pen.setColor(Qt::darkGreen);
            painter.setBrush(Qt::green);
        } else {
            pen.setColor(Qt::darkGreen);
            painter.setBrush(Qt::green);
        }

        pen.setWidthF(3.0);
        painter.setPen(pen);
        painter.setTransform(txtTrans);

        const double info_min_dist = 0.02;

        int last_visible = 0;
        for (int i = 1;i < itNow.size();i++) {
            double dist_view = itNow.at(i).getDistanceTo(itNow.at(last_visible)) * mScaleFactor;
            if (dist_view < info_min_dist) {
                continue;
            }

            bool draw = isPointWithinRect(itNow[last_visible].getPointMm(), xStart2, xEnd2, yStart2, yEnd2);

            if (!draw) {
                draw = isPointWithinRect(itNow[i].getPointMm(), xStart2, xEnd2, yStart2, yEnd2);
            }

            if (!draw) {
                draw = isLineSegmentWithinRect(itNow[last_visible].getPointMm(),
                                               itNow[i].getPointMm(),
                                               xStart2, xEnd2, yStart2, yEnd2);
            }

            if (draw) {
                QPointF p1 = drawTrans.map(itNow[last_visible].getPointMm());
                QPointF p2 = drawTrans.map(itNow[i].getPointMm());

                painter.drawLine(p1, p2);
                info_segments++;
            }

            last_visible = i;
        }

        QList<LocPoint> pts_green;
        QList<LocPoint> pts_red;
        QList<LocPoint> pts_other;

        for (int i = 0;i < itNow.size();i++) {
            LocPoint ip = itNow[i];

            if (mInfoTraceNow != in) {
                ip.setColor(Qt::gray);
            }

            if (ip.getColor() == Qt::darkGreen || ip.getColor() == Qt::green) {
                pts_green.append(ip);
            } else if (ip.getColor() == Qt::darkRed || ip.getColor() == Qt::red) {
                pts_red.append(ip);
            } else {
                pts_other.append(ip);
            }
        }

        info_points += drawInfoPoints(painter, pts_green, drawTrans, txtTrans,
                                      xStart2, xEnd2, yStart2, yEnd2, info_min_dist);
        info_points += drawInfoPoints(painter, pts_other, drawTrans, txtTrans,
                                      xStart2, xEnd2, yStart2, yEnd2, info_min_dist);
        info_points += drawInfoPoints(painter, pts_red, drawTrans, txtTrans,
                                      xStart2, xEnd2, yStart2, yEnd2, info_min_dist);
    }

    // Draw point closest to mouse pointer
    if (mClosestInfo.getInfo().size() > 0) {
        QPointF p = mClosestInfo.getPointMm();
        QPointF p2 = drawTrans.map(p);

        painter.setTransform(txtTrans);
        pen.setColor(Qt::green);
        painter.setBrush(Qt::green);
        painter.setPen(Qt::green);
        painter.drawEllipse(p2, mClosestInfo.getRadius(), mClosestInfo.getRadius());

        pt_txt.setX(p.x() + 5 / mScaleFactor);
        pt_txt.setY(p.y());
        painter.setTransform(txtTrans);
        pt_txt = drawTrans.map(pt_txt);
        pen.setColor(Qt::black);
        painter.setPen(pen);
        rect_txt.setCoords(pt_txt.x(), pt_txt.y() - 20,
                           pt_txt.x() + 500, pt_txt.y() + 500);
        painter.drawText(rect_txt, Qt::AlignTop | Qt::AlignLeft, mClosestInfo.getInfo());
    }

    // Draw trace for the selected car
    pen.setWidthF(5.0 / mScaleFactor);
    pen.setColor(Qt::red);
    painter.setPen(pen);
    painter.setTransform(drawTrans);
    for (int i = 1;i < mCarTrace.size();i++) {
        painter.drawLine(mCarTrace[i - 1].getX() * 1000.0, mCarTrace[i - 1].getY() * 1000.0,
                mCarTrace[i].getX() * 1000.0, mCarTrace[i].getY() * 1000.0);
    }

    // Draw GPS trace for the selected car
    pen.setWidthF(2.5 / mScaleFactor);
    pen.setColor(Qt::magenta);
    painter.setPen(pen);
    painter.setTransform(drawTrans);
    for (int i = 1;i < mCarTraceGps.size();i++) {
        painter.drawLine(mCarTraceGps[i - 1].getX() * 1000.0, mCarTraceGps[i - 1].getY() * 1000.0,
                mCarTraceGps[i].getX() * 1000.0, mCarTraceGps[i].getY() * 1000.0);
    }

    // Draw routes
    for (int rn = 0;rn < mRoutes.size();rn++) {
        QList<LocPoint> &routeNow = mRoutes[rn];

        pen.setWidthF(5.0 / mScaleFactor);

        if (mRouteNow == rn) {
            pen.setColor(Qt::darkYellow);
            painter.setBrush(Qt::yellow);
        } else {
            pen.setColor(Qt::darkGray);
            painter.setBrush(Qt::gray);
        }

        painter.setPen(pen);
        painter.setTransform(drawTrans);
        for (int i = 1;i < routeNow.size();i++) {
            painter.drawLine(routeNow[i - 1].getX() * 1000.0, routeNow[i - 1].getY() * 1000.0,
                    routeNow[i].getX() * 1000.0, routeNow[i].getY() * 1000.0);
        }
        for (int i = 0;i < routeNow.size();i++) {
            QPointF p = routeNow[i].getPointMm();
            painter.setTransform(drawTrans);
            pen.setColor(Qt::darkYellow);
            painter.setPen(pen);
            painter.drawEllipse(p, 10 / mScaleFactor, 10 / mScaleFactor);

            // Draw text only for selected route
            if (mRouteNow == rn) {
                QTime t = QTime::fromMSecsSinceStartOfDay(routeNow[i].getTime());
                txt.sprintf("%.1f km/h\n"
                            "%02d:%02d:%02d:%03d",
                            routeNow[i].getSpeed() * 3.6,
                            t.hour(), t.minute(), t.second(), t.msec());

                pt_txt.setX(p.x() + 10 / mScaleFactor);
                pt_txt.setY(p.y());
                painter.setTransform(txtTrans);
                pt_txt = drawTrans.map(pt_txt);
                pen.setColor(Qt::black);
                painter.setPen(pen);
                rect_txt.setCoords(pt_txt.x(), pt_txt.y() - 20,
                                   pt_txt.x() + 150, pt_txt.y() + 25);
                painter.drawText(rect_txt, txt);
            }
        }
    }

    // Draw cars
    painter.setPen(QPen(textColor));
    for(int i = 0;i < mCarInfo.size();i++) {
        CarInfo &carInfo = mCarInfo[i];
        LocPoint pos = carInfo.getLocation();
        LocPoint pos_gps = carInfo.getLocationGps();
        x = pos.getX() * 1000.0;
        y = pos.getY() * 1000.0;
        double x_gps = pos_gps.getX() * 1000.0;
        double y_gps = pos_gps.getY() * 1000.0;
        angle = pos.getAlpha() * 180.0 / M_PI;
        painter.setTransform(drawTrans);

        QColor col_sigma = Qt::red;
        QColor col_wheels = Qt::black;
        QColor col_bumper = Qt::green;
        QColor col_hull = carInfo.getColor();
        QColor col_center = Qt::blue;
        QColor col_gps = Qt::magenta;
        QColor col_ap = carInfo.getColor();

        if (carInfo.getId() != mSelectedCar) {
            col_wheels = Qt::darkGray;
            col_bumper = Qt::lightGray;
            col_ap = Qt::lightGray;
        }

        // Draw standard deviation
        if (pos.getSigma() > 0.0) {
            QColor col = col_sigma;
            col.setAlphaF(0.2);
            painter.setBrush(QBrush(col));
            painter.drawEllipse(pos.getPointMm(), pos.getSigma() * 1000.0, pos.getSigma() * 1000.0);
        }

        // Draw car
        painter.setBrush(QBrush(col_wheels));
        painter.save();
        painter.translate(x, y);
        painter.rotate(-angle);
        // Wheels
        painter.drawRoundedRect(-car_w / 12.0,-(car_h / 2), car_w / 6.0, car_h, car_corner / 3, car_corner / 3);
        painter.drawRoundedRect(car_w - car_w / 2.5,-(car_h / 2), car_w / 6.0, car_h, car_corner / 3, car_corner / 3);
        // Front bumper
        painter.setBrush(col_bumper);
        painter.drawRoundedRect(-car_w / 6.0, -((car_h - car_w / 20.0) / 2.0), car_w, car_h - car_w / 20.0, car_corner, car_corner);
        // Hull
        painter.setBrush(col_hull);
        painter.drawRoundedRect(-car_w / 6.0, -((car_h - car_w / 20.0) / 2.0), car_w - (car_w / 20.0), car_h - car_w / 20.0, car_corner, car_corner);
        painter.restore();

        // Center
        painter.setBrush(col_center);
        painter.drawEllipse(QPointF(x, y), car_h / 15.0, car_h / 15.0);

        // GPS Location
        painter.setBrush(col_gps);
        painter.drawEllipse(QPointF(x_gps, y_gps), car_h / 15.0, car_h / 15.0);

        // Autopilot state
        LocPoint ap_goal = carInfo.getApGoal();
        if (ap_goal.getRadius() > 0.0) {
            QPointF p = ap_goal.getPointMm();
            pen.setColor(col_ap);
            painter.setPen(pen);
            painter.drawEllipse(p, 10 / mScaleFactor, 10 / mScaleFactor);

            QPointF pm = pos.getPointMm();
            painter.setBrush(Qt::transparent);
            painter.drawEllipse(pm, ap_goal.getRadius() * 1000.0, ap_goal.getRadius() * 1000.0);
        }

        painter.setPen(QPen(textColor));

        // Print data
        QTime t = QTime::fromMSecsSinceStartOfDay(carInfo.getTime());
        txt.sprintf("%s\n"
                    "(%.3f, %.3f, %.0f)\n"
                    "%02d:%02d:%02d:%03d",
                    carInfo.getName().toLocal8Bit().data(),
                    pos.getX(), pos.getY(), angle,
                    t.hour(), t.minute(), t.second(), t.msec());
        pt_txt.setX(x + 120 + (car_w - 190) * ((cos(pos.getAlpha()) + 1) / 2));
        pt_txt.setY(y);
        painter.setTransform(txtTrans);
        pt_txt = drawTrans.map(pt_txt);
        rect_txt.setCoords(pt_txt.x(), pt_txt.y() - 20,
                           pt_txt.x() + 300, pt_txt.y() + 25);
        painter.drawText(rect_txt, txt);
    }

    double start_txt = 30.0;
    const double txt_row_h = 20.0;

    painter.setTransform(txtTrans);
    font.setPointSize(10);
    painter.setFont(font);

    // Draw units (m)
    if (mDrawGrid) {
        double res = stepGrid / 1000.0;
        if (res >= 1000.0) {
            txt.sprintf("Grid res: %.0f km", res / 1000.0);
        } else if (res >= 1.0) {
            txt.sprintf("Grid res: %.0f m", res);
        } else {
            txt.sprintf("Grid res: %.0f cm", res * 100.0);
        }
        painter.drawText(width() - 140.0, start_txt, txt);
        start_txt += txt_row_h;
    }

    // Draw zoom level
    txt.sprintf("Zoom: %.7f", mScaleFactor);
    painter.drawText(width() - 140.0, start_txt, txt);
    start_txt += txt_row_h;

    // Draw OSM zoom level
    if (mDrawOpenStreetmap) {
        txt.sprintf("OSM zoom: %d", mOsmZoomLevel);
        painter.drawText(width() - 140.0, start_txt, txt);
        start_txt += txt_row_h;

        if (mDrawOsmStats) {
            txt.sprintf("DL Tiles: %d", mOsm->getTilesDownloaded());
            painter.drawText(width() - 140.0, start_txt, txt);
            start_txt += txt_row_h;

            txt.sprintf("HDD Tiles: %d", mOsm->getHddTilesLoaded());
            painter.drawText(width() - 140.0, start_txt, txt);
            start_txt += txt_row_h;

            txt.sprintf("RAM Tiles: %d", mOsm->getRamTilesLoaded());
            painter.drawText(width() - 140.0, start_txt, txt);
            start_txt += txt_row_h;
        }
    }

    // Some info
    if (info_segments > 0) {
        txt.sprintf("Info seg: %d", info_segments);
        painter.drawText(width() - 140.0, start_txt, txt);
        start_txt += txt_row_h;
    }

    if (info_points > 0) {
        txt.sprintf("Info pts: %d", info_points);
        painter.drawText(width() - 140.0, start_txt, txt);
        start_txt += txt_row_h;
    }

    painter.end();
}

void MapWidget::mouseMoveEvent(QMouseEvent *e)
{
    if (e->buttons() & Qt::LeftButton && !(e->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier)))
    {
        int x = e->pos().x();
        int y = e->pos().y();

        if (mMouseLastX < 100000)
        {
            int diffx = x - mMouseLastX;
            mXOffset += diffx;
            update();
        }

        if (mMouseLastY < 100000)
        {
            int diffy = y - mMouseLastY;
            mYOffset -= diffy;

            emit offsetChanged(mXOffset, mYOffset);
            update();
        }

        mMouseLastX = x;
        mMouseLastY = y;
    }

    if (mRoutePointSelected >= 0) {
        LocPoint pos;
        QPoint p = getMousePosRelative();
        pos.setXY(p.x() / 1000.0, p.y() / 1000.0);

        mRoutes[mRouteNow][mRoutePointSelected].setXY(pos.getX(), pos.getY());
        update();
    }

    updateClosestInfoPoint();
}

void MapWidget::mousePressEvent(QMouseEvent *e)
{
    bool ctrl = e->modifiers() == Qt::ControlModifier;
    bool shift = e->modifiers() == Qt::ShiftModifier;
    bool ctrl_shift = e->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier);

    LocPoint pos;
    QPoint p = getMousePosRelative();
    pos.setXY(p.x() / 1000.0, p.y() / 1000.0);
    pos.setSpeed(mRoutePointSpeed);
    pos.setTime(mRoutePointTime);
    double routeDist = 0.0;
    int routeInd = getClosestPoint(pos, mRoutes[mRouteNow], routeDist);
    bool routeFound = (routeDist * mScaleFactor * 1000.0) < 20 && routeDist >= 0.0;

    if (ctrl) {
        if (e->buttons() & Qt::LeftButton) {
            if (mSelectedCar >= 0 && (e->buttons() & Qt::LeftButton)) {
                for (int i = 0;i < mCarInfo.size();i++) {
                    CarInfo &carInfo = mCarInfo[i];
                    if (carInfo.getId() == mSelectedCar) {
                        LocPoint pos = carInfo.getLocation();
                        QPoint p = getMousePosRelative();
                        pos.setXY(p.x() / 1000.0, p.y() / 1000.0);
                        carInfo.setLocation(pos);
                        emit posSet(mSelectedCar, pos);
                    }
                }
            }
        } else if (e->buttons() & Qt::RightButton) {
            if (routeFound) {
                mRoutes[mRouteNow][routeInd].setSpeed(mRoutePointSpeed);
                mRoutes[mRouteNow][routeInd].setTime(mRoutePointTime);
            }
        }
        update();
    } else if (shift) {
        if (e->buttons() & Qt::LeftButton) {
            if (routeFound) {
                mRoutePointSelected = routeInd;
                mRoutes[mRouteNow][routeInd].setXY(pos.getX(), pos.getY());
            } else {
                mRoutes[mRouteNow].append(pos);
                emit routePointAdded(pos);
            }
        } else if (e->buttons() & Qt::RightButton) {
            if (routeFound) {
                mRoutes[mRouteNow].removeAt(routeInd);
            } else {
                LocPoint pos;
                if (mRoutes[mRouteNow].size() > 0) {
                    pos = mRoutes[mRouteNow].last();
                    mRoutes[mRouteNow].removeLast();
                }
                emit lastRoutePointRemoved(pos);
            }
        }
        update();
    } else if (ctrl_shift) {
        if (e->buttons() & Qt::LeftButton) {
            QPoint p = getMousePosRelative();
            double iLlh[3], llh[3], xyz[3];
            iLlh[0] = mRefLat;
            iLlh[1] = mRefLon;
            iLlh[2] = mRefHeight;
            xyz[0] = p.x() / 1000.0;
            xyz[1] = p.y() / 1000.0;
            xyz[2] = 0.0;
            utility::enuToLlh(iLlh, xyz, llh);
            mRefLat = llh[0];
            mRefLon = llh[1];
            mRefHeight = 0.0;
        }

        update();
    }
}

void MapWidget::mouseReleaseEvent(QMouseEvent *e)
{
    if (!(e->buttons() & Qt::LeftButton)) {
        mMouseLastX = 1000000;
        mMouseLastY = 1000000;
        mRoutePointSelected = -1;
    }
}

void MapWidget::wheelEvent(QWheelEvent *e)
{
    if (e->modifiers() & Qt::ControlModifier && mSelectedCar >= 0) {
        for (int i = 0;i < mCarInfo.size();i++) {
            CarInfo &carInfo = mCarInfo[i];
            if (carInfo.getId() == mSelectedCar) {
                LocPoint pos = carInfo.getLocation();
                double angle = pos.getAlpha() + (double)e->delta() * 0.0005;
                normalizeAngleRad(angle);
                pos.setAlpha(angle);
                carInfo.setLocation(pos);
                emit posSet(mSelectedCar, pos);
                update();
            }
        }
    } else {
        int x = e->pos().x();
        int y = e->pos().y();
        double scaleDiff = ((double)e->delta() / 600.0);
        if (scaleDiff > 0.8)
        {
            scaleDiff = 0.8;
        }

        if (scaleDiff < -0.8)
        {
            scaleDiff = -0.8;
        }

        x -= width() / 2;
        y -= height() / 2;

        mScaleFactor += mScaleFactor * scaleDiff;
        mXOffset += mXOffset * scaleDiff;
        mYOffset += mYOffset * scaleDiff;

        emit scaleChanged(mScaleFactor);
        emit offsetChanged(mXOffset, mYOffset);
        update();
    }

    updateClosestInfoPoint();
}

double MapWidget::getTraceMinSpaceGps() const
{
    return mTraceMinSpaceGps;
}

void MapWidget::setTraceMinSpaceGps(double traceMinSpaceGps)
{
    mTraceMinSpaceGps = traceMinSpaceGps;
}

double MapWidget::getTraceMinSpaceCar() const
{
    return mTraceMinSpaceCar;
}

void MapWidget::setTraceMinSpaceCar(double traceMinSpaceCar)
{
    mTraceMinSpaceCar = traceMinSpaceCar;
}

qint32 MapWidget::getRoutePointTime() const
{
    return mRoutePointTime;
}

void MapWidget::setRoutePointTime(const qint32 &routePointTime)
{
    mRoutePointTime = routePointTime;
}

int MapWidget::getRouteNow() const
{
    return mRouteNow;
}

void MapWidget::setRouteNow(int routeNow)
{
    mRouteNow = routeNow;
    while (mRoutes.size() < (mRouteNow + 1)) {
        QList<LocPoint> l;
        mRoutes.append(l);
    }
    update();
}

int MapWidget::getInfoTraceNow() const
{
    return mInfoTraceNow;
}

void MapWidget::setInfoTraceNow(int infoTraceNow)
{
    int infoTraceOld = mInfoTraceNow;
    mInfoTraceNow = infoTraceNow;

    while (mInfoTraces.size() < (mInfoTraceNow + 1)) {
        QList<LocPoint> l;
        mInfoTraces.append(l);
    }
    update();

    if (infoTraceOld != mInfoTraceNow) {
        emit infoTraceChanged(mInfoTraceNow);
    }
}

bool MapWidget::getDrawOsmStats() const
{
    return mDrawOsmStats;
}

void MapWidget::setDrawOsmStats(bool drawOsmStats)
{
    mDrawOsmStats = drawOsmStats;
    update();
}

bool MapWidget::getDrawGrid() const
{
    return mDrawGrid;
}

void MapWidget::setDrawGrid(bool drawGrid)
{
    mDrawGrid = drawGrid;
    update();
}

void MapWidget::updateClosestInfoPoint()
{
    QPointF mpq = getMousePosRelative();
    LocPoint mp(mpq.x() / 1000.0, mpq.y() / 1000.0);
    double dist_min = 1e30;
    LocPoint closest;

    for (int i = 0;i < mVisibleInfoTracePoints.size();i++) {
        const LocPoint &ip = mVisibleInfoTracePoints[i];
        if (mp.getDistanceTo(ip) < dist_min) {
            dist_min = mp.getDistanceTo(ip);
            closest = ip;
        }
    }

    bool drawBefore = mClosestInfo.getInfo().size() > 0;
    bool drawNow = false;
    if ((dist_min * mScaleFactor) < 0.02) {
        if (closest != mClosestInfo) {
            mClosestInfo = closest;
            update();
        }

        drawNow = true;
    } else {
        mClosestInfo.setInfo("");
    }

    if (drawBefore && !drawNow) {
        update();
    }
}

int MapWidget::drawInfoPoints(QPainter &painter, const QList<LocPoint> &pts,
                              QTransform drawTrans, QTransform txtTrans,
                              double xStart, double xEnd, double yStart, double yEnd,
                              double min_dist)
{
    int last_visible = 0;
    int drawn = 0;
    QPointF pt_txt;
    QRectF rect_txt;

    painter.setTransform(txtTrans);

    for (int i = 0;i < pts.size();i++) {
        const LocPoint &ip = pts[i];
        QPointF p = ip.getPointMm();
        QPointF p2 = drawTrans.map(p);

        if (isPointWithinRect(p, xStart, xEnd, yStart, yEnd)) {
            if (drawn > 0) {
                double dist_view = pts.at(i).getDistanceTo(pts.at(last_visible)) * mScaleFactor;
                if (dist_view < min_dist) {
                    continue;
                }

                last_visible = i;
            }

            painter.setBrush(ip.getColor());
            painter.setPen(ip.getColor());
            painter.drawEllipse(p2, ip.getRadius(), ip.getRadius());

            drawn++;
            mVisibleInfoTracePoints.append(ip);

            if (mScaleFactor > mInfoTraceTextZoom) {
                pt_txt.setX(p.x() + 5 / mScaleFactor);
                pt_txt.setY(p.y());
                pt_txt = drawTrans.map(pt_txt);
                painter.setPen(Qt::black);
                painter.setFont(QFont("monospace"));
                rect_txt.setCoords(pt_txt.x(), pt_txt.y() - 20,
                                   pt_txt.x() + 500, pt_txt.y() + 500);
                painter.drawText(rect_txt, Qt::AlignTop | Qt::AlignLeft, ip.getInfo());
            }
        }
    }

    return drawn;
}

int MapWidget::getClosestPoint(LocPoint p, QList<LocPoint> points, double &dist)
{
    int closest = -1;
    dist = -1.0;
    for (int i = 0;i < points.size();i++) {
        double d = points[i].getDistanceTo(p);
        if (dist < 0.0 || d < dist) {
            dist = d;
            closest = i;
        }
    }

    return closest;
}

int MapWidget::getOsmZoomLevel() const
{
    return mOsmZoomLevel;
}

int MapWidget::getOsmMaxZoomLevel() const
{
    return mOsmMaxZoomLevel;
}

void MapWidget::setOsmMaxZoomLevel(int osmMaxZoomLevel)
{
    mOsmMaxZoomLevel = osmMaxZoomLevel;
    update();
}

double MapWidget::getInfoTraceTextZoom() const
{
    return mInfoTraceTextZoom;
}

void MapWidget::setInfoTraceTextZoom(double infoTraceTextZoom)
{
    mInfoTraceTextZoom = infoTraceTextZoom;
    update();
}

OsmClient *MapWidget::osmClient()
{
    return mOsm;
}

int MapWidget::getInfoTraceNum()
{
    return mInfoTraces.size();
}

int MapWidget::getInfoPointsInTrace(int trace)
{
    int res = -1;

    if (trace >= 0 && trace < mInfoTraces.size()) {
        res = mInfoTraces.at(trace).size();
    }

    return res;
}

int MapWidget::setNextEmptyOrCreateNewInfoTrace()
{
    int next = -1;

    for (int i = 0;i < mInfoTraces.size();i++) {
        if (mInfoTraces.at(i).isEmpty()) {
            next = i;
            break;
        }
    }

    if (next < 0) {
        next = mInfoTraces.size();
    }

    setInfoTraceNow(next);

    return next;
}

double MapWidget::getOsmRes() const
{
    return mOsmRes;
}

void MapWidget::setOsmRes(double osmRes)
{
    mOsmRes = osmRes;
    update();
}

bool MapWidget::getDrawOpenStreetmap() const
{
    return mDrawOpenStreetmap;
}

void MapWidget::setDrawOpenStreetmap(bool drawOpenStreetmap)
{
    mDrawOpenStreetmap = drawOpenStreetmap;
    update();
}

void MapWidget::setEnuRef(double lat, double lon, double height)
{
    mRefLat = lat;
    mRefLon = lon;
    mRefHeight = height;
    update();
}

void MapWidget::getEnuRef(double *llh)
{
    llh[0] = mRefLat;
    llh[1] = mRefLon;
    llh[2] = mRefHeight;
}

bool MapWidget::loadTrajectoryFromFile(QString filepath)
{
    //QString filepath = "/home/viktorjo/repos/chronos/server/traj/GarageplanInnerring.traj";
    //QList<QPointF> trajTemp;
    QDir dir("./traj/");

    double ref_llh[3];
    double llh[3];
    double xyz[3];

    QString line;
    QFile file(filepath);
    if(!file.open(QIODevice::ReadOnly))
    {
        qDebug() << "Could not open file: " << filepath;
    }

    QList<LocPoint> trajectory;

    QTextStream in(&file);
    while(!in.atEnd())
    {
        line = in.readLine();
        QStringList list = line.split(';');
        if(list[0].compare("LINE") == 0)
        {
            //qDebug() << "X: " << list[2] << " Y: " << list[3] << " Z: " << list[4];

            // Transform  xyz to llh, hardcoded origo
            ref_llh[0] = mRefLat; // LATb
            ref_llh[1] = mRefLon; // LONb
            ref_llh[2] = mRefHeight;

            xyz[0] = list[2].toDouble();
            xyz[1] = list[3].toDouble();
            xyz[2] = list[4].toDouble();
            //utility::enuToLlh(ref_llh,xyz,llh);
            //llh[2] = 219.0;

            // x = (LON - LONb) * (M_PI/180)*R*cos(((LAT - LATb)/2)*(M_PI/180))
            // y = (LAT - LATb) * (M_PI/180)*R

            // LAT = y / ((M_PI/180)*R) + LATb
            // LON = x / ((M_PI/180)*R*cos(((LAT - LATb)/2)*(M_PI/180))) + LONb


            llh[0] = xyz[1] / ((M_PI/180)*6378137.0) + ref_llh[0];
            llh[1] = xyz[0] / ((M_PI/180)*6378137.0*cos(((llh[0] + ref_llh[0])/2)*(M_PI/180))) + ref_llh[1];
            llh[2] = ref_llh[2] + xyz[2];

            //qDebug() << "LLH: " << QString::number(llh[0], 'g', 8) << " " << QString::number(llh[1], 'g', 8) << " " << QString::number(llh[2], 'g', 8);

            // Transform to x,y,z according to getEnuRef
            getEnuRef(ref_llh);
            utility::llhToEnu(ref_llh, llh, xyz);

            //qDebug() << "ref_llh: " << QString::number(ref_llh[0], 'g', 8) << " " << QString::number(ref_llh[1], 'g', 8) << " " << QString::number(ref_llh[2], 'g', 8);

            //qDebug() << "ENU: " << xyz[0] << " " << xyz[1] << " " << xyz[2];

            //trajTemp.append(QPointF(xyz[0],xyz[1]));
            //mInfoTrace.append(LocPoint(xyz[0],xyz[1]));
            trajectory.append(LocPoint(xyz[0],xyz[1]));
        }

    }
    file.close();
    if(trajectory.size() > 0)
    {
        mInfoTraces.append(trajectory);
        return true;
    }
    return false;

}
