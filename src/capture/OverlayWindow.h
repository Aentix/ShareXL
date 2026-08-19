#pragma once

#include <QWidget>
#include <QPixmap>
#include <QPoint>

#include "capture/SessionState.h"

class QPushButton;
class QLineEdit;
class QFontComboBox;
class QComboBox;
class QPainter;
class QEvent;

class OverlayWindow : public QWidget {
    Q_OBJECT
public:
    OverlayWindow(SessionState *session, const QPoint &globalOffset, bool showToolbar, bool standalone = false,
                  bool classicToolbar = false, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    SessionState *session;
    QPoint globalOffset;
    bool standalone;
    bool classicToolbar;
    bool dirty = false;

    QLineEdit *textEdit = nullptr;
    QPoint pendingTextLocalPos;
    SessionState::Tool pendingTextTool = SessionState::Tool::Text;

    QPoint lastMousePos;
    bool hasMouse = false;

    QPixmap strokeLayer;
    QPoint strokeLastPoint;

    QPixmap committedLayer;
    int committedLayerCount = 0;
    int committedLayerExcluded = -1;

    int resizeHandleIndex = -1;
    QPoint dragAnchorGlobal;
    SessionState::Annotation dragOriginal;

    QWidget *toolbar = nullptr;
    QWidget *optionsRow = nullptr;
    QPushButton *selectButton = nullptr;
    QPushButton *penButton = nullptr;
    QPushButton *highlighterButton = nullptr;
    QPushButton *arrowButton = nullptr;
    QPushButton *rectangleButton = nullptr;
    QPushButton *blurButton = nullptr;
    QPushButton *textButton = nullptr;
    QPushButton *ellipseButton = nullptr;
    QPushButton *lineButton = nullptr;
    QPushButton *freehandArrowButton = nullptr;
    QPushButton *stepButton = nullptr;
    QPushButton *speechBalloonButton = nullptr;
    QPushButton *undoButton = nullptr;
    QPushButton *redoButton = nullptr;
    QPushButton *saveButton = nullptr;

    QPushButton *cropButton = nullptr;
    QPushButton *rotateLeftButton = nullptr;
    QPushButton *rotateRightButton = nullptr;
    QPushButton *flipHButton = nullptr;
    QPushButton *flipVButton = nullptr;

    QPushButton *colorButton = nullptr;
    QComboBox *thicknessCombo = nullptr;

    QFontComboBox *fontCombo = nullptr;
    QComboBox *sizeCombo = nullptr;
    QPushButton *bgColorButton = nullptr;

    SessionState::Tool syncedTool = SessionState::Tool::Select;

    QPoint toGlobal(const QPoint &local) const { return local + globalOffset; }
    QPoint toLocal(const QPoint &global) const { return global - globalOffset; }

    void updateGlobalOffsetForCentering();
    void resizeToFitImage();

    void buildToolbar();
    void createToolButtons();
    void layoutModernToolbar();
    void layoutClassicToolbar();
    void repositionToolbar();
    void setTool(SessionState::Tool tool);
    void updateToolControlsVisibility();
    void updateColorButtonSwatch();
    void updateBgButtonSwatch();
    void drawAnnotation(QPainter &p, const SessionState::Annotation &a);
    void drawMagnifier(QPainter &p);
    void appendStrokeSegment(const QPoint &newLocalPoint);
    void syncCommittedLayer();
    QRect annotationBounds(const SessionState::Annotation &a) const;
    bool hasResizeHandles(const SessionState::Annotation &a) const;
    int hitTestHandle(const SessionState::Annotation &a, const QPoint &global) const;
    int hitTestAnnotation(const QPoint &global) const;
    void translateAnnotation(SessionState::Annotation &a, const QPoint &delta) const;
    void drawSelectionHandles(QPainter &p, const SessionState::Annotation &a);

    void beginTextAt(const QPoint &localPos);
    void commitPendingText();
};
