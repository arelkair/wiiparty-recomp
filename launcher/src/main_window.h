#pragma once

#include <QList>
#include <QMainWindow>

#include "project.h"
#include "settings_file.h"
#include "toolchain.h"

class BuildRunner;
class KeyCaptureButton;
class QLabel;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QStackedWidget;
class QVBoxLayout;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(Project project, QWidget* parent = nullptr);

    void save_pages(const QString& prefix);
    void install_and_quit();

private:
    QWidget* make_sidebar();
    QWidget* make_game_page();
    QWidget* make_play_panel();
    QWidget* make_install_panel();
    QWidget* make_settings_page();
    QWidget* make_saves_page();
    void refresh_saves();
    void restore_backup(const QString& name);
    void open_backups();
    QWidget* make_controls_page();
    void refresh();
    void choose_disc();
    void install();
    void set_step_state(int index, const QString& state);
    void append_log(const QString& text);
    void play();

    Project project_;
    SettingsFile settings_;
    Toolchain toolchain_;
    BuildRunner* runner_ = nullptr;
    QStackedWidget* pages_ = nullptr;
    QStackedWidget* game_panels_ = nullptr;
    QLabel* status_ = nullptr;
    QLabel* disc_label_ = nullptr;
    QPushButton* choose_button_ = nullptr;
    QListWidget* steps_ = nullptr;
    QList<QString> step_titles_;
    QPlainTextEdit* log_ = nullptr;
    QLabel* result_ = nullptr;
    QPushButton* install_button_ = nullptr;
    QPushButton* cancel_button_ = nullptr;
    QString disc_;
    QVBoxLayout* backup_rows_ = nullptr;
    QLabel* saves_result_ = nullptr;
    QList<QPair<Option, KeyCaptureButton*>> bindings_;
};
