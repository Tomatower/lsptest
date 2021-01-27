cmake_minimum_required(VERSION 3.8)
project (lsptest VERSION 1.0 LANGUAGES CXX)


add_executable(lsptest "")

set_property(TARGET lsptest PROPERTY CXX_STANDARD 17)
set_property(TARGET lsptest PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET lsptest PROPERTY CXX_EXTENSIONS OFF)

target_compile_options(lsptest PRIVATE
                       -Wall
                       -Wextra
                       -pedantic

                       -Wno-sign-compare
                       -ggdb
                       -Og
)

target_link_options(lsptest PRIVATE
    -pthread
)
# GCC
target_compile_options(lsptest PRIVATE
	-Wno-return-type
	-Wno-unused-result
	)

#Clang
# target_compile_options(lsptest PRIVATE
#	-ftime-trace)

# Find Boost
find_package(Boost 1.74 REQUIRED
    COMPONENTS system coroutine
)
target_include_directories(lsptest PRIVATE ${Boost_INCLUDE_DIRS})
target_link_libraries(lsptest ${Boost_LIBRARIES})


# Find Qt
find_package(Qt5 COMPONENTS Core Network Widgets REQUIRED)
target_link_libraries(lsptest Qt::Core Qt::Network Qt::Widgets)


# Install
install(TARGETS lsptest RUNTIME DESTINATION bin)

# Compile
target_include_directories(lsptest PRIVATE src)

qt_generate_moc(src/connection.h connection.moc.cc TARGET lsptest)
qt_generate_moc(src/connection_handler.h connection_handler.moc.cc TARGET lsptest)


target_sources(lsptest PRIVATE
	src/main.cc
	src/messages.cc
    src/connection.cc
    src/connection_handler.cc
    src/decoding.cc
    src/lsp.cc
    connection.moc.cc
    connection_handler.moc.cc
)



#find_program(iwyu_path NAMES include-what-you-use iwyu)
#if(iwyu_path)
#    message("Using include-what-you-use: ${iwyu_path}")
#    set_property(TARGET lsptest PROPERTY CXX_INCLUDE_WHAT_YOU_USE "${iwyu_path};-Xiwyu;any;-Xiwyu;iwyu;-Xiwyu;args;-std=c++14")
#else()
#    message(ERROR "Could not find include-what-you-use")
#endif()
