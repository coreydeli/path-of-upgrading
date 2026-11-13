// SPDX-License-Identifier: MIT
//
// Only what's kept of a copy. The clipboard itself isn't touched.
#include "platform/systemclipboard.h"

#include <QMimeData>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

// As a browser copies a link.
std::unique_ptr<QMimeData> link()
{
    auto data = std::make_unique<QMimeData>();
    data->setText(u"Path of Upgrading"_s);
    data->setHtml(u"<a href=\"https://example.com\">Path of Upgrading</a>"_s);
    data->setData(u"text/x-moz-url"_s, "https://example.com");
    return data;
}

} // namespace

class TestSystemClipboard : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void keepsEveryFormat()
    {
        const std::unique_ptr<QMimeData> copied = link();
        const std::unique_ptr<QMimeData> kept = copyOf(copied.get());
        QVERIFY(kept);
        QCOMPARE(kept->formats(), copied->formats());
        for (const QString &format : copied->formats())
            QCOMPARE(kept->data(format), copied->data(format));
    }

    void leavesOutEmptyFormats()
    {
        const std::unique_ptr<QMimeData> copied = link();
        copied->setData(u"application/x-empty"_s, {});
        const std::unique_ptr<QMimeData> kept = copyOf(copied.get());
        QVERIFY(kept);
        QVERIFY(!kept->hasFormat(u"application/x-empty"_s));
        QCOMPARE(kept->text(), u"Path of Upgrading"_s);
    }

    void nothingCopied()
    {
        QVERIFY(!copyOf(nullptr));
        const QMimeData empty;
        QVERIFY(!copyOf(&empty));
        QMimeData blank;
        blank.setText({});
        QVERIFY(!copyOf(&blank));
    }

    void tooLarge()
    {
        const std::unique_ptr<QMimeData> copied = link();
        qsizetype size = 0;
        for (const QString &format : copied->formats())
            size += copied->data(format).size();
        QVERIFY(copyOf(copied.get(), size));
        QVERIFY(!copyOf(copied.get(), size - 1));
    }

    void tooSlow()
    {
        const std::unique_ptr<QMimeData> copied = link();
        QVERIFY(!copyOf(copied.get(), 1024, QDeadlineTimer(0)));
    }
};

QTEST_GUILESS_MAIN(TestSystemClipboard)
#include "tst_systemclipboard.moc"
