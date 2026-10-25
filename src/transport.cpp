#include "transport.h"
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <algorithm>
#include <array>

namespace orbit {
namespace {
int octet(char c) { return static_cast<unsigned char>(c); }
quint32 reflect(quint32 value,int width) {
    quint32 result=0;
    for(int i=0;i<width;++i){ result=(result<<1)|((value>>i)&1u); }
    return result;
}
bool isPaddingOnly(const QByteArray &bytes) {
    if(bytes.isEmpty())return true;
    return std::all_of(bytes.cbegin(),bytes.cend(),
                       [](char c){return static_cast<unsigned char>(c)==0xaa;});
}
struct State {
    QByteArray buffer;
    bool started = false;
    bool escaped = false;
    bool oversized = false;
    bool haveCount = false;
    int previousCount = -1;
    int firstMaster = -1, lastMaster = -1;
    int frameCount=0;
    bool allFecReported=true;
    bool anyFecReported=false;
    QDateTime firstTime;
    QString timeBasis="unknown";
    QStringList sources;
    void resetPacket(){
        buffer.clear();escaped=false;oversized=false;
        firstMaster=-1;lastMaster=-1;frameCount=0;
        allFecReported=true;anyFecReported=false;
        firstTime={};timeBasis="unknown";sources.clear();
    }
};
void recognizeOptionalSpacePacket(ApplicationPacket &packet) {
    const auto &b=packet.bytes;
    if(b.size()<7)return;
    const int id=(octet(b[0])<<8)|octet(b[1]);
    const int seq=(octet(b[2])<<8)|octet(b[3]);
    const int len=1+(octet(b[4])<<8)+octet(b[5]);
    if((id>>13)!=0 || len<1 || len+6!=b.size())return;
    packet.apid=id&0x7ff;
    packet.sequence=seq&0x3fff;
    packet.data=b.mid(6);
    packet.classification="ccsds_space_packet_candidate";
    packet.diagnostic="Optional internal six-byte CCSDS layout matches; application CRC is separate";
}
}
QByteArray Transport::frameData(const QByteArray &frame) {
    return frame.size()==223?frame.mid(5,218):QByteArray();
}
QString Transport::checkCrc(const QByteArray &packet,const CrcConfig &config){
    if(!config.enabled)return "not_checked";
    if(config.width!=16&&config.width!=32)return "unsupported_configuration";
    if(config.skipLeadingBytes<0)return "unsupported_configuration";
    const int suffix=config.width/8;
    if(packet.size()<config.skipLeadingBytes+suffix)return "insufficient_bytes";
    const quint32 mask=config.width==32?0xffffffffu:0xffffu;
    quint32 remainder=config.initial&mask;
    const quint32 poly=config.polynomial&mask;
    const int end=packet.size()-suffix;
    for(int n=config.skipLeadingBytes;n<end;++n){
        quint32 v=quint32(octet(packet[n]));
        if(config.reflectInput)v=reflect(v,8);
        remainder ^= v<<(config.width-8);
        for(int bit=0;bit<8;++bit)
            remainder=(remainder & (quint32(1)<<(config.width-1)))
                ? ((remainder<<1)^poly)&mask : (remainder<<1)&mask;
    }
    if(config.reflectOutput)remainder=reflect(remainder,config.width);
    remainder=(remainder ^ config.finalXor)&mask;
    quint32 received=0;
    if(config.trailerLittleEndian){
        for(int n=suffix-1;n>=0;--n)received=(received<<8)|octet(packet[end+n]);
    }else{
        for(int n=0;n<suffix;++n)received=(received<<8)|octet(packet[end+n]);
    }
    return remainder==received?"pass":"fail";
}
QVector<ApplicationPacket> Transport::reassemble(const QVector<Frame> &frames,const CrcConfig &crc){
    QVector<ApplicationPacket> out;
    QHash<int,State> states;
    constexpr int MaximumKissPacket=65536;
    // The original recorder clock may be absent for BIN-only frames. A bounded
    // sequence window can be ordered by the TM master counter without inventing time.
    QVector<const Frame*> ordered;
    ordered.reserve(frames.size());
    QVector<int> counters;
    for(const auto &f:frames){
        ordered.push_back(&f);
        if(f.master>=0) counters.push_back(f.master);
    }
    if(counters.size()>=2 && counters.size()<128){
        std::sort(counters.begin(),counters.end());
        counters.erase(std::unique(counters.begin(),counters.end()),counters.end());
        int gap=-1,start=0;
        for(int i=0;i<counters.size();++i){
            const int next=counters[(i+1)%counters.size()];
            const int distance=(next-counters[i]+256)%256;
            if(distance>gap){gap=distance;start=next;}
        }
        // If the sample covers nearly an entire 256-count cycle, ordering is
        // ambiguous and we preserve source order instead.
        if(gap>128){
            std::stable_sort(ordered.begin(),ordered.end(),[start](const Frame *a,const Frame *b){
                if(a->master<0)return false;
                if(b->master<0)return true;
                return ((a->master-start+256)%256)<((b->master-start+256)%256);
            });
        }
    }
    for(const Frame *source:ordered){
        const Frame &frame=*source;
        if(frame.vcid<0)continue;
        State &s=states[frame.vcid];
        // Only the per-VC counter can assess continuity; master count interleaves VCs.
        if(s.haveCount && ((s.previousCount+1)&0xff)!=frame.virtualCount){
            s.resetPacket();
            s.started=false;
        }
        s.previousCount=frame.virtualCount;
        s.haveCount=frame.virtualCount>=0;
        if(!frame.complete || frame.bytes.size()!=223){
            s.resetPacket();s.started=false;continue;
        }
        const QByteArray data=frame.bytes.mid(5,218);
        bool countedThisFrame=false;
        for(char c:data){
            const int b=octet(c);
            if(b==0xc0){
                if(s.started && !s.buffer.isEmpty() && !s.oversized &&
                   !s.escaped && !isPaddingOnly(s.buffer)){
                    ApplicationPacket pkt;
                    pkt.bytes=s.buffer;pkt.satellite=frame.satellite;
                    pkt.vcid=frame.vcid;
                    pkt.firstMaster=s.firstMaster;
                    pkt.lastMaster=s.lastMaster;
                    pkt.contributingFrames=s.frameCount;
                    pkt.sources=s.sources;
                    pkt.captureLocal=s.firstTime;
                    pkt.timestampBasis=s.timeBasis;
                    pkt.crcStatus=checkCrc(pkt.bytes,crc);
                    pkt.fecEvidence=s.allFecReported && s.anyFecReported
                        ? "log_fec_reported"
                        : (s.anyFecReported?"mixed_fec_unverified":"bin_only_unverified");
                    recognizeOptionalSpacePacket(pkt);
                    out.push_back(std::move(pkt));
                }
                s.resetPacket();
                s.started=true;
                countedThisFrame=false;
                continue;
            }
            // Bytes outside any KISS boundary belong to an incomplete or unusable fragment.
            if(!s.started || s.oversized)continue;
            if(s.firstMaster<0)s.firstMaster=frame.master;
            s.lastMaster=frame.master;
            if(!countedThisFrame){
                ++s.frameCount;countedThisFrame=true;
                if(frame.quality=="log_fec_reported")s.anyFecReported=true;
                else s.allFecReported=false;
            }
            if(!frame.sources.isEmpty())
                for(const QString &source:frame.sources)
                    if(!s.sources.contains(source))s.sources<<source;
            if(!s.firstTime.isValid() && frame.captureLocal.isValid()){
                s.firstTime=frame.captureLocal;s.timeBasis=frame.timeBasis;
            }
            if(s.escaped){
                s.escaped=false;
                if(b==0xdc)s.buffer.append(char(0xc0));
                else if(b==0xdd)s.buffer.append(char(0xdb));
                else {s.oversized=true;s.buffer.clear();}
            }else if(b==0xdb)s.escaped=true;
            else s.buffer.append(c);
            if(s.buffer.size()>MaximumKissPacket){
                s.oversized=true;s.buffer.clear();
            }
        }
    }
    // No closing FEND: never publish a partial packet as a valid application packet.
    return out;
}
bool Transport::exportJson(const QString &path,const QVector<ApplicationPacket> &packets,QString *error){
    QJsonArray a;
    for(const auto &p:packets){
        QJsonArray src;
        for(const auto &s:p.sources)src.append(s);
        a.append(QJsonObject{
          {"satellite",p.satellite},{"vcid",p.vcid},
          {"first_master",p.firstMaster},{"last_master",p.lastMaster},
          {"contributing_frames",p.contributingFrames},
          {"capture_local",p.captureLocal.isValid()?p.captureLocal.toString(Qt::ISODateWithMs):QString()},
          {"timestamp_basis",p.timestampBasis},{"spacecraft_utc",QJsonValue::Null},
          {"classification",p.classification},{"crc_status",p.crcStatus},
          {"fec_evidence",p.fecEvidence},
          {"apid",p.apid},{"packet_sequence",p.sequence},
          {"data_hex",QString::fromLatin1(p.bytes.toHex())},
          {"user_data_hex",QString::fromLatin1(p.data.toHex())},
          {"diagnostic",p.diagnostic},{"sources",src}
        });
    }
    QSaveFile f(path);
    if(!f.open(QIODevice::WriteOnly)){if(error)*error=f.errorString();return false;}
    f.write(QJsonDocument(a).toJson(QJsonDocument::Indented));
    if(!f.commit()){if(error)*error=f.errorString();return false;}
    return true;
}
}
