#include "mainwindow.h"
#include <QApplication>
#include <QFont>
#include <QCommandLineParser>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QTimer>
#include <QVariant>
int main(int argc,char **argv){
    QApplication app(argc,argv);
    QApplication::setApplicationName("OrbitTLM");
    QApplication::setOrganizationName("OrbitTLM");
    QApplication::setApplicationVersion(ORBITTLM_VERSION);
    QCommandLineParser args;
    args.addHelpOption();
    args.addVersionOption();
    args.addOption({"smoke-test","Start the GUI, check storage/SQLite and exit automatically."});
    args.addOption({"smoke-report","Write startup diagnostics as JSON.","file"});
    args.process(app);
    QFont font=app.font();font.setPointSize(10);app.setFont(font);
    MainWindow window;
    window.show();
    if(args.isSet("smoke-test")){
        QTimer::singleShot(1500,&app,[&app,&args]{
            const QDir data(orbit::applicationDataDirectory());
            const bool sqlite=QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE"));
            const bool writable=QFileInfo(data.path()).isWritable();
            const bool database=QFileInfo(data.filePath(QStringLiteral("orbittlm.sqlite"))).size()>0;
            const bool history=QFileInfo::exists(data.filePath(QStringLiteral("index.html")));
            const bool ok=sqlite&&writable&&database&&history;
            const QJsonObject report{{"ok",ok},{"qt",QString::fromLatin1(qVersion())},
                {"language",app.property("orbitTlmLanguage").toString()},
                {"platform",QGuiApplication::platformName()},{"dataDirectory",data.absolutePath()},
                {"sqliteDriver",sqlite},{"dataWritable",writable},{"databaseCreated",database},
                {"historyCreated",history},{"applicationDirectory",QCoreApplication::applicationDirPath()}};
            const QByteArray json=QJsonDocument(report).toJson(QJsonDocument::Indented);
            qInfo().noquote()<<json;
            if(args.isSet("smoke-report")){
                QSaveFile file(args.value("smoke-report"));
                if(!file.open(QIODevice::WriteOnly)||file.write(json)!=json.size()||!file.commit()){
                    qCritical()<<"Cannot write startup report";app.exit(2);return;
                }
            }
            app.exit(ok?0:2);
        });
    }
    return app.exec();
}
