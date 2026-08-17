#include "capture/SessionState.h"

#include <QApplication>
#include <QClipboard>
#include <QDBusInterface>
#include <QDateTime>
#include <QDir>
#include <QFontMetrics>
#include <QPainter>
#include <QStandardPaths>
#include <cmath>

void SessionState::pushAnnotation(const Annotation &annotation) {
    annotations.append(annotation);
    redoStack.clear();
    emit changed();
}

void SessionState::undo() {
    if (annotations.isEmpty()) return;
    redoStack.append(annotations.takeLast());
    emit changed();
}

void SessionState::redo() {
    if (redoStack.isEmpty()) return;
    annotations.append(redoStack.takeLast());
    emit changed();
}

QImage SessionState::pixelate(const QImage &src, const QRect &regionInSrc, int blockSize) {
    const QRect r = regionInSrc.intersected(src.rect());
    if (r.isEmpty()) return QImage();

    const QImage cropped = src.copy(r);
    const QSize small(qMax(1, r.width() / blockSize), qMax(1, r.height() / blockSize));
    const QImage down = cropped.scaled(small, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    return down.scaled(r.size(), Qt::IgnoreAspectRatio, Qt::FastTransformation);
}

void SessionState::drawArrow(QPainter &painter, const QPoint &from, const QPoint &to, const QColor &color,
                              int thickness) {
    QPen pen(color, thickness);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.drawLine(from, to);

    const double angle = std::atan2(double(to.y() - from.y()), double(to.x() - from.x()));
    const double headLen = 8 + thickness * 2.5;
    constexpr double headAngle = 0.4488;

    const QPointF tip(to);
    const QPointF left = tip - QPointF(std::cos(angle - headAngle), std::sin(angle - headAngle)) * headLen;
    const QPointF right = tip - QPointF(std::cos(angle + headAngle), std::sin(angle + headAngle)) * headLen;

    QPolygonF head;
    head << tip << left << right;
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawPolygon(head);
}

void SessionState::finalizeAndCopy() {
    if (!selection.isValid()) {
        cancel();
        return;
    }

    const QPoint origin = selection.topLeft();
    QImage result = fullImage.copy(selection);
    QPainter p(&result);
    p.setRenderHint(QPainter::Antialiasing);

    const auto toLocal = [origin](const QPoint &pt) { return pt - origin; };

    for (const auto &a : annotations) {
        switch (a.type) {
            case AnnotationType::Stroke: {
                QPen pen(a.color, a.thickness);
                pen.setCapStyle(Qt::RoundCap);
                pen.setJoinStyle(Qt::RoundJoin);
                p.setPen(pen);
                p.setBrush(Qt::NoBrush);
                p.drawPath(a.path.translated(-origin));
                break;
            }
            case AnnotationType::Highlight: {
                QColor c = a.color;
                c.setAlpha(100);
                QPen pen(c, 16);
                pen.setCapStyle(Qt::RoundCap);
                pen.setJoinStyle(Qt::RoundJoin);
                p.setPen(pen);
                p.setBrush(Qt::NoBrush);
                p.drawPath(a.path.translated(-origin));
                break;
            }
            case AnnotationType::Arrow:
                drawArrow(p, toLocal(a.start), toLocal(a.end), a.color, a.thickness);
                break;
            case AnnotationType::Rectangle: {
                QPen pen(a.color, a.thickness);
                p.setPen(pen);
                p.setBrush(Qt::NoBrush);
                p.drawRect(QRect(toLocal(a.start), toLocal(a.end)).normalized());
                break;
            }
            case AnnotationType::Blur: {
                const QRect localRect = QRect(toLocal(a.start), toLocal(a.end)).normalized();
                const QImage patch = pixelate(result, localRect);
                if (!patch.isNull()) p.drawImage(localRect.topLeft(), patch);
                break;
            }
            case AnnotationType::Text: {
                const QPoint pos = toLocal(a.textPos);
                QFontMetrics fm(a.font);
                if (a.background.alpha() > 0) {
                    QRect box = fm.boundingRect(a.text).translated(pos);
                    box.adjust(-4, -2, 4, 2);
                    p.fillRect(box, a.background);
                }
                p.setPen(a.color);
                p.setFont(a.font);
                p.drawText(pos, a.text);
                break;
            }
        }
    }
    p.end();

    QApplication::clipboard()->setImage(result);

    QString savedPath;
    if (!originalPath.isEmpty()) {
        if (result.save(originalPath)) savedPath = originalPath;
    } else if (!saveDirectory.isEmpty()) {
        QDir dir(saveDirectory);
        if (!dir.exists()) dir.mkpath(".");
        const QString name = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss") + ".png";
        const QString path = dir.filePath(name);
        if (result.save(path)) savedPath = path;
    }

    notify(savedPath);

    if (!savedPath.isEmpty()) emit savedTo(savedPath);
    emit ended(true);
}

void SessionState::notify(const QString &savedPath) const {
    QDBusInterface notifications("org.freedesktop.Notifications", "/org/freedesktop/Notifications",
                                  "org.freedesktop.Notifications", QDBusConnection::sessionBus());
    if (!notifications.isValid()) return;

    const QString body =
        savedPath.isEmpty() ? "Copied to clipboard" : QString("Copied to clipboard, saved to %1").arg(savedPath);

    notifications.call("Notify", "ShareXL", quint32(0), "camera-photo-symbolic", "ShareXL", body, QStringList(),
                        QVariantMap(), 4000);
}

void SessionState::cancel() { emit ended(false); }
