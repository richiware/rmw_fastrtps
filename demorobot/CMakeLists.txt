cmake_minimum_required(VERSION 2.8.3)

project(demorobot)

if(NOT WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall")
endif()

    find_package(ament_cmake REQUIRED)
    find_package(rclcpp REQUIRED)
    find_package(rmw REQUIRED)
    find_package(rmw_implementation REQUIRED)
find_package(std_interfaces REQUIRED)

ament_package()

    get_available_rmw_implementations(middleware_implementations)
foreach(middleware_impl ${middleware_implementations})
# Find package each of them.
    find_package("${middleware_impl}" REQUIRED)
endforeach()

# Build the executable for the default rmw implementation.
add_executable(demorobot src/demorobot.cpp)
    ament_target_dependencies(demorobot
            "rclcpp"
            "rmw_implementation"
            "std_interfaces"
            "example_interfaces"
            )

install(TARGETS demorobot DESTINATION bin)

# Build an executable for each rmw implementation.
foreach(middleware_impl_tmp ${middleware_implementations})
add_executable(demorobot__${middleware_impl_tmp} src/demorobot.cpp)
    ament_target_dependencies(demorobot__${middleware_impl_tmp}
            "rclcpp"
            "${middleware_impl_tmp}"
            "std_interfaces"
            "example_interfaces"
            )

install(TARGETS demorobot__${middleware_impl_tmp} DESTINATION bin)
endforeach()
