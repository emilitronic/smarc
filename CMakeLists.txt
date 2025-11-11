cmake_minimum_required(VERSION 3.15)
project(smarc LANGUAGES CXX)

# Cedar still carries very old cmake_minimum_required() values which trip
# deprecation warnings on newer hosts. Disable those warnings locally so our
# configure step stays clean without patching upstream Cedar.
set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "suppress cedar cmake_minimum warnings" FORCE)

# --- Point to your local cedar directory ---
# Option 1: hardcode it
#set(CEDAR_DIR "/home/seb/cedar")

# Option 2 (recommended): pass it when configuring:
# cmake -DCEDAR_DIR=../Cascade/cedar ..

# --- Add cedar to the build tree ---
add_subdirectory(${CEDAR_DIR} cedar_build EXCLUDE_FROM_ALL)

# --- Now add your subprojects ---
add_subdirectory(smicro)
add_subdirectory(smile)
