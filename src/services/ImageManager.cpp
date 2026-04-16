#include "ImageManager.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUuid>
#include <QUrl>

ImageManager::ImageManager()
    : QObject(nullptr)
{
    m_imageDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/images";
    QDir().mkpath(m_imageDir);
}

ImageManager& ImageManager::instance()
{
    static ImageManager inst;
    return inst;
}

QString ImageManager::storeImage(const QString &sourcePath)
{
    QFileInfo fi(sourcePath);
    if (!fi.exists()) return QString();

    // Generate unique filename
    QString newName = QUuid::createUuid().toString(QUuid::WithoutBraces) + "." + fi.suffix();
    QString destPath = m_imageDir + "/" + newName;

    if (QFile::copy(sourcePath, destPath)) {
        return QUrl::fromLocalFile(destPath).toString();
    }
    return QString();
}

QString ImageManager::imageDirectory() const
{
    return m_imageDir;
}
