#pragma once

#include <QObject>
#include <QString>

class ImageManager : public QObject
{
    Q_OBJECT

public:
    static ImageManager& instance();

    // Copy image to app data directory and return a file:// URL
    QString storeImage(const QString &sourcePath);

    // Get the base directory for stored images
    QString imageDirectory() const;

private:
    ImageManager();
    QString m_imageDir;
};
