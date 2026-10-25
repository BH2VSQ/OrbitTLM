#include "core.h"
#include "schema.h"
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
        {"note","Recorder clock is not spacecraft UTC; unknown time stays unknown"}
    };
    out<<QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Indented));
    return 0;
}
