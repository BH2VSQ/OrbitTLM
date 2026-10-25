#pragma once
#include "core.h"
#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QVector>

namespace orbit {
struct CrcConfig {
    bool enabled = false;
    int width = 16;                 // supports 16 and 32 bits
    quint32 polynomial = 0x1021;    // direct, non-reflected polynomial
    quint32 initial = 0xffff;
    quint32 finalXor = 0;
    bool reflectInput = false;
    bool reflectOutput = false;
    bool trailerLittleEndian = false;
    int skipLeadingBytes = 0;       // checksum coverage starts after this prefix
};
struct ApplicationPacket {
    QByteArray bytes;               // entire decoded KISS content, opaque by default
    QByteArray data;                // optional CCSDS Space Packet user bytes
    QString satellite;
    QStringList sources;
    QDateTime captureLocal;         // no spacecraft UTC implied
    QString timestampBasis = "unknown";
    QString crcStatus = "not_checked";
    QString classification = "opaque_variable_length";
    QString diagnostic;
    int vcid = -1;
    int firstMaster = -1;
    int lastMaster = -1;
    int apid = -1;                  // populated only if optional 6-byte header validates
    int sequence = -1;
    int contributingFrames = 0;
};
class Transport {
public:
    // Each input is an RS-decoded 223-byte TM *information* frame.
    // KISS decoding happens on the 218-byte TM data zone, across VC frame boundaries.
    // Input must be ordered by on-air frame sequence. No synthetic timestamps.
    static QVector<ApplicationPacket> reassemble(const QVector<Frame> &frames,
                                                const CrcConfig &crc = {});
    static QString checkCrc(const QByteArray &kissPacket,const CrcConfig &config);
    static QByteArray frameData(const QByteArray &informationFrame);
    static bool exportJson(const QString &path,const QVector<ApplicationPacket> &packets,
                           QString *error=nullptr);
};
}
