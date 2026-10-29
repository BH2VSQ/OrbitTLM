#pragma once
#include <QPainter>
#include <QFontMetrics>
#include <QResizeEvent>
#include <QTabBar>
#include <QTabWidget>
#include <QVector>
#include <QPixmap>
#include <QQuaternion>
#include <QVector3D>
#include <QWidget>
#include <algorithm>
#include <cmath>
class AutoWrapTabBar final : public QTabBar {
public:
    using QTabBar::QTabBar;

    void refreshWrapping(){applyWrapping();}
    void setOriginalText(int index,const QString &text){
        if(index<0||index>=count())return;
        while(originalTexts_.size()<count())originalTexts_.push_back(QTabBar::tabText(originalTexts_.size()));
        originalTexts_[index]=text;
        updating_=true;
        QTabBar::setTabText(index,text);
        updating_=false;
        applyWrapping();
    }

protected:
    void tabInserted(int index) override {
        QTabBar::tabInserted(index);
        originalTexts_.insert(qBound(0,index,originalTexts_.size()),QTabBar::tabText(index));
        applyWrapping();
    }
    void tabRemoved(int index) override {
        QTabBar::tabRemoved(index);
        if(index>=0&&index<originalTexts_.size())originalTexts_.removeAt(index);
        applyWrapping();
    }
    void resizeEvent(QResizeEvent *event) override {
        QTabBar::resizeEvent(event);
        applyWrapping();
    }

private:
    QVector<QString> originalTexts_;
    bool updating_=false;

    static QString wrapText(const QString &text){
        // Keep each tab caption on one line. The subsystem tab row and the nested
        // official-packet tab row provide navigation levels without breaking labels.
        return text;
    }
    void applyWrapping(){
        if(updating_||count()==0||width()<80)return;
        while(originalTexts_.size()<count())originalTexts_.push_back(QTabBar::tabText(originalTexts_.size()));
        while(originalTexts_.size()>count())originalTexts_.removeLast();
        const QFontMetrics metrics(font());
        auto tabWidth=[&](const QString &text){
            int widest=0;
            for(const QString &line:text.split(QLatin1Char('\n')))
                widest=qMax(widest,metrics.horizontalAdvance(line));
            return widest+22;
        };
        QVector<QString> display=originalTexts_;
        int total=0;
        for(const auto &text:display)total+=tabWidth(text);
        if(total>width()){
            QVector<int> candidates;
            for(int i=0;i<display.size();++i)
                if(display[i].size()>=5)candidates.push_back(i);
            std::sort(candidates.begin(),candidates.end(),[&](int a,int b){
                return metrics.horizontalAdvance(display[a])>metrics.horizontalAdvance(display[b]);
            });
            for(int i:candidates){
                if(total<=width())break;
                display[i]=wrapText(originalTexts_[i]);
                total=0;
                for(const auto &text:display)total+=tabWidth(text);
            }
        }
        updating_=true;
        for(int i=0;i<count();++i)
            if(QTabBar::tabText(i)!=display[i])QTabBar::setTabText(i,display[i]);
        updating_=false;
    }
};

class AutoWrapTabWidget final : public QTabWidget {
public:
    explicit AutoWrapTabWidget(QWidget *parent=nullptr):QTabWidget(parent){
        wrappingBar_=new AutoWrapTabBar(this);
        wrappingBar_->setElideMode(Qt::ElideNone);
        wrappingBar_->setUsesScrollButtons(true);
        wrappingBar_->setExpanding(false);
        setTabBar(wrappingBar_);
    }
    void refreshTabWrapping(){wrappingBar_->refreshWrapping();}
    void setWrappedTabText(int index,const QString &text){wrappingBar_->setOriginalText(index,text);}
private:
    AutoWrapTabBar *wrappingBar_=nullptr;
};

class AttitudeWidget final : public QWidget {
public:
    explicit AttitudeWidget(QWidget *parent=nullptr):QWidget(parent){setMinimumSize(220,210);}
    void setQuaternion(const QQuaternion &q,bool available){
        active_=available;
        rotation_=q.normalized();
        update();
    }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(),QColor("#f7faff"));
        if(!active_){
            p.setPen(QColor("#758397"));
            p.drawText(rect(),Qt::AlignCenter,
                QStringLiteral("暂无可确认来源的姿态四元数\n等待 SAT-S3 业务遥测"));
            return;
        }
        constexpr QVector3D corners[8]={{-1,-.65f,-.35f},{1,-.65f,-.35f},
            {1,.65f,-.35f},{-1,.65f,-.35f},{-1,-.65f,.35f},
            {1,-.65f,.35f},{1,.65f,.35f},{-1,.65f,.35f}};
        constexpr int edges[12][2]={{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},
            {6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
        QPointF projected[8];
        const qreal scale=qreal(std::min(width(),height()))*0.26;
        const QPointF center(width()/2.,height()/2.);
        for(int i=0;i<8;++i){
            const QVector3D v=rotation_.rotatedVector(corners[i]);
            const qreal perspective=4.0/(4.0+v.z());
            projected[i]=center+QPointF(v.x()*scale*perspective,-v.y()*scale*perspective);
        }
        p.setPen(QPen(QColor("#377fc0"),2.3));
        for(const auto &edge:edges)p.drawLine(projected[edge[0]],projected[edge[1]]);
        p.setPen(QColor("#718096"));
        p.drawText(rect().adjusted(10,0,-10,-8),Qt::AlignBottom|Qt::AlignHCenter,
            QStringLiteral("仅作相对姿态示意 · 坐标系未核实"));
    }
private:
    QQuaternion rotation_;
    bool active_=false;
};
class ImagePreview final : public QWidget {
public:
    explicit ImagePreview(QWidget *parent=nullptr):QWidget(parent){setMinimumSize(260,190);}
    void setImage(const QPixmap &image){image_=image;update();}
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);p.setRenderHint(QPainter::SmoothPixmapTransform);
        p.fillRect(rect(),QColor("#f7faff"));
        if(image_.isNull()){
            p.setPen(QColor("#758397"));
            p.drawText(rect().adjusted(20,20,-20,-20),Qt::AlignCenter|Qt::TextWordWrap,
                QStringLiteral("暂无恢复图像\n\n扫描 SSDV 分片后选择图像 ID，\n即可在软件内重建 JPEG"));
            return;
        }
        const QSize scaled=image_.size().scaled(size()-QSize(20,20),Qt::KeepAspectRatio);
        const QRect dst(QPoint((width()-scaled.width())/2,(height()-scaled.height())/2),scaled);
        p.drawPixmap(dst,image_,image_.rect());
    }
private:
    QPixmap image_;
};
