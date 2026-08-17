#include "services/CatboxUploader.h"

#include <QFile>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>

CatboxUploader::CatboxUploader(QObject *parent) : QObject(parent), manager(new QNetworkAccessManager(this)) {}

void CatboxUploader::upload(const QString &filePath) {
    auto *file = new QFile(filePath);
    if (!file->open(QIODevice::ReadOnly)) {
        emit failed("Could not open file");
        delete file;
        return;
    }

    auto *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart reqTypePart;
    reqTypePart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"reqtype\""));
    reqTypePart.setBody("fileupload");
    multiPart->append(reqTypePart);

    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                        QVariant(QString("form-data; name=\"fileToUpload\"; filename=\"%1\"")
                                     .arg(QFileInfo(filePath).fileName())));
    filePart.setBodyDevice(file);
    file->setParent(multiPart);
    multiPart->append(filePart);

    QNetworkRequest request(QUrl("https://catbox.moe/user/api.php"));
    QNetworkReply *reply = manager->post(request, multiPart);
    multiPart->setParent(reply);

    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit failed(reply->errorString());
            return;
        }
        const QString url = QString::fromUtf8(reply->readAll()).trimmed();
        if (url.startsWith("https://")) {
            emit uploaded(url);
        } else {
            emit failed(url.isEmpty() ? "Empty response from Catbox" : url);
        }
    });
}
