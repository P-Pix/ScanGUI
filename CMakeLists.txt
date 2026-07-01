cmake_minimum_required(VERSION 3.20)
project(ScanGUI VERSION 3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SCANGUI_BUILD_GUI "Build the GTK desktop client" ON)
option(SCANGUI_BUILD_SERVER "Build the local C++ HTTP API server" ON)
option(SCANGUI_BUILD_TESTS "Build lightweight unit tests" ON)

find_package(PkgConfig REQUIRED)

if(SCANGUI_BUILD_GUI)
    pkg_check_modules(GTKMM REQUIRED gtkmm-3.0)
    pkg_check_modules(LIBCURL REQUIRED libcurl)
    file(GLOB_RECURSE SCANGUI_SOURCES CONFIGURE_DEPENDS src/*.cpp)

    add_executable(ScanGUI ${SCANGUI_SOURCES})

    target_include_directories(ScanGUI PRIVATE
        include
        ${GTKMM_INCLUDE_DIRS}
        ${LIBCURL_INCLUDE_DIRS}
    )

    target_link_directories(ScanGUI PRIVATE
        ${GTKMM_LIBRARY_DIRS}
        ${LIBCURL_LIBRARY_DIRS}
    )

    target_link_libraries(ScanGUI PRIVATE
        ${GTKMM_LIBRARIES}
        ${LIBCURL_LIBRARIES}
    )

    target_compile_options(ScanGUI PRIVATE
        ${GTKMM_CFLAGS_OTHER}
        ${LIBCURL_CFLAGS_OTHER}
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

if(SCANGUI_BUILD_SERVER)
    pkg_check_modules(LIBPQ REQUIRED libpq)
    file(GLOB_RECURSE SCANGUI_SERVER_SOURCES CONFIGURE_DEPENDS server_src/*.cpp)

    add_executable(ScanGUIServer
        ${SCANGUI_SERVER_SOURCES}
        src/JsonScanRepository.cpp
    )

    target_include_directories(ScanGUIServer PRIVATE
        include
        ${LIBPQ_INCLUDE_DIRS}
    )

    target_link_directories(ScanGUIServer PRIVATE
        ${LIBPQ_LIBRARY_DIRS}
    )

    target_link_libraries(ScanGUIServer PRIVATE
        ${LIBPQ_LIBRARIES}
        pthread
    )

    target_compile_options(ScanGUIServer PRIVATE
        ${LIBPQ_CFLAGS_OTHER}
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

if(SCANGUI_BUILD_TESTS)
    add_executable(ScanGUITests
        tests/run_unit_tests.cpp
        src/ScanSession.cpp
        src/JsonScanRepository.cpp
        src/application/FileSystemScanDataSource.cpp
        src/application/OfflineLibrarySync.cpp
        src/application/DownloadQueue.cpp
        server_src/HttpTypes.cpp
    )
    target_include_directories(ScanGUITests PRIVATE include)
    target_compile_options(ScanGUITests PRIVATE -Wall -Wextra -Wpedantic)
    target_link_libraries(ScanGUITests PRIVATE pthread)
    enable_testing()
    add_test(NAME ScanGUITests COMMAND ScanGUITests)
endif()
