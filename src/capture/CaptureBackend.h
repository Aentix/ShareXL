#pragma once

#include <QObject>
#include <QImage>

class CaptureBackend : public QObject {
    Q_OBJECT
public:
    explicit CaptureBackend(QObject *parent = nullptr);

    void requestScreenshot();

signals:
    void captured(const QImage &image);
    void failed(const QString &reason);

private slots:
    void onResponse(uint code, const QVariantMap &results);

private:
    void captureX11();
};
