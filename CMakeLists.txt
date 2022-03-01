cmake_minimum_required(VERSION 3.11)

project(KLSXX)

set(_KLS_THS_PUB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/Published)
set(_KLS_THS_MOD_PATH ${CMAKE_CURRENT_SOURCE_DIR}/Module)

file(GLOB_RECURSE SRC_PUB ${_KLS_THS_PUB_PATH}/*.*)
file(GLOB_RECURSE SRC_MOD ${_KLS_THS_MOD_PATH}/*.*)

add_library(kls.thread STATIC ${SRC_PUB} ${SRC_MOD})
target_include_directories(kls.thread PUBLIC ${_KLS_THS_PUB_PATH})
target_include_directories(kls.thread PRIVATE ${_KLS_THS_MOD_PATH})
target_include_directories(kls.thread INTERFACE ${_KLS_THS_PUB_PATH})
target_link_libraries(kls.thread PUBLIC kls.essential)

add_library(klsxx::thread ALIAS kls.thread)
