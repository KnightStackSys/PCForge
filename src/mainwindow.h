#pragma once

#include "core.h"
#include <QMainWindow>
#include <QProcess>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;

class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    pcforge::Catalog catalog_;
    pcforge::Build build_;
    QString catalogPath_;
    QString autosavePath_;
    QComboBox *filter_ = nullptr;
    QLineEdit *search_ = nullptr;
    QListWidget *parts_ = nullptr;
    QTableWidget *buildTable_ = nullptr;
    QPlainTextEdit *prompt_ = nullptr;
    QPlainTextEdit *report_ = nullptr;
    QLabel *total_ = nullptr;
    QDoubleSpinBox *budget_ = nullptr;
    QPushButton *codexButton_ = nullptr;
    QProcess *codex_ = nullptr;
    int codexRun_ = 0;

    void refreshCatalog();
    void refreshBuild();
    void saveCatalog();
    void saveSession();
    void addOrEditPart(bool editing);
    void removePart();
    void assignPart();
    void removeFromBuild();
    void importPartPicker();
    void newBuild();
    void saveBuildAs();
    void loadBuildFrom();
    void generateOffline();
    void generateWithCodex();
    void handleCodexOutput(int exitCode, QProcess::ExitStatus status);
    void message(const QString &title, const QString &body);
};
