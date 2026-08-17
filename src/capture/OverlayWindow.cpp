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

namespace {
QPushButton *makeToolButton(QWidget *parent, const QString &glyph, const QString &tooltip, bool checkable) {
    auto *button = new QPushButton(glyph, parent);
    button->setToolTip(tooltip);
    button->setCheckable(checkable);
    button->setCursor(Qt::PointingHandCursor);
    button->setFixedSize(34, 34);
    QFont f = button->font();
    f.setPointSize(13);
    button->setFont(f);
    return button;
}

QFrame *makeSeparator(QWidget *parent) {
    auto *line = new QFrame(parent);
    line->setFixedWidth(1);
    line->setStyleSheet("background-color: rgba(255,255,255,35);");
    return line;
}
}

OverlayWindow::OverlayWindow(SessionState *session, const QPoint &globalOffset, bool showToolbar,
                             bool standalone, QWidget *parent)
    : QWidget(parent), session(session), globalOffset(globalOffset), standalone(standalone) {
    if (standalone) {
        setWindowFlags(Qt::Window);
        setWindowTitle("ShareXL Editor");
        setFixedSize(session->fullImage.size());
    } else {
        setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    }
    setAttribute(Qt::WA_DeleteOnClose);
    setCursor(Qt::CrossCursor);
    setFocusPolicy(Qt::StrongFocus);

    connect(this->session, &SessionState::changed, this, [this] { update(); });

    if (showToolbar) buildToolbar();
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
    }
}

void OverlayWindow::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.drawImage(rect(), session->fullImage, QRect(globalOffset, size()));

    const QRect localSelection = session->selection.translated(-globalOffset);
    if (session->selecting && session->selection.isValid()) {
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

    for (const auto &a : session->annotations) drawAnnotation(p, a);

    if (session->drawingStroke) {
        SessionState::Annotation preview;
        preview.type = session->tool == SessionState::Tool::Highlighter ? SessionState::AnnotationType::Highlight
                                                                          : SessionState::AnnotationType::Stroke;
        preview.path = session->currentStroke;
        preview.color = session->drawColor;
        preview.thickness = session->drawThickness;
        drawAnnotation(p, preview);
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
            preview.type = session->tool == SessionState::Tool::Arrow ? SessionState::AnnotationType::Arrow
                                                                         : SessionState::AnnotationType::Rectangle;
            drawAnnotation(p, preview);
        }
    }
}

void OverlayWindow::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) return;
    const QPoint local = event->pos();
    const QPoint global = toGlobal(local);

    switch (session->tool) {
        case SessionState::Tool::Pen:
        case SessionState::Tool::Highlighter:
            session->drawingStroke = true;
            session->currentStroke = QPainterPath();
            session->currentStroke.moveTo(global);
            break;
        case SessionState::Tool::Arrow:
        case SessionState::Tool::Rectangle:
        case SessionState::Tool::Blur:
            session->draggingShape = true;
            session->shapeStart = global;
            session->shapeCurrent = global;
            break;
        case SessionState::Tool::Text:
            beginTextAt(local);
            return;
        case SessionState::Tool::Select:
            session->selecting = true;
            session->dragStart = global;
            session->selection = QRect(global, QSize(0, 0));
            break;
    }
    session->notifyChanged();
}

void OverlayWindow::mouseMoveEvent(QMouseEvent *event) {
    const QPoint global = toGlobal(event->pos());
    if (session->selecting) {
        session->selection = QRect(session->dragStart, global).normalized();
        session->notifyChanged();
        return;
    }
    if (session->drawingStroke) {
        session->currentStroke.lineTo(global);
        session->notifyChanged();
        return;
    }
    if (session->draggingShape) {
        session->shapeCurrent = global;
        session->notifyChanged();
    }
}

void OverlayWindow::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) return;

    if (session->selecting) {
        session->selecting = false;
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
        a.type = session->tool == SessionState::Tool::Highlighter ? SessionState::AnnotationType::Highlight
                                                                     : SessionState::AnnotationType::Stroke;
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
            a.type = session->tool == SessionState::Tool::Arrow      ? SessionState::AnnotationType::Arrow
                     : session->tool == SessionState::Tool::Rectangle ? SessionState::AnnotationType::Rectangle
                                                                       : SessionState::AnnotationType::Blur;
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
            dirty = true;
            return;
        }
        if ((event->key() == Qt::Key_Z && (event->modifiers() & Qt::ShiftModifier)) || event->key() == Qt::Key_Y) {
            session->redo();
            dirty = true;
            return;
        }
    }
    QWidget::keyPressEvent(event);
}

void OverlayWindow::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    repositionToolbar();
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
    toolbar->setStyleSheet(
        "QWidget#toolbar { background-color: rgba(32,32,36,240); border-radius: 10px; }"
        "QPushButton { background-color: transparent; border: none; border-radius: 6px; color: white; }"
        "QPushButton:checked { background-color: rgba(66,133,244,220); }"
        "QPushButton:hover:!checked { background-color: rgba(255,255,255,25); }"
        "QComboBox { background-color: rgba(255,255,255,15); color: white; border: none; border-radius: 5px; "
        "padding: 3px 6px; }");
    toolbar->setObjectName("toolbar");

    auto *layout = new QHBoxLayout(toolbar);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(4);

    selectButton = makeToolButton(toolbar, QString::fromUtf8("⬚"), "Select region", true);
    penButton = makeToolButton(toolbar, QString::fromUtf8("✎"), "Pen", true);
    highlighterButton = makeToolButton(toolbar, QString::fromUtf8("▬"), "Highlighter", true);
    arrowButton = makeToolButton(toolbar, QString::fromUtf8("↗"), "Arrow", true);
    rectangleButton = makeToolButton(toolbar, QString::fromUtf8("▭"), "Rectangle", true);
    blurButton = makeToolButton(toolbar, QString::fromUtf8("▒"), "Blur / pixelate", true);
    textButton = makeToolButton(toolbar, QString::fromUtf8("T"), "Text", true);

    if (standalone) {
        session->tool = SessionState::Tool::Pen;
        penButton->setChecked(true);
        selectButton->setVisible(false);
    } else {
        selectButton->setChecked(true);
    }

    for (auto *b : {selectButton, penButton, highlighterButton, arrowButton, rectangleButton, blurButton, textButton})
        layout->addWidget(b);

    layout->addWidget(makeSeparator(toolbar));

    undoButton = makeToolButton(toolbar, QString::fromUtf8("↺"), "Undo (Ctrl+Z)", false);
    redoButton = makeToolButton(toolbar, QString::fromUtf8("↻"), "Redo (Ctrl+Shift+Z)", false);
    layout->addWidget(undoButton);
    layout->addWidget(redoButton);

    layout->addWidget(makeSeparator(toolbar));

    colorButton = new QPushButton(toolbar);
    colorButton->setToolTip("Draw color");
    colorButton->setFixedSize(24, 24);
    colorButton->setCursor(Qt::PointingHandCursor);
    updateColorButtonSwatch();

    thicknessCombo = new QComboBox(toolbar);
    for (int t : {1, 2, 3, 5, 8, 12}) thicknessCombo->addItem(QString::number(t), t);
    thicknessCombo->setCurrentText(QString::number(session->drawThickness));
    thicknessCombo->setFixedWidth(46);

    layout->addWidget(colorButton);
    layout->addWidget(thicknessCombo);

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
    bgColorButton->setFixedSize(24, 24);
    updateBgButtonSwatch();

    layout->addWidget(fontCombo);
    layout->addWidget(sizeCombo);
    layout->addWidget(bgColorButton);

    if (standalone) {
        layout->addWidget(makeSeparator(toolbar));

        saveButton = new QPushButton("Save", toolbar);
        saveButton->setCursor(Qt::PointingHandCursor);
        saveButton->setStyleSheet(
            "QPushButton { background-color: #2f6fd6; padding: 0 16px; font-weight: 600; }"
            "QPushButton:hover { background-color: #4685e8; }");
        saveButton->setFixedHeight(34);
        layout->addWidget(saveButton);
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

    connect(undoButton, &QPushButton::clicked, this, [this] {
        session->undo();
        dirty = true;
        setFocus();
    });
    connect(redoButton, &QPushButton::clicked, this, [this] {
        session->redo();
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

    updateToolControlsVisibility();

    toolbar->adjustSize();
    toolbar->show();
    toolbar->raise();
}

void OverlayWindow::repositionToolbar() {
    if (!toolbar) return;
    toolbar->move((width() - toolbar->width()) / 2, 16);
}

void OverlayWindow::setTool(SessionState::Tool tool) {
    session->tool = tool;
    selectButton->setChecked(tool == SessionState::Tool::Select);
    penButton->setChecked(tool == SessionState::Tool::Pen);
    highlighterButton->setChecked(tool == SessionState::Tool::Highlighter);
    arrowButton->setChecked(tool == SessionState::Tool::Arrow);
    rectangleButton->setChecked(tool == SessionState::Tool::Rectangle);
    blurButton->setChecked(tool == SessionState::Tool::Blur);
    textButton->setChecked(tool == SessionState::Tool::Text);
    updateToolControlsVisibility();
    setFocus();
}

void OverlayWindow::updateToolControlsVisibility() {
    const auto tool = session->tool;
    const bool showText = tool == SessionState::Tool::Text;
    const bool showColor = tool == SessionState::Tool::Pen || tool == SessionState::Tool::Highlighter ||
                            tool == SessionState::Tool::Arrow || tool == SessionState::Tool::Rectangle;
    const bool showThickness =
        tool == SessionState::Tool::Pen || tool == SessionState::Tool::Arrow || tool == SessionState::Tool::Rectangle;

    colorButton->setVisible(showColor);
    thicknessCombo->setVisible(showThickness);

    fontCombo->setVisible(showText);
    sizeCombo->setVisible(showText);
    bgColorButton->setVisible(showText);

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
        a.type = SessionState::AnnotationType::Text;
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
