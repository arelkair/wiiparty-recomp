#include "main_window.h"

#include <algorithm>

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QStackedWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <ctime>
#include <filesystem>

#include "wp/save_backup.h"

#include "build_runner.h"
#include "licenses.h"
#include "payload.h"
#include "key_capture.h"
#include "options.h"
#include "texts.h"
#include "wp/keymap.h"

namespace {

QLabel* label(const QString& text, const char* name, QWidget* parent) {
    auto* widget = new QLabel(text, parent);
    widget->setObjectName(name);
    widget->setWordWrap(true);
    return widget;
}

std::filesystem::path to_path(const QString& path) {
    return std::filesystem::path(path.toStdU16String());
}

QString backup_date(const QString& name) {
    QDateTime date = QDateTime::fromString(name.left(19), "yyyy-MM-dd_HH-mm-ss");
    if (!date.isValid()) {
        return name;
    }
    QLocale locale;
    return locale.toString(date.date(), QLocale::LongFormat) + ", " + locale.toString(date.time(), "HH:mm:ss");
}

qint64 folder_size(const QString& folder) {
    qint64 total = 0;
    QDirIterator it(folder, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        total += it.fileInfo().size();
    }
    return total;
}

const Option* find_option(const QString& key) {
    for (const Option& option : options()) {
        if (option.key == key) {
            return &option;
        }
    }
    return nullptr;
}

QString shown_keys(const QString& value) {
    QString text = QString::fromStdString(wp::keymap::format(wp::keymap::parse(value.toStdString()).codes, ", "));
    return text.isEmpty() ? texts().keys_none : text;
}

QString first_disc(const QString& folder) {
    QDir dir(folder);
    QStringList files = dir.entryList({"*.iso", "*.wbfs", "*.rvz", "*.ciso", "*.wia", "*.gcm"}, QDir::Files, QDir::Name);
    return files.isEmpty() ? QString() : dir.absoluteFilePath(files.first());
}

}

MainWindow::MainWindow(Project project, QWidget* parent)
    : QMainWindow(parent), project_(std::move(project)), settings_(project_.settings_file()), toolchain_(project_.root() + "/build/deps/toolchain") {
    setWindowTitle(project_.title() + " Recomp");
    resize(980, 680);
    setMinimumSize(780, 560);
    runner_ = new BuildRunner(project_.root(), this);
    disc_ = QSettings().value("disc").toString();
    if (disc_.isEmpty()) {
        disc_ = first_disc(project_.game_folder() + "/disc");
    }

    auto* central = new QWidget(this);
    auto* layout = new QHBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    pages_ = new QStackedWidget(central);
    pages_->addWidget(make_game_page());
    pages_->addWidget(make_settings_page());
    pages_->addWidget(make_saves_page());
    pages_->addWidget(make_controls_page());
    pages_->addWidget(make_licenses_page(this));
    layout->addWidget(make_sidebar());
    layout->addWidget(pages_, 1);
    setCentralWidget(central);

    connect(runner_, &BuildRunner::step_started, this, [this](int index) { set_step_state(index, texts().step_running); });
    connect(runner_, &BuildRunner::step_skipped, this, [this](int index) { set_step_state(index, texts().step_skipped); });
    connect(runner_, &BuildRunner::step_finished, this,
            [this](int index, bool success) { set_step_state(index, success ? texts().step_done : texts().step_failed); });
    connect(runner_, &BuildRunner::output, this, &MainWindow::append_log);
    connect(runner_, &BuildRunner::finished, this, [this](bool success, bool cancelled) {
        result_->setText(success ? texts().build_finished : cancelled ? texts().build_cancelled : texts().build_failed);
        install_button_->setEnabled(true);
        choose_button_->setEnabled(true);
        folder_button_->setEnabled(true);
        cancel_button_->setEnabled(false);
        refresh();
    });
    refresh();
}

QWidget* MainWindow::make_sidebar() {
    auto* sidebar = new QWidget(this);
    sidebar->setObjectName("sidebar");
    sidebar->setFixedWidth(220);
    auto* layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(20, 28, 20, 20);
    layout->setSpacing(4);
    auto* icon = new QLabel(sidebar);
    icon->setPixmap(QIcon(":/launcher/icon.ico").pixmap(48, 48));
    layout->addWidget(icon);
    layout->addSpacing(10);
    layout->addWidget(label(project_.title(), "brand", sidebar));
    layout->addWidget(label("Recomp", "detail", sidebar));
    layout->addSpacing(24);
    auto* group = new QButtonGroup(sidebar);
    const QString names[] = {texts().game, texts().settings, texts().saves, texts().controls, texts().licenses};
    for (int i = 0; i < 5; i++) {
        auto* button = new QPushButton(names[i], sidebar);
        button->setObjectName("nav");
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        button->setChecked(i == 0);
        group->addButton(button, i);
        layout->addWidget(button);
    }
    connect(group, &QButtonGroup::idClicked, this, [this](int index) {
        if (index == 2) {
            refresh_saves();
        }
        pages_->setCurrentIndex(index);
    });
    layout->addStretch(1);
    return sidebar;
}

QWidget* MainWindow::make_game_page() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(48, 40, 48, 32);
    layout->setSpacing(10);
    layout->addWidget(label(project_.title(), "display", page));
    status_ = label(QString(), "lead", page);
    layout->addWidget(status_);
    layout->addSpacing(12);
    game_panels_ = new QStackedWidget(page);
    game_panels_->addWidget(make_play_panel());
    game_panels_->addWidget(make_install_panel());
    layout->addWidget(game_panels_, 1);
    return page;
}

QWidget* MainWindow::make_play_panel() {
    auto* panel = new QWidget(this);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* play_button = new QPushButton(texts().play_button, panel);
    play_button->setObjectName("primary");
    play_button->setCursor(Qt::PointingHandCursor);
    play_button->setFixedWidth(200);
    connect(play_button, &QPushButton::clicked, this, &MainWindow::play);
    layout->addWidget(play_button);
    layout->addStretch(1);
    layout->addWidget(label(texts().play_hint, "detail", panel));
    return panel;
}

QWidget* MainWindow::make_install_panel() {
    const Texts& t = texts();
    auto* panel = new QWidget(this);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    layout->addWidget(label(t.install_intro, "detail", panel));
    layout->addSpacing(6);
    folder_widget_ = new QWidget(panel);
    auto* folder_row = new QHBoxLayout(folder_widget_);
    folder_row->setContentsMargins(0, 0, 0, 0);
    auto* folder_text = new QVBoxLayout;
    folder_text->setSpacing(2);
    folder_text->addWidget(label(t.install_folder, "section", folder_widget_));
    folder_text->addWidget(label(QDir::toNativeSeparators(project_.root()), "value", folder_widget_));
    folder_row->addLayout(folder_text, 1);
    folder_button_ = new QPushButton(t.change_folder, folder_widget_);
    folder_button_->setCursor(Qt::PointingHandCursor);
    connect(folder_button_, &QPushButton::clicked, this, &MainWindow::choose_folder);
    folder_row->addWidget(folder_button_, 0, Qt::AlignBottom);
    folder_widget_->setVisible(project_.packaged());
    layout->addWidget(folder_widget_);
    auto* disc_row = new QHBoxLayout;
    auto* disc_text = new QVBoxLayout;
    disc_text->setSpacing(2);
    disc_text->addWidget(label(t.disc, "section", panel));
    disc_label_ = label(QString(), "value", panel);
    disc_text->addWidget(disc_label_);
    disc_row->addLayout(disc_text, 1);
    choose_button_ = new QPushButton(t.choose_disc, panel);
    choose_button_->setCursor(Qt::PointingHandCursor);
    connect(choose_button_, &QPushButton::clicked, this, &MainWindow::choose_disc);
    disc_row->addWidget(choose_button_, 0, Qt::AlignBottom);
    layout->addLayout(disc_row);

    auto* work = new QHBoxLayout;
    work->setSpacing(16);
    steps_ = new QListWidget(panel);
    steps_->setObjectName("steps");
    steps_->setSelectionMode(QAbstractItemView::NoSelection);
    steps_->setFocusPolicy(Qt::NoFocus);
    work->addWidget(steps_, 2);
    log_ = new QPlainTextEdit(panel);
    log_->setObjectName("log");
    log_->setReadOnly(true);
    log_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    work->addWidget(log_, 3);
    layout->addLayout(work, 1);

    auto* actions = new QHBoxLayout;
    result_ = label(QString(), "detail", panel);
    actions->addWidget(result_, 1);
    cancel_button_ = new QPushButton(t.cancel, panel);
    cancel_button_->setEnabled(false);
    connect(cancel_button_, &QPushButton::clicked, runner_, &BuildRunner::cancel);
    actions->addWidget(cancel_button_);
    install_button_ = new QPushButton(t.install, panel);
    install_button_->setObjectName("primary");
    install_button_->setCursor(Qt::PointingHandCursor);
    connect(install_button_, &QPushButton::clicked, this, &MainWindow::install);
    actions->addWidget(install_button_);
    layout->addLayout(actions);
    return panel;
}

QWidget* MainWindow::make_settings_page() {
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* page = new QWidget(scroll);
    page->setObjectName("page");
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(48, 40, 48, 40);
    layout->setSpacing(6);
    layout->addWidget(label(texts().settings_heading, "heading", page));
    layout->addWidget(label(texts().settings_intro, "lead", page));
    QString group;
    for (const Option& option : options()) {
        if (option.kind == Option::Kind::Keys) {
            continue;
        }
        if (option.group != group) {
            group = option.group;
            layout->addSpacing(18);
            layout->addWidget(label(group, "section", page));
        }
        auto* row = new QWidget(page);
        row->setObjectName("row");
        auto* row_layout = new QHBoxLayout(row);
        row_layout->setContentsMargins(0, 10, 0, 10);
        auto* text = new QVBoxLayout;
        text->setSpacing(2);
        text->addWidget(label(option.label, "value", row));
        text->addWidget(label(option.detail, "detail", row));
        row_layout->addLayout(text, 1);
        if (option.kind == Option::Kind::Toggle) {
            auto* box = new QCheckBox(row);
            box->setCursor(Qt::PointingHandCursor);
            box->setChecked(settings_.value(option) != "0");
            connect(box, &QCheckBox::toggled, this, [this, option](bool on) { settings_.set(option, on ? "1" : "0"); });
            row_layout->addWidget(box);
        } else {
            auto* combo = new QComboBox(row);
            for (const auto& [value, name] : option.choices) {
                combo->addItem(name, value);
            }
            int index = combo->findData(settings_.value(option));
            if (index < 0 && !settings_.value(option).isEmpty()) {
                combo->addItem(settings_.value(option), settings_.value(option));
                index = combo->count() - 1;
            }
            combo->setCurrentIndex(index < 0 ? 0 : index);
            connect(combo, &QComboBox::currentIndexChanged, this, [this, option, combo](int) {
                settings_.set(option, combo->currentData().toString());
                if (option.key == "system.interface_language" && !runner_->running()) {
                    QProcess::startDetached(QCoreApplication::applicationFilePath(), {});
                    QCoreApplication::quit();
                }
            });
            row_layout->addWidget(combo);
        }
        layout->addWidget(row);
    }
    layout->addStretch(1);
    scroll->setWidget(page);
    return scroll;
}

QWidget* MainWindow::make_saves_page() {
    const Texts& t = texts();
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* page = new QWidget(scroll);
    page->setObjectName("page");
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(48, 40, 48, 40);
    layout->setSpacing(6);
    layout->addWidget(label(t.saves_heading, "heading", page));
    layout->addWidget(label(t.saves_intro, "lead", page));
    layout->addSpacing(18);
    auto* header = new QHBoxLayout;
    header->addWidget(label(t.saves_backups, "section", page), 1, Qt::AlignBottom);
    auto* open_button = new QPushButton(t.open_folder, page);
    open_button->setCursor(Qt::PointingHandCursor);
    connect(open_button, &QPushButton::clicked, this, &MainWindow::open_backups);
    header->addWidget(open_button);
    layout->addLayout(header);
    backup_rows_ = new QVBoxLayout;
    backup_rows_->setSpacing(0);
    layout->addLayout(backup_rows_);
    layout->addSpacing(10);
    saves_result_ = label(QString(), "detail", page);
    layout->addWidget(saves_result_);
    layout->addStretch(1);
    scroll->setWidget(page);
    refresh_saves();
    return scroll;
}

void MainWindow::refresh_saves() {
    const Texts& t = texts();
    while (QLayoutItem* item = backup_rows_->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    QWidget* page = saves_result_->parentWidget();
    std::vector<wp::saves::Backup> backups = wp::saves::list(to_path(project_.backups_folder()));
    if (backups.empty()) {
        const Option* option = find_option("saves.backups");
        bool disabled = option && settings_.value(*option) == "0";
        auto* empty = label(disabled ? t.saves_disabled : t.saves_empty, "detail", page);
        empty->setContentsMargins(0, 10, 0, 10);
        backup_rows_->addWidget(empty);
        return;
    }
    for (const wp::saves::Backup& backup : backups) {
        QString name = QString::fromStdString(backup.name);
        auto* row = new QWidget(page);
        row->setObjectName("row");
        auto* row_layout = new QHBoxLayout(row);
        row_layout->setContentsMargins(0, 10, 0, 10);
        auto* text = new QVBoxLayout;
        text->setSpacing(2);
        text->addWidget(label(backup_date(name), "value", row));
        qint64 size = folder_size(project_.backups_folder() + "/" + name);
        text->addWidget(label(t.backup_size.arg(QLocale().toString((size + 1023) / 1024)), "detail", row));
        row_layout->addLayout(text, 1);
        auto* restore_button = new QPushButton(t.restore, row);
        restore_button->setCursor(Qt::PointingHandCursor);
        connect(restore_button, &QPushButton::clicked, this, [this, name] { restore_backup(name); });
        row_layout->addWidget(restore_button);
        backup_rows_->addWidget(row);
    }
}

void MainWindow::restore_backup(const QString& name) {
    const Texts& t = texts();
    QString date = backup_date(name);
    if (QMessageBox::question(this, t.restore_title, t.restore_question.arg(date), QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) !=
        QMessageBox::Yes) {
        return;
    }
    const Option* option = find_option("saves.backups");
    int keep = option ? settings_.value(*option).toInt() : 0;
    wp::saves::Backup backup{name.toStdString(), to_path(project_.backups_folder() + "/" + name)};
    wp::saves::Outcome outcome =
        wp::saves::restore(to_path(project_.nand_folder()), to_path(project_.backups_folder()), backup, keep, std::time(nullptr));
    switch (outcome.result) {
    case wp::saves::Result::Created:
        saves_result_->setText(t.restore_done.arg(date));
        break;
    case wp::saves::Result::Unchanged:
        saves_result_->setText(t.restore_unchanged.arg(date));
        break;
    default:
        saves_result_->setText(t.restore_failed.arg(QString::fromLocal8Bit(outcome.message.c_str())));
        break;
    }
    refresh_saves();
}

void MainWindow::open_backups() {
    QDir().mkpath(project_.backups_folder());
    QDesktopServices::openUrl(QUrl::fromLocalFile(project_.backups_folder()));
}

QWidget* MainWindow::make_controls_page() {
    const Texts& t = texts();
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* page = new QWidget(scroll);
    page->setObjectName("page");
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(48, 40, 48, 40);
    layout->setSpacing(6);
    auto* heading = new QHBoxLayout;
    heading->addWidget(label(t.controls_heading, "heading", page), 1);
    auto* reset = new QPushButton(t.controls_reset, page);
    reset->setCursor(Qt::PointingHandCursor);
    heading->addWidget(reset, 0, Qt::AlignVCenter);
    layout->addLayout(heading);
    layout->addWidget(label(t.controls_intro, "lead", page));
    layout->addSpacing(18);
    for (const Option& option : options()) {
        if (option.kind != Option::Kind::Keys) {
            continue;
        }
        auto* row = new QWidget(page);
        row->setObjectName("row");
        auto* row_layout = new QHBoxLayout(row);
        row_layout->setContentsMargins(0, 6, 0, 6);
        row_layout->setSpacing(8);
        auto* text = new QVBoxLayout;
        text->setSpacing(2);
        text->addWidget(label(option.label, "value", row));
        if (!option.detail.isEmpty()) {
            text->addWidget(label(option.detail, "detail", row));
        }
        row_layout->addLayout(text, 1);
        auto* binding = new KeyCaptureButton(row);
        binding->setFixedWidth(300);
        binding->set_binding(shown_keys(settings_.value(option)));
        row_layout->addWidget(binding);
        auto* clear = new QPushButton(t.keys_clear, row);
        clear->setCursor(Qt::PointingHandCursor);
        row_layout->addWidget(clear);
        connect(binding, &KeyCaptureButton::captured, this, [this, option, binding](const QString& name) {
            std::vector<int> codes = wp::keymap::parse(settings_.value(option).toStdString()).codes;
            int code = wp::keymap::key_code(name.toStdString());
            if (std::find(codes.begin(), codes.end(), code) == codes.end()) {
                codes.push_back(code);
            }
            QString value = QString::fromStdString(wp::keymap::format(codes));
            settings_.set(option, value);
            binding->set_binding(shown_keys(value));
        });
        connect(clear, &QPushButton::clicked, this, [this, option, binding] {
            settings_.set(option, QString());
            binding->set_binding(shown_keys(QString()));
        });
        bindings_ << qMakePair(option, binding);
        layout->addWidget(row);
    }
    connect(reset, &QPushButton::clicked, this, [this] {
        for (const auto& [option, binding] : bindings_) {
            settings_.set(option, option.fallback);
            binding->set_binding(shown_keys(option.fallback));
        }
    });
    layout->addStretch(1);
    scroll->setWidget(page);
    return scroll;
}

void MainWindow::refresh() {
    const Texts& t = texts();
    bool installed = project_.built() && project_.extracted();
    if (runner_->running()) {
        return;
    }
    game_panels_->setCurrentIndex(installed ? 0 : 1);
    status_->setText(installed ? t.status_ready : project_.extracted() ? t.status_not_built : t.status_no_disc);
    bool need_disc = !project_.extracted();
    choose_button_->setVisible(need_disc);
    if (!need_disc) {
        disc_label_->setText(t.disc_extracted);
    } else {
        disc_label_->setText(disc_.isEmpty() ? t.disc_none : QFileInfo(disc_).fileName());
    }
}

void MainWindow::choose_disc() {
    QString file = QFileDialog::getOpenFileName(this, texts().choose_disc_title, disc_.isEmpty() ? project_.game_folder() : QFileInfo(disc_).absolutePath(),
                                                texts().disc_filter);
    if (file.isEmpty()) {
        return;
    }
    disc_ = file;
    QSettings().setValue("disc", disc_);
    refresh();
}

void MainWindow::set_step_state(int index, const QString& state) {
    if (index >= 0 && index < steps_->count()) {
        steps_->item(index)->setText(step_titles_[index] + "  ·  " + state);
    }
}

void MainWindow::append_log(const QString& text) {
    log_->moveCursor(QTextCursor::End);
    log_->insertPlainText(text);
    log_->verticalScrollBar()->setValue(log_->verticalScrollBar()->maximum());
}

void MainWindow::install() {
    const Texts& t = texts();
    if (runner_->running()) {
        return;
    }
    if (!project_.extracted() && disc_.isEmpty()) {
        result_->setText(t.choose_disc_first);
        return;
    }
    log_->clear();
    result_->setText(t.checking_tools);
    QCoreApplication::processEvents();
    QList<ToolStatus> tools = toolchain_.inspect();
    for (const ToolStatus& tool : tools) {
        append_log((tool.usable ? t.tool_ready.arg(tool.name, tool.version) : t.tool_to_download.arg(tool.name)) + "\n");
    }
    result_->clear();
    QString python = toolchain_.python();
    Project project = project_;
    QString extracted = QDir(project_.root()).relativeFilePath(project_.extracted_folder());
    QDir().mkpath(project_.root());
    QList<BuildStep> steps;
    if (project_.packaged()) {
        QString root = project_.root();
        steps << BuildStep{t.step_prepare, {}, {}, {}, [root](QString& message) { return extract_payload(root, message); }};
    }
    steps += toolchain_.preparation(tools);
    steps += QList<BuildStep>{
        {t.step_extract, "nodtool", {"extract", disc_, extracted}, [project] { return project.extracted(); }, {}},
        {t.step_sdl, python, {"tools/fetch_sdl.py"}, {}, {}},
        {t.step_unpack, python, {"recompiler/unpack_rels.py"}, {}, {}},
        {t.step_dol, python, {"recompiler/recomp.py"}, {}, {}},
        {t.step_modules, python, {"recompiler/recomp_rel.py", "--all"}, {}, {}},
        {t.step_links, python, {"recompiler/recomp.py"}, {}, {}},
        {t.step_dsp, python, {"recompiler/dsp/recomp_dsp.py"}, {}, {}},
        {t.step_configure, "cmake", {"-S", ".", "-B", "build/out", "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", "-DWP_GAME=" + project_.game()}, {}, {}},
        {t.step_compile, "cmake", {"--build", "build/out"}, {}, {}},
    };
    steps_->clear();
    step_titles_.clear();
    for (const BuildStep& step : steps) {
        step_titles_ << step.title;
        steps_->addItem(step.title + "  ·  " + t.step_waiting);
    }
    install_button_->setEnabled(false);
    choose_button_->setEnabled(false);
    folder_button_->setEnabled(false);
    cancel_button_->setEnabled(true);
    runner_->start(steps, toolchain_.environment());
}

void MainWindow::install_and_quit() {
    connect(runner_, &BuildRunner::finished, this, [this](bool success, bool) {
        QFile file(project_.root() + "/build/install.log");
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.write(log_->toPlainText().toUtf8());
        }
        QCoreApplication::exit(success ? 0 : 1);
    });
    install();
    if (!runner_->running()) {
        QCoreApplication::exit(2);
    }
}

void MainWindow::save_pages(const QString& prefix) {
    for (int panel = 0; panel < 2; panel++) {
        game_panels_->setCurrentIndex(panel);
        pages_->setCurrentIndex(0);
        QCoreApplication::processEvents();
        grab().save(prefix + "game" + QString::number(panel) + ".png");
    }
    pages_->setCurrentIndex(1);
    QCoreApplication::processEvents();
    grab().save(prefix + "settings.png");
    refresh_saves();
    pages_->setCurrentIndex(2);
    QCoreApplication::processEvents();
    grab().save(prefix + "saves.png");
    pages_->setCurrentIndex(3);
    QCoreApplication::processEvents();
    grab().save(prefix + "controls.png");
    pages_->setCurrentIndex(0);
    refresh();
}

void MainWindow::choose_folder() {
    QString start = QFileInfo(project_.root()).absolutePath();
    QString folder = QFileDialog::getExistingDirectory(this, texts().change_folder, start);
    if (folder.isEmpty()) {
        return;
    }
    if (QFileInfo(folder).fileName() != kInstallFolderName) {
        folder = QDir(folder).filePath(kInstallFolderName);
    }
    QSettings().setValue("root", folder);
    QProcess::startDetached(QCoreApplication::applicationFilePath(), {});
    QCoreApplication::quit();
}

void MainWindow::play() {
    if (!QProcess::startDetached(project_.executable(), {}, project_.root())) {
        status_->setText(texts().start_failed);
        return;
    }
    QCoreApplication::quit();
}
