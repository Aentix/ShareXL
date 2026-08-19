#pragma once

#include <QObject>
#include <QImage>
#include <QPainterPath>
#include <QFont>
#include <QColor>
#include <QVector>
#include <QPoint>

class QPainter;

class SessionState : public QObject {
    Q_OBJECT
public:
    explicit SessionState(QObject *parent = nullptr) : QObject(parent) {}

    enum class Tool { Select, Pen, Highlighter, Arrow, Rectangle, Blur, Text, Ellipse, Line, FreehandArrow, Step, SpeechBalloon, Crop };
    enum class AnnotationType { Stroke, Highlight, Arrow, Rectangle, Blur, Text, Ellipse, Line, FreehandArrow, Step, SpeechBalloon };

    struct Annotation {
        AnnotationType type;
        QPainterPath path;
        QPoint start, end;
        QColor color;
        int thickness = 3;
        QString text;
        QFont font;
        QColor background;
        QPoint textPos;
        int stepNumber = 0;
    };

    QImage fullImage;

    Tool tool = Tool::Select;

    bool selecting = false;
    QPoint dragStart;
    QRect selection;

    bool drawingStroke = false;
    QPainterPath currentStroke;

    bool draggingShape = false;
    QPoint shapeStart;
    QPoint shapeCurrent;

    QVector<Annotation> annotations;
    QVector<Annotation> redoStack;
    int selectedAnnotationIndex = -1;
    bool movingAnnotation = false;
    bool resizingAnnotation = false;

    QColor drawColor{237, 28, 36};
    int drawThickness = 3;

    QFont textFont{QStringLiteral("Sans"), 18, QFont::Bold};
    QColor textBackground{Qt::transparent};

    QString saveDirectory;
    QString originalPath;
    int nextStepNumber = 1;

    QImage canvasUndoImage;
    QVector<Annotation> canvasUndoAnnotations;
    bool hasCanvasUndo = false;

    void notifyChanged() { emit changed(); }
    void pushAnnotation(const Annotation &annotation);
    void undo();
    void redo();
    bool canUndo() const { return !annotations.isEmpty(); }
    bool canRedo() const { return !redoStack.isEmpty(); }

    static QImage pixelate(const QImage &src, const QRect &regionInSrc, int blockSize = 14);
    static void drawArrow(QPainter &painter, const QPoint &from, const QPoint &to, const QColor &color,
                           int thickness);
    static void drawArrowHead(QPainter &painter, const QPointF &tip, double angle, const QColor &color,
                               int thickness);
    static double pathEndAngle(const QPainterPath &path);

    void finalizeAndCopy();
    void cancel();
    void pickColorAt(const QPoint &globalPos);

    void cropToSelection();
    void rotate90(bool clockwise);
    void flip(bool horizontal);

signals:
    void changed();
    void savedTo(const QString &path);
    void ended(bool copied);

private:
    void notify(const QString &savedPath) const;
};
