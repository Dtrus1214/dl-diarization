#include "mainwindow.h"
#include "appconfig.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    a.setApplicationName(AppConfig::productName());
    a.setApplicationVersion(AppConfig::version());
    a.setOrganizationName(AppConfig::organizationName());
    a.setOrganizationDomain(AppConfig::organizationDomain());

    MainWindow w;
    w.show();
    return a.exec();
}
