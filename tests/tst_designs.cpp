// SPDX-License-Identifier: MIT
#include "core/designs.h"

#include <QRegularExpression>
#include <QSet>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

class TestDesigns : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void dossierComesFirstAsTheDefault()
    {
        QCOMPARE(designs().first().design, Design::Dossier);
        QCOMPARE(designFromId(u"dossier"), Design::Dossier);
    }

    void everyLookHasItsOwnIdAndName()
    {
        QSet<QString> ids;
        QSet<QString> names;
        for (const DesignInfo &info : designs()) {
            QVERIFY(!info.id.isEmpty());
            QCOMPARE(info.id, info.id.toLower());
            QVERIFY(!info.name.isEmpty());
            QVERIFY(!info.note.isEmpty());
            QVERIFY2(!ids.contains(info.id), qPrintable(info.id));
            QVERIFY2(!names.contains(info.name), qPrintable(info.name));
            ids.insert(info.id);
            names.insert(info.name);
        }
    }

    void idsRoundTrip()
    {
        for (const DesignInfo &info : designs()) {
            QCOMPARE(designId(info.design), info.id);
            QCOMPARE(designFromId(info.id), info.design);
            QCOMPARE(&designInfo(info.design), &info);
        }
    }

    // the choices saved by earlier versions
    void savedChoicesStillMeanWhatTheyDid()
    {
        QCOMPARE(designFromId(u"dossier"), Design::Dossier);
        QCOMPARE(designFromId(u"material"), Design::Material);
    }

    void anUnknownIdIsDossier()
    {
        QCOMPARE(designFromId(u"neon"), Design::Dossier);
        QCOMPARE(designFromId(u""), Design::Dossier);
    }

    void everyLookShowsFourColorsInThePickers()
    {
        for (const DesignInfo &info : designs()) {
            QCOMPARE(info.swatch.size(), 4);
            for (const QString &color : info.swatch) {
                static const QRegularExpression hex(u"^#[0-9a-f]{6}$"_s);
                QVERIFY2(hex.match(color).hasMatch(), qPrintable(info.id + u" "_s + color));
            }
        }
    }

    void everyLookNamesHowItDrawsTheScore()
    {
        QCOMPARE(designInfo(Design::Dossier).tile, u"diamonds"_s);
        QCOMPARE(designInfo(Design::Material).tile, u"diamonds"_s);
        // the new looks each have their own
        QSet<QString> own;
        for (Design d : {Design::Slate, Design::Pocket, Design::Edge, Design::Gilt, Design::Cinder}) {
            const QString &tile = designInfo(d).tile;
            QVERIFY(!tile.isEmpty());
            QVERIFY2(tile != u"diamonds"_s && !own.contains(tile), qPrintable(tile));
            own.insert(tile);
        }
    }

    // the files are named after it, and Dossier has the one
    void aLookWithALightPaletteHasASchemeName()
    {
        for (const DesignInfo &info : designs()) {
            if (info.hasLight)
                QVERIFY2(!info.schemeName.isEmpty(), qPrintable(info.id));
        }
        QVERIFY(!designInfo(Design::Dossier).hasLight);
        QVERIFY(designInfo(Design::Dossier).schemeName.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestDesigns)
#include "tst_designs.moc"
