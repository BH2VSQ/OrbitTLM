#include "core.h"
#include "schema.h"
#include "ssdv.h"
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
        QCOMPARE(f.payload.size(),192);QCOMPARE(f.fields.size(),8);
        QCOMPARE(f.fields.at(3).status,QString("candidate_unverified"));
    }
    void kissEscapeBeforeLength() {
        Frame f=Protocol::parse(syntheticFrame(true),"ASRTU-1");
        QVERIFY2(f.complete,qPrintable(f.error));
        QCOMPARE(quint8(f.payload[30]),quint8(0xc0));
    }
    void corruptFrameIsNotPromoted() {
        QByteArray raw=syntheticFrame();raw[5]=char(0x42);
        Frame f=Protocol::parse(raw,"ASRTU-1");
        QVERIFY(!f.complete);
        QCOMPARE(f.quality,QString("invalid"));
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
        QVERIFY2(Protocol::exportCsv(temp.filePath("samples.csv"),{f},&error),qPrintable(error));
        QVERIFY2(Protocol::exportJson(temp.filePath("samples.json"),{f},&error),qPrintable(error));
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
        const auto found=ImageTools::candidates({a,b},0x321);
        QCOMPARE(found.size(),1);
        QCOMPARE(found[0].packetId,1);
        QCOMPARE(found[0].width,320);
        QCOMPARE(ImageTools::candidates({a,b},0x320).size(),0);
        b.sequence=14;
        QCOMPARE(ImageTools::candidates({a,b},0x321).size(),0);
    }
    void by04DoesNotMislabelAttitude(){
        Frame f=Protocol::parse(syntheticFrame(),"BY04");
        QVERIFY(f.complete);
        QCOMPARE(f.fields.size(),3);
    }
};
QTEST_GUILESS_MAIN(ProtocolTest)
#include "protocol.moc"
