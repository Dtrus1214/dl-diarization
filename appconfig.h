#ifndef APPCONFIG_H
#define APPCONFIG_H

#include <QString>

namespace AppConfig {

QString productName();
QString organizationName();
QString organizationDomain();

QString version();
QString versionLabel();
QString buildLabel();

}

#endif // APPCONFIG_H
