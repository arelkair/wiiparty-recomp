#include "project.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

namespace {

QString find_root(const QString& start, const QString& game) {
    QDir dir(start);
    while (true) {
        if (QFileInfo::exists(dir.filePath("games/" + game + "/game.toml"))) {
            return dir.absolutePath();
        }
        if (!dir.cdUp()) {
            return {};
        }
    }
}

QString read_title(const QString& toml) {
    QFile file(toml);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    static const QRegularExpression name(R"re(^\s*name\s*=\s*"([^"]*)")re", QRegularExpression::MultilineOption);
    QRegularExpressionMatch match = name.match(QString::fromUtf8(file.readAll()));
    return match.hasMatch() ? match.captured(1) : QString();
}

}

Project Project::locate(const QString& game) {
    Project project;
    project.game_ = game;
    project.root_ = find_root(QCoreApplication::applicationDirPath(), game);
    if (project.root_.isEmpty()) {
        project.root_ = find_root(QDir::currentPath(), game);
    }
    if (!project.root_.isEmpty()) {
        project.title_ = read_title(project.game_folder() + "/game.toml");
    }
    if (project.title_.isEmpty()) {
        project.title_ = game;
    }
    return project;
}

bool Project::valid() const {
    return !root_.isEmpty();
}

QString Project::root() const {
    return root_;
}

QString Project::game() const {
    return game_;
}

QString Project::title() const {
    return title_;
}

QString Project::game_folder() const {
    return root_ + "/games/" + game_;
}

QString Project::extracted_folder() const {
    return game_folder() + "/extracted";
}

QString Project::settings_file() const {
    return game_folder() + "/settings.ini";
}

QString Project::nand_folder() const {
    return game_folder() + "/nand";
}

QString Project::backups_folder() const {
    return game_folder() + "/backups";
}

QString Project::executable() const {
#ifdef Q_OS_WIN
    return root_ + "/build/out/" + game_ + ".exe";
#else
    return root_ + "/build/out/" + game_;
#endif
}

bool Project::extracted() const {
    return QFileInfo::exists(extracted_folder() + "/sys/main.dol");
}

bool Project::built() const {
    return QFileInfo::exists(executable());
}
