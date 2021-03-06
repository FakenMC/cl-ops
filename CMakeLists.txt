cmake_minimum_required(VERSION 2.8)
project(cl_ops C)

# Set modules directory
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules")

# Set log domain
add_definitions(-DG_LOG_DOMAIN=\"${PROJECT_NAME}\")

# #################### #
# DEPENDENCIES SECTION #
# #################### #

# Find git
find_package(Git QUIET)
# Update version from Git?
if (GIT_FOUND)
	option(VERSION_UPDATE_FROM_GIT "Update project version from git?" OFF)
endif()
# Determine version from Git
include(GetVersionFromGitTag)
# Required for building with Visual Studio
include(GenerateExportHeader)

# How to get Glib location
option(GLIB_USE_PKGCONFIG "Use package config for Glib? (will cause problems with MSVC)" ON)
if(GLIB_USE_PKGCONFIG)
	# Search for glib with pkg-config (this will cause problems with
	# Visual Studio). Works on Linux, Mac and MinGW32
	unset(GLIB_INCLUDE_DIR CACHE)
	unset(GLIB_CONFIG_INCLUDE_DIR CACHE)
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(GLIB REQUIRED glib-2.0>=2.32.1)
else()
	# PkgConfig doesnt work well with Visual Studio, so make user
	# specify location of GLib stuff
	find_library(GLIB_LIBRARIES glib-2.0)
	find_path(GLIB_INCLUDE_DIR glib.h)
	find_path(GLIB_CONFIG_INCLUDE_DIR glibconfig.h)
	set(GLIB_INCLUDE_DIRS "${GLIB_INCLUDE_DIR}" "${GLIB_CONFIG_INCLUDE_DIR}")
endif()

# Find OpenCL
find_package(OpenCL REQUIRED)

# Find cf4ocl2
find_package(cf4ocl2 REQUIRED)

# Find optional executables for creating docs
find_package(Doxygen 1.8.3 QUIET)
find_package(LATEX QUIET)

# ################ #
# COMPILER OPTIONS #
# ################ #

# Compiler options for GCC/Clang
# -Wno-comment because of comment within comment in OpenCL headers
if((${CMAKE_C_COMPILER_ID} STREQUAL "Clang")
	OR (${CMAKE_C_COMPILER_ID} STREQUAL "GNU"))
	# Clang and GCC flags
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wextra -Wall -Wno-comment -std=c99")
elseif(${CMAKE_C_COMPILER_ID} STREQUAL "MSVC")
	# This line is required for glib to work with MSVC
	add_definitions("-Dinline=__inline")
	# And this avoids some warnings
	add_definitions("-D_CRT_SECURE_NO_WARNINGS")
endif()

# Avoid including MinGW dll dependency
if(MINGW)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -static-libgcc")
endif()

# ########################### #
# SET LIBRARY INCLUDE FOLDERS #
# ########################### #
include_directories(
	${CMAKE_SOURCE_DIR}/src/${PROJECT_NAME}
	${CMAKE_SOURCE_DIR}/src/
	${CMAKE_BINARY_DIR} ${GLIB_INCLUDE_DIRS}
	${OPENCL_INCLUDE_DIRS} ${CF4OCL2_INCLUDE_DIRS})

# ###################### #
# DEFINE INSTALL FOLDERS #
# ###################### #
set(INSTALL_SUBDIR_DOCS_HTML share/doc/${PROJECT_NAME}/html)
set(INSTALL_SUBDIR_DOCS_PDF share/doc/${PROJECT_NAME})
set(INSTALL_SUBDIR_DOCS_MAN share/man/)
set(INSTALL_SUBDIR_INCLUDE include)
set(INSTALL_SUBDIR_BIN bin)
set(INSTALL_SUBDIR_LIB lib)

# ########################### #
# BUILD DOCUMENTATION SECTION #
# ########################### #

# add a target to generate API documentation with Doxygen
if(DOXYGEN_FOUND AND DOXYGEN_DOT_FOUND AND LATEX_COMPILER)

	# Doc build is enabled
	set(DOC_BUILD "ON" CACHE INTERNAL "Build documentation")

	# Set doc generation directory
	set(DOCS_DIRECTORY "doc")

	# Document generation options
	option(DOC_GENERATE_HTML "Generate HTML documentation?" ON)
	option(DOC_GENERATE_LATEX "Generate Latex documentation?" OFF)
	if (DOC_GENERATE_LATEX AND CMAKE_MAKE_PROGRAM AND PDFLATEX_COMPILER AND MAKEINDEX_COMPILER)
		option(DOC_GENERATE_PDF "Generate PDF documentation?" OFF)
	else()
		unset(DOC_GENERATE_PDF CACHE)
	endif()
	if(UNIX)
		option(DOC_GENERATE_MAN "Generate man pages?" OFF)
	endif()

	# Set doxygen document generation options
	if (DOC_GENERATE_HTML)
		set(DOX_GENERATE_HTML "YES")
	else()
		set(DOX_GENERATE_HTML "NO")
	endif()
	if (DOC_GENERATE_LATEX)
		set(DOX_GENERATE_LATEX "YES")
	else()
		set(DOX_GENERATE_LATEX "NO")
	endif()
	if (DOC_GENERATE_MAN)
		set(DOX_GENERATE_MAN "YES")
	else()
		set(DOX_GENERATE_MAN "NO")
	endif()

	# Document install options
	if (DOC_GENERATE_HTML)
		option(DOC_INSTALL_HTML "Install HTML documentation?" OFF)
	else()
		unset(DOC_INSTALL_HTML CACHE)
	endif()
	if (DOC_GENERATE_PDF)
		option(DOC_INSTALL_PDF "Install Latex documentation?" OFF)
	else()
		unset(DOC_INSTALL_PDF CACHE)
	endif()
	if (DOC_GENERATE_MAN)
		option(DOC_INSTALL_MAN "Install man pages?" OFF)
	else()
		unset(DOC_INSTALL_MAN CACHE)
	endif()

	# If git is found, set document file version filter to last
	# commit where file was changed
	if (GIT_FOUND)
		set(DOX_FILE_VERSION_FILTER
			"${GIT_EXECUTABLE} log -n 1 --pretty=%h --")
	endif()

	# Copy and configure doxigen configuration file
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in
		${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)

	# Add a build docs target
	if (DOC_GENERATE_HTML OR DOC_GENERATE_LATEX OR DOC_GENERATE_MAN)
		add_custom_target(doc
			${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
			WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
			COMMENT "Generating documentation with Doxygen" VERBATIM)
	else()
		add_custom_target(doc
			${CMAKE_COMMAND} -E echo "Enable at least one document format."
			COMMENT "All document generation formats are disabled!" VERBATIM)
	endif()

	# Generate PDF from latex files
	if (DOC_GENERATE_PDF)
		add_custom_command(TARGET doc POST_BUILD
			COMMAND ${CMAKE_MAKE_PROGRAM}
			COMMAND ${CMAKE_COMMAND} -E make_directory ../pdf
			COMMAND ${CMAKE_COMMAND} -E copy refman.pdf ../pdf
			WORKING_DIRECTORY ${DOCS_DIRECTORY}/latex)
	endif()

	# Post-process man pages created by Doxygen
	if(DOC_GENERATE_MAN)
		add_custom_command(TARGET doc POST_BUILD
			COMMAND ${CMAKE_SOURCE_DIR}/auxbuild/buildman.sh
			WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/${DOCS_DIRECTORY}/man/)
	endif()

	# Add documentation installation target (if the docs have been built)
	if (DOC_INSTALL_HTML)
		install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${DOCS_DIRECTORY}/html/
			DESTINATION ${INSTALL_SUBDIR_DOCS_HTML} OPTIONAL)
	endif()

	if (DOC_INSTALL_PDF)
		install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${DOCS_DIRECTORY}/latex/refman.pdf
			DESTINATION ${INSTALL_SUBDIR_DOCS_PDF} OPTIONAL)
	endif()

	if (DOC_INSTALL_MAN)
		install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${DOCS_DIRECTORY}/man/
			DESTINATION ${INSTALL_SUBDIR_DOCS_MAN} OPTIONAL
			FILES_MATCHING REGEX "(ccl|CCL).*gz")
	endif()

else()

	# No doc builds
	unset(DOC_BUILD CACHE)

	# Add doc target which warns that some document build dependencies
	# are missing
	add_custom_target(doc
		${CMAKE_COMMAND} -E echo "Documentation generation is disabled."
		COMMENT "One or more tools required for documentation generation were not found!"
		VERBATIM)

endif()

# ############### #
# PROCESS SUBDIRS #
# ############### #

# Add src folder
add_subdirectory(src)

# Add scripts folder
add_subdirectory(scripts)

