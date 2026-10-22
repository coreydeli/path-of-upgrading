add_library(pou_core STATIC
    item.h
    numbers.cpp numbers.h
    slots.cpp slots.h
    statmap.cpp statmap.h
    statnorm.cpp statnorm.h
)

target_include_directories(pou_core PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(pou_core PUBLIC Qt6::Core)
