#include "core.h"
#include "schema.h"
#include "transport.h"
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
using namespace orbit;
int main(int argc,char **argv){
    QCoreApplication app(argc,argv);
    QCoreApplication::setApplicationName("OrbitTLM-cli");
    QCommandLineParser args;
    args.setApplicationDescription("Satellite telemetry batch analysis; all data quality flags retained");
    args.addHelpOption();
    args.addOptions({
        QCommandLineOption({"l","log"},"FEC LOG path","file"),
        QCommandLineOption({"b","bin"},"223-byte BIN path","file"),
        QCommandLineOption({"s","satellite"},"ASRTU-1 or BY04 (8104)","name","ASRTU-1"),
        QCommandLineOption({"p","profile"},"Optional engineering rules JSON","file"),
        QCommandLineOption("json","Write full ordered JSON","file"),
        QCommandLineOption("packets-json","Write reassembled variable-length KISS packets with CRC status","file"),
        QCommandLineOption("crc-profile","Optional CRC profile: none, ccitt-false, x25, crc32-iso; default NONE","profile","none"),
        QCommandLineOption("crc-skip","Skip N prefix octets when computing CRC (default 0)","count","0"),
        QCommandLineOption("csv","Write full ordered CSV","file"),
        QCommandLineOption("quaternion-csv","Write unverified quaternion candidate time-series","file"),
        QCommandLineOption("sqlite","Persist one session to SQLite","file")
    });
    args.process(app);
    QTextStream out(stdout),err(stderr);
    const QString sat=args.value("satellite");
    if(sat!="ASRTU-1"&&sat!="BY04"){
        err<<"ERROR: satellite must be ASRTU-1 or BY04\n";return 2;
    }
    if(!args.isSet("log")&&!args.isSet("bin")){
        err<<"ERROR: pass --log and/or --bin\n";return 2;
    }
    QString reason;
    const auto logs=args.isSet("log")?Protocol::fromLog(args.value("log"),sat,&reason):QVector<Frame>{};
    if(!reason.isEmpty()){err<<"LOG: "<<reason<<"\n";return 3;}
    const auto bins=args.isSet("bin")?Protocol::fromBin(args.value("bin"),sat,&reason):QVector<Frame>{};
    if(!reason.isEmpty()){err<<"BIN: "<<reason<<"\n";return 3;}
    Stats stats;
    auto frames=Protocol::fuse(logs,bins,&stats);
    if(args.isSet("profile")){
        Schema schema;
        if(!schema.load(args.value("profile"),&reason)){
            err<<"Schema: "<<reason<<"\n";return 4;
        }
        if(schema.satellite()!=sat){
            err<<"Schema satellite mismatch\n";return 4;
        }
        for(auto &f:frames)schema.apply(f);
    }
    CrcConfig crc;
    const QString profile=args.value("crc-profile").toLower();
    if(profile!="none"){
        crc.enabled=true;
        if(profile=="ccitt-false"){
            crc.width=16;crc.polynomial=0x1021;crc.initial=0xffff;
        }else if(profile=="x25"){
            crc.width=16;crc.polynomial=0x1021;crc.initial=0xffff;
            crc.reflectInput=true;crc.reflectOutput=true;
            crc.finalXor=0xffff;crc.trailerLittleEndian=true;
        }else if(profile=="crc32-iso"){
            crc.width=32;crc.polynomial=0x04c11db7;crc.initial=0xffffffff;
            crc.reflectInput=true;crc.reflectOutput=true;
            crc.finalXor=0xffffffff;crc.trailerLittleEndian=true;
        }else {
            err<<"Unsupported CRC preset. Configure only a confirmed mission CRC.\n";
            return 7;
        }
        bool ok=false;
        crc.skipLeadingBytes=args.value("crc-skip").toInt(&ok);
        if(!ok||crc.skipLeadingBytes<0){err<<"Invalid --crc-skip\n";return 7;}
    }
    const auto packets=Transport::reassemble(frames,crc);
    if(args.isSet("packets-json")&&
       !Transport::exportJson(args.value("packets-json"),packets,&reason)){
        err<<"KISS packets: "<<reason<<"\n";return 6;
    }
    if(args.isSet("sqlite")){
        Database db;
        if(!db.open(args.value("sqlite"),&reason)||
           !db.save(sat+" batch import",frames,&reason)){
            err<<"SQLite: "<<reason<<"\n";return 5;
        }
    }
    if(args.isSet("csv")&&!Protocol::exportCsv(args.value("csv"),frames,&reason)){
        err<<"CSV: "<<reason<<"\n";return 6;
    }
    if(args.isSet("json")&&!Protocol::exportJson(args.value("json"),frames,&reason)){
        err<<"JSON: "<<reason<<"\n";return 6;
    }
    if(args.isSet("quaternion-csv")&&
       !Protocol::exportAttitudeCsv(args.value("quaternion-csv"),frames,&reason)){
        err<<"Quaternion: "<<reason<<"\n";return 6;
    }
    const QJsonObject result{
        {"satellite",sat},{"log_observations",stats.logObservations},
        {"bin_blocks",stats.binBlocks},{"unique_frames",stats.unique},
        {"log_fec_reported",stats.trustedFromLog},
        {"structural_only",stats.structuralOnly},
        {"invalid",stats.invalid},{"duplicate_observations",stats.duplicates},
        {"application_kiss_packets",packets.size()},
        {"crc_profile",profile},
        {"note","223 bytes = 5-byte short TM header + 218-byte KISS transport region; CRC not assumed unless --crc-profile is set"}
    };
    out<<QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Indented));
    return 0;
}
