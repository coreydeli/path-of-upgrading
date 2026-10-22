find_package(ZLIB REQUIRED) # Path of Building codes are deflated

add_library(pou_core STATIC
    autoweights.cpp autoweights.h
    budget.cpp budget.h
    buildprofiles.cpp buildprofiles.h
    catalog.cpp catalog.h
    character.cpp character.h
    characterstore.cpp characterstore.h
    gearscore.cpp gearscore.h
    ignore.cpp ignore.h
    item.h
    datafiles.cpp datafiles.h
    league.cpp league.h
    listing.cpp listing.h
    numbers.cpp numbers.h
    parser.cpp parser.h
    pobbuild.cpp pobbuild.h
    pobverdict.cpp pobverdict.h
    rates.cpp rates.h
    resists.cpp resists.h
    scorer.cpp scorer.h
    seenstats.cpp seenstats.h
    settings.cpp settings.h
    slots.cpp slots.h
    statmap.cpp statmap.h
    statnorm.cpp statnorm.h
    statsources.cpp statsources.h
    tradedata.cpp tradedata.h
    tradequery.cpp tradequery.h
    tradesite.cpp tradesite.h
    verdict.h
    weights.cpp weights.h
)

target_include_directories(pou_core PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(pou_core PUBLIC Qt6::Core PRIVATE ZLIB::ZLIB)
