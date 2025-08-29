cmake_minimum_required(VERSION 3.24 FATAL_ERROR)

########################################

add_library(plugin_fileserve MODULE
	./plugin_fileserve.cpp
)
target_compile_features(plugin_fileserve PUBLIC cxx_std_17)
set_target_properties(plugin_fileserve PROPERTIES
	C_VISIBILITY_PRESET hidden
)
target_compile_definitions(plugin_fileserve PUBLIC ENTT_API_IMPORT)
target_link_libraries(plugin_fileserve PUBLIC
	solanaceae_plugin
	fileserve
)

