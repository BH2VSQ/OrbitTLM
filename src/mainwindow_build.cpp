#include "mainwindow.h"
#include "mainwindow_widgets.h"
#include <QAction>
#include <QApplication>
#include <QChart>
#include <QChartView>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDesktopServices>
#include <QCoreApplication>
#include <QUrl>
#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <functional>
#include <algorithm>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QLineSeries>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTimer>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace {
QLabel *subtitle(const QString &text,QWidget *parent=nullptr){
    auto *label=new QLabel(text,parent);
    label->setObjectName("subtitle");
    label->setWordWrap(true);
    return label;
}
QWidget *metricCard(const QString &label,QLabel *&number){
    auto *panel=new QFrame;
    panel->setObjectName("metricCard");
    auto *layout=new QVBoxLayout(panel);
    layout->setSpacing(4);layout->setContentsMargins(14,10,14,10);
    layout->addWidget(subtitle(label));
    number=new QLabel(QStringLiteral("—"));
    number->setObjectName("metricNumber");
    number->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(number);
    return panel;
}
QGroupBox *statusCard(const QString &title){
    auto *card=new QGroupBox(title);
    card->setObjectName("statusCard");
    auto *layout=new QVBoxLayout(card);
    layout->setContentsMargins(5,16,5,5);
    layout->setSpacing(0);
    auto *table=new QTableWidget(0,4,card);
    table->setObjectName("statusTable");
    table->setHorizontalHeaderLabels({QStringLiteral("参数"),QStringLiteral("当前值"),
                                      QStringLiteral("参数"),QStringLiteral("当前值")});
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->verticalHeader()->hide();
    table->verticalHeader()->setDefaultSectionSize(20);
    table->setShowGrid(true);
    table->setWordWrap(false);
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    table->horizontalHeader()->setSectionResizeMode(0,QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(2,QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(3,QHeaderView::Stretch);
    layout->addWidget(table,1);
    return card;
}
void setupTable(QTableWidget *table){
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->verticalHeader()->hide();
    table->setShowGrid(false);
    table->setAlternatingRowColors(false);
    table->horizontalHeader()->setStretchLastSection(true);
    table->setWordWrap(false);
}
}
MainWindow::MainWindow(QWidget *parent):QMainWindow(parent){
    setWindowTitle(QStringLiteral("OrbitTLM · 卫星遥测回放工作台"));
    setMinimumSize(1100,690);
    resize(1480,910);
    live_=new orbit::LiveTcp(this);
    applyStyles();
    buildToolbar();
    buildWorkspace();
    buildMenus();
    timer_=new QTimer(this);timer_->setInterval(500);
    connect(timer_,&QTimer::timeout,this,[this]{
        if(business_.isEmpty()){timer_->stop();return;}
        seekVisible(1);
    });
    connect(live_,&orbit::LiveTcp::status,this,[this](const QString &message){
        statusBar()->showMessage(QStringLiteral("TCP 状态：")+message,7500);
        if(!tcpStatusLabel_)return;
        if(message==QStringLiteral("Connected")){
            tcpStatusLabel_->setText(QStringLiteral("TCP：已连接"));
            tcpStatusLabel_->setStyleSheet(QStringLiteral("color:#15803d;font-weight:600;"));
        }else if(message.startsWith(QStringLiteral("正在连接"))){
            tcpStatusLabel_->setText(QStringLiteral("TCP：连接中"));
            tcpStatusLabel_->setStyleSheet(QStringLiteral("color:#b45309;font-weight:600;"));
        }else if(message.startsWith(QStringLiteral("连接失败"))){
            tcpStatusLabel_->setText(QStringLiteral("TCP：连接失败"));
            tcpStatusLabel_->setToolTip(message);
            tcpStatusLabel_->setStyleSheet(QStringLiteral("color:#b91c1c;font-weight:600;"));
        }else if(message==QStringLiteral("Disconnected")){
            if(tcpStatusLabel_->text()!=QStringLiteral("TCP：连接失败")){
                tcpStatusLabel_->setText(QStringLiteral("TCP：未连接"));
                tcpStatusLabel_->setStyleSheet(QString());
                tcpStatusLabel_->setToolTip(QString());
            }
        }
    });
    connect(live_,&orbit::LiveTcp::received,this,[this](const orbit::Frame &frame){
        bool inserted=false;
        QString storageError;
        const bool stored=satelliteStore_.append(frame,&inserted,&storageError);
        if(!stored){
            statusBar()->showMessage(QStringLiteral("SQLite 写入失败：%1").arg(storageError),10000);
        }else if(!inserted){
            statusBar()->showMessage(QStringLiteral("重复帧已去重：%1").arg(frame.satellite),3000);
            return;
        }
        for(const auto &existing:frames_){
            if(existing.satellite==frame.satellite&&existing.hash==frame.hash)return;
        }
        frames_.push_back(frame);
        std::stable_sort(frames_.begin(),frames_.end(),[](const orbit::Frame &left,const orbit::Frame &right){
            if(left.captureLocal.isValid()!=right.captureLocal.isValid())return left.captureLocal.isValid();
            if(left.captureLocal.isValid()&&right.captureLocal.isValid()&&left.captureLocal!=right.captureLocal)
                return left.captureLocal<right.captureLocal;
            if(left.satellite!=right.satellite)return left.satellite<right.satellite;
            return left.hash<right.hash;
        });
        rebuildSession();
        if(stored){
            statusBar()->showMessage(QStringLiteral("已保存至 %1.db（按时间排序，重复帧自动忽略）").arg(frame.satellite),5000);
        }
    });
    const QString dbFile=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)+
                         "/orbittlm.sqlite";
    QDir().mkpath(QFileInfo(dbFile).absolutePath());
    QString error;
    if(!db_.open(dbFile,&error))
        statusBar()->showMessage(QStringLiteral("SQLite 数据库无法打开：")+error,18000);
    error.clear();
    if(catalog_.loadDirectory(QApplication::applicationDirPath()+"/protocols/asrtu1",&error)){
        catalogReady_=true;
    }else{
        statusBar()->showMessage(QStringLiteral("缺少 ASRTU-1 遥测字段库：")+error,18000);
    }
    updatePaths();
    QString startupStoreError;
    frames_=satelliteStore_.loadAll(&startupStoreError);
    if(!startupStoreError.isEmpty())statusBar()->showMessage(QStringLiteral("读取 SQLite 遥测库失败：%1").arg(startupStoreError),10000);
    if(!frames_.isEmpty())rebuildSession();
    updateMetrics();
    updateWebHistory();
    showEvent(-1);
    setDiagnosticsVisible(false);
}
void MainWindow::applyStyles(){
    setStyleSheet(QStringLiteral(R"(
        QMainWindow { background:#f3f6fb; color:#182940; }
        QWidget { color:#1f3045; }
        QMenuBar { background:#ffffff; padding:5px; border-bottom:1px solid #e0e7f0; }
        QMenuBar::item:selected, QMenu::item:selected {background:#e7f1fd;}
        QMenu { background:#ffffff; border:1px solid #dbe3ee; }
        QToolBar { background:#ffffff; border:0; border-bottom:1px solid #e0e7f0;
                   spacing:9px; padding:9px 12px; }
        QToolButton, QPushButton {padding:7px 13px; border:1px solid #d6e0ec;
                  border-radius:7px; background:#ffffff; font-weight:500;}
        QToolButton:hover,QPushButton:hover {background:#edf5ff; border-color:#92bcf0;}
        QPushButton#primaryButton {background:#256cb4; border-color:#256cb4; color:white;}
        QPushButton#primaryButton:hover {background:#1f5b98;}
        QComboBox,QSpinBox,QLineEdit {background:white; border:1px solid #d4deeb;
                  border-radius:7px; padding:6px 9px; min-height:19px;}
        QLineEdit:focus,QComboBox:focus {border:1px solid #3c87d0;}
        QFrame#metricCard, QFrame#heroCard, QFrame#panel {
                  background:#ffffff; border:1px solid #e2e8f1; border-radius:10px;}
        QLabel#subtitle {color:#6d7f95; font-size:12px;}
        QLabel#metricNumber {font-size:20px; font-weight:700; color:#1d3b5e;}
        QLabel#currentHeading {font-size:21px; font-weight:700; color:#152d4c;}
        QLabel#detailHeading {font-size:16px; font-weight:700; color:#193958;}
        QLabel#note {color:#6b7f96; font-size:12px;}
        QTabWidget::pane {background:white; border:1px solid #e1e8f1;
                         border-radius:8px;}
        QTabBar::tab {background:#edf2f8; border:0; border-radius:4px;
                      margin-right:2px; padding:5px 8px; min-width:48px;}
        QTabBar::tab:selected {background:#dcecff; color:#18558f; font-weight:700;}
        QTabBar::tab:hover {background:#e3effb;}
        QTableWidget,QTreeWidget,QPlainTextEdit {background:white;
                     alternate-background-color:#f9fbff; border:1px solid #e1e8f1;
                     border-radius:6px; gridline-color:#edf2f6;}
        QHeaderView::section {background:#f5f8fc; border:none;
                              border-bottom:1px solid #e1e8f1;
                              color:#56718b; font-weight:600; padding:3px 4px;}
        QTableWidget::item {padding:1px 3px; border-bottom:1px solid #eff2f7;}
        QTableWidget::item:selected,QTreeWidget::item:selected {background:#e2f1ff;color:#183c64;}
        QGroupBox#statusCard {background:#ffffff; border:1px solid #e1e8f1;
                             border-radius:9px; margin-top:12px; font-weight:700;}
        QGroupBox#statusCard::title {subcontrol-origin:margin; left:14px;
                                    padding:0 6px;color:#245b8c;}
        QLabel#statusContent {font-weight:400; color:#38516c; line-height:150%; padding:2px;}
        QScrollArea#statusScroll {background:transparent;}
        QLabel#attitudeModeLabel {font-size:15px; font-weight:700; color:#174f83;
                                  background:#eaf4ff; border:1px solid #cfe4fb;
                                  border-radius:6px; padding:7px 10px;}
        QSplitter::handle {background:#edf1f7;}
        QStatusBar {background:#ffffff; color:#61768d; border-top:1px solid #e0e7f0;}
        QSlider::groove:horizontal {height:6px; background:#d9e4f0; border-radius:3px;}
        QSlider::handle:horizontal {width:13px;background:#337fc3;
                                  border:2px solid #ffffff; margin:-5px 0;border-radius:7px;}
    )"));
}
void MainWindow::buildToolbar(){
    auto *bar=addToolBar(QStringLiteral("遥测工作台"));
    bar->setMovable(false);
    auto *log=bar->addAction(QStringLiteral("导入 LOG"));
    connect(log,&QAction::triggered,this,[this]{
        const QString p=QFileDialog::getOpenFileName(this,QStringLiteral("选择解码日志"),{},
                                                     QStringLiteral("日志 (*.log *.txt);;全部文件 (*)"));
        if(!p.isEmpty()){logPath_=p;updatePaths();}
    });
    auto *bin=bar->addAction(QStringLiteral("导入 BIN"));
    connect(bin,&QAction::triggered,this,[this]{
        const QString p=QFileDialog::getOpenFileName(this,QStringLiteral("选择 223 字节帧文件"),{},
                                                     QStringLiteral("数据帧 (*.bin);;全部文件 (*)"));
        if(!p.isEmpty()){binPath_=p;updatePaths();}
    });
    bar->addSeparator();
    bar->addWidget(new QLabel(QStringLiteral("卫星  ")));
    satelliteSelect_=new QComboBox;
    satelliteSelect_->addItem(QStringLiteral("自动识别"),QStringLiteral("AUTO"));
    satelliteSelect_->addItem(QStringLiteral("阿斯图系列 · ASRTU-1"),QStringLiteral("ASRTU-1"));
    satelliteSelect_->addItem(QStringLiteral("BY70-4（BY04）"),QStringLiteral("BY04"));
    bar->addWidget(satelliteSelect_);
    auto *import=bar->addAction(QStringLiteral("开始导入"));
    connect(import,&QAction::triggered,this,[this]{importSession();});
    bar->addSeparator();
    auto *exportCsv=bar->addAction(QStringLiteral("导出遥测 CSV"));
    connect(exportCsv,&QAction::triggered,this,[this]{exportEngineering(true);});
    auto *exportQ=bar->addAction(QStringLiteral("导出四元数"));
    connect(exportQ,&QAction::triggered,this,[this]{exportQuaternions(false);});
    bar->addSeparator();
    auto *connectTcp=bar->addAction(QStringLiteral("连接 TCP"));
    tcpStatusLabel_=new QLabel(QStringLiteral("TCP：未连接"),bar);
    tcpStatusLabel_->setObjectName(QStringLiteral("tcpStatusLabel"));
    tcpStatusLabel_->setMinimumWidth(112);
    bar->addWidget(tcpStatusLabel_);
    connect(connectTcp,&QAction::triggered,this,[this]{
        bool ok=false;
        const QString address=QInputDialog::getText(this,QStringLiteral("TCP 实时输入"),
          QStringLiteral("主机地址"),QLineEdit::Normal,"127.0.0.1",&ok);
        if(!ok||address.trimmed().isEmpty())return;
        const int port=QInputDialog::getInt(this,QStringLiteral("TCP 实时输入"),
            QStringLiteral("端口"),9985,1,65535,1,&ok);
        if(ok)live_->start(address,quint16(port),QStringLiteral("AUTO"));
    });
    auto *stop=bar->addAction(QStringLiteral("断开"));
    connect(stop,&QAction::triggered,live_,&orbit::LiveTcp::stop);
    bar->addSeparator();
    auto *openWeb=bar->addAction(QStringLiteral("HTML 历史"));
    connect(openWeb,&QAction::triggered,this,[this]{
        updateWebHistory();
        const QString path=QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("data/index.html"));
        if(!QDesktopServices::openUrl(QUrl::fromLocalFile(path)))
            QMessageBox::warning(this,QStringLiteral("打开 HTML 历史失败"),path);
    });
}
void MainWindow::buildMenus(){
    auto *file=menuBar()->addMenu(QStringLiteral("文件(&F)"));
    file->addAction(QStringLiteral("打开 SQLite 历史会话"),[this]{loadHistory();});
    file->addAction(QStringLiteral("打开 HTML 遥测历史"),[this]{
        updateWebHistory();
        const QString path=QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("data/index.html"));
        if(!QDesktopServices::openUrl(QUrl::fromLocalFile(path)))
            QMessageBox::warning(this,QStringLiteral("打开 HTML 历史失败"),path);
    });
    file->addSeparator();
    file->addAction(QStringLiteral("导出传输帧 CSV"),[this]{exportFrames(true);});
    file->addAction(QStringLiteral("导出传输帧 JSON"),[this]{exportFrames(false);});
    file->addAction(QStringLiteral("导出业务遥测 CSV"),[this]{exportEngineering(true);});
    file->addAction(QStringLiteral("导出业务遥测 JSON"),[this]{exportEngineering(false);});
    file->addAction(QStringLiteral("导出 SAT-S3 四元数"),[this]{exportQuaternions(false);});
    file->addAction(QStringLiteral("导出实验性候选四元数"),[this]{exportQuaternions(true);});
    file->addAction(QStringLiteral("导出 KISS 应用包"),[this]{exportKiss();});
    auto *tools=menuBar()->addMenu(QStringLiteral("工具(&T)"));
    tools->addAction(QStringLiteral("载入自定义字段规则 JSON"),[this]{
        const QString p=QFileDialog::getOpenFileName(this,QStringLiteral("选择字段规则"),{},
                                                       QStringLiteral("JSON 文件 (*.json)"));
        if(p.isEmpty())return;
        orbit::Schema candidate;QString error;
        if(!candidate.load(p,&error)){QMessageBox::warning(this,QStringLiteral("字段规则错误"),error);return;}
        if(candidate.satellite()!=resolvedSatellite()){
            QMessageBox::warning(this,QStringLiteral("卫星不匹配"),QStringLiteral("该规则属于另一颗卫星"));return;
        }
        customSchema_=candidate;hasCustomSchema_=true;
        for(auto &f:frames_)customSchema_.apply(f);
        inspectFrame(0);
        statusBar()->showMessage(QStringLiteral("已载入独立字段规则，尚需验证字段物理含义"),9000);
    });
    auto *debug=tools->addAction(QStringLiteral("显示调试信息"));
    debug->setCheckable(true);
    connect(debug,&QAction::toggled,this,[this](bool active){setDiagnosticsVisible(active);});
}
void MainWindow::buildWorkspace(){
    auto *root=new QWidget;
    setCentralWidget(root);
    auto *main=new QVBoxLayout(root);
    main->setContentsMargins(12,12,12,9);
    main->setSpacing(9);
    auto *fileBar=new QFrame;
    fileBar->setObjectName("panel");
    auto *fileLayout=new QHBoxLayout(fileBar);
    fileLayout->setContentsMargins(15,9,15,9);
    sourceLabel_=new QLabel;
    sourceLabel_->setWordWrap(true);
    fileLayout->addWidget(sourceLabel_,1);
    message_=new QLabel(QStringLiteral("离线会话 · 应用层 CRC 未校验"));
    message_->setObjectName("note");
    fileLayout->addWidget(message_);
    main->addWidget(fileBar);
    auto *split=new QSplitter(Qt::Horizontal);
    split->setChildrenCollapsible(false);
    auto *left=new QFrame;
    left->setObjectName("panel");
    left->setMinimumWidth(460);
    auto *leftLayout=new QVBoxLayout(left);
    leftLayout->setContentsMargins(13,12,13,12);
    leftLayout->setSpacing(8);
    auto *header=new QLabel(QStringLiteral("数据回放列表"));
    header->setObjectName("detailHeading");
    leftLayout->addWidget(header);
    leftLayout->addWidget(subtitle(QStringLiteral("按记录时间浏览已重组的业务数据包")));
    eventSearch_=new QLineEdit;
    eventSearch_->setPlaceholderText(QStringLiteral("搜索时间、分系统或数据包"));
    leftLayout->addWidget(eventSearch_);
    recognizedOnly_=new QCheckBox(QStringLiteral("仅显示已识别数据"));
    leftLayout->addWidget(recognizedOnly_);
    eventTable_=new QTableWidget(0,4);
    eventTable_->setObjectName("eventTimeline");
    eventTable_->setHorizontalHeaderLabels({QStringLiteral("时间"),
        QStringLiteral("数据包 / 事件"),QStringLiteral("状态"),QStringLiteral("来源")});
    setupTable(eventTable_);
    eventTable_->horizontalHeader()->setSectionResizeMode(0,QHeaderView::ResizeToContents);
    eventTable_->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Interactive);
    eventTable_->setColumnWidth(1,230);
    eventTable_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    eventTable_->horizontalHeader()->setSectionResizeMode(2,QHeaderView::ResizeToContents);
    eventTable_->horizontalHeader()->setSectionResizeMode(3,QHeaderView::ResizeToContents);
    eventTable_->verticalHeader()->setDefaultSectionSize(43);
    leftLayout->addWidget(eventTable_,1);
    connect(eventTable_,&QTableWidget::itemSelectionChanged,this,[this]{
        const int row=eventTable_->currentRow();
        if(row>=0)selectEvent(row);
    });
    connect(eventSearch_,&QLineEdit::textChanged,this,[this]{applyEventFilter();});
    connect(recognizedOnly_,&QCheckBox::toggled,this,[this]{applyEventFilter();});
    split->addWidget(left);
    pages_=new QTabWidget;
    pages_->setObjectName("mainPages");
    pages_->setMinimumWidth(600);
    buildOverview(pages_);
    buildEngineering(pages_);
    buildAttitude(pages_);
    buildImages(pages_);
    buildRaw(pages_);
    buildDiagnostics(pages_);
    split->addWidget(pages_);
    split->setStretchFactor(0,0);
    split->setStretchFactor(1,1);
    split->setSizes({490,960});
    main->addWidget(split,1);
    auto *transport=new QFrame;
    transport->setObjectName("panel");
    auto *transportLayout=new QHBoxLayout(transport);
    transportLayout->setContentsMargins(12,7,12,7);
    auto *back=new QPushButton(QStringLiteral("上一条"));
    auto *play=new QPushButton(QStringLiteral("播放"));
    auto *pause=new QPushButton(QStringLiteral("暂停"));
    auto *next=new QPushButton(QStringLiteral("下一条"));
    transportLayout->addWidget(back);
    transportLayout->addWidget(play);
    transportLayout->addWidget(pause);
    transportLayout->addWidget(next);
    timeline_=new QSlider(Qt::Horizontal);
    timeline_->setRange(0,0);
    transportLayout->addWidget(timeline_,1);
    speedSelect_=new QComboBox;
    speedSelect_->addItems({"0.5×","1×","2×","4×","8×"});
    speedSelect_->setCurrentIndex(1);
    transportLayout->addWidget(speedSelect_);
    positionLabel_=new QLabel(QStringLiteral("0 / 0"));
    positionLabel_->setMinimumWidth(63);
    transportLayout->addWidget(positionLabel_);
    main->addWidget(transport);
    connect(back,&QPushButton::clicked,this,[this]{seekVisible(-1);});
    connect(next,&QPushButton::clicked,this,[this]{seekVisible(1);});
    connect(play,&QPushButton::clicked,this,[this]{if(!business_.isEmpty())timer_->start();});
    connect(pause,&QPushButton::clicked,this,[this]{timer_->stop();});
    connect(timeline_,&QSlider::valueChanged,this,[this](int i){selectEvent(i);});
    connect(speedSelect_,&QComboBox::currentIndexChanged,this,[this](int i){
        const int intervals[5]={1000,500,250,125,63};if(timer_)timer_->setInterval(intervals[i]);
    });
    statusBar()->showMessage(QStringLiteral("就绪 · 传输帧 FEC 证据与应用层 CRC 状态独立显示"));
}
void MainWindow::buildOverview(QTabWidget *tabs){
    auto *page=new QWidget;
    auto *layout=new QVBoxLayout(page);
    layout->setContentsMargins(5,5,5,5);
    layout->setSpacing(4);

    auto *hero=new QFrame;hero->setObjectName("heroCard");
    auto *heroLayout=new QHBoxLayout(hero);
    heroLayout->setContentsMargins(8,5,8,5);
    heroLayout->setSpacing(8);
    currentTitle_=new QLabel(QStringLiteral("当前数据包：未解析"));
    currentTitle_->setObjectName("currentHeading");
    currentTitle_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    heroLayout->addWidget(currentTitle_,1);
    attitudeModeLabel_=new QLabel(QStringLiteral("UTC累计秒：未解析"));
    attitudeModeLabel_->setObjectName("attitudeModeLabel");
    attitudeModeLabel_->setMinimumWidth(220);
    attitudeModeLabel_->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
    attitudeModeLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    heroLayout->addWidget(attitudeModeLabel_);
    layout->addWidget(hero,0);

    auto *cards=new QWidget;
    auto *cardsLayout=new QGridLayout(cards);
    cardsLayout->setContentsMargins(0,0,0,0);
    cardsLayout->setHorizontalSpacing(4);cardsLayout->setVerticalSpacing(4);
    powerCard_=statusCard(QStringLiteral("总线电压与电流"));
    attitudeCard_=statusCard(QStringLiteral("姿态与轨道"));
    thermalCard_=statusCard(QStringLiteral("各总线与部件温度"));
    payloadCard_=statusCard(QStringLiteral("部件开关状态"));
    for(auto *card:{powerCard_,attitudeCard_,thermalCard_,payloadCard_})
        overviewTables_.insert(card,card->findChild<QTableWidget*>("statusTable"));
    cardsLayout->addWidget(powerCard_,0,0);
    cardsLayout->addWidget(attitudeCard_,0,1);
    cardsLayout->addWidget(thermalCard_,1,0);
    cardsLayout->addWidget(payloadCard_,1,1);
    cardsLayout->setRowStretch(0,1);cardsLayout->setRowStretch(1,1);
    cardsLayout->setColumnStretch(0,1);cardsLayout->setColumnStretch(1,1);
    layout->addWidget(cards,1);
    tabs->addTab(page,QStringLiteral("总览"));
}

void MainWindow::buildEngineering(QTabWidget *tabs){
    auto *page=new QWidget;auto *layout=new QVBoxLayout(page);
    engineeringSummary_=subtitle(QStringLiteral("选择一条已识别的业务包，查看分系统字段与工程量"));
    layout->addWidget(engineeringSummary_);
    engineeringSearch_=new QLineEdit;
    engineeringSearch_->setPlaceholderText(QStringLiteral("筛选当前分包字段名或编号"));
    layout->addWidget(engineeringSearch_);
    auto *split=new QSplitter(Qt::Horizontal);
    engineeringTable_=new QTableWidget(0,3);
    engineeringTable_->setHorizontalHeaderLabels({QStringLiteral("时间"),QStringLiteral("分系统"),QStringLiteral("字段数")});
    setupTable(engineeringTable_);
    engineeringTable_->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Stretch);
    engineeringTable_->horizontalHeader()->setSectionResizeMode(0,QHeaderView::ResizeToContents);
    engineeringTable_->verticalHeader()->setDefaultSectionSize(34);
    split->addWidget(engineeringTable_);
    auto *right=new QWidget;auto *rightLayout=new QVBoxLayout(right);
    rightLayout->setContentsMargins(5,2,5,2);
    fieldTree_=new QTreeWidget;
    fieldTree_->setHeaderLabels({QStringLiteral("参数"),QStringLiteral("数值"),QStringLiteral("单位"),QStringLiteral("解析状态")});
    fieldTree_->setColumnHidden(3,true);
    fieldTree_->setColumnWidth(0,245);
    fieldTree_->setColumnWidth(1,132);
    fieldTree_->setRootIsDecorated(true);
    rightLayout->addWidget(fieldTree_,3);
    fieldDetail_=subtitle(QStringLiteral("选择参数查看原始 HEX、字段位置与换算公式"));
    fieldDetail_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    rightLayout->addWidget(fieldDetail_);
    engineeringChart_=new QChartView(new QChart);
    engineeringChart_->setRenderHint(QPainter::Antialiasing);
    engineeringChart_->setMinimumHeight(165);
    rightLayout->addWidget(engineeringChart_,2);
    split->addWidget(right);
    split->setStretchFactor(1,1);split->setSizes({230,700});
    layout->addWidget(split,1);
    connect(engineeringTable_,&QTableWidget::itemSelectionChanged,this,[this]{
        const auto *item=engineeringTable_->item(engineeringTable_->currentRow(),0);
        if(item){
            const int idx=item->data(Qt::UserRole).toInt();
            if(idx>=0)selectEvent(idx);
        }
    });
    connect(fieldTree_,&QTreeWidget::itemSelectionChanged,this,[this]{
        const auto chosen=fieldTree_->selectedItems();
        if(chosen.isEmpty())return;
        const auto *item=chosen.first();
        if(item->data(0,Qt::UserRole).isNull())return;
        const QString code=item->data(0,Qt::UserRole).toString();
        const QString packet=item->data(0,Qt::UserRole+1).toString();
        fieldDetail_->setText(QStringLiteral("字段编号 %1 · 位位置 %2 · 换算公式 %3\n英文名：%4\n定义来源：%5\n当前会话尚未找到该字段的已解码样本。")
            .arg(code,item->data(0,Qt::UserRole+2).toString(),item->data(0,Qt::UserRole+3).toString(),
                 item->data(0,Qt::UserRole+4).toString(),item->data(0,Qt::UserRole+5).toString()));
        bool hasSample=false;
        for(int i=0;i<business_.size();++i)if(i<=currentIndex_ && business_[i].packetName==packet)
            for(const auto &f:business_[i].fields)if(f.code==code){
                fieldDetail_->setText(QStringLiteral("字段编号 %1 · 位位置 %2\n原始 HEX：%3 · 原始值：%4\n当前工程值：%5 %6\n换算公式：%7\n定义来源：%8")
                    .arg(f.code,f.bits,f.rawHex,f.rawNumber,f.value,f.unit,f.formula,f.definitionEvidence));
                hasSample=true;
                break;
            }
        Q_UNUSED(hasSample);
        plotEngineering(packet,code,currentIndex_);
    });
    connect(engineeringSearch_,&QLineEdit::textChanged,this,[this](const QString &needle){
        if(!fieldTree_)return;
        std::function<bool(QTreeWidgetItem*)> filter=[&](QTreeWidgetItem *node){
            bool childVisible=false;
            for(int i=0;i<node->childCount();++i)
                if(filter(node->child(i)))childVisible=true;
            const bool selfMatch=needle.isEmpty()||node->text(0).contains(needle,Qt::CaseInsensitive)||
                node->data(0,Qt::UserRole).toString().contains(needle,Qt::CaseInsensitive);
            const bool visible=selfMatch||childVisible;
            node->setHidden(!visible);
            return visible;
        };
        for(int i=0;i<fieldTree_->topLevelItemCount();++i)filter(fieldTree_->topLevelItem(i));
    });
    tabs->addTab(page,QStringLiteral("业务遥测"));
    buildComponentTabs(tabs);
}
void MainWindow::buildComponentTabs(QTabWidget *tabs){
    auto *page=new QWidget;
    auto *layout=new QVBoxLayout(page);
    layout->setContentsMargins(4,4,4,4);
    layout->setSpacing(3);

    // Two visible tab levels: subsystem group first, official telemetry packet name second.
    // The JSON metadata is derived only from official tree labels and field identifiers;
    // sampled f_text/f_double values are deliberately not copied into the catalog.
    auto *systems=new AutoWrapTabWidget(page);
    systems->setObjectName("officialSystemTabs");
    systems->setDocumentMode(true);

    const QString catalogRoot=QDir(QCoreApplication::applicationDirPath())
                                  .filePath(QStringLiteral("telemetry-trees"));
    for(const QString &treeSatellite : {QStringLiteral("ASRTU-1"),QStringLiteral("BY04")}){
        const QString catalogName=treeSatellite+QStringLiteral(".json");
        QFile file(QDir(catalogRoot).filePath(catalogName));
        if(!file.open(QIODevice::ReadOnly)){
            file.setFileName(QStringLiteral(":/telemetry-trees/")+catalogName);
            if(!file.open(QIODevice::ReadOnly)){
                auto *errorPage=new QWidget;
                errorPage->setProperty("treeSatellite",treeSatellite);
                auto *errorLayout=new QVBoxLayout(errorPage);
                errorLayout->addWidget(new QLabel(QStringLiteral("官方遥测字段目录无法读取：%1").arg(catalogName)));
                const int errorTab=systems->addTab(errorPage,QStringLiteral("目录错误"));
                systems->setTabToolTip(errorTab,treeSatellite+QStringLiteral(" · 目录错误"));
                continue;
            }
        }
        QJsonParseError parseError;
        const QByteArray catalogBytes=file.readAll();
        file.close();
        const QJsonDocument document=QJsonDocument::fromJson(catalogBytes,&parseError);
        const QJsonArray packetArray=document.object().value(QStringLiteral("packets")).toArray();
        if(!document.isObject()||packetArray.isEmpty()){
            auto *errorPage=new QWidget;
            errorPage->setProperty("treeSatellite",treeSatellite);
            auto *errorLayout=new QVBoxLayout(errorPage);
            errorLayout->addWidget(new QLabel(QStringLiteral("遥测目录格式错误：%1（%2）")
                .arg(catalogName,parseError.errorString())));
            const int errorTab=systems->addTab(errorPage,QStringLiteral("目录错误"));
            systems->setTabToolTip(errorTab,treeSatellite+QStringLiteral(" · 目录错误"));
            continue;
        }

        QStringList groupOrder;
        QHash<QString,int> groupIndices;
        QVector<QVector<QJsonObject>> packetsByGroup;
        for(const auto &packetValue:packetArray){
            if(!packetValue.isObject())continue;
            const QJsonObject packet=packetValue.toObject();
            const QString groupName=packet.value(QStringLiteral("group")).toString(
                                        QStringLiteral("其他分系统"));
            if(!groupIndices.contains(groupName)){
                groupIndices.insert(groupName,groupOrder.size());
                groupOrder.push_back(groupName);
                packetsByGroup.push_back(QVector<QJsonObject>());
            }
            packetsByGroup[groupIndices.value(groupName)].push_back(packet);
        }

        for(int groupIndex=0;groupIndex<groupOrder.size();++groupIndex){
            const QString groupName=groupOrder[groupIndex];
            auto *groupPage=new QWidget;
            groupPage->setProperty("treeSatellite",treeSatellite);
            auto *groupLayout=new QVBoxLayout(groupPage);
            groupLayout->setContentsMargins(2,2,2,2);
            groupLayout->setSpacing(2);

            auto *packetTabs=new AutoWrapTabWidget(groupPage);
            packetTabs->setObjectName("officialPacketTabs");
            packetTabs->setDocumentMode(true);

            for(const QJsonObject &packet:packetsByGroup[groupIndex]){
                const QString officialName=packet.value(QStringLiteral("official_name")).toString();
                const QString packetCode=packet.value(QStringLiteral("packet_code")).toString();
                const QJsonArray fields=packet.value(QStringLiteral("fields")).toArray();

                auto *table=new QTableWidget(fields.isEmpty()?1:fields.size(),4,packetTabs);
                table->setObjectName("officialComponentTable");
                table->setProperty("treeSatellite",treeSatellite);
                table->setProperty("officialPacketCode",packetCode);
                table->setProperty("officialPacketName",officialName);
                table->setHorizontalHeaderLabels({QStringLiteral("字段名称"),
                    QStringLiteral("完整字段编号"),QStringLiteral("当前值"),QStringLiteral("接收时间")});
                setupTable(table);
                table->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
                table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
                table->verticalHeader()->setDefaultSectionSize(22);
                table->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Interactive);
                table->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Interactive);
                table->horizontalHeader()->setSectionResizeMode(2,QHeaderView::Stretch);
                table->horizontalHeader()->setSectionResizeMode(3,QHeaderView::ResizeToContents);
                table->setColumnWidth(0,250);
                table->setColumnWidth(1,190);

                if(fields.isEmpty()){
                    auto *nameItem=new QTableWidgetItem(QStringLiteral("附件中未提供该包的字段条目"));
                    nameItem->setData(Qt::UserRole+2,true);
                    nameItem->setToolTip(QStringLiteral("保留官方数据包位置；附件中的对应 JSON data 对象为空，未臆造字段。"));
                    table->setItem(0,0,nameItem);
                    table->setItem(0,1,new QTableWidgetItem(QString()));
                    table->setItem(0,2,new QTableWidgetItem(QStringLiteral("未解析")));
                    table->setItem(0,3,new QTableWidgetItem(QString()));
                }else{
                    for(int row=0;row<fields.size();++row){
                        const QJsonObject field=fields.at(row).toObject();
                        const QString fieldName=field.value(QStringLiteral("name")).toString();
                        const QString fullCode=field.value(QStringLiteral("code")).toString();
                        const QString englishName=field.value(QStringLiteral("english_name")).toString();
                        auto *nameItem=new QTableWidgetItem(fieldName);
                        nameItem->setData(Qt::UserRole,fullCode);
                        nameItem->setData(Qt::UserRole+1,englishName);
                        nameItem->setToolTip(QStringLiteral("官方字段：%1\n英文名：%2")
                                               .arg(fullCode,englishName));
                        table->setItem(row,0,nameItem);
                        table->setItem(row,1,new QTableWidgetItem(fullCode));
                        table->setItem(row,2,new QTableWidgetItem(QStringLiteral("未解析")));
                        table->setItem(row,3,new QTableWidgetItem(QString()));
                    }
                }
                packetTabs->addTab(table,officialName);
                componentTables_.insert(treeSatellite+QLatin1Char('|')+packetCode,table);
            }
            groupLayout->addWidget(packetTabs,1);
            const int systemTab=systems->addTab(groupPage,groupName);
            systems->setTabToolTip(systemTab,treeSatellite+QStringLiteral(" · ")+groupName);
        }
    }

    layout->addWidget(systems,1);
    tabs->addTab(page,QStringLiteral("分系统数据"));
    if(satelliteSelect_){
        connect(satelliteSelect_,&QComboBox::currentTextChanged,this,
                [this]{updateComponentTabs(currentIndex_);populateEngineering(currentIndex_);updateOverview(currentIndex_);updateAttitude(currentIndex_);});
    }
}
void MainWindow::buildAttitude(QTabWidget *tabs){
    auto *page=new QWidget;auto *layout=new QVBoxLayout(page);
    attitudeInfo_=subtitle(QStringLiteral("仅展示 SAT-S3 已识别字段；四元数参考系与应用层 CRC 未验证"));
    layout->addWidget(attitudeInfo_);
    auto *quats=new QWidget;auto *grid=new QGridLayout(quats);
    grid->setContentsMargins(0,0,0,0);
    for(int i=0;i<4;++i){
        QLabel *number=nullptr;
        grid->addWidget(metricCard(QString("q%1").arg(i),number),0,i);
        quaternion_[i]=number;
    }
    QLabel *n=nullptr;
    grid->addWidget(metricCard(QStringLiteral("四元数模长"),n),1,0,1,2);
    normLabel_=n;
    auto *explanation=new QLabel(QStringLiteral("SAT-S3：有符号 32 位整数 / 10000\n仅作数值与相对姿态展示，未定义绝对旋转参考系"));
    explanation->setWordWrap(true);
    grid->addWidget(explanation,1,2,1,2);
    const QStringList axes={QStringLiteral("X"),QStringLiteral("Y"),QStringLiteral("Z")};
    for(int i=0;i<3;++i){
        QLabel *rate=nullptr;
        grid->addWidget(metricCard(QStringLiteral("角速度 %1").arg(axes[i]),rate),2,i);
        angularVelocity_[i]=rate;
    }
    layout->addWidget(quats);
    auto *views=new QSplitter(Qt::Horizontal);
    attitudeWidget_=new AttitudeWidget;
    views->addWidget(attitudeWidget_);
    attitudeChart_=new QChartView(new QChart);
    attitudeChart_->setRenderHint(QPainter::Antialiasing);
    views->addWidget(attitudeChart_);
    views->setSizes({380,650});
    layout->addWidget(views,1);
    tabs->addTab(page,QStringLiteral("姿态与轨道"));
}
void MainWindow::buildImages(QTabWidget *tabs){
    auto *page=new QWidget;auto *layout=new QVBoxLayout(page);
    imageInfo_=subtitle(QStringLiteral("SSDV 图片恢复：候选图像数据不等价于 CRC 验证成功"));
    layout->addWidget(imageInfo_);
    auto *row=new QHBoxLayout;
    row->addWidget(new QLabel(QStringLiteral("图像业务 APID（BY04 固定 VC1）")));
    imageApid_=new QSpinBox;
    imageApid_->setRange(0,2047);imageApid_->setValue(240);
    row->addWidget(imageApid_);
    auto *scan=new QPushButton(QStringLiteral("扫描候选分片"));
    row->addWidget(scan);
    auto *save=new QPushButton(QStringLiteral("保存 SSDV 数据"));
    row->addWidget(save);
    auto *decode=new QPushButton(QStringLiteral("重建 JPEG"));
    row->addWidget(decode);
    row->addStretch(1);
    layout->addLayout(row);
    imageCoverageNote_=subtitle(QStringLiteral("总分片数未知 · 无法推断整幅图片完成度"));
    layout->addWidget(imageCoverageNote_);
    imageCoverage_=new QProgressBar;
    imageCoverage_->setRange(0,1);
    imageCoverage_->setValue(0);
    imageCoverage_->setFormat(QStringLiteral("尚无观测序号"));
    layout->addWidget(imageCoverage_);
    auto *split=new QSplitter(Qt::Horizontal);
    imagePreview_=new ImagePreview;
    split->addWidget(imagePreview_);
    imageTable_=new QTableWidget(0,4);
    imageTable_->setHorizontalHeaderLabels({QStringLiteral("图像 ID"),
        QStringLiteral("包序号"),QStringLiteral("宽 × 高"),QStringLiteral("校验状态")});
    setupTable(imageTable_);
    imageTable_->horizontalHeader()->setStretchLastSection(true);
    split->addWidget(imageTable_);
    split->setSizes({550,430});
    layout->addWidget(split,1);
    connect(scan,&QPushButton::clicked,this,[this]{scanSsdv();});
    connect(imageTable_,&QTableWidget::itemSelectionChanged,this,[this]{
        const int idx=imageTable_->currentRow();
        if(idx>=0&&idx<images_.size())updateImageProgress(images_[idx].imageId);
    });
    connect(save,&QPushButton::clicked,this,[this]{exportSsdv();});
    connect(decode,&QPushButton::clicked,this,[this]{decodeSsdv();});
    tabs->addTab(page,QStringLiteral("图像数据"));
}
void MainWindow::buildRaw(QTabWidget *tabs){
    auto *page=new QTabWidget;
    rawHex_=new QPlainTextEdit;
    rawHex_->setReadOnly(true);
    rawHex_->setLineWrapMode(QPlainTextEdit::NoWrap);
    rawHex_->setPlainText(QStringLiteral("选择回放数据包后显示原始传输字节。"));
    page->addTab(rawHex_,QStringLiteral("KISS / HEX"));
    packetTable_=new QTableWidget(0,6);
    packetTable_->setHorizontalHeaderLabels({QStringLiteral("VCID"),
        QStringLiteral("帧号范围"),QStringLiteral("字节数"),QStringLiteral("候选 APID"),
        QStringLiteral("FEC 证据"),QStringLiteral("完整性")});
    setupTable(packetTable_);
    packetTable_->horizontalHeader()->setStretchLastSection(true);
    page->addTab(packetTable_,QStringLiteral("分包明细"));
    tabs->addTab(page,QStringLiteral("原始数据"));
}
void MainWindow::buildDiagnostics(QTabWidget *tabs){
    auto *page=new QWidget;auto *layout=new QVBoxLayout(page);
    diagnosticsSummary_=subtitle(QStringLiteral("底层帧列表、重组诊断、错误来源及协议调试信息"));
    layout->addWidget(diagnosticsSummary_);
    auto *split=new QSplitter(Qt::Vertical);
    frameTable_=new QTableWidget(0,7);
    frameTable_->setHorizontalHeaderLabels({QStringLiteral("接收时间"),
        QStringLiteral("MFC"),"VCID","APID",QStringLiteral("序号"),
        QStringLiteral("来源 / FEC 证据"),QStringLiteral("结构状态")});
    setupTable(frameTable_);
    frameTable_->horizontalHeader()->setStretchLastSection(true);
    split->addWidget(frameTable_);
    debugText_=new QPlainTextEdit;
    debugText_->setReadOnly(true);
    debugText_->setLineWrapMode(QPlainTextEdit::NoWrap);
    split->addWidget(debugText_);
    split->setSizes({420,210});
    layout->addWidget(split);
    connect(frameTable_,&QTableWidget::itemSelectionChanged,this,[this]{
        inspectFrame(frameTable_->currentRow());
    });
    diagnosticTab_=tabs->addTab(page,QStringLiteral("调试信息"));
}
