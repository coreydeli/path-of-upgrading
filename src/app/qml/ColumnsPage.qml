// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

// A page of form cards that makes use of a wide window. The cards flow into
// as many columns as fit, up to three, and each grows to fill its column.
// A narrow window has the one column. A card stays with its header, and
// with the SectionText after it, so the page is written as a plain list of
// them, as on a FormCardPage.
FormCard.FormCardPage {
    id: page

    // the least a column gets, which decides how many fit
    property real minColumnWidth: Kirigami.Units.gridUnit * 26
    property int maxColumns: 3
    // the most, so lines of text stay short enough to read
    property real maxColumnWidth: Kirigami.Units.gridUnit * 40
    // the space around a card when there are columns
    readonly property real gutter: Kirigami.Units.largeSpacing
    readonly property int columnCount: Math.max(1, Math.min(maxColumns, Math.floor(width / minColumnWidth)))

    // the headers with their cards, each as { items, cards }
    property var sections: []
    // the column each of them is in now, so they only move when that changes
    property string placed
    // how many columns have something in them, as a page of one card has one
    // column however wide the window is
    property int used: 1

    // What a card's width is held to, which is what makes it a card with
    // space round it. One column goes edge to edge until it's wide enough
    // that FormCard would put it in the middle, as on any other page.
    function limitWidth(item: Item): real {
        const room = item.width - 2 * gutter;
        return columnCount > 1 ? room : Math.max(Kirigami.Units.gridUnit * 30, room);
    }

    // Sorts the cards and headers on the page into sections, each starting at
    // a header, or at a card that has no header to follow.
    function collect(): void {
        const found = [];
        let current = null;
        for (let i = 0; i < cards.length; ++i) {
            const o = cards[i];
            // not an item: a dialog or such
            if (o === columns || o.childrenRect === undefined)
                continue;
            const isHeader = o.title !== undefined;
            const isText = !isHeader && o.wrapMode !== undefined;
            if (!current || isHeader || (!isText && current.cards > 0)) {
                current = {
                    items: [],
                    cards: 0
                };
                found.push(current);
            }
            current.items.push(o);
            if (!isHeader && !isText)
                current.cards++;
            if (o.maximumWidth !== undefined)
                o.maximumWidth = Qt.binding(() => page.limitWidth(o));
            o.visibleChanged.connect(page.arrange);
            o.implicitHeightChanged.connect(page.arrange);
        }
        sections = found;
    }

    function heightOf(section): real {
        let height = 0;
        for (const item of section.items) {
            if (item.visible)
                height += item.implicitHeight;
        }
        return height;
    }

    // Puts each section in the shortest column, in the order they come, so
    // the page reads down the first column and then the next.
    function arrange(): void {
        Qt.callLater(place);
    }

    function place(): void {
        const heights = new Array(columnCount).fill(0);
        const plan = [];
        for (const section of sections) {
            let shortest = 0;
            for (let c = 1; c < columnCount; ++c) {
                if (heights[c] < heights[shortest])
                    shortest = c;
            }
            plan.push(shortest);
            heights[shortest] += heightOf(section);
        }
        used = Math.max(1, Math.min(columnCount, Math.max(0, ...plan) + 1));
        if (plan.join() === placed)
            return;
        placed = plan.join();
        const where = [column0, column1, column2];
        // an item keeps its place in the column it's already in, so take them
        // all out to put them back in the order they were written in
        for (const section of sections) {
            for (const item of section.items)
                item.parent = null;
        }
        for (let s = 0; s < sections.length; ++s) {
            for (const item of sections[s].items)
                item.parent = where[plan[s]];
        }
    }

    // Not through arrange(), so the columns always fit the width. Kirigami 6.13
    // (Debian 13) gives the page its width a moment after the window shows,
    // and a placing put off till then left the old columns for that moment.
    onColumnCountChanged: place()
    Component.onCompleted: {
        collect();
        place();
    }

    component PageColumn: ColumnLayout {
        required property int index

        // an equal share each
        Layout.fillWidth: true
        Layout.preferredWidth: 1
        Layout.alignment: Qt.AlignTop
        visible: index < page.used
        spacing: 0
    }

    RowLayout {
        id: columns

        Layout.fillWidth: true
        Layout.maximumWidth: page.used * page.maxColumnWidth
        Layout.alignment: Qt.AlignHCenter
        spacing: 0

        PageColumn {
            id: column0
            index: 0
        }

        PageColumn {
            id: column1
            index: 1
        }

        PageColumn {
            id: column2
            index: 2
        }
    }
}
