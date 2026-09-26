#pragma once

#include <QList>
#include <QObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

#include <functional>

struct BuildStep {
    QString title;
    QString program;
    QStringList arguments;
    std::function<bool()> skip;
    std::function<bool(QString& message)> action;
};

class BuildRunner : public QObject {
    Q_OBJECT

public:
    explicit BuildRunner(QString working_directory, QObject* parent = nullptr);

    void start(QList<BuildStep> steps, QProcessEnvironment environment);
    void cancel();
    bool running() const;

signals:
    void step_started(int index);
    void step_skipped(int index);
    void step_finished(int index, bool success);
    void output(const QString& text);
    void finished(bool success, bool cancelled);

private:
    void next();
    void fail();

    QString working_directory_;
    QList<BuildStep> steps_;
    QProcessEnvironment environment_;
    int current_ = -1;
    bool cancelled_ = false;
    QProcess* process_ = nullptr;
};
