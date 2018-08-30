QT += core websockets
QT -= gui

CONFIG += c++11

LIBS += ../server/build/libutil.a
LIBS += -lrt

TARGET = websocket_visualization
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += main.cpp \
    visualizationserver.cpp \
    generator.cpp \
    tcpserversimple.cpp \
    nmea2etsi.cpp \
    tcpclientsimple.cpp


HEADERS += \
    visualizationserver.h \
    generator.h \
    ../server/inc/util.h \
    tcpserversimple.h \
    nmea2etsi.h \
    tcpclientsimple.h

DISTFILES +=
