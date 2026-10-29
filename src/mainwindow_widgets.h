#pragma once
#include <QPainter>
#include <QVBoxLayout>
#include <QToolButton>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QPushButton>
#include <QLayout>
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
class FlowLayout final : public QLayout {
public:
    explicit FlowLayout(QWidget *parent=nullptr,int spacing=4):QLayout(parent){
        setContentsMargins(0,0,0,0);
        setSpacing(spacing);
    }
    ~FlowLayout() override { while(QLayoutItem *item=takeAt(0))delete item; }
    void addItem(QLayoutItem *item) override { items_.push_back(item); }
    int count() const override { return int(items_.size()); }
    QLayoutItem *itemAt(int index) const override {
        return index>=0&&index<count()?items_.at(index):nullptr;
    }
    QLayoutItem *takeAt(int index) override {
        if(index<0||index>=count())return nullptr;
        return items_.takeAt(index);
    }
    Qt::Orientations expandingDirections() const override { return {}; }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override { return doLayout(QRect(0,0,width,0),true); }
    QSize sizeHint() const override { return minimumSize(); }
    QSize minimumSize() const override {
        QSize size;
        for(QLayoutItem *item:items_)if(!item->isEmpty())size=size.expandedTo(item->minimumSize());
        const QMargins margins=contentsMargins();
        size+=QSize(margins.left()+margins.right(),margins.top()+margins.bottom());
        return size;
    }
    void setGeometry(const QRect &rect) override {
        QLayout::setGeometry(rect);
        doLayout(rect,false);
    }
private:
    int doLayout(const QRect &rect,bool testOnly) const {
        const QMargins margins=contentsMargins();
        const QRect effective=rect.adjusted(margins.left(),margins.top(),-margins.right(),-margins.bottom());
        int x=effective.x(),y=effective.y(),lineHeight=0;
        const int gap=qMax(0,spacing());
        for(QLayoutItem *item:items_){
            if(item->isEmpty())continue;
            const QSize size=item->sizeHint();
            int nextX=x+size.width()+gap;
            if(nextX-gap>effective.right()+1&&lineHeight>0){
                x=effective.x();
                y+=lineHeight+gap;
                nextX=x+size.width()+gap;
                lineHeight=0;
            }
            if(!testOnly)item->setGeometry(QRect(QPoint(x,y),size));
            x=nextX;
            lineHeight=qMax(lineHeight,size.height());
        }
        return y+lineHeight-rect.y()+margins.bottom();
    }
    QVector<QLayoutItem*> items_;
};

class AutoWrapTabWidget final : public QWidget {
    Q_OBJECT
public:
    explicit AutoWrapTabWidget(QWidget *parent=nullptr):QWidget(parent){
        auto *root=new QVBoxLayout(this);
        root->setContentsMargins(0,0,0,0);
        root->setSpacing(3);
        auto *tabArea=new QWidget(this);
        tabArea->setObjectName(QStringLiteral("flowTabButtonArea"));
        tabArea->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
        tabLayout_=new FlowLayout(tabArea,4);
        tabLayout_->setAlignment(Qt::AlignLeft|Qt::AlignTop);
        stack_=new QStackedWidget(this);
        root->addWidget(tabArea,0);
        root->addWidget(stack_,1);
    }
    int addTab(QWidget *page,const QString &text){
        if(!page)return -1;
        const int index=int(buttons_.size());
        pages_.push_back(page);
        labels_.push_back(text);
        visible_.push_back(true);
        stack_->addWidget(page);
        auto *button=new QToolButton;
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        button->setText(text);
        button->setToolTip(text);
        button->setCheckable(true);
        button->setAutoRaise(false);
        button->setMinimumHeight(28);
        button->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);
        buttons_.push_back(button);
        tabLayout_->addWidget(button);
        QObject::connect(button,&QToolButton::clicked,this,[this,index]{setCurrentIndex(index);});
        if(currentIndex_<0)setCurrentIndex(index);
        refreshTabWrapping();
        return index;
    }
    void setDocumentMode(bool) {}
    int count() const { return int(pages_.size()); }
    QWidget *widget(int index) const {
        return index>=0&&index<count()?pages_.at(index):nullptr;
    }
    int currentIndex() const { return currentIndex_; }
    void setCurrentIndex(int index){
        if(index<0||index>=count()||!visible_.at(index))return;
        currentIndex_=index;
        stack_->setCurrentIndex(index);
        for(int i=0;i<buttons_.size();++i)buttons_[i]->setChecked(i==index);
    }
    void setTabVisible(int index,bool visible){
        if(index<0||index>=count())return;
        visible_[index]=visible;
        buttons_[index]->setVisible(visible);
        if(!visible&&currentIndex_==index){
            currentIndex_=-1;
            for(int i=0;i<count();++i)if(visible_[i]){setCurrentIndex(i);break;}
        }
        refreshTabWrapping();
    }
    void setTabText(int index,const QString &text){
        if(index<0||index>=count())return;
        labels_[index]=text;
        buttons_[index]->setText(text);
        refreshTabWrapping();
    }
    QString tabText(int index) const {
        return index>=0&&index<count()?labels_.at(index):QString();
    }
    void setTabToolTip(int index,const QString &tip){
        if(index>=0&&index<count())buttons_[index]->setToolTip(tip);
    }
    void setWrappedTabText(int index,const QString &text){setTabText(index,text);}
    void refreshTabWrapping(){
        if(tabLayout_){tabLayout_->invalidate();tabLayout_->activate();}
        updateGeometry();
    }
private:
    FlowLayout *tabLayout_=nullptr;
    QStackedWidget *stack_=nullptr;
    QVector<QWidget*> pages_;
    QVector<QToolButton*> buttons_;
    QVector<QString> labels_;
    QVector<bool> visible_;
    int currentIndex_=-1;
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
