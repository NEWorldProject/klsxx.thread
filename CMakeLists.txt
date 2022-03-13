cmake_minimum_required(VERSION 3.11)

project(KLSXX)

kls_add_library_module(kls.thread klsxx::thread)
kls_public_source_directory(kls.thread Published)
kls_module_source_directory(kls.thread Module)
target_link_libraries(kls.thread PUBLIC kls.essential)
