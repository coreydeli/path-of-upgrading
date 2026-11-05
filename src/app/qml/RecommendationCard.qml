// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound

import QtQuick
import org.kde.kirigamiaddons.formcard as FormCard

// The slots to upgrade first, each with what to look for and a trade search.
// It's a card of its own to put on any page.
FormCard.FormCard {
    id: card

    required property RecommendedUpgrades upgrades

    objectName: "recommendedUpgrades"
    visible: repeater.count > 0

    Repeater {
        id: repeater

        model: card.upgrades

        delegate: RecommendationRow {
            required property int index

            tradeAvailable: card.upgrades.tradeAvailable
            first: index === 0
            onFindAsked: card.upgrades.find(index)
            onShowResultsAsked: card.upgrades.showResults(index)
        }
    }
}
