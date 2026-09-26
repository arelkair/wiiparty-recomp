#include "payload.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

#include "texts.h"

namespace {

constexpr const char* kPrefix = ":/source";
constexpr const char* kRevisionFile = "/.launcher-revision";

QString stored_revision(const QString& root) {
    QFile file(root + kRevisionFile);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QString::fromUtf8(file.readAll()).trimmed();
}

}

bool payload_available() {
    return QFileInfo::exists(QString(kPrefix) + "/CMakeLists.txt");
}

bool extract_payload(const QString& root, QString& message) {
    if (stored_revision(root) == WP_SOURCE_REVISION) {
        return true;
    }
    int count = 0;
    QDirIterator it(kPrefix, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString source = it.next();
        QString target = root + source.mid(QString(kPrefix).size());
        QDir().mkpath(QFileInfo(target).absolutePath());
        QFile in(source);
        QFile out(target);
        if (!in.open(QIODevice::ReadOnly) || !out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            message = texts().extract_failed.arg(target);
            return false;
        }
        out.write(in.readAll());
        count++;
    }
    QFile revision(root + kRevisionFile);
    if (revision.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        revision.write(WP_SOURCE_REVISION);
    }
    message = texts().extracted_files.arg(count).arg(root);
    return true;
}
