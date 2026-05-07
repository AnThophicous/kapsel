cmake_minimum_required(VERSION 3.20)

project(kapsel VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(ote_core STATIC
    src/config.cpp
    src/api.cpp
    src/json.cpp
    src/layers.cpp
    src/platform.cpp
    src/terminal.cpp
    src/mcp.cpp
    src/mcp_client.cpp
    src/runtime.cpp
    src/secret.cpp
)

target_include_directories(ote_core PUBLIC include)

add_executable(kapsel
    src/app.cpp
    src/main.cpp
)

target_link_libraries(kapsel PRIVATE ote_core)
target_include_directories(kapsel PRIVATE include)

if (MSVC)
    target_compile_options(ote_core PRIVATE /W4 /permissive-)
    target_compile_options(kapsel PRIVATE /W4 /permissive-)
else()
    target_compile_options(ote_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(kapsel PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_compile_definitions(ote_core PRIVATE OTE_VERSION="${PROJECT_VERSION}")
target_compile_definitions(kapsel PRIVATE OTE_VERSION="${PROJECT_VERSION}")

if (WIN32)
    target_link_libraries(ote_core PRIVATE crypt32)
endif()

if (WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(kapsel PRIVATE -static-libgcc -static-libstdc++)
    get_filename_component(OTE_GNU_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    find_file(OTE_WINPTHREAD_DLL
        NAMES libwinpthread-1.dll
        PATHS "${OTE_GNU_BIN_DIR}"
        NO_DEFAULT_PATH
    )
    if (OTE_WINPTHREAD_DLL)
        install(FILES "${OTE_WINPTHREAD_DLL}" DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()
endif()

include(GNUInstallDirs)

install(TARGETS kapsel RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES README.md DESTINATION ${CMAKE_INSTALL_DATADIR}/kapsel)
install(FILES ascii.txt DESTINATION ${CMAKE_INSTALL_DATADIR}/kapsel)
install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/kapsel)
install(FILES ote-config.example.config DESTINATION ${CMAKE_INSTALL_DATADIR}/kapsel)

set(CPACK_PACKAGE_NAME "kapsel")
set(CPACK_PACKAGE_VENDOR "Kapsel")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Kapsel secure secret store and runtime")
set(CPACK_GENERATOR "ZIP;TGZ")
include(CPack)
