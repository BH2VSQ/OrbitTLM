#!/usr/bin/env python3
# Apply the OrbitTLM official telemetry-tree UI update to a local checkout.
# This script intentionally edits only known UI/CMake files and creates .bak backups.
from __future__ import annotations

import argparse
import re
import shutil
from pathlib import Path

def replace_function(source: str, start: str, end: str, replacement: str, filename: str) -> str:
    pattern = re.compile(re.escape(start) + r".*?(?=" + re.escape(end) + r")", re.S)
    updated, count = pattern.subn(replacement.rstrip() + "\n", source, count=1)
    if count != 1:
        raise RuntimeError(f"{filename}: could not locate function range starting {start!r}")
    return updated

def replace_once(source: str, old: str, new: str, filename: str, expected: int = 1) -> str:
    count = source.count(old)
    if count != expected:
        raise RuntimeError(f"{filename}: expected {expected} occurrence(s), found {count} for {old!r}")
    return source.replace(old, new, expected)

def write_changed(path: Path, content: str):
    if path.read_text(encoding="utf-8") == content:
        print(f"unchanged: {path.relative_to(repo)}")
        return
    backup = path.with_suffix(path.suffix + ".pre-telemetry-tree.bak")
    if not backup.exists():
        shutil.copy2(path, backup)
    path.write_text(content, encoding="utf-8", newline="\n")
    print(f"updated:   {path.relative_to(repo)} (backup: {backup.name})")

def build_component_tabs_function() -> str:
    return r'''void MainWindow::buildComponentTabs(QTabWidget *tabs){
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
        QFile file(QDir(catalogRoot).filePath(treeSatellite+QStringLiteral(".json")));
        if(!file.open(QIODevice::ReadOnly))continue;
        const QJsonDocument document=QJsonDocument::fromJson(file.readAll());
        if(!document.isObject())continue;
        const QJsonArray packetArray=document.object().value(QStringLiteral("packets")).toArray();

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
            systems->addTab(groupPage,treeSatellite+QStringLiteral(" · ")+groupName);
        }
    }

    layout->addWidget(systems,1);
    tabs->addTab(page,QStringLiteral("分系统数据"));
    if(satelliteSelect_){
        connect(satelliteSelect_,&QComboBox::currentTextChanged,this,
                [this]{updateComponentTabs(currentIndex_);});
    }
}
'''

def update_component_tabs_function() -> str:
    return r'''void MainWindow::updateComponentTabs(int index){
    if(componentTables_.isEmpty())return;
    const bool by04=resolvedSatellite()==QStringLiteral("BY04");
    const QString activeSatellite=by04?QStringLiteral("BY04"):QStringLiteral("ASRTU-1");

    // Hide the other satellite's subsystem tabs while keeping both official trees loaded.
    auto *systems=pages_?pages_->findChild<QTabWidget*>(QStringLiteral("officialSystemTabs")):nullptr;
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
'''

def main():
    parser = argparse.ArgumentParser(description="Apply official telemetry-tree improvements to OrbitTLM")
    parser.add_argument("--repo", required=True, help="Path to the local OrbitTLM repository checkout")
    args = parser.parse_args()
    global repo
    repo = Path(args.repo).resolve()
    if not (repo / "CMakeLists.txt").is_file() or not (repo / "src/mainwindow_build.cpp").is_file():
        raise SystemExit("错误：--repo 必须指向 OrbitTLM 仓库根目录（包含 CMakeLists.txt 和 src/mainwindow_build.cpp）。")

    assets = Path(__file__).resolve().parent / "telemetry-trees"
    if not (assets / "ASRTU-1.json").is_file() or not (assets / "BY04.json").is_file():
        raise SystemExit("错误：未找到 telemetry-trees/ASRTU-1.json 或 BY04.json。请从更新包完整解压后运行。")
    destination = repo / "telemetry-trees"
    destination.mkdir(parents=True, exist_ok=True)
    for name in ("ASRTU-1.json", "BY04.json"):
        shutil.copy2(assets / name, destination / name)
    print(f"copied:    {destination.relative_to(repo)}/ASRTU-1.json")
    print(f"copied:    {destination.relative_to(repo)}/BY04.json")

    build_path = repo / "src/mainwindow_build.cpp"
    build = build_path.read_text(encoding="utf-8")
    for include in ("#include <QFile>\n", "#include <QJsonArray>\n", "#include <QJsonDocument>\n", "#include <QJsonObject>\n"):
        if include.strip() not in build:
            build = build.replace('#include <QFileDialog>\n', '#include <QFileDialog>\n' + include, 1)
    build = replace_once(build, '    setMinimumSize(1050,690);',
                         '    setMinimumSize(1100,690);', str(build_path))
    build = replace_once(build, '    left->setObjectName("panel");',
                         '    left->setObjectName("panel");\n    left->setMinimumWidth(460);', str(build_path))
    build = replace_once(build, '    eventTable_->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Stretch);',
                         '    eventTable_->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Interactive);\n'
                         '    eventTable_->setColumnWidth(1,230);\n'
                         '    eventTable_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);', str(build_path))
    build = replace_once(build, '    pages_->setObjectName("mainPages");',
                         '    pages_->setObjectName("mainPages");\n    pages_->setMinimumWidth(600);', str(build_path))
    build = replace_once(build, '    split->setSizes({465,995});',
                         '    split->setSizes({490,960});', str(build_path))
    build = replace_function(build,
        'void MainWindow::buildComponentTabs(QTabWidget *tabs){',
        'void MainWindow::buildAttitude(QTabWidget *tabs){',
        build_component_tabs_function(), str(build_path))
    data_path = repo / "src/mainwindow_data.cpp"
    data = data_path.read_text(encoding="utf-8")

    old_quats = '''            addBy04(QStringLiteral("姿态四元数"),{QStringLiteral("姿态四元数q0"),QStringLiteral("姿态四元数q1"),
                QStringLiteral("姿态四元数q2"),QStringLiteral("姿态四元数q3")},true);
            addBy04(QStringLiteral("偏差四元数"),{QStringLiteral("偏差四元数q0"),QStringLiteral("偏差四元数q1"),
                QStringLiteral("偏差四元数q2"),QStringLiteral("偏差四元数q3")},true);'''
    new_quats = '''            addBy04(QStringLiteral("姿态四元数 q0"),{QStringLiteral("姿态四元数q0")});
            addBy04(QStringLiteral("姿态四元数 q1"),{QStringLiteral("姿态四元数q1")});
            addBy04(QStringLiteral("姿态四元数 q2"),{QStringLiteral("姿态四元数q2")});
            addBy04(QStringLiteral("姿态四元数 q3"),{QStringLiteral("姿态四元数q3")});
            addBy04(QStringLiteral("偏差四元数 q0"),{QStringLiteral("偏差四元数q0")});
            addBy04(QStringLiteral("偏差四元数 q1"),{QStringLiteral("偏差四元数q1")});
            addBy04(QStringLiteral("偏差四元数 q2"),{QStringLiteral("偏差四元数q2")});
            addBy04(QStringLiteral("偏差四元数 q3"),{QStringLiteral("偏差四元数q3")});'''
    data = replace_once(data, old_quats, new_quats, str(data_path))
    data = replace_once(data,
        '            addBy04(QStringLiteral("电池温度"),{QStringLiteral("电池温度1"),QStringLiteral("电池温度2")},true);',
        '            addBy04(QStringLiteral("电池温度1"),{QStringLiteral("电池温度1")});\n'
        '            addBy04(QStringLiteral("电池温度2"),{QStringLiteral("电池温度2")});', str(data_path))
    data = replace_once(data,
        '''            addBy04(QStringLiteral("温度采集"),{QStringLiteral("温度采集4-屏幕"),QStringLiteral("温度采集6-储箱"),
                QStringLiteral("温度采集7-冯相机"),QStringLiteral("温度采集8-帆板")},true);''',
        '''            addBy04(QStringLiteral("温度采集4-屏幕"),{QStringLiteral("温度采集4-屏幕")});
            addBy04(QStringLiteral("温度采集6-储箱"),{QStringLiteral("温度采集6-储箱")});
            addBy04(QStringLiteral("温度采集7-冯相机"),{QStringLiteral("温度采集7-冯相机")});
            addBy04(QStringLiteral("温度采集8-帆板"),{QStringLiteral("温度采集8-帆板")});''', str(data_path))
    data = replace_once(data,
        '''                        if(field.unit.isEmpty()&&field.status==QStringLiteral("raw_bits"))
                            shown+=QStringLiteral("（原始值，单位未提供）");
''', '', str(data_path))
    data = replace_once(data,
        '''            if(by04&&!row.sourceNames.isEmpty()&&value!=QStringLiteral("未解析")&&
               unit.isEmpty()&&resolvedField.status==QStringLiteral("raw_bits"))
                value+=QStringLiteral("（原始值，单位未提供）");
''', '', str(data_path))
    data = replace_function(data,
        'void MainWindow::updateComponentTabs(int index){',
        'void MainWindow::updateWebHistory(){',
        update_component_tabs_function(), str(data_path))
    widgets_path = repo / "src/mainwindow_widgets.h"
    widgets = widgets_path.read_text(encoding="utf-8")
    old_wrap = '''    static QString wrapText(const QString &text){
        if(text.contains(QLatin1Char('\\n')))return text;
        // Keep common Latin acronyms together, then wrap their Chinese suffix.
        int latinPrefix=0;
        while(latinPrefix<text.size()&&text.at(latinPrefix).isLetterOrNumber()&&
              text.at(latinPrefix).unicode()<128)++latinPrefix;
        if(latinPrefix>=3&&latinPrefix<text.size())
            return text.left(latinPrefix)+QLatin1Char('\\n')+text.mid(latinPrefix);
        const int split=qBound(1,text.size()/2,text.size()-1);
        return text.left(split)+QLatin1Char('\\n')+text.mid(split);
    }'''
    new_wrap = '''    static QString wrapText(const QString &text){
        // Keep each tab caption on one line. The subsystem tab row and the nested
        // official-packet tab row provide navigation levels without breaking labels.
        return text;
    }'''
    widgets = replace_once(widgets, old_wrap, new_wrap, str(widgets_path))
    widgets = replace_once(widgets, '        wrappingBar_->setElideMode(Qt::ElideNone);\n',
                           '        wrappingBar_->setElideMode(Qt::ElideNone);\n'
                           '        wrappingBar_->setUsesScrollButtons(true);\n', str(widgets_path))
    cmake_path = repo / "CMakeLists.txt"
    cmake = cmake_path.read_text(encoding="utf-8")
    copy_anchor = '''      COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/protocols/asrtu1"
        "$<TARGET_FILE_DIR:${target}>/protocols/asrtu1"
'''
    copy_new = copy_anchor + '''      COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/telemetry-trees"
        "$<TARGET_FILE_DIR:${target}>/telemetry-trees"
'''
    cmake = replace_once(cmake, copy_anchor, copy_new, str(cmake_path))
    install_anchor = '''install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/protocols/asrtu1"
        DESTINATION "bin/protocols")
'''
    install_new = install_anchor + '''install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/telemetry-trees"
        DESTINATION "bin")
'''
    cmake = replace_once(cmake, install_anchor, install_new, str(cmake_path))
    # Only write after all expected source anchors were validated successfully.
    write_changed(build_path, build)
    write_changed(data_path, data)
    write_changed(widgets_path, widgets)
    write_changed(cmake_path, cmake)

    print("\n完成。请重新配置并构建项目，然后检查总览、回放列表和两颗卫星的分系统数据页。")
    print("所有被改写的源码旁均保留了 .pre-telemetry-tree.bak 备份。")

if __name__ == "__main__":
    main()
