cmake_minimum_required(VERSION 3.14.0)

option(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES "Enable VST 3 Plug-in Examples" OFF)
option(SMTG_ENABLE_VST3_HOSTING_EXAMPLES "Enable VST 3 Hosting Examples" OFF)

set(CMAKE_OSX_DEPLOYMENT_TARGET 10.13 CACHE STRING "")

set(vst3sdk_SOURCE_DIR "../vst3sdk")
if(NOT vst3sdk_SOURCE_DIR)
    message(FATAL_ERROR "Path to VST3 SDK is empty!")
endif()

project(WetCompressor
    # This is your plug-in version number. Change it here only.
    VERSION 1.0.1.0
    DESCRIPTION "WetCompressor VST 3 Plug-in"
)

set(SMTG_VSTGUI_ROOT "${vst3sdk_SOURCE_DIR}")

add_subdirectory(${vst3sdk_SOURCE_DIR} ${PROJECT_BINARY_DIR}/vst3sdk)
smtg_enable_vst3_sdk()

smtg_add_vst3plugin(WetCompressor
    source/version.h
    source/wetcompcids.h
    source/wetcore.h
    source/fetcore.h
    source/compengine.h
    source/compengine.cpp
    source/wetcompprocessor.h
    source/wetcompprocessor.cpp
    source/wetcompcontroller.h
    source/wetcompcontroller.cpp
    source/wetcompentry.cpp
    source/ledmeterview.h
    source/ledmeterview.cpp
    source/modeswitch.h
    source/modeswitch.cpp
    source/compknob.h
    source/compknob.cpp
    source/customviewcreator.h
    source/customviewcreator.cpp
)

#- VSTGUI Wanted ----
if(SMTG_ENABLE_VSTGUI_SUPPORT)
    target_sources(WetCompressor
        PRIVATE
            resource/wetcompeditor.uidesc
    )
    target_link_libraries(WetCompressor
        PRIVATE
            vstgui_support
    )
    # Everything the editor loads at runtime. The knob filmstrips and the
    # backplate are generated in the private toolkit from the panel art.
    smtg_target_add_plugin_resources(WetCompressor
        RESOURCES
            "resource/wetcompeditor.uidesc"
            "resource/backplate.png"
            "resource/knob_input.png"
            "resource/knob_output.png"
    )
endif(SMTG_ENABLE_VSTGUI_SUPPORT)
# -------------------

target_link_libraries(WetCompressor
    PRIVATE
        sdk
)

smtg_target_configure_version_file(WetCompressor)

if(SMTG_MAC)
    smtg_target_set_bundle(WetCompressor
        BUNDLE_IDENTIFIER org.yonie.vst3.wetcompressor
        COMPANY_NAME "Yonie"
    )
    smtg_target_set_debug_executable(WetCompressor
        "/Applications/VST3PluginTestHost.app"
        "--pluginfolder;$(BUILT_PRODUCTS_DIR)"
    )
elseif(SMTG_WIN)
    target_sources(WetCompressor PRIVATE
        resource/win32resource.rc
    )
    if(MSVC)
        set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT WetCompressor)

        smtg_target_set_debug_executable(WetCompressor
            "$(ProgramW6432)/Steinberg/VST3PluginTestHost/VST3PluginTestHost.exe"
            "--pluginfolder \"$(OutDir)/\""
        )
    endif()
elseif(SMTG_LINUX)
    # Linux uses default settings
endif(SMTG_MAC)
