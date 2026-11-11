// SPDX-License-Identifier: MIT
#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QtQmlIntegration/qqmlintegration.h>

namespace pou {

class Opener;
class PobOptions;

// What the About page says: which version this is, what it runs on, where
// Path of Building is, who made what it uses, and the links to the project.
// Links open only when they're asked for, through the opener.
class About : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The app has the one.")

    // "0.7.0", from the application's version
    Q_PROPERTY(QString version READ version CONSTANT)
    // "Beta" or "Release"
    Q_PROPERTY(QString edition READ edition CONSTANT)
    Q_PROPERTY(QString license READ license CONSTANT)
    Q_PROPERTY(QString qtVersion READ qtVersion CONSTANT)
    Q_PROPERTY(QString frameworksVersion READ frameworksVersion CONSTANT)
    // Where Path of Building is, or that none was found.
    Q_PROPERTY(QString pobStatus READ pobStatus NOTIFY pobStatusChanged)
    // Each with name, what, license and url.
    Q_PROPERTY(QVariantList credits READ credits CONSTANT)

public:
    // Both have to outlive it. The opener may be null, and then it opens nothing.
    About(PobOptions *pob, Opener *opener, QObject *parent = nullptr);

    QString version() const;
    QString edition() const;
    QString license() const;
    QString qtVersion() const;
    QString frameworksVersion() const;
    QString pobStatus() const;
    QVariantList credits() const;

    Q_INVOKABLE void openRepository();
    Q_INVOKABLE void openReleases();
    Q_INVOKABLE void openIssues();
    // The link of a credit, by its place in credits().
    Q_INVOKABLE void openCredit(int index);

Q_SIGNALS:
    void pobStatusChanged();

private:
    void open(const QString &url);

    PobOptions *m_pob;
    Opener *m_opener;
};

} // namespace pou
