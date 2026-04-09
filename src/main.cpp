#include "mainwindow.h"

#include <QApplication>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);

    MainWindow window;
    window.showMaximized();
    QTimer::singleShot(0, &window, [&window]() {
        if (!window.isFullScreen())
        {
            window.showMaximized();
        }
    });

    return application.exec();
}
