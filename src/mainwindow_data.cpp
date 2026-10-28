#include "mainwindow.h"
#include "mainwindow_widgets.h"
#include "by04.h"
#include <QApplication>
#include <QCoreApplication>
#include <QImage>
#include <QDesktopServices>
#include <QUrl>
#include <QChart>
#include <QChartView>
#include <QCheckBox>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QGroupBox>
#include <QColor>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QLineSeries>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProgressBar>
#include <QSet>
#include <QQuaternion>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <limits>

using orbit::BusinessPacket;
using orbit::EngineeringField;
namespace {
QString categoryFor(const QString &name){
    if(name.contains(QStringLiteral("时间"))||name.contains(QStringLiteral("时钟"))||
       name.contains(QStringLiteral("计数")))return QStringLiteral("时间与计数");
    if(name.contains(QStringLiteral("四元数"))||name.contains(QStringLiteral("姿态"))||
       name.contains(QStringLiteral("角速度"))||name.contains(QStringLiteral("飞轮"))||
       name.contains(QStringLiteral("磁场")))return QStringLiteral("姿态与测量");
    if(name.contains(QStringLiteral("温度"))||name.contains(QStringLiteral("热")))return QStringLiteral("温度与热控");
    if(name.contains(QStringLiteral("电压"))||name.contains(QStringLiteral("电流"))||
       name.contains(QStringLiteral("电源"))||name.contains(QStringLiteral("充电")))return QStringLiteral("电源与配电");
    if(name.contains(QStringLiteral("状态"))||name.contains(QStringLiteral("模式"))||
       name.contains(QStringLiteral("开关")))return QStringLiteral("状态与模式");
    return QStringLiteral("其他工程量");
}
QString shortTime(const QDateTime &time){
    return time.isValid()?time.toString("HH:mm:ss.zzz"):QStringLiteral("时间未知");
}
QString labelStatus(const QString &s){
    if(s=="table_formula"||s=="table_enum"||s=="table_string"||s=="table_raw_hex")
        return QStringLiteral("字段表解析");
    if(s=="table_enum_unknown_value")return QStringLiteral("未知枚举值");
    if(s=="invalid_source_bit_position")return QStringLiteral("原始字段定义异常");
    if(s=="raw_unsupported_formula"||s.startsWith("raw_"))return QStringLiteral("仅原始值");
    if(s=="candidate_unverified")return QStringLiteral("实验性候选");
    if(s=="protocol_header_only")return QStringLiteral("CCSDS 协议字段");
    if(s=="raw_unverified_business")return QStringLiteral("业务含义未定义");
    return s.isEmpty()?QStringLiteral("未知"):s;
}
QTableWidgetItem *item(const QString &text,const QString &tooltip={}){
    auto *i=new QTableWidgetItem(text);
    if(!tooltip.isEmpty())i->setToolTip(tooltip);
    return i;
}
bool containsAny(const QString &text,const QStringList &keywords){
    for(const QString &keyword:keywords)if(!keyword.isEmpty()&&text.contains(keyword,Qt::CaseInsensitive))return true;
    return false;
}
bool startsWithAny(const QString &text,const QStringList &prefixes){
    for(const QString &prefix:prefixes)if(!prefix.isEmpty()&&text.startsWith(prefix,Qt::CaseInsensitive))return true;
    return false;
}
bool excludedLimitField(const QString &name){
    return name.contains(QStringLiteral("上限"))||name.contains(QStringLiteral("下限"))||
           name.contains(QStringLiteral("阈值"))||name.contains(QStringLiteral("目标温度"));
}
QString fieldStatusText(const QString &status){
    if(status=="table_formula"||status=="table_enum"||status=="table_string"||status=="table_raw_hex")return QStringLiteral("字段表解析");
    if(status=="candidate_unverified")return QStringLiteral("候选未验证");
    if(status=="invalid_source_bit_position")return QStringLiteral("字段定义异常");
    if(status.startsWith("raw_"))return QStringLiteral("原始值，含义未验证");
    if(status=="table_enum_unknown_value")return QStringLiteral("未知枚举值");
    return status.isEmpty()?QStringLiteral("未解析"):status;
}
QString telemetryValue(const EngineeringField &field,const QString &value,const QString &unit){
    QString shown=value;
    const bool switchField=field.nameCn.contains(QStringLiteral("开关"))||
        field.nameCn.contains(QStringLiteral("使能"))||field.nameCn.contains(QStringLiteral("状态"));
    bool widthOk=false;
    const int width=field.bits.section('.',2,2).toInt(&widthOk);
    if(switchField&&widthOk&&width==1&&field.status==QStringLiteral("raw_bits")){
        if(field.rawNumber==QStringLiteral("0"))shown=QStringLiteral("关");
        else if(field.rawNumber==QStringLiteral("1"))shown=QStringLiteral("开");
    }
    if(!unit.isEmpty()&&unit!=QStringLiteral("1"))shown+=QLatin1Char(' ')+unit;
    return shown;
}
int telemetrySwitchState(const EngineeringField &field,const QString &value){
    const bool switchField=field.nameCn.contains(QStringLiteral("开关"))||
        field.nameCn.contains(QStringLiteral("使能"))||field.nameCn.contains(QStringLiteral("状态"));
    if(!switchField)return -1;
    if(value.contains(QStringLiteral("开")))return 1;
    if(value.contains(QStringLiteral("关")))return 0;
    bool widthOk=false;
    const int width=field.bits.section('.',2,2).toInt(&widthOk);
    if(widthOk&&width==1&&field.status==QStringLiteral("raw_bits")){
        if(field.rawNumber==QStringLiteral("1"))return 1;
        if(field.rawNumber==QStringLiteral("0"))return 0;
    }
    return -1;
}
}
QString MainWindow::satellite() const{
    return satelliteSelect_?satelliteSelect_->currentData().toString():QStringLiteral("ASRTU-1");
}
QString MainWindow::resolvedSatellite() const{
    const QString selected=satellite();
    if(selected!=QStringLiteral("AUTO"))return selected;
    for(const auto &frame:frames_)
        if(!frame.satellite.isEmpty()&&frame.satellite!=QStringLiteral("AUTO"))return frame.satellite;
    for(const auto &packet:packets_)
        if(!packet.satellite.isEmpty()&&packet.satellite!=QStringLiteral("AUTO"))return packet.satellite;
    return QStringLiteral("AUTO");
}
QString MainWindow::qualityLabel(const QString &value) const{
    if(value=="log_fec_reported")return QStringLiteral("日志报告 FEC 成功");
    if(value=="mixed_fec_unverified")return QStringLiteral("混合来源·部分未经验证");
    if(value=="bin_only_unverified"||value=="structural_only")
        return QStringLiteral("仅结构恢复");
    if(value=="invalid")return QStringLiteral("帧结构异常");
    return value.isEmpty()?QStringLiteral("未验证"):value;
}
QString MainWindow::sourceLabel(const QStringList &sources) const{
    bool hasLog=false,hasBin=false,hasLive=false;
    for(const auto &s:sources){
        if(s.startsWith("log:"))hasLog=true;
        if(s.startsWith("bin:"))hasBin=true;
        if(s.startsWith("live:"))hasLive=true;
    }
    QStringList labels;
    if(hasLog)labels<<QStringLiteral("日志");
    if(hasBin)labels<<QStringLiteral("BIN");
    if(hasLive)labels<<QStringLiteral("TCP");
    return labels.isEmpty()?QStringLiteral("未知"):labels.join("+");
}
QString MainWindow::utcLabel(const QDateTime &time)const{
    return time.isValid()?time.toString("yyyy-MM-dd HH:mm:ss.zzz"):
                          QStringLiteral("接收时间未知");
}
QString MainWindow::eventTitle(const BusinessPacket &packet)const{
    if(packet.identification=="by04_ccsds_header_only")
        return packet.packetLabel;
    if(packet.identification=="by04_opaque_kiss")
        return QStringLiteral("BY04 · 未分类应用数据包");
    if(!packet.packetName.isEmpty()){
        const QString &id=packet.packetName;
        if(id.startsWith("SAT"))return QStringLiteral("卫星平台遥测 · ")+id;
        if(id.startsWith("ZGK"))return QStringLiteral("姿轨控遥测 · ")+id;
        if(id.startsWith("PDB"))return QStringLiteral("配电遥测 · ")+id;
        if(id.startsWith("TCB"))return QStringLiteral("热控遥测 · ")+id;
        if(id.startsWith("GPS"))return QStringLiteral("GNSS 遥测 · ")+id;
        if(id.startsWith("SSY")||id.startsWith("SSZ"))
            return QStringLiteral("星敏感器遥测 · ")+id;
        if(id.startsWith("CAM")||id.startsWith("CMS"))
            return QStringLiteral("相机遥测 · ")+id;
        if(id.startsWith("TTC"))return QStringLiteral("测控遥测 · ")+id;
        if(id.startsWith("BBB"))return QStringLiteral("数传遥测 · ")+id;
        return QStringLiteral("遥测业务包 · ")+id;
    }
    if(packet.mainType>=0&&packet.subType>=0){
        return QStringLiteral("未识别业务包（%1 / %2）")
            .arg(packet.mainType,2,16,QChar('0'))
            .arg(packet.subType,2,16,QChar('0')).toUpper();
    }
    return QStringLiteral("未识别 KISS 应用包");
}
const BusinessPacket *MainWindow::latestRecognized(int index,
                                                   const QStringList &prefixes)const{
    if(index<0)return nullptr;
    for(int i=std::min(index,int(business_.size())-1);i>=0;--i){
        const auto &p=business_[i];
        for(const auto &prefix:prefixes)
            if(p.packetName.startsWith(prefix)&&!p.fields.isEmpty())
                return &p;
    }
    return nullptr;
}
void MainWindow::updatePaths(){
    auto basename=[](const QString &p){return p.isEmpty()?QStringLiteral("未选择"):QFileInfo(p).fileName();};
    sourceLabel_->setText(QStringLiteral("解码日志：%1     数据帧文件：%2")
                              .arg(basename(logPath_),basename(binPath_)));
    sourceLabel_->setToolTip(QStringLiteral("LOG: %1\nBIN: %2").arg(logPath_,binPath_));
}
void MainWindow::importSession(){
    if(logPath_.isEmpty()&&binPath_.isEmpty()){
        QMessageBox::information(this,QStringLiteral("导入会话"),
                  QStringLiteral("请选择解码日志或 223 字节 BIN 文件。"));
        return;
    }
    QString error;
    const QString sat=satellite();
    auto log=logPath_.isEmpty()?QVector<orbit::Frame>{}:
        orbit::Protocol::fromLog(logPath_,sat,&error);
    if(!error.isEmpty()){
        QMessageBox::warning(this,QStringLiteral("日志读取失败"),error);return;
    }
    auto bin=binPath_.isEmpty()?QVector<orbit::Frame>{}:
        orbit::Protocol::fromBin(binPath_,sat,&error);
    if(!error.isEmpty()){
        QMessageBox::warning(this,QStringLiteral("二进制读取失败"),error);return;
    }
    frames_=orbit::Protocol::fuse(log,bin,&frameStats_);
    const QString actualSat=resolvedSatellite();
    if(hasCustomSchema_&&customSchema_.satellite()==actualSat)
        for(auto &f:frames_)customSchema_.apply(f);
    rebuildSession();
    if(!db_.save(actualSat+QStringLiteral(" · LOG/BIN 离线会话"),frames_,&error))
        statusBar()->showMessage(QStringLiteral("数据可回放，但未保存到 SQLite：")+error,12000);
    else statusBar()->showMessage(QStringLiteral("会话导入完成，原始传输帧及来源已保留"),8500);
}
void MainWindow::loadHistory(){
    QString error;
    const auto entries=db_.sessions(&error);
    if(!error.isEmpty()){QMessageBox::warning(this,QStringLiteral("SQLite 错误"),error);return;}
    if(entries.isEmpty()){
        QMessageBox::information(this,QStringLiteral("历史会话"),QStringLiteral("还没有已保存的会话。"));
        return;
    }
    QStringList choices;
    for(const auto &p:entries)choices<<QString("#%1 · %2").arg(p.first).arg(p.second);
    bool ok=false;
    const QString selected=QInputDialog::getItem(this,QStringLiteral("打开历史会话"),
        QStringLiteral("请选择会话"),choices,0,false,&ok);
    if(!ok||selected.isEmpty())return;
    const int i=choices.indexOf(selected);
    frames_=db_.load(entries[i].first,&error);
    if(!error.isEmpty()){QMessageBox::warning(this,QStringLiteral("历史会话读取失败"),error);return;}
    if(!frames_.isEmpty()){
        const QString sat=frames_.first().satellite;
        for(int j=0;j<satelliteSelect_->count();++j)
            if(satelliteSelect_->itemData(j).toString()==sat)satelliteSelect_->setCurrentIndex(j);
    }
    frameStats_={};
    frameStats_.unique=frames_.size();
    for(const auto &f:frames_){
        if(f.quality=="log_fec_reported")++frameStats_.trustedFromLog;
        else if(f.complete)++frameStats_.structuralOnly;
        else ++frameStats_.invalid;
    }
    rebuildSession();
    sourceLabel_->setText(QStringLiteral("历史会话：")+selected);
    statusBar()->showMessage(QStringLiteral("历史会话已恢复：")+selected,9000);
}
void MainWindow::rebuildSession(){
    timer_->stop();
    packets_=orbit::Transport::reassemble(frames_,&transportStats_);
    orbit::BusinessStats bs;
    // BY04 packets take an independent mission map and segmented-packet path.
    // The ASRTU-1 catalog is never used to decode BY04 APIDs or field offsets.
    business_=catalog_.decodeBySatellite(packets_,&bs);
    populateEvents();
    updateFrameList();
    scanSsdv();
    updateMetrics();
    timeline_->setRange(0,std::max(0,int(business_.size())-1));
    currentIndex_=-1;
    if(!business_.isEmpty())selectEvent(0);
    else showEvent(-1);
    updateWebHistory();
}
void MainWindow::populateEvents(){
    QSignalBlocker blocked(eventTable_);
    eventTable_->setRowCount(business_.size());
    for(int i=0;i<business_.size();++i){
        const auto &b=business_[i];
        const QString title=eventTitle(b);
        const QString basis=b.transport.timestampBasis;
        const QString source=sourceLabel(b.transport.sources);
        eventTable_->setItem(i,0,item(shortTime(b.transport.captureLocal),
            QStringLiteral("本机记录时间；尚未确认日志时区\n%1").arg(basis)));
        eventTable_->setItem(i,1,item(title,
            QStringLiteral("应用数据包字节：%1\nCRC：未校验").arg(b.transport.bytes.size())));
        const QString status=b.identification=="by04_ccsds_header_only"?
            QStringLiteral("CCSDS 包头已解析 · 未匹配业务字段表"):
            (b.packetName.isEmpty()?QStringLiteral("类型未知，原始数据已保留"):
                                    QStringLiteral("已匹配字段表"));
        auto *quality=item(status+" · "+(b.transport.fecEvidence=="log_fec_reported"?
                                         QStringLiteral("FEC 日志"):QStringLiteral("结构恢复")));
        quality->setForeground(b.packetName.isEmpty()?QColor("#73859a"):QColor("#216a9f"));
        eventTable_->setItem(i,2,quality);
        eventTable_->setItem(i,3,item(source));
    }
    applyEventFilter();
}
void MainWindow::applyEventFilter(){
    if(!eventTable_)return;
    const QString query=eventSearch_->text().trimmed();
    const bool recognized=recognizedOnly_->isChecked();
    for(int row=0;row<eventTable_->rowCount();++row){
        const bool matchKind=!recognized || !business_[row].packetName.isEmpty();
        bool matchText=query.isEmpty();
        for(int col=0;col<eventTable_->columnCount()&&!matchText;++col){
            auto *i=eventTable_->item(row,col);
            if(i&&i->text().contains(query,Qt::CaseInsensitive))matchText=true;
        }
        eventTable_->setRowHidden(row,!(matchKind&&matchText));
    }
}
void MainWindow::selectEvent(int index){
    if(index<0||index>=business_.size()){showEvent(-1);return;}
    currentIndex_=index;
    {
        QSignalBlocker block(eventTable_);
        eventTable_->selectRow(index);
    }
    {
        QSignalBlocker block(timeline_);
        timeline_->setValue(index);
    }
    showEvent(index);
}
void MainWindow::seekVisible(int direction){
    if(business_.isEmpty()){timer_->stop();return;}
    const int from=currentIndex_ < 0?(direction>0?-1:int(business_.size())):currentIndex_;
    for(int i=from+direction;i>=0&&i<business_.size();i+=direction){
        if(!eventTable_->isRowHidden(i)){selectEvent(i);return;}
    }
    timer_->stop();
}
void MainWindow::showEvent(int index){
    positionLabel_->setText(index<0?QStringLiteral("0 / %1").arg(business_.size()):
                          QString("%1 / %2").arg(index+1).arg(business_.size()));
    updateOverview(index);
    populateEngineering(index);
    updateAttitude(index);
    updateRawPacket(index);
    updateComponentTabs(index);
}
void MainWindow::updateMetrics(){
    QDateTime earliest,latest;
    for(const auto &f:frames_)if(f.captureLocal.isValid()){
        if(!earliest.isValid()||f.captureLocal<earliest)earliest=f.captureLocal;
        if(!latest.isValid()||f.captureLocal>latest)latest=f.captureLocal;
    }
    // The compact UI no longer creates the legacy metric-card labels. Keep
    // their updates optional so startup and live refreshes do not dereference
    // null QLabel pointers when those cards are absent.
    if(timeMetric_){
        if(earliest.isValid()){
            timeMetric_->setText(earliest.date()==latest.date()?
                earliest.toString("HH:mm")+" — "+latest.toString("HH:mm"):
                earliest.toString("MM-dd HH:mm")+"—"+latest.toString("MM-dd HH:mm"));
        }else timeMetric_->setText(QStringLiteral("未记录"));
    }
    if(frameMetric_)frameMetric_->setText(QString::number(frames_.size()));
    int good=0;
    for(const auto &p:business_)
        if(!p.packetName.isEmpty())++good;
    const bool by04=resolvedSatellite()==QStringLiteral("BY04");
    if(recognizedOnly_)recognizedOnly_->setText(QStringLiteral("仅显示已识别数据"));
    auto label=[](QLabel *value){
        return value&&value->parentWidget()?
            value->parentWidget()->findChild<QLabel*>("subtitle"):nullptr;
    };
    if(auto *caption=label(recognizedMetric_))
        caption->setText(by04?QStringLiteral("已解析 CCSDS 包头"):
                             QStringLiteral("已识别业务包"));
    if(auto *caption=label(unknownMetric_))
        caption->setText(by04?QStringLiteral("未分类 KISS 业务包"):
                             QStringLiteral("未识别业务包"));
    if(recognizedMetric_)recognizedMetric_->setText(QString::number(good));
    if(unknownMetric_)unknownMetric_->setText(QString::number(business_.size()-good));
    if(imageMetric_)imageMetric_->setText(QString::number(images_.size()));
    if(transportMetric_)transportMetric_->setText(QStringLiteral("%1 包").arg(packets_.size()));
    message_->setText(by04?
        QStringLiteral("BY04 · 应用包 %1 · 已匹配字段表 %2 · VC1 SSDV %3")
            .arg(packets_.size()).arg(good).arg(images_.size()):
        QStringLiteral("KISS 已重组：%1 · 应用 CRC 未校验 · 缺帧中断：%2")
            .arg(packets_.size()).arg(transportStats_.interruptedByCounterGap));
}
void MainWindow::updateOverview(int index){
    if(index<0||index>=business_.size()){
        currentTitle_->setText(QStringLiteral("当前数据包：未解析"));
        attitudeModeLabel_->setText(QStringLiteral("UTC累计秒：未解析"));
    }else{
        const auto &packet=business_[index];
        currentTitle_->setText(QStringLiteral("当前数据包：%1").arg(eventTitle(packet)));
        QString utc;
        bool candidateUtc=false;
        for(int i=index;i>=0&&utc.isEmpty();--i){
            for(const auto &field:business_[i].fields){
                if(field.nameCn.contains(QStringLiteral("UTC")) &&
                   field.nameCn.contains(QStringLiteral("累计秒")) &&
                   !field.value.isEmpty()&&!field.status.startsWith("raw_")){
                    utc=field.value;
                    candidateUtc=field.status==QStringLiteral("candidate_unverified");
                    if(!field.unit.isEmpty())utc+=QLatin1Char(' ')+field.unit;
                    break;
                }
            }
        }
        const QString suffix=candidateUtc?QStringLiteral("（候选未验证）"):QString();
        attitudeModeLabel_->setText(QStringLiteral("UTC累计秒：%1%2").arg(utc.isEmpty()?QStringLiteral("未解析"):utc,suffix));
    }
    updateFieldCard(powerCard_,{},index);
    updateFieldCard(attitudeCard_,{},index);
    updateFieldCard(thermalCard_,{},index);
    updateFieldCard(payloadCard_,{},index);
}
void MainWindow::updateFieldCard(QGroupBox *card,const QStringList &prefixes,int index){
    Q_UNUSED(prefixes);
    if(!card)return;
    auto *table=overviewTables_.value(card,card->findChild<QTableWidget*>("statusTable"));
    if(!table)return;
    const QString title=card->title();
    struct Row { QString name; QString packet; EngineeringField definition; bool manual=false; };
    QVector<Row> rows;
    QSet<QString> seen;
    auto add=[&](const QString &name,const QString &packet,const EngineeringField &field,bool manual=false){
        const QString key=name;
        if(name.isEmpty()||seen.contains(key))return;
        seen.insert(key);rows.push_back({name,packet,field,manual});
    };
    const auto definitions=catalog_.definitionPackets();
    if(resolvedSatellite()==QStringLiteral("BY04")){
        // Build BY04 overview rows from the fields actually decoded by its own
        // catalogs. Do not inherit ASRTU-1 packet names or field indices.
        for(int i=0;i<=index&&i<business_.size();++i){
            const auto &packet=business_[i];
            for(const auto &field:packet.fields){
                const QString &name=field.nameCn;
                const QString group=categoryFor(name);
                bool keep=false;
                if(card==powerCard_)keep=group==QStringLiteral("电源与配电");
                else if(card==attitudeCard_)keep=group==QStringLiteral("时间与计数")||
                    group==QStringLiteral("姿态与测量");
                else if(card==thermalCard_){
                    const bool rail=name.contains(QStringLiteral("12V"))||
                        name.contains(QStringLiteral("5V"))||name.contains(QStringLiteral("3.3V"));
                    const bool mppt=name.contains(QStringLiteral("MPPT"));
                    keep=name.contains(QStringLiteral("温度"))&&(rail||mppt)&&!excludedLimitField(name);
                }else if(card==payloadCard_){
                    keep=name.contains(QStringLiteral("开关"))||name.contains(QStringLiteral("状态"))||
                         name.contains(QStringLiteral("使能"));
                }
                if(keep)add(name,packet.packetName,field);
            }
        }
    }else if(title==QStringLiteral("总线电压与电流")){
        for(const auto &packet:definitions)for(const auto &field:packet.fields){
            const QString &name=field.nameCn;
            if((name.contains(QStringLiteral("母线"))||name.contains(QStringLiteral("总线"))) &&
               (name.contains(QStringLiteral("电压"))||name.contains(QStringLiteral("电流"))) &&
               !excludedLimitField(name))
                add(name,packet.packetName,field);
        }
    }else if(title==QStringLiteral("各总线与部件温度")){
        for(const auto &packet:definitions)for(const auto &field:packet.fields){
            const QString &name=field.nameCn;
            const bool rail=name.contains(QStringLiteral("12V"))||name.contains(QStringLiteral("5V"))||
                            name.contains(QStringLiteral("3.3V"));
            const bool mppt=name.contains(QStringLiteral("MPPT"));
            if(name.contains(QStringLiteral("温度"))&&(rail||mppt)&&!excludedLimitField(name))
                add(name,packet.packetName,field);
        }
    }else if(title==QStringLiteral("UTC、姿态四元数、GPS速度与轨道参数")){
        // Always reserve one row per requested overview datum, even when the
        // loaded source field table lacks a corresponding definition.
        const QStringList fixed={QStringLiteral("解算四元数q0"),QStringLiteral("解算四元数q1"),
            QStringLiteral("解算四元数q2"),QStringLiteral("解算四元数q3"),
            QStringLiteral("GPS速度X"),QStringLiteral("GPS速度Y"),QStringLiteral("GPS速度Z"),
            QStringLiteral("半长轴"),QStringLiteral("偏心率"),QStringLiteral("升交点赤经"),
            QStringLiteral("近地点角距"),QStringLiteral("轨道倾角")};
        for(const QString &name:fixed){
            bool added=false;
            for(const auto &packet:definitions)for(const auto &field:packet.fields){
                const QString &n=field.nameCn;
                bool match=n==name;
                if(name==QStringLiteral("GPS速度X"))match=match||n.contains(QStringLiteral("GNSS卫星速度X方向"));
                else if(name==QStringLiteral("GPS速度Y"))match=match||n.contains(QStringLiteral("GNSS卫星速度Y方向"));
                else if(name==QStringLiteral("GPS速度Z"))match=match||n.contains(QStringLiteral("GNSS卫星速度Z方向"));
                else if(name==QStringLiteral("半长轴"))match=match||n.contains(QStringLiteral("半长轴"));
                else if(name==QStringLiteral("偏心率"))match=match||n.contains(QStringLiteral("偏心率"));
                else if(name==QStringLiteral("升交点赤经"))match=match||n.contains(QStringLiteral("升交点赤经"));
                else if(name==QStringLiteral("近地点角距"))match=match||n.contains(QStringLiteral("近地点角距"));
                else if(name==QStringLiteral("轨道倾角"))match=match||n.contains(QStringLiteral("轨道倾角"));
                if(match){add(name,packet.packetName,field);added=true;break;}
            }
            if(!added)add(name,QString(),EngineeringField{name,{},name,{},{},{},{},{},{},QStringLiteral("catalog_definition")},true);
        }
    }else if(title==QStringLiteral("部件开关状态")){
        const QVector<QPair<QString,QStringList>> devices={
            {QStringLiteral("GNSS开关状态"),{QStringLiteral("GNSS")}},
            {QStringLiteral("数传通道开关状态"),{QStringLiteral("数传通道")}},
            {QStringLiteral("数传基带开关状态"),{QStringLiteral("数传基带")}},
            {QStringLiteral("太阳敏感器开关状态"),{QStringLiteral("太阳敏感器")}},
            {QStringLiteral("CMOS相机开关状态"),{QStringLiteral("CMOS")}},
            {QStringLiteral("高分相机开关状态"),{QStringLiteral("高分相机")}},
            {QStringLiteral("光纤陀螺开关状态"),{QStringLiteral("光纤陀螺")}},
            {QStringLiteral("MEMS惯组开关状态"),{QStringLiteral("MEMS惯组"),QStringLiteral("MEMS")}},
            {QStringLiteral("磁强计开关状态"),{QStringLiteral("磁强计")}},
            {QStringLiteral("星敏感器Y开关状态"),{QStringLiteral("星敏感器Y"),QStringLiteral("星敏Y")}},
            {QStringLiteral("星敏感器Z开关状态"),{QStringLiteral("星敏感器Z"),QStringLiteral("星敏Z")}}
        };
        for(const auto &device:devices)
            add(device.first,QString(),EngineeringField{device.first,{},device.first,{},{},{},{},{},{},QStringLiteral("catalog_definition")},true);
    }

    const int rowsPerColumn=(rows.size()+1)/2;
    table->setRowCount(rowsPerColumn);
    for(int r=0;r<rows.size();++r){
        const Row &row=rows[r];
        QString value=QStringLiteral("未解析"),unit,status=QStringLiteral("未解析"),when;
        EngineeringField resolvedField=row.definition;
        int from=qMin(index,int(business_.size())-1);
        for(int i=from;i>=0&&(value==QStringLiteral("未解析")||value.isEmpty());--i){
            const auto &packet=business_[i];
            if(!row.packet.isEmpty()&&packet.packetName!=row.packet)continue;
            for(const auto &field:packet.fields){
                bool matched=false;
                if(row.manual && title==QStringLiteral("部件开关状态")){
                    QStringList keys;
                    if(row.name.startsWith(QStringLiteral("GNSS")))keys={QStringLiteral("GNSS")};
                    else if(row.name.startsWith(QStringLiteral("数传通道")))keys={QStringLiteral("数传通道")};
                    else if(row.name.startsWith(QStringLiteral("数传基带")))keys={QStringLiteral("数传基带")};
                    else if(row.name.startsWith(QStringLiteral("太阳敏感器")))keys={QStringLiteral("太阳敏感器")};
                    else if(row.name.startsWith(QStringLiteral("CMOS")))keys={QStringLiteral("CMOS")};
                    else if(row.name.startsWith(QStringLiteral("高分相机")))keys={QStringLiteral("高分相机")};
                    else if(row.name.startsWith(QStringLiteral("光纤陀螺")))keys={QStringLiteral("光纤陀螺")};
                    else if(row.name.startsWith(QStringLiteral("MEMS")))keys={QStringLiteral("MEMS")};
                    else if(row.name.startsWith(QStringLiteral("磁强计")))keys={QStringLiteral("磁强计")};
                    else if(row.name.contains(QStringLiteral("星敏感器Y")))keys={QStringLiteral("星敏感器Y"),QStringLiteral("星敏Y")};
                    else if(row.name.contains(QStringLiteral("星敏感器Z")))keys={QStringLiteral("星敏感器Z"),QStringLiteral("星敏Z")};
                    matched=containsAny(field.nameCn,keys)&&(field.nameCn.contains(QStringLiteral("开关"))||field.nameCn.contains(QStringLiteral("状态"))||field.nameCn.contains(QStringLiteral("使能")));
                    if(row.name.startsWith(QStringLiteral("高分相机"))&&field.nameCn.contains(QStringLiteral("热刀")))matched=false;
                    if(field.nameCn.contains(QStringLiteral("最终指令")))matched=false;
                }else if(title==QStringLiteral("UTC、姿态四元数、GPS速度与轨道参数") && row.name.startsWith(QStringLiteral("解算四元数q"))){
                    const int q=row.name.right(1).toInt();
                    matched=field.nameCn==row.name||field.code==QStringLiteral("candidate.quaternion.q%1").arg(q);
                }else if(title==QStringLiteral("UTC、姿态四元数、GPS速度与轨道参数") && row.name.startsWith(QStringLiteral("GPS速度"))){
                    const QString axis=row.name.right(1);
                    matched=field.nameCn==row.name || field.nameCn.contains(row.name) ||
                        field.nameCn.contains(QStringLiteral("GNSS卫星速度")+axis+QStringLiteral("方向"));
                }else if(row.name.contains(QStringLiteral("UTC累计秒"))||row.name.contains(QStringLiteral("UTC时间累计秒"))){
                    matched=field.nameCn.contains(QStringLiteral("UTC"))&&field.nameCn.contains(QStringLiteral("累计秒"));
                }else if(title==QStringLiteral("UTC、姿态四元数、GPS速度与轨道参数") && row.name==QStringLiteral("半长轴")){
                    matched=field.nameCn.contains(QStringLiteral("半长轴"));
                }else if(title==QStringLiteral("UTC、姿态四元数、GPS速度与轨道参数") && row.name==QStringLiteral("偏心率")){
                    matched=field.nameCn.contains(QStringLiteral("偏心率"));
                }else if(title==QStringLiteral("UTC、姿态四元数、GPS速度与轨道参数") && row.name==QStringLiteral("升交点赤经")){
                    matched=field.nameCn.contains(QStringLiteral("升交点赤经"));
                }else if(title==QStringLiteral("UTC、姿态四元数、GPS速度与轨道参数") && row.name==QStringLiteral("近地点角距")){
                    matched=field.nameCn.contains(QStringLiteral("近地点角距"));
                }else if(title==QStringLiteral("UTC、姿态四元数、GPS速度与轨道参数") && row.name==QStringLiteral("轨道倾角")){
                    matched=field.nameCn.contains(QStringLiteral("轨道倾角"));
                }else{
                    matched=field.nameCn==row.name;
                }
                const bool allowMappedRaw=resolvedSatellite()==QStringLiteral("BY04")&&field.status==QStringLiteral("raw_bits");
                if(!matched||field.value.isEmpty()||
                   (field.status.startsWith("raw_")&&!allowMappedRaw)||field.status=="invalid_source_bit_position")continue;
                value=field.value;unit=field.unit;status=fieldStatusText(field.status);resolvedField=field;
                when=shortTime(packet.transport.captureLocal);break;
            }
            if(value!=QStringLiteral("未解析")&&!value.isEmpty())break;
        }
        auto *nameItem=item(row.name,row.packet.isEmpty()?QString():row.packet+QStringLiteral(" · ")+row.definition.code);
        auto *valueItem=item(telemetryValue(resolvedField,value,unit),status);
        const int rowIndex=r%rowsPerColumn;
        const int columnOffset=(r/rowsPerColumn)*2;
        table->setItem(rowIndex,columnOffset,nameItem);
        table->setItem(rowIndex,columnOffset+1,valueItem);
        if(title==QStringLiteral("部件开关状态")){
            const int switchState=telemetrySwitchState(resolvedField,value);
            if(switchState==1)valueItem->setForeground(QColor("#16803d"));
            else if(switchState==0)valueItem->setForeground(QColor("#c62828"));
        }
        if(value==QStringLiteral("未解析"))valueItem->setForeground(QColor("#64748b"));
        if(!when.isEmpty())valueItem->setToolTip(QStringLiteral("接收时间：%1").arg(when));
    }
    table->verticalHeader()->setDefaultSectionSize(20);
}
void MainWindow::updateComponentTabs(int index){
    if(!catalogReady_||componentTables_.isEmpty())return;
    const auto definitions=catalog_.definitionPackets();
    const bool by04=resolvedSatellite()==QStringLiteral("BY04");
    for(auto it=componentTables_.begin();it!=componentTables_.end();++it){
        const QString title=it.key();
        QTableWidget *table=it.value();
        const QStringList prefixes=componentPrefixes_.value(title);
        const QStringList keywords=componentKeywords_.value(title);
        struct Row { QString packet; EngineeringField field; };
        QVector<Row> rows;QSet<QString> seen;
        auto matches=[&](const QString &packet,const QString &name){
            if(by04){
                if(prefixes.isEmpty()&&keywords.isEmpty())return true;
                return containsAny(name,keywords);
            }
            if(prefixes.isEmpty()&&keywords.isEmpty())return true;
            return startsWithAny(packet,prefixes)||containsAny(name,keywords);
        };
        if(by04){
            // The BY04 table is generated exclusively from decoded BY04 fields.
            // ASRTU packet prefixes and byte offsets are intentionally ignored.
            for(int i=0;i<=index&&i<business_.size();++i){
                const auto &packet=business_[i];
                if(packet.transport.satellite!="BY04")continue;
                for(const auto &field:packet.fields){
                    if(field.nameCn.isEmpty()||!matches(packet.packetName,field.nameCn))continue;
                    const QString key=packet.packetName+QLatin1Char('|')+field.code;
                    if(seen.contains(key))continue;
                    seen.insert(key);rows.push_back({packet.packetName,field});
                }
            }
        }else{
        for(const auto &packet:definitions)for(const auto &field:packet.fields){
            if(!matches(packet.packetName,field.nameCn))continue;
            const QString key=packet.packetName+QLatin1Char('|')+field.code;
            if(seen.contains(key))continue;
            seen.insert(key);rows.push_back({packet.packetName,field});
        }
        // Include decoded fields unknown to the public catalog, such as the
        }
        // BY04 quaternion candidate, without removing any catalog placeholders.
        for(int i=0;i<=index&&i<business_.size();++i){
            const auto &packet=business_[i];
            for(const auto &field:packet.fields){
                if(field.nameCn.isEmpty()||field.status.startsWith("raw_")||!matches(packet.packetName,field.nameCn))continue;
                const QString key=packet.packetName+QLatin1Char('|')+field.code;
                if(seen.contains(key))continue;
                seen.insert(key);rows.push_back({packet.packetName,field});
            }
        }
        table->setRowCount(rows.size());
        for(int r=0;r<rows.size();++r){
            const Row &row=rows[r];
            QString value=QStringLiteral("未解析"),unit,status=QStringLiteral("未解析"),when;
            EngineeringField resolvedField=row.field;
            for(int i=qMin(index,int(business_.size())-1);i>=0&&value==QStringLiteral("未解析");--i){
                const auto &packet=business_[i];
                if(packet.packetName!=row.packet)continue;
                for(const auto &field:packet.fields)if(field.code==row.field.code){
                    if(field.value.isEmpty()||(!by04&&field.status.startsWith("raw_"))||
                       field.status=="invalid_source_bit_position")continue;
                    value=field.value;unit=field.unit;status=fieldStatusText(field.status);
                    resolvedField=field;
                    when=shortTime(packet.transport.captureLocal);break;
                }
            }
            table->setItem(r,0,item(row.packet));
            table->setItem(r,1,item(row.field.nameCn.isEmpty()?row.field.code:row.field.nameCn,row.field.code+QStringLiteral(" · ")+row.field.bits+QStringLiteral(" · ")+row.field.formula));
            table->setItem(r,2,item(telemetryValue(resolvedField,value,unit),status));
            table->setItem(r,3,item(when));
        }
        table->verticalHeader()->setDefaultSectionSize(20);
    }
}

void MainWindow::updateWebHistory(){
    const QString root=QCoreApplication::applicationDirPath()+QStringLiteral("/data");
    if(!QDir().mkpath(root))return;
    const QString jsonPath=root+QStringLiteral("/telemetry.json");
    QJsonArray records;QSet<QString> seen;
    QFile existing(jsonPath);
    if(existing.open(QIODevice::ReadOnly)){
        const auto doc=QJsonDocument::fromJson(existing.readAll());
        if(doc.isArray())for(const auto &value:doc.array())if(value.isObject()){
            const auto obj=value.toObject();
            const QString key=obj.value(QStringLiteral("satellite")).toString()+QLatin1Char('|')+
                obj.value(QStringLiteral("capture_local")).toString()+QLatin1Char('|')+
                obj.value(QStringLiteral("raw_kiss_hex")).toString();
            if(!seen.contains(key)){seen.insert(key);records.append(obj);}
        }
    }
    for(const auto &packet:business_){
        QJsonObject obj=orbit::BusinessCatalog::toJson(packet);
        obj.insert(QStringLiteral("satellite"),packet.transport.satellite);
        const QString key=packet.transport.satellite+QLatin1Char('|')+
            obj.value(QStringLiteral("capture_local")).toString()+QLatin1Char('|')+
            obj.value(QStringLiteral("raw_kiss_hex")).toString();
        if(!seen.contains(key)){seen.insert(key);records.append(obj);}
    }
    QVector<QJsonObject> sorted;sorted.reserve(records.size());
    for(const auto &value:records)if(value.isObject())sorted.push_back(value.toObject());
    std::stable_sort(sorted.begin(),sorted.end(),[](const QJsonObject &a,const QJsonObject &b){
        return a.value(QStringLiteral("capture_local")).toString()<b.value(QStringLiteral("capture_local")).toString();
    });
    records=QJsonArray();for(const auto &obj:sorted)records.append(obj);
    const QByteArray json=QJsonDocument(records).toJson(QJsonDocument::Indented);
    auto writeFile=[](const QString &path,const QByteArray &bytes){
        QSaveFile file(path);if(!file.open(QIODevice::WriteOnly))return false;
        if(file.write(bytes)!=bytes.size())return false;return file.commit();
    };
    writeFile(jsonPath,json);
    QString csv=QStringLiteral("time,satellite,packet,field_code,name_cn,name_en,raw_hex,raw_number,value,unit,status\r\n");
    auto quote=[](QString v){v.replace(QStringLiteral("\""),QStringLiteral("\"\""));return QStringLiteral("\"")+v+QStringLiteral("\"");};
    for(const auto &value:records){
        const auto packet=value.toObject();
        const QString time=packet.value(QStringLiteral("capture_local")).toString();
        const QString sat=packet.value(QStringLiteral("satellite")).toString();
        const QString name=packet.value(QStringLiteral("packet")).toString();
        for(const auto &fieldValue:packet.value(QStringLiteral("fields")).toArray()){
            const auto f=fieldValue.toObject();
            QStringList row={time,sat,name,f.value("code").toString(),f.value("name_cn").toString(),
                f.value("name_en").toString(),f.value("raw_hex").toString(),f.value("raw_number").toString(),
                f.value("value").toString(),f.value("unit").toString(),f.value("status").toString()};
            for(QString &v:row)v=quote(v);
            csv+=row.join(QLatin1Char(','))+QStringLiteral("\r\n");
        }
    }
    writeFile(root+QStringLiteral("/telemetry.csv"),csv.toUtf8());
    QFile templateFile(QCoreApplication::applicationDirPath()+QStringLiteral("/web/index.template.html"));
    QByteArray html;
    if(templateFile.open(QIODevice::ReadOnly))html=templateFile.readAll();
    if(html.isEmpty())return;
    QByteArray safeJson=json;safeJson.replace("<","\\u003c");
    html.replace("__DATA_JSON__",safeJson);
    writeFile(root+QStringLiteral("/index.html"),html);
}

void MainWindow::populateEngineering(int index){
    QSignalBlocker block(engineeringTable_);
    engineeringTable_->setRowCount(0);
    fieldTree_->clear();
    fieldDetail_->setText(QStringLiteral("选择参数查看原始值、位字段位置及换算公式"));
    if(index<0)return;
    int highlighted=-1;
    for(int i=0;i<=index && i<business_.size();++i){
        const auto &p=business_[i];
        if(p.packetName.isEmpty())continue;
        const int row=engineeringTable_->rowCount();
        engineeringTable_->insertRow(row);
        auto *time=item(shortTime(p.transport.captureLocal));
        time->setData(Qt::UserRole,i);
        engineeringTable_->setItem(row,0,time);
        engineeringTable_->setItem(row,1,item(p.packetName));
        engineeringTable_->setItem(row,2,item(QStringLiteral("%1 项").arg(p.fields.size())));
        highlighted=i;
    }
    const auto *chosen=latestRecognized(index,{""});
    if(!chosen){
        engineeringSummary_->setText(
            QStringLiteral("当前时段暂无符合公开 27 类定义的分系统业务包；原始 KISS 已保留于“原始数据”。"));
        return;
    }
    engineeringSummary_->setText(chosen->identification=="by04_ccsds_header_only"?
        QStringLiteral("%1 · 保留 %2 项协议/候选字段 · 未匹配的业务值以原始数据形式保留")
            .arg(eventTitle(*chosen)).arg(chosen->fields.size()):
        QStringLiteral("%1 · %2 项参数 · 识别方式：%3 · 应用 CRC 未校验")
            .arg(eventTitle(*chosen)).arg(chosen->fields.size()).arg(chosen->identification));
    QHash<QString,QTreeWidgetItem*> groups;
    QSignalBlocker treeBlocked(fieldTree_);
    for(const auto &f:chosen->fields){
        const QString group=categoryFor(f.nameCn);
        auto *root=groups.value(group,nullptr);
        if(!root){
            root=new QTreeWidgetItem(fieldTree_,{group,"","",""});
            auto font=root->font(0);font.setBold(true);root->setFont(0,font);
            groups.insert(group,root);
        }
        auto *field=new QTreeWidgetItem(root,{
            f.nameCn.isEmpty()?f.code:f.nameCn,f.value,f.unit,labelStatus(f.status)});
        field->setData(0,Qt::UserRole,f.code);
        field->setData(0,Qt::UserRole+1,chosen->packetName);
        field->setToolTip(0,f.code+"\n"+f.nameEn);
        field->setToolTip(1,QStringLiteral("原始 HEX：")+f.rawHex);
    }
    fieldTree_->expandAll();
    engineeringSearch_->setText(engineeringSearch_->text()); // keep search input intact
    if(highlighted>=0){
        for(int row=0;row<engineeringTable_->rowCount();++row){
            auto *i=engineeringTable_->item(row,0);
            if(i&&i->data(Qt::UserRole).toInt()==highlighted){
                engineeringTable_->selectRow(row);break;
            }
        }
    }
    plotEngineering(chosen->packetName,
        chosen->fields.isEmpty()?QString():chosen->fields.first().code,index);
}
void MainWindow::plotEngineering(const QString &packet,const QString &code,int upTo){
    auto *chart=new QChart;
    chart->setBackgroundVisible(false);
    if(!code.isEmpty()&&upTo>=0){
        auto *series=new QLineSeries;
        QDateTime first;
        for(int i=0;i<=upTo&&i<business_.size();++i){
            const auto &p=business_[i];
            if(p.packetName!=packet)continue;
            for(const auto &f:p.fields)if(f.code==code){
                bool ok=false;const double value=f.value.toDouble(&ok);
                if(!ok||!std::isfinite(value))continue;
                if(!first.isValid()&&p.transport.captureLocal.isValid())
                    first=p.transport.captureLocal;
                const qreal elapsed=(first.isValid()&&p.transport.captureLocal.isValid())?
                    qreal(first.msecsTo(p.transport.captureLocal))/1000.0 : qreal(i);
                series->append(elapsed,value);
            }
        }
        if(series->count()){
            chart->addSeries(series);chart->createDefaultAxes();
            chart->setTitle(QStringLiteral("%1 · %2（记录时间相对秒）").arg(packet,code));
        }else{
            delete series;
            chart->setTitle(QStringLiteral("所选参数暂无可用数值序列"));
        }
    }else chart->setTitle(QStringLiteral("选择业务遥测参数以查看时序曲线"));
    engineeringChart_->setChart(chart);
}
void MainWindow::updateAttitude(int index){
    const BusinessPacket *last=nullptr;
    bool candidate=false;
    for(int i=qMin(index,int(business_.size())-1);i>=0;--i){
        const auto &packet=business_[i];
        bool hasQ[4]{};
        for(const auto &field:packet.fields){
            for(int j=0;j<4;++j){
                if((field.nameCn==QStringLiteral("解算四元数q%1").arg(j)&&field.status=="table_formula")||
                   (field.code==QStringLiteral("candidate.quaternion.q%1").arg(j)&&field.status=="candidate_unverified"))
                    hasQ[j]=!field.value.isEmpty();
            }
        }
        if(std::all_of(std::begin(hasQ),std::end(hasQ),[](bool v){return v;})){
            last=&packet;
            candidate=std::any_of(packet.fields.cbegin(),packet.fields.cend(),[](const EngineeringField &f){return f.status=="candidate_unverified"&&f.code.startsWith("candidate.quaternion.");});
            break;
        }
    }
    bool has[4]{false,false,false,false};
    double q[4]{};
    if(last){
        for(const auto &field:last->fields)for(int j=0;j<4;++j){
            const bool exact=field.nameCn==QStringLiteral("解算四元数q%1").arg(j)&&field.status=="table_formula";
            const bool guessed=field.code==QStringLiteral("candidate.quaternion.q%1").arg(j)&&field.status=="candidate_unverified";
            if(!exact&&!guessed)continue;
            bool ok=false;const double value=field.value.toDouble(&ok);
            if(ok&&std::isfinite(value)){q[j]=value;has[j]=true;}
        }
    }
    const bool ready=std::all_of(std::begin(has),std::end(has),[](bool value){return value;});
    double norm2=0;
    for(int i=0;i<4;++i){
        quaternion_[i]->setText(ready?QString::number(q[i],'f',5):QStringLiteral("未解析"));
        norm2+=q[i]*q[i];
    }
    const double norm=std::sqrt(norm2);
    normLabel_->setText(ready?QString::number(norm,'f',6):QStringLiteral("未解析"));
    attitudeWidget_->setQuaternion(QQuaternion(float(q[0]),float(q[1]),float(q[2]),float(q[3])),ready&&norm>1e-9);
    if(!ready){
        attitudeInfo_->setText(QStringLiteral("未解析：当前数据包没有完整四元数"));
    }else if(candidate){
        attitudeInfo_->setText(QStringLiteral("BY04 四元数候选值 · 未验证字段位置与坐标定义 · 接收时间：%1")
            .arg(shortTime(last->transport.captureLocal)));
    }else{
        attitudeInfo_->setText(QStringLiteral("SAT-S3 四元数 · 记录时间：%1 · 数据来源：%2 · 参考系与应用 CRC 尚未核实")
            .arg(shortTime(last->transport.captureLocal),qualityLabel(last->transport.fecEvidence)));
    }
    auto *chart=new QChart;chart->setBackgroundVisible(false);
    QVector<QLineSeries*> series;
    for(int k=0;k<4;++k){auto *line=new QLineSeries;line->setName(QString("q%1").arg(k));series.push_back(line);}
    QDateTime first;
    for(int i=0;i<=index&&i<business_.size();++i){
        const auto &packet=business_[i];double values[4]{};bool found[4]{};
        bool packetCandidate=false;
        for(const auto &field:packet.fields)for(int j=0;j<4;++j){
            const bool exact=field.nameCn==QStringLiteral("解算四元数q%1").arg(j)&&field.status=="table_formula";
            const bool guessed=field.code==QStringLiteral("candidate.quaternion.q%1").arg(j)&&field.status=="candidate_unverified";
            if(!exact&&!guessed)continue;
            packetCandidate=packetCandidate||guessed;
            bool ok=false;const double value=field.value.toDouble(&ok);
            if(ok&&std::isfinite(value)){values[j]=value;found[j]=true;}
        }
        if(!std::all_of(std::begin(found),std::end(found),[](bool value){return value;}))continue;
        if(!first.isValid()&&packet.transport.captureLocal.isValid())first=packet.transport.captureLocal;
        const qreal x=first.isValid()&&packet.transport.captureLocal.isValid()?
            qreal(first.msecsTo(packet.transport.captureLocal))/1000.0:qreal(i);
        for(int j=0;j<4;++j)series[j]->append(x,values[j]);
        candidate=candidate||packetCandidate;
    }
    const bool hasSeries=series[0]->count()>0;
    for(auto *line:series){if(hasSeries)chart->addSeries(line);else delete line;}
    if(hasSeries){chart->createDefaultAxes();chart->legend()->setVisible(true);}
    chart->setTitle(hasSeries?(candidate?QStringLiteral("候选四元数时序，未经工程字段表验证"):
                                      QStringLiteral("解算四元数时序（相对记录时间）")):
                              QStringLiteral("四元数未解析"));
    attitudeChart_->setChart(chart);
}
void MainWindow::updateRawPacket(int index){
    if(index<0||index>=business_.size()){
        rawHex_->setPlainText(QStringLiteral("当前没有可检查的 KISS 应用包。"));
        return;
    }
    const auto &packet=business_[index];
    QString payload=QStringLiteral("当前包：%1\n识别方式：%2\nCRC：未校验\nFEC 来源：%3\n"
                   "KISS 字节数：%4\n接收时间：%5\n\n完整 KISS 解码内容：\n%6")
        .arg(eventTitle(packet),packet.identification,
             qualityLabel(packet.transport.fecEvidence))
        .arg(packet.transport.bytes.size())
        .arg(utcLabel(packet.transport.captureLocal),
             QString::fromLatin1(packet.transport.bytes.toHex(' ').toUpper()));
    if(!packet.transport.data.isEmpty())
        payload+=QStringLiteral("\n\n内部 Space Packet 候选数据：\n")+
                 QString::fromLatin1(packet.transport.data.toHex(' ').toUpper());
    rawHex_->setPlainText(payload);
}
void MainWindow::updateFrameList(){
    QSignalBlocker blocked(frameTable_);
    frameTable_->setRowCount(frames_.size());
    for(int i=0;i<frames_.size();++i){
        const auto &f=frames_[i];
        const QStringList cells={shortTime(f.captureLocal),
            f.master<0?"—":QString::number(f.master),
            f.vcid<0?"—":QString::number(f.vcid),
            f.apid<0?"—":QString("0x%1").arg(f.apid,3,16,QChar('0')).toUpper(),
            f.sequence<0?"—":QString::number(f.sequence),
            sourceLabel(f.sources)+" · "+qualityLabel(f.quality),
            f.complete?QStringLiteral("结构可读"):QStringLiteral("结构异常")};
        for(int j=0;j<cells.size();++j)
            frameTable_->setItem(i,j,item(cells[j]));
    }
    QSignalBlocker packetBlocked(packetTable_);
    packetTable_->setRowCount(packets_.size());
    for(int i=0;i<packets_.size();++i){
        const auto &p=packets_[i];
        const QStringList values={QString::number(p.vcid),
            QString("%1 — %2").arg(p.firstMaster).arg(p.lastMaster),
            QString::number(p.bytes.size()),
            p.apid<0?"—":QString("0x%1").arg(p.apid,3,16,QChar('0')).toUpper(),
            qualityLabel(p.fecEvidence),QStringLiteral("CRC 未校验")};
        for(int j=0;j<values.size();++j)
            packetTable_->setItem(i,j,item(values[j]));
    }
    diagnosticsSummary_->setText(
        QStringLiteral("传输帧 %1 条 · KISS %2 包 · 计数缺口中断 %3 次 · 非法转义 %4 次 · "
        "超长包 %5 个 · EOF 未完成 %6 个")
        .arg(frames_.size()).arg(packets_.size())
        .arg(transportStats_.interruptedByCounterGap)
        .arg(transportStats_.malformedEscapePackets)
        .arg(transportStats_.oversizedPackets)
        .arg(transportStats_.incompleteAtEof));
}
void MainWindow::inspectFrame(int index){
    if(index<0||index>=frames_.size()){debugText_->clear();return;}
    const auto &f=frames_[index];
    const int scid=f.bytes.size()>=2?
        ((quint8(f.bytes[0])&0x3f)<<4)|(quint8(f.bytes[1])>>4):-1;
    QString result=QStringLiteral("接收时间：%1\n记录时间依据：%2\n航天器标识 SCID：0x%3\nMFC：%4\n虚拟信道：%5\n"
        "候选 APID：%6\nFEC/来源：%7\n帧质量：%8\n校验：应用层 CRC 未执行\n"
        "诊断：%9\nSHA-256：%10\n\n原始 223 字节信息帧：\n%11")
        .arg(utcLabel(f.captureLocal),f.timeBasis)
        .arg(scid,3,16,QChar('0')).arg(f.master).arg(f.vcid).arg(f.apid)
        .arg(sourceLabel(f.sources)).arg(qualityLabel(f.quality))
        .arg(f.error,f.hash,QString::fromLatin1(f.bytes.toHex(' ').toUpper()));
    debugText_->setPlainText(result);
}
void MainWindow::setDiagnosticsVisible(bool visible){
    if(pages_&&diagnosticTab_>=0)pages_->setTabVisible(diagnosticTab_,visible);
}
void MainWindow::scanSsdv(){
    if(!imageApid_)return;
    const bool by04=resolvedSatellite()==QStringLiteral("BY04");
    images_=by04?orbit::By04Decoder::images(frames_):
                 orbit::ImageTools::candidatesFromKiss(packets_,imageApid_->value());
    imageApid_->setEnabled(!by04);
    imageTable_->setRowCount(images_.size());
    for(int i=0;i<images_.size();++i){
        const auto &p=images_[i];
        const QStringList row={QString::number(p.imageId),
            QString::number(p.packetId),
            QString("%1 × %2").arg(p.width).arg(p.height),
            p.status=="dslwp_crc32_pass"?QStringLiteral("DSLWP CRC32 通过"):
            (p.status=="dslwp_crc32_failed"?QStringLiteral("DSLWP CRC32 失败"):
                                               QStringLiteral("候选，CRC 未校验"))};
        for(int j=0;j<row.size();++j)imageTable_->setItem(i,j,item(row[j]));
    }
    imageInfo_->setText(by04?
        QStringLiteral("BY04 · VC1 218 字节 DSLWP SSDV · %1 个有效结构分片；仅通过独立 DSLWP CRC 的分片标记为已校验。")
            .arg(images_.size()):
        QStringLiteral("APID %1：发现 %2 个 SSDV 候选分片；选择图像 ID 后可在软件内重建 JPEG。")
            .arg(imageApid_->value()).arg(images_.size()));
    if(imageMetric_)imageMetric_->setText(QString::number(images_.size()));
    if(!images_.isEmpty())updateImageProgress(images_.first().imageId);
    else{
        imageCoverage_->setRange(0,1);
        imageCoverage_->setValue(0);
        imageCoverage_->setFormat(QStringLiteral("尚无观测序号"));
        imageCoverageNote_->setText(QStringLiteral("尚无候选分片 · 总包数未知"));
    }
}
void MainWindow::updateImageProgress(int imageId){
    QSet<int> unique;
    int lo=std::numeric_limits<int>::max(),hi=-1;
    for(const auto &p:images_)if(p.imageId==imageId){
        unique.insert(p.packetId);
        lo=std::min(lo,p.packetId);hi=std::max(hi,p.packetId);
    }
    if(hi<lo)return;
    const int span=hi-lo+1;
    const int missing=span-unique.size();
    imageCoverage_->setRange(0,std::max(1,span));
    imageCoverage_->setValue(unique.size());
    imageCoverage_->setFormat(QStringLiteral("已观测 %v / %m 个序号"));
    imageCoverageNote_->setText(QStringLiteral(
       "图像 ID %1 · 观测包序号 %2–%3 · 范围内缺口 %4 个 · 总包数未知，"
       "此进度只反映观测序号跨度，不是图片完整度")
       .arg(imageId).arg(lo).arg(hi).arg(missing));
}
void MainWindow::exportSsdv(){
    if(images_.isEmpty()){
        QMessageBox::information(this,QStringLiteral("保存图像数据"),
            QStringLiteral("当前没有 SSDV 候选分片，请先扫描正确的图像 APID。"));return;
    }
    const QString path=QFileDialog::getSaveFileName(this,
       QStringLiteral("保存 SSDV 候选数据"),"satellite.ssdv",
       QStringLiteral("SSDV 数据 (*.ssdv *.bin)"));
    if(path.isEmpty())return;
    QString error;
    if(!orbit::ImageTools::savePackets(images_,path,&error))
        QMessageBox::warning(this,QStringLiteral("保存失败"),error);
    else statusBar()->showMessage(QStringLiteral("已保存候选分片；未独立完成图像 CRC 验证"),9500);
}
void MainWindow::decodeSsdv(){
    if(images_.isEmpty())scanSsdv();
    if(images_.isEmpty()){
        QMessageBox::information(this,QStringLiteral("SSDV 图片重建"),
            QStringLiteral("没有可解码的 SSDV 分片，请先扫描数据。"));
        return;
    }
    int row=imageTable_->currentRow();
    if(row<0||row>=images_.size())row=0;
    const int imageId=images_[row].imageId;
    const bool by04=resolvedSatellite()==QStringLiteral("BY04");
    const auto rebuilt=orbit::ImageTools::decodeImage(images_,imageId,by04);
    if(rebuilt.jpeg.isEmpty()){
        QMessageBox::warning(this,QStringLiteral("SSDV 图片重建失败"),
            rebuilt.error.isEmpty()?QStringLiteral("当前分片无法重建 JPEG。"):rebuilt.error);
        return;
    }
    const QImage image=QImage::fromData(rebuilt.jpeg,"JPEG");
    if(image.isNull()){
        QMessageBox::warning(this,QStringLiteral("SSDV 图片重建失败"),
            QStringLiteral("解码器返回的数据不是可读取的 JPEG 图像。"));
        return;
    }
    decodedImage_=QPixmap::fromImage(image);
    imagePreview_->setImage(decodedImage_);
    imageInfo_->setText(QStringLiteral("图像 ID %1 · %2 × %3 · 已送入解码器 %4 个分片 · %5。CRC 通过数量仅表示已通过对应格式校验的分片，不代表整幅图像完整。")
        .arg(imageId).arg(image.width()).arg(image.height()).arg(rebuilt.receivedPackets)
        .arg(rebuilt.complete?QStringLiteral("检测到图像结束标志"):QStringLiteral("当前为可恢复预览")));

    const QString outputPath=QFileDialog::getSaveFileName(this,
        QStringLiteral("保存重建的 JPEG"),QStringLiteral("BY70-4_ID%1.jpg").arg(imageId),
        QStringLiteral("JPEG 图像 (*.jpg *.jpeg)"));
    if(outputPath.isEmpty()){
        statusBar()->showMessage(QStringLiteral("JPEG 已在软件内重建并显示，尚未保存到磁盘"),9000);
        return;
    }
    QSaveFile output(outputPath);
    if(!output.open(QIODevice::WriteOnly)||output.write(rebuilt.jpeg)!=rebuilt.jpeg.size()||!output.commit()){
        QMessageBox::warning(this,QStringLiteral("JPEG 保存失败"),output.errorString());
        return;
    }
    imagePath_=outputPath;
    statusBar()->showMessage(QStringLiteral("JPEG 重建完成并已保存：")+outputPath,9000);
}
void MainWindow::exportFrames(bool csv){
    if(frames_.isEmpty())return;
    const QString path=QFileDialog::getSaveFileName(this,QStringLiteral("导出原始传输帧"),
        csv?"frames.csv":"frames.json",csv?QStringLiteral("CSV (*.csv)"):QStringLiteral("JSON (*.json)"));
    if(path.isEmpty())return;
    QString error;
    const bool ok=csv?orbit::Protocol::exportCsv(path,frames_,&error):
                      orbit::Protocol::exportJson(path,frames_,&error);
    if(!ok)QMessageBox::warning(this,QStringLiteral("导出失败"),error);
    else statusBar()->showMessage(QStringLiteral("原始传输帧已导出"),8000);
}
void MainWindow::exportEngineering(bool csv){
    if(business_.isEmpty()){
        QMessageBox::information(this,QStringLiteral("业务遥测"),
             QStringLiteral("暂无可导出的业务包；请先导入会话。"));return;
    }
    const QString path=QFileDialog::getSaveFileName(this,
        QStringLiteral("导出业务遥测字段"),
        csv?"engineering.csv":"engineering.json",
        csv?QStringLiteral("CSV (*.csv)"):QStringLiteral("JSON (*.json)"));
    if(path.isEmpty())return;
    QString error;
    const bool ok=csv?orbit::BusinessCatalog::exportCsv(path,business_,&error):
                      orbit::BusinessCatalog::exportJson(path,business_,&error);
    if(!ok)QMessageBox::warning(this,QStringLiteral("导出失败"),error);
    else statusBar()->showMessage(QStringLiteral("业务遥测已按已知接收时间排序导出"),8000);
}
void MainWindow::exportQuaternions(bool experimental){
    if(frames_.isEmpty())return;
    if(!experimental && satellite()=="BY04"){
        QMessageBox::information(this,QStringLiteral("BY04 四元数"),
            QStringLiteral("BY04 的姿态工程遥测位置和换算尚未确认，"
                           "不能套用 ASRTU-1 的 SAT-S3 字段定义导出四元数。"));
        return;
    }
    const QString path=QFileDialog::getSaveFileName(this,
        experimental?QStringLiteral("实验性候选四元数（非正式姿态）"):
                     QStringLiteral("SAT-S3 解算四元数"),
        experimental?"candidate-quaternion.csv":"SAT-S3-quaternions.csv",
        QStringLiteral("CSV (*.csv)"));
    if(path.isEmpty())return;
    QString error;
    const bool ok=experimental?
        orbit::Protocol::exportAttitudeCsv(path,frames_,&error):
        orbit::BusinessCatalog::exportQuaternionCsv(path,business_,&error);
    if(!ok)QMessageBox::warning(this,QStringLiteral("导出失败"),error);
    else statusBar()->showMessage(QStringLiteral("四元数已导出；应用 CRC / 参考坐标系未验证"),9000);
}
void MainWindow::exportKiss(){
    if(packets_.isEmpty())return;
    const QString path=QFileDialog::getSaveFileName(this,QStringLiteral("导出 KISS 应用包"),
                                                     "kiss-packets.json",QStringLiteral("JSON (*.json)"));
    if(path.isEmpty())return;
    QString error;
    if(!orbit::Transport::exportJson(path,packets_,&error))
        QMessageBox::warning(this,QStringLiteral("导出失败"),error);
    else statusBar()->showMessage(QStringLiteral("KISS 应用包已导出"),8000);
}
