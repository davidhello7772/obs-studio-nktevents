cmake_minimum_required(VERSION 3.28...3.30)

project(audio-monitor VERSION 1.0.0)

option(ENABLE_AUDIO_MONITOR "Enable building OBS with audio-monitor plugin" ON)
if(NOT ENABLE_AUDIO_MONITOR)
  target_disable(audio-monitor)
  return()
endif()

# Find Qt
find_package(Qt6 REQUIRED Core Widgets)

# Add Qt resource file for SVG icons
qt6_add_resources(AUDIO_MONITOR_RESOURCES resources.qrc)

add_library(audio-monitor MODULE)

target_sources(
  audio-monitor
  PRIVATE # cmake-format: sortable
          ${AUDIO_MONITOR_RESOURCES}
          src/audio-monitor-window.cpp
          src/audio-monitor-window.hpp
          src/audio-source-widget.cpp
          src/audio-source-widget.hpp
          src/filter-item-widget.cpp
          src/filter-item-widget.hpp
          src/filter-list-widget.cpp
          src/filter-list-widget.hpp
          src/plugin-main.cpp
          src/plugin-main.hpp
          src/volume-meter.cpp
          src/volume-meter.hpp
          src/volume-monitor.cpp
          src/volume-monitor.hpp)

target_link_libraries(
  audio-monitor
  PRIVATE OBS::libobs
          OBS::frontend-api
          Qt::Core
          Qt::Widgets)

target_include_directories(audio-monitor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

set_target_properties_obs(
  audio-monitor
  PROPERTIES FOLDER plugins
             PREFIX ""
             AUTOMOC ON)
