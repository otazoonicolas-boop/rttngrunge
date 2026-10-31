cmake_minimum_required(VERSION 3.22)
project(SR1VocalChain VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: usar -DJUCE_DIR=/ruta/a/JUCE, o se descarga automaticamente.
if(SR1_JUCE_INSTALLED)
  find_package(JUCE CONFIG REQUIRED)   # JUCE instalado (necesario para cross-compilar)
elseif(JUCE_DIR)
  add_subdirectory(${JUCE_DIR} juce EXCLUDE_FROM_ALL)
else()
  include(FetchContent)
  FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.6 GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(JUCE)
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/Source/Generated/plugin_info.cmake)  # SR1_PRODUCT_NAME, SR1_PLUGIN_CODE

# Runtime WebAssembly (wasm3), compilado como libreria C estatica
set(W3 ${CMAKE_CURRENT_SOURCE_DIR}/third_party/wasm3)
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

if(CMAKE_CROSSCOMPILING)
  set(SR1_MANIFEST FALSE)   # el helper de manifiesto VST3 no se puede ejecutar al cross-compilar
else()
  set(SR1_MANIFEST TRUE)
endif()

juce_add_plugin(SR1VocalChain
  COMPANY_NAME "SR1"
  PLUGIN_MANUFACTURER_CODE Sr1c
  PLUGIN_CODE ${SR1_PLUGIN_CODE}
  FORMATS VST3
  PRODUCT_NAME "${SR1_PRODUCT_NAME}"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  VST3_CATEGORIES Fx
  VST3_AUTO_MANIFEST ${SR1_MANIFEST}
  COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(SR1VocalChain PRIVATE
  Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/SR1Engine.cpp)

target_compile_definitions(SR1VocalChain PUBLIC
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(SR1VocalChain
  PRIVATE wasm3 juce::juce_audio_utils
  PUBLIC juce::juce_recommended_config_flags)

if(MINGW)  # DLL autonoma: sin dependencias de libstdc++/libgcc en la PC del usuario
  target_link_options(SR1VocalChain PRIVATE -static -static-libgcc -static-libstdc++)
endif()
