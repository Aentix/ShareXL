#pragma once

#include <QDialog>
#include <QStringList>
#include <functional>

class QListWidget;

class HistoryDialog : public QDialog {
    Q_OBJECT
public:
    explicit HistoryDialog(const QStringList &paths, std::function<void()> reopenAfterEdit,
                            QWidget *parent = nullptr);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    QListWidget *list;

    void updateGridSize();
};
