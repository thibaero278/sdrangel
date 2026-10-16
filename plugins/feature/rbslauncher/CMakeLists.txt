project(rbslauncher)

# RBS Launcher: simplified SDR++ style front end that drives SDRangel.
# Reuses the MCP server data feed to collect decoded packets and map items from the message pipes.

set(rbslauncher_SOURCES
    rbslauncher.cpp
    rbslaunchersettings.cpp
    rbslauncherplugin.cpp
)

set(rbslauncher_HEADERS
    rbslauncher.h
    rbslaunchersettings.h
    rbslauncherplugin.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(rbslauncher_SOURCES
        ${rbslauncher_SOURCES}
        rbslaunchergui.cpp
        rbswindow.cpp
        rbscontroller.cpp
        rbsmode.cpp
        rbsft8log.cpp
        rbsspectrumtee.cpp
        rbssection.cpp
        rbsdecodemodel.cpp
        rbsfrequencydial.cpp
        rbsbandpopup.cpp
        rbsdelegates.cpp
        ${CMAKE_SOURCE_DIR}/plugins/feature/mcpserver/mcpdatafeed.cpp
        rbslauncher.qrc
    )
    set(rbslauncher_HEADERS
        ${rbslauncher_HEADERS}
        rbslaunchergui.h
        rbswindow.h
        rbscontroller.h
        rbsmode.h
        rbsft8log.h
        rbsspectrumtee.h
        rbssection.h
        rbsdecodemodel.h
        rbsfrequencydial.h
        rbsbandpopup.h
        rbsdelegates.h
        ${CMAKE_SOURCE_DIR}/plugins/feature/mcpserver/mcpdatafeed.h
    )
    include_directories(${CMAKE_SOURCE_DIR}/plugins/feature/mcpserver)

    set(TARGET_NAME ${PLUGINS_PREFIX}featurerbslauncher)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}featurerbslaunchersrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${rbslauncher_SOURCES} ${rbslauncher_HEADERS})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME RBSLauncherPlugin)
    target_sources(${TARGET_NAME} PRIVATE ${rbslauncher_SOURCES} ${rbslauncher_HEADERS})
endif()

if(NOT BUILD_SHARED_LIBS)
    set_property(GLOBAL APPEND PROPERTY STATIC_PLUGINS_PROPERTY ${TARGET_NAME})
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
    Qt::Core
    ${TARGET_LIB}
    sdrbase
    ${TARGET_LIB_GUI}
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
