#include "UpdateLogManager.h"

#include "apppaths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QMutexLocker>
#include <QTextStream>

namespace {
const char *kUpdateLogFileName = "logUpdate.txt";
constexpr qint64 kMaxUpdateLogBytes = 2 * 1024 * 1024;
}

UpdateLogManager::UpdateLogManager(QObject *parent)
    : QObject(parent)
{
}

void UpdateLogManager::logUpdate(const QString &moduleName,
                                 const QString &oldVersion,
                                 const QString &newVersion)
{
    const QString module = moduleName.trimmed().isEmpty()
            ? QStringLiteral("не указан")
            : moduleName.trimmed();
    const QString oldVer = oldVersion.trimmed().isEmpty() ? QStringLiteral("—") : oldVersion.trimmed();
    const QString newVer = newVersion.trimmed().isEmpty() ? QStringLiteral("—") : newVersion.trimmed();

    QMutexLocker locker(&m_mutex);
    if (!ensureLogDir() || isLogOverSizeLimit()) {
        return;
    }

    QFile file(logFilePath());
    if (!file.open(QIODevice::Append | QIODevice::Text)) {
        return;
    }

    QTextStream out(&file);
    out << QDateTime::currentDateTime().toString(QStringLiteral("dd-MM-yyyy hh:mm:ss"))
        << QStringLiteral(" | ")
        << module
        << QStringLiteral(" | ")
        << oldVer
        << QStringLiteral(" -> ")
        << newVer
        << Qt::endl;
}

QStringList UpdateLogManager::readLogLines(int maxLines) const
{
    if (maxLines <= 0) {
        maxLines = 2000;
    }

    QMutexLocker locker(&m_mutex);
    QFile file(logFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    QStringList lines;
    QTextStream in(&file);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty()) {
            continue;
        }
        lines.append(line);
        if (lines.size() > maxLines) {
            lines.removeFirst();
        }
    }

    return lines;
}

int UpdateLogManager::lineCount() const
{
    return readLogLines(100000).size();
}

QString UpdateLogManager::logFilePath() const
{
    return logDirPath() + QLatin1Char('/') + QLatin1String(kUpdateLogFileName);
}

QString UpdateLogManager::logDirPath() const
{
    return AppPaths::instance().onyxLogDir();
}

bool UpdateLogManager::ensureLogDir() const
{
    QDir dir(logDirPath());
    return dir.exists() || dir.mkpath(QStringLiteral("."));
}

bool UpdateLogManager::isLogOverSizeLimit() const
{
    const QFileInfo info(logFilePath());
    return info.exists() && info.size() >= kMaxUpdateLogBytes;
}
