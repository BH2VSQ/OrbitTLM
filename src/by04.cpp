#include "by04.h"
extern "C" {
#include <third_party/ssdv_dslwp/ssdv.h>
}
#include <QSet>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QResource>
#include <algorithm>
#include <cstdint>
#include <bit>
#include <cmath>
// Keep Qt resource registration outside namespace orbit for static linking.
static void registerBy04Catalog(){Q_INIT_RESOURCE(by04_catalog);}
namespace orbit {
namespace {
int u8(const QByteArray &data,int index){return int(quint8(data.at(index)));}
QString hex(unsigned value,int digits=2){
    return QStringLiteral("0x%1").arg(value,digits,16,QChar('0')).toUpper();
}
EngineeringField field(const QString &code,const QString &name,
                       int value,int bytes,const QString &status="protocol_header_only"){
    QString raw=QString("%1").arg(quint32(value),bytes*2,16,QChar('0')).toUpper();
    return {code,"",name,"","",raw,QString::number(value),
            QString::number(value),"",status};
}
quint32 crc32Ssdv(const QByteArray &frame,quint32 initial) {
    // DSLWP and JAMX use the same 218-byte image packet format with different CRC seeds.
    if(frame.size()!=218)return 0;
    quint32 crc=initial;
    for(int i=0;i<214;++i){
        quint32 part=(crc ^ quint8(frame.at(i)))&0xffu;
        for(int bit=0;bit<8;++bit)
            part=(part&1u)?(part>>1u)^0xEDB88320u:(part>>1u);
        crc=(crc>>8u)^part;
    }
    return crc ^ 0xffffffffu;
}
const QVector<QJsonObject> &by04Catalog(){
    static const auto catalog=[](){
        registerBy04Catalog();
        QVector<QJsonObject> definitions;
        for(const auto &name:{"OBC-SF","BUV-S0"}){
            QFile file(QString(":/by04/%1.json").arg(name));
            if(file.open(QIODevice::ReadOnly)){
                const auto doc=QJsonDocument::fromJson(file.readAll());
                if(doc.isObject())definitions.push_back(doc.object());
            }
        }
        return definitions;
    }();
    return catalog;
}
bool decodeBusiness(const ApplicationPacket &packet,BusinessPacket &result){
    if(canonicalSatelliteName(packet.satellite)!=QStringLiteral("BY04"))return false;
    const QByteArray &bytes=packet.assembledBytes.isEmpty()?packet.bytes:packet.assembledBytes;
    if(bytes.size()<7||(u8(bytes,2)>>6)!=3)return false;
    const int identity=(u8(bytes,0)<<8)|u8(bytes,1);
    const int declaredOctets=((u8(bytes,4)<<8)|u8(bytes,5))+7;
    const int suffix=bytes.size()-declaredOctets;
    if(suffix!=0&&suffix!=2&&suffix!=4)return false;
    for(const auto &definition:by04Catalog()){
        if(identity!=definition.value("identity").toInt()||
           packet.vcid!=definition.value("vcid").toInt()||
           declaredOctets!=definition.value("kiss_octets").toInt()||
           u8(bytes,6)!=definition.value("subtype").toInt())continue;
        const QString code=definition.value("packet").toString();
        result.packetName="BY04-"+code;
        result.packetLabel=code=="OBC-SF"?QStringLiteral("计算机 · 关键遥测"):
                                            QStringLiteral("基带板 · UV 遥测");
        result.identification="by04_kiss_envelope";
        result.mainType=u8(bytes,1);
        result.subType=u8(bytes,6);
        result.fieldBaseKissOctet=7;
        result.definitionEvidence=definition.value("identification_evidence").toString();
        const QByteArray payload=bytes.mid(7,declaredOctets-7);
        for(const auto &entry:definition.value("fields").toArray()){
            auto field=BusinessCatalog::decodeField(entry.toObject(),payload);
            if(field.status=="truncated_field"){
                ++result.skippedFields;
                continue;
            }
            result.fields.push_back(std::move(field));
        }
        return true;
    }
    return false;
}
}
int By04Decoder::spacecraftId(const QByteArray &tm){
    if(tm.size()!=223)return -1;
    return ((quint8(tm[0])&0x3fu)<<4)|(quint8(tm[1])>>4);
}
BusinessPacket By04Decoder::decode(const ApplicationPacket &packet){
    BusinessPacket result;
    result.transport=packet;
    result.transport.satellite=canonicalSatelliteName(packet.satellite);
    result.packetLabel=QStringLiteral("BY04 未分类应用数据包");
    if(packet.assemblyStatus=="incomplete_segments"){
        result.identification="by04_incomplete_segments";
        result.packetLabel=QStringLiteral("分块数据 · 未收齐");
        return result;
    }
    if(decodeBusiness(packet,result))return result;
    if(packet.classification!="ccsds_space_packet_candidate" ||
       packet.apid<0 || packet.bytes.size()<6) {
        result.identification="by04_opaque_kiss";
        return result;
    }
    const QByteArray &b=packet.bytes;
    const int identity=(u8(b,0)<<8)|u8(b,1);
    const int sequence=(u8(b,2)<<8)|u8(b,3);
    const int declaredLength=((u8(b,4)<<8)|u8(b,5))+1;
    if(((identity>>13)&7)!=0 || declaredLength!=packet.data.size()) {
        result.identification="by04_opaque_kiss";
        return result;
    }
    result.packetName=QStringLiteral("BY04-CCSDS");
    result.packetLabel=QStringLiteral("BY04 应用包 · APID %1").arg(hex(packet.apid,3));
    result.identification="by04_ccsds_header_only";
    // APID is not a BY04 mission subsystem main/sub type.
    result.mainType=-1;
    result.subType=-1;
    result.fields.push_back(field("by04.ccsds.apid",QStringLiteral("应用进程标识 APID"),
                                  packet.apid,2));
    result.fields.push_back(field("by04.ccsds.sequence_flags",
                                  QStringLiteral("包序列标志"),(sequence>>14)&3,1));
    result.fields.push_back(field("by04.ccsds.sequence_count",
                                  QStringLiteral("包序列计数"),sequence&0x3fff,2));
    result.fields.push_back(field("by04.ccsds.data_octets",
                                  QStringLiteral("CCSDS 用户数据长度"),declaredLength,2));
    result.fields.push_back(field("by04.ccsds.extra_octets",
                                  QStringLiteral("外部未定义尾部字节数"),
                                  std::max(0,packet.opaqueSuffixBytes),1));
    if(!packet.data.isEmpty()){
        const QString preview=QString::fromLatin1(packet.data.left(32).toHex(' ').toUpper());
        result.fields.push_back({"by04.payload.preview","",QStringLiteral("业务数据前 32 字节（未定义字段）"),"","",
                                 QString::fromLatin1(packet.data.left(32).toHex().toUpper()),
                                 "",preview,"","raw_unverified_business"});
        result.fields.push_back(field("by04.payload.first_octet",
             QStringLiteral("业务载荷首字节（含义未确认）"),u8(packet.data,0),1,
             "raw_unverified_business"));
    }
    return result;
}
QVector<BusinessPacket> By04Decoder::decodeAll(const QVector<ApplicationPacket> &packets){
    QVector<BusinessPacket> decoded;
    decoded.reserve(packets.size());
    for(const auto &packet:Transport::assembleSegments(packets,1))decoded.push_back(decode(packet));
    std::stable_sort(decoded.begin(),decoded.end(),[](const BusinessPacket &a,const BusinessPacket &b){
        const QDateTime &ta=a.transport.captureLocal,&tb=b.transport.captureLocal;
        if(ta.isValid()!=tb.isValid())return ta.isValid();
        if(!ta.isValid())return false;
        return ta<tb;
    });
    return decoded;
}
QVector<BusinessPacket> By04Decoder::definitionPackets(){
    QVector<BusinessPacket> result;
    for(const auto &definition:by04Catalog()){
        BusinessPacket packet;
        const QString code=definition.value("packet").toString();
        packet.packetName=QStringLiteral("BY04-")+code;
        packet.packetLabel=definition.value("label_cn").toString();
        packet.identification=QStringLiteral("catalog_definition");
        packet.definitionEvidence=definition.value("identification_evidence").toString();
        packet.fieldBaseKissOctet=definition.value("field_base_kiss_octet").toInt(7);
        packet.transport.satellite=QStringLiteral("BY04");
        for(const auto &entry:definition.value("fields").toArray()){
            const auto object=entry.toObject();
            EngineeringField field;
            field.code=object.value("id").toString();
            field.bits=object.value("bits").toString();
            field.nameCn=object.value("name_cn").toString();
            field.nameEn=object.value("name_en").toString();
            field.formula=object.value("formula").toString();
            field.unit=object.value("unit").toString();
            field.dataType=object.value("data_type").toString();
            field.definitionEvidence=object.value("formula_source").toString();
            field.status=QStringLiteral("catalog_definition");
            packet.fields.push_back(std::move(field));
        }
        result.push_back(std::move(packet));
    }
    return result;
}
QVector<ImagePacket> By04Decoder::images(const QVector<Frame> &frames){
    QVector<ImagePacket> packets;
    QSet<QByteArray> seen;
    for(const Frame &f:frames){
        // The upstream receiver identifies BY04 short TM VC1 using 0x2052.
        if(!f.complete || f.bytes.size()!=223 || f.vcid!=1 ||
           spacecraftId(f.bytes)!=SpacecraftId)continue;
        const QByteArray data=f.bytes.mid(5,218);
        if(seen.contains(data))continue;
        // Match metadata validation to the upstream SSDV implementation used to
        // reconstruct the JPEG, rather than duplicating its header interpretation.
        const auto *bytes=reinterpret_cast<const std::uint8_t*>(data.constData());
        ssdv_packet_info_t info{};
        ssdv_dec_header(&info,bytes,ssdv_dslwp_mode);
        const int width=info.width,height=info.height;
        if(width<=0||height<=0||width>8192||height>8192||
           info.mcu_count==0||info.mcu_count>65535||
           (info.mcu_id!=0xffff &&
            (info.mcu_id>=info.mcu_count||info.mcu_offset>=205)))continue;
        seen.insert(data);
        ImagePacket image;
        image.bytes=data;image.apid=-1;
        image.imageId=info.image_id;
        image.packetId=info.packet_id;
        image.width=width;image.height=height;
        const quint32 expected=(quint32(u8(data,214))<<24)|
                               (quint32(u8(data,215))<<16)|
                               (quint32(u8(data,216))<<8)|quint32(u8(data,217));
        image.status=crc32Ssdv(data,0x4EE4FDE1u)==expected?
                        "dslwp_crc32_pass":"dslwp_crc32_failed";
        packets.push_back(image);
    }
    std::stable_sort(packets.begin(),packets.end(),[](const ImagePacket &a,const ImagePacket &b){
        if(a.imageId!=b.imageId)return a.imageId<b.imageId;
        return a.packetId<b.packetId;
    });
    return packets;
}
}
