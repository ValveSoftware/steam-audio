# Injected into the reference build (core/CMakeLists.txt) without modifying it:
#
#   cmake -S core -B build/reference -DCMAKE_PROJECT_Phonon_INCLUDE=<this file> ...
#
# CMake includes this file right after `project(Phonon)`. At that point no upstream targets exist yet, so we defer
# including golden.cmake until the end of the top-level directory. The generator then sees the same compile options
# and definitions as upstream's own phonon_test and can link the internal `core` object library.
cmake_language(EVAL CODE "cmake_language(DEFER CALL include [[${CMAKE_CURRENT_LIST_DIR}/golden.cmake]])")
