add_library(pou_core STATIC
    slots.cpp slots.h
)

target_include_directories(pou_core PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(pou_core PUBLIC Qt6::Core)
