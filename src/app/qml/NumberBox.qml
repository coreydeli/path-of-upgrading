// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls as QQC2

// A spin box for a number with decimals, which it only shows when the number
// has them: 2, like 2.5 Divine Orbs.
QQC2.SpinBox {
    id: box

    property real number
    property real minimum: 0
    property real maximum: 100000
    property real step: 1
    property int decimals: 0
    // what 0 shows as, when it means there's none
    property string noneText
    // as the spin box counts in whole numbers
    readonly property real factor: Math.pow(10, decimals)

    signal edited(real number)

    from: Math.round(minimum * factor)
    to: Math.round(maximum * factor)
    stepSize: Math.round(step * factor)
    editable: true
    value: Math.round(number * factor)
    textFromValue: (value, locale) => {
        if (value === 0 && box.noneText !== "")
            return box.noneText;
        const text = Number(value / box.factor).toLocaleString(locale, "f", box.decimals);
        // 1.5 rather than 1.50, and 2 rather than 2.00
        return box.decimals > 0 ? text.replace(new RegExp("\\" + locale.decimalPoint + "?0+$"), "") : text;
    }
    valueFromText: (text, locale) => {
        try {
            return Math.round(Number.fromLocaleString(locale, text) * box.factor);
        } catch (error) {
            // the none text
            return 0;
        }
    }
    validator: DoubleValidator {
        bottom: box.minimum
        top: box.maximum
        decimals: box.decimals
        notation: DoubleValidator.StandardNotation
        locale: box.locale.name
    }
    onValueModified: edited(value / factor)
}
