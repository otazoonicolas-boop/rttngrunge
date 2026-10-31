# Version sin JUCE: VST3 construido directamente con el SDK de Steinberg.
# Uso:  cmake -S NoJUCE -B build -DVST3SDK_DIR=/ruta/a/vst3sdk [-DCMAKE_TOOLCHAIN_FILE=mingw-toolchain.cmake]
cmake_minimum_required(VERSION 3.22)
project(SR1VocalChainSDK VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(VST3SDK_DIR "" CACHE PATH "Ruta al SDK VST3 (github.com/steinbergmedia/vst3sdk)")
option(SR1_BUILD_VALIDATOR "Compilar tambien el validador del SDK (solo para pruebas)" OFF)
set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES ${SR1_BUILD_VALIDATOR} CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VSTGUI_SUPPORT OFF CACHE BOOL "" FORCE)
set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)
set(SMTG_RUN_VST_VALIDATOR OFF CACHE BOOL "" FORCE)
add_subdirectory(${VST3SDK_DIR} vst3sdk)

set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(W3 ${ROOT}/third_party/wasm3)
add_library(wasm3 STATIC
  ${W3}/m3_bind.c ${W3}/m3_code.c ${W3}/m3_compile.c ${W3}/m3_core.c ${W3}/m3_env.c ${W3}/m3_exec.c
  ${W3}/m3_function.c ${W3}/m3_info.c ${W3}/m3_module.c ${W3}/m3_parse.c ${W3}/m3_validate.c)
target_include_directories(wasm3 PUBLIC ${W3})
target_compile_options(wasm3 PRIVATE -w)
# MSVC: el preprocesador clasico no soporta las macros de "fold" de wasm3; se desactivan (resultado identico).
target_compile_definitions(wasm3 PRIVATE d_m3FoldSetLocal=0 d_m3FuseBranch=0)
if(MSVC)
  target_compile_options(wasm3 PRIVATE /Zc:preprocessor)
endif()
set_target_properties(wasm3 PROPERTIES POSITION_INDEPENDENT_CODE ON)

smtg_add_vst3plugin(SR1VocalChain ${CMAKE_CURRENT_SOURCE_DIR}/SR1Vst3.cpp ${ROOT}/Source/SR1Engine.cpp
  ${VST3SDK_DIR}/public.sdk/source/vst/vstsinglecomponenteffect.cpp)
if(WIN32)
  target_sources(SR1VocalChain PRIVATE ${VST3SDK_DIR}/public.sdk/source/main/dllmain.cpp)
elseif(UNIX AND NOT APPLE)
  target_sources(SR1VocalChain PRIVATE ${VST3SDK_DIR}/public.sdk/source/main/linuxmain.cpp)
endif()
target_link_libraries(SR1VocalChain PRIVATE sdk wasm3)
target_include_directories(SR1VocalChain PRIVATE ${W3})
if(MINGW)
  target_link_options(SR1VocalChain PRIVATE -static -static-libgcc -static-libstdc++)
endif()
