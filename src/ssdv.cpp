#include "ssdv.h"
#include <QFile>
#include <QSaveFile>
namespace orbit {
QVector<ImagePacket> ImageTools::candidates(const QVector<Frame> &frames,int apid){
    QVector<ImagePacket> out;
    QByteArray stream;
    int lastSequence=-1;
    for(const Frame &f:frames){
        if(f.apid!=apid || !f.complete || f.quality!="log_fec_reported")continue;
        if(lastSequence>=0 && (lastSequence+1)%16384 != f.sequence)stream.clear();
        lastSequence=f.sequence;
        stream+=f.payload;
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
}
