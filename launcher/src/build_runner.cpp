#include "build_runner.h"

#include <QDir>
#include <QStandardPaths>
#include <QTimer>

#include "texts.h"

BuildRunner::BuildRunner(QString working_directory, QObject* parent) : QObject(parent), working_directory_(std::move(working_directory)) {}

void BuildRunner::start(QList<BuildStep> steps, QProcessEnvironment environment) {
    if (running()) {
        return;
    }
    steps_ = std::move(steps);
    environment_ = std::move(environment);
    current_ = -1;
    cancelled_ = false;
    next();
}

void BuildRunner::cancel() {
    if (!running()) {
        return;
    }
    cancelled_ = true;
    if (process_) {
        process_->kill();
    }
}

bool BuildRunner::running() const {
    return current_ >= 0 && current_ < steps_.size();
}

void BuildRunner::fail() {
    emit step_finished(current_, false);
    bool cancelled = cancelled_;
    current_ = -1;
    emit finished(false, cancelled);
}

void BuildRunner::next() {
    current_++;
    while (current_ < steps_.size() && steps_[current_].skip && steps_[current_].skip()) {
        emit step_skipped(current_);
        current_++;
    }
    if (cancelled_) {
        current_ = qMax(0, current_ - 1);
        fail();
        return;
    }
    if (current_ >= steps_.size()) {
        current_ = -1;
        emit finished(true, false);
        return;
    }
    const BuildStep& step = steps_[current_];
    emit step_started(current_);
    if (step.action) {
        QTimer::singleShot(0, this, [this] {
            QString message;
            bool success = steps_[current_].action(message);
            if (!message.isEmpty()) {
                emit output(message + "\n");
            }
            if (!success) {
                fail();
                return;
            }
            emit step_finished(current_, true);
            next();
        });
        return;
    }
    QStringList paths = environment_.value("PATH").split(QDir::listSeparator(), Qt::SkipEmptyParts);
    QString program = QStandardPaths::findExecutable(step.program, paths);
    if (program.isEmpty()) {
        program = QStandardPaths::findExecutable(step.program);
    }
    if (program.isEmpty()) {
        emit output(texts().missing_tool.arg(step.program) + "\n");
        fail();
        return;
    }
    emit output("> " + step.program + " " + step.arguments.join(' ') + "\n");
    process_ = new QProcess(this);
    process_->setWorkingDirectory(working_directory_);
    process_->setProcessEnvironment(environment_);
    process_->setProcessChannelMode(QProcess::MergedChannels);
    connect(process_, &QProcess::readyReadStandardOutput, this, [this] { emit output(QString::fromLocal8Bit(process_->readAllStandardOutput())); });
    connect(process_, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        bool success = !cancelled_ && status == QProcess::NormalExit && code == 0;
        process_->deleteLater();
        process_ = nullptr;
        if (!success) {
            fail();
            return;
        }
        emit step_finished(current_, true);
        next();
    });
    connect(process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart) {
            return;
        }
        emit output(process_->errorString() + "\n");
        process_->deleteLater();
        process_ = nullptr;
        fail();
    });
    process_->start(program, step.arguments);
}
