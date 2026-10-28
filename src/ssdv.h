#pragma once
#include "core.h"
#include "transport.h"
#include <QByteArray>
#include <QVector>
namespace orbit {
struct ImagePacket {
    QByteArray bytes;
    int apid=-1, imageId=-1, packetId=-1, width=-1, height=-1;
    QString status="candidate_unverified";
};
struct DecodedImage {
    QByteArray jpeg;
    int imageId=-1, width=-1, height=-1;
    int receivedPackets=0, crcPassedPackets=0;
    bool complete=false;
    QString error;
};
// Scans a single selected APID data stream. Recognizing an SSDV-like header
// is not an image checksum or RS-validity proof.
class ImageTools {
public:
    static QVector<ImagePacket> candidates(const QVector<Frame> &frames,int apid);
    static QVector<ImagePacket> candidatesFromKiss(const QVector<ApplicationPacket> &packets,int apid);
    static bool savePackets(const QVector<ImagePacket> &packets,const QString &filename,QString *error=nullptr);
    // Rebuild one image from sorted or unsorted 256-byte SSDV or 218-byte
    // DSLWP packets. The decoder is linked into OrbitTLM, so no subprocess is used.
    static DecodedImage decodeImage(const QVector<ImagePacket> &packets,int imageId,bool dslwp);
};
}
