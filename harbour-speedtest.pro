TARGET = harbour-speedtest

CONFIG += sailfishapp c++11
QT += network

HEADERS += src/speedtest.h
SOURCES += src/harbour-speedtest.cpp \
    src/speedtest.cpp

DISTFILES += qml/harbour-speedtest.qml \
    qml/cover/CoverPage.qml \
    qml/pages/FirstPage.qml \
    qml/pages/AboutPage.qml \
    rpm/harbour-speedtest.spec \
    harbour-speedtest.desktop

SAILFISHAPP_ICONS = 86x86 108x108 128x128 172x172
