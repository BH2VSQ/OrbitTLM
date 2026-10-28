#include "mainwindow.h"
#include "mainwindow_widgets.h"
#include "by04.h"
#include <QApplication>
#include <QChart>
#include <QChartView>
#include <QCheckBox>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QColor>
#include <QDesktopServices>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
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
}
QString MainWindow::satellite() const{
    if(!satelliteSelect_)return "ASRTU-1";
    const QString selected=satelliteSelect_->currentData().toString();
    return selected=="AUTO"?detectedSatellite_:selected;
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
        return QStringLiteral("BY04 · 未分类 KISS 业务包");
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
    const bool autoDetect=satelliteSelect_->currentData().toString()=="AUTO";
    const QString parseAs=autoDetect?QStringLiteral("ASRTU-1"):satellite();
    auto log=logPath_.isEmpty()?QVector<orbit::Frame>{}:
        orbit::Protocol::fromLog(logPath_,parseAs,&error);
    if(!error.isEmpty()){
        QMessageBox::warning(this,QStringLiteral("日志读取失败"),error);return;
    }
    auto bin=binPath_.isEmpty()?QVector<orbit::Frame>{}:
        orbit::Protocol::fromBin(binPath_,parseAs,&error);
    if(!error.isEmpty()){
        QMessageBox::warning(this,QStringLiteral("二进制读取失败"),error);return;
    }
    if(autoDetect){
        int asrtu=0,by04=0;
        auto countScid=[&](const QVector<orbit::Frame> &items){
            for(const auto &frame:items){
                if(frame.bytes.size()!=223)continue;
                const int scid=((quint8(frame.bytes[0])&0x3f)<<4)|(quint8(frame.bytes[1])>>4);
                if(scid==0x205)++by04;
                else if(scid==0x032)++asrtu;
            }
        };
        countScid(log);countScid(bin);
        if(by04==0&&asrtu==0){
            QMessageBox::warning(this,QStringLiteral("自动识别失败"),
                QStringLiteral("导入的数据中没有发现已支持的航天器标识（ASRTU-1：0x032，BY04：0x205）。请手动选择卫星。"));
            return;
        }
        detectedSatellite_=by04>asrtu?QStringLiteral("BY04"):QStringLiteral("ASRTU-1");
        const QString satName=detectedSatellite_;
        auto reparse=[&](QVector<orbit::Frame> &items){
            for(auto &frame:items){
                const auto sources=frame.sources;
                const auto captured=frame.captureLocal;
                const auto basis=frame.timeBasis;
                const auto quality=frame.quality;
                auto parsed=orbit::Protocol::parse(frame.bytes,satName);
                parsed.sources=sources;parsed.captureLocal=captured;parsed.timeBasis=basis;
                parsed.quality=quality;parsed.complete=frame.complete;
                frame=std::move(parsed);
            }
        };
        reparse(log);reparse(bin);
        statusBar()->showMessage(QStringLiteral("自动识别结果：%1（ASRTU-1 %2 帧，BY04 %3 帧）")
            .arg(satName).arg(asrtu).arg(by04),10000);
    }
    const QString sat=satellite();
    frames_=orbit::Protocol::fuse(log,bin,&frameStats_);
    if(hasCustomSchema_&&customSchema_.satellite()==sat)
        for(auto &f:frames_)customSchema_.apply(f);
    rebuildSession();
    if(!db_.save(sat+QStringLiteral(" · LOG/BIN 离线会话"),frames_,&error))
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
    business_=catalogReady_&&satellite()=="ASRTU-1"?
        catalog_.decodeAll(packets_,&bs):
        (satellite()=="BY04"?orbit::By04Decoder::decodeAll(packets_):
                             QVector<BusinessPacket>{});
    // BY04 headers are decoded independently; its physical engineering field
    // maps must not be borrowed from the ASRTU-1 catalog.
    if(business_.isEmpty()&&!packets_.isEmpty()){
        business_.reserve(packets_.size());
        for(const auto &p:packets_){
            BusinessPacket event;event.transport=p;
            event.packetLabel=QStringLiteral("待定义的卫星业务包");
            business_.push_back(event);
        }
    }
    populateEvents();
    updateFrameList();
    scanSsdv();
    updateMetrics();
    timeline_->setRange(0,std::max(0,int(business_.size())-1));
    currentIndex_=-1;
    if(!business_.isEmpty())selectEvent(0);
    else showEvent(-1);
    updateHtmlHistory();
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
            QStringLiteral("CCSDS 包头已解析 · 工程量待定义"):
            (b.packetName.isEmpty()?QStringLiteral("类型未知"):
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
}
void MainWindow::updateMetrics(){
    QDateTime earliest,latest;
    for(const auto &f:frames_)if(f.captureLocal.isValid()){
        if(!earliest.isValid()||f.captureLocal<earliest)earliest=f.captureLocal;
        if(!latest.isValid()||f.captureLocal>latest)latest=f.captureLocal;
    }
    if(earliest.isValid()){
        timeMetric_->setText(earliest.date()==latest.date()?
            earliest.toString("HH:mm")+" — "+latest.toString("HH:mm"):
            earliest.toString("MM-dd HH:mm")+"—"+latest.toString("MM-dd HH:mm"));
    }else timeMetric_->setText(QStringLiteral("未记录"));
    frameMetric_->setText(QString::number(frames_.size()));
    int good=0;
    for(const auto &p:business_)
        if(!p.packetName.isEmpty())++good;
    const bool by04=satellite()=="BY04";
    recognizedOnly_->setText(by04?QStringLiteral("仅显示已解析包头"):
                                  QStringLiteral("仅显示已识别数据"));
    auto label=[](QLabel *value){
        return value->parentWidget()->findChild<QLabel*>("subtitle");
    };
    if(auto *caption=label(recognizedMetric_))
        caption->setText(by04?QStringLiteral("已解析 CCSDS 包头"):
                             QStringLiteral("已识别业务包"));
    if(auto *caption=label(unknownMetric_))
        caption->setText(by04?QStringLiteral("未分类 KISS 业务包"):
                             QStringLiteral("未识别业务包"));
    recognizedMetric_->setText(QString::number(good));
    unknownMetric_->setText(QString::number(business_.size()-good));
    imageMetric_->setText(QString::number(images_.size()));
    transportMetric_->setText(QStringLiteral("%1 包").arg(packets_.size()));
    message_->setText(satellite()=="BY04"?
        QStringLiteral("BY04 · KISS %1 包 · 工程字段表待确认 · VC1 原生 SSDV %2 包")
            .arg(packets_.size()).arg(images_.size()):
        QStringLiteral("KISS 已重组：%1 · 应用 CRC 未校验 · 缺帧中断：%2")
            .arg(packets_.size()).arg(transportStats_.interruptedByCounterGap));
}
void MainWindow::updateOverview(int index){
    if(index<0||index>=business_.size()){
        currentTitle_->setText(QStringLiteral("尚未载入可回放的业务数据"));
        currentMeta_->setText(QStringLiteral("通过上方工具栏导入 LOG 与 BIN，然后开始回放"));
        integrityLabel_->setText(QStringLiteral("星载 UTC：未知 · 应用 CRC：未校验"));
        attitudeModeLabel_->setText(QStringLiteral("当前姿态模式：等待遥测"));
    }else{
        const auto &b=business_[index];
        currentTitle_->setText(eventTitle(b));
        currentMeta_->setText(QStringLiteral("记录时间：%1　　来源：%2　　虚拟信道：%3")
            .arg(utcLabel(b.transport.captureLocal),sourceLabel(b.transport.sources))
            .arg(b.transport.vcid));
        integrityLabel_->setText(QStringLiteral("传输状态：%1　　应用 CRC：未校验　　星载 UTC：未知")
           .arg(qualityLabel(b.transport.fecEvidence)));
        QString mode;
        QDateTime modeTime;
        for(int i=index;i>=0&&mode.isEmpty();--i){
            const auto &packet=business_[i];
            if(packet.packetName.isEmpty())continue;
            for(const auto &field:packet.fields){
                if(field.nameCn.contains(QStringLiteral("姿态工作模式")) &&
                   !field.value.isEmpty() && !field.status.startsWith("raw_")){
                    mode=field.value;
                    modeTime=packet.transport.captureLocal;
                    break;
                }
            }
        }
        if(mode.isEmpty())
            attitudeModeLabel_->setText(QStringLiteral("当前姿态模式：暂无有效的姿态工作模式字段"));
        else
            attitudeModeLabel_->setText(QStringLiteral("当前姿态模式：%1　　更新时间：%2")
                .arg(mode.toHtmlEscaped(),shortTime(modeTime).toHtmlEscaped()));
    }
    updateFieldCard(powerCard_,{"PDB","SAT"},index);
    updateFieldCard(attitudeCard_,{"SAT","ZGK","SSY","SSZ","GPS"},index);
    updateFieldCard(thermalCard_,{"TCB"},index);
    updateFieldCard(payloadCard_,{"CAM","CMS","TTC","BBB"},index);
}
void MainWindow::updateFieldCard(QGroupBox *card,const QStringList &prefixes,int index){
    auto *content=card->findChild<QLabel*>("statusContent");
    if(!content)return;
    if(index<0||business_.isEmpty()){
        content->setText(QStringLiteral("暂无已识别的遥测参数<br><span style='color:#8190a3'>等待匹配分系统数据包</span>"));
        return;
    }
    auto belongsToCard=[&](const BusinessPacket &packet,const EngineeringField &field){
        bool packetMatch=false;
        for(const auto &prefix:prefixes)
            if(packet.packetName.startsWith(prefix)){packetMatch=true;break;}
        if(!packetMatch)return false;
        const QString n=field.nameCn;
        if(card==powerCard_)
            return n.contains(QStringLiteral("电压"))||n.contains(QStringLiteral("电流"))||
                   n.contains(QStringLiteral("电源"))||n.contains(QStringLiteral("配电"))||
                   n.contains(QStringLiteral("电池"))||n.contains(QStringLiteral("充电"))||
                   n.contains(QStringLiteral("放电"))||n.contains(QStringLiteral("开关"))||
                   n.contains(QStringLiteral("功率"));
        if(card==attitudeCard_)
            return n.contains(QStringLiteral("姿态"))||n.contains(QStringLiteral("角速度"))||
                   n.contains(QStringLiteral("四元数"))||n.contains(QStringLiteral("飞轮"))||
                   n.contains(QStringLiteral("磁场"))||n.contains(QStringLiteral("星敏"))||
                   n.contains(QStringLiteral("太阳敏感"))||n.contains(QStringLiteral("轨道"))||
                   n.contains(QStringLiteral("工作模式"))||n.contains(QStringLiteral("控制模式"))||
                   n.contains(QStringLiteral("控制状态"))||n.contains(QStringLiteral("陀螺"));
        if(card==thermalCard_)
            return n.contains(QStringLiteral("温度"))||n.contains(QStringLiteral("热控"))||
                   n.contains(QStringLiteral("加热"))||n.contains(QStringLiteral("热敏"));
        if(card==payloadCard_)
            return n.contains(QStringLiteral("状态"))||n.contains(QStringLiteral("模式"))||
                   n.contains(QStringLiteral("开关"))||n.contains(QStringLiteral("工作"))||
                   n.contains(QStringLiteral("故障"))||n.contains(QStringLiteral("错误"))||
                   n.contains(QStringLiteral("成像"))||n.contains(QStringLiteral("载荷"))||
                   n.contains(QStringLiteral("通信"))||n.contains(QStringLiteral("发射"))||
                   n.contains(QStringLiteral("接收"))||n.contains(QStringLiteral("温度"))||
                   n.contains(QStringLiteral("电压"))||n.contains(QStringLiteral("电流"));
        return false;
    };
    // Keep the newest value for every packet-field pair at the current replay position.
    QStringList order;
    QHash<QString,EngineeringField> latest;
    QHash<QString,QString> source;
    QHash<QString,QDateTime> times;
    for(int i=0;i<=std::min(index,int(business_.size())-1);++i){
        const auto &packet=business_[i];
        if(packet.packetName.isEmpty())continue;
        for(const auto &field:packet.fields){
            if(!belongsToCard(packet,field)||field.nameCn.isEmpty()||field.value.isEmpty()||
               field.status.startsWith("raw_")||field.status=="invalid_source_bit_position")continue;
            const QString key=packet.packetName+QStringLiteral("/")+field.code;
            if(!latest.contains(key))order.push_back(key);
            latest.insert(key,field);
            source.insert(key,packet.packetName);
            times.insert(key,packet.transport.captureLocal);
        }
    }
    if(order.isEmpty()){
        content->setText(QStringLiteral("暂无符合本分区的有效工程量<br><span style='color:#8190a3'>可在“工程量”页查看全部已识别字段</span>"));
        return;
    }
    // Most recent packets are encountered last. Render recent values first, with
    // state/mode fields promoted above ordinary numeric measurements.
    std::reverse(order.begin(),order.end());
    std::stable_sort(order.begin(),order.end(),[&](const QString &a,const QString &b){
        auto priority=[&](const QString &key){
            const QString n=latest.value(key).nameCn;
            return (n.contains(QStringLiteral("姿态工作模式"))||n.contains(QStringLiteral("工作模式"))||
                    n.contains(QStringLiteral("控制模式"))||n.contains(QStringLiteral("状态"))||
                    n.contains(QStringLiteral("故障")))?0:1;
        };
        return priority(a)<priority(b);
    });
    const int totalFields=order.size();
    const int visibleFields=std::min(totalFields,8);
    QString html=QStringLiteral("<div style='color:#68809b;margin-bottom:4px'>最新有效值 · 单位按字段表</div>");
    for(int fieldIndex=0;fieldIndex<visibleFields;++fieldIndex){
        const QString key=order[fieldIndex];
        const auto field=latest.value(key);
        QString value=field.value.toHtmlEscaped();
        if(!field.unit.isEmpty())value+=QStringLiteral(" ")+field.unit.toHtmlEscaped();
        html+=QStringLiteral("<table width='100%' cellspacing='0' cellpadding='3' style='border-bottom:1px solid #edf2f7'>"
                             "<tr><td width='57%' valign='top'><b>%1</b><br><span style='color:#8190a3;font-size:10px'>%2 · %3</span></td>"
                             "<td valign='top' align='right'><b>%4</b></td></tr></table>")
            .arg(field.nameCn.toHtmlEscaped(),source.value(key).toHtmlEscaped(),
                 shortTime(times.value(key)).toHtmlEscaped(),value);
    }
    html+=QStringLiteral("<div style='color:#8798aa;margin-top:4px'>显示 %1 / %2 项 · 应用 CRC 未校验</div>")
        .arg(visibleFields).arg(totalFields);
    content->setText(html);
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
        QStringLiteral("%1 · CCSDS 包头已解析 · %2 项协议字段 · BY04 工程遥测字段仍待确认")
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
    if(satellite()=="BY04"){
        for(auto *q:quaternion_)if(q)q->setText(QStringLiteral("—"));
        normLabel_->setText(QStringLiteral("—"));
        attitudeWidget_->setQuaternion(QQuaternion(),false);
        attitudeInfo_->setText(QStringLiteral(
            "BY04 · 航天器姿态工程字段映射尚未确认。"
            "不使用 ASRTU-1 SAT-S3 四元数位置，也不推断轨道或极化角。"));
        auto *chart=new QChart;
        chart->setTitle(QStringLiteral("BY04 姿态时序：等待专用工程遥测字段表"));
        attitudeChart_->setChart(chart);
        return;
    }
    const auto *last=latestRecognized(index,{"SAT-S3"});
    bool has[4]{false,false,false,false};
    double q[4]{};
    if(last){
        for(const auto &f:last->fields)for(int j=0;j<4;j++)
            if(f.nameCn==QStringLiteral("解算四元数q%1").arg(j)&&
               f.status=="table_formula"){
                bool ok=false;const double value=f.value.toDouble(&ok);
                if(ok&&std::isfinite(value)){q[j]=value;has[j]=true;}
            }
    }
    const bool ready=std::all_of(std::begin(has),std::end(has),[](bool a){return a;});
    double norm=0;
    for(int i=0;i<4;i++){
        quaternion_[i]->setText(ready?QString::number(q[i],'f',5):QStringLiteral("—"));
        norm+=q[i]*q[i];
    }
    norm=std::sqrt(norm);
    normLabel_->setText(ready?QString::number(norm,'f',6):QStringLiteral("—"));
    attitudeWidget_->setQuaternion(
        QQuaternion(float(q[0]),float(q[1]),float(q[2]),float(q[3])),
        ready&&norm>1e-9);
    attitudeInfo_->setText(ready?
        QStringLiteral("SAT-S3 四元数 · 记录时间：%1 · 数据来源：%2\n"
          "数值按公开字段表换算；假设 q0 为标量仅作相对图形示意，参考系、天线安装矩阵及 CRC 均未核实")
            .arg(shortTime(last->transport.captureLocal),
                 qualityLabel(last->transport.fecEvidence)):
        QStringLiteral("当前时段暂无 SAT-S3 完整四元数；不会从未知业务包推断卫星姿态"));
    auto *chart=new QChart;
    chart->setBackgroundVisible(false);
    QVector<QLineSeries*> series;
    for(int k=0;k<4;k++){auto *s=new QLineSeries;s->setName(QString("q%1").arg(k));series.push_back(s);}
    QDateTime first;
    if(index>=0)for(int i=0;i<=index&&i<business_.size();++i){
        const auto &p=business_[i];
        if(p.packetName!="SAT-S3")continue;
        double values[4]{};bool found[4]{};
        for(const auto &f:p.fields)for(int j=0;j<4;j++)
            if(f.nameCn==QStringLiteral("解算四元数q%1").arg(j)&&f.status=="table_formula"){
                bool ok=false;const double val=f.value.toDouble(&ok);
                if(ok&&std::isfinite(val)){values[j]=val;found[j]=true;}
            }
        if(!std::all_of(std::begin(found),std::end(found),[](bool x){return x;}))continue;
        if(!first.isValid()&&p.transport.captureLocal.isValid())first=p.transport.captureLocal;
        const qreal x=(first.isValid()&&p.transport.captureLocal.isValid())?
            qreal(first.msecsTo(p.transport.captureLocal))/1000.0:qreal(i);
        for(int j=0;j<4;j++)series[j]->append(x,values[j]);
    }
    const bool nonempty=series[0]->count()>0;
    for(auto *s:series){if(nonempty)chart->addSeries(s);else delete s;}
    if(nonempty){chart->createDefaultAxes();chart->legend()->setVisible(true);}
    chart->setTitle(nonempty?QStringLiteral("解算四元数时序（相对记录时间）"):
        QStringLiteral("暂无已识别的 SAT-S3 时序数据"));
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
    const bool by04=satellite()=="BY04";
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
        QStringLiteral("APID %1：发现 %2 个 SSDV 候选分片；JPEG 还原需要外部解码工具。")
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
    const QString tool=QFileDialog::getOpenFileName(this,
        QStringLiteral("选择可信任的 SSDV 外部解码程序"));
    if(tool.isEmpty())return;
    const QString input=QFileDialog::getOpenFileName(this,
        QStringLiteral("选择已提取的 SSDV 数据文件"),{},
        QStringLiteral("SSDV (*.ssdv *.bin);;全部文件 (*)"));
    if(input.isEmpty())return;
    const QString output=QFileDialog::getSaveFileName(this,
        QStringLiteral("指定 JPEG 输出文件"),"decoded.jpg",
        QStringLiteral("JPEG 图像 (*.jpg *.jpeg)"));
    if(output.isEmpty())return;
    auto *process=new QProcess(this);
    connect(process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,
        [this,process,output](int exitCode,QProcess::ExitStatus status){
            QPixmap pixmap(output);
            if(exitCode==0&&status==QProcess::NormalExit&&!pixmap.isNull()){
                decodedImage_=pixmap;imagePath_=output;
                imagePreview_->setImage(decodedImage_);
                imageInfo_->setText(QStringLiteral("JPEG 已由外部工具生成：%1 × %2 像素。"
                    "这不代表星载应用 CRC 已经验证。")
                    .arg(pixmap.width()).arg(pixmap.height()));
                statusBar()->showMessage(QStringLiteral("外部 JPEG 解码完成"),9000);
            }else{
                QMessageBox::warning(this,QStringLiteral("SSDV 解码失败"),
                    QStringLiteral("外部工具未成功输出可读取的 JPEG。\n")+
                    QString::fromUtf8(process->readAllStandardError()));
            }
            process->deleteLater();
        });
    process->start(tool,{"-d",input,output});
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
QString MainWindow::htmlHistoryPath() const{
    return QDir(QApplication::applicationDirPath()).filePath(QStringLiteral("data/index.html"));
}
void MainWindow::updateHtmlHistory(){
    const QString dataDir=QDir(QApplication::applicationDirPath()).filePath(QStringLiteral("data"));
    if(!QDir().mkpath(dataDir))return;
    const QString jsonPath=QDir(dataDir).filePath(QStringLiteral("telemetry.json"));
    QJsonArray samples;
    QFile existing(jsonPath);
    if(existing.open(QIODevice::ReadOnly)){
        const auto doc=QJsonDocument::fromJson(existing.readAll());
        if(doc.isArray())samples=doc.array();
    }
    QSet<QString> keys;
    for(const auto &v:samples){
        const auto o=v.toObject();
        keys.insert(o.value("key").toString());
    }
    for(const auto &packet:business_){
        const QString time=packet.transport.captureLocal.isValid()?
            packet.transport.captureLocal.toString(Qt::ISODateWithMs):QString();
        if(time.isEmpty())continue;
        for(const auto &field:packet.fields){
            if(field.nameCn.isEmpty()||field.value.isEmpty()||field.status.startsWith("raw_"))continue;
            const QString key=QStringLiteral("%1|%2|%3|%4|%5")
                .arg(satellite(),time,packet.packetName,field.code,field.value);
            if(keys.contains(key))continue;
            keys.insert(key);
            samples.append(QJsonObject{{"key",key},{"time",time},{"satellite",satellite()},
                {"packet",packet.packetName},{"code",field.code},{"name",field.nameCn},
                {"value",field.value},{"numeric",field.value.toDouble()},
                {"isNumeric",[&field]{bool ok=false;field.value.toDouble(&ok);return ok;}()},
                {"unit",field.unit},{"status",field.status}});
        }
    }
    QSaveFile jsonOut(jsonPath);
    if(jsonOut.open(QIODevice::WriteOnly)){
        jsonOut.write(QJsonDocument(samples).toJson(QJsonDocument::Indented));
        jsonOut.commit();
    }
    QString dataJson=QString::fromUtf8(QJsonDocument(samples).toJson(QJsonDocument::Compact));
    dataJson.replace(QStringLiteral("<"),QStringLiteral("\\u003c"));
    const QString html=QString::fromUtf8(R"HTML(<!doctype html>
<html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>OrbitTLM 遥测历史</title><style>
:root{font-family:Segoe UI,"Microsoft YaHei",sans-serif;color:#1f3045;background:#f3f6fb}body{margin:0;padding:22px}h1{font-size:24px;margin:0 0 6px}.muted{color:#71839a;font-size:13px}.panel{background:#fff;border:1px solid #e1e8f1;border-radius:12px;padding:16px;margin:14px 0;box-shadow:0 3px 14px #1b3a5c0a}.filters{display:flex;gap:10px;flex-wrap:wrap;align-items:end}label{display:flex;flex-direction:column;gap:5px;color:#60758c;font-size:12px}input,select,button{padding:8px 10px;border:1px solid #d4deeb;border-radius:7px;background:#fff;color:#203750}button{cursor:pointer;background:#256cb4;color:white;border-color:#256cb4}canvas{width:100%;height:300px}table{border-collapse:collapse;width:100%;font-size:13px}th,td{text-align:left;padding:8px;border-bottom:1px solid #edf1f6}th{color:#61768d;background:#f7f9fc}.stats{display:flex;gap:22px;flex-wrap:wrap}.stat strong{font-size:20px;display:block;color:#174f83}#empty{padding:20px;color:#71839a}
</style></head><body><h1>OrbitTLM · 遥测历史</h1><div class="muted">数据来自程序本地保存的已识别遥测字段。时间为接收端记录时间，未经确认时区不代表星载 UTC。</div>
<div class="panel filters"><label>卫星<select id="sat"></select></label><label>参数<select id="field"></select></label><label>开始时间<input type="datetime-local" id="from"></label><label>结束时间<input type="datetime-local" id="to"></label><button id="apply">应用筛选</button><button id="csv">导出筛选 CSV</button></div>
<div class="panel"><div class="stats"><div class="stat"><span class="muted">样本数</span><strong id="count">0</strong></div><div class="stat"><span class="muted">最小值</span><strong id="min">—</strong></div><div class="stat"><span class="muted">最大值</span><strong id="max">—</strong></div><div class="stat"><span class="muted">平均值</span><strong id="avg">—</strong></div></div><canvas id="chart" width="1200" height="320"></canvas><div id="empty"></div></div>
<div class="panel"><h3>筛选后的数据记录</h3><div style="overflow:auto;max-height:420px"><table><thead><tr><th>记录时间</th><th>卫星</th><th>参数</th><th>分系统</th><th>数值</th><th>单位</th><th>状态</th></tr></thead><tbody id="rows"></tbody></table></div></div>
<script>const DATA=__DATA__;const $=id=>document.getElementById(id);const fmt=n=>Number.isFinite(n)?Number(n.toPrecision(7)).toString():'—';
function init(){const sats=[...new Set(DATA.map(x=>x.satellite))].sort();$('sat').innerHTML='<option value="">全部卫星</option>'+sats.map(x=>`<option>${esc(x)}</option>`).join('');const times=DATA.map(x=>x.time).filter(Boolean).sort();if(times.length){$('from').value=times[0].slice(0,16);$('to').value=times[times.length-1].slice(0,16)}updateFields();render()}
function esc(s){return String(s??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]))}
function updateFields(){const old=$('field').value;const sat=$('sat').value;const vals=[...new Map(DATA.filter(x=>!sat||x.satellite===sat).map(x=>[x.satellite+'|'+x.packet+'|'+x.code,{id:x.satellite+'|'+x.packet+'|'+x.code,label:x.name+' · '+x.packet+' · '+x.satellite}])).values()];$('field').innerHTML='<option value="">全部参数</option>'+vals.map(x=>`<option value="${esc(x.id)}">${esc(x.label)}</option>`).join('');if(vals.some(x=>x.id===old))$('field').value=old}
function selected(){const from=$('from').value?new Date($('from').value).getTime():-Infinity;const to=$('to').value?new Date($('to').value).getTime()+59999:Infinity;return DATA.filter(x=>{const t=new Date(x.time).getTime();return (! $('sat').value||x.satellite===$('sat').value)&&(!$('field').value||$('field').value===x.satellite+'|'+x.packet+'|'+x.code)&&t>=from&&t<=to})}
function render(){const rows=selected();$('count').textContent=rows.length;const nums=rows.filter(x=>x.isNumeric).map(x=>Number(x.numeric));$('min').textContent=nums.length?fmt(Math.min(...nums)):'—';$('max').textContent=nums.length?fmt(Math.max(...nums)):'—';$('avg').textContent=nums.length?fmt(nums.reduce((a,b)=>a+b,0)/nums.length):'—';$('rows').innerHTML=rows.slice().reverse().map(x=>`<tr><td>${esc(x.time)}</td><td>${esc(x.satellite)}</td><td>${esc(x.name)}</td><td>${esc(x.packet)}</td><td>${esc(x.value)}</td><td>${esc(x.unit)}</td><td>${esc(x.status)}</td></tr>`).join('');draw(rows.filter(x=>x.isNumeric));$('empty').textContent=nums.length?'数值型样本绘图；非数值状态仍保留在下方记录表。':'当前筛选没有可绘制的数值型样本。'}
function draw(rows){const c=$('chart'),ctx=c.getContext('2d'),w=c.width,h=c.height;ctx.clearRect(0,0,w,h);if(!rows.length)return;const vals=rows.map(x=>Number(x.numeric)),min=Math.min(...vals),max=Math.max(...vals),span=max-min||1;ctx.strokeStyle='#dce5ef';ctx.lineWidth=1;for(let i=0;i<5;i++){const y=20+(h-50)*i/4;ctx.beginPath();ctx.moveTo(48,y);ctx.lineTo(w-12,y);ctx.stroke();ctx.fillStyle='#71839a';ctx.fillText(fmt(max-(max-min)*i/4),4,y+4)}ctx.strokeStyle='#287bc0';ctx.lineWidth=2;ctx.beginPath();rows.forEach((x,i)=>{const px=48+(w-64)*(i/Math.max(1,rows.length-1)),py=20+(h-50)*(max-Number(x.numeric))/span;i?ctx.lineTo(px,py):ctx.moveTo(px,py)});ctx.stroke();ctx.fillStyle='#71839a';ctx.fillText(rows[0].time,48,h-8);ctx.fillText(rows[rows.length-1].time,w-180,h-8)}
function csv(){const rows=selected(),cols=['time','satellite','packet','code','name','value','unit','status'];const quote=v=>'"'+String(v??'').replace(/"/g,'""')+'"';const text=[cols.join(','),...rows.map(x=>cols.map(k=>quote(x[k])).join(','))].join('\r\n');const a=document.createElement('a');a.href=URL.createObjectURL(new Blob(['\ufeff'+text],{type:'text/csv;charset=utf-8'}));a.download='telemetry-filtered.csv';a.click();URL.revokeObjectURL(a.href)}$('sat').addEventListener('change',()=>{updateFields();render()});$('field').addEventListener('change',render);$('apply').addEventListener('click',render);$('csv').addEventListener('click',csv);init();</script></body></html>)HTML");
    QString page=html;
    page.replace(QStringLiteral("__DATA__"),dataJson);
    QSaveFile htmlOut(htmlHistoryPath());
    if(htmlOut.open(QIODevice::WriteOnly|QIODevice::Text)){
        htmlOut.write(page.toUtf8());htmlOut.commit();
    }
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
