cmake_minimum_required(VERSION 3.0)
project(export-fimfiction VERSION 0.1)

include(FindPkgConfig)

pkg_check_modules(GUI
  IMPORTED_TARGET REQUIRED
  glib-2.0 gtk+-3.0)

add_subdirectory(libxmlfixes)

add_executable(export-fimfiction
  src/main.c
  src/gui.c
  src/wordcount.c)

target_link_libraries(export-fimfiction PRIVATE xmlfixes PkgConfig::GUI)
