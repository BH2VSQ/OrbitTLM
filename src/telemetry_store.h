#pragma once

#include "core.h"
#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>

namespace orbit {

// Persistent, per-spacecraft telemetry storage. The raw 223-byte frame is the
// source of truth; JSON/CSV are not used as a persistence format.
class SatelliteTelemetryStore final {
public:
    explicit SatelliteTelemetryStore(const QString &rootDirectory = QString());
    ~SatelliteTelemetryStore();

    SatelliteTelemetryStore(const SatelliteTelemetryStore &) = delete;
    SatelliteTelemetryStore &operator=(const SatelliteTelemetryStore &) = delete;

    // Returns false on database errors. `inserted` is false for a duplicate
    // frame (same satellite and SHA-256 hash) and true for a newly stored row.
    bool append(const Frame &frame, bool *inserted = nullptr, QString *error = nullptr);
    QVector<Frame> load(const QString &satellite, QString *error = nullptr);
    QVector<Frame> loadAll(QString *error = nullptr);
    QString databasePath(const QString &satellite) const;
    QString rootDirectory() const { return rootDirectory_; }

private:
    QString normalizedSatellite(const QString &satellite) const;
    QSqlDatabase connectionFor(const QString &satellite, bool create, QString *error);
    bool ensureSchema(const QString &satellite, QSqlDatabase &db, QString *error);

    QString rootDirectory_;
    QString connectionPrefix_;
    QHash<QString, QString> connectionNames_;
    QSet<QString> initialized_;
};

} // namespace orbit
