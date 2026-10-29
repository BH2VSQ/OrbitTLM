#include "telemetry_store.h"

#include <QCryptographicHash>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

class SatelliteTelemetryStoreTests final : public QObject {
    Q_OBJECT
private:
    static orbit::Frame makeFrame(const QString &satellite, char fill, const QString &timestamp,
                                  int master) {
        orbit::Frame frame;
        frame.bytes = QByteArray(223, fill);
        frame.hash = QString::fromLatin1(
            QCryptographicHash::hash(frame.bytes, QCryptographicHash::Sha256).toHex());
        frame.satellite = satellite;
        frame.captureLocal = QDateTime::fromString(timestamp, Qt::ISODateWithMs);
        frame.timeBasis = QStringLiteral("receiver_local");
        frame.quality = QStringLiteral("structural_only");
        frame.error = QStringLiteral("test fixture");
        frame.master = master;
        frame.virtualCount = master;
        frame.vcid = 0;
        frame.fields.push_back({QStringLiteral("tm.master_count"), QString::number(master), {},
                                QStringLiteral("verified_protocol")});
        return frame;
    }

private slots:
    void duplicateFramesAreIgnoredPerSatellite() {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        orbit::SatelliteTelemetryStore store(temp.path());
        const auto asrtu = makeFrame(QStringLiteral("ASRTU-1"), '\x11',
                                     QStringLiteral("2026-10-29T12:00:00.000"), 1);
        bool inserted = false;
        QString error;
        QVERIFY2(store.append(asrtu, &inserted, &error), qPrintable(error));
        QVERIFY(inserted);
        QVERIFY2(store.append(asrtu, &inserted, &error), qPrintable(error));
        QVERIFY(!inserted);
        auto sameBytesDifferentReportedHash = asrtu;
        sameBytesDifferentReportedHash.hash = QStringLiteral("not-the-raw-frame-hash");
        QVERIFY2(store.append(sameBytesDifferentReportedHash, &inserted, &error), qPrintable(error));
        QVERIFY(!inserted);

        auto by04 = asrtu;
        by04.satellite = QStringLiteral("BY04");
        QVERIFY2(store.append(by04, &inserted, &error), qPrintable(error));
        QVERIFY(inserted); // The same byte sequence is deduplicated independently per satellite.
        QCOMPARE(store.load(QStringLiteral("ASRTU-1"), &error).size(), 1);
        QCOMPARE(store.load(QStringLiteral("BY04"), &error).size(), 1);
        QVERIFY(QFileInfo::exists(store.databasePath(QStringLiteral("ASRTU-1"))));
        QVERIFY(QFileInfo::exists(store.databasePath(QStringLiteral("BY04"))));
        QVERIFY(!QFileInfo::exists(temp.filePath(QStringLiteral("telemetry.json"))));
        QVERIFY(!QFileInfo::exists(temp.filePath(QStringLiteral("telemetry.csv"))));
    }

    void framesLoadInTimestampOrder() {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        orbit::SatelliteTelemetryStore store(temp.path());
        QString error;
        bool inserted = false;
        QVERIFY2(store.append(makeFrame(QStringLiteral("ASRTU-1"), '\x22',
                                        QStringLiteral("2026-10-29T12:02:00.000"), 2), &inserted, &error),
                 qPrintable(error));
        QVERIFY(inserted);
        QVERIFY2(store.append(makeFrame(QStringLiteral("ASRTU-1"), '\x33',
                                        QStringLiteral("2026-10-29T12:01:00.000"), 1), &inserted, &error),
                 qPrintable(error));
        QVERIFY(inserted);
        const auto loaded = store.load(QStringLiteral("ASRTU-1"), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(loaded.size(), 2);
        QCOMPARE(loaded[0].master, 1);
        QCOMPARE(loaded[1].master, 2);
    }

    void allSatelliteFramesAreGloballyOrdered() {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        orbit::SatelliteTelemetryStore store(temp.path());
        QString error;
        bool inserted = false;
        QVERIFY2(store.append(makeFrame(QStringLiteral("ASRTU-1"), '\x66',
                                        QStringLiteral("2026-10-29T12:02:00.000"), 2), &inserted, &error),
                 qPrintable(error));
        QVERIFY(inserted);
        QVERIFY2(store.append(makeFrame(QStringLiteral("BY04"), '\x77',
                                        QStringLiteral("2026-10-29T12:01:00.000"), 1), &inserted, &error),
                 qPrintable(error));
        QVERIFY(inserted);
        const auto loaded = store.loadAll(&error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(loaded.size(), 2);
        QCOMPARE(loaded[0].satellite, QStringLiteral("BY04"));
        QCOMPARE(loaded[1].satellite, QStringLiteral("ASRTU-1"));
    }

    void timestampsWithDifferentOffsetsAreOrderedByInstant() {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        orbit::SatelliteTelemetryStore store(temp.path());
        QString error;
        bool inserted = false;
        // 12:00 at UTC+08:00 is 04:00 UTC, earlier than 05:00 UTC.
        QVERIFY2(store.append(makeFrame(QStringLiteral("BY04"), '\x44',
                                        QStringLiteral("2026-10-29T05:00:00.000Z"), 2), &inserted, &error),
                 qPrintable(error));
        QVERIFY(inserted);
        QVERIFY2(store.append(makeFrame(QStringLiteral("BY04"), '\x55',
                                        QStringLiteral("2026-10-29T12:00:00.000+08:00"), 1), &inserted, &error),
                 qPrintable(error));
        QVERIFY(inserted);
        const auto loaded = store.load(QStringLiteral("BY04"), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(loaded.size(), 2);
        QCOMPARE(loaded[0].master, 1);
        QCOMPARE(loaded[1].master, 2);
    }
};

QTEST_GUILESS_MAIN(SatelliteTelemetryStoreTests)
#include "telemetry_store.moc"
