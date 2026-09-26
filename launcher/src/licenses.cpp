#include "licenses.h"

#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QVBoxLayout>

#include "texts.h"

namespace {

QString read_text(const QString& path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}

QString read_folder(const QString& folder) {
    QStringList parts;
    QDir dir(folder);
    for (const QString& name : dir.entryList(QDir::Files, QDir::Name)) {
        parts << "== " + name + " ==\n\n" + read_text(dir.filePath(name));
    }
    return parts.join("\n\n");
}

}

QWidget* make_licenses_page(QWidget* parent) {
    const Texts& t = texts();
    auto* page = new QWidget(parent);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(48, 40, 48, 32);
    layout->setSpacing(10);
    auto* heading = new QLabel(t.licenses_heading, page);
    heading->setObjectName("heading");
    layout->addWidget(heading);
    auto* intro = new QLabel(t.licenses_intro, page);
    intro->setObjectName("lead");
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto* row = new QHBoxLayout;
    row->setSpacing(16);
    auto* list = new QListWidget(page);
    list->setObjectName("steps");
    list->setFixedWidth(230);
    auto* text = new QPlainTextEdit(page);
    text->setObjectName("log");
    text->setReadOnly(true);
    text->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    row->addWidget(list);
    row->addWidget(text, 1);
    layout->addLayout(row, 1);

    QList<QPair<QString, QString>> entries = {{t.license_project, ":/source/LICENSE"}, {t.license_notices, ":/source/THIRD_PARTY_NOTICES.md"}};
    for (const QString& name : QDir(":/licenses").entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        entries.append({name == "qt6-base" ? QString("Qt 6") : name, ":/licenses/" + name});
    }
    for (const auto& [name, path] : entries) {
        auto* item = new QListWidgetItem(name, list);
        item->setData(Qt::UserRole, path);
    }
    QObject::connect(list, &QListWidget::currentItemChanged, text, [text](QListWidgetItem* item) {
        if (!item) {
            return;
        }
        QString path = item->data(Qt::UserRole).toString();
        text->setPlainText(path.startsWith(":/licenses/") ? read_folder(path) : read_text(path));
    });
    list->setCurrentRow(0);
    return page;
}
