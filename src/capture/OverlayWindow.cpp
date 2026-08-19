#include "capture/OverlayWindow.h"

#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QFontComboBox>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QSvgRenderer>
#include <QVBoxLayout>

namespace {
const QColor kIconColor(0xD8, 0xDA, 0xDB);
const QSize kMinEditorSize(860, 520);

QIcon tintedIcon(const QString &svgPath, const QColor &color, int size) {
    QSvgRenderer renderer(svgPath);
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    renderer.render(&painter);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(pixmap.rect(), color);
    painter.end();
    return QIcon(pixmap);
}

QPushButton *makeToolButton(QWidget *parent, const QString &iconPath, const QString &tooltip, bool checkable,
                             int buttonSize, int iconSize) {
    auto *button = new QPushButton(parent);
    button->setIcon(tintedIcon(iconPath, kIconColor, iconSize));
    button->setIconSize(QSize(iconSize, iconSize));
    button->setToolTip(tooltip);
    button->setCheckable(checkable);
    button->setCursor(Qt::PointingHandCursor);
    button->setFixedSize(buttonSize, buttonSize);
    return button;
}

QFrame *makeSeparator(QWidget *parent) {
    auto *line = new QFrame(parent);
    line->setFixedWidth(1);
    line->setStyleSheet("background-color: #333333;");
    return line;
}

QWidget *makeGrooveSeparator(QWidget *parent) {
    auto *groove = new QWidget(parent);
    groove->setFixedWidth(2);
    auto *layout = new QHBoxLayout(groove);
    layout->setContentsMargins(0, 2, 0, 2);
    layout->setSpacing(0);
    auto *dark = new QFrame(groove);
    dark->setFixedWidth(1);
    dark->setStyleSheet("background-color: #1f1f1f;");
    auto *light = new QFrame(groove);
    light->setFixedWidth(1);
    light->setStyleSheet("background-color: #2c2c2c;");
    layout->addWidget(dark);
    layout->addWidget(light);
    return groove;
}
}

OverlayWindow::OverlayWindow(SessionState *session, const QPoint &globalOffset, bool showToolbar,
                             bool standalone, bool classicToolbar, QWidget *parent)
    : QWidget(parent), session(session), globalOffset(globalOffset), standalone(standalone),
      classicToolbar(classicToolbar) {
    if (standalone) {
        setWindowFlags(Qt::Window);
        setWindowTitle("ShareXL Editor");
    } else {
        setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    }
    setAttribute(Qt::WA_DeleteOnClose);
    setCursor(Qt::CrossCursor);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    connect(this->session, &SessionState::changed, this, [this] {
        const bool interactiveMove = this->session->drawingStroke || this->session->draggingShape ||
                                      this->session->selecting || this->session->movingAnnotation ||
                                      this->session->resizingAnnotation;
        if (!interactiveMove || hasMouse) update();
        if (toolbar && this->session->tool != syncedTool) {
            syncedTool = this->session->tool;
            updateToolControlsVisibility();
        }
    });

    if (showToolbar) buildToolbar();

    if (standalone) {
        const QSize minSize(qMax(kMinEditorSize.width(), toolbar ? toolbar->width() + 40 : 0), kMinEditorSize.height());
        setMinimumSize(minSize);
        resizeToFitImage();
    }
}

void OverlayWindow::drawAnnotation(QPainter &p, const SessionState::Annotation &a) {
    switch (a.type) {
        case SessionState::AnnotationType::Stroke: {
            QPen pen(a.color, a.thickness);
            pen.setCapStyle(Qt::RoundCap);
            pen.setJoinStyle(Qt::RoundJoin);
            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
            p.drawPath(a.path.translated(QPointF(-globalOffset)));
            break;
        }
        case SessionState::AnnotationType::Highlight: {
            QColor c = a.color;
            c.setAlpha(100);
            QPen pen(c, 16);
            pen.setCapStyle(Qt::RoundCap);
            pen.setJoinStyle(Qt::RoundJoin);
            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
            p.drawPath(a.path.translated(QPointF(-globalOffset)));
            break;
        }
        case SessionState::AnnotationType::Arrow:
            SessionState::drawArrow(p, a.start - globalOffset, a.end - globalOffset, a.color, a.thickness);
            break;
        case SessionState::AnnotationType::Rectangle: {
            QPen pen(a.color, a.thickness);
            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
            p.drawRect(QRect(a.start - globalOffset, a.end - globalOffset).normalized());
            break;
        }
        case SessionState::AnnotationType::Blur: {
            const QRect localRect = QRect(a.start, a.end).normalized().translated(-globalOffset);
            const QImage patch = SessionState::pixelate(session->fullImage, QRect(a.start, a.end).normalized());
            if (!patch.isNull()) p.drawImage(localRect.topLeft(), patch);
            break;
        }
        case SessionState::AnnotationType::Text: {
            const QPoint localPos = a.textPos - globalOffset;
            QFontMetrics fm(a.font);
            if (a.background.alpha() > 0) {
                QRect box = fm.boundingRect(a.text).translated(localPos);
                box.adjust(-4, -2, 4, 2);
                p.fillRect(box, a.background);
            }
            p.setPen(a.color);
            p.setFont(a.font);
            p.drawText(localPos, a.text);
            break;
        }
        case SessionState::AnnotationType::Ellipse: {
            QPen pen(a.color, a.thickness);
            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(QRect(a.start - globalOffset, a.end - globalOffset).normalized());
            break;
        }
        case SessionState::AnnotationType::Line: {
            QPen pen(a.color, a.thickness);
            pen.setCapStyle(Qt::RoundCap);
            p.setPen(pen);
            p.drawLine(a.start - globalOffset, a.end - globalOffset);
            break;
        }
        case SessionState::AnnotationType::FreehandArrow: {
            QPen pen(a.color, a.thickness);
            pen.setCapStyle(Qt::RoundCap);
            pen.setJoinStyle(Qt::RoundJoin);
            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
            p.drawPath(a.path.translated(QPointF(-globalOffset)));
            SessionState::drawArrowHead(p, QPointF(a.path.currentPosition()) - QPointF(globalOffset),
                                         SessionState::pathEndAngle(a.path), a.color, a.thickness);
            break;
        }
        case SessionState::AnnotationType::Step: {
            const QPoint center = a.textPos - globalOffset;
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
        case SessionState::AnnotationType::SpeechBalloon: {
            const QPoint pos = a.textPos - globalOffset;
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

void OverlayWindow::paintEvent(QPaintEvent *) {
    QPainter p(this);
    if (standalone) {
        p.fillRect(rect(), QColor(30, 30, 30));
        p.drawImage(-globalOffset, session->fullImage);
    } else {
        p.drawImage(rect(), session->fullImage, QRect(globalOffset, size()));
    }

    const bool showSelectionOutline =
        session->selecting || (standalone && session->tool == SessionState::Tool::Crop);
    const QRect localSelection = session->selection.translated(-globalOffset);
    if (showSelectionOutline && session->selection.isValid()) {
        QRegion outside = QRegion(rect()).subtracted(QRegion(localSelection));
        p.save();
        p.setClipRegion(outside);
        p.fillRect(rect(), QColor(0, 0, 0, 130));
        p.restore();

        p.setPen(QPen(QColor(66, 133, 244), 2));
        p.setBrush(Qt::NoBrush);
        p.drawRect(localSelection.adjusted(0, 0, -1, -1));
    } else {
        p.fillRect(rect(), QColor(0, 0, 0, 60));
    }

    p.setRenderHint(QPainter::Antialiasing);

    syncCommittedLayer();
    p.drawPixmap(0, 0, committedLayer);

    if (session->drawingStroke && !strokeLayer.isNull()) {
        p.drawPixmap(0, 0, strokeLayer);
    }

    if (session->selectedAnnotationIndex >= 0 && session->selectedAnnotationIndex < session->annotations.size()) {
        const SessionState::Annotation &selected = session->annotations[session->selectedAnnotationIndex];
        drawAnnotation(p, selected);
        drawSelectionHandles(p, selected);
    }

    if (session->draggingShape) {
        SessionState::Annotation preview;
        preview.start = session->shapeStart;
        preview.end = session->shapeCurrent;
        preview.color = session->drawColor;
        preview.thickness = session->drawThickness;

        if (session->tool == SessionState::Tool::Blur) {
            preview.type = SessionState::AnnotationType::Blur;
            drawAnnotation(p, preview);
            p.setPen(QPen(Qt::white, 1, Qt::DashLine));
            p.setBrush(Qt::NoBrush);
            p.drawRect(QRect(preview.start, preview.end).normalized().translated(-globalOffset));
        } else {
            switch (session->tool) {
                case SessionState::Tool::Arrow: preview.type = SessionState::AnnotationType::Arrow; break;
                case SessionState::Tool::Ellipse: preview.type = SessionState::AnnotationType::Ellipse; break;
                case SessionState::Tool::Line: preview.type = SessionState::AnnotationType::Line; break;
                default: preview.type = SessionState::AnnotationType::Rectangle; break;
            }
            drawAnnotation(p, preview);
        }
    }

    if (!standalone && hasMouse && session->tool == SessionState::Tool::Select) drawMagnifier(p);
}

void OverlayWindow::drawMagnifier(QPainter &p) {
    constexpr int kDiameter = 110;
    constexpr int kZoom = 4;
    constexpr int kSourceSize = kDiameter / kZoom;
    constexpr int kCursorOffset = 10;
    constexpr int kGap = 10;

    const QPoint globalCenter = toGlobal(lastMousePos);
    const QRect sourceRect(globalCenter.x() - kSourceSize / 2, globalCenter.y() - kSourceSize / 2, kSourceSize,
                            kSourceSize);

    const QString label = session->selecting
                               ? QString("%1 x %2").arg(session->selection.width()).arg(session->selection.height())
                               : QString("%1, %2").arg(globalCenter.x()).arg(globalCenter.y());

    QFont labelFont = p.font();
    labelFont.setPointSize(9);
    QFontMetrics fm(labelFont);
    const QRect labelSizeRect = fm.boundingRect(label).adjusted(-8, -4, 8, 4);

    const int totalWidth = qMax(kDiameter, labelSizeRect.width());
    const int totalHeight = kDiameter + kGap + labelSizeRect.height();

    int x = lastMousePos.x() + kCursorOffset;
    if (x + totalWidth > width()) x = lastMousePos.x() - kCursorOffset - totalWidth;
    int y = lastMousePos.y() + kCursorOffset;
    if (y + totalHeight > height()) y = lastMousePos.y() - kCursorOffset - totalHeight;

    const QRect loupeRect(x + (totalWidth - kDiameter) / 2, y, kDiameter, kDiameter);

    p.save();
    QPainterPath clipPath;
    clipPath.addEllipse(loupeRect);
    p.setClipPath(clipPath);
    p.fillRect(loupeRect, QColor(0x22, 0x22, 0x22));
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    p.drawImage(loupeRect, session->fullImage, sourceRect);
    p.restore();

    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor(0x1f, 0x1f, 0x1f), 2));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(loupeRect.adjusted(1, 1, -1, -1));

    const QPoint center = loupeRect.center();
    p.setPen(QPen(QColor(0x3e, 0x83, 0xf2), 1));
    p.drawLine(center - QPoint(8, 0), center + QPoint(8, 0));
    p.drawLine(center - QPoint(0, 8), center + QPoint(0, 8));

    QRect labelRect(0, 0, labelSizeRect.width(), labelSizeRect.height());
    labelRect.moveTopLeft(QPoint(x + (totalWidth - labelSizeRect.width()) / 2, loupeRect.bottom() + kGap));

    p.setFont(labelFont);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x22, 0x22, 0x22, 235));
    p.drawRoundedRect(labelRect, 4, 4);
    p.setPen(QColor(0xd8, 0xda, 0xdb));
    p.drawText(labelRect, Qt::AlignCenter, label);
}

void OverlayWindow::appendStrokeSegment(const QPoint &newLocalPoint) {
    if (strokeLayer.isNull() || strokeLayer.size() != size()) {
        strokeLayer = QPixmap(size());
        strokeLayer.fill(Qt::transparent);
        strokeLastPoint = newLocalPoint;
        return;
    }

    QPainter sp(&strokeLayer);
    sp.setRenderHint(QPainter::Antialiasing);
    if (session->tool == SessionState::Tool::Highlighter) {
        QColor c = session->drawColor;
        c.setAlpha(100);
        QPen pen(c, 16);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        sp.setPen(pen);
    } else {
        QPen pen(session->drawColor, session->drawThickness);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        sp.setPen(pen);
    }
    sp.drawLine(strokeLastPoint, newLocalPoint);
    strokeLastPoint = newLocalPoint;
}

void OverlayWindow::syncCommittedLayer() {
    const int excluded = session->selectedAnnotationIndex;

    if (committedLayer.isNull() || committedLayer.size() != size() || committedLayerExcluded != excluded) {
        committedLayer = QPixmap(size());
        committedLayer.fill(Qt::transparent);
        committedLayerCount = 0;
        committedLayerExcluded = excluded;
    }

    if (session->annotations.size() < committedLayerCount) {
        committedLayer.fill(Qt::transparent);
        committedLayerCount = 0;
    }

    if (session->annotations.size() > committedLayerCount) {
        QPainter cp(&committedLayer);
        cp.setRenderHint(QPainter::Antialiasing);
        for (int i = committedLayerCount; i < session->annotations.size(); ++i) {
            if (i == excluded) continue;
            drawAnnotation(cp, session->annotations[i]);
        }
        committedLayerCount = session->annotations.size();
    }
}

QRect OverlayWindow::annotationBounds(const SessionState::Annotation &a) const {
    switch (a.type) {
        case SessionState::AnnotationType::Rectangle:
        case SessionState::AnnotationType::Ellipse:
        case SessionState::AnnotationType::Blur:
        case SessionState::AnnotationType::Line:
        case SessionState::AnnotationType::Arrow:
            return QRect(a.start, a.end).normalized();
        case SessionState::AnnotationType::Stroke:
        case SessionState::AnnotationType::Highlight:
        case SessionState::AnnotationType::FreehandArrow:
            return a.path.boundingRect().toRect();
        case SessionState::AnnotationType::Text: {
            QFontMetrics fm(a.font);
            return fm.boundingRect(a.text).translated(a.textPos).adjusted(-4, -2, 4, 2);
        }
        case SessionState::AnnotationType::SpeechBalloon: {
            QFontMetrics fm(a.font);
            return fm.boundingRect(a.text).translated(a.textPos).adjusted(-8, -6, 8, 20);
        }
        case SessionState::AnnotationType::Step: {
            constexpr int radius = 14;
            return QRect(a.textPos.x() - radius, a.textPos.y() - radius, radius * 2, radius * 2);
        }
    }
    return QRect();
}

bool OverlayWindow::hasResizeHandles(const SessionState::Annotation &a) const {
    switch (a.type) {
        case SessionState::AnnotationType::Rectangle:
        case SessionState::AnnotationType::Ellipse:
        case SessionState::AnnotationType::Blur:
        case SessionState::AnnotationType::Line:
        case SessionState::AnnotationType::Arrow:
            return true;
        default:
            return false;
    }
}

int OverlayWindow::hitTestHandle(const SessionState::Annotation &a, const QPoint &global) const {
    if (!hasResizeHandles(a)) return -1;
    constexpr int tolerance = 10;
    if ((global - a.start).manhattanLength() <= tolerance) return 0;
    if ((global - a.end).manhattanLength() <= tolerance) return 1;
    return -1;
}

int OverlayWindow::hitTestAnnotation(const QPoint &global) const {
    constexpr int tolerance = 6;
    for (int i = session->annotations.size() - 1; i >= 0; --i) {
        const QRect bounds =
            annotationBounds(session->annotations[i]).adjusted(-tolerance, -tolerance, tolerance, tolerance);
        if (bounds.contains(global)) return i;
    }
    return -1;
}

void OverlayWindow::translateAnnotation(SessionState::Annotation &a, const QPoint &delta) const {
    switch (a.type) {
        case SessionState::AnnotationType::Stroke:
        case SessionState::AnnotationType::Highlight:
        case SessionState::AnnotationType::FreehandArrow:
            a.path.translate(delta);
            break;
        case SessionState::AnnotationType::Text:
        case SessionState::AnnotationType::SpeechBalloon:
        case SessionState::AnnotationType::Step:
            a.textPos += delta;
            break;
        default:
            a.start += delta;
            a.end += delta;
            break;
    }
}

void OverlayWindow::drawSelectionHandles(QPainter &p, const SessionState::Annotation &a) {
    p.setRenderHint(QPainter::Antialiasing);
    if (hasResizeHandles(a)) {
        constexpr int handleSize = 8;
        for (const QPoint &pt : {a.start, a.end}) {
            const QPoint local = pt - globalOffset;
            const QRect handle(local.x() - handleSize / 2, local.y() - handleSize / 2, handleSize, handleSize);
            p.setPen(QPen(Qt::white, 1));
            p.setBrush(QColor(0x3e, 0x83, 0xf2));
            p.drawRect(handle);
        }
    } else {
        const QRect bounds = annotationBounds(a).translated(-globalOffset);
        p.setPen(QPen(QColor(0x3e, 0x83, 0xf2), 1, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawRect(bounds);
    }
}

void OverlayWindow::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) return;
    if (textEdit) commitPendingText();
    const QPoint local = event->pos();
    const QPoint global = toGlobal(local);

    switch (session->tool) {
        case SessionState::Tool::Pen:
        case SessionState::Tool::Highlighter:
        case SessionState::Tool::FreehandArrow:
            session->drawingStroke = true;
            session->currentStroke = QPainterPath();
            session->currentStroke.moveTo(global);
            strokeLayer = QPixmap(size());
            strokeLayer.fill(Qt::transparent);
            strokeLastPoint = local;
            break;
        case SessionState::Tool::Arrow:
        case SessionState::Tool::Rectangle:
        case SessionState::Tool::Blur:
        case SessionState::Tool::Ellipse:
        case SessionState::Tool::Line:
            session->draggingShape = true;
            session->shapeStart = global;
            session->shapeCurrent = global;
            break;
        case SessionState::Tool::Text:
        case SessionState::Tool::SpeechBalloon:
            beginTextAt(local);
            return;
        case SessionState::Tool::Step: {
            SessionState::Annotation a;
            a.type = SessionState::AnnotationType::Step;
            a.textPos = global;
            a.color = session->drawColor;
            a.stepNumber = session->nextStepNumber++;
            session->pushAnnotation(a);
            dirty = true;
            return;
        }
        case SessionState::Tool::Crop:
            session->selectedAnnotationIndex = -1;
            session->selecting = true;
            session->dragStart = global;
            session->selection = QRect(global, QSize(0, 0));
            break;
        case SessionState::Tool::Select: {
            if (session->selectedAnnotationIndex >= 0 &&
                session->selectedAnnotationIndex < session->annotations.size()) {
                SessionState::Annotation &selected = session->annotations[session->selectedAnnotationIndex];
                const int handle = hitTestHandle(selected, global);
                if (handle != -1) {
                    session->resizingAnnotation = true;
                    resizeHandleIndex = handle;
                    dragOriginal = selected;
                    dragAnchorGlobal = global;
                    return;
                }
                if (annotationBounds(selected).adjusted(-6, -6, 6, 6).contains(global)) {
                    session->movingAnnotation = true;
                    dragOriginal = selected;
                    dragAnchorGlobal = global;
                    return;
                }
            }

            const int hit = hitTestAnnotation(global);
            if (hit != -1) {
                session->selectedAnnotationIndex = hit;
                session->movingAnnotation = true;
                dragOriginal = session->annotations[hit];
                dragAnchorGlobal = global;
                session->notifyChanged();
                return;
            }

            session->selectedAnnotationIndex = -1;
            session->selecting = true;
            session->dragStart = global;
            session->selection = QRect(global, QSize(0, 0));
            break;
        }
    }
    session->notifyChanged();
}

void OverlayWindow::mouseMoveEvent(QMouseEvent *event) {
    lastMousePos = event->pos();
    hasMouse = true;

    const QPoint global = toGlobal(event->pos());

    if (session->movingAnnotation && session->selectedAnnotationIndex >= 0 &&
        session->selectedAnnotationIndex < session->annotations.size()) {
        SessionState::Annotation moved = dragOriginal;
        translateAnnotation(moved, global - dragAnchorGlobal);
        session->annotations[session->selectedAnnotationIndex] = moved;
        session->notifyChanged();
        return;
    }
    if (session->resizingAnnotation && session->selectedAnnotationIndex >= 0 &&
        session->selectedAnnotationIndex < session->annotations.size()) {
        SessionState::Annotation resized = dragOriginal;
        if (resizeHandleIndex == 0) {
            resized.start = global;
        } else {
            resized.end = global;
        }
        session->annotations[session->selectedAnnotationIndex] = resized;
        session->notifyChanged();
        return;
    }

    if (session->selecting) {
        session->selection = QRect(session->dragStart, global).normalized();
        session->notifyChanged();
        return;
    }
    if (session->drawingStroke) {
        session->currentStroke.lineTo(global);
        appendStrokeSegment(event->pos());
        session->notifyChanged();
        return;
    }
    if (session->draggingShape) {
        session->shapeCurrent = global;
        session->notifyChanged();
        return;
    }
    if (session->tool == SessionState::Tool::Select) update();
}

void OverlayWindow::leaveEvent(QEvent *) {
    hasMouse = false;
    update();
}

void OverlayWindow::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) return;

    if (session->movingAnnotation || session->resizingAnnotation) {
        session->movingAnnotation = false;
        session->resizingAnnotation = false;
        resizeHandleIndex = -1;
        dirty = true;
        session->notifyChanged();
        return;
    }

    if (session->selecting) {
        session->selecting = false;
        const bool isClick = session->selection.width() < 5 && session->selection.height() < 5;
        if (!standalone && isClick && session->tool == SessionState::Tool::Select) {
            if (event->modifiers() & Qt::ControlModifier) {
                session->pickColorAt(toGlobal(event->pos()));
            } else {
                session->selection = QRect(globalOffset, size());
                session->finalizeAndCopy();
            }
            return;
        }
        if (!standalone && session->selection.width() >= 5 && session->selection.height() >= 5) {
            session->finalizeAndCopy();
        } else {
            if (!standalone) session->selection = QRect();
            session->notifyChanged();
        }
        return;
    }

    if (session->drawingStroke) {
        session->drawingStroke = false;
        SessionState::Annotation a;
        if (session->tool == SessionState::Tool::Highlighter) {
            a.type = SessionState::AnnotationType::Highlight;
        } else if (session->tool == SessionState::Tool::FreehandArrow) {
            a.type = SessionState::AnnotationType::FreehandArrow;
        } else {
            a.type = SessionState::AnnotationType::Stroke;
        }
        a.path = session->currentStroke;
        a.color = session->drawColor;
        a.thickness = session->drawThickness;
        session->currentStroke = QPainterPath();
        session->pushAnnotation(a);
        dirty = true;
        return;
    }

    if (session->draggingShape) {
        session->draggingShape = false;
        const QRect r = QRect(session->shapeStart, session->shapeCurrent).normalized();
        if (r.width() >= 3 || r.height() >= 3) {
            SessionState::Annotation a;
            switch (session->tool) {
                case SessionState::Tool::Arrow: a.type = SessionState::AnnotationType::Arrow; break;
                case SessionState::Tool::Rectangle: a.type = SessionState::AnnotationType::Rectangle; break;
                case SessionState::Tool::Ellipse: a.type = SessionState::AnnotationType::Ellipse; break;
                case SessionState::Tool::Line: a.type = SessionState::AnnotationType::Line; break;
                default: a.type = SessionState::AnnotationType::Blur; break;
            }
            a.start = session->shapeStart;
            a.end = session->shapeCurrent;
            a.color = session->drawColor;
            a.thickness = session->drawThickness;
            session->pushAnnotation(a);
            dirty = true;
        } else {
            session->notifyChanged();
        }
    }
}

void OverlayWindow::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape) {
        if (textEdit) {
            textEdit->deleteLater();
            textEdit = nullptr;
        }
        if (standalone) {
            close();
        } else {
            session->cancel();
        }
        return;
    }
    if (event->modifiers() & Qt::ControlModifier) {
        if (event->key() == Qt::Key_Z && !(event->modifiers() & Qt::ShiftModifier)) {
            session->undo();
            if (standalone) resizeToFitImage();
            dirty = true;
            return;
        }
        if ((event->key() == Qt::Key_Z && (event->modifiers() & Qt::ShiftModifier)) || event->key() == Qt::Key_Y) {
            session->redo();
            if (standalone) resizeToFitImage();
            dirty = true;
            return;
        }
    }
    if ((event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) &&
        session->selectedAnnotationIndex >= 0 && session->selectedAnnotationIndex < session->annotations.size()) {
        session->annotations.removeAt(session->selectedAnnotationIndex);
        session->selectedAnnotationIndex = -1;
        dirty = true;
        session->notifyChanged();
        return;
    }
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && standalone &&
        session->tool == SessionState::Tool::Crop && session->selection.width() >= 5 &&
        session->selection.height() >= 5 && session->selection != QRect(QPoint(0, 0), session->fullImage.size())) {
        session->cropToSelection();
        resizeToFitImage();
        dirty = true;
        return;
    }
    QWidget::keyPressEvent(event);
}

void OverlayWindow::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    updateGlobalOffsetForCentering();
    repositionToolbar();
}

void OverlayWindow::updateGlobalOffsetForCentering() {
    if (!standalone) return;
    const QPoint centering(qMax(0, (width() - session->fullImage.width()) / 2),
                            qMax(0, (height() - session->fullImage.height()) / 2));
    globalOffset = -centering;
    update();
}

void OverlayWindow::resizeToFitImage() {
    if (!isMaximized() && !isFullScreen()) {
        resize(session->fullImage.size().expandedTo(minimumSize()));
    }
    updateGlobalOffsetForCentering();
}

void OverlayWindow::closeEvent(QCloseEvent *event) {
    if (!standalone || !dirty) {
        event->accept();
        return;
    }

    const auto choice =
        QMessageBox::question(this, "Unsaved changes", "You have unsaved changes. Save before closing?",
                               QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (choice == QMessageBox::Cancel) {
        event->ignore();
        return;
    }
    if (choice == QMessageBox::Save) {
        session->finalizeAndCopy();
        dirty = false;
    }
    event->accept();
}

void OverlayWindow::buildToolbar() {
    toolbar = new QWidget(this);
    toolbar->setObjectName("toolbar");

    createToolButtons();

    if (classicToolbar) {
        layoutClassicToolbar();
    } else {
        layoutModernToolbar();
    }

    updateToolControlsVisibility();
    syncedTool = session->tool;

    toolbar->adjustSize();
    toolbar->show();
    toolbar->raise();
}

void OverlayWindow::createToolButtons() {
    const int btnSize = classicToolbar ? 24 : 40;
    const int iconSize = classicToolbar ? 16 : 20;
    const int swatchSize = classicToolbar ? 20 : 32;

    selectButton = makeToolButton(toolbar, ":/icons/mouse-pointer-2.svg", "Select region", true, btnSize, iconSize);
    penButton = makeToolButton(toolbar, ":/icons/pencil.svg", "Pen", true, btnSize, iconSize);
    highlighterButton = makeToolButton(toolbar, ":/icons/highlighter.svg", "Highlighter", true, btnSize, iconSize);
    arrowButton = makeToolButton(toolbar, ":/icons/arrow-right.svg", "Arrow", true, btnSize, iconSize);
    rectangleButton = makeToolButton(toolbar, ":/icons/square.svg", "Rectangle", true, btnSize, iconSize);
    blurButton = makeToolButton(toolbar, ":/icons/droplet.svg", "Blur / pixelate", true, btnSize, iconSize);
    textButton = makeToolButton(toolbar, ":/icons/type.svg", "Text", true, btnSize, iconSize);
    ellipseButton = makeToolButton(toolbar, ":/icons/circle.svg", "Ellipse", true, btnSize, iconSize);
    lineButton = makeToolButton(toolbar, ":/icons/minus.svg", "Line", true, btnSize, iconSize);
    freehandArrowButton = makeToolButton(toolbar, ":/icons/spline.svg", "Freehand arrow", true, btnSize, iconSize);
    stepButton = makeToolButton(toolbar, ":/icons/hash.svg", "Numbered step", true, btnSize, iconSize);
    speechBalloonButton =
        makeToolButton(toolbar, ":/icons/message-square.svg", "Speech balloon", true, btnSize, iconSize);
    cropButton = makeToolButton(toolbar, ":/icons/crop.svg", "Crop (drag a rect, Enter to apply)", true, btnSize,
                                 iconSize);
    cropButton->setVisible(standalone);

    rotateLeftButton = makeToolButton(toolbar, ":/icons/rotate-ccw.svg", "Rotate left", false, btnSize, iconSize);
    rotateRightButton = makeToolButton(toolbar, ":/icons/rotate-cw.svg", "Rotate right", false, btnSize, iconSize);
    flipHButton =
        makeToolButton(toolbar, ":/icons/flip-horizontal-2.svg", "Flip horizontal", false, btnSize, iconSize);
    flipVButton = makeToolButton(toolbar, ":/icons/flip-vertical-2.svg", "Flip vertical", false, btnSize, iconSize);
    rotateLeftButton->setVisible(standalone);
    rotateRightButton->setVisible(standalone);
    flipHButton->setVisible(standalone);
    flipVButton->setVisible(standalone);

    if (standalone) {
        session->tool = SessionState::Tool::Pen;
        penButton->setChecked(true);
        selectButton->setVisible(false);
    } else {
        selectButton->setChecked(true);
    }

    undoButton = makeToolButton(toolbar, ":/icons/undo-2.svg", "Undo (Ctrl+Z)", false, btnSize, iconSize);
    redoButton = makeToolButton(toolbar, ":/icons/redo-2.svg", "Redo (Ctrl+Shift+Z)", false, btnSize, iconSize);

    colorButton = new QPushButton(toolbar);
    colorButton->setToolTip("Draw color");
    colorButton->setFixedSize(swatchSize, swatchSize);
    colorButton->setCursor(Qt::PointingHandCursor);
    updateColorButtonSwatch();

    thicknessCombo = new QComboBox(toolbar);
    for (int t : {1, 2, 3, 5, 8, 12}) thicknessCombo->addItem(QString::number(t), t);
    thicknessCombo->setCurrentText(QString::number(session->drawThickness));
    thicknessCombo->setFixedWidth(46);

    fontCombo = new QFontComboBox(toolbar);
    fontCombo->setCurrentFont(session->textFont);
    fontCombo->setFixedWidth(130);

    sizeCombo = new QComboBox(toolbar);
    sizeCombo->setEditable(false);
    for (int size : {12, 14, 18, 24, 32, 48, 64}) sizeCombo->addItem(QString::number(size), size);
    sizeCombo->setCurrentText(QString::number(session->textFont.pointSize()));
    sizeCombo->setFixedWidth(46);

    bgColorButton = new QPushButton(toolbar);
    bgColorButton->setToolTip("Text background color");
    bgColorButton->setFixedSize(swatchSize, swatchSize);
    updateBgButtonSwatch();

    if (standalone) {
        saveButton = new QPushButton("Save", toolbar);
        saveButton->setCursor(Qt::PointingHandCursor);
        saveButton->setStyleSheet(
            "QPushButton { background-color: #2f6fd6; padding: 0 16px; font-weight: 600; }"
            "QPushButton:hover { background-color: #4685e8; }");
        saveButton->setFixedHeight(34);
        connect(saveButton, &QPushButton::clicked, this, [this] {
            session->finalizeAndCopy();
            dirty = false;
            close();
        });
    }

    connect(selectButton, &QPushButton::clicked, this, [this] { setTool(SessionState::Tool::Select); });
    connect(penButton, &QPushButton::clicked, this, [this] { setTool(SessionState::Tool::Pen); });
    connect(highlighterButton, &QPushButton::clicked, this, [this] { setTool(SessionState::Tool::Highlighter); });
    connect(arrowButton, &QPushButton::clicked, this, [this] { setTool(SessionState::Tool::Arrow); });
    connect(rectangleButton, &QPushButton::clicked, this, [this] { setTool(SessionState::Tool::Rectangle); });
    connect(blurButton, &QPushButton::clicked, this, [this] { setTool(SessionState::Tool::Blur); });
    connect(textButton, &QPushButton::clicked, this, [this] { setTool(SessionState::Tool::Text); });
    connect(ellipseButton, &QPushButton::clicked, this, [this] { setTool(SessionState::Tool::Ellipse); });
    connect(lineButton, &QPushButton::clicked, this, [this] { setTool(SessionState::Tool::Line); });
    connect(freehandArrowButton, &QPushButton::clicked, this,
            [this] { setTool(SessionState::Tool::FreehandArrow); });
    connect(stepButton, &QPushButton::clicked, this, [this] { setTool(SessionState::Tool::Step); });
    connect(speechBalloonButton, &QPushButton::clicked, this,
            [this] { setTool(SessionState::Tool::SpeechBalloon); });
    connect(cropButton, &QPushButton::clicked, this, [this] {
        setTool(SessionState::Tool::Crop);
        session->selection = QRect(QPoint(0, 0), session->fullImage.size());
        session->notifyChanged();
    });

    connect(rotateLeftButton, &QPushButton::clicked, this, [this] {
        session->rotate90(false);
        resizeToFitImage();
        dirty = true;
        setFocus();
    });
    connect(rotateRightButton, &QPushButton::clicked, this, [this] {
        session->rotate90(true);
        resizeToFitImage();
        dirty = true;
        setFocus();
    });
    connect(flipHButton, &QPushButton::clicked, this, [this] {
        session->flip(true);
        dirty = true;
        setFocus();
    });
    connect(flipVButton, &QPushButton::clicked, this, [this] {
        session->flip(false);
        dirty = true;
        setFocus();
    });

    connect(undoButton, &QPushButton::clicked, this, [this] {
        session->undo();
        if (standalone) resizeToFitImage();
        dirty = true;
        setFocus();
    });
    connect(redoButton, &QPushButton::clicked, this, [this] {
        session->redo();
        if (standalone) resizeToFitImage();
        dirty = true;
        setFocus();
    });

    connect(colorButton, &QPushButton::clicked, this, [this] {
        QColor chosen = QColorDialog::getColor(session->drawColor, this, "Draw color");
        if (chosen.isValid()) {
            session->drawColor = chosen;
            updateColorButtonSwatch();
        }
        setFocus();
    });
    connect(thicknessCombo, &QComboBox::currentTextChanged, this,
            [this](const QString &text) { session->drawThickness = text.toInt(); });

    connect(fontCombo, &QFontComboBox::currentFontChanged, this,
            [this](const QFont &f) { session->textFont.setFamily(f.family()); });
    connect(sizeCombo, &QComboBox::currentTextChanged, this,
            [this](const QString &text) { session->textFont.setPointSize(text.toInt()); });
    connect(bgColorButton, &QPushButton::clicked, this, [this] {
        QColor chosen = QColorDialog::getColor(
            session->textBackground.alpha() > 0 ? session->textBackground : Qt::white, this,
            "Text background color", QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) {
            session->textBackground = chosen;
            updateBgButtonSwatch();
        }
        setFocus();
    });
}

void OverlayWindow::layoutModernToolbar() {
    toolbar->setStyleSheet(
        "QWidget#toolbar { background-color: #222222; border: 1px solid #1f1f1f; border-radius: 8px; }"
        "QPushButton { background-color: #2e2e2e; border: 1px solid #1f1f1f; border-radius: 4px; color: #d8dadb; }"
        "QPushButton:checked { background-color: qlineargradient(x1:0, y1:0, x2:1, y2:1, "
        "stop:0 #3e83f2, stop:1 #3975d5); border: 1px solid #1f1f1f; }"
        "QPushButton:hover:!checked { background-color: #3a3a3a; }"
        "QComboBox { background-color: #2e2e2e; color: #d8dadb; border: 1px solid #1f1f1f; border-radius: 4px; "
        "padding: 3px 6px; }"
        "QComboBox:hover { background-color: #3a3a3a; }");

    auto *outerLayout = new QVBoxLayout(toolbar);
    outerLayout->setContentsMargins(8, 8, 8, 8);
    outerLayout->setSpacing(6);

    auto *toolsRow = new QWidget(toolbar);
    auto *layout = new QHBoxLayout(toolsRow);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    outerLayout->addWidget(toolsRow);

    optionsRow = new QWidget(toolbar);
    auto *optionsLayout = new QHBoxLayout(optionsRow);
    optionsLayout->setContentsMargins(0, 0, 0, 0);
    optionsLayout->setSpacing(4);
    outerLayout->addWidget(optionsRow);

    for (auto *b : {selectButton, penButton, highlighterButton, lineButton, arrowButton, freehandArrowButton,
                    rectangleButton, ellipseButton, blurButton, textButton, speechBalloonButton, stepButton,
                    cropButton})
        layout->addWidget(b);

    layout->addWidget(makeSeparator(toolbar));
    layout->addWidget(undoButton);
    layout->addWidget(redoButton);

    optionsLayout->addWidget(colorButton);
    optionsLayout->addWidget(thicknessCombo);
    optionsLayout->addWidget(fontCombo);
    optionsLayout->addWidget(sizeCombo);
    optionsLayout->addWidget(bgColorButton);
    optionsLayout->addStretch();

    if (standalone) {
        layout->addWidget(makeSeparator(toolbar));
        layout->addWidget(rotateLeftButton);
        layout->addWidget(rotateRightButton);
        layout->addWidget(flipHButton);
        layout->addWidget(flipVButton);
        layout->addWidget(makeSeparator(toolbar));
        layout->addWidget(saveButton);
    }
}

void OverlayWindow::layoutClassicToolbar() {
    toolbar->setStyleSheet(
        "QWidget#toolbar { background-color: #272727; }"
        "QPushButton { background-color: transparent; border: 1px solid transparent; border-radius: 0px; "
        "color: #e7e9ea; }"
        "QPushButton:checked { background-color: #333333; border: 1px solid #3f3f3f; }"
        "QPushButton:hover:!checked { background-color: #2e2e2e; border: 1px solid #3f3f3f; }"
        "QComboBox { background-color: transparent; color: #e7e9ea; border: 1px solid transparent; "
        "border-radius: 0px; padding: 1px 4px; }"
        "QComboBox:hover { background-color: #2e2e2e; border: 1px solid #3f3f3f; }");

    optionsRow = nullptr;

    auto *layout = new QHBoxLayout(toolbar);
    layout->setContentsMargins(2, 1, 2, 1);
    layout->setSpacing(0);

    for (auto *b : {selectButton, penButton, highlighterButton, lineButton, arrowButton, freehandArrowButton,
                    rectangleButton, ellipseButton, blurButton, textButton, speechBalloonButton, stepButton,
                    cropButton})
        layout->addWidget(b);

    layout->addWidget(makeGrooveSeparator(toolbar));
    layout->addWidget(undoButton);
    layout->addWidget(redoButton);
    layout->addWidget(makeGrooveSeparator(toolbar));

    layout->addWidget(colorButton);
    layout->addWidget(thicknessCombo);
    layout->addWidget(fontCombo);
    layout->addWidget(sizeCombo);
    layout->addWidget(bgColorButton);
}

void OverlayWindow::repositionToolbar() {
    if (!toolbar) return;
    toolbar->move((width() - toolbar->width()) / 2, 16);
}

void OverlayWindow::setTool(SessionState::Tool tool) {
    if (textEdit) commitPendingText();
    session->tool = tool;
    syncedTool = tool;
    if (tool != SessionState::Tool::Select) session->selectedAnnotationIndex = -1;
    updateToolControlsVisibility();
    setFocus();
}

void OverlayWindow::updateToolControlsVisibility() {
    const auto tool = session->tool;
    selectButton->setChecked(tool == SessionState::Tool::Select);
    penButton->setChecked(tool == SessionState::Tool::Pen);
    highlighterButton->setChecked(tool == SessionState::Tool::Highlighter);
    arrowButton->setChecked(tool == SessionState::Tool::Arrow);
    rectangleButton->setChecked(tool == SessionState::Tool::Rectangle);
    blurButton->setChecked(tool == SessionState::Tool::Blur);
    textButton->setChecked(tool == SessionState::Tool::Text);
    ellipseButton->setChecked(tool == SessionState::Tool::Ellipse);
    lineButton->setChecked(tool == SessionState::Tool::Line);
    freehandArrowButton->setChecked(tool == SessionState::Tool::FreehandArrow);
    stepButton->setChecked(tool == SessionState::Tool::Step);
    speechBalloonButton->setChecked(tool == SessionState::Tool::SpeechBalloon);
    cropButton->setChecked(tool == SessionState::Tool::Crop);

    const bool showText = tool == SessionState::Tool::Text || tool == SessionState::Tool::SpeechBalloon;
    const bool showColor = tool == SessionState::Tool::Pen || tool == SessionState::Tool::Highlighter ||
                            tool == SessionState::Tool::Arrow || tool == SessionState::Tool::Rectangle ||
                            tool == SessionState::Tool::Ellipse || tool == SessionState::Tool::Line ||
                            tool == SessionState::Tool::FreehandArrow || tool == SessionState::Tool::Step;
    const bool showThickness = tool == SessionState::Tool::Pen || tool == SessionState::Tool::Arrow ||
                                tool == SessionState::Tool::Rectangle || tool == SessionState::Tool::Ellipse ||
                                tool == SessionState::Tool::Line || tool == SessionState::Tool::FreehandArrow;

    colorButton->setVisible(showColor);
    thicknessCombo->setVisible(showThickness);

    fontCombo->setVisible(showText);
    sizeCombo->setVisible(showText);
    bgColorButton->setVisible(showText);

    if (optionsRow) optionsRow->setVisible(showColor || showThickness || showText);

    toolbar->adjustSize();
    repositionToolbar();
}

void OverlayWindow::updateColorButtonSwatch() {
    colorButton->setStyleSheet(
        QString("background-color: %1; border-radius: 4px;").arg(session->drawColor.name(QColor::HexArgb)));
}

void OverlayWindow::updateBgButtonSwatch() {
    if (session->textBackground.alpha() == 0) {
        bgColorButton->setStyleSheet(
            "background-color: rgba(255,255,255,15); border: 1px dashed white; border-radius: 4px;");
        bgColorButton->setText(QString::fromUtf8("∅"));
    } else {
        bgColorButton->setText(QString());
        bgColorButton->setStyleSheet(QString("background-color: %1; border-radius: 4px;")
                                          .arg(session->textBackground.name(QColor::HexArgb)));
    }
}

void OverlayWindow::beginTextAt(const QPoint &localPos) {
    if (textEdit) commitPendingText();

    pendingTextTool = session->tool;
    pendingTextLocalPos = localPos;
    textEdit = new QLineEdit(this);
    textEdit->setStyleSheet(
        "background-color: rgba(255,255,255,220); color: black; "
        "font-size: 14pt; border: 1px solid #ed1c24;");
    textEdit->move(localPos);
    textEdit->resize(240, 34);
    textEdit->show();
    textEdit->setFocus();
    connect(textEdit, &QLineEdit::returnPressed, this, &OverlayWindow::commitPendingText);
}

void OverlayWindow::commitPendingText() {
    if (!textEdit) return;
    const QString text = textEdit->text();
    if (!text.isEmpty()) {
        QFontMetrics fm(session->textFont);
        SessionState::Annotation a;
        a.type = pendingTextTool == SessionState::Tool::SpeechBalloon ? SessionState::AnnotationType::SpeechBalloon
                                                                       : SessionState::AnnotationType::Text;
        a.textPos = toGlobal(pendingTextLocalPos) + QPoint(4, fm.ascent());
        a.text = text;
        a.font = session->textFont;
        a.background = session->textBackground;
        a.color = session->drawColor;
        session->pushAnnotation(a);
        dirty = true;
    }
    textEdit->deleteLater();
    textEdit = nullptr;
    setFocus();
}
