add_library(pou_services STATIC
    buildfetcher.cpp buildfetcher.h
    characterstore.cpp characterstore.h
    datafiles.cpp datafiles.h
    logging.cpp logging.h
    loopbackserver.cpp loopbackserver.h
    network.cpp network.h
    pobengine.cpp pobengine.h
    poeauth.cpp poeauth.h
    poeconfig.cpp poeconfig.h
    pobfinder.cpp pobfinder.h
    ratesrefresher.cpp ratesrefresher.h
    ownpaths.cpp ownpaths.h
    ratesstore.cpp ratesstore.h
    settingsfile.cpp settingsfile.h
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
    POU_POE_CLIENT_ID="${POU_POE_CLIENT_ID}"
    POU_POE_CONTACT="${POU_POE_CONTACT}"
    POU_POE_REDIRECT_PORTS="${POU_POE_REDIRECT_PORTS}"
)

target_link_libraries(pou_services PUBLIC pou_core Qt6::Network)
