cmake_minimum_required(VERSION 3.20)
project(KekMeIfYouCan CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS src/engine/*.cpp src/engine/*.h)
file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS src/game/*.cpp src/game/*.h)

# Engine + Spiel als statische Bibliothek, damit GUI- und Konsolen-Build dieselben Quellen teilen.
add_library(phcore STATIC ${ENGINE_SOURCES} ${GAME_SOURCES})
target_include_directories(phcore PUBLIC src)
target_compile_definitions(phcore PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS)
target_compile_options(phcore PUBLIC /utf-8 /W3 /permissive- /fp:fast /EHsc /MP /wd4244 /wd4267 /wd4305)
target_link_libraries(phcore PUBLIC opengl32 winmm ws2_32 gdi32 user32 ole32 winhttp bcrypt mfplat mfreadwrite mfuuid shlwapi)

# Spiel (Windows-Subsystem, kein Konsolenfenster) – mit Programmsymbol und Versionsinfo aus res/app.rc
# res/intro.rc bettet die Intro-Videos ein (nur in der Spiel-EXE)
add_executable(KekMeIfYouCan WIN32 src/main/main_win.cpp res/app.rc res/intro.rc)
target_link_libraries(KekMeIfYouCan PRIVATE phcore)

# Konsolen-Build: dedizierter Server, automatische Tests, Screenshots
add_executable(KekMeIfYouCanCLI src/main/main_cli.cpp res/app.rc)
target_link_libraries(KekMeIfYouCanCLI PRIVATE phcore)
