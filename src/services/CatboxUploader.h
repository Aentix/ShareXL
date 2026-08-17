#pragma once

#include <QObject>
#include <QString>

class QNetworkAccessManager;

class CatboxUploader : public QObject {
    Q_OBJECT
public:
    explicit CatboxUploader(QObject *parent = nullptr);
    void upload(const QString &filePath);

signals:
    void uploaded(const QString &url);
    void failed(const QString &reason);

private:
    QNetworkAccessManager *manager;
};
