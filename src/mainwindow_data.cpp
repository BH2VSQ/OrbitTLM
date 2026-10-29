#include "mainwindow.h"
#include "mainwindow_widgets.h"
#include "by04.h"
#include <QApplication>
#include <QCoreApplication>
#include <QDate>
#include <QTime>
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
bool isSwitchTelemetryField(const EngineeringField &field){
    const QString &name=field.nameCn;
    if(name.contains(QStringLiteral("开关"))||name.contains(QStringLiteral("使能"))||
       name.contains(QStringLiteral("状态"))||name.contains(QStringLiteral("继电器")))return true;
    return name==QStringLiteral("基带板卡")||name==QStringLiteral("基带HW")||
           name==QStringLiteral("基带UV")||name==QStringLiteral("星敏X_5V")||
           name==QStringLiteral("星敏Y_5V")||name==QStringLiteral("GNSS_5V")||
           name==QStringLiteral("GNSS_5V-回读")||
           name.contains(QStringLiteral("X测控数传"))||
           name.contains(QStringLiteral("X频段数传"));
}
QString telemetryValue(const EngineeringField &field,const QString &value,const QString &unit){
    QString shown=value;
    const bool switchField=isSwitchTelemetryField(field);
    bool widthOk=false;
    const int width=field.bits.section('.',2,2).toInt(&widthOk);
    if(switchField&&widthOk&&width==1){
        // Use the unsigned decoded value when available, regardless of whether
        // the field came through a raw_bits or a table-formula definition.
        const QString raw=field.rawNumber.isEmpty()?value.trimmed():field.rawNumber.trimmed();
        if(raw==QStringLiteral("0"))shown=QStringLiteral("0(关)");
        else if(raw==QStringLiteral("1"))shown=QStringLiteral("1(开)");
    }
    QString displayUnit=unit;
    if(field.nameCn.contains(QStringLiteral("温度")) &&
       (displayUnit.isEmpty()||displayUnit==QStringLiteral("1")||
        displayUnit==QStringLiteral("C")||displayUnit==QStringLiteral("°C")))
        displayUnit=QStringLiteral("℃");
    if(!displayUnit.isEmpty()&&displayUnit!=QStringLiteral("1"))
        shown+=QLatin1Char(' ')+displayUnit;
    return shown;
}
int telemetrySwitchState(const EngineeringField &field,const QString &value){
    if(!isSwitchTelemetryField(field))return -1;
    if(value.contains(QStringLiteral("开")))return 1;
    if(value.contains(QStringLiteral("关")))return 0;
    bool widthOk=false;
    const int width=field.bits.section('.',2,2).toInt(&widthOk);
    if(widthOk&&width==1){
        const QString raw=field.rawNumber.isEmpty()?value.trimmed():field.rawNumber.trimmed();
        if(raw==QStringLiteral("1"))return 1;
        if(raw==QStringLiteral("0"))return 0;
    }
    return -1;
}
}
QString MainWindow::satellite() const{
    return satelliteSelect_?orbit::canonicalSatelliteName(satelliteSelect_->currentData().toString()):QStringLiteral("ASRTU-1");
}
QString MainWindow::resolvedSatellite() const{
    const QString selected=satellite();
    if(selected!=QStringLiteral("AUTO"))return selected;
    for(const auto &frame:frames_)
        if(!frame.satellite.isEmpty()&&frame.satellite!=QStringLiteral("AUTO"))return orbit::canonicalSatelliteName(frame.satellite);
    for(const auto &packet:packets_)
        if(!packet.satellite.isEmpty()&&packet.satellite!=QStringLiteral("AUTO"))return orbit::canonicalSatelliteName(packet.satellite);
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
    // ORBITTLM_ROUND4_IMPORT
    if(logPath_.isEmpty()&&binPath_.isEmpty()){
        QMessageBox::information(this,QStringLiteral("导入会话"),
                  QStringLiteral("请选择解码日志或 223 字节 BIN 文件。"));
        return;
    }
    QString error;
    const QString selectedSatellite=satellite();
    const auto log=logPath_.isEmpty()?QVector<orbit::Frame>{}:
        orbit::Protocol::fromLog(logPath_,selectedSatellite,&error);
    if(!error.isEmpty()){
        QMessageBox::warning(this,QStringLiteral("日志读取失败"),error);return;
    }
    const auto bin=binPath_.isEmpty()?QVector<orbit::Frame>{}:
        orbit::Protocol::fromBin(binPath_,selectedSatellite,&error);
    if(!error.isEmpty()){
        QMessageBox::warning(this,QStringLiteral("二进制读取失败"),error);return;
    }
    const QVector<orbit::Frame> importedFrames=orbit::Protocol::fuse(log,bin,&frameStats_);
    if(importedFrames.isEmpty()){
        QMessageBox::warning(this,QStringLiteral("导入未完成"),
            QStringLiteral("所选文件没有解析出有效传输帧。当前会话保持不变，也不会创建空的卫星数据库。"));
        return;
    }
    frames_=importedFrames;
    QString importStoreError;
    QStringList databasePaths;
    QSet<QString> seenDatabasePaths;
    for(const orbit::Frame &importedFrame:frames_){
        const QString path=satelliteStore_.databasePath(importedFrame.satellite);
        if(!seenDatabasePaths.contains(path)){seenDatabasePaths.insert(path);databasePaths.push_back(path);}
        bool inserted=false;
        if(!satelliteStore_.append(importedFrame,&inserted,&importStoreError))break;
    }
    const QString actualSat=resolvedSatellite();
    if(hasCustomSchema_&&customSchema_.satellite()==actualSat)
        for(auto &frame:frames_)customSchema_.apply(frame);
    rebuildSession();

    QString sessionStoreError;
    const bool sessionStored=db_.save(actualSat+QStringLiteral(" · LOG/BIN 离线会话"),frames_,&sessionStoreError);
    QString htmlStoreError;
    const bool htmlStored=updateWebHistory(&htmlStoreError);
    const QString htmlPath=QDir(satelliteStore_.rootDirectory()).filePath(QStringLiteral("index.html"));
    QStringList report;
    report<<QStringLiteral("导入 %1 帧完成").arg(frames_.size());
    if(importStoreError.isEmpty())report<<QStringLiteral("卫星数据库：")+databasePaths.join(QStringLiteral("；"));
    else report<<QStringLiteral("卫星数据库写入失败：%1（目标：%2）").arg(importStoreError,databasePaths.join(QStringLiteral("；")));
    if(!sessionStored)report<<QStringLiteral("历史会话保存失败：%1").arg(sessionStoreError);
    if(htmlStored)report<<QStringLiteral("HTML 索引：%1").arg(htmlPath);
    else report<<QStringLiteral("HTML 索引生成失败：%1").arg(htmlStoreError);
    statusBar()->showMessage(report.join(QStringLiteral("  |  ")),20000);
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
    QString legacyStoreError;
    for(const orbit::Frame &legacyFrame:frames_){
        bool inserted=false;
        if(!satelliteStore_.append(legacyFrame,&inserted,&legacyStoreError))break;
    }
    if(!legacyStoreError.isEmpty())statusBar()->showMessage(QStringLiteral("旧历史迁移到卫星 SQLite 库失败：%1").arg(legacyStoreError),10000);

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
void MainWindow::rebuildSession(bool preferLatest){
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
    if(!business_.isEmpty())selectEvent(preferLatest?business_.size()-1:0);
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
    const bool by04=resolvedSatellite()==QStringLiteral("BY04");
    if(index<0||index>=business_.size()){
        currentTitle_->setText(QStringLiteral("当前数据包：未解析"));
        attitudeModeLabel_->setText(by04?QStringLiteral("UTC时间：未解析"):
                                         QStringLiteral("UTC累计秒：未解析"));
    }else{
        const auto &packet=business_[index];
        currentTitle_->setText(QStringLiteral("当前数据包：%1").arg(eventTitle(packet)));
        if(by04){
            QString utc;
            for(int i=index;i>=0&&utc.isEmpty();--i){
                const auto &candidate=business_[i];
                if(candidate.transport.satellite!="BY04")continue;
                for(const auto &field:candidate.fields){
                    if(field.nameCn!=QStringLiteral("UTC时间")||field.value.isEmpty()||
                       field.status.startsWith("raw_"))continue;
                    bool ok=false;
                    const qint64 elapsed=field.value.toLongLong(&ok);
                    if(!ok||elapsed<0)continue;
                    // The BY70-4 OBC-SF UTC counter is seconds from 2017-01-01 UTC.
                    const QDateTime epoch(QDate(2017,1,1),QTime(0,0),Qt::UTC);
                    const QDateTime parsed=epoch.addSecs(elapsed);
                    if(parsed.isValid())utc=parsed.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss 'UTC'"));
                    break;
                }
            }
            attitudeModeLabel_->setText(utc.isEmpty()?QStringLiteral("UTC时间：未解析"):
                                            QStringLiteral("UTC时间：%1").arg(utc));
        }else{
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
    const bool by04=resolvedSatellite()==QStringLiteral("BY04");
    if(card==attitudeCard_)card->setTitle(by04?QStringLiteral("姿态与轨道"):QStringLiteral("姿态与 GPS 轨道"));
    const QString title=card->title();
    struct Row {
        QString name;
        QString packet;
        EngineeringField definition;
        bool manual=false;
        QStringList sourceNames;
        bool combineSources=false;
    };
    QVector<Row> rows;
    QSet<QString> seen;
    auto add=[&](const QString &name,const QString &packet,const EngineeringField &field,bool manual=false){
        if(name.isEmpty()||seen.contains(name))return;
        seen.insert(name);rows.push_back({name,packet,field,manual,{},false});
    };
    auto addBy04=[&](const QString &name,const QStringList &sources,bool combine=false){
        EngineeringField placeholder;
        placeholder.code=sources.join(QStringLiteral(" | "));
        placeholder.nameCn=name;
        placeholder.status=QStringLiteral("catalog_definition");
        if(name.isEmpty()||seen.contains(name))return;
        seen.insert(name);rows.push_back({name,QString(),placeholder,true,sources,combine});
    };
    const auto definitions=catalog_.definitionPackets();
    if(by04){
        if(card==powerCard_){
            // ORBITTLM_ROUND4_POWER_ORDER: keep six rows per column; battery voltage is on the left,
            // and RF 3.3V voltage is directly above RF 3.3V current in the right column.
            addBy04(QStringLiteral("母线电压"),{QStringLiteral("母线电压"),QStringLiteral("A12-母线电压遥测")});
            addBy04(QStringLiteral("12V电压"),{QStringLiteral("12V电压"),QStringLiteral("A15-12V电压遥测")});
            addBy04(QStringLiteral("12V电流"),{QStringLiteral("12V电流"),QStringLiteral("A20-12V电流遥测")});
            addBy04(QStringLiteral("5V电压"),{QStringLiteral("5V电压"),QStringLiteral("A14-5V电压遥测")});
            addBy04(QStringLiteral("5V电流"),{QStringLiteral("5V电流"),QStringLiteral("A19-5V电流遥测")});
            addBy04(QStringLiteral("电池电压"),{QStringLiteral("电池电压"),QStringLiteral("A17-蓄电池电压")});
            addBy04(QStringLiteral("射频3.3V电压"),{QStringLiteral("射频 3.3V电压")});
            addBy04(QStringLiteral("射频3.3V电流"),{QStringLiteral("射频 3.3V电流")});
            addBy04(QStringLiteral("射频1.8V电压"),{QStringLiteral("射频 1.8V电压")});
            addBy04(QStringLiteral("射频1.8V电流"),{QStringLiteral("射频 1.8V电流")});
            addBy04(QStringLiteral("充电电流"),{QStringLiteral("充电电流"),QStringLiteral("A26-充电电流")});
            addBy04(QStringLiteral("放电电流"),{QStringLiteral("放电电流"),QStringLiteral("A24-放电电流")});
        }else if(card==attitudeCard_){
            addBy04(QStringLiteral("姿态模式"),{QStringLiteral("姿态模式")});
            addBy04(QStringLiteral("姿态四元数 q0"),{QStringLiteral("姿态四元数q0")});
            addBy04(QStringLiteral("姿态四元数 q1"),{QStringLiteral("姿态四元数q1")});
            addBy04(QStringLiteral("姿态四元数 q2"),{QStringLiteral("姿态四元数q2")});
            addBy04(QStringLiteral("姿态四元数 q3"),{QStringLiteral("姿态四元数q3")});
            addBy04(QStringLiteral("偏差四元数 q0"),{QStringLiteral("偏差四元数q0")});
            addBy04(QStringLiteral("偏差四元数 q1"),{QStringLiteral("偏差四元数q1")});
            addBy04(QStringLiteral("偏差四元数 q2"),{QStringLiteral("偏差四元数q2")});
            addBy04(QStringLiteral("偏差四元数 q3"),{QStringLiteral("偏差四元数q3")});
            addBy04(QStringLiteral("卫星姿态更新标志"),{QStringLiteral("卫星姿态更新标志")});
            addBy04(QStringLiteral("卫星姿态可信标志"),{QStringLiteral("卫星姿态可信标志")});
            addBy04(QStringLiteral("GPS轨道有效计数"),{QStringLiteral("GPS轨道有效计数")});
        }else if(card==thermalCard_){
            addBy04(QStringLiteral("电池温度1"),{QStringLiteral("电池温度1")});
            addBy04(QStringLiteral("电池温度2"),{QStringLiteral("电池温度2")});
            addBy04(QStringLiteral("温度采集4-屏幕"),{QStringLiteral("温度采集4-屏幕")});
            addBy04(QStringLiteral("温度采集6-储箱"),{QStringLiteral("温度采集6-储箱")});
            addBy04(QStringLiteral("温度采集7-冯相机"),{QStringLiteral("温度采集7-冯相机")});
            addBy04(QStringLiteral("温度采集8-帆板"),{QStringLiteral("温度采集8-帆板")});
            addBy04(QStringLiteral("PCB温度"),{QStringLiteral("PCB温度")});
            addBy04(QStringLiteral("Z7温度"),{QStringLiteral("Z7温度")});
            addBy04(QStringLiteral("STM32内部温度"),{QStringLiteral("STM32内部温度")});
        }else if(card==payloadCard_){
            addBy04(QStringLiteral("基带板"),{QStringLiteral("基带板继电器")});
            addBy04(QStringLiteral("基带板卡"),{QStringLiteral("基带板卡")});
            addBy04(QStringLiteral("基带HW"),{QStringLiteral("基带HW")});
            addBy04(QStringLiteral("基带UV"),{QStringLiteral("基带UV")});
            addBy04(QStringLiteral("星敏A"),{QStringLiteral("星敏X_5V")});
            addBy04(QStringLiteral("星敏B"),{QStringLiteral("星敏Y_5V")});
            addBy04(QStringLiteral("GNSS"),{QStringLiteral("GNSS_5V"),QStringLiteral("GNSS_5V-回读")});
            addBy04(QStringLiteral("X发射机开关状态"),{QStringLiteral("X发射机开关状态"),QStringLiteral("B0：发射机开关状态")});
            addBy04(QStringLiteral("X频段数传状态"),{QStringLiteral("X频段数传状态"),QStringLiteral("数传锁定状态"),QStringLiteral("B4： 数传锁定状态"),QStringLiteral("B4：数传锁定状态")});
            addBy04(QStringLiteral("X测控数传"),{QStringLiteral("X测控数传继电器"),QStringLiteral("X测控数传开-回读"),QStringLiteral("X测控数传关-回读")});
        }
    }else if(title==QStringLiteral("总线电压与电流")){
        for(const auto &packet:definitions)for(const auto &field:packet.fields){
            const QString &name=field.nameCn;
            if((name.contains(QStringLiteral("母线"))||name.contains(QStringLiteral("总线"))) &&
               (name.contains(QStringLiteral("电压"))||name.contains(QStringLiteral("电流"))) &&
               !excludedLimitField(name))add(name,packet.packetName,field);
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
    }else if(title==QStringLiteral("姿态与 GPS 轨道")){
        const QStringList fixed={QStringLiteral("解算四元数q0"),QStringLiteral("解算四元数q1"),
            QStringLiteral("解算四元数q2"),QStringLiteral("解算四元数q3"),
            QStringLiteral("GPS速度X"),QStringLiteral("GPS速度Y"),
            QStringLiteral("升交点赤经"),QStringLiteral("近地点角距"),QStringLiteral("GPS速度Z"),
            QStringLiteral("半长轴"),QStringLiteral("偏心率"),QStringLiteral("轨道倾角")};
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

    const bool alignedBy04Quaternions=by04&&card==attitudeCard_;
    struct RowCell{int row=0;int column=0;};
    QVector<RowCell> rowCells(rows.size());
    const int rowsPerColumn=qMax(1,(rows.size()+1)/2);
    int renderRows=rowsPerColumn;
    if(alignedBy04Quaternions){
        renderRows=6;
        int extraSlot=0;
        for(int r=0;r<rows.size();++r){
            const QString &name=rows[r].name;
            // Place matching q0..q3 pairs on the same table row explicitly.
            // Left pair: attitude quaternion; right pair: deviation quaternion.
            bool quaternionMapped=false;
            for(int qIndex=0;qIndex<4;++qIndex){
                if(name==QStringLiteral("姿态四元数 q%1").arg(qIndex)){
                    rowCells[r]={qIndex,0};
                    quaternionMapped=true;
                    break;
                }
                if(name==QStringLiteral("偏差四元数 q%1").arg(qIndex)){
                    rowCells[r]={qIndex,2};
                    quaternionMapped=true;
                    break;
                }
            }
            if(quaternionMapped)continue;
            int slot=-1;
            if(name==QStringLiteral("姿态模式"))slot=0;
            else if(name==QStringLiteral("GPS轨道有效计数"))slot=1;
            else if(name==QStringLiteral("卫星姿态更新标志"))slot=2;
            else if(name==QStringLiteral("卫星姿态可信标志"))slot=3;
            else slot=4+extraSlot++;
            const int row=4+slot/2;
            rowCells[r]={row,(slot%2)*2};
            renderRows=qMax(renderRows,row+1);
        }
    }
    table->setWordWrap(false);
    table->setRowCount(renderRows);
    for(int r=0;r<rows.size();++r){
        const Row &row=rows[r];
        QString value=QStringLiteral("未解析"),unit,status=QStringLiteral("未解析"),when;
        EngineeringField resolvedField=row.definition;
        if(row.combineSources){
            QStringList parts;
            int resolvedCount=0;
            for(const QString &sourceName:row.sourceNames){
                bool found=false;
                for(int i=qMin(index,int(business_.size())-1);i>=0&&!found;--i){
                    const auto &packet=business_[i];
                    if(by04&&packet.transport.satellite!="BY04")continue;
                    for(const auto &field:packet.fields){
                        if(field.nameCn!=sourceName||field.value.isEmpty()||field.status==QStringLiteral("invalid_source_bit_position"))continue;
                        if(field.status.startsWith("raw_")&&!(by04&&field.status==QStringLiteral("raw_bits")))continue;
                        QString shown=telemetryValue(field,field.value,field.unit);
                        parts.push_back(sourceName+QStringLiteral("：")+shown);
                        resolvedField=field;
                        when=shortTime(packet.transport.captureLocal);
                        ++resolvedCount;found=true;break;
                    }
                }
                if(!found)parts.push_back(sourceName+QStringLiteral("：未解析"));
            }
            if(resolvedCount>0){
                value=parts.join(QStringLiteral("；"));
                status=resolvedCount==row.sourceNames.size()?QStringLiteral("字段表解析"):
                                                               QStringLiteral("部分字段已解析");
            }
        }else{
            int from=qMin(index,int(business_.size())-1);
            for(int i=from;i>=0&&(value==QStringLiteral("未解析")||value.isEmpty());--i){
                const auto &packet=business_[i];
                if(by04&&packet.transport.satellite!="BY04")continue;
                if(!row.packet.isEmpty()&&packet.packetName!=row.packet)continue;
                for(const auto &field:packet.fields){
                    bool matched=false;
                    if(by04&&!row.sourceNames.isEmpty()){
                        matched=row.sourceNames.contains(field.nameCn);
                    }else if(row.manual && title==QStringLiteral("部件开关状态")){
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
                    }else if(title==QStringLiteral("姿态与 GPS 轨道") && row.name.startsWith(QStringLiteral("解算四元数q"))){
                        const int q=row.name.right(1).toInt();
                        matched=field.nameCn==row.name||field.code==QStringLiteral("candidate.quaternion.q%1").arg(q);
                    }else if(title==QStringLiteral("姿态与 GPS 轨道") && row.name.startsWith(QStringLiteral("GPS速度"))){
                        const QString axis=row.name.right(1);
                        matched=field.nameCn==row.name || field.nameCn.contains(row.name) ||
                            field.nameCn.contains(QStringLiteral("GNSS卫星速度")+axis+QStringLiteral("方向"));
                    }else if(row.name.contains(QStringLiteral("UTC累计秒"))||row.name.contains(QStringLiteral("UTC时间累计秒"))){
                        matched=field.nameCn.contains(QStringLiteral("UTC"))&&field.nameCn.contains(QStringLiteral("累计秒"));
                    }else if(title==QStringLiteral("姿态与 GPS 轨道") && row.name==QStringLiteral("半长轴")){
                        matched=field.nameCn.contains(QStringLiteral("半长轴"));
                    }else if(title==QStringLiteral("姿态与 GPS 轨道") && row.name==QStringLiteral("偏心率")){
                        matched=field.nameCn.contains(QStringLiteral("偏心率"));
                    }else if(title==QStringLiteral("姿态与 GPS 轨道") && row.name==QStringLiteral("升交点赤经")){
                        matched=field.nameCn.contains(QStringLiteral("升交点赤经"));
                    }else if(title==QStringLiteral("姿态与 GPS 轨道") && row.name==QStringLiteral("近地点角距")){
                        matched=field.nameCn.contains(QStringLiteral("近地点角距"));
                    }else if(title==QStringLiteral("姿态与 GPS 轨道") && row.name==QStringLiteral("轨道倾角")){
                        matched=field.nameCn.contains(QStringLiteral("轨道倾角"));
                    }else{
                        matched=field.nameCn==row.name;
                    }
                    const bool allowMappedRaw=by04&&field.status==QStringLiteral("raw_bits");
                    if(!matched||field.value.isEmpty()||
                       (field.status.startsWith("raw_")&&!allowMappedRaw)||field.status=="invalid_source_bit_position")continue;
                    value=field.value;unit=field.unit;status=fieldStatusText(field.status);resolvedField=field;
                    when=shortTime(packet.transport.captureLocal);break;
                }
                if(value!=QStringLiteral("未解析")&&!value.isEmpty())break;
            }
        }
        QString tooltip=row.packet.isEmpty()?QString():row.packet+QStringLiteral(" · ")+row.definition.code;
        if(!row.sourceNames.isEmpty())tooltip=row.sourceNames.join(QStringLiteral("，"));
        auto *nameItem=item(row.name,tooltip);
        auto *valueItem=item(row.combineSources?value:telemetryValue(resolvedField,value,unit),status);
        int rowIndex=r%rowsPerColumn;
        int columnOffset=(r/rowsPerColumn)*2;
        if(alignedBy04Quaternions){rowIndex=rowCells[r].row;columnOffset=rowCells[r].column;}
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
    if(by04)table->resizeRowsToContents();
}

void MainWindow::updateComponentTabs(int index){
    if(componentTables_.isEmpty())return;
    const bool by04=resolvedSatellite()==QStringLiteral("BY04");
    const QString activeSatellite=by04?QStringLiteral("BY04"):QStringLiteral("ASRTU-1");

    // Hide the other satellite's subsystem tabs while keeping both official trees loaded.
    auto *systems=pages_?pages_->findChild<AutoWrapTabWidget*>(QStringLiteral("officialSystemTabs")):nullptr;
    if(systems){
        int firstVisible=-1;
        bool currentIsVisible=false;
        for(int i=0;i<systems->count();++i){
            QWidget *groupPage=systems->widget(i);
            const bool visible=groupPage->property("treeSatellite").toString()==activeSatellite;
            systems->setTabVisible(i,visible);
            if(visible&&firstVisible<0)firstVisible=i;
            if(visible&&i==systems->currentIndex())currentIsVisible=true;
        }
        if(firstVisible>=0&&!currentIsVisible)systems->setCurrentIndex(firstVisible);
    }

    struct PacketScope { QString satellite; QString code; QString key; };
    struct LiveField { EngineeringField field; QString when; };
    QVector<PacketScope> scopes;
    scopes.reserve(componentTables_.size());
    for(auto it=componentTables_.cbegin();it!=componentTables_.cend();++it){
        QTableWidget *table=it.value();
        if(!table||table->property("treeSatellite").toString()!=activeSatellite)continue;
        scopes.push_back({activeSatellite,table->property("officialPacketCode").toString(),it.key()});
    }

    // Build the latest available value index once per replay position instead of
    // rescanning the whole replay history independently for every displayed field.
    QHash<QString,QHash<QString,LiveField>> latestValues;
    for(int i=qMin(index,int(business_.size())-1);i>=0;--i){
        const BusinessPacket &packet=business_[i];
        const QString packetSatellite=orbit::canonicalSatelliteName(packet.transport.satellite);
        if(packetSatellite!=activeSatellite)continue;
        for(const PacketScope &scope:scopes){
            if(!packet.packetName.contains(scope.code,Qt::CaseInsensitive)&&
               !packet.packetLabel.contains(scope.code,Qt::CaseInsensitive))continue;
            auto &byField=latestValues[scope.key];
            for(const EngineeringField &field:packet.fields){
                if(field.value.isEmpty()||field.status=="invalid_source_bit_position"||
                   field.status=="truncated_field")continue;
                const QString composite=field.bits.isEmpty()?field.code:
                    field.code+QLatin1Char('.')+field.bits;
                const LiveField live{field,shortTime(packet.transport.captureLocal)};
                if(!composite.isEmpty()&&!byField.contains(composite))byField.insert(composite,live);
                if(!field.code.isEmpty()&&!byField.contains(field.code))byField.insert(field.code,live);
                const QString nameKey=QStringLiteral("@")+field.nameCn;
                if(!field.nameCn.isEmpty()&&!byField.contains(nameKey))byField.insert(nameKey,live);
            }
        }
    }

    for(const PacketScope &scope:scopes){
        QTableWidget *table=componentTables_.value(scope.key,nullptr);
        if(!table)continue;
        const auto byField=latestValues.value(scope.key);
        for(int row=0;row<table->rowCount();++row){
            QTableWidgetItem *nameItem=table->item(row,0);
            if(!nameItem)continue;
            if(nameItem->data(Qt::UserRole+2).toBool()){
                if(auto *valueItem=table->item(row,2))valueItem->setText(QStringLiteral("字段目录未提供"));
                if(auto *timeItem=table->item(row,3))timeItem->setText(QString());
                continue;
            }

            const QString officialName=nameItem->text();
            const QString officialCode=nameItem->data(Qt::UserRole).toString();
            auto liveIt=byField.constFind(officialCode);
            if(liveIt==byField.cend()&&!officialCode.isEmpty())
                liveIt=byField.constFind(officialCode.section(QLatin1Char('.'),0,0));
            if(liveIt==byField.cend())
                liveIt=byField.constFind(QStringLiteral("@")+officialName);

            QString shown=QStringLiteral("未解析");
            QString status=QStringLiteral("当前会话未匹配到该字段");
            QString when;
            const bool found=liveIt!=byField.cend();
            if(found){
                const EngineeringField &field=liveIt.value().field;
                shown=telemetryValue(field,field.value,field.unit);
                status=fieldStatusText(field.status);
                when=liveIt.value().when;
            }

            auto *valueItem=table->item(row,2);
            if(!valueItem){
                valueItem=new QTableWidgetItem;
                table->setItem(row,2,valueItem);
            }
            valueItem->setText(shown);
            valueItem->setToolTip(status);
            valueItem->setForeground(found?QColor("#1f3045"):QColor("#64748b"));
            auto *timeItem=table->item(row,3);
            if(!timeItem){
                timeItem=new QTableWidgetItem;
                table->setItem(row,3,timeItem);
            }
            timeItem->setText(when);
        }
        table->verticalHeader()->setDefaultSectionSize(22);
    }
}
bool MainWindow::updateWebHistory(QString *error){
    // ORBITTLM_ROUND4_HISTORY
    if(error)error->clear();
    QJsonArray records;
    for(const auto &packet:business_){
        QJsonObject record=orbit::BusinessCatalog::toJson(packet);
        records.append(record);
    }
    QByteArray json=QJsonDocument(records).toJson(QJsonDocument::Compact);
    json.replace("<","\\u003c");

    // Keep each satellite .db and the generated index.html in the same writable directory.
    const QString dataDir=satelliteStore_.rootDirectory();
    if(!QDir().mkpath(dataDir)){
        const QString message=QStringLiteral("无法创建遥测存储目录：%1").arg(dataDir);
        if(error)*error=message;statusBar()->showMessage(message,10000);return false;
    }
    const QDir appDir(QCoreApplication::applicationDirPath());
    const QStringList templateCandidates={
        appDir.filePath(QStringLiteral("web/index.template.html")),
        appDir.filePath(QStringLiteral("web/index.html")),
        appDir.filePath(QStringLiteral("index.template.html"))
    };
    QFile templateFile;
    QByteArray html;
    for(const QString &candidate:templateCandidates){
        if(!QFileInfo::exists(candidate))continue;
        templateFile.setFileName(candidate);
        if(templateFile.open(QIODevice::ReadOnly)){
            html=templateFile.readAll();templateFile.close();break;
        }
    }
    // A self-contained fallback guarantees that an index is still generated when a
    // packaged build forgot to copy web/index.template.html.
    if(html.isEmpty()||!html.contains("__DATA_JSON__")){
        html=QString::fromUtf8(R"ORBITTLM_HTML(<!doctype html>
<html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>OrbitTLM 遥测趋势与 CSV</title><style>
body{font:14px Segoe UI,Microsoft YaHei,sans-serif;margin:18px;color:#182940;background:#f3f6fb}h1{font-size:21px}.panel{background:#fff;border:1px solid #dce5f0;border-radius:9px;padding:12px;margin:10px 0}.row{display:flex;gap:10px;align-items:end;flex-wrap:wrap}label{display:flex;flex-direction:column;gap:4px;font-size:12px}input,select,button{padding:7px;border:1px solid #cbd8e8;border-radius:6px;background:#fff;font:inherit}button{cursor:pointer;background:#eaf3ff}select[multiple]{min-width:270px;min-height:100px}canvas{width:100%;height:330px;display:block}table{border-collapse:collapse;width:100%;font-size:12px}td,th{text-align:left;border-bottom:1px solid #e4ebf3;padding:6px}th{background:#f5f8fc;position:sticky;top:0}.scroll{max-height:300px;overflow:auto}.muted{color:#687d94;font-size:12px}
</style></head><body><h1>OrbitTLM · 遥测趋势与 CSV</h1><p class="muted">当前会话数据；选择卫星、参数和时间范围后绘制趋势。多参数曲线归一化到 0–100%，CSV 保留原始工程值。</p>
<section class="panel row"><label>卫星<select id="sat"><option value="all">全部卫星</option></select></label><label>参数（可多选）<select id="fields" multiple></select></label><label>开始时间<input id="from" type="datetime-local"></label><label>结束时间<input id="to" type="datetime-local"></label><label>关键词<input id="q" placeholder="参数名 / 字段编号"></label><button onclick="render()">应用筛选</button><button onclick="resetFilters()">重置</button><button onclick="downloadCsv(false)">导出筛选 CSV</button><button onclick="downloadCsv(true)">导出曲线 CSV</button></section>
<section class="panel"><b id="summary">请选择参数和时间范围。</b><canvas id="chart" width="1200" height="360"></canvas><div id="legend"></div></section>
<section class="panel"><h3>参数统计</h3><div class="scroll"><table><thead><tr><th>参数</th><th>样本</th><th>最小值</th><th>最大值</th><th>平均值</th></tr></thead><tbody id="stats"></tbody></table></div></section>
<section class="panel"><h3>筛选后的遥测值</h3><div class="scroll"><table><thead><tr><th>时间</th><th>卫星</th><th>参数</th><th>值</th><th>单位</th><th>状态</th></tr></thead><tbody id="rows"></tbody></table></div></section>
<script>
const DATA=__DATA_JSON__,all=[];for(const p of DATA)for(const f of (p.fields||[]))all.push({time:p.capture_local||'',sat:p.satellite||'未知',packet:p.packet||p.packet_label||'',name:f.name_cn||f.code||'未命名',code:f.code||'',value:f.value??'',unit:f.unit||'',status:f.status||''});
const $=id=>document.getElementById(id),numeric=x=>{if(x===null||x===undefined||String(x).trim()==='')return NaN;const n=Number(String(x).replace(/,/g,''));return Number.isFinite(n)?n:NaN},key=x=>x.code||x.name,colors=['#2675c9','#d9485f','#0f9d78','#8b5cf6','#d97706','#0891b2','#be185d','#64748b'];
function fillSat(){const s=$('sat');for(const v of [...new Set(all.map(x=>x.sat))].sort())if(![...s.options].some(o=>o.value===v))s.add(new Option(v,v));}
function fillFields(){const sat=$('sat').value,prev=new Set([...$('fields').selectedOptions].map(o=>o.value)),m=new Map();for(const x of all){if(sat!=='all'&&x.sat!==sat)continue;const k=key(x);if(!m.has(k))m.set(k,{k,name:x.name,code:x.code,unit:x.unit});}const e=$('fields');e.innerHTML='';for(const v of [...m.values()].sort((a,b)=>a.name.localeCompare(b.name,'zh-CN'))){const o=new Option(v.name+(v.code?' · '+v.code:'')+(v.unit?' ('+v.unit+')':''),v.k);o.selected=prev.has(v.k);e.add(o);}}
function filtered(){const sat=$('sat').value,q=$('q').value.trim().toLowerCase(),from=$('from').value?new Date($('from').value).getTime():-Infinity,to=$('to').value?new Date($('to').value).getTime():Infinity;return all.filter(x=>{const t=new Date(x.time).getTime();return(sat==='all'||x.sat===sat)&&(!q||(x.name+' '+x.code+' '+x.packet).toLowerCase().includes(q))&&(!Number.isFinite(t)||(t>=from&&t<=to));});}
function selected(){return new Set([...$('fields').selectedOptions].map(o=>o.value));}
function render(){const rows=filtered(),keys=selected(),chosen=rows.filter(x=>keys.has(key(x))&&Number.isFinite(numeric(x.value)));$('rows').innerHTML=rows.filter(x=>!keys.size||keys.has(key(x))).slice(0,3000).map(x=>'<tr><td>'+esc(x.time)+'</td><td>'+esc(x.sat)+'</td><td>'+esc(x.name)+'</td><td>'+esc(x.value)+'</td><td>'+esc(x.unit)+'</td><td>'+esc(x.status)+'</td></tr>').join('');const groups=[...keys].map(k=>({k,items:chosen.filter(x=>key(x)===k)})).filter(g=>g.items.length);$('stats').innerHTML=groups.map(g=>{const a=g.items.map(x=>numeric(x.value)),mn=Math.min(...a),mx=Math.max(...a),avg=a.reduce((s,v)=>s+v,0)/a.length,n=(all.find(x=>key(x)===g.k)||{}).name||g.k;return '<tr><td>'+esc(n)+'</td><td>'+a.length+'</td><td>'+mn+'</td><td>'+mx+'</td><td>'+avg.toFixed(4)+'</td></tr>';}).join('');$('summary').textContent='筛选记录 '+rows.length+' 条 · 有效数值 '+chosen.length+' 个 · 已选参数 '+keys.size+' 个';draw(groups);}
function draw(groups){const c=$('chart'),ctx=c.getContext('2d'),w=c.width,h=c.height;ctx.clearRect(0,0,w,h);ctx.strokeStyle='#dce5ef';ctx.fillStyle='#657a91';ctx.font='12px sans-serif';const multi=groups.length>1,allValues=groups.flatMap(g=>g.items.map(x=>numeric(x.value))),rawMin=allValues.length?Math.min(...allValues):0,rawMax=allValues.length?Math.max(...allValues):1,axisMin=multi?0:(rawMin===rawMax?rawMin-1:rawMin),axisMax=multi?100:(rawMin===rawMax?rawMax+1:rawMax);for(let i=0;i<5;i++){const y=22+i*(h-54)/4;ctx.beginPath();ctx.moveTo(48,y);ctx.lineTo(w-14,y);ctx.stroke();ctx.fillText(multi?(100-i*25)+'%':(axisMax-i*(axisMax-axisMin)/4).toPrecision(4),4,y+4);}const allTimes=groups.flatMap(g=>g.items.map(x=>new Date(x.time).getTime()).filter(Number.isFinite)),xmin=allTimes.length?Math.min(...allTimes):0,xmax=allTimes.length?Math.max(...allTimes):0;const legend=[];groups.forEach((g,i)=>{const vals=g.items.map(x=>numeric(x.value)),mn=Math.min(...vals),mx=Math.max(...vals),color=colors[i%colors.length],pts=g.items.map(x=>({t:new Date(x.time).getTime(),v:numeric(x.value)})).filter(p=>Number.isFinite(p.t));ctx.strokeStyle=color;ctx.lineWidth=2;ctx.beginPath();pts.forEach((p,j)=>{const x=48+(xmax>xmin?(p.t-xmin)/(xmax-xmin):j/Math.max(1,pts.length-1))*(w-62),v=multi?(mx>mn?(p.v-mn)/(mx-mn)*100:50):p.v,y=22+(h-54)*(1-(multi?v/100:(v-axisMin)/(axisMax-axisMin)));if(j)ctx.lineTo(x,y);else ctx.moveTo(x,y);});ctx.stroke();const name=(all.find(x=>key(x)===g.k)||{}).name||g.k;legend.push('<span style="color:'+color+'">■ '+esc(name)+(multi?'（归一化）':'')+'</span>');});$('legend').innerHTML=legend.join('　');if(!groups.length){ctx.fillStyle='#657a91';ctx.fillText('请选择一个或多个可转为数值的参数。',60,70);}}
function esc(x){return String(x??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));}
function downloadCsv(curves){const rows=filtered(),keys=selected(),out=curves? [['时间','卫星','参数','值','单位','状态']]:[['时间','卫星','业务包','参数','字段编号','值','单位','状态']];for(const x of rows){if(keys.size&&!keys.has(key(x)))continue;if(curves&&!Number.isFinite(numeric(x.value)))continue;out.push(curves?[x.time,x.sat,x.name,x.value,x.unit,x.status]:[x.time,x.sat,x.packet,x.name,x.code,x.value,x.unit,x.status]);}const csv='\ufeff'+out.map(r=>r.map(v=>'"'+String(v??'').replace(/"/g,'""')+'"').join(',')).join('\r\n'),blob=new Blob([csv],{type:'text/csv;charset=utf-8;'}),url=URL.createObjectURL(blob),a=document.createElement('a');a.href=url;a.download=curves?'OrbitTLM-curves.csv':'OrbitTLM-filtered.csv';a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);}
function resetFilters(){$('sat').value='all';$('q').value='';$('from').value='';$('to').value='';fillFields();render();}
$('sat').addEventListener('change',()=>{fillFields();render();});$('fields').addEventListener('change',render);fillSat();fillFields();render();
</script></body></html>)ORBITTLM_HTML").toUtf8();
    }
    html.replace("__DATA_JSON__",json);
    const QString outputPath=QDir(dataDir).filePath(QStringLiteral("index.html"));
    QSaveFile output(outputPath);
    if(!output.open(QIODevice::WriteOnly)||output.write(html)!=html.size()||!output.commit()){
        const QString message=QStringLiteral("HTML 历史索引写入失败：%1（%2）").arg(outputPath,output.errorString());
        if(error)*error=message;statusBar()->showMessage(message,10000);return false;
    }
    return true;
}

void MainWindow::populateEngineering(int index){
    QSignalBlocker block(engineeringTable_);
    engineeringTable_->setRowCount(0);
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

    const bool by04=resolvedSatellite()==QStringLiteral("BY04");
    const QString treeSatellite=by04?QStringLiteral("BY04"):QStringLiteral("ASRTU-1");
    if(fieldTreeSatellite_!=treeSatellite){
        QSignalBlocker treeBlocked(fieldTree_);
        fieldTree_->clear();
        fieldTreeFieldItems_.clear();
        fieldTreeSatellite_=treeSatellite;
        QHash<QString,QTreeWidgetItem*> componentRoots;
        auto componentRoot=[&](const QString &name){
            auto *root=componentRoots.value(name,nullptr);
            if(!root){
                root=new QTreeWidgetItem(fieldTree_,{name,"","",""});
                QFont font=root->font(0);font.setBold(true);root->setFont(0,font);
                componentRoots.insert(name,root);
            }
            return root;
        };
        const QString treeSatellite=by04?QStringLiteral("BY04"):QStringLiteral("ASRTU-1");
        const QString treePath=QDir(QCoreApplication::applicationDirPath())
                              .filePath(QStringLiteral("telemetry-trees/")+treeSatellite+QStringLiteral(".json"));
        QFile officialFile(treePath);
        bool officialOpen=officialFile.open(QIODevice::ReadOnly);
        if(!officialOpen){
            officialFile.setFileName(QStringLiteral(":/telemetry-trees/")+treeSatellite+QStringLiteral(".json"));
            officialOpen=officialFile.open(QIODevice::ReadOnly);
        }
        bool loadedOfficial=false;
        if(officialOpen){
            const QJsonDocument document=QJsonDocument::fromJson(officialFile.readAll());
            officialFile.close();
            if(document.isObject()){
                const QJsonArray packets=document.object().value(QStringLiteral("packets")).toArray();
                if(!packets.isEmpty()){
                    QHash<QString,QTreeWidgetItem*> groups;
                    for(const QJsonValue &packetValue:packets){
                        if(!packetValue.isObject())continue;
                        const QJsonObject packet=packetValue.toObject();
                        const QString officialName=packet.value(QStringLiteral("official_name")).toString();
                        const QString packetCode=packet.value(QStringLiteral("packet_code")).toString();
                        const QString groupName=packet.value(QStringLiteral("group")).toString(QStringLiteral("其他分系统"));
                        const QString internalPacketName=by04
                            ?QStringLiteral("BY04-")+packetCode:packetCode;
                        QTreeWidgetItem *groupNode=groups.value(groupName,nullptr);
                        if(!groupNode){
                            groupNode=componentRoot(groupName);
                            groups.insert(groupName,groupNode);
                        }
                        auto *packetNode=new QTreeWidgetItem(groupNode,
                            {officialName+QStringLiteral(" [%1]").arg(packetCode),QString(),QString(),QString()});
                        packetNode->setToolTip(0,QStringLiteral("官方遥测包名：%1\n包代码：%2").arg(officialName,packetCode));
                        packetNode->setData(0,Qt::UserRole,internalPacketName);
                        const QJsonArray fields=packet.value(QStringLiteral("fields")).toArray();
                        if(fields.isEmpty()){
                            auto *placeholder=new QTreeWidgetItem(packetNode,
                                {QStringLiteral("附件未提供该包的字段条目"),QString(),QStringLiteral("未解析"),QStringLiteral("目录未提供")});
                            placeholder->setToolTip(0,QStringLiteral("保留官方数据包位置；附件中该包 fields 数组为空，未臆造字段。"));
                            placeholder->setData(0,Qt::UserRole+7,true);
                            continue;
                        }
                        for(const QJsonValue &fieldValue:fields){
                            if(!fieldValue.isObject())continue;
                            const QJsonObject field=fieldValue.toObject();
                            const QString fieldName=field.value(QStringLiteral("name")).toString();
                            const QString fullCode=field.value(QStringLiteral("code")).toString();
                            const QString englishName=field.value(QStringLiteral("english_name")).toString();
                            const QString baseCode=fullCode.section(QLatin1Char('.'),0,0);
                            const QString bitWidth=fullCode.contains(QLatin1Char('.'))
                                ?fullCode.section(QLatin1Char('.'),1):QString();
                            auto *entry=new QTreeWidgetItem(packetNode,
                                {fieldName.isEmpty()?fullCode:fieldName,fullCode,
                                 QStringLiteral("未解析"),QStringLiteral("官方字段定义")});
                            entry->setData(0,Qt::UserRole,fullCode);
                            entry->setData(0,Qt::UserRole+1,internalPacketName);
                            entry->setData(0,Qt::UserRole+2,bitWidth);
                            entry->setData(0,Qt::UserRole+3,QString());
                            entry->setData(0,Qt::UserRole+4,englishName);
                            entry->setData(0,Qt::UserRole+5,QStringLiteral("官方遥测树"));
                            entry->setData(0,Qt::UserRole+6,QString());
                            entry->setToolTip(0,QStringLiteral("官方字段编号：%1\n位/宽度：%2\n英文名：%3\n工程换算公式：官方目录未提供；当前不生成推测数值。")
                                                  .arg(fullCode,bitWidth,englishName));
                            fieldTreeFieldItems_.insert(internalPacketName+QLatin1Char('|')+fullCode,entry);
                        }
                    }
                    loadedOfficial=true;
                }
            }
        }
        if(!loadedOfficial){
            auto *missing=new QTreeWidgetItem(fieldTree_,
                {QStringLiteral("官方遥测树字段目录未加载"),treePath,QStringLiteral("不可用"),QStringLiteral("请检查安装资源")});
            missing->setToolTip(0,QStringLiteral("无法读取官方字段树 JSON；没有退回到不完整的静态字段清单。"));
        }
        fieldTree_->expandToDepth(3);
    }

    QHash<QString,EngineeringField> latestFields;
    if(index>=0){
        for(int i=0;i<=index&&i<business_.size();++i){
            const auto &packet=business_[i];
            if(by04!=(packet.transport.satellite==QStringLiteral("BY04")))continue;
            for(const auto &field:packet.fields){
                if(field.code.isEmpty())continue;
                QString fullCode=field.code;
                if(!field.bits.isEmpty()&&!fullCode.endsWith(QLatin1Char('.')+field.bits))
                    fullCode+=QLatin1Char('.')+field.bits;
                QStringList packetKeys{packet.packetName};
                if(packet.transport.satellite==QStringLiteral("BY04")&&
                   !packet.packetName.startsWith(QStringLiteral("BY04-")))
                    packetKeys.push_back(QStringLiteral("BY04-")+packet.packetName);
                for(const QString &packetKey:packetKeys){
                    latestFields.insert(packetKey+QLatin1Char('|')+field.code,field);
                    latestFields.insert(packetKey+QLatin1Char('|')+fullCode,field);
                }
            }
        }
    }
    {
        QSignalBlocker treeBlocked(fieldTree_);
        for(auto it=fieldTreeFieldItems_.begin();it!=fieldTreeFieldItems_.end();++it){
            QTreeWidgetItem *entry=it.value();
            const QString key=it.key();
            const QString defaultUnit=entry->data(0,Qt::UserRole+6).toString();
            auto found=latestFields.constFind(key);
            if(found==latestFields.constEnd()){
                entry->setText(1,QStringLiteral("未解析"));
                entry->setText(2,defaultUnit);
                entry->setText(3,QStringLiteral("未收到样本"));
                entry->setForeground(1,QColor("#64748b"));
                continue;
            }
            const auto &field=found.value();
            const QString shown=telemetryValue(field,field.value,field.unit);
            entry->setText(1,shown);
            entry->setText(2,field.unit.isEmpty()?defaultUnit:field.unit);
            entry->setText(3,fieldStatusText(field.status));
            entry->setToolTip(1,QStringLiteral("原始 HEX：%1").arg(field.rawHex));
            const int state=telemetrySwitchState(field,field.value);
            if(state==1)entry->setForeground(1,QColor("#16803d"));
            else if(state==0)entry->setForeground(1,QColor("#c62828"));
            else entry->setForeground(1,QBrush());
        }
    }

    const auto *chosen=index>=0?latestRecognized(index,{""}):nullptr;
    if(!chosen){
        int packetCount=0;
        if(by04){
            packetCount=0;
            for(auto it=componentTables_.cbegin();it!=componentTables_.cend();++it)
                if(it.value()&&it.value()->property("treeSatellite").toString()==QStringLiteral("BY04"))++packetCount;
        }else packetCount=catalog_.packetKinds();
        engineeringSummary_->setText(by04?
            QStringLiteral("BY70-4 官方遥测树：%1 类面板分包；本地已接入 %2 项字段定义，其余分包保留官方字段位置，未匹配样本显示“未解析”。")
                .arg(packetCount).arg(fieldTreeFieldItems_.size()):
            QStringLiteral("ASRTU-1 遥测树：%1 类分包、%2 个字段定义；数值按当前会话中最新匹配样本更新。")
                .arg(catalog_.packetKinds()).arg(fieldTreeFieldItems_.size()));
        fieldDetail_->setText(QStringLiteral("展开组件分组和数据包即可浏览完整字段定义。选择字段可查看位位置、公式与来源；无样本时保留定义并显示“未解析”。"));
        return;
    }
    const QString treeNote=by04?
        QStringLiteral("官方面板分包 %1 类 · 本地字段定义 %2 项").arg([&]{int count=0;for(auto it=componentTables_.cbegin();it!=componentTables_.cend();++it)if(it.value()&&it.value()->property("treeSatellite").toString()==QStringLiteral("BY04"))++count;return count;}()).arg(fieldTreeFieldItems_.size()):
        QStringLiteral("完整字段树 %1 类分包 · %2 个字段").arg(catalog_.packetKinds()).arg(fieldTreeFieldItems_.size());
    engineeringSummary_->setText(chosen->identification=="by04_ccsds_header_only"?
        QStringLiteral("%1 · 保留 %2 项协议字段 · %3 · 未匹配业务字段保持原样")
            .arg(eventTitle(*chosen)).arg(chosen->fields.size()).arg(treeNote):
        QStringLiteral("%1 · %2 项当前包字段 · %3 · 识别方式：%4 · 应用 CRC 未校验")
            .arg(eventTitle(*chosen)).arg(chosen->fields.size()).arg(treeNote,chosen->identification));
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
    const bool by04=resolvedSatellite()==QStringLiteral("BY04");
    const QStringList axes={QStringLiteral("X"),QStringLiteral("Y"),QStringLiteral("Z")};
    for(int axis=0;axis<3;++axis){
        QString shown=QStringLiteral("未解析");
        for(int i=qMin(index,int(business_.size())-1);i>=0&&shown==QStringLiteral("未解析");--i){
            const auto &packet=business_[i];
            if(by04!=(packet.transport.satellite==QStringLiteral("BY04")))continue;
            const QString wanted=by04?QStringLiteral("角速度%1").arg(axes[axis].toLower()):
                                      QStringLiteral("解算角速度%1").arg(axes[axis]);
            for(const auto &field:packet.fields){
                if(field.nameCn!=wanted||field.value.isEmpty()||field.status.startsWith("raw_"))continue;
                shown=field.value;
                if(!field.unit.isEmpty())shown+=QLatin1Char(' ')+field.unit;
                else shown+=QStringLiteral("（单位未提供）");
                if(angularVelocity_[axis])angularVelocity_[axis]->setToolTip(QStringLiteral("%1 · %2 · %3")
                    .arg(field.code,field.bits,field.formula));
                break;
            }
        }
        if(angularVelocity_[axis])angularVelocity_[axis]->setText(shown);
    }
    const BusinessPacket *last=nullptr;
    bool candidate=false;
    for(int i=qMin(index,int(business_.size())-1);i>=0;--i){
        const auto &packet=business_[i];
        bool hasQ[4]{};
        for(const auto &field:packet.fields){
            for(int j=0;j<4;++j){
                if(((field.nameCn==QStringLiteral("解算四元数q%1").arg(j)||
                     (by04&&field.nameCn==QStringLiteral("姿态四元数q%1").arg(j)))&&field.status=="table_formula")||
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
            const bool exact=(field.nameCn==QStringLiteral("解算四元数q%1").arg(j)||
                (by04&&field.nameCn==QStringLiteral("姿态四元数q%1").arg(j)))&&field.status=="table_formula";
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
    }else if(by04){
        attitudeInfo_->setText(QStringLiteral("BY70-4 姿态四元数来自 OBC-SF 独立字段定义；参考坐标系和姿态含义仍待官方说明"));
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
            const bool exact=(field.nameCn==QStringLiteral("解算四元数q%1").arg(j)||
                (by04&&field.nameCn==QStringLiteral("姿态四元数q%1").arg(j)))&&field.status=="table_formula";
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
    if(rawFrameTable_){
        QSignalBlocker rawFrameBlocked(rawFrameTable_);
        rawFrameTable_->setRowCount(frames_.size());
        for(int i=0;i<frames_.size();++i){
            const auto &f=frames_[i];
            const QStringList cells={shortTime(f.captureLocal),f.satellite,
                f.master<0?QStringLiteral("—"):QString::number(f.master),
                f.vcid<0?QStringLiteral("—"):QString::number(f.vcid),
                QString::number(f.bytes.size()),
                f.complete?QStringLiteral("结构可读"):QStringLiteral("结构异常")};
            for(int j=0;j<cells.size();++j)rawFrameTable_->setItem(i,j,item(cells[j]));
        }
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
    QVector<orbit::ImagePacket> found=orbit::ImageTools::candidatesFromFrames(frames_);
    found+=orbit::ImageTools::candidatesFromKiss(packets_,imageApid_->value());
    const bool hasShortSsdv=std::any_of(found.cbegin(),found.cend(),[](const orbit::ImagePacket &packet){
        return packet.bytes.size()==218;
    });
    std::stable_sort(found.begin(),found.end(),[](const orbit::ImagePacket &a,const orbit::ImagePacket &b){
        if(a.imageId!=b.imageId)return a.imageId<b.imageId;
        if(a.packetId!=b.packetId)return a.packetId<b.packetId;
        return a.bytes.size()<b.bytes.size();
    });
    QSet<QString> unique;
    images_.clear();
    for(const auto &packet:found){
        const QString key=QStringLiteral("%1:%2:%3")
            .arg(packet.imageId).arg(packet.packetId).arg(packet.bytes.size());
        if(unique.contains(key))continue;
        unique.insert(key);
        images_.push_back(packet);
    }
    {
        QSignalBlocker blocker(imageTable_);
        imageTable_->setRowCount(images_.size());
        for(int i=0;i<images_.size();++i){
            const auto &packet=images_[i];
            const QStringList row={QString::number(packet.imageId),
                QString::number(packet.packetId),
                QString("%1 × %2").arg(packet.width).arg(packet.height),
                packet.status=="ssdv_crc32_pass"?QStringLiteral("SSDV CRC 通过"):
                packet.status=="ssdv_crc32_failed"?QStringLiteral("SSDV CRC 失败"):
                packet.status=="ssdv_crc_pass"?QStringLiteral("SSDV FEC/CRC 通过"):
                                                   QStringLiteral("候选，尚未校验")};
            for(int col=0;col<row.size();++col)imageTable_->setItem(i,col,item(row[col]));
        }
        imageTable_->clearSelection();
        if(!images_.isEmpty())imageTable_->setCurrentCell(0,0);
    }
    imageApid_->setEnabled(!hasShortSsdv&&resolvedSatellite()!=QStringLiteral("BY04"));
    imageInfo_->setText(QStringLiteral("%1 · 已自动扫描 %2 个图像分片，按图像 ID / 包序号排序；进入本页即可查看并自动尝试重建。")
        .arg(hasShortSsdv?QStringLiteral("VC1 218 字节 SSDV（ASRTU-1 / BY04 兼容格式）"):
             QStringLiteral("APID %1 标准 SSDV").arg(imageApid_->value()))
        .arg(images_.size()));
    if(imageMetric_)imageMetric_->setText(QString::number(images_.size()));
    decodedImage_=QPixmap();
    if(imagePreview_)imagePreview_->setImage(decodedImage_);
    if(!images_.isEmpty()){
        QSet<int> triedIds;
        int previewRow=-1;
        for(int row=0;row<images_.size();++row){
            const int imageId=images_[row].imageId;
            if(triedIds.contains(imageId))continue;
            triedIds.insert(imageId);
            if(decodeSsdvImage(imageId,false)){previewRow=row;break;}
        }
        if(previewRow>=0){
            QSignalBlocker blocker(imageTable_);
            imageTable_->setCurrentCell(previewRow,0);
            updateImageProgress(images_[previewRow].imageId);
        }else{
            updateImageProgress(images_.first().imageId);
            imageInfo_->setText(QStringLiteral("已扫描 %1 个 SSDV 分片、%2 个图像 ID；尚无图像可独立重建，仍已显示全部候选分片。")
                .arg(images_.size()).arg(triedIds.size()));
        }
    }else{
        imageCoverage_->setRange(0,1);
        imageCoverage_->setValue(0);
        imageCoverage_->setFormat(QStringLiteral("尚无观测序号"));
        imageCoverageNote_->setText(QStringLiteral("未识别到 SSDV 分片。请检查卫星、VCID/APID 和导入数据格式。"));
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
    const QString path=exportDirectoryOverride_.isEmpty()?QFileDialog::getSaveFileName(this,
       QStringLiteral("保存 SSDV 候选数据"),"satellite.ssdv",
       QStringLiteral("SSDV 数据 (*.ssdv *.bin)")):
       QDir(exportDirectoryOverride_).filePath(QStringLiteral("satellite.ssdv"));
    if(path.isEmpty())return;
    QString error;
    if(!orbit::ImageTools::savePackets(images_,path,&error))
        QMessageBox::warning(this,QStringLiteral("保存失败"),error);
    else statusBar()->showMessage(QStringLiteral("已保存候选分片；未独立完成图像 CRC 验证"),9500);
}
bool MainWindow::decodeSsdvImage(int imageId,bool showErrors){
    if(images_.isEmpty())return false;
    bool shortPacket=false;
    for(const auto &packet:images_)if(packet.imageId==imageId){
        shortPacket=packet.bytes.size()==218;
        break;
    }
    const auto rebuilt=orbit::ImageTools::decodeImage(images_,imageId,shortPacket);
    if(rebuilt.jpeg.isEmpty()){
        if(showErrors)QMessageBox::warning(this,QStringLiteral("SSDV 图片重建失败"),
            rebuilt.error.isEmpty()?QStringLiteral("当前分片无法重建 JPEG。"):rebuilt.error);
        return false;
    }
    const QImage image=QImage::fromData(rebuilt.jpeg,"JPEG");
    if(image.isNull()){
        if(showErrors)QMessageBox::warning(this,QStringLiteral("SSDV 图片重建失败"),
            QStringLiteral("解码器返回的数据不是可读取的 JPEG 图像。"));
        return false;
    }
    decodedImage_=QPixmap::fromImage(image);
    if(imagePreview_)imagePreview_->setImage(decodedImage_);
    if(imageInfo_)imageInfo_->setText(QStringLiteral(
        "图像 ID %1 · %2 × %3 · 已使用 %4 个分片 · %5。切换到其他图像 ID 可查看其他图片。")
        .arg(imageId).arg(image.width()).arg(image.height()).arg(rebuilt.receivedPackets)
        .arg(rebuilt.complete?QStringLiteral("检测到图像结束标志"):QStringLiteral("当前为可恢复预览")));
    return true;
}
void MainWindow::decodeSsdv(){
    if(images_.isEmpty())scanSsdv();
    if(images_.isEmpty()){
        QMessageBox::information(this,QStringLiteral("SSDV 图片重建"),
            QStringLiteral("没有识别到 SSDV 分片。请先导入 LOG/BIN 或检查卫星、VCID/APID 设置。"));
        return;
    }
    int row=imageTable_->currentRow();
    if(row<0||row>=images_.size())row=0;
    const int imageId=images_[row].imageId;
    if(!decodeSsdvImage(imageId,true))return;
    if(exportDirectoryOverride_.isEmpty()){
        statusBar()->showMessage(QStringLiteral("JPEG 已在软件内重建并显示；如需保存，请使用统一导出界面。"),9000);
        return;
    }
    const QString jpegName=QStringLiteral("BY70-4_ID%1.jpg").arg(imageId);
    const QString outputPath=QDir(exportDirectoryOverride_).filePath(jpegName);
    QSaveFile output(outputPath);
    if(!output.open(QIODevice::WriteOnly)){
        QMessageBox::warning(this,QStringLiteral("JPEG 保存失败"),output.errorString());
        return;
    }
    // Save the original decoded JPEG bytes rather than re-encoding the preview.
    bool shortPacket=false;
    for(const auto &packet:images_)if(packet.imageId==imageId){shortPacket=packet.bytes.size()==218;break;}
    const auto rebuilt=orbit::ImageTools::decodeImage(images_,imageId,shortPacket);
    if(rebuilt.jpeg.isEmpty()||output.write(rebuilt.jpeg)!=rebuilt.jpeg.size()||!output.commit()){
        QMessageBox::warning(this,QStringLiteral("JPEG 保存失败"),output.errorString());
        return;
    }
    statusBar()->showMessage(QStringLiteral("JPEG 重建完成并已保存：")+outputPath,9000);
}
void MainWindow::exportSelectedImageBin(){
    if(images_.isEmpty()){
        QMessageBox::information(this,QStringLiteral("导出图像 BIN"),QStringLiteral("当前没有可导出的 SSDV 分片。"));
        return;
    }
    int row=imageTable_->currentRow();
    if(row<0||row>=images_.size())row=0;
    const int imageId=images_[row].imageId;
    QVector<orbit::ImagePacket> selected;
    for(const auto &packet:images_)if(packet.imageId==imageId)selected.push_back(packet);
    if(selected.isEmpty())return;
    const QString path=QFileDialog::getSaveFileName(this,QStringLiteral("导出所选图像 BIN"),
        QStringLiteral("image_%1.bin").arg(imageId),QStringLiteral("BIN 文件 (*.bin);;所有文件 (*.*)"));
    if(path.isEmpty())return;
    QString error;
    if(!orbit::ImageTools::savePackets(selected,path,&error)){
        QMessageBox::warning(this,QStringLiteral("导出图像 BIN 失败"),error);
        return;
    }
    statusBar()->showMessage(QStringLiteral("已导出图像 ID %1 的 %2 个 SSDV 分片：%3")
        .arg(imageId).arg(selected.size()).arg(path),10000);
}
void MainWindow::importImageBin(){
    const QString path=QFileDialog::getOpenFileName(this,QStringLiteral("导入修改后的图像 BIN"),{},
        QStringLiteral("BIN / SSDV 数据 (*.bin *.ssdv);;所有文件 (*.*)"));
    if(path.isEmpty())return;
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly)){
        QMessageBox::warning(this,QStringLiteral("导入图像 BIN 失败"),file.errorString());
        return;
    }
    const QByteArray data=file.readAll();
    QVector<orbit::ImagePacket> imported;
    QString error;
    const bool prefer218=resolvedSatellite()==QStringLiteral("BY04");
    const bool likelyFrameStream=(data.size()%223==0&&data.size()%218!=0&&data.size()%256!=0);
    if(likelyFrameStream){
        QVector<orbit::Frame> rawFrames;
        for(int offset=0;offset+223<=data.size();offset+=223)
            rawFrames.push_back(orbit::Protocol::parse(data.mid(offset,223),resolvedSatellite()));
        imported=orbit::ImageTools::candidatesFromFrames(rawFrames);
        imported+=orbit::ImageTools::candidatesFromKiss(
            orbit::Transport::reassemble(rawFrames),imageApid_->value());
    }else{
        imported=orbit::ImageTools::parseBinaryPackets(data,imageApid_->value(),prefer218,&error);
    }
    if(imported.isEmpty()){
        QMessageBox::warning(this,QStringLiteral("导入图像 BIN 失败"),
            error.isEmpty()?QStringLiteral("文件中没有可用的 SSDV 分片。"):error);
        return;
    }
    std::stable_sort(imported.begin(),imported.end(),[](const orbit::ImagePacket &a,const orbit::ImagePacket &b){
        if(a.imageId!=b.imageId)return a.imageId<b.imageId;
        return a.packetId<b.packetId;
    });
    images_=imported;
    {
        QSignalBlocker blocker(imageTable_);
        imageTable_->setRowCount(images_.size());
        for(int i=0;i<images_.size();++i){
            const auto &packet=images_[i];
            const QStringList row={QString::number(packet.imageId),QString::number(packet.packetId),
                QString("%1 × %2").arg(packet.width).arg(packet.height),
                packet.status=="ssdv_crc32_pass"?QStringLiteral("SSDV CRC 通过"):
                packet.status=="ssdv_crc32_failed"?QStringLiteral("SSDV CRC 失败"):
                packet.status=="ssdv_crc_pass"?QStringLiteral("SSDV FEC/CRC 通过"):
                                                   QStringLiteral("候选，尚未校验")};
            for(int col=0;col<row.size();++col)imageTable_->setItem(i,col,item(row[col]));
        }
        imageTable_->clearSelection();
        imageTable_->setCurrentCell(0,0);
    }
    decodedImage_=QPixmap();
    imagePreview_->setImage(decodedImage_);
    imageInfo_->setText(QStringLiteral("已导入 %1 · %2 个 SSDV 分片；当前列表来自导入 BIN，选择图像 ID 将自动重建预览。")
        .arg(QFileInfo(path).fileName()).arg(images_.size()));
    if(imageMetric_)imageMetric_->setText(QString::number(images_.size()));
    QSet<int> triedIds;
    bool decodedAny=false;
    int previewRow=0;
    for(int row=0;row<images_.size();++row){
        const int imageId=images_[row].imageId;
        if(triedIds.contains(imageId))continue;
        triedIds.insert(imageId);
        if(decodeSsdvImage(imageId,false)){decodedAny=true;previewRow=row;break;}
    }
    {
        QSignalBlocker blocker(imageTable_);
        imageTable_->setCurrentCell(previewRow,0);
    }
    updateImageProgress(images_[previewRow].imageId);
    if(!decodedAny)
        statusBar()->showMessage(QStringLiteral("已载入 BIN 分片，但当前图像 ID 尚不能完整重建；全部候选已保留，可继续导入更多分片或检查 CRC。"),10000);
    else
        statusBar()->showMessage(QStringLiteral("图像 BIN 已载入并自动解码：")+path,9000);
}

void MainWindow::inspectRawFrame(int index){
    if(!rawFrameHex_)return;
    if(index<0||index>=frames_.size()){rawFrameHex_->clear();return;}
    const auto &f=frames_[index];
    QString result=QStringLiteral("接收时间：%1\n卫星：%2\n主帧计数：%3\nVCID：%4\n帧长度：%5 字节\n结构：%6\nSHA-256：%7")
        .arg(utcLabel(f.captureLocal),f.satellite)
        .arg(f.master).arg(f.vcid).arg(f.bytes.size())
        .arg(f.complete?QStringLiteral("可读"):QStringLiteral("异常"),f.hash);
    result+=QStringLiteral("\n\n完整原始帧 HEX（每行 16 字节，左侧为十六进制偏移）：\n");
    for(int offset=0;offset<f.bytes.size();offset+=16){
        result+=QStringLiteral("%1  %2\n")
            .arg(offset,4,16,QChar('0')).arg(QString::fromLatin1(f.bytes.mid(offset,16).toHex(' ').toUpper()));
    }
    rawFrameHex_->setPlainText(result);
}
void MainWindow::exportFrames(bool csv){
    if(frames_.isEmpty())return;
    const QString defaultName=csv?QStringLiteral("frames.csv"):QStringLiteral("frames.json");
    const QString path=exportDirectoryOverride_.isEmpty()?QFileDialog::getSaveFileName(this,QStringLiteral("导出原始传输帧"),
        defaultName,csv?QStringLiteral("CSV (*.csv)"):QStringLiteral("JSON (*.json)")):
        QDir(exportDirectoryOverride_).filePath(defaultName);
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
    const QString defaultName=csv?QStringLiteral("engineering.csv"):QStringLiteral("engineering.json");
    const QString path=exportDirectoryOverride_.isEmpty()?QFileDialog::getSaveFileName(this,
        QStringLiteral("导出业务遥测字段"),defaultName,
        csv?QStringLiteral("CSV (*.csv)"):QStringLiteral("JSON (*.json)")):
        QDir(exportDirectoryOverride_).filePath(defaultName);
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
    const QString defaultName=experimental?QStringLiteral("candidate-quaternion.csv"):
                                            QStringLiteral("SAT-S3-quaternions.csv");
    const QString path=exportDirectoryOverride_.isEmpty()?QFileDialog::getSaveFileName(this,
        experimental?QStringLiteral("实验性候选四元数（非正式姿态）"):
                     QStringLiteral("SAT-S3 解算四元数"),defaultName,
        QStringLiteral("CSV (*.csv)")):
        QDir(exportDirectoryOverride_).filePath(defaultName);
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
    const QString path=exportDirectoryOverride_.isEmpty()?QFileDialog::getSaveFileName(this,QStringLiteral("导出 KISS 应用包"),
                                                     "kiss-packets.json",QStringLiteral("JSON (*.json)")):
        QDir(exportDirectoryOverride_).filePath(QStringLiteral("kiss-packets.json"));
    if(path.isEmpty())return;
    QString error;
    if(!orbit::Transport::exportJson(path,packets_,&error))
        QMessageBox::warning(this,QStringLiteral("导出失败"),error);
    else statusBar()->showMessage(QStringLiteral("KISS 应用包已导出"),8000);
}
