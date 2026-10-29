#pragma once
#include <QScrollBar>
#include <QScrollArea>
#include <QFrame>
#include <QHBoxLayout>
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
// ORBITTLM_ROUND4_PACKET_ARROWS
class FlowLayout final : public QLayout {
public:
    explicit FlowLayout(QWidget *parent=nullptr,int spacing=4):QLayout(parent){
        setContentsMargins(0,0,0,0);setSpacing(spacing);
    }
    ~FlowLayout() override { while(QLayoutItem *item=takeAt(0))delete item; }
    void addItem(QLayoutItem *item) override { items_.push_back(item); }
    int count() const override { return int(items_.size()); }
    QLayoutItem *itemAt(int index) const override { return index>=0&&index<count()?items_.at(index):nullptr; }
    QLayoutItem *takeAt(int index) override { if(index<0||index>=count())return nullptr; return items_.takeAt(index); }
    Qt::Orientations expandingDirections() const override { return {}; }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override { return doLayout(QRect(0,0,width,0),true); }
    QSize sizeHint() const override { return minimumSize(); }
    QSize minimumSize() const override {
        QSize size;
        for(QLayoutItem *item:items_)if(!item->isEmpty())size=size.expandedTo(item->minimumSize());
        const QMargins margins=contentsMargins();
        return size+QSize(margins.left()+margins.right(),margins.top()+margins.bottom());
    }
    void setGeometry(const QRect &rect) override { QLayout::setGeometry(rect); doLayout(rect,false); }
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
                x=effective.x();y+=lineHeight+gap;nextX=x+size.width()+gap;lineHeight=0;
            }
            if(!testOnly)item->setGeometry(QRect(QPoint(x,y),size));
            x=nextX;lineHeight=qMax(lineHeight,size.height());
        }
        return y+lineHeight-rect.y()+margins.bottom();
    }
    QVector<QLayoutItem*> items_;
};

class AutoWrapTabWidget final : public QWidget {
    Q_OBJECT
public:
    explicit AutoWrapTabWidget(QWidget *parent=nullptr):QWidget(parent){
        auto *root=new QVBoxLayout(this);root->setContentsMargins(0,0,0,0);root->setSpacing(3);
        // Keep the wrapping layout directly in the root layout so Qt can honor
        // FlowLayout::heightForWidth() and allocate every wrapped row visibly.
        wrapArea_=new QWidget(this);
        tabLayout_=new FlowLayout(wrapArea_,4);
        tabLayout_->setAlignment(Qt::AlignLeft|Qt::AlignTop);
        root->addWidget(wrapArea_,0);

        scrollRow_=new QWidget(this);
        auto *scrollRowLayout=new QHBoxLayout(scrollRow_);
        scrollRowLayout->setContentsMargins(0,0,0,0);scrollRowLayout->setSpacing(4);
        previous_=new QPushButton(QStringLiteral("◀"),scrollRow_);
        previous_->setObjectName(QStringLiteral("packetTabPrevious"));
        previous_->setFixedSize(30,30);previous_->setToolTip(QStringLiteral("向左滑动子包标签"));
        next_=new QPushButton(QStringLiteral("▶"),scrollRow_);
        next_->setObjectName(QStringLiteral("packetTabNext"));
        next_->setFixedSize(30,30);next_->setToolTip(QStringLiteral("向右滑动子包标签"));
        scrollArea_=new QScrollArea(scrollRow_);
        scrollArea_->setObjectName(QStringLiteral("packetTabScrollArea"));
        scrollArea_->setWidgetResizable(false);
        scrollArea_->setFrameShape(QFrame::NoFrame);
        scrollArea_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scrollArea_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scrollContents_=new QWidget;
        scrollContents_->setObjectName(QStringLiteral("packetTabScrollContents"));
        scrollContents_->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
        scrollTabsLayout_=new QHBoxLayout(scrollContents_);
        scrollTabsLayout_->setContentsMargins(0,0,0,0);scrollTabsLayout_->setSpacing(4);
        scrollTabsLayout_->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
        scrollArea_->setWidget(scrollContents_);
        scrollRowLayout->addWidget(previous_);scrollRowLayout->addWidget(scrollArea_,1);scrollRowLayout->addWidget(next_);
        scrollRow_->setMinimumHeight(32);
        root->addWidget(scrollRow_,0);
        scrollRow_->hide();

        stack_=new QStackedWidget(this);
        root->addWidget(stack_,1);
        QObject::connect(previous_,&QPushButton::clicked,this,[this]{
            if(scrollArea_)scrollArea_->horizontalScrollBar()->setValue(
                scrollArea_->horizontalScrollBar()->value()-qMax(120,scrollArea_->viewport()->width()*3/4));
        });
        QObject::connect(next_,&QPushButton::clicked,this,[this]{
            if(scrollArea_)scrollArea_->horizontalScrollBar()->setValue(
                scrollArea_->horizontalScrollBar()->value()+qMax(120,scrollArea_->viewport()->width()*3/4));
        });
        scrollRow_->setStyleSheet(QStringLiteral("QPushButton {background:#e2e8f0;color:#334155;border:1px solid #cbd5e1;border-radius:5px;padding:2px;} QPushButton:hover {background:#cbd5e1;}"));
        setTabPresentation(false,QStringLiteral("system"));
    }
    int addTab(QWidget *page,const QString &text){
        if(!page)return -1;
        const int index=int(buttons_.size());pages_.push_back(page);labels_.push_back(text);visible_.push_back(true);
        stack_->addWidget(page);
        auto *button=new QToolButton;
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);button->setText(text);button->setToolTip(text);
        button->setCheckable(true);button->setAutoRaise(false);button->setMinimumHeight(30);
        button->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
        applyTabButtonStyle(button);button->adjustSize();button->setFixedSize(button->sizeHint());
        buttons_.push_back(button);
        if(horizontalScrollable_)scrollTabsLayout_->addWidget(button);else tabLayout_->addWidget(button);
        QObject::connect(button,&QToolButton::clicked,this,[this,index]{setCurrentIndex(index);});
        if(currentIndex_<0)setCurrentIndex(index);
        refreshTabWrapping();return index;
    }
    void setDocumentMode(bool) {}
    void setTabPresentation(bool horizontalScrollable,const QString &level){
        horizontalScrollable_=horizontalScrollable;tabLevel_=level;
        for(QToolButton *button:buttons_){
            const int flowIndex=tabLayout_->indexOf(button);
            if(flowIndex>=0){QLayoutItem *item=tabLayout_->takeAt(flowIndex);delete item;}
            scrollTabsLayout_->removeWidget(button);
            if(horizontalScrollable_)scrollTabsLayout_->addWidget(button);else tabLayout_->addWidget(button);
            applyTabButtonStyle(button);button->adjustSize();button->setFixedSize(button->sizeHint());
        }
        wrapArea_->setVisible(!horizontalScrollable_);
        scrollRow_->setVisible(horizontalScrollable_);
        refreshTabWrapping();
    }
    int count() const { return int(pages_.size()); }
    QWidget *widget(int index) const { return index>=0&&index<count()?pages_.at(index):nullptr; }
    int currentIndex() const { return currentIndex_; }
    void setCurrentIndex(int index){
        if(index<0||index>=count()||!visible_.at(index))return;
        currentIndex_=index;stack_->setCurrentIndex(index);
        for(int i=0;i<buttons_.size();++i)buttons_[i]->setChecked(i==index);
    }
    void setTabVisible(int index,bool visible){
        if(index<0||index>=count())return;
        visible_[index]=visible;buttons_[index]->setVisible(visible);
        if(!visible&&currentIndex_==index){currentIndex_=-1;for(int i=0;i<count();++i)if(visible_[i]){setCurrentIndex(i);break;}}
        refreshTabWrapping();
    }
    void setTabText(int index,const QString &text){
        if(index<0||index>=count())return;
        labels_[index]=text;buttons_[index]->setText(text);applyTabButtonStyle(buttons_[index]);
        buttons_[index]->adjustSize();buttons_[index]->setFixedSize(buttons_[index]->sizeHint());refreshTabWrapping();
    }
    QString tabText(int index) const { return index>=0&&index<count()?labels_.at(index):QString(); }
    void setTabToolTip(int index,const QString &tip){if(index>=0&&index<count())buttons_[index]->setToolTip(tip);}
    void setWrappedTabText(int index,const QString &text){setTabText(index,text);}
    void refreshTabWrapping(){
        if(tabLayout_){tabLayout_->invalidate();tabLayout_->activate();}
        if(scrollContents_){scrollContents_->adjustSize();scrollContents_->updateGeometry();}
        if(scrollArea_&&scrollArea_->widget())scrollArea_->widget()->adjustSize();
        updateGeometry();
    }
private:
    void applyTabButtonStyle(QToolButton *button){
        if(!button)return;
        if(tabLevel_==QStringLiteral("packet")){
            button->setStyleSheet(QStringLiteral(
                "QToolButton {background:#f1f5f9;color:#334155;border:1px solid #cbd5e1;border-radius:5px;padding:5px 9px;font-weight:500;}"
                "QToolButton:checked {background:#0f766e;color:white;border-color:#115e59;font-weight:700;}"
                "QToolButton:hover {background:#ccfbf1;border-color:#5eead4;}"));
        }else{
            button->setStyleSheet(QStringLiteral(
                "QToolButton {background:#dbeafe;color:#1e3a8a;border:1px solid #93c5fd;border-radius:6px;padding:5px 10px;font-weight:600;}"
                "QToolButton:checked {background:#2563eb;color:white;border-color:#1d4ed8;font-weight:700;}"
                "QToolButton:hover {background:#bfdbfe;border-color:#60a5fa;}"));
        }
    }
    FlowLayout *tabLayout_=nullptr;
    QStackedWidget *stack_=nullptr;
    QWidget *wrapArea_=nullptr,*scrollRow_=nullptr,*scrollContents_=nullptr;
    QScrollArea *scrollArea_=nullptr;
    QHBoxLayout *scrollTabsLayout_=nullptr;
    QPushButton *previous_=nullptr,*next_=nullptr;
    QVector<QWidget*> pages_;
    QVector<QToolButton*> buttons_;
    QVector<QString> labels_;
    QVector<bool> visible_;
    QString tabLevel_=QStringLiteral("system");
    bool horizontalScrollable_=false;
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
