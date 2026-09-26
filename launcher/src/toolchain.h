#pragma once

#include <QList>
#include <QProcessEnvironment>
#include <QString>

#include "build_runner.h"

struct ToolStatus {
    QString name;
    QString program;
    QString path;
    QString version;
    bool usable = false;
    bool downloadable = false;
};

class Toolchain {
public:
    explicit Toolchain(QString folder);

    QList<ToolStatus> inspect() const;
    QList<BuildStep> preparation(const QList<ToolStatus>& tools) const;
    QProcessEnvironment environment() const;
    QString python() const;

private:
    QString folder_;
};
