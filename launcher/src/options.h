#pragma once

#include <QList>
#include <QPair>
#include <QString>

struct Option {
    enum class Kind { Toggle, Choice };

    QString key;
    Kind kind;
    QString fallback;
    QString group;
    QString label;
    QString detail;
    QList<QPair<QString, QString>> choices;
};

const QList<Option>& options();
