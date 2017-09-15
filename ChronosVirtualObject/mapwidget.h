/*
    Copyright 2012 - 2016 Benjamin Vedder	benjamin@vedder.se

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

#ifndef MAPWIDGET_H
#define MAPWIDGET_H

#include <QWidget>
#include <QPainter>
#include <QPaintEvent>
#include <QBrush>
#include <QFont>
#include <QPen>
#include <QPalette>
#include <QList>
#include <QInputDialog>
#include <QTimer>
#include <QUdpSocket>
#include <QWebSocket>

#include "locpoint.h"
#include "carinfo.h"
#include "perspectivepixmap.h"
#include "osmclient.h"
#include "chronos.h"

// QWidget or QGLWidget
#ifdef HAS_OPENGL
#include <QGLWidget>
#define MapWidgetType QGLWidget
#else
#define MapWidgetType QWidget
#endif

class MapWidget : public MapWidgetType
{
    Q_OBJECT

public:
    explicit MapWidget(QWidget *parent = 0);
    ~MapWidget();
    CarInfo* getCarInfo(int car);
    void setFollowCar(int car);
    void setTraceCar(int car);
    void setSelectedCar(int car);
    void addCar(CarInfo car);
    bool removeCar(int carId);
    void setScaleFactor(double scale);
    void setRotation(double rotation);
    void setXOffset(double offset);
    void setYOffset(double offset);

    /* Viktor Johansson */
    /*---------------------------------------------------*/

    /* Display Tools */
    int addInfoTrace(QList<LocPoint>);

    /* Utility Functions */
    void setRefPos(double lat, double lon, double alt);
    double getRefLat();
    double getRefLon();
    double getRefAlt();
    /*---------------------------------------------------*/

    void clearTrace();
    void addRoutePoint(double px, double py, double speed = 0.0, qint32 time = 0);
    QList<LocPoint> getRoute();
    void setRoute(QList<LocPoint> route);
    void clearRoute();
    void clearAllRoutes();
    void setRoutePointSpeed(double speed);
    void addInfoPoint(LocPoint &info);
    void clearInfoTrace();
    void clearAllInfoTraces();
    void addPerspectivePixmap(PerspectivePixmap map);
    void clearPerspectivePixmaps();
    QPoint getMousePosRelative();
    void setAntialiasDrawings(bool antialias);
    void setAntialiasOsm(bool antialias);
    bool getDrawOpenStreetmap() const;
    void setDrawOpenStreetmap(bool drawOpenStreetmap);
    void setEnuRef(double lat, double lon, double height);
    void getEnuRef(double *llh);
    double getOsmRes() const;
    void setOsmRes(double osmRes);
    double getInfoTraceTextZoom() const;
    void setInfoTraceTextZoom(double infoTraceTextZoom);
    OsmClient *osmClient();
    int getInfoTraceNum();
    int getInfoPointsInTrace(int trace);
    int setNextEmptyOrCreateNewInfoTrace();

    int getOsmMaxZoomLevel() const;
    void setOsmMaxZoomLevel(int osmMaxZoomLevel);

    int getOsmZoomLevel() const;

    bool getDrawGrid() const;
    void setDrawGrid(bool drawGrid);

    bool getDrawOsmStats() const;
    void setDrawOsmStats(bool drawOsmStats);

    int getRouteNow() const;
    void setRouteNow(int routeNow);

    qint32 getRoutePointTime() const;
    void setRoutePointTime(const qint32 &routePointTime);

    double getTraceMinSpaceCar() const;
    void setTraceMinSpaceCar(double traceMinSpaceCar);

    double getTraceMinSpaceGps() const;
    void setTraceMinSpaceGps(double traceMinSpaceGps);

    int getInfoTraceNow() const;
    void setInfoTraceNow(int infoTraceNow);

signals:
    void scaleChanged(double newScale);
    void offsetChanged(double newXOffset, double newYOffset);
    void posSet(quint8 id, LocPoint pos);
    void routePointAdded(LocPoint pos);
    void lastRoutePointRemoved(LocPoint pos);
    void infoTraceChanged(int traceNow);

    // Chronos handling
    void handle_osem(chronos_osem data);
    void handle_dopm(QVector<chronos_dopm_pt> data);


private slots:
    void tileReady(OsmTile tile);
    void errorGetTile(QString reason);
    void readPendingDatagrams();
    void onConnected();
    void onBinaryMessageReceived(const QByteArray &message);
    void onTextMessageReceived(const QString &message);
    void displayMessage(const QByteArray &message);

    // Method for OSEM
    void chronosOSEM(chronos_osem data);
    void chronosDOPM(QVector<chronos_dopm_pt> data);

protected:
    void paintEvent(QPaintEvent *event);
    void mouseMoveEvent (QMouseEvent * e);
    void mousePressEvent(QMouseEvent * e);
    void mouseReleaseEvent(QMouseEvent * e);
    void wheelEvent(QWheelEvent * e);

private:
    QList<CarInfo> mCarInfo;
    QList<LocPoint> mCarTrace;
    QList<LocPoint> mCarTraceGps;
    QList<QList<LocPoint> > mRoutes;
    QList<QList<LocPoint> > mInfoTraces;
    QList<LocPoint> mVisibleInfoTracePoints;
    QList<PerspectivePixmap> mPerspectivePixmaps;
    double mRoutePointSpeed;
    qint32 mRoutePointTime;
    double mScaleFactor;
    double mRotation;
    double mXOffset;
    double mYOffset;
    int mMouseLastX;
    int mMouseLastY;
    int mFollowCar;
    int mTraceCar;
    int mSelectedCar;
    double xRealPos;
    double yRealPos;
    bool mAntialiasDrawings;
    bool mAntialiasOsm;
    double mOsmRes;
    double mInfoTraceTextZoom;
    OsmClient *mOsm;
    int mOsmZoomLevel;
    int mOsmMaxZoomLevel;
    bool mDrawOpenStreetmap;
    bool mDrawOsmStats;
    double mRefLat;
    double mRefLon;
    double mRefHeight;
    LocPoint mClosestInfo;
    bool mDrawGrid;
    int mRoutePointSelected;
    int mRouteNow;
    int mInfoTraceNow;
    double mTraceMinSpaceCar;
    double mTraceMinSpaceGps;

    QUdpSocket *mUdpSocket;
    QWebSocket *mWebSocket;

    void updateClosestInfoPoint();
    int drawInfoPoints(QPainter &painter, const QList<LocPoint> &pts,
                        QTransform drawTrans, QTransform txtTrans,
                       double xStart, double xEnd, double yStart, double yEnd,
                       double min_dist);
    int getClosestPoint(LocPoint p, QList<LocPoint> points, double &dist);

};

#endif // MAPWIDGET_H
