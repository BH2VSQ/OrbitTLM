#pragma once
#include <QScrollBar>
#include <QCoreApplication>
#include <QLabel>
#include <QJsonObject>
#include <QScrollArea>
#include <QFrame>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPainter>
#include <QVBoxLayout>
#include <QToolButton>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QPushButton>
#include <QLayout>
#include <QFontMetrics>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QDateTime>
#include <QStringList>
#include <functional>
#include <QTabBar>
#include <QTabWidget>
#include <QVector>
#include <QPixmap>
#include <QQuaternion>
#include <QVector3D>
#include <QWidget>
#include <QHash>
#include <algorithm>
#include <cmath>
#include <limits>
inline bool orbitContainsCjk(const QString &text){
    for(const QChar ch:text){
        const ushort u=ch.unicode();
        if((u>=0x3400&&u<=0x9fff)||(u>=0xf900&&u<=0xfaff))return true;
    }
    return false;
}
// Runtime translator used by custom-painted widgets and table builders. The
// former hot path scanned every translation phrase for every telemetry value,
// including numeric/ASCII-only values. Cache repeated strings and skip phrase
// matching when the input contains no CJK characters.
inline QString orbitWidgetText(const QString &source){
    auto *app=QCoreApplication::instance();
    if(!app||app->property("orbitTlmLanguage").toString()!=QStringLiteral("en-US"))return source;
    static QString cacheLanguage;
    static QHash<QString,QString> cache;
    const QString language=app->property("orbitTlmLanguage").toString();
    if(cacheLanguage!=language){cache.clear();cacheLanguage=language;}
    const auto cached=cache.constFind(source);
    if(cached!=cache.constEnd())return cached.value();
    const QJsonObject map=app->property("orbitTlmTranslations").value<QJsonObject>();
    const auto exact=map.constFind(source);
    if(exact!=map.constEnd()){
        const QString result=exact.value().toString(source);
        if(source.size()<=256&&cache.size()<20000)cache.insert(source,result);
        return result;
    }
    if(!orbitContainsCjk(source))return source;
    QString translated=source;
    const QStringList keys=app->property("orbitTlmTranslationKeys").toStringList();
    for(const QString &key:keys){
        if(key.isEmpty()||!translated.contains(key))continue;
        const QString value=map.value(key).toString();
        if(key==value)continue;
        translated.replace(key,value);
        if(!orbitContainsCjk(translated))break;
    }
    if(source.size()<=256&&cache.size()<20000)cache.insert(source,translated);
    return translated;
}
inline QString orbitEnglishFieldLabel(const QString &source,const QString &catalogEnglish,const QString &code){
    if(!catalogEnglish.isEmpty()&&!orbitContainsCjk(catalogEnglish))return catalogEnglish;
    const QString translated=orbitWidgetText(source);
    if(!orbitContainsCjk(translated))return translated;
    // Some mission catalogs contain no English metadata for a field. Keep the
    // English UI free of untranslated CJK; the stable official field code lets
    // operators locate and cross-reference the exact catalog entry.
    return code.isEmpty()?QStringLiteral("Telemetry field"):QStringLiteral("Field %1").arg(code);
}
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
        previous_->setFixedSize(26,48);previous_->setToolTip(orbitWidgetText(QStringLiteral("向左翻页子包标签")));
        next_=new QPushButton(QStringLiteral("▶"),scrollRow_);
        next_->setObjectName(QStringLiteral("packetTabNext"));
        next_->setFixedSize(26,48);next_->setToolTip(orbitWidgetText(QStringLiteral("向右翻页子包标签")));
        scrollArea_=new QScrollArea(scrollRow_);
        scrollArea_->setObjectName(QStringLiteral("packetTabScrollArea"));
        scrollArea_->setWidgetResizable(false);
        scrollArea_->setFixedHeight(52);
        scrollArea_->setFrameShape(QFrame::NoFrame);
        scrollArea_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scrollArea_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scrollContents_=new QWidget;
        scrollContents_->setObjectName(QStringLiteral("packetTabScrollContents"));
        scrollContents_->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
        scrollTabsLayout_=new QGridLayout(scrollContents_);
        scrollTabsLayout_->setContentsMargins(0,0,0,0);
        scrollTabsLayout_->setHorizontalSpacing(4);
        scrollTabsLayout_->setVerticalSpacing(3);
        scrollTabsLayout_->setAlignment(Qt::AlignLeft|Qt::AlignTop);
        scrollArea_->setWidget(scrollContents_);
        scrollRowLayout->addWidget(previous_);scrollRowLayout->addWidget(scrollArea_,1);scrollRowLayout->addWidget(next_);
        scrollRow_->setMinimumHeight(52);
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
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);button->setText(text);
        QString tooltipSource=page->property("orbitSourceTabTooltip").toString();
        if(tooltipSource.isEmpty())tooltipSource=page->property("orbitSourceTabText").toString();
        if(tooltipSource.isEmpty())tooltipSource=text;
        button->setProperty("orbitSourceToolTip",tooltipSource);
        button->setToolTip(orbitWidgetText(tooltipSource));
        button->setCheckable(true);button->setAutoRaise(false);button->setMinimumHeight(24);
        button->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
        applyTabButtonStyle(button);button->adjustSize();button->setFixedSize(button->sizeHint());
        buttons_.push_back(button);
        if(horizontalScrollable_)scrollTabsLayout_->addWidget(button,index%2,index/2);else tabLayout_->addWidget(button);
        QObject::connect(button,&QToolButton::clicked,this,[this,index]{setCurrentIndex(index);});
        if(currentIndex_<0)setCurrentIndex(index);
        refreshTabWrapping();return index;
    }
    void setDocumentMode(bool) {}
    void setTabPresentation(bool horizontalScrollable,const QString &level){
        horizontalScrollable_=horizontalScrollable;tabLevel_=level;
        for(int i=0;i<buttons_.size();++i){
            QToolButton *button=buttons_[i];
            const int flowIndex=tabLayout_->indexOf(button);
            if(flowIndex>=0){QLayoutItem *item=tabLayout_->takeAt(flowIndex);delete item;}
            scrollTabsLayout_->removeWidget(button);
            if(horizontalScrollable_)scrollTabsLayout_->addWidget(button,i%2,i/2);else tabLayout_->addWidget(button);
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
    void setTabTexts(const QStringList &texts){
        const int n=qMin(count(),texts.size());
        bool changed=false;
        for(int i=0;i<n;++i){
            if(labels_[i]==texts.at(i)&&buttons_[i]->text()==texts.at(i))continue;
            labels_[i]=texts.at(i);
            buttons_[i]->setText(texts.at(i));
            applyTabButtonStyle(buttons_[i]);
            buttons_[i]->adjustSize();
            buttons_[i]->setFixedSize(buttons_[i]->sizeHint());
            changed=true;
        }
        // Reflow all tabs exactly once, and only if at least one label changed.
        if(changed)refreshTabWrapping();
    }
    QString tabText(int index) const { return index>=0&&index<count()?labels_.at(index):QString(); }
    void setTabToolTip(int index,const QString &tip){
        if(index<0||index>=count())return;
        buttons_[index]->setProperty("orbitSourceToolTip",tip);
        buttons_[index]->setToolTip(orbitWidgetText(tip));
    }
    void setWrappedTabText(int index,const QString &text){setTabText(index,text);}
    void refreshTabWrapping(){
        if(tabLayout_){tabLayout_->invalidate();tabLayout_->activate();}
        if(horizontalScrollable_&&scrollTabsLayout_){
            for(QToolButton *button:buttons_)scrollTabsLayout_->removeWidget(button);
            int visibleIndex=0;
            for(int i=0;i<buttons_.size();++i){
                QToolButton *button=buttons_[i];
                if(!visible_.value(i)){button->hide();continue;}
                button->show();
                scrollTabsLayout_->addWidget(button,visibleIndex%2,visibleIndex/2);
                ++visibleIndex;
            }
            scrollTabsLayout_->invalidate();scrollTabsLayout_->activate();
        }
        if(scrollContents_){scrollContents_->adjustSize();scrollContents_->updateGeometry();}
        if(scrollArea_&&scrollArea_->widget())scrollArea_->widget()->adjustSize();
        updateGeometry();
    }
private:
    void applyTabButtonStyle(QToolButton *button){
        if(!button)return;
        if(tabLevel_==QStringLiteral("packet")){
            button->setStyleSheet(QStringLiteral(
                "QToolButton {background:#f1f5f9;color:#334155;border:1px solid #cbd5e1;border-radius:5px;padding:2px 6px;font-weight:500;}"
                "QToolButton:checked {background:#0f766e;color:white;border-color:#115e59;font-weight:700;}"
                "QToolButton:hover {background:#ccfbf1;border-color:#5eead4;}"));
        }else{
            button->setStyleSheet(QStringLiteral(
                "QToolButton {background:#dbeafe;color:#1e3a8a;border:1px solid #93c5fd;border-radius:6px;padding:2px 7px;font-weight:600;}"
                "QToolButton:checked {background:#2563eb;color:white;border-color:#1d4ed8;font-weight:700;}"
                "QToolButton:hover {background:#bfdbfe;border-color:#60a5fa;}"));
        }
    }
    FlowLayout *tabLayout_=nullptr;
    QStackedWidget *stack_=nullptr;
    QWidget *wrapArea_=nullptr,*scrollRow_=nullptr,*scrollContents_=nullptr;
    QScrollArea *scrollArea_=nullptr;
    QGridLayout *scrollTabsLayout_=nullptr;
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
    explicit AttitudeWidget(QWidget *parent=nullptr):QWidget(parent){setMinimumSize(180,150);}
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
                orbitWidgetText(QStringLiteral("暂无可确认来源的姿态四元数\n等待 SAT-S3 业务遥测")));
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
            orbitWidgetText(QStringLiteral("仅作相对姿态示意 · 坐标系未核实")));
    }
private:
    QQuaternion rotation_;
    bool active_=false;
};

struct ReplayTimelineItem {
    int index = -1;
    QString timeLabel;
    QString title;
    bool recognized = false;
    bool visible = true;
};

// A compact keyframe-style replay timeline. It draws event markers directly,
// supports click-to-seek, drag-to-pan, mouse-wheel zoom, and explicit zoom buttons.
class ReplayTimelineWidget final : public QWidget {
public:
    explicit ReplayTimelineWidget(QWidget *parent=nullptr):QWidget(parent){
        setMinimumHeight(80);
        setMaximumHeight(112);
        setMouseTracking(true);
        setFocusPolicy(Qt::StrongFocus);
        setToolTip(orbitWidgetText(QStringLiteral("单击定位；拖动左右浏览；滚轮缩放时间轴")));
    }
    void setItems(const QVector<ReplayTimelineItem> &items){
        items_=items;
        if(items_.isEmpty()){currentIndex_=-1;windowStart_=0;}
        else {
            if(currentIndex_>=int(items_.size()))currentIndex_=int(items_.size())-1;
            clampWindow();
        }
        update();
    }
    void setSelectedIndex(int index){
        currentIndex_=index;
        if(index>=0&&index<int(items_.size())){
            const int count=visibleCount();
            if(index<windowStart_)windowStart_=index;
            else if(index>=windowStart_+count)windowStart_=index-count+1;
            clampWindow();
        }
        update();
    }
    void setItemVisible(int index,bool visible){
        if(index<0||index>=int(items_.size()))return;
        items_[index].visible=visible;
        update();
    }
    void zoomIn(){setZoomLevel(zoomLevel_+1);}
    void zoomOut(){setZoomLevel(zoomLevel_-1);}
    void setZoomLevel(int level){
        const int oldCount=visibleCount();
        zoomLevel_=qBound(0,level,8);
        const int newCount=visibleCount();
        if(currentIndex_>=0){
            const int center=currentIndex_;
            windowStart_=center-newCount/2;
        }else if(oldCount>0){
            const double center=windowStart_+oldCount/2.0;
            windowStart_=int(center-newCount/2.0);
        }
        clampWindow();update();
    }
    int zoomLevel()const{return zoomLevel_;}
    std::function<void(int)> onActivated;
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing,true);
        painter.fillRect(rect(),QColor("#fbfdff"));
        painter.setPen(QPen(QColor("#e2e8f0"),1));
        painter.drawRect(rect().adjusted(0,0,-1,-1));
        if(items_.isEmpty()){
            painter.setPen(QColor("#718096"));
            painter.drawText(rect(),Qt::AlignCenter,orbitWidgetText(QStringLiteral("导入或接收数据后，将在此显示回放关键帧")));
            return;
        }
        const int count=visibleCount();
        const int start=qBound(0,windowStart_,qMax(0,int(items_.size())-count));
        const int end=qMin(int(items_.size()),start+count);
        const qreal left=18.0,right=qMax(left+1.0,width()-18.0);
        const qreal axisY=height()*0.57;
        painter.setPen(QPen(QColor("#b8c7d8"),2));
        painter.drawLine(QPointF(left,axisY),QPointF(right,axisY));
        painter.setFont(QFont(font().family(),8));
        painter.setPen(QColor("#718096"));
        const QString rangeText=orbitWidgetText(QStringLiteral("%1–%2 / %3 条"))
            .arg(start+1).arg(end).arg(items_.size());
        painter.drawText(QRectF(left,4,right-left,15),Qt::AlignLeft|Qt::AlignVCenter,rangeText);
        const int displayed=end-start;
        const qreal denominator=qMax(1,displayed-1);
        const qreal pxPerItem=(right-left)/denominator;
        const int labelStride=qMax(1,int(std::ceil(54.0/qMax(1.0,pxPerItem))));
        for(int index=start;index<end;++index){
            const auto &it=items_[index];
            if(!it.visible)continue;
            const qreal x=left+(index-start)*pxPerItem;
            const bool selected=index==currentIndex_;
            const QColor marker=selected?QColor("#2563eb"):(it.recognized?QColor("#0d9488"):QColor("#94a3b8"));
            painter.setPen(QPen(marker,selected?2.4:1.5));
            painter.drawLine(QPointF(x,axisY-8),QPointF(x,axisY+9));
            painter.setBrush(marker);
            painter.drawEllipse(QPointF(x,axisY),selected?5.1:3.0,selected?5.1:3.0);
            painter.setBrush(Qt::NoBrush);
            if(selected){
                painter.setPen(QPen(QColor("#2563eb"),1));
                painter.drawRoundedRect(QRectF(x-7,axisY-7,14,14),3,3);
                painter.setPen(QColor("#1d4f91"));
                painter.setFont(QFont(font().family(),8,QFont::DemiBold));
                const QString label=it.title.left(30);
                const qreal labelWidth=qMin(170.0,right-left);
                const qreal labelX=qBound(left,x-labelWidth/2.0,right-labelWidth);
                painter.drawText(QRectF(labelX,axisY-39,labelWidth,25),Qt::AlignHCenter|Qt::AlignBottom,label);
                painter.setFont(QFont(font().family(),8));
            }else if((index-start)%labelStride==0){
                painter.setPen(QColor("#657991"));
                const QString label=it.timeLabel.isEmpty()?QString::number(index+1):it.timeLabel;
                const qreal labelWidth=qMax(40.0,qMin(78.0,pxPerItem*labelStride));
                const qreal labelX=qBound(left,x-labelWidth/2.0,right-labelWidth);
                const int row=((index-start)/labelStride)%2;
                painter.drawText(QRectF(labelX,axisY+(row?10:9),labelWidth,16),
                                 Qt::AlignHCenter|Qt::AlignTop,label);
            }
        }
        const auto &first=items_[start];
        const auto &last=items_[end-1];
        painter.setPen(QColor("#94a3b8"));
        painter.drawText(QRectF(left,height()-17,(right-left)/2,13),Qt::AlignLeft|Qt::AlignVCenter,
                         first.timeLabel);
        painter.drawText(QRectF(left+(right-left)/2,height()-17,(right-left)/2,13),
                         Qt::AlignRight|Qt::AlignVCenter,last.timeLabel);
    }
    void mousePressEvent(QMouseEvent *event) override {
        if(event->button()==Qt::LeftButton){dragging_=true;dragMoved_=false;dragStart_=event->position().toPoint();lastDrag_=dragStart_;event->accept();return;}
        QWidget::mousePressEvent(event);
    }
    void mouseMoveEvent(QMouseEvent *event) override {
        if(dragging_){
            const QPoint now=event->position().toPoint();
            const int delta=now.x()-lastDrag_.x();
            if(qAbs(now.x()-dragStart_.x())>3)dragMoved_=true;
            const int count=visibleCount();
            const int plotWidth=qMax(1,width()-36);
            if(delta!=0&&count<int(items_.size())){
                const int shift=qRound(-double(delta)*count/plotWidth);
                if(shift!=0){windowStart_+=shift;clampWindow();update();}
            }
            lastDrag_=now;event->accept();return;
        }
        QWidget::mouseMoveEvent(event);
    }
    void mouseReleaseEvent(QMouseEvent *event) override {
        if(event->button()==Qt::LeftButton&&dragging_){
            dragging_=false;
            if(!dragMoved_){const int index=indexAt(event->position().x());if(index>=0&&onActivated)onActivated(index);}
            event->accept();return;
        }
        QWidget::mouseReleaseEvent(event);
    }
    void wheelEvent(QWheelEvent *event) override {
        if(event->modifiers().testFlag(Qt::ControlModifier)||event->angleDelta().y()!=0){
            setZoomLevel(zoomLevel_+(event->angleDelta().y()>0?1:-1));
        }else if(event->angleDelta().x()!=0){
            const int count=visibleCount();windowStart_+=event->angleDelta().x()>0?-qMax(1,count/6):qMax(1,count/6);clampWindow();update();
        }
        event->accept();
    }
private:
    int visibleCount()const{
        if(items_.isEmpty())return 1;
        const double divisor=std::pow(1.7,zoomLevel_);
        return qBound(qMin(12,int(items_.size())),int(std::ceil(items_.size()/divisor)),int(items_.size()));
    }
    void clampWindow(){
        const int count=visibleCount();
        windowStart_=qBound(0,windowStart_,qMax(0,int(items_.size())-count));
    }
    int indexAt(qreal x)const{
        if(items_.isEmpty())return -1;
        const int count=visibleCount();
        const int start=qBound(0,windowStart_,qMax(0,int(items_.size())-count));
        const qreal left=18.0,right=qMax(left+1.0,width()-18.0);
        const int approximate=start+qBound(0,int(qRound((x-left)/(right-left)*qMax(0,count-1))),qMax(0,count-1));
        int best=-1,bestDistance=std::numeric_limits<int>::max();
        for(int i=qMax(start,approximate-2);i<qMin(int(items_.size()),approximate+3);++i){
            if(!items_[i].visible)continue;
            const int distance=qAbs(i-approximate);
            if(distance<bestDistance){bestDistance=distance;best=i;}
        }
        return bestDistance<=qMax(1,count/18)?best:approximate<int(items_.size())&&items_[approximate].visible?approximate:best;
    }
    QVector<ReplayTimelineItem> items_;
    int currentIndex_=-1,windowStart_=0,zoomLevel_=0;
    bool dragging_=false,dragMoved_=false;
    QPoint dragStart_,lastDrag_;
};

class ImagePreview final : public QWidget {
public:
    explicit ImagePreview(QWidget *parent=nullptr):QWidget(parent){setMinimumSize(220,145);}
    void setImage(const QPixmap &image){image_=image;update();}
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);p.setRenderHint(QPainter::SmoothPixmapTransform);
        p.fillRect(rect(),QColor("#f7faff"));
        if(image_.isNull()){
            p.setPen(QColor("#758397"));
            p.drawText(rect().adjusted(20,20,-20,-20),Qt::AlignCenter|Qt::TextWordWrap,
                orbitWidgetText(QStringLiteral("暂无恢复图像\n\n扫描 SSDV 分片后选择图像 ID，\n即可在软件内重建 JPEG")));
            return;
        }
        const QSize scaled=image_.size().scaled(size()-QSize(20,20),Qt::KeepAspectRatio);
        const QRect dst(QPoint((width()-scaled.width())/2,(height()-scaled.height())/2),scaled);
        p.drawPixmap(dst,image_,image_.rect());
    }
private:
    QPixmap image_;
};
