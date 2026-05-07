cmake_minimum_required(VERSION 3.20)

project(ote VERSION 1.0.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(ote_core STATIC
    src/config.cpp
    src/api.cpp
    src/layers.cpp
    src/platform.cpp
    src/terminal.cpp
    src/mcp.cpp
    src/runtime.cpp
    src/secret.cpp
)

target_include_directories(ote_core PUBLIC include)

add_executable(ote
    src/app.cpp
    src/main.cpp
)

target_link_libraries(ote PRIVATE ote_core)
target_include_directories(ote PRIVATE include)

if (MSVC)
    target_compile_options(ote_core PRIVATE /W4 /permissive-)
    target_compile_options(ote PRIVATE /W4 /permissive-)
else()
    target_compile_options(ote_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(ote PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_compile_definitions(ote_core PRIVATE OTE_VERSION="${PROJECT_VERSION}")
target_compile_definitions(ote PRIVATE OTE_VERSION="${PROJECT_VERSION}")

if (WIN32)
    target_link_libraries(ote_core PRIVATE crypt32)
endif()

include(GNUInstallDirs)

install(TARGETS ote RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES README.md DESTINATION ${CMAKE_INSTALL_DATADIR}/ote)
install(FILES ascii.txt DESTINATION ${CMAKE_INSTALL_DATADIR}/ote)
install(FILES docs/architecture.md DESTINATION ${CMAKE_INSTALL_DATADIR}/ote/docs)
install(FILES docs/security.md DESTINATION ${CMAKE_INSTALL_DATADIR}/ote/docs)
install(FILES docs/mcp.md DESTINATION ${CMAKE_INSTALL_DATADIR}/ote/docs)
install(FILES docs/agent-integration.md DESTINATION ${CMAKE_INSTALL_DATADIR}/ote/docs)
install(FILES docs/release.md DESTINATION ${CMAKE_INSTALL_DATADIR}/ote/docs)
install(FILES CHANGELOG.md DESTINATION ${CMAKE_INSTALL_DATADIR}/ote)
install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/ote)
install(FILES ote-config.example.config DESTINATION ${CMAKE_INSTALL_DATADIR}/ote)

set(CPACK_PACKAGE_NAME "ote")
set(CPACK_PACKAGE_VENDOR "OTE")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "One Time Execution secure secret store and runtime")
set(CPACK_GENERATOR "ZIP;TGZ")
include(CPack)
