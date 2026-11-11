// SPDX-License-Identifier: MIT
#pragma once

#include <QObject>
#include <QVariantList>
#include <QtQmlIntegration/qqmlintegration.h>

namespace pou {

class Opener;

// What the Help page links to: the wiki, and the wiki's pages for each part of
// the app. They open only when they're asked for, through the opener.
class Help : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The app has the one.")

    // Each with title, what and page, the pages the app has.
    Q_PROPERTY(QVariantList topics READ topics CONSTANT)

public:
    // The opener has to outlive it, and may be null, and then it opens nothing.
    // The trade pages are left out when the app has no trade searches.
    Help(Opener *opener, bool tradeOffered, QObject *parent = nullptr);

    QVariantList topics() const;

    Q_INVOKABLE void openWiki();
    // By its place in topics().
    Q_INVOKABLE void openTopic(int index);

private:
    struct Topic
    {
        QString title;
        QString what;
        QString page; // in the wiki's address
    };

    void open(const QString &url);

    Opener *m_opener;
    QList<Topic> m_topics;
};

} // namespace pou
