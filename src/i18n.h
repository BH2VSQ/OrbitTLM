#pragma once
#include <QJsonObject>
#include <QStringList>

// Replace source spans once. Translated Japanese may itself contain source
// characters, so replacement output must never be passed through the map again.
QString orbitTranslateSource(const QString &source, const QJsonObject &translations,
                             const QStringList &keys, bool matchFormattedText=true);
