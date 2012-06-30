PROJECT( namecoin )

CMAKE_MINIMUM_REQUIRED( VERSION 2.8.0 )

SET( CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMakeModules;${CMAKE_MODULE_PATH}" )

INCLUDE( VersionMacros )

SET( DEFAULT_HEADER_INSTALL_DIR include/\${target} )
SET( DEFAULT_LIBRARY_INSTALL_DIR lib/ )
SET( DEFAULT_EXECUTABLE_INSTALL_DIR bin/ )
SET( CMAKE_DEBUG_POSTFIX _debug )
SET( BUILD_SHARED_LIBS NO )

FIND_PACKAGE( BerkeleyDB REQUIRED )

SET(Boost_USE_STATIC_LIBS ON)
FIND_PACKAGE( Boost COMPONENTS thread date_time system filesystem program_options signals ) 


#INCLUDE_DIRECTORIES( /opt/local/include/db52 )
#LINK_DIRECTORIES( /opt/local/lib/db52 )
#SET( BerkeleyDB_LIBRARY db_cxx )

MESSAGE( STATUS "BerkeleyDB Include Dir: ${DB_INCLUDE_DIR}" )

LINK_DIRECTORIES( ${Boost_LIBRARY_DIRS} )
INCLUDE_DIRECTORIES( ${Boost_INCLUDE_DIR} )
INCLUDE_DIRECTORIES( ${DB_INCLUDE_DIR} )

SET( sources 
    auxpow.cpp
    wallet.cpp 
    keystore.cpp
    cryptopp/sha.cpp
    cryptopp/cpu.cpp
    db.cpp
    init.cpp
    irc.cpp
    main.cpp
    namecoin.cpp
    net.cpp
    rpc.cpp
    script.cpp
    util.cpp )

SET( libraries
    ssl
    crypto
    ${Boost_SYSTEM_LIBRARY} 
    ${Boost_THREAD_LIBRARY} 
    ${Boost_FILESYSTEM_LIBRARY} 
    ${Boost_PROGRAM_OPTIONS_LIBRARY} 
    ${DB_LIBRARY}
    )

ADD_DEFINITIONS( -DMSG_NOSIGNAL=0 )

add_executable( namecoind ${sources} )
target_link_libraries( namecoind ${libraries}  )
