#pragma once

#include <QMap>
#include <QString>

#include "options.h"

class SettingsFile {
public:
    explicit SettingsFile(QString path);

    QString value(const Option& option) const;
    void set(const Option& option, const QString& value);

private:
    void load();
    void save() const;

    QString path_;
    QMap<QString, QString> values_;
};
