// SPDX-License-Identifier: MIT
#include "core/pobbuild.h"

#include <QTest>

using namespace pou;
using namespace Qt::Literals::StringLiterals;

namespace {

const QString buildXml = u"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                         u"<PathOfBuilding2>\n"
                         u"\t<Build level=\"92\" className=\"Sorceress\" ascendClassName=\"Stormweaver\"/>\n"
                         u"</PathOfBuilding2>"_s;

// The zlib stream Path of Building puts in its codes. qCompress() writes the
// same with the length in front.
QByteArray zlibData(const QString &text)
{
    return qCompress(text.toUtf8()).sliced(4);
}

QString codeFor(const QByteArray &data)
{
    return QString::fromLatin1(data.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

QString decoded(const QString &code)
{
    return decodeBuildCode(code).value_or(u"(not a code)"_s);
}

} // namespace

class TestPobBuild : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void decodesCodes() { QCOMPARE(decoded(codeFor(zlibData(buildXml))), buildXml); }

    void wrappedOverLines()
    {
        // and in the standard base64 alphabet, with padding
        QString code = QString::fromLatin1(zlibData(buildXml).toBase64());
        for (qsizetype i = 40; i < code.size(); i += 41)
            code.insert(i, u'\n');
        QCOMPARE(decoded(u"  "_s + code + u"\r\n"_s), buildXml);
    }

    void rawDeflate()
    {
        // without zlib's two byte header and the checksum at the end
        QCOMPARE(decoded(codeFor(zlibData(buildXml).sliced(2).chopped(4))), buildXml);
    }

    void plainXml() { QCOMPARE(decoded(u"\n  "_s + buildXml + u"\n"_s), buildXml); }

    void notACode()
    {
        QVERIFY(!decodeBuildCode(u"not a code!!"_s));
        QVERIFY(!decodeBuildCode(u"   "_s));
        // base64, but not deflated
        QVERIFY(!decodeBuildCode(codeFor("Path of Upgrading")));
        // cut short
        const QString code = codeFor(zlibData(buildXml));
        QVERIFY(!decodeBuildCode(code.first(code.size() / 2)));
    }
};

QTEST_GUILESS_MAIN(TestPobBuild)
#include "tst_pobbuild.moc"
