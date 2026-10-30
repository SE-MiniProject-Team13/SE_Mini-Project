cmake_minimum_required(VERSION 3.10)
project(TrafficManagementSimulation LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(traffic_sim
    src/main.cpp
    src/config.cpp
    src/event_logger.cpp
    src/signal_controller.cpp
    src/simulation_engine.cpp
    src/vehicle_generator.cpp
    src/vehicle_queue.cpp
)
target_include_directories(traffic_sim PRIVATE include)
