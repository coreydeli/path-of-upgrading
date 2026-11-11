// SPDX-License-Identifier: MIT
#include "app/fonts.h"
#include "app/itemreport.h"
#include "stylefortests.h"

#include <QAccessible>
#include <QColor>
#include <QFont>
#include <QFontDatabase>
#include <QFontInfo>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTest>
#include <cmath>
#include <memory>

Q_IMPORT_QML_PLUGIN(PathOfUpgradingPlugin)

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

// Tone::Good, Bad and Warning
constexpr int plain = 0;
constexpr int good = 1;
constexpr int bad = 2;
constexpr int warning = 3;

// An item with the functions of Style, which are given the Kirigami.Theme
// of the item they color.
const char *probe = "import QtQuick\n"
                    "import org.kde.kirigami as Kirigami\n"
                    "import PathOfUpgrading\n"
                    "Item {\n"
                    "    function rarity(name) { return Style.rarity(name, Kirigami.Theme); }\n"
                    "    function grade(name) { return Style.grade(name, Kirigami.Theme); }\n"
                    "    function tone(tone, blind) { return Style.tone(tone, blind, Kirigami.Theme); }\n"
                    "    function frame() { return Style.frame(Kirigami.Theme); }\n"
                    "    function inner() { return Style.inner(Kirigami.Theme); }\n"
                    "    function title(font) { return Style.titleFont(font); }\n"
                    "    function label(font) { return Style.labelFont(font); }\n"
                    "    function figure(font) { return Style.figureFont(font); }\n"
                    "    function step(grade) { return Style.gradeStep(grade); }\n"
                    "    function clear(c) { return Style.clear(c); }\n"
                    "    function bigTitle(font) { return Style.titleFont(font, 2); }\n"
                    "    function section(font) { return Style.sectionFont(font); }\n"
                    "    readonly property color text: Kirigami.Theme.textColor\n"
                    "    readonly property color background: Kirigami.Theme.backgroundColor\n"
                    "    readonly property color highlight: Kirigami.Theme.highlightColor\n"
                    "    readonly property color positive: Kirigami.Theme.positiveTextColor\n"
                    "    readonly property color negative: Kirigami.Theme.negativeTextColor\n"
                    "    readonly property color neutral: Kirigami.Theme.neutralTextColor\n"
                    "    readonly property color disabled: Kirigami.Theme.disabledTextColor\n"
                    "}\n";

QColor hex(const char *name)
{
    return QColor(QLatin1StringView(name));
}

} // namespace

class TestStyle : public QObject
{
    Q_OBJECT

public:
    static void initMain()
    {
        qputenv("QT_QPA_PLATFORM", "offscreen");
        // the app's own controls, as the program has
        useStyleInTests();
        qputenv("QT_NO_XDG_DESKTOP_PORTAL", "1");
    }

private Q_SLOTS:
    void init() { QTest::failOnWarning(); }

    void initTestCase()
    {
        QQmlComponent component(&m_engine);
        component.setData(probe, QUrl());
        m_probe.reset(qobject_cast<QQuickItem *>(component.create()));
        QVERIFY2(m_probe, qPrintable(component.errorString()));
        m_probe->setParentItem(m_window.contentItem());
        m_style = m_engine.singletonInstance<QObject *>("PathOfUpgrading", "Style");
        QVERIFY(m_style);
    }

    void cleanup()
    {
        m_style->setProperty("plain", false);
        m_style->setProperty("appColors", false);
        m_style->setProperty("headingFamily", u"Big Shoulders Display"_s);
    }

    void rarityColorsAreTheGames()
    {
        QCOMPARE(rarity(u"Normal"_s), hex("#c8c8c8"));
        QCOMPARE(rarity(u"Magic"_s), hex("#8888ff"));
        QCOMPARE(rarity(u"Rare"_s), hex("#ffff77"));
        QCOMPARE(rarity(u"Unique"_s), hex("#af6025"));
        QCOMPARE(rarity(u"Currency"_s), hex("#aa9e82"));
        QCOMPARE(rarity(u"Gem"_s), hex("#1ba29b"));
    }

    void rarityIsTheSameWhicheverColorsAreOn()
    {
        m_style->setProperty("appColors", true);
        QCOMPARE(rarity(u"Rare"_s), hex("#ffff77"));
        QCOMPARE(rarity(u"Magic"_s), hex("#8888ff"));
    }

    void anUnknownRarityIsText()
    {
        QCOMPARE(rarity(u"Strange"_s), color(u"text"_s));
        QCOMPARE(rarity(QString()), color(u"text"_s));
    }

    void gradesTakeTheSchemesColors()
    {
        QCOMPARE(grade(u"S"_s), color(u"highlight"_s));
        QCOMPARE(grade(u"A"_s), color(u"positive"_s));
        QCOMPARE(grade(u"B"_s), color(u"text"_s));
        QCOMPARE(grade(u"C"_s), color(u"neutral"_s));
        QCOMPARE(grade(u"D"_s), color(u"disabled"_s));
        QCOMPARE(grade(QString()), color(u"text"_s));
    }

    void gradesClimbFiveSteps()
    {
        QCOMPARE(step(u"D"_s), 1);
        QCOMPARE(step(u"C"_s), 2);
        QCOMPARE(step(u"B"_s), 3);
        QCOMPARE(step(u"A"_s), 4);
        QCOMPARE(step(u"S"_s), 5);
        QCOMPARE(step(QString()), 0);
    }

    void tonesFollowTheTheme()
    {
        QCOMPARE(tone(plain, false), color(u"text"_s));
        QCOMPARE(tone(good, false), color(u"positive"_s));
        QCOMPARE(tone(bad, false), color(u"negative"_s));
        QCOMPARE(tone(warning, false), color(u"neutral"_s));
    }

    void tonesHaveBlueAndOrangeForColorblind()
    {
        QCOMPARE(tone(good, true), hex("#3daee9"));
        QCOMPARE(tone(bad, true), hex("#f67400"));
        QCOMPARE(tone(warning, true), hex("#fdbc4b"));
        QCOMPARE(tone(plain, true), color(u"text"_s));
    }

    void framesAreBetweenTheBackgroundAndTheText()
    {
        const QColor frame = call(u"frame"_s).value<QColor>();
        const QColor inner = call(u"inner"_s).value<QColor>();
        const QColor background = color(u"background"_s);
        const QColor text = color(u"text"_s);
        // whichever way round the scheme is, they step from the background toward the text
        const auto away = [&](const QColor &c) {
            return std::abs(c.lightnessF() - background.lightnessF());
        };
        QVERIFY(away(inner) > 0);
        QVERIFY(away(inner) < away(frame));
        QVERIFY(away(frame) < away(text));
    }

    void titlesAreInCapitalsInTheHeadingFamily()
    {
        QVERIFY(addBundledFonts());
        QFont base;
        base.setPointSizeF(10);
        const QFont font = call(u"title"_s, {base}).value<QFont>();
        QCOMPARE(font.family(), headingFontFamily());
        QCOMPARE(font.weight(), QFont::ExtraBold);
        QCOMPARE(font.capitalization(), QFont::AllUppercase);
        QCOMPARE(font.pointSizeF(), 14.0);
        m_style->setProperty("headingFamily", u"Made Up Serif"_s);
        QCOMPARE(call(u"title"_s, {base}).value<QFont>().family(), u"Made Up Serif"_s);
    }

    void labelsAreSmallCapitalsInTheTextFace()
    {
        QFont base;
        base.setPointSizeF(10);
        const QFont font = call(u"label"_s, {base}).value<QFont>();
        QCOMPARE(font.family(), u"Big Shoulders Text"_s);
        QCOMPARE(font.weight(), QFont::Bold);
        QCOMPARE(font.capitalization(), QFont::AllUppercase);
        QCOMPARE(call(u"section"_s, {base}).value<QFont>().family(), u"Big Shoulders Text"_s);
        QVERIFY(call(u"section"_s, {base}).value<QFont>().pointSizeF() > font.pointSizeF());
    }

    void figuresAreSetBigAndBlack()
    {
        QFont base;
        base.setPointSizeF(10);
        const QFont font = call(u"figure"_s, {base}).value<QFont>();
        QCOMPARE(font.family(), u"Big Shoulders Display"_s);
        QCOMPARE(font.weight(), QFont::Black);
        QCOMPARE(font.pointSizeF(), 15.0);
    }

    void theFacesAreInTheProgram()
    {
        QVERIFY(addBundledFonts());
        const QStringList families = QFontDatabase::families();
        for (const QString &family : {u"Big Shoulders Display"_s, u"Big Shoulders Text"_s, u"Spectral"_s})
            QVERIFY2(families.contains(family), qPrintable(family));
        QCOMPARE(QFontInfo(QFont(u"Big Shoulders Display"_s)).family(), u"Big Shoulders Display"_s);
    }

    void clearKeepsTheColorAndLosesItsAlpha()
    {
        const QColor from = hex("#d4b06a");
        const QColor out = call(u"clear"_s, {from}).value<QColor>();
        QCOMPARE(out.alpha(), 0);
        QCOMPARE(out.red(), from.red());
        QCOMPARE(out.green(), from.green());
        QCOMPARE(out.blue(), from.blue());
    }

    void titlesScaleTheFontTheyAreGiven()
    {
        QFont base;
        base.setPointSizeF(10);
        QCOMPARE(call(u"bigTitle"_s, {base}).value<QFont>().pointSizeF(), 29.0);
    }

    void decorationsAreOnUnlessPlain() { QCOMPARE(m_style->property("plain").toBool(), false); }

    void aGaugeFillsToItsScoreInTheColorOfItsGrade()
    {
        std::unique_ptr<QQuickItem> gauge = make("ScoreGauge { score: \"72\"; grade: \"A\" }");
        QVERIFY(gauge);
        QCOMPARE(gauge->property("fraction").toDouble(), 0.72);
        QCOMPARE(gauge->property("tint").value<QColor>(), grade(u"A"_s));
        QCOMPARE(gauge->property("text").toString(), u"72 A"_s);
        QVERIFY(gauge->implicitWidth() > 0);
    }

    void aGaugeLightsPipsToItsGrade()
    {
        QCOMPARE(make("ScoreGauge { score: \"72\"; grade: \"A\" }")->property("lit").toInt(), 4);
        QCOMPARE(make("ScoreGauge { score: \"10\"; grade: \"D\" }")->property("lit").toInt(), 1);
        QCOMPARE(make("ScoreGauge { score: \"\"; grade: \"\" }")->property("lit").toInt(), 0);
    }

    void aGaugeStaysWithinTheRing()
    {
        std::unique_ptr<QQuickItem> over = make("ScoreGauge { score: \"140\"; grade: \"S\" }");
        QCOMPARE(over->property("fraction").toDouble(), 1.0);
        std::unique_ptr<QQuickItem> none = make("ScoreGauge { score: \"\"; grade: \"\" }");
        QCOMPARE(none->property("fraction").toDouble(), 0.0);
    }

    void gaugesComeInThreeSizes()
    {
        const auto width = [&](const char *size) {
            return make(QByteArray("ScoreGauge { score: \"50\"; grade: \"C\"; size: \"") + size + "\" }")
                ->implicitWidth();
        };
        QVERIFY(width("small") < width("medium"));
        QVERIFY(width("medium") < width("large"));
    }

    void aGaugeIsReadAsItsScore()
    {
        std::unique_ptr<QQuickItem> gauge = make("ScoreGauge { score: \"72\"; grade: \"A\" }");
        QAccessibleInterface *read = QAccessible::queryAccessibleInterface(gauge.get());
        QVERIFY(read);
        QCOMPARE(read->text(QAccessible::Name), u"Gear score 72, grade A"_s);
    }

    void aPlateIsSquareWhenPlain()
    {
        std::unique_ptr<QQuickItem> plate = make("Plate { width: 100; height: 50 }");
        QCOMPARE(plate->property("notch").toDouble(), 12.0);
        m_style->setProperty("plain", true);
        std::unique_ptr<QQuickItem> square = make("Plate { width: 100; height: 50 }");
        QCOMPARE(square->property("notch").toDouble(), 0.0);
    }

    void aSheetHoldsItsRowsToTheMaximumWidth()
    {
        std::unique_ptr<QQuickItem> sheet = make("Sheet { width: 1000; maximumWidth: 400 }");
        QVERIFY(sheet);
        QCOMPARE(sheet->property("panelWidth").toDouble(), 400.0);
        sheet->setWidth(300);
        const double side = sheet->property("side").toDouble();
        QCOMPARE(sheet->property("panelWidth").toDouble(), 300.0 - 2 * side);
    }

    void aNoticeShowsOneLineEach()
    {
        std::unique_ptr<QQuickItem> none = make("Notice { }");
        QCOMPARE(none->isVisible(), false);
        std::unique_ptr<QQuickItem> two = make("Notice { lines: [\"Loses life\", \"Loses speed\"] }");
        QCOMPARE(two->isVisible(), true);
        QCOMPARE(two->property("tint").value<QColor>(), tone(3, false));
    }

    void aTableMeasuresItsChangesAgainstTheBiggest()
    {
        std::unique_ptr<QQuickItem> table = make("ChangesTable { function size(c) { return sizeOf(c) } }");
        QVERIFY(table);
        QList<ReportFigure> figures;
        for (const QString &change : {u"+1,234%"_s, u"-300"_s, u"at cap"_s}) {
            ReportFigure figure;
            figure.change = change;
            figures << figure;
        }
        table->setProperty("figures", QVariant::fromValue(figures));
        QCOMPARE(table->property("biggest").toDouble(), 1234.0);
        QVariant size;
        QVERIFY(QMetaObject::invokeMethod(table.get(), "size", Q_RETURN_ARG(QVariant, size),
                                          Q_ARG(QVariant, u"-300"_s)));
        QCOMPARE(size.toDouble(), 300.0);
        QVERIFY(QMetaObject::invokeMethod(table.get(), "size", Q_RETURN_ARG(QVariant, size),
                                          Q_ARG(QVariant, u"at cap"_s)));
        QCOMPARE(size.toDouble(), 0.0);
    }

    void aPillIsTintedInTheColorOfItsTone()
    {
        std::unique_ptr<QQuickItem> pill = make("TonePill { text: \"Upgrade\"; tone: 1 }");
        QCOMPARE(pill->property("tint").value<QColor>(), tone(good, false));
        std::unique_ptr<QQuickItem> blind = make("TonePill { text: \"Upgrade\"; tone: 1; colorblind: true }");
        QCOMPARE(blind->property("tint").value<QColor>(), tone(good, true));
        QVERIFY(pill->implicitWidth() > 0);
    }

private:
    std::unique_ptr<QQuickItem> make(const QByteArray &body)
    {
        QQmlComponent component(&m_engine);
        component.setData("import QtQuick\nimport PathOfUpgrading\n" + body + "\n", QUrl());
        std::unique_ptr<QQuickItem> item(qobject_cast<QQuickItem *>(component.create()));
        if (!item)
            qWarning("%s", qPrintable(component.errorString()));
        else
            item->setParentItem(m_window.contentItem());
        return item;
    }

    QVariant call(const QString &name, const QVariantList &args = {}) const
    {
        QVariant result;
        const QByteArray method = name.toLatin1();
        bool ok = false;
        switch (args.size()) {
        case 0:
            ok = QMetaObject::invokeMethod(m_probe.get(), method.constData(), Q_RETURN_ARG(QVariant, result));
            break;
        case 1:
            ok = QMetaObject::invokeMethod(m_probe.get(), method.constData(), Q_RETURN_ARG(QVariant, result),
                                           Q_ARG(QVariant, args.at(0)));
            break;
        default:
            ok = QMetaObject::invokeMethod(m_probe.get(), method.constData(), Q_RETURN_ARG(QVariant, result),
                                           Q_ARG(QVariant, args.at(0)), Q_ARG(QVariant, args.at(1)));
        }
        if (!ok)
            qWarning("Couldn't call %s", method.constData());
        return result;
    }

    QColor rarity(const QString &name) const { return call(u"rarity"_s, {name}).value<QColor>(); }
    QColor grade(const QString &name) const { return call(u"grade"_s, {name}).value<QColor>(); }
    QColor tone(int tone, bool colorblind) const
    {
        return call(u"tone"_s, {tone, colorblind}).value<QColor>();
    }
    int step(const QString &name) const { return call(u"step"_s, {name}).toInt(); }
    QColor color(const QString &property) const
    {
        return m_probe->property(property.toLatin1().constData()).value<QColor>();
    }

    QQmlEngine m_engine;
    QQuickWindow m_window;
    std::unique_ptr<QQuickItem> m_probe;
    QObject *m_style = nullptr;
};

QTEST_MAIN(TestStyle)
#include "tst_style.moc"
