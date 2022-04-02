cmake_minimum_required(VERSION 3.12)

project(test LANGUAGES CXX)

enable_testing()

add_executable (${PROJECT_NAME}_search_server server_test.cpp)
target_include_directories(${PROJECT_NAME}_search_server PRIVATE serch_server)
target_link_libraries(${PROJECT_NAME}_search_server serch_server)

add_executable (${PROJECT_NAME}_benchmark server_test_benchmark.cpp)
target_include_directories(${PROJECT_NAME}_benchmark PRIVATE serch_server)
target_link_libraries(${PROJECT_NAME}_benchmark serch_server)

add_test(${PROJECT_NAME}_search_server ${PROJECT_NAME}_search_server)
add_test(${PROJECT_NAME}_benchmark ${PROJECT_NAME}_benchmark)