#include "ssdv.h"
#include <QFile>
#include <QSaveFile>
#include <QSet>
#include <algorithm>
#include <cstdint>
#include <map>
#include <limits>
#include <utility>
#include <vector>
extern "C" {
#include <third_party/ssdv_dslwp/ssdv.h>
}
namespace orbit {
QVector<ImagePacket> ImageTools::candidates(const QVector<Frame> &frames,int apid){
    return candidatesFromKiss(Transport::reassemble(frames),apid);
}
QVector<ImagePacket> ImageTools::candidatesFromFrames(const QVector<Frame> &frames){
    QVector<ImagePacket> out;
    QSet<QByteArray> seen;
    for(const Frame &frame:frames){
        // Both ASRTU-1 and BY04 VC1 carry the 218-byte SSDV core payload
        // after the five-byte TM header. Spacecraft ID is not an SSDV format
        // discriminator, so validate the payload itself instead of hard-coding
        // the BY04 SCID.
        if(frame.bytes.size()!=223||frame.vcid!=1)continue;
        const QByteArray payload=frame.bytes.mid(5,218);
        if(seen.contains(payload))continue;
        QByteArray checked=payload;
        int correctedSymbols=0;
        const int status=ssdv_dec_is_packet(
            reinterpret_cast<std::uint8_t*>(checked.data()),&correctedSymbols,ssdv_dslwp_mode);
        QByteArray header=status==SSDV_OK?checked:payload;
        ssdv_packet_info_t info{};
        ssdv_dec_header(&info,reinterpret_cast<std::uint8_t*>(header.data()),ssdv_dslwp_mode);
        if(info.width<=0||info.height<=0||info.width>8192||info.height>8192||
           info.mcu_count==0||info.mcu_count>65535||
           (info.mcu_id!=0xffff&&(info.mcu_id>=info.mcu_count||info.mcu_offset>=205)))continue;
        seen.insert(payload);
        ImagePacket packet;
        packet.bytes=status==SSDV_OK?checked:payload;
        packet.apid=-1;packet.imageId=info.image_id;packet.packetId=info.packet_id;
        packet.width=info.width;packet.height=info.height;
        packet.satellite=canonicalSatelliteName(frame.satellite);
        packet.captureLocal=frame.captureLocal;packet.sources=frame.sources;
        packet.fecEvidence=frame.quality;packet.vcid=frame.vcid;
        packet.firstMaster=frame.master;packet.lastMaster=frame.master;
        packet.status=status==SSDV_OK?QStringLiteral("ssdv_crc32_pass"):
                                       QStringLiteral("ssdv_crc32_failed");
        out.push_back(packet);
    }
    std::stable_sort(out.begin(),out.end(),[](const ImagePacket &a,const ImagePacket &b){
        if(a.imageId!=b.imageId)return a.imageId<b.imageId;
        return a.packetId<b.packetId;
    });
    return out;
}
QVector<ImagePacket> ImageTools::candidatesFromKiss(
        const QVector<ApplicationPacket> &packets,int apid){
    QVector<ImagePacket> out;
    QByteArray stream;
    int lastSequence=-1;
    for(const auto &packet:packets){
        // Never stitch arbitrary 223-byte TM regions. The application KISS
        // stream must be reconstructed before image data is consumed.
        if(packet.apid!=apid ||
           packet.classification!="ccsds_space_packet_candidate")continue;
        if(lastSequence>=0 && (lastSequence+1)%16384!=packet.sequence)stream.clear();
        lastSequence=packet.sequence;
        stream+=packet.data;
        while(stream.size()>=256){
            const int pos=stream.indexOf(char(0x55));
            if(pos<0){stream.clear();break;}
            if(pos>0)stream.remove(0,pos);
            if(stream.size()<256)break;
            // Try the reference SSDV validator before extracting metadata. Keep a
            // structurally valid but CRC-failing candidate visible for diagnosis;
            // decodeImage will reject it before JPEG reconstruction.
            QByteArray rawCandidate=stream.left(256);
            QByteArray checkedCandidate=rawCandidate;
            int correctedSymbols=0;
            const int packetStatus=ssdv_dec_is_packet(
                reinterpret_cast<std::uint8_t*>(checkedCandidate.data()),&correctedSymbols,ssdv_normal_mode);
            const QByteArray &headerBytes=packetStatus==SSDV_OK?checkedCandidate:rawCandidate;
            const int type=quint8(headerBytes[1]);
            const int width=quint8(headerBytes[9])*16;
            const int height=quint8(headerBytes[10])*16;
            if((type==0x66||type==0x67)&&width>0&&height>0&&width<=4080&&height<=4080){
                ImagePacket p;
                p.bytes=packetStatus==SSDV_OK?checkedCandidate:rawCandidate;p.apid=apid;
                p.imageId=quint8(headerBytes[6]);
                p.packetId=(quint8(headerBytes[7])<<8)|quint8(headerBytes[8]);
                p.width=width;p.height=height;
                p.satellite=canonicalSatelliteName(packet.satellite);
                p.captureLocal=packet.captureLocal;p.sources=packet.sources;
                p.fecEvidence=packet.fecEvidence;p.vcid=packet.vcid;
                p.firstMaster=packet.firstMaster;p.lastMaster=packet.lastMaster;
                if(packetStatus==SSDV_OK)p.status=QStringLiteral("ssdv_crc_pass");
                else if(packet.fecEvidence!="log_fec_reported")p.status=QStringLiteral("bin_only_unverified");
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


QVector<ImagePacket> ImageTools::parseBinaryPackets(
        const QByteArray &data,int apid,bool prefer218,QString *error){
    QVector<ImagePacket> out;
    if(data.isEmpty()){
        if(error)*error=QStringLiteral("BIN 文件为空。");
        return out;
    }
    const bool packets218=(data.size()%218==0) &&
        ((data.size()%256)!=0 || prefer218);
    if(packets218){
        for(int offset=0;offset+218<=data.size();offset+=218){
            QByteArray bytes=data.mid(offset,218);
            auto *raw=reinterpret_cast<std::uint8_t*>(bytes.data());
            ssdv_packet_info_t info{};
            ssdv_dec_header(&info,raw,ssdv_dslwp_mode);
            if(info.width<=0||info.height<=0||info.width>8192||info.height>8192||
               info.mcu_count==0||info.mcu_count>65535)continue;
            int correctedSymbols=0;
            QByteArray checked=bytes;
            const int status=ssdv_dec_is_packet(
                reinterpret_cast<std::uint8_t*>(checked.data()),&correctedSymbols,ssdv_dslwp_mode);
            ImagePacket packet;
            packet.bytes=status==SSDV_OK?checked:bytes;
            packet.apid=apid;packet.imageId=info.image_id;packet.packetId=info.packet_id;
            packet.width=info.width;packet.height=info.height;
            packet.status=status==SSDV_OK?QStringLiteral("ssdv_crc32_pass"):
                                           QStringLiteral("ssdv_crc32_failed");
            out.push_back(packet);
        }
    }else if(data.size()%256==0){
        for(int offset=0;offset+256<=data.size();offset+=256){
            QByteArray bytes=data.mid(offset,256);
            if(quint8(bytes[0])!=0x55)continue;
            const int type=quint8(bytes[1]);
            if(type!=0x66&&type!=0x67)continue;
            QByteArray checked=bytes;
            int correctedSymbols=0;
            const int status=ssdv_dec_is_packet(
                reinterpret_cast<std::uint8_t*>(checked.data()),&correctedSymbols,ssdv_normal_mode);
            const QByteArray &header=status==SSDV_OK?checked:bytes;
            const int width=quint8(header[9])*16;
            const int height=quint8(header[10])*16;
            if(width<=0||height<=0||width>4080||height>4080)continue;
            ImagePacket packet;
            packet.bytes=status==SSDV_OK?checked:bytes;packet.apid=apid;
            packet.imageId=quint8(header[6]);
            packet.packetId=(quint8(header[7])<<8)|quint8(header[8]);
            packet.width=width;packet.height=height;
            packet.status=status==SSDV_OK?QStringLiteral("ssdv_crc_pass"):
                                           QStringLiteral("bin_only_unverified");
            out.push_back(packet);
        }
    }else{
        if(error)*error=QStringLiteral(
            "BIN 长度不符合已支持的分片格式（218 字节 SSDV 分片或 256 字节标准 SSDV 包）。");
        return out;
    }
    if(out.isEmpty()&&error)*error=QStringLiteral(
        "文件中没有识别到有效 SSDV 分片；请确认导入的是图像 BIN，而不是遥测原始帧。");
    return out;
}

DecodedImage ImageTools::decodeImage(const QVector<ImagePacket> &packets,int imageId,bool shortPacketFormat){
    DecodedImage decoded;
    decoded.imageId=imageId;
    const int expectedSize=shortPacketFormat?SSDV_PKT_SIZE_DSLWP:SSDV_PKT_SIZE;
    const ssdv_mode_t mode=shortPacketFormat?ssdv_dslwp_mode:ssdv_normal_mode;
    std::map<int,ImagePacket> ordered;
    std::map<int,ImagePacket> observed;
    for(const auto &packet:packets){
        if(packet.imageId!=imageId||packet.bytes.size()!=expectedSize||packet.packetId<0)continue;
        auto rawFound=observed.find(packet.packetId);
        if(rawFound==observed.end())observed.emplace(packet.packetId,packet);
        QByteArray correctedBytes=packet.bytes;
        int correctedSymbols=0;
        const int validation=ssdv_dec_is_packet(
            reinterpret_cast<std::uint8_t*>(correctedBytes.data()),&correctedSymbols,mode);
        if(validation!=SSDV_OK){
            if(!decoded.badPacketIds.contains(packet.packetId))decoded.badPacketIds.push_back(packet.packetId);
            continue;
        }
        ImagePacket validated=packet;
        validated.bytes=correctedBytes;
        if(shortPacketFormat)validated.status=QStringLiteral("ssdv_crc32_pass");
        else validated.status=QStringLiteral("ssdv_crc_pass");
        auto found=ordered.find(validated.packetId);
        if(found==ordered.end())ordered.emplace(validated.packetId,validated);
    }
    if(!observed.empty()){
        const int last=observed.rbegin()->first;
        for(int id=0;id<=last;++id)
            if(observed.find(id)==observed.end())decoded.missingPacketIds.push_back(id);
    }
    std::sort(decoded.badPacketIds.begin(),decoded.badPacketIds.end());
    if(observed.empty()){
        decoded.error=QStringLiteral("所选图像没有可用的 SSDV 分片，或分片长度与解码模式不匹配。");
        return decoded;
    }

    // A damaged or missing packet zero must not prevent a partial image preview.
    // The SSDV header carries the image dimensions, quantisation mode and MCU
    // layout in every packet. Build a header-only packet zero from the most
    // trustworthy packet for this image, and set its MCU id to 0xffff so the
    // decoder emits JPEG headers without consuming the damaged payload. The
    // normal decoder then fills missing MCU ranges with blank blocks.
    auto makeHeaderOnlyPacket=[&](const ImagePacket &source)->ImagePacket{
        ImagePacket bootstrap=source;
        QByteArray bytes=source.bytes;
        if(shortPacketFormat){
            if(bytes.size()!=SSDV_PKT_SIZE_DSLWP||bytes.size()<9)return ImagePacket{};
            bytes[1]=char(0); bytes[2]=char(0); // packet ID = 0
            bytes[6]=char(0);                    // first MCU offset
            bytes[7]=char(0xff); bytes[8]=char(0xff); // no MCU in bootstrap
        }else{
            if(bytes.size()!=SSDV_PKT_SIZE||bytes.size()<15)return ImagePacket{};
            bytes[0]=char(0x55);                  // standard SSDV sync byte
            bytes[7]=char(0); bytes[8]=char(0);  // packet ID = 0
            bytes[12]=char(0);                   // first MCU offset
            bytes[13]=char(0xff); bytes[14]=char(0xff); // no MCU in bootstrap
        }
        bootstrap.bytes=bytes;
        bootstrap.packetId=0;
        bootstrap.status=QStringLiteral("synthetic_header_only");
        return bootstrap;
    };

    // Keep dimensions available even when every received packet failed CRC.
    const ImagePacket *metadataSource=nullptr;
    if(!ordered.empty())metadataSource=&ordered.begin()->second;
    else if(observed.find(0)!=observed.end())metadataSource=&observed.find(0)->second;
    else metadataSource=&observed.begin()->second;
    decoded.width=metadataSource->width;
    decoded.height=metadataSource->height;
    decoded.receivedPackets=int(ordered.size());
    for(const auto &entry:ordered){
        const auto &packet=entry.second;
        if(packet.width>0)decoded.width=packet.width;
        if(packet.height>0)decoded.height=packet.height;
        if(packet.status==QStringLiteral("ssdv_crc32_pass")||
           packet.status==QStringLiteral("ssdv_crc_pass"))++decoded.crcPassedPackets;
    }

    std::map<int,ImagePacket> feedOrdered=ordered;
    bool syntheticBootstrap=false;
    if(feedOrdered.find(0)==feedOrdered.end()){
        ImagePacket bootstrap=makeHeaderOnlyPacket(*metadataSource);
        if(bootstrap.bytes.isEmpty()){
            decoded.error=QStringLiteral("无法从候选分片读取 SSDV 图像头，不能生成部分预览。");
            return decoded;
        }
        feedOrdered.emplace(0,std::move(bootstrap));
        syntheticBootstrap=true;
    }

    std::vector<std::uint8_t> jpegBuffer(32U*1024U*1024U);
    // Retry after dropping a packet that the JPEG state machine cannot consume.
    // If the actual packet zero fails, replace it with a header-only bootstrap
    // once and continue; do not lose the image header along with its damaged MCU.
    int restartBudget=int(feedOrdered.size())+2;
    bool replacedPacketZero=false;
    while(!feedOrdered.empty()&&restartBudget-->=0){
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
        for(const auto &entry:feedOrdered){
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
            if(!(syntheticBootstrap&&entry.first==0)){
                ssdv_packet_info_t info{};
                ssdv_dec_header(&info,bytes,mode);
                decoded.complete=decoded.complete||info.eoi!=0;
            }
            if(result==SSDV_OK)break;
        }
        if(rejectedId>=0){
            if(rejectedId==0&&syntheticBootstrap){
                decoded.error=QStringLiteral("无法从候选分片建立 SSDV JPEG 图像头，无法生成部分预览。");
                return decoded;
            }
            if(rejectedId==0&&!syntheticBootstrap&&!replacedPacketZero){
                const auto zero=feedOrdered.find(0);
                const ImagePacket *source=zero==feedOrdered.end()?metadataSource:&zero->second;
                ImagePacket bootstrap=makeHeaderOnlyPacket(*source);
                if(!bootstrap.bytes.isEmpty()){
                    if(!decoded.badPacketIds.contains(0))decoded.badPacketIds.push_back(0);
                    const auto acceptedZero=ordered.find(0);
                    if(acceptedZero!=ordered.end()&&
                       (acceptedZero->second.status==QStringLiteral("ssdv_crc32_pass")||
                        acceptedZero->second.status==QStringLiteral("ssdv_crc_pass")))
                        decoded.crcPassedPackets=qMax(0,decoded.crcPassedPackets-1);
                    feedOrdered[0]=std::move(bootstrap);
                    ordered.erase(0);
                    decoded.receivedPackets=int(ordered.size());
                    syntheticBootstrap=true;
                    replacedPacketZero=true;
                    decoded.complete=false;
                    continue;
                }
            }
            if(!decoded.badPacketIds.contains(rejectedId))decoded.badPacketIds.push_back(rejectedId);
            feedOrdered.erase(rejectedId);
            const auto accepted=ordered.find(rejectedId);
            if(accepted!=ordered.end()&&
               (accepted->second.status==QStringLiteral("ssdv_crc32_pass")||
                accepted->second.status==QStringLiteral("ssdv_crc_pass")))
                decoded.crcPassedPackets=qMax(0,decoded.crcPassedPackets-1);
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
            decoded.error=QStringLiteral("已接收 %1 个有效分片，正在使用缺块填充策略，但当前数据仍不足以生成 JPEG。")
                .arg(qulonglong(ordered.size()));
            return decoded;
        }
        decoded.jpeg=QByteArray(reinterpret_cast<const char*>(jpeg),int(jpegLength));
        decoded.receivedPackets=int(ordered.size());
        std::sort(decoded.badPacketIds.begin(),decoded.badPacketIds.end());
        return decoded;
    }
    decoded.error=QStringLiteral("有效图像头或分片不足，无法重建 JPEG。");
    std::sort(decoded.badPacketIds.begin(),decoded.badPacketIds.end());
    return decoded;
}
}
