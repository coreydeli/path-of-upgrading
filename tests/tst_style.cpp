// SPDX-License-Identifier: MIT
#include "app/fonts.h"

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
        qputenv("QT_QUICK_CONTROLS_STYLE", "Fusion");
        qputenv("QT_NO_XDG_DESKTOP_PORTAL", "1");
        qputenv("QT_LOGGING_RULES", "kf.kirigami.platform.warning=false");
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

    void cleanup() { m_style->setProperty("appColors", false); }

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

    void gradesHaveColorsOfTheirOwn()
    {
        m_style->setProperty("appColors", true);
        QCOMPARE(grade(u"S"_s), hex("#d4b06a"));
        QCOMPARE(grade(u"A"_s), hex("#7fb069"));
        QCOMPARE(grade(u"B"_s), hex("#8fb3c9"));
        QCOMPARE(grade(u"C"_s), hex("#c9b48f"));
        QCOMPARE(grade(u"D"_s), hex("#948776"));
        QCOMPARE(grade(QString()), color(u"text"_s));
    }

    void gradesTakeTheDesktopsColors()
    {
        // nothing gold is left on the desktop's colors
        QCOMPARE(grade(u"S"_s), color(u"highlight"_s));
        QCOMPARE(grade(u"A"_s), color(u"positive"_s));
        QCOMPARE(grade(u"B"_s), color(u"text"_s));
        QCOMPARE(grade(u"C"_s), color(u"neutral"_s));
        QCOMPARE(grade(u"D"_s), color(u"disabled"_s));
        QCOMPARE(grade(QString()), color(u"text"_s));
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

    void framesAreGoldBrownWithTheAppsColors()
    {
        m_style->setProperty("appColors", true);
        QCOMPARE(call(u"frame"_s).value<QColor>(), hex("#5c4a32"));
        QCOMPARE(call(u"inner"_s).value<QColor>(), hex("#2b241c"));
    }

    void framesAreMixedFromTheThemeWithTheDesktops()
    {
        const QColor text = color(u"text"_s);
        const QColor background = color(u"background"_s);
        const auto mixed = [&](double part) {
            return QColor::fromRgbF(background.redF() + (text.redF() - background.redF()) * part,
                                    background.greenF() + (text.greenF() - background.greenF()) * part,
                                    background.blueF() + (text.blueF() - background.blueF()) * part);
        };
        const QColor frame = call(u"frame"_s).value<QColor>();
        const QColor inner = call(u"inner"_s).value<QColor>();
        QCOMPARE(frame.name(), mixed(0.2).name());
        QCOMPARE(inner.name(), mixed(0.1).name());
        QVERIFY(frame != inner);
    }

    void titlesAreInCinzel()
    {
        QVERIFY(addBundledFonts());
        QFont base;
        base.setPointSizeF(11);
        const QFont font = call(u"title"_s, {base}).value<QFont>();
        QCOMPARE(font.family(), headingFontFamily());
        QCOMPARE(font.weight(), QFont::Bold);
        QCOMPARE(font.pointSizeF(), 11.0);
        QCOMPARE(font.letterSpacing(), 0.5);
        QCOMPARE(font.capitalization(), QFont::MixedCase);
    }

    void cinzelIsInTheProgram()
    {
        QVERIFY(addBundledFonts());
        QVERIFY(QFontDatabase::families().contains(u"Cinzel"_s));
        QCOMPARE(QFontInfo(QFont(u"Cinzel"_s)).family(), u"Cinzel"_s);
        // one file for every weight
        QVERIFY(QFontDatabase::isSmoothlyScalable(u"Cinzel"_s, QFontDatabase::styles(u"Cinzel"_s).value(0)));
    }

private:
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
