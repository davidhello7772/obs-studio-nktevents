cmake_minimum_required(VERSION 3.28...3.30)

add_library(rnnoise-filter-sh-model MODULE)
add_library(OBS::rnnoise-filter-sh-model ALIAS rnnoise-filter-sh-model)

target_sources(
  rnnoise-filter-sh-model
  PRIVATE
    rnnoise-filter-sh-model.c
    rnnoise-filter-sh-model.h
)

target_link_libraries(rnnoise-filter-sh-model PRIVATE OBS::libobs Librnnoise::Librnnoise)

target_compile_definitions(rnnoise-filter-sh-model PRIVATE LIBRNNOISE_ENABLED)

set_target_properties_obs(rnnoise-filter-sh-model PROPERTIES FOLDER plugins PREFIX "")
