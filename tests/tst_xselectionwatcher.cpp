// SPDX-License-Identifier: MIT
//
// With the X server in $DISPLAY, which on a Wayland desktop is Xwayland. It's
// skipped when there's none. The copies go to a selection of the test's own,
// which leaves the clipboard alone.
#include "platform/xcb/xselectionwatcher.h"

#include <QSignalSpy>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

constexpr QByteArrayView testSelection = "PATH_OF_UPGRADING_TEST_SELECTION";
constexpr QByteArrayView otherSelection = "PATH_OF_UPGRADING_TEST_SELECTION_2";

// Another program that copies text, on a connection of its own.
class Owner
{
public:
    explicit Owner(QByteArrayView selection = testSelection)
        : m_selection(selection)
    {
        if (!m_x.isValid())
            return;
        m_window = xcb_generate_id(m_x.xcb());
        xcb_create_window(m_x.xcb(), XCB_COPY_FROM_PARENT, m_window, m_x.root(), 0, 0, 1, 1, 0,
                          XCB_WINDOW_CLASS_INPUT_ONLY, XCB_COPY_FROM_PARENT, 0, nullptr);
        m_notifier.setSocket(xcb_get_file_descriptor(m_x.xcb()));
        QObject::connect(&m_notifier, &QSocketNotifier::activated, [this] { answerRequests(); });
        m_notifier.setEnabled(true);
    }

    // Takes the selection, to give out this text or none when it's null.
    void copy(const QByteArray &text)
    {
        m_text = text;
        xcb_set_selection_owner(m_x.xcb(), m_window, m_x.atom(m_selection), XCB_CURRENT_TIME);
        xcb_flush(m_x.xcb());
    }

    // Has the X server close the connection of the client that made that
    // window.
    void closeConnection(xcb_window_t window)
    {
        xcb_kill_client(m_x.xcb(), window);
        xcb_flush(m_x.xcb());
    }

    int requests = 0;
    // the window the last one asking for the text gave
    xcb_window_t requestor = XCB_WINDOW_NONE;

private:
    void answerRequests()
    {
        while (const XReply<xcb_generic_event_t> event{xcb_poll_for_event(m_x.xcb())}) {
            if ((event->response_type & 0x7f) == XCB_SELECTION_REQUEST)
                answer(reinterpret_cast<const xcb_selection_request_event_t &>(*event));
        }
    }

    void answer(const xcb_selection_request_event_t &request)
    {
        ++requests;
        requestor = request.requestor;
        // events go out as 32 bytes, whatever their size
        union {
            xcb_selection_notify_event_t notify;
            char bytes[32];
        } event = {};
        event.notify = {.response_type = XCB_SELECTION_NOTIFY,
                        .time = request.time,
                        .requestor = request.requestor,
                        .selection = request.selection,
                        .target = request.target,
                        .property = XCB_ATOM_NONE};
        if (!m_text.isNull() && request.target == m_x.atom("UTF8_STRING")) {
            xcb_change_property(m_x.xcb(), XCB_PROP_MODE_REPLACE, request.requestor, request.property,
                                request.target, 8, m_text.size(), m_text.constData());
            event.notify.property = request.property;
        }
        xcb_send_event(m_x.xcb(), false, request.requestor, XCB_EVENT_MASK_NO_EVENT,
                       reinterpret_cast<const char *>(&event));
        xcb_flush(m_x.xcb());
    }

    XConnection m_x;
    QByteArrayView m_selection;
    xcb_window_t m_window = XCB_WINDOW_NONE;
    QByteArray m_text;
    QSocketNotifier m_notifier{QSocketNotifier::Read};
};

} // namespace

class TestXSelectionWatcher : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        if (!m_watcher.isValid())
            QSKIP(qPrintable(m_watcher.errorString()));
    }

    void copiedText()
    {
        QSignalSpy copied(&m_watcher, &XSelectionWatcher::textCopied);
        // from a game client that isn't in English
        const QString text = u"Gegenstandsklasse: Ringe\nSeltenheit: Selten\nGrübelnde Schlinge\nGoldring"_s;
        m_owner.copy(text.toUtf8());
        QTRY_COMPARE(copied.size(), 1);
        QCOMPARE(copied.at(0).at(0).toString(), text);
    }

    void sameTextAgain()
    {
        QSignalSpy copied(&m_watcher, &XSelectionWatcher::textCopied);
        m_owner.copy("Rarity: Rare");
        QTRY_COMPARE(copied.size(), 1);
        m_owner.copy("Rarity: Rare");
        QTRY_COMPARE(copied.size(), 2);
    }

    void ownerWithoutText_data()
    {
        QTest::addColumn<QByteArray>("text");
        QTest::newRow("none") << QByteArray();
        QTest::newRow("empty") << QByteArray("");
    }

    void ownerWithoutText()
    {
        QFETCH(QByteArray, text);
        QSignalSpy copied(&m_watcher, &XSelectionWatcher::textCopied);
        const int requests = m_owner.requests;
        m_owner.copy(text);
        QTRY_COMPARE(m_owner.requests, requests + 1);
        // the watcher hears it turned down before it hears of the next copy
        m_owner.copy("Rarity: Magic");
        QTRY_COMPARE(copied.size(), 1);
        QCOMPARE(copied.at(0).at(0).toString(), u"Rarity: Magic"_s);
    }

    void serverGone()
    {
        // The server closes the connection, as when Xwayland goes. Another
        // client can have it closed, by naming the watcher's window.
        XSelectionWatcher watcher{otherSelection};
        Owner owner{otherSelection};
        QSignalSpy copied(&watcher, &XSelectionWatcher::textCopied);
        QSignalSpy disconnected(&watcher, &XSelectionWatcher::disconnected);
        owner.copy("Rarity: Unique");
        QTRY_COMPARE(copied.size(), 1);
        QVERIFY(watcher.isValid());

        owner.closeConnection(owner.requestor);
        QTRY_COMPARE(disconnected.size(), 1);
        QVERIFY(!watcher.isValid());
        QCOMPARE(watcher.errorString(), u"Lost the connection to the X server."_s);
    }

private:
    XSelectionWatcher m_watcher{testSelection};
    Owner m_owner;
};

QTEST_GUILESS_MAIN(TestXSelectionWatcher)
#include "tst_xselectionwatcher.moc"
