#include "mainwindow.h"

#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("云途智能交通预约系统"));
    app.setOrganizationName(QStringLiteral("程序设计最终QT"));
    MainWindow window;
    window.show();
    return app.exec();
}
