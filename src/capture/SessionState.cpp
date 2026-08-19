#include "capture/SessionState.h"

#include <QApplication>
#include <QClipboard>
#include <QDBusInterface>
#include <QDateTime>
#include <QDir>
#include <QFontMetrics>
#include <QPainter>
#include <QStandardPaths>
#include <QTransform>
#include <cmath>

void SessionState::pushAnnotation(const Annotation &annotation) {
    annotations.append(annotation);
    redoStack.clear();
    emit changed();
}

void SessionState::undo() {
    if (annotations.isEmpty()) {
        if (!hasCanvasUndo) return;
        fullImage = canvasUndoImage;
        annotations = canvasUndoAnnotations;
        hasCanvasUndo = false;
        selectedAnnotationIndex = -1;
        emit changed();
        return;
    }
    const Annotation a = annotations.takeLast();
    if (a.type == AnnotationType::Step) nextStepNumber--;
    redoStack.append(a);
    if (selectedAnnotationIndex >= annotations.size()) selectedAnnotationIndex = -1;
    emit changed();
}

void SessionState::redo() {
    if (redoStack.isEmpty()) return;
    const Annotation a = redoStack.takeLast();
    if (a.type == AnnotationType::Step) nextStepNumber++;
    annotations.append(a);
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
    drawArrowHead(painter, QPointF(to), angle, color, thickness);
}

void SessionState::drawArrowHead(QPainter &painter, const QPointF &tip, double angle, const QColor &color,
                                  int thickness) {
    const double headLen = 8 + thickness * 2.5;
    constexpr double headAngle = 0.4488;

    const QPointF left = tip - QPointF(std::cos(angle - headAngle), std::sin(angle - headAngle)) * headLen;
    const QPointF right = tip - QPointF(std::cos(angle + headAngle), std::sin(angle + headAngle)) * headLen;

    QPolygonF head;
    head << tip << left << right;
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawPolygon(head);
}

double SessionState::pathEndAngle(const QPainterPath &path) {
    const int count = path.elementCount();
    if (count < 2) return 0.0;
    const QPainterPath::Element last = path.elementAt(count - 1);
    const QPainterPath::Element prev = path.elementAt(count - 2);
    return std::atan2(last.y - prev.y, last.x - prev.x);
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
            case AnnotationType::Ellipse: {
                QPen pen(a.color, a.thickness);
                p.setPen(pen);
                p.setBrush(Qt::NoBrush);
                p.drawEllipse(QRect(toLocal(a.start), toLocal(a.end)).normalized());
                break;
            }
            case AnnotationType::Line: {
                QPen pen(a.color, a.thickness);
                pen.setCapStyle(Qt::RoundCap);
                p.setPen(pen);
                p.drawLine(toLocal(a.start), toLocal(a.end));
                break;
            }
            case AnnotationType::FreehandArrow: {
                QPen pen(a.color, a.thickness);
                pen.setCapStyle(Qt::RoundCap);
                pen.setJoinStyle(Qt::RoundJoin);
                p.setPen(pen);
                p.setBrush(Qt::NoBrush);
                p.drawPath(a.path.translated(-origin));
                drawArrowHead(p, QPointF(toLocal(a.path.currentPosition().toPoint())), pathEndAngle(a.path), a.color,
                              a.thickness);
                break;
            }
            case AnnotationType::Step: {
                const QPoint center = toLocal(a.textPos);
                constexpr int radius = 14;
                p.setPen(Qt::NoPen);
                p.setBrush(a.color);
                p.drawEllipse(center, radius, radius);
                QFont font = a.font;
                font.setBold(true);
                font.setPointSize(radius);
                p.setFont(font);
                p.setPen(Qt::white);
                p.drawText(QRect(center.x() - radius, center.y() - radius, radius * 2, radius * 2), Qt::AlignCenter,
                           QString::number(a.stepNumber));
                break;
            }
            case AnnotationType::SpeechBalloon: {
                const QPoint pos = toLocal(a.textPos);
                QFontMetrics fm(a.font);
                QRect box = fm.boundingRect(a.text).translated(pos);
                box.adjust(-8, -6, 8, 6);

                QPainterPath balloon;
                balloon.addRoundedRect(box, 10, 10);
                QPainterPath tail;
                tail.moveTo(box.left() + 16, box.bottom());
                tail.lineTo(box.left() + 6, box.bottom() + 14);
                tail.lineTo(box.left() + 32, box.bottom());
                tail.closeSubpath();
                balloon = balloon.united(tail);

                const QColor fill = a.background.alpha() > 0 ? a.background : QColor(255, 255, 255, 235);
                p.setPen(QPen(a.color, 2));
                p.setBrush(fill);
                p.drawPath(balloon);

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

void SessionState::pickColorAt(const QPoint &globalPos) {
    if (!fullImage.rect().contains(globalPos)) {
        cancel();
        return;
    }

    const QString hex = fullImage.pixelColor(globalPos).name(QColor::HexRgb);
    QApplication::clipboard()->setText(hex);

    QDBusInterface notifications("org.freedesktop.Notifications", "/org/freedesktop/Notifications",
                                  "org.freedesktop.Notifications", QDBusConnection::sessionBus());
    if (notifications.isValid()) {
        notifications.call("Notify", "ShareXL", quint32(0), "camera-photo-symbolic", "ShareXL",
                            QString("Copied color %1 to clipboard").arg(hex), QStringList(), QVariantMap(), 4000);
    }

    emit ended(false);
}

void SessionState::cropToSelection() {
    const QRect crop = selection.intersected(fullImage.rect());
    if (crop.isEmpty()) return;

    canvasUndoImage = fullImage;
    canvasUndoAnnotations = annotations;
    hasCanvasUndo = true;

    fullImage = fullImage.copy(crop);
    const QPoint origin = crop.topLeft();
    for (auto &a : annotations) {
        a.start -= origin;
        a.end -= origin;
        a.textPos -= origin;
        a.path.translate(-origin);
    }

    selection = QRect(QPoint(0, 0), fullImage.size());
    selectedAnnotationIndex = -1;
    emit changed();
}

void SessionState::rotate90(bool clockwise) {
    canvasUndoImage = fullImage;
    canvasUndoAnnotations = annotations;
    hasCanvasUndo = true;

    const QSize oldSize = fullImage.size();
    QTransform t;
    t.rotate(clockwise ? 90 : -90);
    fullImage = fullImage.transformed(t, Qt::SmoothTransformation);

    const auto rotatePoint = [&](const QPoint &p) {
        return clockwise ? QPoint(oldSize.height() - p.y(), p.x()) : QPoint(p.y(), oldSize.width() - p.x());
    };

    for (auto &a : annotations) {
        a.start = rotatePoint(a.start);
        a.end = rotatePoint(a.end);
        a.textPos = rotatePoint(a.textPos);

        QPainterPath newPath;
        for (int i = 0; i < a.path.elementCount(); ++i) {
            const QPainterPath::Element el = a.path.elementAt(i);
            const QPoint np = rotatePoint(QPoint(qRound(el.x), qRound(el.y)));
            if (el.type == QPainterPath::MoveToElement) {
                newPath.moveTo(np);
            } else {
                newPath.lineTo(np);
            }
        }
        a.path = newPath;
    }

    selection = QRect(QPoint(0, 0), fullImage.size());
    selectedAnnotationIndex = -1;
    emit changed();
}

void SessionState::flip(bool horizontal) {
    canvasUndoImage = fullImage;
    canvasUndoAnnotations = annotations;
    hasCanvasUndo = true;

    const QSize size = fullImage.size();
    fullImage = fullImage.flipped(horizontal ? Qt::Horizontal : Qt::Vertical);

    const auto flipPoint = [&](const QPoint &p) {
        return horizontal ? QPoint(size.width() - p.x(), p.y()) : QPoint(p.x(), size.height() - p.y());
    };

    for (auto &a : annotations) {
        a.start = flipPoint(a.start);
        a.end = flipPoint(a.end);
        a.textPos = flipPoint(a.textPos);

        QPainterPath newPath;
        for (int i = 0; i < a.path.elementCount(); ++i) {
            const QPainterPath::Element el = a.path.elementAt(i);
            const QPoint np = flipPoint(QPoint(qRound(el.x), qRound(el.y)));
            if (el.type == QPainterPath::MoveToElement) {
                newPath.moveTo(np);
            } else {
                newPath.lineTo(np);
            }
        }
        a.path = newPath;
    }

    selectedAnnotationIndex = -1;
    emit changed();
}
