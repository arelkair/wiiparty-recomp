#include <QApplication>
#include <QCommandLineParser>
#include <QTimer>
#include <QFile>
#include <QIcon>
#include <QMessageBox>

#include "main_window.h"
#include "project.h"
#include "texts.h"

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    QApplication::setOrganizationName("wiiparty-recomp");
    QApplication::setApplicationName("launcher");
    QApplication::setWindowIcon(QIcon(":/launcher/icon.ico"));
    QApplication::setStyle("Fusion");
    QFile style(":/launcher/launcher.qss");
    if (style.open(QIODevice::ReadOnly)) {
        application.setStyleSheet(QString::fromUtf8(style.readAll()));
    }
    Project project = Project::locate("wiiparty");
    if (!project.valid()) {
        QMessageBox::critical(nullptr, "Wii Party Recomp", texts().project_missing);
        return 1;
    }
    QCommandLineParser parser;
    QCommandLineOption screenshot("screenshot", "Save a picture of each page and quit.", "prefix");
    parser.addOption(screenshot);
    QCommandLineOption install("install", "Install the game without interaction, write build/install.log and quit.");
    parser.addOption(install);
    parser.process(application);
    MainWindow window(project);
    window.show();
    if (parser.isSet(screenshot)) {
        QString prefix = parser.value(screenshot);
        QTimer::singleShot(300, &window, [&window, prefix] {
            window.save_pages(prefix);
            QApplication::quit();
        });
    }
    if (parser.isSet(install)) {
        QTimer::singleShot(0, &window, [&window] { window.install_and_quit(); });
    }
    return application.exec();
}
