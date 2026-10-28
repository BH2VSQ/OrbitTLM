#pragma once
#include "business.h"
#include "ssdv.h"
namespace orbit {
// BY04/BY70-4 uses the same 5+218 short transfer frames as ASRTU-1,
// but the spacecraft ID, application field map, and VC1 image packets differ.
// Header-only output is NEVER equivalent to decoded engineering telemetry.
class By04Decoder {
public:
    static constexpr int SpacecraftId = 0x205;
    static int spacecraftId(const QByteArray &tm);
    static BusinessPacket decode(const ApplicationPacket &packet);
    static QVector<BusinessPacket> decodeAll(const QVector<ApplicationPacket> &packets);
    // VC1 carries raw 218-byte DSLWP-style SSDV image packets, NOT KISS.
    static QVector<ImagePacket> images(const QVector<Frame> &frames);
    static QVector<BusinessPacket> definitionPackets();
};
}
