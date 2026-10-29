#include "telemetry_store.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QVariant>
#include <utility>
#include <QUuid>
#include <algorithm>

namespace orbit {

SatelliteTelemetryStore::SatelliteTelemetryStore(const QString &rootDirectory)
    : rootDirectory_(rootDirectory.trimmed()),
      connectionPrefix_(QStringLiteral("OrbitTLM_satellite_")) {
    if (rootDirectory_.isEmpty()) {
        rootDirectory_ = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                         + QStringLiteral("/telemetry");
    }
}

SatelliteTelemetryStore::~SatelliteTelemetryStore() {
    QStringList names;
    names.reserve(connectionNames_.size());
    for (auto it = connectionNames_.cbegin(); it != connectionNames_.cend(); ++it)
        names.push_back(it.value());
    connectionNames_.clear();
    initialized_.clear();
    for (const QString &name : names) {
        {
            QSqlDatabase db = QSqlDatabase::database(name, false);
            if (db.isValid()) db.close();
        }
        QSqlDatabase::removeDatabase(name);
    }
}

QString SatelliteTelemetryStore::normalizedSatellite(const QString &satellite) const {
    QString result = canonicalSatelliteName(satellite);
    if (result.isEmpty() || result == QStringLiteral("AUTO"))
        result = QStringLiteral("UNKNOWN");
    static const QRegularExpression unsafe(QStringLiteral("[^A-Za-z0-9_.-]"));
    result.replace(unsafe, QStringLiteral("_"));
    while (result.contains(QStringLiteral(".."))) result.replace(QStringLiteral(".."), QStringLiteral("."));
    result.remove(QLatin1Char('/'));
    result.remove(QLatin1Char('\\'));
    if (result.isEmpty() || result == QStringLiteral(".")) result = QStringLiteral("UNKNOWN");
    return result;
}

QString SatelliteTelemetryStore::databasePath(const QString &satellite) const {
    return QDir(rootDirectory_).filePath(normalizedSatellite(satellite) + QStringLiteral(".db"));
}

QSqlDatabase SatelliteTelemetryStore::connectionFor(const QString &satellite, bool create,
                                                       QString *error) {
    const QString sat = normalizedSatellite(satellite);
    auto existing = connectionNames_.constFind(sat);
    if (existing != connectionNames_.cend()) {
        QSqlDatabase db = QSqlDatabase::database(existing.value(), false);
        if (!db.isValid() || (!db.isOpen() && !db.open())) {
            if (error) *error = db.lastError().text();
            return {};
        }
        if (!initialized_.contains(sat) && !ensureSchema(sat, db, error)) return {};
        initialized_.insert(sat);
        return db;
    }

    const QString path = databasePath(sat);
    if (!create && !QFileInfo::exists(path)) return {};
    if (!QDir().mkpath(rootDirectory_)) {
        if (error) *error = QStringLiteral("无法创建 SQLite 目录：%1").arg(rootDirectory_);
        return {};
    }

    const QString name = connectionPrefix_ + QUuid::createUuid().toString(QUuid::WithoutBraces);
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
    db.setDatabaseName(path);
    if (!db.open()) {
        if (error) *error = QStringLiteral("无法打开 %1：%2").arg(path, db.lastError().text());
        db = {};
        QSqlDatabase::removeDatabase(name);
        return {};
    }
    connectionNames_.insert(sat, name);
    if (!ensureSchema(sat, db, error)) return {};
    initialized_.insert(sat);
    return db;
}

bool SatelliteTelemetryStore::ensureSchema(const QString &satellite, QSqlDatabase &db,
                                            QString *error) {
    Q_UNUSED(satellite);
    const QStringList statements = {
        QStringLiteral("PRAGMA foreign_keys = ON"),
        QStringLiteral("PRAGMA journal_mode = WAL"),
        QStringLiteral("PRAGMA synchronous = NORMAL"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS frames ("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                       "hash TEXT NOT NULL UNIQUE, satellite TEXT NOT NULL, capture_local TEXT, "
                       "capture_ms INTEGER, time_basis TEXT NOT NULL DEFAULT 'unknown', quality TEXT NOT NULL DEFAULT 'invalid', "
                       "diagnostic TEXT, master INTEGER, virtual_count INTEGER, vcid INTEGER, apid INTEGER, "
                       "sequence INTEGER, sources TEXT, raw BLOB NOT NULL, payload BLOB)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS frame_fields ("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT, frame_id INTEGER NOT NULL "
                       "REFERENCES frames(id) ON DELETE CASCADE, name TEXT NOT NULL, value TEXT, "
                       "unit TEXT, status TEXT)")
    };
    QSqlQuery query(db);
    for (const QString &sql : statements) {
        if (!query.exec(sql)) {
            if (error) *error = query.lastError().text();
            return false;
        }
    }

    // Upgrade databases created by earlier builds that stored only ISO timestamp text.
    bool hasCaptureMs = false;
    if (!query.exec(QStringLiteral("PRAGMA table_info(frames)"))) {
        if (error) *error = query.lastError().text();
        return false;
    }
    while (query.next()) {
        if (query.value(1).toString() == QStringLiteral("capture_ms")) {
            hasCaptureMs = true;
            break;
        }
    }
    query.finish();
    if (!hasCaptureMs && !query.exec(QStringLiteral("ALTER TABLE frames ADD COLUMN capture_ms INTEGER"))) {
        if (error) *error = query.lastError().text();
        return false;
    }

    const QStringList indexes = {
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_frames_capture_ms ON frames(capture_ms, id)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_frame_fields_name ON frame_fields(name)")
    };
    for (const QString &sql : indexes) {
        if (!query.exec(sql)) {
            if (error) *error = query.lastError().text();
            return false;
        }
    }
    return true;
}

bool SatelliteTelemetryStore::append(const Frame &frame, bool *inserted, QString *error) {
    if (inserted) *inserted = false;
    const QString satellite = normalizedSatellite(frame.satellite);
    QSqlDatabase db = connectionFor(satellite, true, error);
    if (!db.isValid() || !db.isOpen()) return false;

    if (frame.bytes.isEmpty()) {
        if (error) *error = QStringLiteral("拒绝保存没有原始帧字节的遥测记录");
        return false;
    }
    // Hash the raw frame bytes rather than trusting a caller-supplied display hash.
    // This makes duplicate detection stable across LOG/BIN and live TCP ingestion.
    const QString hash = QString::fromLatin1(
        QCryptographicHash::hash(frame.bytes, QCryptographicHash::Sha256).toHex());

    if (!db.transaction()) {
        if (error) *error = db.lastError().text();
        return false;
    }
    QSqlQuery insert(db);
    insert.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO frames(hash,satellite,capture_local,capture_ms,time_basis,quality,diagnostic,"
        "master,virtual_count,vcid,apid,sequence,sources,raw,payload) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"));
    insert.addBindValue(hash);
    insert.addBindValue(satellite);
    insert.addBindValue(frame.captureLocal.isValid()
        ? QVariant(frame.captureLocal.toString(Qt::ISODateWithMs)) : QVariant());
    insert.addBindValue(frame.captureLocal.isValid()
        ? QVariant::fromValue<qint64>(frame.captureLocal.toMSecsSinceEpoch()) : QVariant());
    insert.addBindValue(frame.timeBasis);
    insert.addBindValue(frame.quality);
    insert.addBindValue(frame.error);
    insert.addBindValue(frame.master);
    insert.addBindValue(frame.virtualCount);
    insert.addBindValue(frame.vcid);
    insert.addBindValue(frame.apid);
    insert.addBindValue(frame.sequence);
    insert.addBindValue(frame.sources.join(QLatin1Char(';')));
    insert.addBindValue(frame.bytes);
    insert.addBindValue(frame.payload);
    if (!insert.exec()) {
        if (error) *error = insert.lastError().text();
        db.rollback();
        return false;
    }
    if (insert.numRowsAffected() == 0) {
        db.rollback();
        if (inserted) *inserted = false;
        return true; // Same raw frame was already stored in this satellite's database.
    }

    const qint64 frameId = insert.lastInsertId().toLongLong();
    QSqlQuery field(db);
    field.prepare(QStringLiteral("INSERT INTO frame_fields(frame_id,name,value,unit,status) VALUES(?,?,?,?,?)"));
    for (const orbit::Field &value : frame.fields) {
        field.bindValue(0, frameId);
        field.bindValue(1, value.name);
        field.bindValue(2, value.value);
        field.bindValue(3, value.unit);
        field.bindValue(4, value.status);
        if (!field.exec()) {
            if (error) *error = field.lastError().text();
            db.rollback();
            return false;
        }
    }
    if (!db.commit()) {
        if (error) *error = db.lastError().text();
        db.rollback();
        return false;
    }
    if (inserted) *inserted = true;
    return true;
}

QVector<Frame> SatelliteTelemetryStore::load(const QString &satellite, QString *error) {
    QVector<Frame> frames;
    QSqlDatabase db = connectionFor(satellite, false, error);
    if (!db.isValid() || !db.isOpen()) return frames;

    QSqlQuery query(db);
    if (!query.exec(QStringLiteral(
        "SELECT id,hash,satellite,capture_local,capture_ms,time_basis,quality,diagnostic,master,virtual_count,"
        "vcid,apid,sequence,sources,raw,payload FROM frames "
        "ORDER BY CASE WHEN capture_ms IS NULL AND (capture_local IS NULL OR capture_local='') THEN 1 ELSE 0 END, "
        "COALESCE(capture_ms, CAST(strftime('%s', capture_local) AS INTEGER)*1000) ASC, id ASC"))) {
        if (error) *error = query.lastError().text();
        return frames;
    }
    QSqlQuery fields(db);
    fields.prepare(QStringLiteral("SELECT name,value,unit,status FROM frame_fields WHERE frame_id=? ORDER BY id"));
    while (query.next()) {
        Frame frame;
        const qint64 id = query.value(0).toLongLong();
        frame.hash = query.value(1).toString();
        frame.satellite = query.value(2).toString();
        const QString timestamp = query.value(3).toString();
        frame.captureLocal = QDateTime::fromString(timestamp, Qt::ISODateWithMs);
        if (!frame.captureLocal.isValid()) frame.captureLocal = QDateTime::fromString(timestamp, Qt::ISODate);
        frame.timeBasis = query.value(5).toString();
        frame.quality = query.value(6).toString();
        frame.error = query.value(7).toString();
        frame.master = query.value(8).toInt();
        frame.virtualCount = query.value(9).toInt();
        frame.vcid = query.value(10).toInt();
        frame.apid = query.value(11).toInt();
        frame.sequence = query.value(12).toInt();
        frame.sources = query.value(13).toString().split(QLatin1Char(';'), Qt::SkipEmptyParts);
        frame.bytes = query.value(14).toByteArray();
        frame.payload = query.value(15).toByteArray();
        frame.complete = frame.quality != QStringLiteral("invalid");

        fields.bindValue(0, id);
        if (!fields.exec()) {
            if (error) *error = fields.lastError().text();
            return {};
        }
        while (fields.next()) {
            frame.fields.push_back({fields.value(0).toString(), fields.value(1).toString(),
                                    fields.value(2).toString(), fields.value(3).toString()});
        }
        fields.finish();
        frames.push_back(std::move(frame));
    }
    return frames;
}

QVector<Frame> SatelliteTelemetryStore::loadAll(QString *error) {
    QVector<Frame> result;
    QDir root(rootDirectory_);
    if (!root.exists()) return result;
    const QStringList files = root.entryList({QStringLiteral("*.db")}, QDir::Files, QDir::Name);
    for (const QString &file : files) {
        QString currentError;
        const QString sat = QFileInfo(file).completeBaseName();
        const QVector<Frame> part = load(sat, &currentError);
        if (!currentError.isEmpty()) {
            if (error) *error = QStringLiteral("读取 %1 失败：%2").arg(file, currentError);
            return {};
        }
        result += part;
    }
    std::stable_sort(result.begin(), result.end(), [](const Frame &left, const Frame &right) {
        if (left.captureLocal.isValid() != right.captureLocal.isValid()) return left.captureLocal.isValid();
        if (left.captureLocal.isValid() && right.captureLocal.isValid() && left.captureLocal != right.captureLocal)
            return left.captureLocal < right.captureLocal;
        if (left.satellite != right.satellite) return left.satellite < right.satellite;
        return left.hash < right.hash;
    });
    return result;
}

} // namespace orbit
