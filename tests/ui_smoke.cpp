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
#include <QStandardPaths>

class UiSmoke:public QObject {
    Q_OBJECT
private slots:
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
        QCOMPARE(languageMenu->actions().size(),2);
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
        const auto *event=window.findChild<QTableWidget*>("eventTimeline");
        QVERIFY(event!=nullptr);
        QCOMPARE(event->columnCount(),4);
        QCOMPARE(event->horizontalHeaderItem(0)->text(),QStringLiteral("时间"));
        QCOMPARE(event->horizontalHeaderItem(1)->text(),QStringLiteral("数据包 / 事件"));
        QVERIFY(event->rowCount()==0);
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
