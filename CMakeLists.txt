cmake_minimum_required(VERSION 3.2.0)

project(text-pango)

# Standalone build
if(NOT OBS_OUTPUT_DIR)
	set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
	include(ObsHelpers)

	set(CMAKE_C_STANDARD 11)
	set(STANDALONE_PLUGIN TRUE)

	find_path(OBS_INCLUDE_DIRS
		obs-module.h
		HINTS $ENV{OBS_DIR} ${OBS_DIR}
		PATH_SUFFIXES
			libobs)
	find_library(OBS_LIBRARIES
		NAMES obs libobs
		HINTS $ENV{OBS_DIR} ${OBS_DIR}
		PATH_SUFFIXES
			install/lib
			install/bin/64bit
			libobs)
# Build as submodule of obs-studio
else()
	set(OBS_INCLUDE_DIRS
		../../libobs)
	set(OBS_LIBRARIES
		libobs)
	set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/plugins/obs-text-pango/cmake/")
endif()

find_package(Pango REQUIRED)
find_package(Cairo REQUIRED)
find_package(Pangocairo REQUIRED)
find_package(Pangoft2 REQUIRED)
if(WIN32)
	find_package(Pangowin32 REQUIRED)
endif()
find_package(Fontconfig REQUIRED)
find_package(Freetype REQUIRED)

include_directories(
	${OBS_INCLUDE_DIRS}
	${PANGO_INCLUDE_DIRS}
	${CAIRO_INCLUDE_DIRS}
	${PANGOCAIRO_INCLUDE_DIRS}
	${PANGOFT2_INCLUDE_DIRS}
	${FONTCONFIG_INCLUDE_DIRS}
	${FREETYPE_INCLUDE_DIRS})
if(NOT WIN32)
	FIND_PACKAGE(PkgConfig)
	PKG_CHECK_MODULES(GLIB glib-2.0 REQUIRED)
	include_directories(${GLIB_INCLUDE_DIRS})
endif()

link_directories(
	${PANGO_LIBRARY_DIRS}
	${CAIRO_LIBRARY_DIRS}
	${FONTCONFIG_LIBRARY_DIRS}
	${FREETYPE_LIBRARY_DIRS})

set(text-pango_SOURCES
	text-pango.c
	text-pango.h
	text-utilities.h)

set(win32_static_LIBRARIES
	intl
	mini-fribidi
	pixman-1_static
	zlib
	harfbuzz
	libpng16_static
	expat
	usp10)
if(WIN32)
	set(PLATFORM_LIBRARIES ${win32_static_LIBRARIES})
endif()

add_library(text-pango MODULE
	${text-pango_SOURCES})
target_link_libraries(text-pango
	${OBS_LIBRARIES}
	${CAIRO_LIBRARIES}
	${PANGO_LIBRARIES}
	${PANGOWIN32_LIBRARIES}
	${PANGOCAIRO_LIBRARIES}
	${PANGOFT2_LIBRARIES}
	${FREETYPE_LIBRARIES}
	${FONTCONFIG_LIBRARIES}
	${PLATFORM_LIBRARIES}) # win only currently
target_compile_definitions(text-pango PRIVATE ${CAIRO_C_FLAGS})

if(STANDALONE_PLUGIN)
	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(_bit_suffix "64bit/")
	else()
		set(_bit_suffix "32bit/")
	endif()
	if(APPLE)
		install(TARGETS text-pango
			LIBRARY DESTINATION "Library/Application Support/obs-studio/plugins/libtext-pango/bin/")
		install(DIRECTORY data/common/
			DESTINATION "Library/Application Support/obs-studio/plugins/libtext-pango/data")
		install(DIRECTORY data/apple/
			DESTINATION "Library/Application Support/obs-studio/plugins/libtext-pango/data")
		# for packaging, copy all dependencies, change dependency links
		# run pkgbuild --identifier user.obsproject.kurufu.text-pango --root ./obs-text-pango-install/ text-pango-mac.pkg
	elseif(UNIX AND NOT APPLE)
		install(TARGETS text-pango 
			LIBRARY DESTINATION "bin/")
		install(DIRECTORY data/common/
			DESTINATION data/)
	else() # Not apple so normal layout
		install(TARGETS text-pango
			LIBRARY DESTINATION "obs-plugins/${_bit_suffix}/")
		install(DIRECTORY data/common/
			DESTINATION data/obs-plugins/text-pango)
		# Platform specific data, like fontconfig in win32
		install(FILES $<TARGET_PDB_FILE:${PROJECT_NAME}>
			DESTINATION "obs-plugins/${_bit_suffix}/" OPTIONAL)
		install(DIRECTORY data/win32/
			DESTINATION data/obs-plugins/text-pango)
	endif()
else()
	install_obs_plugin_with_data(text-pango data/common)
	# calling this twice might not work?
	if(WIN32)
		install_obs_plugin_with_data(text-pango data/win32)
	endif()
	if(APPLE)
		install_obs_plugin_with_data(text-pango data/apple)
	endif()
endif()

