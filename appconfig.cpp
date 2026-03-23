#include "appconfig.h"

namespace {

const char *const kProductName = "Diarization Studio";
const char *const kOrganizationName = "STS Audio Labs";
const char *const kOrganizationDomain = "sts-audio.local";

const char *const kVersion = "0.1.0";
const char *const kBuild = "alpha";

}

QString AppConfig::productName()
{
    return QString::fromLatin1(kProductName);
}

QString AppConfig::organizationName()
{
    return QString::fromLatin1(kOrganizationName);
}

QString AppConfig::organizationDomain()
{
    return QString::fromLatin1(kOrganizationDomain);
}

QString AppConfig::version()
{
    return QString::fromLatin1(kVersion);
}

QString AppConfig::versionLabel()
{
    return QStringLiteral("v%1").arg(version());
}

QString AppConfig::buildLabel()
{
    return QString::fromLatin1(kBuild);
}
