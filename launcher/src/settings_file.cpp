#include "settings_file.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>

SettingsFile::SettingsFile(QString path) : path_(std::move(path)) {
    load();
}

QString SettingsFile::value(const Option& option) const {
    return values_.value(option.key, option.fallback);
}

void SettingsFile::set(const Option& option, const QString& value) {
    values_[option.key] = value;
    save();
}

void SettingsFile::load() {
    QFile file(path_);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }
    QString section;
    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(';') || line.startsWith('#')) {
            continue;
        }
        if (line.startsWith('[') && line.endsWith(']')) {
            section = line.mid(1, line.size() - 2).trimmed();
            continue;
        }
        int equals = line.indexOf('=');
        if (equals < 0) {
            continue;
        }
        values_[section + "." + line.left(equals).trimmed()] = line.mid(equals + 1).trimmed();
    }
}

void SettingsFile::save() const {
    QDir().mkpath(QFileInfo(path_).absolutePath());
    QSaveFile file(path_);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return;
    }
    QTextStream out(&file);
    QString section;
    for (const Option& option : options()) {
        QString group = option.key.section('.', 0, 0);
        if (group != section) {
            if (!section.isEmpty()) {
                out << '\n';
            }
            out << '[' << group << "]\n";
            section = group;
        }
        out << option.key.section('.', 1) << '=' << value(option) << '\n';
    }
    out.flush();
    file.commit();
}
