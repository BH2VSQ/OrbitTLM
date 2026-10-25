#include "core.h"
#include "schema.h"
#include <QFileInfo>
#include <QSignalBlocker>
#include <QApplication>
#include <QAction>
#include <QChart>
#include <QChartView>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QLineSeries>
#include <QMainWindow>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QQuaternion>
#include <QSlider>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <QVector3D>
#include <QWidget>
#include <algorithm>
#include <cmath>
using namespace orbit;
class AttitudeView final: public QWidget {
public:
    explicit AttitudeView(QWidget *parent=nullptr):QWidget(parent) {setMinimumHeight(220);}
    void display(const Frame &f) {
        quaternion_ = QQuaternion();
        valid_=false;
        if(f.quality!="log_fec_reported") {update();return;}
        double q[4]{};int seen=0;
        for(const auto &x:f.fields)for(int i=0;i<4;++i)
            if(x.name==QString("candidate.quaternion.q%1").arg(i)){
                bool ok=false;q[i]=x.value.toDouble(&ok);if(ok)++seen;
            }
        if(seen==4){
            quaternion_=QQuaternion(float(q[0]),float(q[1]),float(q[2]),float(q[3])).normalized();
            valid_=true;
        }
        update();
    }
protected:
    void paintEvent(QPaintEvent *)override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(),palette().window());
        p.setPen(palette().text().color());
        p.drawText(QRect(12,8,width()-24,42),Qt::AlignTop|Qt::TextWordWrap,
           "Unverified quaternion candidate — body frame / antenna alignment unknown");
        if(!valid_){
            p.drawText(rect().adjusted(8,40,-8,-8),Qt::AlignCenter,"No FEC-reported candidate");
            return;
        }
        constexpr QVector3D v[8]={
            {-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},
            {-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}};
        constexpr int edges[12][2]={{0,1},{1,2},{2,3},{3,0},
            {4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
        const QPointF mid(width()*0.5,height()*0.59);
        const qreal scale=std::min(width(),height())*0.21;
        QPointF projected[8];
        for(int i=0;i<8;++i){
            QVector3D q=quaternion_.rotatedVector(v[i]);
            const qreal z=3.5+q.z();
            projected[i]=mid+QPointF(q.x()*scale*3.5/z,-q.y()*scale*3.5/z);
        }
        p.setPen(QPen(QColor("#4f8cc9"),2.0));
        for(const auto &edge:edges)p.drawLine(projected[edge[0]],projected[edge[1]]);
    }
private:
    QQuaternion quaternion_;
    bool valid_=false;
};
class OrbitWindow final:public QMainWindow {
public:
    OrbitWindow() {
        setWindowTitle("OrbitTLM — Satellite Telemetry Workbench");
        resize(1400,850);
        live_=new LiveTcp(this);
        const auto menu=menuBar()->addMenu("&File");
        menu->addAction("Choose decoder.log",[this]{
            logPath_=QFileDialog::getOpenFileName(this,"Select decoder.log",{},"Logs (*.log *.txt);;All files (*)");
            updatePaths();
        });
        menu->addAction("Choose 223-byte BIN",[this]{
            binPath_=QFileDialog::getOpenFileName(this,"Select 223-byte binary",{},"Binary (*.bin);;All files (*)");
            updatePaths();
        });
        menu->addAction("Import chosen sources",[this]{loadSession();});
        menu->addAction("Load telemetry schema...",[this]{
            const QString path=QFileDialog::getOpenFileName(this,"Load JSON telemetry schema",{},"JSON (*.json)");
            if(path.isEmpty())return;
            Schema candidate;QString why;
            if(!candidate.load(path,&why)){QMessageBox::warning(this,"Invalid schema",why);return;}
            if(candidate.satellite()!=selectedSatellite()){
                QMessageBox::warning(this,"Satellite mismatch","Schema is for "+candidate.satellite());return;
            }
            schema_=candidate;hasSchema_=true;
            for(auto &frame:frames_)schema_.apply(frame);
            for(const auto &name:schema_.names())if(chartSelect_->findText(name)<0)
                chartSelect_->addItem(name);
            fillTable();updateChart();
            statusBar()->showMessage("Loaded "+path+" (engineering semantics require independent validation)",8000);
        });
        menu->addSeparator();
        menu->addAction("Export CSV",[this]{exportFile(true);});
        menu->addAction("Export JSON",[this]{exportFile(false);});
        auto bar=addToolBar("Session");
        auto chooseLog=bar->addAction("LOG");connect(chooseLog,&QAction::triggered,this,[this]{
            logPath_=QFileDialog::getOpenFileName(this,"Select log",{},"Log files (*.log *.txt);;All files (*)");
            updatePaths();
        });
        auto chooseBin=bar->addAction("BIN");connect(chooseBin,&QAction::triggered,this,[this]{
            binPath_=QFileDialog::getOpenFileName(this,"Select BIN",{},"Binary files (*.bin);;All files (*)");
            updatePaths();
        });
        satellite_=new QComboBox;
        satellite_->addItem("ASRTU-1");
        satellite_->addItem("BY04 / 8104","BY04");
        bar->addWidget(satellite_);
        auto import=bar->addAction("Import session");
        connect(import,&QAction::triggered,this,[this]{loadSession();});
        auto saveCsv=bar->addAction("Export CSV");
        connect(saveCsv,&QAction::triggered,this,[this]{exportFile(true);});
        auto saveJson=bar->addAction("Export JSON");
        connect(saveJson,&QAction::triggered,this,[this]{exportFile(false);});
        auto tcp=bar->addAction("Connect TCP");
        connect(tcp,&QAction::triggered,this,[this]{
            bool ok=false;
            QString host=QInputDialog::getText(this,"Live TCP","Host",QLineEdit::Normal,"127.0.0.1",&ok);
            if(!ok||host.isEmpty())return;
            const int port=QInputDialog::getInt(this,"Live TCP","Port",9985,1,65535,1,&ok);
            if(ok)live_->start(host,quint16(port),selectedSatellite());
        });
        auto disconnect=bar->addAction("Stop TCP");
        connect(disconnect,&QAction::triggered,live_,&LiveTcp::stop);
        connect(live_,&LiveTcp::status,this,[this](const QString &s){statusBar()->showMessage(s,6000);});
        connect(live_,&LiveTcp::received,this,[this](const Frame &f){
            for(const auto &x:frames_)if(x.hash==f.hash)return;
            frames_.push_back(f);fillTable();timeline_->setMaximum(std::max(0,int(frames_.size())-1));
            statusBar()->showMessage("TCP frame received (RS parity not independently verified)",3000);
        });

        auto root=new QWidget;setCentralWidget(root);
        auto layout=new QVBoxLayout(root);layout->setContentsMargins(8,8,8,8);
        sources_=new QLabel("No input files selected");
        sources_->setWordWrap(true);
        layout->addWidget(sources_);
        auto split=new QSplitter(Qt::Horizontal);layout->addWidget(split,1);
        auto left=new QWidget;auto leftLayout=new QVBoxLayout(left);
        leftLayout->addWidget(new QLabel("Frame browser"));
        filter_=new QLineEdit;filter_->setPlaceholderText("Filter APID, VCID, quality, SHA...");
        leftLayout->addWidget(filter_);
        framesTable_=new QTableWidget(0,7);
        framesTable_->setHorizontalHeaderLabels(
            {"Recorder time","MFC","VCID","APID","Seq","Quality","Sources"});
        framesTable_->verticalHeader()->hide();
        framesTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
        framesTable_->setSelectionMode(QAbstractItemView::SingleSelection);
        framesTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        framesTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
        framesTable_->horizontalHeader()->setStretchLastSection(true);
        leftLayout->addWidget(framesTable_,1);split->addWidget(left);
        auto right=new QTabWidget;split->addWidget(right);split->setSizes({690,670});
        auto inspect=new QWidget;auto inspectLayout=new QVBoxLayout(inspect);
        banner_=new QLabel("Select a frame to inspect its provenance and quality.");
        banner_->setWordWrap(true);inspectLayout->addWidget(banner_);
        fieldsTable_=new QTableWidget(0,4);
        fieldsTable_->setHorizontalHeaderLabels({"Field","Value","Unit","Status"});
        fieldsTable_->horizontalHeader()->setStretchLastSection(true);
        fieldsTable_->verticalHeader()->hide();
        fieldsTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        inspectLayout->addWidget(fieldsTable_,1);
        right->addTab(inspect,"Telemetry");
        rawText_=new QPlainTextEdit;rawText_->setReadOnly(true);
        rawText_->setLineWrapMode(QPlainTextEdit::NoWrap);
        right->addTab(rawText_,"Raw HEX");
        auto chartPage=new QWidget;auto chartLayout=new QVBoxLayout(chartPage);
        chartSelect_=new QComboBox;chartSelect_->addItems({
            "candidate.quaternion.q0","candidate.quaternion.q1",
            "candidate.quaternion.q2","candidate.quaternion.q3",
            "candidate.quaternion.norm"});
        chartLayout->addWidget(chartSelect_);
        chart_=new QChartView(new QChart);
        chart_->setRenderHint(QPainter::Antialiasing);
        chartLayout->addWidget(chart_,1);right->addTab(chartPage,"Time series");
        auto attitude=new AttitudeView;
        attitude_=attitude;right->addTab(attitude,"Attitude preview");
        auto imagePane=new QLabel("No verified SSDV image in session. Raw packets are preserved.\n"
              "Image reconstruction requires independently identified SSDV packets.");
        imagePane->setWordWrap(true);imagePane->setAlignment(Qt::AlignCenter);
        right->addTab(imagePane,"Images");
        auto controls=new QHBoxLayout;
        auto play=new QPushButton("Play");controls->addWidget(play);
        auto pause=new QPushButton("Pause");controls->addWidget(pause);
        timeline_=new QSlider(Qt::Horizontal);controls->addWidget(timeline_,1);
        speed_=new QComboBox;speed_->addItems({"0.5x","1x","2x","4x","8x"});
        speed_->setCurrentIndex(1);controls->addWidget(speed_);
        timeLabel_=new QLabel("No frames");controls->addWidget(timeLabel_);
        layout->addLayout(controls);
        timer_=new QTimer(this);timer_->setInterval(500);
        connect(play,&QPushButton::clicked,timer_,qOverload<>(&QTimer::start));
        connect(pause,&QPushButton::clicked,timer_,&QTimer::stop);
        connect(timer_,&QTimer::timeout,this,[this]{
            if(frames_.isEmpty()){timer_->stop();return;}
            if(timeline_->value()==timeline_->maximum()){timer_->stop();return;}
            timeline_->setValue(timeline_->value()+1);
        });
        connect(speed_,&QComboBox::currentIndexChanged,this,[this](int i){
            static const int intervals[]={1000,500,250,125,62};
            timer_->setInterval(intervals[i]);
        });
        connect(timeline_,&QSlider::valueChanged,this,[this](int idx){showFrame(idx);});
        connect(filter_,&QLineEdit::textChanged,this,[this]{applyFilter();});
        connect(framesTable_,&QTableWidget::itemSelectionChanged,this,[this]{
            auto *item=framesTable_->item(framesTable_->currentRow(),0);
            if(item)timeline_->setValue(item->data(Qt::UserRole).toInt());
        });
        connect(chartSelect_,&QComboBox::currentTextChanged,this,[this]{updateChart();});
        statusBar()->showMessage("Ready — spacecraft UTC and engineering conventions are never inferred");
        const QString dbFile=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)+"/orbittlm.sqlite";
        QDir().mkpath(QFileInfo(dbFile).absolutePath());
        QString err;
        if(!database_.open(dbFile,&err))statusBar()->showMessage("SQLite unavailable: "+err,15000);
    }
private:
    QString selectedSatellite()const{
        const QVariant id=satellite_->currentData();
        return id.isValid()?id.toString():satellite_->currentText();
    }
    void updatePaths(){
        sources_->setText("LOG: "+(logPath_.isEmpty()?"(none)":logPath_)+
            "\nBIN: "+(binPath_.isEmpty()?"(none)":binPath_));
    }
    void loadSession(){
        if(logPath_.isEmpty()&&binPath_.isEmpty()){
            QMessageBox::information(this,"Import","Choose a LOG or BIN input first.");return;
        }
        QString err;const QString sat=selectedSatellite();
        auto logs=logPath_.isEmpty()?QVector<Frame>{}:Protocol::fromLog(logPath_,sat,&err);
        if(!err.isEmpty()){QMessageBox::warning(this,"LOG import",err);return;}
        auto bins=binPath_.isEmpty()?QVector<Frame>{}:Protocol::fromBin(binPath_,sat,&err);
        if(!err.isEmpty()){QMessageBox::warning(this,"BIN import",err);return;}
        frames_=Protocol::fuse(logs,bins,&stats_);
        if(hasSchema_ && schema_.satellite()==sat){
            for(auto &frame:frames_)schema_.apply(frame);
        }
        fillTable();
        timeline_->setRange(0,std::max(0,int(frames_.size())-1));
        if(!frames_.isEmpty())showFrame(0);
        updateChart();
        const QString msg=QString("Unique %1; log/FEC %2; structural-only %3; invalid %4; duplicates %5")
            .arg(stats_.unique).arg(stats_.trustedFromLog).arg(stats_.structuralOnly)
            .arg(stats_.invalid).arg(stats_.duplicates);
        QString dbError;
        if(!database_.save(sat+" paired import",frames_,&dbError))
            QMessageBox::warning(this,"SQLite",dbError);
        statusBar()->showMessage(msg,20000);
    }
    void fillTable(){
        framesTable_->setRowCount(frames_.size());
        for(int n=0;n<frames_.size();++n){
            const auto &f=frames_[n];
            const QStringList cols={
                f.captureLocal.isValid()?f.captureLocal.toString("yyyy-MM-dd HH:mm:ss.zzz"):"(unknown)",
                f.master<0?"":QString::number(f.master),
                f.vcid<0?"":QString::number(f.vcid),
                f.apid<0?"":QString("0x%1").arg(f.apid,3,16,QChar('0')).toUpper(),
                f.sequence<0?"":QString::number(f.sequence),
                f.quality,f.sources.join("; ")};
            for(int i=0;i<cols.size();++i){
                auto item=new QTableWidgetItem(cols[i]);
                if(i==0)item->setData(Qt::UserRole,n);
                framesTable_->setItem(n,i,item);
            }
        }
        applyFilter();
    }
    void applyFilter(){
        const auto needle=filter_->text().trimmed();
        for(int row=0;row<framesTable_->rowCount();++row){
            bool match=needle.isEmpty();
            for(int col=0;col<framesTable_->columnCount()&&!match;++col){
                auto *item=framesTable_->item(row,col);
                if(item&&item->text().contains(needle,Qt::CaseInsensitive))match=true;
            }
            framesTable_->setRowHidden(row,!match);
        }
    }
    void showFrame(int index){
        if(index<0||index>=frames_.size())return;
        const Frame &f=frames_[index];
        QSignalBlocker block(framesTable_);
        framesTable_->selectRow(index);
        banner_->setText(QString("Satellite %1 | SHA-256 %2\nQuality: %3 | Basis: %4\n%5")
            .arg(f.satellite,f.hash,f.quality,f.timeBasis,f.error));
        fieldsTable_->setRowCount(f.fields.size());
        for(int i=0;i<f.fields.size();++i){
            const auto &x=f.fields[i];
            const QStringList values={x.name,x.value,x.unit,x.status};
            for(int j=0;j<4;++j)fieldsTable_->setItem(i,j,new QTableWidgetItem(values[j]));
        }
        rawText_->setPlainText("FRAME\n"+QString::fromLatin1(f.bytes.toHex(' ').toUpper())+
                            "\n\nSPACE PACKET PAYLOAD\n"+QString::fromLatin1(f.payload.toHex(' ').toUpper()));
        timeLabel_->setText(QString("%1 / %2").arg(index+1).arg(frames_.size()));
        attitude_->display(f);
    }
    void updateChart(){
        auto chart=new QChart;
        auto series=new QLineSeries(chart);
        const QString requested=chartSelect_->currentText();
        for(int i=0;i<frames_.size();++i){
            const Frame &f=frames_[i];
            if(f.quality!="log_fec_reported")continue;
            for(const auto &x:f.fields)if(x.name==requested){
                bool ok=false;double value=x.value.toDouble(&ok);
                if(ok&&std::isfinite(value))series->append(i,value);
            }
        }
        chart->addSeries(series);chart->createDefaultAxes();
        chart->setTitle(requested+" (unverified semantics; X = frame index)");
        chart_->setChart(chart);
    }
    void exportFile(bool csv){
        if(frames_.isEmpty())return;
        const QString name=QFileDialog::getSaveFileName(this,"Export chronological telemetry",
                 csv?"orbittlm.csv":"orbittlm.json",csv?"CSV (*.csv)":"JSON (*.json)");
        if(name.isEmpty())return;
        QString err;
        const bool ok=csv?Protocol::exportCsv(name,frames_,&err):Protocol::exportJson(name,frames_,&err);
        if(!ok)QMessageBox::warning(this,"Export",err);
        else statusBar()->showMessage("Exported: "+name,7000);
    }
    QString logPath_,binPath_;
    Database database_;
    LiveTcp *live_=nullptr;
    Stats stats_;
    Schema schema_;
    bool hasSchema_=false;
    QVector<Frame> frames_;
    QLabel *sources_=nullptr,*banner_=nullptr,*timeLabel_=nullptr;
    QComboBox *satellite_=nullptr,*speed_=nullptr,*chartSelect_=nullptr;
    QLineEdit *filter_=nullptr;
    QTableWidget *framesTable_=nullptr,*fieldsTable_=nullptr;
    QPlainTextEdit *rawText_=nullptr;
    QSlider *timeline_=nullptr;
    QTimer *timer_=nullptr;
    QChartView *chart_=nullptr;
    AttitudeView *attitude_=nullptr;
};
int main(int argc,char **argv){
    QApplication app(argc,argv);
    QApplication::setApplicationName("OrbitTLM");
    QApplication::setOrganizationName("OrbitTLM");
    OrbitWindow w;
    w.show();
    return app.exec();
}
