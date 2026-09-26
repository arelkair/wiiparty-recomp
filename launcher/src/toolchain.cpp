#include "toolchain.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

#include "texts.h"

namespace {

enum class Packaging { Archive, SelfExtracting, Single };

struct Package {
    const char* program;
    const char* name;
    const char* url;
    const char* sha256;
    const char* file;
    Packaging packaging;
    const char* folder;
    const char* bin;
};

#ifdef Q_OS_WIN
constexpr Package kPackages[] = {
    {"nodtool", "nodtool 1.4.4", "https://github.com/encounter/nod/releases/download/v1.4.4/nodtool-windows-x86_64.exe",
     "fb3203a68a59fa19ba9de0aa2f8c339396b6e4c5f9274d0858d302074cdedf8b", "nodtool.exe", Packaging::Single, "nodtool", "nodtool"},
    {"python", "Python 3.12.10", "https://www.python.org/ftp/python/3.12.10/python-3.12.10-embed-amd64.zip",
     "4acbed6dd1c744b0376e3b1cf57ce906f9dc9e95e68824584c8099a63025a3c3", "python-3.12.10-embed-amd64.zip", Packaging::Archive, "python", "python"},
    {"cmake", "CMake 4.4.3", "https://github.com/Kitware/CMake/releases/download/v4.4.3/cmake-4.4.3-windows-x86_64.zip",
     "4d52ebab7193a698651639ed80d8d04fd903358843572cf44c7fd234cb7c26ab", "cmake-4.4.3-windows-x86_64.zip", Packaging::Archive, "cmake",
     "cmake/cmake-4.4.3-windows-x86_64/bin"},
    {"ninja", "Ninja 1.13.2", "https://github.com/ninja-build/ninja/releases/download/v1.13.2/ninja-win.zip",
     "07fc8261b42b20e71d1720b39068c2e14ffcee6396b76fb7a795fb460b78dc65", "ninja-win.zip", Packaging::Archive, "ninja", "ninja"},
    {"g++", "MinGW-w64 GCC 15.2 (nuwen.net 20.0)", "https://nuwen.net/files/mingw/mingw-20.0.exe",
     "bce8bdaa095848561488d34fd80371a1f36eb0a590202eb534f32e026e5cd8ef", "mingw-20.0.exe", Packaging::SelfExtracting, "mingw", "mingw/MinGW/bin"},
};
#else
constexpr Package kPackages[1] = {};
#endif

struct Requirement {
    const char* program;
    QStringList arguments;
    const char* pattern;
    int major;
    int minor;
};

QString python_name() {
#ifdef Q_OS_WIN
    return "python";
#else
    return "python3";
#endif
}

QString compiler_name() {
#ifdef Q_OS_WIN
    return "g++";
#else
    return "c++";
#endif
}

const Package* package_for(const QString& program) {
    for (const Package& package : kPackages) {
        if (package.program && program == package.program) {
            return &package;
        }
    }
    return nullptr;
}

QString run(const QString& program, const QStringList& arguments) {
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(program, arguments);
    if (!process.waitForFinished(8000) || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        process.kill();
        return {};
    }
    return QString::fromLocal8Bit(process.readAll()).trimmed();
}

QString system_program(const char* name) {
#ifdef Q_OS_WIN
    return QDir(qEnvironmentVariable("SystemRoot", "C:/Windows")).filePath(QString("System32/") + name + ".exe");
#else
    return name;
#endif
}

QString sha256_of(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&file);
    return QString::fromLatin1(hash.result().toHex());
}

}

Toolchain::Toolchain(QString folder) : folder_(std::move(folder)) {}

QProcessEnvironment Toolchain::environment() const {
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    QStringList paths;
    for (const Package& package : kPackages) {
        if (package.bin) {
            paths << QDir::toNativeSeparators(folder_ + "/" + package.bin);
        }
    }
    paths << environment.value("PATH");
    environment.insert("PATH", paths.join(QDir::listSeparator()));
    return environment;
}

QString Toolchain::python() const {
    return python_name();
}

QList<ToolStatus> Toolchain::inspect() const {
    const QList<Requirement> requirements = {
        {"nodtool", {"--version"}, R"(nodtool)", 0, 0},
        {nullptr, {"--version"}, R"(Python (\d+)\.(\d+))", 3, 11},
        {"cmake", {"--version"}, R"(cmake version (\d+)\.(\d+))", 3, 20},
        {"ninja", {"--version"}, R"((\d+)\.(\d+))", 1, 10},
        {nullptr, {"-dumpfullversion"}, R"((\d+)\.(\d+))", 13, 0},
    };
    QStringList search = environment().value("PATH").split(QDir::listSeparator(), Qt::SkipEmptyParts);
    QList<ToolStatus> tools;
    for (int i = 0; i < requirements.size(); i++) {
        const Requirement& requirement = requirements[i];
        ToolStatus status;
        status.program = requirement.program ? QString(requirement.program) : i == 1 ? python_name() : compiler_name();
        status.name = status.program;
        status.downloadable = package_for(status.program) != nullptr;
        status.path = QStandardPaths::findExecutable(status.program, search);
        if (status.path.isEmpty() || status.path.contains("WindowsApps", Qt::CaseInsensitive)) {
            status.path.clear();
            tools << status;
            continue;
        }
        QString text = run(status.path, requirement.arguments);
        QRegularExpressionMatch match = QRegularExpression(requirement.pattern).match(text);
        if (!match.hasMatch()) {
            tools << status;
            continue;
        }
        status.version = match.captured(0);
        bool numeric = match.lastCapturedIndex() >= 2;
        int major = numeric ? match.captured(1).toInt() : 0;
        int minor = numeric ? match.captured(2).toInt() : 0;
        status.usable = major > requirement.major || (major == requirement.major && minor >= requirement.minor);
#ifdef Q_OS_WIN
        if (status.usable && i == 4) {
            status.usable = run(status.path, {"-dumpmachine"}).contains("mingw32");
        }
#endif
        tools << status;
    }
    return tools;
}

QList<BuildStep> Toolchain::preparation(const QList<ToolStatus>& tools) const {
    const Texts& t = texts();
    QList<BuildStep> steps;
    QString downloads = folder_ + "/downloads";
    for (const ToolStatus& tool : tools) {
        if (tool.usable) {
            continue;
        }
        const Package* package = package_for(tool.program);
        if (!package) {
            QString program = tool.program;
            steps << BuildStep{t.step_tool.arg(program), {}, {}, {}, [program](QString& message) {
                                   message = texts().tool_unavailable.arg(program);
                                   return false;
                               }};
            continue;
        }
        QString archive = downloads + "/" + package->file;
        QString destination = folder_ + "/" + package->folder;
        QString name = package->name;
        QString expected = package->sha256;
        steps << BuildStep{t.step_download.arg(name), system_program("curl"), {"-L", "--fail", "--silent", "--show-error", "--create-dirs", "-o", archive, package->url}, {}, {}};
        steps << BuildStep{t.step_verify.arg(name), {}, {}, {}, [archive, expected, destination](QString& message) {
                               if (sha256_of(archive) != expected) {
                                   QFile::remove(archive);
                                   message = texts().hash_mismatch.arg(QFileInfo(archive).fileName());
                                   return false;
                               }
                               QDir(destination).removeRecursively();
                               QDir().mkpath(destination);
                               return true;
                           }};
        switch (package->packaging) {
        case Packaging::Archive:
            steps << BuildStep{t.step_install.arg(name), system_program("tar"), {"-xf", archive, "-C", destination}, {}, {}};
            break;
        case Packaging::SelfExtracting:
            steps << BuildStep{t.step_install.arg(name), archive, {"-y", "-o" + QDir::toNativeSeparators(destination)}, {}, {}};
            break;
        case Packaging::Single:
            steps << BuildStep{t.step_install.arg(name), {}, {}, {}, [archive, destination, program = tool.program](QString&) {
                                   return QFile::copy(archive, destination + "/" + program + ".exe");
                               }};
            break;
        }
        steps << BuildStep{t.step_finish.arg(name), {}, {}, {}, [archive, destination](QString&) {
                               for (const QString& restriction : QDir(destination).entryList({"*._pth"}, QDir::Files)) {
                                   QFile::remove(destination + "/" + restriction);
                               }
                               QFile::remove(archive);
                               return true;
                           }};
    }
    return steps;
}
