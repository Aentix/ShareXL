#pragma once

#include <QWidget>
#include <QPoint>

#include "capture/SessionState.h"

class QPushButton;
class QLineEdit;
class QFontComboBox;
class QComboBox;
class QPainter;

class OverlayWindow : public QWidget {
    Q_OBJECT
public:
    OverlayWindow(SessionState *session, const QPoint &globalOffset, bool showToolbar, bool standalone = false,
                  QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    SessionState *session;
    QPoint globalOffset;
    bool standalone;
    bool dirty = false;

    QLineEdit *textEdit = nullptr;
    QPoint pendingTextLocalPos;

    QWidget *toolbar = nullptr;
    QPushButton *selectButton = nullptr;
    QPushButton *penButton = nullptr;
    QPushButton *highlighterButton = nullptr;
    QPushButton *arrowButton = nullptr;
    QPushButton *rectangleButton = nullptr;
    QPushButton *blurButton = nullptr;
    QPushButton *textButton = nullptr;
    QPushButton *undoButton = nullptr;
    QPushButton *redoButton = nullptr;
    QPushButton *saveButton = nullptr;

    QPushButton *colorButton = nullptr;
    QComboBox *thicknessCombo = nullptr;

    QFontComboBox *fontCombo = nullptr;
    QComboBox *sizeCombo = nullptr;
    QPushButton *bgColorButton = nullptr;

    QPoint toGlobal(const QPoint &local) const { return local + globalOffset; }
    QPoint toLocal(const QPoint &global) const { return global - globalOffset; }

    void buildToolbar();
    void repositionToolbar();
    void setTool(SessionState::Tool tool);
    void updateToolControlsVisibility();
    void updateColorButtonSwatch();
    void updateBgButtonSwatch();
    void drawAnnotation(QPainter &p, const SessionState::Annotation &a);

    void beginTextAt(const QPoint &localPos);
    void commitPendingText();
};
