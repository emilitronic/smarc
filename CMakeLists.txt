cmake_minimum_required(VERSION 3.15)
project(smarc LANGUAGES CXX)

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
