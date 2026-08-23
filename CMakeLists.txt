cmake_minimum_required(VERSION 3.21)
project(logu CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# On Windows, link statically so logu.exe doesn't depend on MinGW
# runtime DLLs (libstdc++-6.dll, libgcc_s_seh-1.dll, libwinpthread-1.dll) or
# the MSVC redistributable being present/matching on the target machine.
if(WIN32 AND MSVC)
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# ---------------------------------------------------------------------------
# Warning flags, applied to this project's own targets only (via the interface
# library below) -- never to the fetched dependencies, which have their own
# ideas about what is worth warning about and would drown the build in noise
# we can't act on.
#
# The codebase builds clean under all of these today, including -Wconversion,
# which is the one most likely to catch a real bug here: the model indexes
# lines with a mix of int and size_t, and a narrowing conversion introduced by
# accident is exactly how a large-log bug gets in.
# ---------------------------------------------------------------------------
add_library(logu_warnings INTERFACE)
target_compile_options(logu_warnings INTERFACE
	$<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra;-Wshadow;-Wconversion>
	$<$<CXX_COMPILER_ID:MSVC>:/W4>
)

# ---------------------------------------------------------------------------
# Vendored, header-only libraries already checked into libs/ (CLI11, mio,
# nlohmann/json). Nothing to download for these — just expose their include
# paths, matching the #include forms used in src/ ("<CLI11.hpp>", "<mio.hpp>",
# "<nlohmann/json.hpp>").
# ---------------------------------------------------------------------------
add_library(vendored_headers INTERFACE)
target_include_directories(vendored_headers INTERFACE
	${CMAKE_CURRENT_SOURCE_DIR}/libs
	${CMAKE_CURRENT_SOURCE_DIR}/libs/cli11
	${CMAKE_CURRENT_SOURCE_DIR}/libs/mio
)

# ---------------------------------------------------------------------------
# zlib — prefer an already-installed system copy (very commonly present on
# Linux/macOS); fetch and build it from source otherwise (typical on Windows).
# ---------------------------------------------------------------------------
find_package(ZLIB QUIET)
if(NOT ZLIB_FOUND)
	message(STATUS "System zlib not found, fetching madler/zlib instead")

	set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
	set(ZLIB_BUILD_TESTING OFF CACHE BOOL "" FORCE)
	set(ZLIB_BUILD_SHARED OFF CACHE BOOL "" FORCE)
	set(ZLIB_BUILD_STATIC ON CACHE BOOL "" FORCE)
	set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

	FetchContent_Declare(
		zlib
		GIT_REPOSITORY https://github.com/madler/zlib.git
		GIT_TAG v1.3.1
		GIT_SHALLOW TRUE
	)
	FetchContent_MakeAvailable(zlib)

	# zlib's own CMakeLists.txt does not export target_include_directories
	# for its targets, and zconf.h is generated into the build directory, so
	# both the source and binary dirs need to be added explicitly.
	target_include_directories(zlibstatic PUBLIC
		${zlib_SOURCE_DIR}
		${zlib_BINARY_DIR}
	)
	add_library(ZLIB::ZLIB ALIAS zlibstatic)
endif()

# ---------------------------------------------------------------------------
# FTXUI — powers the interactive terminal viewer. Not commonly packaged by
# system package managers, so always fetch it.
# ---------------------------------------------------------------------------
set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_TESTS_FUZZER OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
	ftxui
	GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
	GIT_TAG v7.0.3
	GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(ftxui)

# ---------------------------------------------------------------------------
# clip — cross-platform clipboard access used by the viewer's /copy command.
# Not commonly packaged by system package managers, so always fetch it. On
# Linux it talks to X11/xcb; clip's own CMakeLists.txt finds those.
# ---------------------------------------------------------------------------
set(CLIP_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CLIP_TESTS OFF CACHE BOOL "" FORCE)
set(CLIP_X11_WITH_PNG OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
	clip
	GIT_REPOSITORY https://github.com/dacap/clip.git
	GIT_TAG main
	GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(clip)

# ---------------------------------------------------------------------------
# logu
# ---------------------------------------------------------------------------
file(GLOB_RECURSE LOG_UTILS_SOURCES CONFIGURE_DEPENDS
	${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

add_executable(logu ${LOG_UTILS_SOURCES})
target_include_directories(logu PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Fixed, generator-independent output directory (build/bin) rather than the
# generator's default — Makefile/Ninja generators would put logu
# directly in the build dir, but multi-config generators (Visual Studio,
# Xcode) nest it under a per-config subdirectory. Pinning it to build/bin/
# for every generator/config keeps "where's the binary" a fixed answer, and
# lets the formats.json copy step below target a plain, static path instead
# of a generator expression.
set_target_properties(logu PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
foreach(config_type ${CMAKE_CONFIGURATION_TYPES})
	string(TOUPPER ${config_type} config_type_upper)
	set_target_properties(logu PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY_${config_type_upper} ${CMAKE_BINARY_DIR}/bin
	)
endforeach()

find_package(Threads REQUIRED)

target_link_libraries(logu PRIVATE
	logu_warnings
	vendored_headers
	ftxui::screen
	ftxui::dom
	ftxui::component
	clip
	ZLIB::ZLIB
	Threads::Threads
)

if(WIN32 AND MINGW)
	target_link_options(logu PRIVATE -static -static-libgcc -static-libstdc++)
endif()

# Convenience: keep the timestamp-format catalog next to the built binary.
# Purely optional — TimestampFormatCatalog falls back to built-in defaults
# when formats.json is absent (see service/TimestampFormat.cpp).
#
# Deliberately NOT a POST_BUILD step on the logu target: with the
# Makefile generators, POST_BUILD commands only run when the target actually
# relinks, so an incremental build that leaves logu.exe untouched would
# silently skip the copy. An OUTPUT-based custom command wired in via
# add_dependencies is re-checked on every build invocation instead.
add_custom_command(
	OUTPUT ${CMAKE_BINARY_DIR}/bin/formats.json
	COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/bin
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		${CMAKE_CURRENT_SOURCE_DIR}/formats.json
		${CMAKE_BINARY_DIR}/bin/formats.json
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/formats.json
	COMMENT "Copying formats.json next to logu"
	VERBATIM
)
add_custom_target(logu-formats ALL
	DEPENDS ${CMAKE_BINARY_DIR}/bin/formats.json
)
add_dependencies(logu logu-formats)

# ---------------------------------------------------------------------------
# logu-tests — unit tests over the parts that don't touch the terminal:
# timestamp parsing, merging/dedup, the document model, filter expressions,
# the command layer, and the file/directory helpers. Deliberately excludes
# view/, controller/, theme/ and util/Clipboard.cpp, so the test binary needs
# neither FTXUI nor clip and builds in seconds.
#
# Run with: ctest --test-dir build --output-on-failure
#       or: ./build/bin/logu-tests
# ---------------------------------------------------------------------------
option(LOGU_BUILD_TESTS "Build the unit tests" ON)

if(LOGU_BUILD_TESTS)
	enable_testing()

	file(GLOB_RECURSE LOG_UTILS_TESTABLE_SOURCES CONFIGURE_DEPENDS
		${CMAKE_CURRENT_SOURCE_DIR}/src/command/*.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/src/model/*.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/src/service/*.cpp
	)
	file(GLOB_RECURSE LOG_UTILS_TEST_SOURCES CONFIGURE_DEPENDS
		${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp
	)

	add_executable(logu-tests
		${LOG_UTILS_TEST_SOURCES}
		${LOG_UTILS_TESTABLE_SOURCES}
		${CMAKE_CURRENT_SOURCE_DIR}/src/util/TextUtil.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/src/util/DirectoryScan.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/src/util/PathUtil.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/src/util/TempFile.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/src/util/UserConfig.cpp
	)
	target_include_directories(logu-tests PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/src
		${CMAKE_CURRENT_SOURCE_DIR}/tests
	)
	target_link_libraries(logu-tests PRIVATE
		logu_warnings
		vendored_headers
		ZLIB::ZLIB
		Threads::Threads
	)
	set_target_properties(logu-tests PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
	)
	foreach(config_type ${CMAKE_CONFIGURATION_TYPES})
		string(TOUPPER ${config_type} config_type_upper)
		set_target_properties(logu-tests PROPERTIES
			RUNTIME_OUTPUT_DIRECTORY_${config_type_upper} ${CMAKE_BINARY_DIR}/bin
		)
	endforeach()

	add_test(NAME logu-tests COMMAND logu-tests)
endif()
