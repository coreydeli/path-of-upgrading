add_library(pou_core STATIC
    buildprofiles.cpp buildprofiles.h
    character.cpp character.h
    ignore.cpp ignore.h
    item.h
    numbers.cpp numbers.h
    parser.cpp parser.h
    resists.cpp resists.h
    scorer.cpp scorer.h
    slots.cpp slots.h
    statmap.cpp statmap.h
    statnorm.cpp statnorm.h
    statsources.cpp statsources.h
    tradedata.cpp tradedata.h
    verdict.h
    weights.cpp weights.h
)

target_include_directories(pou_core PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(pou_core PUBLIC Qt6::Core)
