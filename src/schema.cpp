#include "schema.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QtEndian>
#include <bit>
#include <cmath>
namespace orbit {
namespace {
int parseId(const QJsonValue &v) {
    if(v.isDouble())return v.toInt(-1);
    QString s=v.toString();bool ok=false;
    const int n=s.startsWith("0x",Qt::CaseInsensitive)?s.mid(2).toInt(&ok,16):s.toInt(&ok);
    return ok?n:-1;
}
template<typename T>T be(const QByteArray &x,int at){
    return qFromBigEndian<T>(reinterpret_cast<const uchar*>(x.constData()+at));
}
}
bool Schema::load(const QString &path,QString *error){
    QFile f(path);
    if(!f.open(QIODevice::ReadOnly)){if(error)*error=f.errorString();return false;}
    QJsonParseError issue;
    const auto json=QJsonDocument::fromJson(f.readAll(),&issue);
    if(issue.error!=QJsonParseError::NoError || !json.isObject()){
        if(error)*error=issue.errorString();return false;
    }
    const QJsonObject root=json.object();
    const QString sat=root.value("satellite").toString();
    const QString version=root.value("revision").toString();
    const QJsonArray fields=root.value("fields").toArray();
    if(sat.isEmpty()||version.isEmpty()||!root.value("fields").isArray()){
        if(error)*error="Expected satellite, revision and fields[]";return false;
    }
    QVector<Rule> rules;
    for(const auto &item:fields){
        if(!item.isObject()){if(error)*error="Field item must be an object";return false;}
        const auto obj=item.toObject();
        Rule r;
        r.name=obj.value("name").toString();
        r.type=obj.value("type").toString();
        r.unit=obj.value("unit").toString();
        r.status=obj.value("status").toString("schema_declared_unverified");
        r.apid=parseId(obj.value("apid"));
        r.offset=obj.value("offset").toInt(-1);
        r.bit=obj.value("bit").toInt(-1);
        r.scale=obj.value("scale").toDouble(1.0);
        r.bias=obj.value("bias").toDouble(0.0);
        r.enumerations=obj.value("enums").toObject();
        const auto condition=obj.value("condition").toObject();
        r.conditionOffset=condition.value("offset").toInt(-1);
        r.conditionHex=condition.value("hex").toString();
        if(r.name.isEmpty()||r.apid<0||r.apid>2047||r.offset<0||r.offset>65535||
           !std::isfinite(r.scale)||!std::isfinite(r.bias) ||
           (r.conditionOffset>=0&&r.conditionHex.isEmpty())){
            if(error)*error="Invalid rule: "+r.name;return false;
        }
        if(r.type!="u8"&&r.type!="i8"&&r.type!="u16be"&&r.type!="i16be"&&
           r.type!="u32be"&&r.type!="i32be"&&r.type!="f32be"&&r.type!="bit"){
            if(error)*error="Unsupported field type "+r.type;return false;
        }
        if(r.type=="bit"&&(r.bit<0||r.bit>7)){if(error)*error="Bit index outside [0,7]";return false;}
        if(!r.conditionHex.isEmpty()){
            if(r.conditionHex.size()%2!=0){
                if(error)*error="Odd-length hex condition";return false;
            }
            for(const auto ch:r.conditionHex)if(!ch.isDigit()&&
                    (ch.toLower()<'a'||ch.toLower()>'f')){
                if(error)*error="Invalid hex condition";return false;
            }
        }
        rules.push_back(r);
    }
    satellite_=sat;revision_=version;rules_=rules;return true;
}
int Schema::apply(Frame &f) const {
    if(!f.complete||f.satellite!=satellite_)return 0;
    int count=0;
    for(const Rule &r:rules_){
        if(r.apid!=f.apid)continue;
        if(r.conditionOffset>=0){
            const QByteArray prefix=QByteArray::fromHex(r.conditionHex.toLatin1());
            if(f.payload.mid(r.conditionOffset,prefix.size())!=prefix)continue;
        }
        const bool wide=r.type.endsWith("32be");
        const bool medium=r.type.endsWith("16be");
        const int size=wide?4:medium?2:1;
        if(f.payload.size()-r.offset<size)continue;
        double raw=0;
        if(r.type=="u8")raw=quint8(f.payload.at(r.offset));
        else if(r.type=="i8")raw=qint8(f.payload.at(r.offset));
        else if(r.type=="u16be")raw=be<quint16>(f.payload,r.offset);
        else if(r.type=="i16be")raw=be<qint16>(f.payload,r.offset);
        else if(r.type=="u32be")raw=be<quint32>(f.payload,r.offset);
        else if(r.type=="i32be")raw=be<qint32>(f.payload,r.offset);
        else if(r.type=="f32be")raw=std::bit_cast<float>(be<quint32>(f.payload,r.offset));
        else if(r.type=="bit")raw=(quint8(f.payload.at(r.offset))>>r.bit)&1;
        if(!std::isfinite(raw))continue;
        const double converted=raw*r.scale+r.bias;
        if(!std::isfinite(converted))continue;
        QString value=QString::number(converted,'g',12);
        const QString named=r.enumerations.value(QString::number(qint64(raw))).toString();
        if(!named.isEmpty())value+=" ("+named+")";
        bool already=false;
        for(const auto &present:f.fields)if(present.name==r.name){already=true;break;}
        if(!already){f.fields.push_back({r.name,value,r.unit,r.status});++count;}
    }
    return count;
}
QStringList Schema::names()const{
    QStringList result;for(const auto &r:rules_)result<<r.name;return result;
}
}
