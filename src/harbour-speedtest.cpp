#ifdef QT_QML_DEBUG
#include <QtQuick>
#endif

#include "speedtest.h"

#include <QGuiApplication>
#include <QQmlContext>
#include <QQuickView>
#include <sailfishapp.h>

int main(int argc, char *argv[])
{
    QScopedPointer<QGuiApplication> app(SailfishApp::application(argc, argv));
    app->setOrganizationName(QStringLiteral("org.mrcyjanek"));
    app->setApplicationName(QStringLiteral("harbour-speedtest"));
    QScopedPointer<QQuickView> view(SailfishApp::createView());
    SpeedTest speed;
    view->rootContext()->setContextProperty(QStringLiteral("speed"), &speed);
    view->setSource(SailfishApp::pathToMainQml());
    view->show();
    return app->exec();
}
