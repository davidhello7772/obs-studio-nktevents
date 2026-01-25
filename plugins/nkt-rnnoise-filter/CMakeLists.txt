cmake_minimum_required(VERSION 3.28...3.30)

add_library(nkt-rnnoise-filter MODULE)
add_library(OBS::nkt-rnnoise-filter ALIAS nkt-rnnoise-filter)

target_sources(
  nkt-rnnoise-filter
  PRIVATE
    nkt-rnnoise-filter.c
    nkt-rnnoise-filter.h
)

target_link_libraries(nkt-rnnoise-filter PRIVATE OBS::libobs Librnnoise::Librnnoise)

target_compile_definitions(nkt-rnnoise-filter PRIVATE LIBRNNOISE_ENABLED)

set_target_properties_obs(nkt-rnnoise-filter PROPERTIES FOLDER plugins/nkt PREFIX "")
