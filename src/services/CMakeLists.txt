add_library(pou_services STATIC
    network.cpp network.h
)

target_compile_definitions(pou_services PRIVATE
    APP_VERSION="${PROJECT_VERSION}"
    APP_HOMEPAGE="${PROJECT_HOMEPAGE_URL}"
)

target_link_libraries(pou_services PUBLIC pou_core Qt6::Network)
