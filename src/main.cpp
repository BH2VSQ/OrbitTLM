#include "mainwindow.h"
#include <QApplication>
#include <QFont>
int main(int argc,char **argv){
    QApplication app(argc,argv);
    QApplication::setApplicationName("OrbitTLM");
    QApplication::setOrganizationName("OrbitTLM");
    QFont font=app.font();font.setPointSize(10);app.setFont(font);
    MainWindow window;
    window.show();
    return app.exec();
}
