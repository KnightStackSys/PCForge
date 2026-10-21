#include "mainwindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QFont>

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("KnightStackSys");
    QCoreApplication::setApplicationName("PCForge");
    app.setStyle("Fusion");
    QFont font = app.font();
    font.setPointSize(10);
    app.setFont(font);

    MainWindow window;
    window.show();
    return app.exec();
}
