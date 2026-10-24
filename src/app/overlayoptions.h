// SPDX-License-Identifier: MIT
#pragma once

#include "core/settings.h"

#include <QKeySequence>
#include <QObject>
#include <QString>
#include <QtQmlIntegration/qqmlintegration.h>

namespace pou {

class Hotkeys;
class SettingsStore;

// The popup's settings and the hotkeys, as the settings window shows them,
// saved as soon as they're changed. The position goes by its id, "cursor".
class OverlayOptions : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The app has the one.")

    Q_PROPERTY(QKeySequence checkKey READ checkKey NOTIFY changed)
    Q_PROPERTY(QKeySequence resultsKey READ resultsKey NOTIFY changed)
    Q_PROPERTY(QString hotkeysError READ hotkeysError NOTIFY changed)
    Q_PROPERTY(bool advancedCopy READ isAdvancedCopy WRITE setAdvancedCopy NOTIFY changed)
    Q_PROPERTY(bool watchClipboard READ isWatchingClipboard WRITE setWatchClipboard NOTIFY changed)
    Q_PROPERTY(QString position READ position WRITE setPosition NOTIFY changed)
    Q_PROPERTY(double scale READ scale WRITE setScale NOTIFY changed)
    Q_PROPERTY(bool compact READ isCompact WRITE setCompact NOTIFY changed)
    Q_PROPERTY(bool colorblind READ isColorblind WRITE setColorblind NOTIFY changed)
    Q_PROPERTY(int autoHideSeconds READ autoHideSeconds WRITE setAutoHideSeconds NOTIFY changed)
    Q_PROPERTY(bool paused READ isPaused WRITE setPaused NOTIFY changed)

public:
    // They have to outlive it.
    OverlayOptions(SettingsStore *settings, Hotkeys *hotkeys, QObject *parent = nullptr);

    // None when it has no key.
    QKeySequence checkKey() const;
    QKeySequence resultsKey() const;
    // Whether it has the key now. When another program or the other hotkey
    // has it, it keeps the one it had.
    Q_INVOKABLE bool setCheckKey(const QKeySequence &key);
    Q_INVOKABLE bool setResultsKey(const QKeySequence &key);
    // Why the hotkeys don't work, or empty when they do.
    QString hotkeysError() const;

    bool isAdvancedCopy() const { return overlay().advancedCopy; }
    void setAdvancedCopy(bool advanced);
    bool isWatchingClipboard() const { return overlay().watchClipboard; }
    void setWatchClipboard(bool watch);
    QString position() const;
    void setPosition(const QString &position);
    // From half the size up to twice it.
    double scale() const { return overlay().scale; }
    void setScale(double scale);
    bool isCompact() const { return overlay().compact; }
    void setCompact(bool compact);
    bool isColorblind() const { return overlay().colorblind; }
    void setColorblind(bool colorblind);
    // From 2 up to 120.
    int autoHideSeconds() const { return overlay().autoHideSeconds; }
    void setAutoHideSeconds(int seconds);
    bool isPaused() const { return overlay().paused; }
    void setPaused(bool paused);

Q_SIGNALS:
    // For any of them, as when a hotkey is changed in System Settings.
    void changed();

private:
    const OverlaySettings &overlay() const;
    template<typename Edit>
    void change(Edit edit);

    SettingsStore *m_settings;
    Hotkeys *m_hotkeys;
};

} // namespace pou
