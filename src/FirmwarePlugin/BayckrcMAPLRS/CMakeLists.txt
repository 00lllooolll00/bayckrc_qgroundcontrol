target_sources(${CMAKE_PROJECT_NAME}
    PRIVATE
        BayckrcMAPLRSFirmwarePlugin.cc
        BayckrcMAPLRSFirmwarePlugin.h
        BayckrcMAPLRSFirmwarePluginFactory.cc
        BayckrcMAPLRSFirmwarePluginFactory.h
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
