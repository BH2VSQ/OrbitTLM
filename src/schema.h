#pragma once
#include "core.h"
#include <QJsonObject>
#include <QVector>
namespace orbit {
// Versioned schema rules are evaluated against the CCSDS Space Packet data field.
// No implicit fixed offset is accepted for a different satellite or APID.
class Schema {
public:
    bool load(const QString &path,QString *error=nullptr);
    int apply(Frame &frame) const;
    QString satellite() const {return satellite_;}
    QString revision() const {return revision_;}
    QStringList names() const;
private:
    struct Rule {
        QString name,unit,type,status,conditionHex;
        int apid=-1, offset=-1,bit=-1,conditionOffset=-1;
        double scale=1,bias=0;
        QJsonObject enumerations;
    };
    QString satellite_,revision_;
    QVector<Rule> rules_;
};
}
