#include "core.h"
#include "schema.h"
#include "ssdv.h"
#include "transport.h"
#include <QtTest/QtTest>
#include <QFile>
#include <QTemporaryDir>
#include <QtEndian>
#include <bit>
using namespace orbit;
namespace {
QByteArray syntheticFrame(bool escaping=false) {
    QByteArray packet;
    packet.append(char(0x08));packet.append(char(0xf0));
    packet.append(char(0xfb));packet.append(char(0xa1));
    packet.append(char(0x00));packet.append(char(0xbf));
    QByteArray payload(192,char(0));
    const quint32 one=qToBigEndian(std::bit_cast<quint32>(1.0f));
    memcpy(payload.data()+140,&one,4);
    if(escaping)payload[30]=char(0xc0);
    packet+=payload;
    QByteArray encoded;
    for(char c:packet){
        if(quint8(c)==0xc0){encoded.append(char(0xdb));encoded.append(char(0xdc));}
        else if(quint8(c)==0xdb){encoded.append(char(0xdb));encoded.append(char(0xdd));}
        else encoded.append(c);
    }
    QByteArray frame;
    frame.append(QByteArray::fromHex("205491bf00"));
    frame.append(char(0xc0));frame+=encoded;frame.append(char(0xc0));
    frame.append(QByteArray(223-frame.size(),char(0xaa)));
    return frame;
}
}
class ProtocolTest:public QObject {
    Q_OBJECT
private slots:
    void parseKnownStructure() {
        Frame f=Protocol::parse(syntheticFrame(),"ASRTU-1");
        QVERIFY2(f.complete,qPrintable(f.error));
        QCOMPARE(f.vcid,2);QCOMPARE(f.master,0x91);
        QCOMPARE(f.apid,0xf0);QCOMPARE(f.sequence,0x3ba1);
        QCOMPARE(f.payload.size(),192);
        QVERIFY(f.fields.size() >= 10);
        bool hasCandidate=false;
        for(const auto &x:f.fields)
            if(x.name=="candidate.quaternion.q0" && x.status=="candidate_unverified")
                hasCandidate=true;
        QVERIFY(hasCandidate);
    }
    void kissEscapeBeforeLength() {
        Frame f=Protocol::parse(syntheticFrame(true),"ASRTU-1");
        QVERIFY2(f.complete,qPrintable(f.error));
        QCOMPARE(quint8(f.payload[30]),quint8(0xc0));
    }
    void corruptFrameIsNotPromoted() {
        QByteArray raw=syntheticFrame();raw[5]=char(0x42);
        Frame f=Protocol::parse(raw,"ASRTU-1");
        QVERIFY(f.complete); // the 5-byte TM frame is still valid
        QCOMPARE(f.apid,-1);  // but the KISS subpacket preview is unavailable
        QCOMPARE(f.quality,QString("structural_only"));
    }
    void duplicateFusionAndNoFakeUtc(){
        Frame log=Protocol::parse(syntheticFrame(),"ASRTU-1");
        log.quality="log_fec_reported";log.sources<<"log:primary";
        log.captureLocal=QDateTime::fromString("2026-10-24 23:06:02.776","yyyy-MM-dd HH:mm:ss.zzz");
        log.timeBasis="logger_local_unverified";
        Frame bin=Protocol::parse(syntheticFrame(),"ASRTU-1");bin.sources<<"bin:block:0";
        Stats s;
        auto frames=Protocol::fuse({log},{bin},&s);
        QCOMPARE(frames.size(),1);
        QCOMPARE(s.duplicates,1);
        QCOMPARE(s.trustedFromLog,1);
        QCOMPARE(frames[0].sources.size(),2);
        const auto obj=Protocol::toJson(frames[0]);
        QVERIFY(obj.value("spacecraft_utc").isNull());
    }
    void persistAndExport(){
        QTemporaryDir temp;QVERIFY(temp.isValid());
        Frame f=Protocol::parse(syntheticFrame(),"ASRTU-1");
        f.sources<<"bin:block:0";
        Database db;QString error;
        QVERIFY2(db.open(temp.filePath("db.sqlite"),&error),qPrintable(error));
        QVERIFY2(db.save("test",{f},&error),qPrintable(error));
        const auto sessions=db.sessions(&error);
        QCOMPARE(sessions.size(),1);
        const auto restored=db.load(sessions[0].first,&error);
        QCOMPARE(restored.size(),1);
        QCOMPARE(restored[0].hash,f.hash);
        QCOMPARE(restored[0].fields.size(),f.fields.size());
        QVERIFY2(Protocol::exportCsv(temp.filePath("samples.csv"),{f},&error),qPrintable(error));
        QVERIFY2(Protocol::exportJson(temp.filePath("samples.json"),{f},&error),qPrintable(error));
        f.quality="log_fec_reported";
        QVERIFY2(Protocol::exportAttitudeCsv(temp.filePath("attitude.csv"),{f},&error),qPrintable(error));
        QFile qcsv(temp.filePath("attitude.csv"));
        QVERIFY(qcsv.open(QIODevice::ReadOnly));
        const auto lines=qcsv.readAll();
        QVERIFY(lines.contains("candidate_unverified"));
        QVERIFY(lines.contains("q0,q1,q2,q3,norm"));
        QFile csv(temp.filePath("samples.csv"));QVERIFY(csv.open(QIODevice::ReadOnly));
        QVERIFY(csv.readAll().contains("candidate.quaternion.q0"));
        QFile json(temp.filePath("samples.json"));QVERIFY(json.open(QIODevice::ReadOnly));
        QVERIFY(json.readAll().contains("candidate_unverified"));
    }
    void externalSchemaIsSatelliteIsolated(){
        QTemporaryDir temp;QVERIFY(temp.isValid());
        QFile file(temp.filePath("fields.json"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(R"({"satellite":"BY04","revision":"v1","fields":[{"name":"raw.test","apid":"0xF0","offset":30,"type":"u8","status":"schema_declared_unverified"}]})");
        file.close();
        Schema schema;QString error;
        QVERIFY2(schema.load(file.fileName(),&error),qPrintable(error));
        Frame by04=Protocol::parse(syntheticFrame(),"BY04");
        QCOMPARE(schema.apply(by04),1);
        QCOMPARE(by04.fields.back().status,QString("schema_declared_unverified"));
        Frame asrtu=Protocol::parse(syntheticFrame(),"ASRTU-1");
        QCOMPARE(schema.apply(asrtu),0);
    }
    void ssdvOnlyWhenExplicitApidAndConsecutivePackets(){
        QByteArray bytes(256,char(0));
        bytes[0]=char(0x55);bytes[1]=char(0x67);
        bytes[6]=char(2);bytes[7]=char(0);bytes[8]=char(1);
        bytes[9]=char(20);bytes[10]=char(15);
        Frame a,b;
        a.apid=0x321;a.sequence=11;a.complete=true;a.quality="log_fec_reported";
        a.payload=bytes.left(128);
        b.apid=a.apid;b.sequence=12;b.complete=true;b.quality=a.quality;
        b.payload=bytes.mid(128);
        ApplicationPacket pa,pb;
        pa.apid=0x321;pa.sequence=11;pa.fecEvidence="log_fec_reported";
        pa.classification="ccsds_space_packet_candidate";
        pa.data=bytes.left(128);
        pb=pa;pb.sequence=12;pb.data=bytes.mid(128);
        const auto found=ImageTools::candidatesFromKiss({pa,pb},0x321);
        QCOMPARE(found.size(),1);
        QCOMPARE(found[0].packetId,1);
        QCOMPARE(found[0].width,320);
        QCOMPARE(ImageTools::candidatesFromKiss({pa,pb},0x320).size(),0);
        b.sequence=14;
        pb.sequence=14;
        QCOMPARE(ImageTools::candidatesFromKiss({pa,pb},0x321).size(),0);
    }

    void physicalHeaderIsFiveBytes(){
        const QByteArray info=syntheticFrame();
        const auto data=Transport::frameData(info);
        QCOMPARE(data.size(),218);
        QCOMPARE(quint8(data[0]),quint8(0xc0));
        QCOMPARE(quint8(info[4]),quint8(0x00)); // first header pointer, not length
        Frame f=Protocol::parse(info,"ASRTU-1");
        QVERIFY(f.complete);
        QCOMPARE(f.quality,QString("structural_only"));
    }
    void variableLengthPacketsInOneTmFrame(){
        QByteArray raw(223,char(0xc0));
        raw.replace(0,5,QByteArray::fromHex("2054010100"));
        int offset=5;
        for(const QByteArray &p:{QByteArray::fromHex("011122"),
                                 QByteArray::fromHex("02AABBCCDD"),
                                 QByteArray::fromHex("03")}) {
            raw[offset++]=char(0xc0);
            for(char c:p)raw[offset++]=c;
            raw[offset++]=char(0xc0);
        }
        Frame f=Protocol::parse(raw,"ASRTU-1");
        QVERIFY(f.complete);
        const auto packets=Transport::reassemble({f});
        QCOMPARE(packets.size(),3);
        QCOMPARE(packets[0].bytes,QByteArray::fromHex("011122"));
        QCOMPARE(packets[1].bytes,QByteArray::fromHex("02AABBCCDD"));
        QCOMPARE(packets[2].bytes,QByteArray::fromHex("03"));
        QCOMPARE(packets[0].crcStatus,QString("not_checked"));
    }
    void crossTmFragmentAndKissEscape(){
        QByteArray rawA(223,char(0x31));
        QByteArray rawB(223,char(0xc0));
        rawA.replace(0,5,QByteArray::fromHex("2054010100"));
        rawB.replace(0,5,QByteArray::fromHex("2054020200"));
        rawA[5]=char(0xc0);
        rawA[222]=char(0xdb); // FESC at end of the first 218-octet block
        rawB[5]=char(0xdc); // TFEND arrives in the next TM frame
        rawB[6]=char(0xdb);rawB[7]=char(0xdd); // escaped FESC
        rawB[8]=char(0x42);rawB[9]=char(0xc0);
        Frame a=Protocol::parse(rawA,"ASRTU-1");
        Frame b=Protocol::parse(rawB,"ASRTU-1");
        QVERIFY(a.complete && b.complete);
        const auto packets=Transport::reassemble({a,b});
        QCOMPARE(packets.size(),1);
        QCOMPARE(packets[0].bytes.size(),219);
        QCOMPARE(quint8(packets[0].bytes[216]),quint8(0xc0));
        QCOMPARE(quint8(packets[0].bytes[217]),quint8(0xdb));
        QCOMPARE(quint8(packets[0].bytes[218]),quint8(0x42));
        QCOMPARE(packets[0].contributingFrames,2);
    }
    void gapMustNotJoinUnrelatedFrames(){
        QByteArray rawA(223,char(0x31));
        QByteArray rawB(223,char(0xc0));
        rawA.replace(0,5,QByteArray::fromHex("2054010100"));
        rawB.replace(0,5,QByteArray::fromHex("2054020400")); // virtual count gap
        rawA[5]=char(0xc0);
        rawB[5]=char(0x42);rawB[6]=char(0xc0);
        QCOMPARE(Transport::reassemble({Protocol::parse(rawA,"ASRTU-1"),
                                       Protocol::parse(rawB,"ASRTU-1")}).size(),0);
    }
    void configurableCrcDoesNotGuessPolynomial(){
        const QByteArray message("123456789");
        const QByteArray bytes=message+QByteArray::fromHex("29B1");
        CrcConfig crc; // disabled by default even for checksum-bearing data
        QCOMPARE(Transport::checkCrc(bytes,crc),QString("not_checked"));
        crc.enabled=true;
        QCOMPARE(Transport::checkCrc(bytes,crc),QString("pass"));
        QByteArray corrupted=bytes;corrupted[2]=char('X');
        QCOMPARE(Transport::checkCrc(corrupted,crc),QString("fail"));
    }
    void by04DoesNotMislabelAttitude(){
        Frame f=Protocol::parse(syntheticFrame(),"BY04");
        QVERIFY(f.complete);
        QVERIFY(f.fields.size()>=4);
        for(const auto &x:f.fields)QVERIFY(!x.name.startsWith("candidate.quaternion"));
    }
};
QTEST_GUILESS_MAIN(ProtocolTest)
#include "protocol.moc"
