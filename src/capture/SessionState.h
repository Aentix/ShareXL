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

    enum class Tool { Select, Pen, Highlighter, Arrow, Rectangle, Blur, Text };
    enum class AnnotationType { Stroke, Highlight, Arrow, Rectangle, Blur, Text };

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

    QColor drawColor{237, 28, 36};
    int drawThickness = 3;

    QFont textFont{QStringLiteral("Sans"), 18, QFont::Bold};
    QColor textBackground{Qt::transparent};

    QString saveDirectory;
    QString originalPath;

    void notifyChanged() { emit changed(); }
    void pushAnnotation(const Annotation &annotation);
    void undo();
    void redo();
    bool canUndo() const { return !annotations.isEmpty(); }
    bool canRedo() const { return !redoStack.isEmpty(); }

    static QImage pixelate(const QImage &src, const QRect &regionInSrc, int blockSize = 14);
    static void drawArrow(QPainter &painter, const QPoint &from, const QPoint &to, const QColor &color,
                           int thickness);

    void finalizeAndCopy();
    void cancel();

signals:
    void changed();
    void savedTo(const QString &path);
    void ended(bool copied);

private:
    void notify(const QString &savedPath) const;
};
