#pragma once
#include "core.h"
#include <QVector>
namespace orbit {
struct ImagePacket {
    QByteArray bytes;
    int apid=-1, imageId=-1, packetId=-1, width=-1, height=-1;
    QString status="candidate_unverified";
};
// Scans a single selected APID data stream. Recognizing an SSDV-like header
// is not an image checksum or RS-validity proof.
class ImageTools {
public:
    static QVector<ImagePacket> candidates(const QVector<Frame> &frames,int apid);
    static bool savePackets(const QVector<ImagePacket> &packets,const QString &filename,QString *error=nullptr);
};
}
