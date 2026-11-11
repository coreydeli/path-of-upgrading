find_package(ZLIB REQUIRED) # Path of Building codes are deflated

add_library(pou_core STATIC
    autoweights.cpp autoweights.h
    budget.cpp budget.h
    buildimport.cpp buildimport.h
    buildprofiles.cpp buildprofiles.h
    catalog.cpp catalog.h
    character.cpp character.h
    characterstore.cpp characterstore.h
    datafiles.cpp datafiles.h
    evaluation.cpp evaluation.h
    firstrun.cpp firstrun.h
    gearscore.cpp gearscore.h
    hands.cpp hands.h
    ignore.cpp ignore.h
    item.h
    league.cpp league.h
    links.cpp links.h
    listing.cpp listing.h
    mapcheck.cpp mapcheck.h
    numbers.cpp numbers.h
    parser.cpp parser.h
    placement.cpp placement.h
    places.cpp places.h
    pobbuild.cpp pobbuild.h
    pobeval.cpp pobeval.h
    pobguide.cpp pobguide.h
    pobinstall.cpp pobinstall.h
    pobverdict.cpp pobverdict.h
    pobweighing.cpp pobweighing.h
    ratelimit.cpp ratelimit.h
    recommendations.cpp recommendations.h
    rates.cpp rates.h
    resists.cpp resists.h
    scorer.cpp scorer.h
    sessionguide.cpp sessionguide.h
    seenstats.cpp seenstats.h
    settings.cpp settings.h
    slotmods.cpp slotmods.h
    slots.cpp slots.h
    statmap.cpp statmap.h
    statnorm.cpp statnorm.h
    statsources.cpp statsources.h
    tradedata.cpp tradedata.h
    tradequery.cpp tradequery.h
    tradesite.cpp tradesite.h
    verdict.h
    weighedprofile.cpp weighedprofile.h
    weights.cpp weights.h
    wearing.cpp wearing.h
)

target_include_directories(pou_core PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_compile_definitions(pou_core PRIVATE POU_BETA_BUILD=$<BOOL:${POU_BETA}>)
target_link_libraries(pou_core PUBLIC Qt6::Core PRIVATE ZLIB::ZLIB)

# what each kind of item can roll, from tools/slot-mods.py
qt_add_resources(pou_core slotmods
    PREFIX /pou
    BASE ${PROJECT_SOURCE_DIR}/data
    FILES ${PROJECT_SOURCE_DIR}/data/slotmods.json
)
