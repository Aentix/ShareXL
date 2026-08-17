#include "ui/HistoryDialog.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScrollBar>
#include <QUrl>
#include <QVBoxLayout>

#include "capture/OverlayWindow.h"
#include "capture/SessionState.h"
#include "services/CatboxUploader.h"
#include "services/Config.h"

namespace {

OverlayWindow *openEditor(const QString &path) {
    const QImage image(path);
    if (image.isNull()) return nullptr;

    const Config config = Config::load();

    auto *session = new SessionState();
    session->fullImage = image;
    session->selection = QRect(QPoint(0, 0), image.size());
    session->originalPath = path;
    session->drawColor = config.drawColor;
    session->drawThickness = config.drawThickness;
    session->textFont = QFont(config.textFontFamily, config.textFontSize, QFont::Bold);
    session->textBackground = config.textBackground;

    auto *editor = new OverlayWindow(session, QPoint(0, 0), true, true);
    session->setParent(editor);
    editor->show();
    return editor;
}

void showItemActions(const QString &path, QWidget *parent, const std::function<void()> &reopenAfterEdit) {
    QDialog dialog(parent);
    dialog.setWindowTitle(QFileInfo(path).fileName());

    auto *layout = new QVBoxLayout(&dialog);

    auto *preview = new QLabel(&dialog);
    const QPixmap pixmap(path);
    preview->setPixmap(pixmap.scaled(420, 420, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    preview->setAlignment(Qt::AlignCenter);
    layout->addWidget(preview);

    auto *status = new QLabel(&dialog);
    status->setWordWrap(true);
    layout->addWidget(status);

    auto *buttonsRow = new QHBoxLayout();
    auto *editButton = new QPushButton("Edit", &dialog);
    auto *folderButton = new QPushButton("Open Directory", &dialog);
    auto *copyButton = new QPushButton("Copy", &dialog);
    auto *uploadButton = new QPushButton("Upload to Catbox", &dialog);
    buttonsRow->addWidget(editButton);
    buttonsRow->addWidget(folderButton);
    buttonsRow->addWidget(copyButton);
    buttonsRow->addWidget(uploadButton);
    layout->addLayout(buttonsRow);

    QObject::connect(editButton, &QPushButton::clicked, &dialog, [path, &dialog, parent, reopenAfterEdit] {
        OverlayWindow *editor = openEditor(path);
        dialog.close();
        if (parent) parent->close();
        if (editor && reopenAfterEdit) {
            QObject::connect(editor, &QObject::destroyed, qApp, [reopenAfterEdit] { reopenAfterEdit(); });
        }
    });

    QObject::connect(folderButton, &QPushButton::clicked, &dialog, [path] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
    });

    QObject::connect(copyButton, &QPushButton::clicked, &dialog, [path, status] {
        const QImage image(path);
        if (!image.isNull()) {
            QApplication::clipboard()->setImage(image);
            status->setText("Copied to clipboard.");
        }
    });

    QObject::connect(uploadButton, &QPushButton::clicked, &dialog, [&dialog, path, status, uploadButton] {
        uploadButton->setEnabled(false);
        status->setText("Uploading...");

        auto *uploader = new CatboxUploader();
        QObject::connect(uploader, &CatboxUploader::uploaded, &dialog,
                          [status, uploadButton, uploader](const QString &url) {
                              QApplication::clipboard()->setText(url);
                              status->setText("Uploaded, link copied to clipboard: " + url);
                              uploadButton->setEnabled(true);
                              uploader->deleteLater();
                          });
        QObject::connect(uploader, &CatboxUploader::failed, &dialog,
                          [status, uploadButton, uploader](const QString &reason) {
                              status->setText("Upload failed: " + reason);
                              uploadButton->setEnabled(true);
                              uploader->deleteLater();
                          });
        uploader->upload(path);
    });

    dialog.exec();
}

}

HistoryDialog::HistoryDialog(const QStringList &paths, std::function<void()> reopenAfterEdit, QWidget *parent)
    : QDialog(parent) {
    setWindowTitle("Recent Captures");
    resize(780, 640);

    list = new QListWidget(this);
    list->setViewMode(QListWidget::IconMode);
    list->setMovement(QListWidget::Static);
    list->setSpacing(8);
    list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    list->setUniformItemSizes(true);

    if (paths.isEmpty()) {
        auto *item = new QListWidgetItem("No captures yet");
        item->setFlags(Qt::NoItemFlags);
        list->addItem(item);
    } else {
        for (const QString &path : paths) {
            auto *item = new QListWidgetItem(QIcon(path), QString());
            item->setData(Qt::UserRole, path);
            item->setToolTip(QFileInfo(path).fileName());
            list->addItem(item);
        }
    }

    connect(list, &QListWidget::itemClicked, this, [this, reopenAfterEdit](QListWidgetItem *item) {
        const QString path = item->data(Qt::UserRole).toString();
        if (path.isEmpty()) return;
        showItemActions(path, this, reopenAfterEdit);
    });

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(list);
}

void HistoryDialog::resizeEvent(QResizeEvent *event) {
    QDialog::resizeEvent(event);
    updateGridSize();
}

void HistoryDialog::showEvent(QShowEvent *event) {
    QDialog::showEvent(event);
    updateGridSize();
}

void HistoryDialog::updateGridSize() {
    const int scrollbarWidth = list->verticalScrollBar()->sizeHint().width();
    const int available = list->viewport()->width() > 0 ? list->viewport()->width()
                                                          : list->width() - scrollbarWidth;
    const int spacing = list->spacing();
    const int cell = qMax(60, (available - spacing * 4) / 3);

    list->setIconSize(QSize(cell, cell));
    list->setGridSize(QSize(cell + spacing, cell + spacing));
}
