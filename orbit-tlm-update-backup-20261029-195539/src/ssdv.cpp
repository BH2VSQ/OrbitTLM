#include "ssdv.h"
#include <QFile>
#include <QSaveFile>
#include <algorithm>
#include <cstdint>
#include <map>
#include <limits>
#include <vector>
extern "C" {
#include <third_party/ssdv_dslwp/ssdv.h>
}
namespace orbit {
QVector<ImagePacket> ImageTools::candidates(const QVector<Frame> &frames,int apid){
    return candidatesFromKiss(Transport::reassemble(frames),apid);
}
QVector<ImagePacket> ImageTools::candidatesFromKiss(
        const QVector<ApplicationPacket> &packets,int apid){
    QVector<ImagePacket> out;
    QByteArray stream;
    int lastSequence=-1;
    for(const auto &packet:packets){
        // Never stitch arbitrary 223-byte TM regions. The application KISS
        // stream must be reconstructed before image data is consumed.
        if(packet.apid!=apid || packet.fecEvidence!="log_fec_reported" ||
           packet.classification!="ccsds_space_packet_candidate")continue;
        if(lastSequence>=0 && (lastSequence+1)%16384!=packet.sequence)stream.clear();
        lastSequence=packet.sequence;
        stream+=packet.data;
        while(stream.size()>=256){
            const int pos=stream.indexOf(char(0x55));
            if(pos<0){stream.clear();break;}
            if(pos>0)stream.remove(0,pos);
            if(stream.size()<256)break;
            const int type=quint8(stream[1]);
            const int width=quint8(stream[9])*16;
            const int height=quint8(stream[10])*16;
            if((type==0x66||type==0x67)&&width>0&&height>0&&
               width<=4080&&height<=4080){
                ImagePacket p;
                p.bytes=stream.left(256);p.apid=apid;
                p.imageId=quint8(stream[6]);
                p.packetId=(quint8(stream[7])<<8)|quint8(stream[8]);
                p.width=width;p.height=height;
                out.push_back(p);
                stream.remove(0,256);
            }else stream.remove(0,1);
        }
        if(stream.size()>4096)stream=stream.right(255);
    }
    return out;
}
bool ImageTools::savePackets(const QVector<ImagePacket> &packets,const QString &filename,QString *error){
    QSaveFile file(filename);
    if(!file.open(QIODevice::WriteOnly)){if(error)*error=file.errorString();return false;}
    for(const auto &p:packets)if(file.write(p.bytes)!=p.bytes.size()){
        if(error)*error=file.errorString();file.cancelWriting();return false;
    }
    if(!file.commit()){if(error)*error=file.errorString();return false;}
    return true;
}

DecodedImage ImageTools::decodeImage(const QVector<ImagePacket> &packets,int imageId,bool dslwp){
    DecodedImage decoded;
    decoded.imageId=imageId;
    const int expectedSize=dslwp?SSDV_PKT_SIZE_DSLWP:SSDV_PKT_SIZE;
    std::map<int,ImagePacket> ordered;
    for(const auto &packet:packets){
        if(packet.imageId!=imageId||packet.bytes.size()!=expectedSize)continue;
        auto found=ordered.find(packet.packetId);
        if(found==ordered.end()){
            ordered.emplace(packet.packetId,packet);
        }else if(found->second.status!="dslwp_crc32_pass"&&
                 packet.status=="dslwp_crc32_pass"){
            found->second=packet;
        }
    }
    if(ordered.empty()){
        decoded.error=QStringLiteral("所选图像没有可用的 SSDV 分片，或分片长度与解码模式不匹配。");
        return decoded;
    }
    if(ordered.find(0)==ordered.end()){
        decoded.error=QStringLiteral("缺少 SSDV 首包（序号 0），无法初始化 JPEG 头。请导入更完整的 log 或 BIN 记录。");
        return decoded;
    }
    decoded.receivedPackets=int(ordered.size());
    for(const auto &entry:ordered){
        const auto &packet=entry.second;
        decoded.width=packet.width;
        decoded.height=packet.height;
        if(packet.status=="dslwp_crc32_pass")++decoded.crcPassedPackets;
    }

    const ssdv_mode_t mode=dslwp?ssdv_dslwp_mode:ssdv_normal_mode;
    std::vector<std::uint8_t> jpegBuffer(32U*1024U*1024U);
    // Match the receiver's recovery strategy: if one damaged packet prevents
    // reconstruction, discard that packet and restart with the remaining
    // ordered packets. A later retransmission can still provide a clean copy.
    while(!ordered.empty()){
        ssdv_t decoder{};
        if(ssdv_dec_init(&decoder)!=SSDV_OK){
            decoded.error=QStringLiteral("SSDV 解码器初始化失败。");
            return decoded;
        }
        if(ssdv_dec_set_buffer(&decoder,jpegBuffer.data(),jpegBuffer.size())!=SSDV_OK){
            decoded.error=QStringLiteral("SSDV JPEG 缓冲区初始化失败。");
            return decoded;
        }
        int rejectedId=-1;
        for(const auto &entry:ordered){
            const auto *bytes=reinterpret_cast<const std::uint8_t*>(entry.second.bytes.constData());
            const char result=ssdv_dec_feed(&decoder,bytes,mode);
            if(result==SSDV_BUFFER_FULL){
                decoded.error=QStringLiteral("SSDV JPEG 输出缓冲区已满。");
                return decoded;
            }
            if(result!=SSDV_FEED_ME&&result!=SSDV_OK){
                rejectedId=entry.first;
                break;
            }
            ssdv_packet_info_t info{};
            ssdv_dec_header(&info,bytes,mode);
            decoded.complete=decoded.complete||info.eoi!=0;
            if(result==SSDV_OK)break;
        }
        if(rejectedId>=0){
            ordered.erase(rejectedId);
            decoded.receivedPackets=int(ordered.size());
            decoded.complete=false;
            continue;
        }
        std::uint8_t *jpeg=nullptr;
        std::size_t jpegLength=0;
        if(ssdv_dec_get_jpeg(&decoder,&jpeg,&jpegLength)!=SSDV_OK||
           jpeg==nullptr||jpegLength==0||
           jpegLength>std::size_t(std::numeric_limits<int>::max())){
            decoded.error=QStringLiteral("已接收 %1 个分片，但当前数据不足以生成 JPEG。")
                .arg(qulonglong(ordered.size()));
            return decoded;
        }
        decoded.jpeg=QByteArray(reinterpret_cast<const char*>(jpeg),int(jpegLength));
        decoded.receivedPackets=int(ordered.size());
        return decoded;
    }
    decoded.error=QStringLiteral("有效分片不足，无法重建 JPEG。");
    return decoded;
}
}
