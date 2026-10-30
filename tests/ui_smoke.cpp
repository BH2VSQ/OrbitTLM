#include "mainwindow.h"
#include <QtTest/QtTest>
#include <QApplication>
#include <QLabel>
#include <QTableWidget>
#include <QTabWidget>
#include <QLineEdit>
#include <QMenu>
#include <QAction>
#include <QPushButton>
#include <QGroupBox>
#include <QStandardPaths>
#include <QFontDatabase>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTcpServer>
#include <QTcpSocket>
#include "mainwindow_widgets.h"

class UiSmoke:public QObject {
    Q_OBJECT
    QByteArray originalSettings_;
    bool hadSettings_=false;
    QString settingsPath_;
private slots:
    void initTestCase(){
        QStandardPaths::setTestModeEnabled(true);
#ifdef Q_OS_WIN
        const int font=QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR")+"/Fonts/meiryo.ttc");
        if(font>=0)QApplication::setFont(QFont(QFontDatabase::applicationFontFamilies(font).first()));
#endif
        settingsPath_=QApplication::applicationDirPath()+"/data/app-settings.json";
        QFile settings(settingsPath_);
        hadSettings_=settings.open(QIODevice::ReadOnly);
        if(hadSettings_)originalSettings_=settings.readAll();
    }
    void init(){
        QDir().mkpath(QFileInfo(settingsPath_).absolutePath());
        QFile settings(settingsPath_);
        QVERIFY(settings.open(QIODevice::WriteOnly));
        QVERIFY(settings.write("{\"language\":\"zh-CN\",\"tcpHost\":\"127.0.0.1\",\"tcpPort\":9985}")>0);
    }
    void cleanupTestCase(){
        if(hadSettings_){
            QFile settings(settingsPath_);
            QVERIFY(settings.open(QIODevice::WriteOnly));
            QCOMPARE(settings.write(originalSettings_),originalSettings_.size());
        }else{
            QVERIFY(QFile::remove(settingsPath_));
        }
    }
    void startupShowsChineseOverviewAndHidesDebug() {
        QStandardPaths::setTestModeEnabled(true);
        MainWindow window;
        window.show();
        QApplication::processEvents();
        // Best-effort offscreen render snapshot, not a prerequisite for tests.
        const QPixmap preview=window.grab();
        if(!preview.isNull())
            preview.save(QApplication::applicationDirPath()+"/OrbitTLM-UI-preview.png");
        const auto *tabs=window.findChild<QTabWidget*>("mainPages");
        QVERIFY(tabs!=nullptr);
        QVERIFY(tabs->count()>=6);
        QCOMPARE(tabs->tabText(0),QStringLiteral("总览"));
        QCOMPARE(tabs->tabText(1),QStringLiteral("业务遥测"));
        QCOMPARE(tabs->tabText(2),QStringLiteral("分系统数据"));
        QCOMPARE(tabs->tabText(3),QStringLiteral("姿态与轨道"));
        QCOMPARE(tabs->tabText(4),QStringLiteral("图像数据"));
        QCOMPARE(tabs->tabText(5),QStringLiteral("原始数据"));
        QVERIFY(!tabs->isTabVisible(tabs->count()-1));
        auto *languageMenu=window.findChild<QMenu*>("languageMenu");
        QVERIFY(languageMenu!=nullptr);
        QCOMPARE(languageMenu->actions().size(),3);
        auto *zhAction=window.findChild<QAction*>("languageAction_zh-CN");
        auto *enAction=window.findChild<QAction*>("languageAction_en-US");
        QVERIFY(zhAction!=nullptr);
        QVERIFY(enAction!=nullptr);
        enAction->trigger();
        QApplication::processEvents();
        QVERIFY(enAction->isChecked());
        QVERIFY(!zhAction->isChecked());
        zhAction->trigger();
        QApplication::processEvents();
        QVERIFY(zhAction->isChecked());
        QVERIFY(window.findChild<QPushButton*>("exportSelectedImageBin")!=nullptr);
        QVERIFY(window.findChild<QPushButton*>("importImageBin")!=nullptr);
        // The visible replay surface is the custom keyframe timeline. The table
        // is retained only as a hidden event model for stable filtering/selection.
        const auto *timeline=window.findChild<QWidget*>("replayKeyframeTimeline");
        QVERIFY2(timeline!=nullptr,"The bottom replay keyframe timeline must be created");
        QVERIFY(!timeline->isHidden());
        const auto *event=window.findChild<QTableWidget*>("eventTimelineModel");
        QVERIFY2(event!=nullptr,"The hidden event model must remain available");
        QCOMPARE(event->columnCount(),4);
        QCOMPARE(event->horizontalHeaderItem(0)->text(),QStringLiteral("时间"));
        QCOMPARE(event->horizontalHeaderItem(1)->text(),QStringLiteral("数据包 / 事件"));
        QCOMPARE(event->rowCount(),0);
        QVERIFY(event->isHidden());
        auto *collapse=window.findChild<QPushButton*>("replayCollapseButton");
        QVERIFY(collapse!=nullptr);
        const auto *playPause=window.findChild<QPushButton*>("playPauseButton");
        QVERIFY(playPause!=nullptr);
        QCOMPARE(playPause->text(),QStringLiteral("播放"));
        auto *previous=window.findChild<QPushButton*>("replayPreviousButton");
        auto *next=window.findChild<QPushButton*>("replayNextButton");
        QVERIFY(previous!=nullptr); QVERIFY(next!=nullptr);
        QVERIFY(!previous->isHidden()); QVERIFY(!next->isHidden());
        collapse->click(); QApplication::processEvents();
        QVERIFY(timeline->isHidden());
        QVERIFY(!previous->isHidden()); QVERIFY(!playPause->isHidden()); QVERIFY(!next->isHidden());
        collapse->click(); QApplication::processEvents();
        QVERIFY(!timeline->isHidden());
        for(auto *card:window.findChildren<QGroupBox*>("statusCard")){
            auto *table=card->findChild<QTableWidget*>("statusTable");
            if(table)QCOMPARE(table->rowCount(),0);
        }
        const auto *overview=window.findChild<QLabel*>("currentHeading");
        QVERIFY(overview!=nullptr);
        QVERIFY(overview->text().contains(QStringLiteral("未解析")));
    }
    void switchingTabsWithEmptyDataDoesNotCrash() {
        MainWindow window;
        auto *tabs=window.findChild<QTabWidget*>("mainPages");
        QVERIFY(tabs!=nullptr);
        for(int i=0;i<tabs->count()-1;i++){
            tabs->setCurrentIndex(i);
            QCOMPARE(tabs->currentIndex(),i);
        }
    }
    void japaneseRoundTripAndFreshCatalogs(){
        MainWindow window;
        QApplication::processEvents();
        QTRY_VERIFY(!window.tcpConnecting_);
        window.live_->stop();
        auto *ja=window.findChild<QAction*>("languageAction_ja-JP");
        QVERIFY(ja);
        QCOMPARE(ja->text(),QStringLiteral("日本語"));
        for(int round=0;round<2;++round){
            ja->trigger();
            QApplication::processEvents();
            QVERIFY(ja->isChecked());
            QCOMPARE(window.pages_->tabText(0),QStringLiteral("概要"));
            QCOMPARE(window.pages_->tabText(1),QStringLiteral("機器テレメトリ"));
            QCOMPARE(window.pages_->tabText(2),QStringLiteral("サブシステムデータ"));
            QCOMPARE(window.pages_->tabText(3),QStringLiteral("姿勢・軌道"));
            QCOMPARE(window.pages_->tabText(4),QStringLiteral("画像データ"));
            QCOMPARE(window.pages_->tabText(5),QStringLiteral("生データ"));
            QCOMPARE(window.tcpStatusLabel_->text(),QStringLiteral("TCP：未接続"));
            QVERIFY(!window.sourceLabel_->text().contains(QStringLiteral("未选择")));
            QVERIFY(!window.message_->text().contains(QStringLiteral("已重组")));
            QCOMPARE(window.uiText(QStringLiteral("KISS 已重组：12 · 应用 CRC 未校验 · 缺帧中断：0")),
                     QStringLiteral("KISS 再組立て済み：12 · アプリケーション CRC 未検証 · フレーム欠落による中断：0"));
            QCOMPARE(orbitWidgetText(QStringLiteral("磁强计")),QStringLiteral("磁力計"));
            QCOMPARE(window.uiText(QStringLiteral("12V母线电压")),QStringLiteral("12Vバス電圧"));
            QCOMPARE(QCoreApplication::translate("QPlatformTheme","Cancel"),QStringLiteral("キャンセル"));
            window.findChild<QAction*>("languageAction_en-US")->trigger();
            QCOMPARE(window.pages_->tabText(1),QStringLiteral("Engineering Telemetry"));
            QCOMPARE(QCoreApplication::translate("QPlatformTheme","Cancel"),QStringLiteral("Cancel"));
            window.findChild<QAction*>("languageAction_zh-CN")->trigger();
            QCOMPARE(window.pages_->tabText(1),QStringLiteral("业务遥测"));
        }
        ja->trigger();
        {
            MainWindow reopened;
            QCOMPARE(reopened.languageCode_,QStringLiteral("ja-JP"));
            int checked=0;
            for(auto *table:reopened.findChildren<QTableWidget*>()){
                for(int row=0;row<table->rowCount();++row){
                    const auto *cell=table->item(row,0);
                    if(!cell)continue;
                    if(cell->data(Qt::UserRole+123).toString()==QStringLiteral("遥测主类型")){
                        QCOMPARE(cell->text(),QStringLiteral("テレメトリ主種別"));
                        ++checked;
                    }
                }
            }
            QVERIFY2(checked>1,"Fresh catalog cells must use Japanese before a language-toggle pass");
        }
        window.findChild<QAction*>("languageAction_ja-JP")->trigger();
        window.show();
        QApplication::processEvents();
        QVERIFY(window.grab().save(QApplication::applicationDirPath()+"/OrbitTLM-Japanese-preview.png"));
        window.findChild<QAction*>("languageAction_zh-CN")->trigger();
    }
    void replacementDoesNotTranslateItsOwnOutput(){
        const QJsonObject map{{QStringLiteral("甲"),QStringLiteral("乙甲")},
                              {QStringLiteral("乙"),QStringLiteral("丙")},
                              {QStringLiteral("甲乙"),QStringLiteral("長い語")}};
        const QStringList keys={QStringLiteral("甲"),QStringLiteral("乙"),QStringLiteral("甲乙")};
        QCOMPARE(orbitTranslateSource(QStringLiteral("前甲後 %1"),map,keys),QStringLiteral("前乙甲後 %1"));
        QCOMPARE(orbitTranslateSource(QStringLiteral("前甲乙後"),map,keys),QStringLiteral("前長い語後"));
    }
    void japaneseTcpUpdatesAndHistoryKeepRawValues(){
        MainWindow window;
        QApplication::processEvents();
        QTRY_VERIFY(!window.tcpConnecting_);
        window.live_->stop();
        window.findChild<QAction*>("languageAction_ja-JP")->trigger();
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost,0));
        window.live_->start(QStringLiteral("127.0.0.1"),server.serverPort(),QStringLiteral("AUTO"));
        QTRY_VERIFY(server.hasPendingConnections());
        auto *peer=server.nextPendingConnection();
        QVERIFY(peer);
        QTRY_COMPARE(window.tcpStatusLabel_->text(),QStringLiteral("TCP：接続済み"));
        window.live_->stop();
        QTRY_COMPARE(window.tcpStatusLabel_->text(),QStringLiteral("TCP：未接続"));
        delete peer;
        orbit::BusinessPacket packet;
        packet.packetName=QStringLiteral("SAT-S0");
        packet.transport.satellite=QStringLiteral("ASRTU-1");
        packet.transport.bytes=QByteArray::fromHex("1acd");
        packet.transport.captureLocal=QDateTime::fromString(QStringLiteral("2026-10-30T00:00:00Z"),Qt::ISODate);
        orbit::EngineeringField field;
        field.nameCn=QStringLiteral("12V母线电压");field.code=QStringLiteral("SAT-C00042.7.16");
        field.value=QStringLiteral("12.345");field.unit=QStringLiteral("V");
        field.rawHex=QStringLiteral("1ACD");field.rawNumber=QStringLiteral("6861");
        field.status=QStringLiteral("table_formula");
        packet.fields.push_back(field);
        window.business_.push_back(packet);
        const QByteArray canonical=QJsonDocument(orbit::BusinessCatalog::toJson(packet)).toJson();
        QString error;
        QVERIFY2(window.updateWebHistory(&error),qPrintable(error));
        QFile history(QDir(window.satelliteStore_.rootDirectory()).filePath("index.html"));
        QVERIFY(history.open(QIODevice::ReadOnly));
        const QByteArray html=history.readAll();
        QVERIFY(html.contains("lang=\"ja-JP\""));
        QVERIFY(html.contains(QStringLiteral("テレメトリ履歴").toUtf8()));
        QVERIFY(html.contains(QStringLiteral("12Vバス電圧").toUtf8()));
        QVERIFY(html.contains("SAT-C00042.7.16"));
        QVERIFY(html.contains("12.345"));
        QVERIFY(!html.contains("__DATA_JSON__"));
        QCOMPARE(QJsonDocument(orbit::BusinessCatalog::toJson(window.business_.first())).toJson(),canonical);
        window.findChild<QAction*>("languageAction_zh-CN")->trigger();
    }
};
QTEST_MAIN(UiSmoke)
#include "ui_smoke.moc"
