add_library(pou_services STATIC
    buildfetcher.cpp buildfetcher.h
    logging.cpp logging.h
    network.cpp network.h
    pobengine.cpp pobengine.h
    ratesrefresher.cpp ratesrefresher.h
    ratesstore.cpp ratesstore.h
    tradeclient.cpp tradeclient.h
    tradedatastore.cpp tradedatastore.h
    tradesearcher.cpp tradesearcher.h
    updatechecker.cpp updatechecker.h
    updatedownloader.cpp updatedownloader.h
)

qt_add_resources(pou_services pobbridge
    PREFIX /
    FILES pobbridge.lua
)

target_compile_definitions(pou_services PRIVATE
    APP_VERSION="${PROJECT_VERSION}"
    APP_HOMEPAGE="${PROJECT_HOMEPAGE_URL}"
)

target_link_libraries(pou_services PUBLIC pou_core Qt6::Network)
