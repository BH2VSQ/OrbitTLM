#include "i18n.h"
#include <QHash>
#include <QSet>
#include <QRegularExpression>

QString orbitTranslateSource(const QString &source, const QJsonObject &translations,
                             const QStringList &keys, bool matchFormattedText){
    const auto exact=translations.constFind(source);
    if(exact!=translations.constEnd())return exact.value().toString(source);
    if(matchFormattedText){
        static const QRegularExpression placeholder(QStringLiteral("%L?(\\d+)"));
        static QHash<QString,QRegularExpression> patterns;
        for(const QString &key:keys){
            if(!key.contains(placeholder))continue;
            if(!patterns.contains(key)){
                QString pattern=QStringLiteral("\\A");
                qsizetype cursor=0;
                QSet<QString> seen;
                auto tokens=placeholder.globalMatch(key);
                while(tokens.hasNext()){
                    const auto token=tokens.next();
                    pattern+=QRegularExpression::escape(key.mid(cursor,token.capturedStart()-cursor));
                    const QString group=QStringLiteral("p")+token.captured(1);
                    pattern+=seen.contains(group)?QStringLiteral("\\k<")+group+QStringLiteral(">"):
                        QStringLiteral("(?<")+group+QStringLiteral(">.*?)");
                    seen.insert(group);
                    cursor=token.capturedEnd();
                }
                pattern+=QRegularExpression::escape(key.mid(cursor))+QStringLiteral("\\z");
                patterns.insert(key,QRegularExpression(pattern,QRegularExpression::DotMatchesEverythingOption));
            }
            const auto match=patterns.value(key).match(source);
            if(!match.hasMatch())continue;
            const QString target=translations.value(key).toString(key);
            QString formatted;
            qsizetype cursor=0;
            auto tokens=placeholder.globalMatch(target);
            while(tokens.hasNext()){
                const auto token=tokens.next();
                formatted+=target.mid(cursor,token.capturedStart()-cursor);
                const QString value=match.captured(QStringLiteral("p")+token.captured(1));
                // Only canonical display labels are localized. Unmapped values,
                // IDs, byte strings, units and numbers are copied verbatim.
                formatted+=translations.value(value).toString(value);
                cursor=token.capturedEnd();
            }
            return formatted+target.mid(cursor);
        }
    }
    QString result;
    qsizetype cursor=0;
    while(cursor<source.size()){
        qsizetype next=source.size();
        QString matched;
        for(const QString &key:keys){
            if(key.isEmpty())continue;
            const qsizetype position=source.indexOf(key,cursor);
            if(position>=0&&(position<next||(position==next&&key.size()>matched.size()))){
                next=position;
                matched=key;
            }
        }
        result+=source.mid(cursor,next-cursor);
        if(matched.isEmpty())break;
        result+=translations.value(matched).toString(matched);
        cursor=next+matched.size();
    }
    return result;
}
