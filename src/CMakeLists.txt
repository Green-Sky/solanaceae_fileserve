cmake_minimum_required(VERSION 3.24 FATAL_ERROR)

add_library(fileserve
	./fileserve.hpp
	./fileserve.cpp
)
target_include_directories(fileserve PUBLIC .)
target_compile_features(fileserve PUBLIC cxx_std_17)
target_link_libraries(fileserve PUBLIC
	solanaceae_contact
	solanaceae_message3
)

