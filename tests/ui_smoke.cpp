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
#include <QPlainTextEdit>
#include <QGroupBox>
#include <QStandardPaths>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

class UiSmoke:public QObject {
    Q_OBJECT
private slots:
    void startupShowsChineseOverviewAndHidesDebug() {
        QStandardPaths::setTestModeEnabled(true);
        MainWindow window;
        QFile embeddedZh(QStringLiteral(":/i18n/zh-CN.json"));
        QVERIFY2(embeddedZh.open(QIODevice::ReadOnly),"Chinese i18n must be embedded in the application binary");
        const QJsonObject zhRoot=QJsonDocument::fromJson(embeddedZh.readAll()).object();
        QVERIFY(zhRoot.value(QStringLiteral("translations")).toObject().size()>=4000);
        QFile embeddedEn(QStringLiteral(":/i18n/en-US.json"));
        QVERIFY2(embeddedEn.open(QIODevice::ReadOnly),"English i18n must be embedded in the application binary");
        const QJsonObject enRoot=QJsonDocument::fromJson(embeddedEn.readAll()).object();
        QCOMPARE(zhRoot.value(QStringLiteral("translations")).toObject().size(),
                 enRoot.value(QStringLiteral("translations")).toObject().size());
        QVERIFY(enRoot.value(QStringLiteral("webTranslations")).toObject().size()>=70);
        const QJsonObject zhTranslations=zhRoot.value(QStringLiteral("translations")).toObject();
        const QJsonObject enTranslations=enRoot.value(QStringLiteral("translations")).toObject();
        QCOMPARE(zhTranslations.value(QStringLiteral("遥测轮询计数")).toString(),QStringLiteral("遥测轮询计数"));
        QCOMPARE(enTranslations.value(QStringLiteral("遥测轮询计数")).toString(),QStringLiteral("Telemetry Poll Count"));
        QCOMPARE(enTranslations.value(QStringLiteral("升交点赤经")).toString(),QStringLiteral("Right Ascension of the Ascending Node"));
        QCOMPARE(enTranslations.value(QStringLiteral("近地点角距")).toString(),QStringLiteral("Argument of Perigee"));
        QCOMPARE(zhTranslations.value(QStringLiteral("日志文件：%1\n数据帧文件：%2")).toString(),
                 QStringLiteral("日志文件：%1\n数据帧文件：%2"));
        QCOMPARE(enTranslations.value(QStringLiteral("日志文件：%1\n数据帧文件：%2")).toString(),
                 QStringLiteral("Log file: %1\nFrame file: %2"));
        QCOMPARE(enTranslations.value(QStringLiteral("日志与数据帧文件路径")).toString(),
                 QStringLiteral("LOG/BIN session input paths"));
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
        auto *rawTabs=window.findChild<QTabWidget*>("rawDataTabs");
        auto *rawHex=window.findChild<QPlainTextEdit*>("rawPacketHexView");
        QVERIFY(rawTabs!=nullptr);
        QVERIFY(rawHex!=nullptr);
        QVERIFY2(rawHex->isHidden(),"Raw KISS/HEX content must stay hidden while the Overview tab is active");
        tabs->setCurrentIndex(5); QApplication::processEvents();
        QVERIFY2(!rawHex->isHidden(),"Raw KISS/HEX view should be visible inside the Raw Data tab");
        tabs->setCurrentIndex(0); QApplication::processEvents();
        QVERIFY2(rawHex->isHidden(),"Raw KISS/HEX view must be hidden again when returning to Overview");
        QVERIFY(rawHex->toPlainText().isEmpty());
        auto *languageMenu=window.findChild<QMenu*>("languageMenu");
        QVERIFY(languageMenu!=nullptr);
        QCOMPARE(languageMenu->actions().size(),2);
        auto *zhAction=window.findChild<QAction*>("languageAction_zh-CN");
        auto *enAction=window.findChild<QAction*>("languageAction_en-US");
        QVERIFY(zhAction!=nullptr);
        QVERIFY(enAction!=nullptr);
        enAction->trigger();
        QApplication::processEvents();
        QVERIFY(enAction->isChecked());
        QVERIFY(!zhAction->isChecked());
        QVERIFY(window.windowTitle().contains(QStringLiteral("Satellite Telemetry Replay Workbench")));
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
};
QTEST_MAIN(UiSmoke)
#include "ui_smoke.moc"
