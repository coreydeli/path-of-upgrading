// SPDX-License-Identifier: MIT
import QtQuick
import org.kde.kirigami as Kirigami

// A list of stats as chips, as many to a line as fit.
Item {
    id: chips

    property list<string> stats
    // all of them as one line, for screen readers
    property string text: stats.join(", ")

    // the widest chip, so how wide it is doesn't hang on how it wraps
    implicitWidth: {
        let widest = 0;
        for (let i = 0; i < shown.count; ++i)
            widest = Math.max(widest, shown.itemAt(i)?.implicitWidth ?? 0);
        return widest;
    }
    implicitHeight: flow.implicitHeight

    Accessible.role: Accessible.StaticText
    Accessible.name: text

    Flow {
        id: flow

        width: chips.width
        spacing: Kirigami.Units.smallSpacing

        Repeater {
            id: shown

            model: chips.stats

            delegate: StatChip {
                required property string modelData

                objectName: "statChip"
                text: modelData
                // nothing to go by until it's laid out
                maxWidth: chips.width > 0 ? chips.width : Infinity
                Accessible.ignored: true
            }
        }
    }
}
