#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#

# library versions
include (versions.cmake)

# Option to require building optional plugins
foreach (r ${REQUIRE})
  set(${r}_force ON)
  message(STATUS "Forcing ${r} to ${${r}_force}")
endforeach(r)

include(CheckFunctionExists)
include(CheckIncludeFileCXX)
include(CheckIncludeFiles)
include(CheckIncludeFileCXX)
include(CheckLibraryExists)
include(CheckSymbolExists)
include(FindBoost)
include(FindDoxygen)

#set (CMAKE_VERBOSE_MAKEFILE ON)  # for debugging

#
# Set up installation of .pdb files if the compiler is Visual Studio
#
# Sample: install_pdb (qpidcommon ${QPID_COMPONENT_COMMON})
#
MACRO (install_pdb theLibrary theComponent)
    if (MSVC)
        get_target_property(library_dll ${theLibrary} LOCATION)
        string(REPLACE .dll .pdb library_pdb ${library_dll})
        string(REPLACE $(OutDir)        \${CMAKE_INSTALL_CONFIG_NAME} library_pdb ${library_pdb})
        string(REPLACE $(Configuration) \${CMAKE_INSTALL_CONFIG_NAME} library_pdb ${library_pdb})
        string(REPLACE .pdb d.pdb libraryd_pdb ${library_pdb})
        #message(STATUS "_pdb: ${library_pdb}, ${libraryd_pdb}")
        install (PROGRAMS
                ${library_pdb}
                DESTINATION ${QPID_INSTALL_LIBDIR}/ReleasePDB
                COMPONENT ${theComponent}
                OPTIONAL
                CONFIGURATIONS Release|MinSizeRel)
        install (PROGRAMS
                ${library_pdb}
                DESTINATION ${QPID_INSTALL_LIBDIR}/ReleasePDB
                COMPONENT ${theComponent}
                CONFIGURATIONS RelWithDebInfo)
        install (PROGRAMS
                ${libraryd_pdb}
                DESTINATION ${QPID_INSTALL_LIBDIR}/DebugPDB
                COMPONENT ${theComponent}
                CONFIGURATIONS Debug)
    endif (MSVC)
ENDMACRO (install_pdb)

#
# inherit_value - if the symbol is undefined then set it to the given value.
# Set flag to indicate this symbol was defined here.
#
MACRO (inherit_value theSymbol theValue)
    if (NOT DEFINED ${theSymbol})
        set (${theSymbol} ${theValue})
        # message ("Set symbol '${theSymbol}' to value '${theValue}'")
        set (${theSymbol}_inherited = "true")
    endif (NOT DEFINED ${theSymbol})
ENDMACRO (inherit_value)

#
# If compiler is Visual Studio then create a "version resource" for the project.
# Use this call to override CPACK and file global settings but not file per-project settings.
# Two groups of four version numbers specify "file" and "product" versions separately.
#
# Sample: add_msvc_version_full (qmfengine library dll 1 0 0 1 1 0 0 1)
#
MACRO (add_msvc_version_full verProject verProjectType verProjectFileExt verFN1 verFN2 verFN3 verFN4 verPN1 verPN2 verPN3 verPN4)
    if (MSVC)
        # Create project-specific version strings
        inherit_value ("winver_${verProject}_FileVersionBinary"    "${verFN1},${verFN2},${verFN3},${verFN4}")
        inherit_value ("winver_${verProject}_ProductVersionBinary" "${verPN1},${verPN2},${verPN3},${verPN4}")
        inherit_value ("winver_${verProject}_FileVersionString"    "${verFN1}, ${verFN2}, ${verFN3}, ${verFN4}")
        inherit_value ("winver_${verProject}_ProductVersionString" "${verPN1}, ${verPN2}, ${verPN3}, ${verPN4}")
        inherit_value ("winver_${verProject}_FileDescription"      "${winver_PACKAGE_NAME}-${verProject} ${verProjectType}")
        inherit_value ("winver_${verProject}_LegalCopyright"       "${winver_LEGAL_COPYRIGHT}")
        inherit_value ("winver_${verProject}_InternalName"         "${verProject}")
        inherit_value ("winver_${verProject}_OriginalFilename"     "${verProject}.${verProjectFileExt}")
        inherit_value ("winver_${verProject}_ProductName"          "${winver_DESCRIPTION_SUMMARY}")

        # Create strings to be substituted into the template file
        set ("winverFileVersionBinary"     "${winver_${verProject}_FileVersionBinary}")
        set ("winverProductVersionBinary"  "${winver_${verProject}_ProductVersionBinary}")
        set ("winverFileVersionString"     "${winver_${verProject}_FileVersionString}")
        set ("winverProductVersionString"  "${winver_${verProject}_ProductVersionString}")
        set ("winverFileDescription"       "${winver_${verProject}_FileDescription}")
        set ("winverLegalCopyright"        "${winver_${verProject}_LegalCopyright}")
        set ("winverInternalName"          "${winver_${verProject}_InternalName}")
        set ("winverOriginalFilename"      "${winver_${verProject}_OriginalFilename}")
        set ("winverProductName"           "${winver_${verProject}_ProductName}")

        configure_file(${CMAKE_CURRENT_SOURCE_DIR}/windows/resources/template-resource.rc
                       ${CMAKE_CURRENT_BINARY_DIR}/windows/resources/${verProject}-resource.rc)
        set (${verProject}_SOURCES
            ${${verProject}_SOURCES}
            ${CMAKE_CURRENT_BINARY_DIR}/windows/resources/${verProject}-resource.rc
        )
    endif (MSVC)
ENDMACRO (add_msvc_version_full)

#
# If compiler is Visual Studio then create a "version resource" for the project.
# Use this call to accept file override version settings or
#  inherited CPACK_PACKAGE_VERSION version settings.
#
# Sample: add_msvc_version (qpidcommon library dll)
#
MACRO (add_msvc_version verProject verProjectType verProjectFileExt)
    if (MSVC)
        add_msvc_version_full (${verProject}
                               ${verProjectType}
                               ${verProjectFileExt}
                               ${winver_FILE_VERSION_N1}
                               ${winver_FILE_VERSION_N2}
                               ${winver_FILE_VERSION_N3}
                               ${winver_FILE_VERSION_N4}
                               ${winver_PRODUCT_VERSION_N1}
                               ${winver_PRODUCT_VERSION_N2}
                               ${winver_PRODUCT_VERSION_N3}
                               ${winver_PRODUCT_VERSION_N4})
    endif (MSVC)
ENDMACRO (add_msvc_version)


#
# Install optional windows version settings. Override variables are specified in a file.
#
include (./CMakeWinVersions.cmake OPTIONAL)

#
# Inherit global windows version settings from CPACK settings.
#
inherit_value ("winver_PACKAGE_NAME"         "${CPACK_PACKAGE_NAME}")
inherit_value ("winver_DESCRIPTION_SUMMARY"  "${CPACK_PACKAGE_DESCRIPTION_SUMMARY}")
inherit_value ("winver_FILE_VERSION_N1"      "${CPACK_PACKAGE_VERSION_MAJOR}")
inherit_value ("winver_FILE_VERSION_N2"      "${CPACK_PACKAGE_VERSION_MINOR}")
inherit_value ("winver_FILE_VERSION_N3"      "${CPACK_PACKAGE_VERSION_PATCH}")
inherit_value ("winver_FILE_VERSION_N4"      "1")
inherit_value ("winver_PRODUCT_VERSION_N1"   "${winver_FILE_VERSION_N1}")
inherit_value ("winver_PRODUCT_VERSION_N2"   "${winver_FILE_VERSION_N2}")
inherit_value ("winver_PRODUCT_VERSION_N3"   "${winver_FILE_VERSION_N3}")
inherit_value ("winver_PRODUCT_VERSION_N4"   "${winver_FILE_VERSION_N4}")
inherit_value ("winver_LEGAL_COPYRIGHT"      "")


# check if we generate source as part of the build
#   - rubygen generates the amqp spec
#   - managementgen generates the broker management code
#
# rubygen subdir is excluded from stable distributions
# If the main AMQP spec is present, then check if ruby and python are
# present, and if any sources have changed, forcing a re-gen of source code.

set(AMQP_SPEC_DIR ${qpid-cpp_SOURCE_DIR}/../specs)
set(AMQP_SPEC ${AMQP_SPEC_DIR}/amqp.0-10-qpid-errata.stripped.xml)
if (EXISTS ${AMQP_SPEC})
  include(FindRuby)
  if (NOT RUBY_EXECUTABLE)
    message(FATAL_ERROR "Can't locate ruby, needed to generate source files.")
  endif (NOT RUBY_EXECUTABLE)

  set(specs ${AMQP_SPEC})
  set(regen_amqp OFF)
  set(rgen_dir ${qpid-cpp_SOURCE_DIR}/rubygen)
  file(GLOB_RECURSE rgen_progs ${rgen_dir}/*.rb)
  # If any of the specs, or any of the sources used to generate code, change
  # then regenerate the sources.
  foreach (spec_file ${specs} ${rgen_progs})
    if (${spec_file} IS_NEWER_THAN ${CMAKE_CURRENT_BINARY_DIR}/rubygen.cmake)
      set(regen_amqp ON)
    endif (${spec_file} IS_NEWER_THAN ${CMAKE_CURRENT_BINARY_DIR}/rubygen.cmake)
  endforeach (spec_file ${specs})
  if (regen_amqp)
    message(STATUS "Regenerating AMQP protocol sources")
execute_process(COMMAND ${RUBY_EXECUTABLE} -I ${rgen_dir} ${rgen_dir}/generate ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_BINARY_DIR}/../include  ${specs} all rubygen.cmake
                    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  else (regen_amqp)
    message(STATUS "No need to generate AMQP protocol sources")
  endif (regen_amqp)

  # Pull in the names of the generated files, i.e. ${rgen_framing_srcs}
  include (${CMAKE_CURRENT_BINARY_DIR}/rubygen.cmake)

else (EXISTS ${AMQP_SPEC})
  message(STATUS "No AMQP spec... presume generated sources are included")
  set(QPID_GENERATED_HEADERS_IN_SOURCE ON)
  include (rubygen.cmake)
endif (EXISTS ${AMQP_SPEC})

# FindDoxygen module tries to locate doxygen and Graphviz dot
set (docs_default ON)
if (NOT DOXYGEN_EXECUTABLE)
  set (docs_default OFF)
endif (NOT DOXYGEN_EXECUTABLE)
option(GEN_DOXYGEN "Use doxygen to generate user documentation" ${docs_default})
if (GEN_DOXYGEN AND NOT DOXYGEN_EXECUTABLE)
  message(STATUS "Can't locate the doxygen command; user documentation will not be generated")
  set (GEN_DOXYGEN OFF)
endif (GEN_DOXYGEN AND NOT DOXYGEN_EXECUTABLE)

find_program(VALGRIND valgrind DOC "Location of the valgrind program")
option(ENABLE_VALGRIND "Use valgrind to detect run-time problems" ON)
if (ENABLE_VALGRIND AND NOT VALGRIND)
  message(STATUS "Can't locate the valgrind command; no run-time error detection")
endif (ENABLE_VALGRIND AND NOT VALGRIND)

if (CMAKE_COMPILER_IS_GNUCXX)
   # Warnings: Enable as many as possible, keep the code clean. Please
   # do not disable warnings or remove -Werror without discussing on
   # qpid-dev list.
   #
   # The following warnings are deliberately omitted, they warn on valid code.
   # -Wunreachable-code -Wpadded -Winline
   # -Wshadow - warns about boost headers.
   set (WARNING_FLAGS
        "-Werror -pedantic -Wall -Wextra -Wno-shadow -Wpointer-arith -Wcast-qual -Wcast-align -Wno-long-long -Wvolatile-register-var -Winvalid-pch -Wno-system-headers -Woverloaded-virtual")

   set (CATCH_UNDEFINED "-Wl,--no-undefined")
   # gcc on SunOS uses native linker whose "-z defs" is too fussy
   if (CMAKE_SYSTEM_NAME STREQUAL SunOS)
      set (CATCH_UNDEFINED "")
   endif (CMAKE_SYSTEM_NAME STREQUAL SunOS)
   set (COMPILER_FLAGS "-fvisibility-inlines-hidden")
   # gcc 4.1.2 on RHEL 5 needs -Wno-attributes to avoid an error that's fixed
   # in later gcc versions.
   execute_process(COMMAND ${CMAKE_CXX_COMPILER} -dumpversion
                   OUTPUT_VARIABLE GCC_VERSION)
   if (GCC_VERSION VERSION_EQUAL 4.1.2)
     message (STATUS "Cannot use -fvisibility=hidden on gcc 4.1.2")
   else (GCC_VERSION VERSION_EQUAL 4.1.2)
     set (HIDE_SYMBOL_FLAGS "-fvisibility=hidden")
   endif (GCC_VERSION VERSION_EQUAL 4.1.2)
endif (CMAKE_COMPILER_IS_GNUCXX)

if (CMAKE_CXX_COMPILER_ID STREQUAL SunPro)
  set (COMPILER_FLAGS "-library=stlport4 -mt")
  set (WARNING_FLAGS "+w")
  set (CATCH_UNDEFINED "")
  set (HIDE_SYMBOL_FLAGS "")
endif (CMAKE_CXX_COMPILER_ID STREQUAL SunPro)

if (CMAKE_SYSTEM_NAME STREQUAL Windows)
  # Allow MSVC user to select 'WinXP-SP3/Windows Server 2003' as build target version
  set (win32_winnt_default OFF)
    if (MSVC)
      set (win32_winnt_default ON)
    endif (MSVC)
  option(SET_WIN32_WINNT "In Windows-MSVC build: define _WIN32_WINNT=0x0502 to select target version: Windows XP with SP3" ${win32_winnt_default})
endif (CMAKE_SYSTEM_NAME STREQUAL Windows)

if (MSVC)
  add_definitions(
    /D "_CRT_NONSTDC_NO_WARNINGS"
    /D "NOMINMAX"
    /D "WIN32_LEAN_AND_MEAN"
    /wd4244
    /wd4800
    /wd4355
    /wd4267
  )

  if (SET_WIN32_WINNT)
    add_definitions(/D "_WIN32_WINNT=0x0502")
  endif (SET_WIN32_WINNT)

  # set the RelWithDebInfo compile/link switches to equal Release
  set (CMAKE_CXX_FLAGS_RELWITHDEBINFO "/MD /O2 /Ob2 /D NDEBUG")
  set (CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO "/debug /INCREMENTAL:NO")

  if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../bindings/qpid/dotnet/src)
    # Set the windows version for the .NET Binding cpp project
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../bindings/qpid/dotnet/src/org.apache.qpid.messaging.template.rc
                   ${CMAKE_CURRENT_BINARY_DIR}/windows/resources/org.apache.qpid.messaging.rc)
    # Set the windows version for the .NET Binding sessionreceiver project
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../bindings/qpid/dotnet/src/sessionreceiver/properties/sessionreceiver-AssemblyInfo-template.cs
                   ${CMAKE_CURRENT_BINARY_DIR}/windows/generated_src/sessionreceiver-AssemblyInfo.cs)
  endif (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../bindings/qpid/dotnet/src)
endif (MSVC)

option(ENABLE_WARNINGS "Enable lots of compiler warnings (recommended)" ON)
if (NOT ENABLE_WARNINGS)
  set (WARNING_FLAGS "")
endif (NOT ENABLE_WARNINGS)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMPILER_FLAGS} ${WARNING_FLAGS}")

# Expand a bit from the basic Find_Boost; be specific about what's needed.
# Boost.system is sometimes needed; it's handled separately, below.
if (CMAKE_SYSTEM_NAME STREQUAL Windows)
  set (Boost_components program_options date_time thread unit_test_framework regex)
else (CMAKE_SYSTEM_NAME STREQUAL Windows)
  set (Boost_components program_options unit_test_framework)
endif (CMAKE_SYSTEM_NAME STREQUAL Windows)

# Visual Studio 2010 requires boost 1.45 or better.
# The choice here is to fail demanding the user to update CMake to version N
# where Boost 1.45 is supported, or we can just accept some versions using
# the Additional_versions variable.
if (NOT DEFINED Boost_ADDITIONAL_VERSIONS)
  set (Boost_ADDITIONAL_VERSIONS 
        "1.45" "1.45.0" "1.46" "1.46.0" "1.47" "1.47.0"
        "1.48" "1.48.0" "1.49" "1.49.0" "1.50" "1.50.0"
        "1.51" "1.51.0" "1.52" "1.52.0" "1.53" "1.53.0")
endif (NOT DEFINED Boost_ADDITIONAL_VERSIONS)

find_package(Boost 1.33 REQUIRED COMPONENTS ${Boost_components})
if(NOT Boost_FOUND)
  message(FATAL_ERROR "Boost C++ libraries not found.  Please install or try setting BOOST_ROOT")
endif(NOT Boost_FOUND)

# Boost.system was introduced at Boost 1.35; it's needed secondarily by other
# Boost libs Qpid needs, so be sure it's there.
if (Boost_VERSION GREATER 103499)
  find_package(Boost COMPONENTS system)

  # Allow for cmake pre 2.6 and boost post 1.35
  if (NOT Boost_SYSTEM_LIBRARY)
    set(Boost_SYSTEM_LIBRARY boost_system)
  endif (NOT Boost_SYSTEM_LIBRARY)
endif (Boost_VERSION GREATER 103499)

# Versions of cmake pre 2.6 don't set the Boost_*_LIBRARY variables correctly
# these values are correct for Linux
if (NOT Boost_PROGRAM_OPTIONS_LIBRARY)
  set(Boost_PROGRAM_OPTIONS_LIBRARY boost_program_options)
endif (NOT Boost_PROGRAM_OPTIONS_LIBRARY)

if (NOT Boost_UNIT_TEST_FRAMEWORK_LIBRARY)
  set(Boost_UNIT_TEST_FRAMEWORK_LIBRARY boost_unit_test_framework)
endif (NOT Boost_UNIT_TEST_FRAMEWORK_LIBRARY)

if (NOT Boost_REGEX_LIBRARY)
  set(Boost_REGEX_LIBRARY boost_regex)
endif (NOT Boost_REGEX_LIBRARY)

# The Windows install also wants the Boost DLLs, libs and headers that the
# release is built with. The DLLs enable everything to run, and the headers
# and libs ensure that users building Qpid C++ client programs can compile
# (the C++ API still exposes Boost headers, but hopefully this will be fixed
# in the future).
#
# On Windows you can pick whether the static or dynamic versions of the libs
# are used; allow this choice to the user. Since we also install the Boost
# DLLs that are needed for the Windows package, none are needed for the
# static link case; else drop them into the install. Do this all first, since
# Boost on Windows can use automatic linking to pick up the correct
# Boost libs based on compile-time touching of the headers. Since we don't
# really need to add them to the link lines, set the names to blanks.
option(QPID_LINK_BOOST_DYNAMIC "Link with dynamic Boost libs (OFF to link static)" ON)
if (MSVC)
   install (PROGRAMS
            ${Boost_DATE_TIME_LIBRARY_DEBUG} ${Boost_DATE_TIME_LIBRARY_RELEASE}
            ${Boost_PROGRAM_OPTIONS_LIBRARY_DEBUG} ${Boost_PROGRAM_OPTIONS_LIBRARY_RELEASE}
            ${Boost_REGEX_LIBRARY_DEBUG} ${Boost_REGEX_LIBRARY_RELEASE}
            ${Boost_THREAD_LIBRARY_DEBUG} ${Boost_THREAD_LIBRARY_RELEASE}
            DESTINATION ${QPID_INSTALL_LIBDIR}/boost
            COMPONENT ${QPID_COMPONENT_COMMON})

   if (NOT Boost_VERSION LESS 103500)
      install (PROGRAMS
               ${Boost_SYSTEM_LIBRARY_DEBUG} ${Boost_SYSTEM_LIBRARY_RELEASE}
               DESTINATION ${QPID_INSTALL_LIBDIR}/boost
               COMPONENT ${QPID_COMPONENT_COMMON})
   endif (NOT Boost_VERSION LESS 103500)

   if (QPID_LINK_BOOST_DYNAMIC)
      add_definitions( /D BOOST_ALL_DYN_LINK)
      string (REPLACE .lib .dll
              _boost_date_time_debug ${Boost_DATE_TIME_LIBRARY_DEBUG})
      string (REPLACE .lib .dll
              _boost_date_time_release ${Boost_DATE_TIME_LIBRARY_RELEASE})
      string (REPLACE .lib .dll
              _boost_program_options_debug ${Boost_PROGRAM_OPTIONS_LIBRARY_DEBUG})
      string (REPLACE .lib .dll
              _boost_program_options_release ${Boost_PROGRAM_OPTIONS_LIBRARY_RELEASE})
      string (REPLACE .lib .dll
              _boost_regex_debug ${Boost_REGEX_LIBRARY_DEBUG})
      string (REPLACE .lib .dll
              _boost_regex_release ${Boost_REGEX_LIBRARY_RELEASE})
      string (REPLACE .lib .dll
              _boost_thread_debug ${Boost_THREAD_LIBRARY_DEBUG})
      string (REPLACE .lib .dll
              _boost_thread_release ${Boost_THREAD_LIBRARY_RELEASE})
      # Boost 1.35 added the system library, which gets indirectly linked in
      # via other Boost libs. So, if building with Boost 1.35 or later, also
      # include system in the Windows install package.
      if (NOT Boost_VERSION LESS 103500)
         string (REPLACE boost_thread boost_system
                 _boost_system_debug ${_boost_thread_debug})
         string (REPLACE boost_thread boost_system
                 _boost_system_release ${_boost_thread_release})
      endif (NOT Boost_VERSION LESS 103500)
      install (PROGRAMS
               ${_boost_date_time_debug} ${_boost_date_time_release}
               ${_boost_program_options_debug} ${_boost_program_options_release}
               ${_boost_regex_debug} ${_boost_regex_release}
               ${_boost_system_debug} ${_boost_system_release}
               ${_boost_thread_debug} ${_boost_thread_release}
               DESTINATION ${QPID_INSTALL_LIBDIR}/boost
               COMPONENT ${QPID_COMPONENT_COMMON})
   endif (QPID_LINK_BOOST_DYNAMIC)

   # Need the boost headers regardless of which way the libs go. Try to
   # weed out what we don't need, else it's giant and unnecessary.
   install (DIRECTORY ${Boost_INCLUDE_DIR}/boost
            DESTINATION ${QPID_INSTALL_INCLUDEDIR}
            COMPONENT ${QPID_COMPONENT_CLIENT_INCLUDE}
            PATTERN "accumulators/*" EXCLUDE
            PATTERN "algorithm/*" EXCLUDE
            PATTERN "archive/*" EXCLUDE
            PATTERN "asio*" EXCLUDE
            PATTERN "bimap*" EXCLUDE
            PATTERN "circular_buffer*" EXCLUDE
            PATTERN "concept*" EXCLUDE
            PATTERN "dynamic_bitset*" EXCLUDE
            PATTERN "flyweight*" EXCLUDE
            PATTERN "fusion/*" EXCLUDE
            PATTERN "gil/*" EXCLUDE
            PATTERN "graph/*" EXCLUDE
            PATTERN "interprocess/*" EXCLUDE
            PATTERN "lambda/*" EXCLUDE
            PATTERN "logic/*" EXCLUDE
            PATTERN "math*" EXCLUDE
            PATTERN "mpi*" EXCLUDE
            PATTERN "multi_*" EXCLUDE
            PATTERN "numeric/*" EXCLUDE
            PATTERN "pending/*" EXCLUDE
            PATTERN "pool/*" EXCLUDE
            PATTERN "property_map/*" EXCLUDE
            PATTERN "proto/*" EXCLUDE
            PATTERN "random*" EXCLUDE
            PATTERN "signals*" EXCLUDE
            PATTERN "spirit*" EXCLUDE
            PATTERN "statechart/*" EXCLUDE
            PATTERN "units/*" EXCLUDE
            PATTERN "unordered*" EXCLUDE
            PATTERN "wave*" EXCLUDE
            PATTERN "xpressive/*" EXCLUDE)

   set(Boost_DATE_TIME_LIBRARY "")
   set(Boost_THREAD_LIBRARY "")
   set(Boost_PROGRAM_OPTIONS_LIBRARY "")
   set(Boost_UNIT_TEST_FRAMEWORK_LIBRARY "")
   set(Boost_REGEX_LIBRARY "")
   include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/windows/resources )
endif (MSVC)

include_directories( ${Boost_INCLUDE_DIR} )

include_directories( ${CMAKE_CURRENT_SOURCE_DIR} )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../include )
include_directories( ${CMAKE_CURRENT_BINARY_DIR} )
include_directories( ${CMAKE_CURRENT_BINARY_DIR}/../include )

link_directories( ${Boost_LIBRARY_DIRS} )

CHECK_LIBRARY_EXISTS (rt clock_gettime "" CLOCK_GETTIME_IN_RT)
if (NOT CLOCK_GETTIME_IN_RT)
  CHECK_FUNCTION_EXISTS (clock_gettime QPID_HAS_CLOCK_GETTIME)
else (NOT CLOCK_GETTIME_IN_RT)
  set(CMAKE_REQUIRED_LIBS ${CMAKE_REQUIRED_LIBS} rt)
  set(QPID_HAS_CLOCK_GETTIME YES CACHE BOOL "Platform has clock_gettime")
endif (NOT CLOCK_GETTIME_IN_RT)

# Check for header file for dtrace static probes
check_include_files(sys/sdt.h HAVE_SYS_SDT_H)
if (HAVE_SYS_SDT_H)
  set(probes_default ON)
endif (HAVE_SYS_SDT_H)
option(BUILD_PROBES "Build with DTrace/systemtap static probes" ${probes_default})
if (NOT BUILD_PROBES)
  set (HAVE_SYS_SDT_H 0)
endif (NOT BUILD_PROBES)

# Check for poll/epoll header files
check_include_files(sys/poll.h HAVE_POLL)
check_include_files(sys/epoll.h HAVE_EPOLL)

# Set default poller implementation (check from general to specific to allow overriding)
if (HAVE_POLL)
  set(poller_default poll)
endif (HAVE_POLL)
if (HAVE_EPOLL)
  set(poller_default epoll)
endif (HAVE_EPOLL)
set(POLLER ${poller_default} CACHE STRING "Poller implementation (poll/epoll)")

# If not windows ensure that we have uuid library
if (NOT CMAKE_SYSTEM_NAME STREQUAL Windows)
  CHECK_LIBRARY_EXISTS (uuid uuid_compare "" HAVE_UUID)
  CHECK_INCLUDE_FILES(uuid/uuid.h HAVE_UUID_H)
  if (NOT HAVE_UUID AND NOT HAVE_UUID_H)
    message(FATAL_ERROR "Uuid library and/or header file not found")
  endif (NOT HAVE_UUID AND NOT HAVE_UUID_H)
endif (NOT CMAKE_SYSTEM_NAME STREQUAL Windows)

# See if Cyrus SASL is desired and available
CHECK_LIBRARY_EXISTS (sasl2 sasl_checkpass "" HAVE_SASL)
CHECK_INCLUDE_FILES (sasl/sasl.h HAVE_SASL_H)

set (sasl_default ${sasl_force})
if (HAVE_SASL AND HAVE_SASL_H)
  set (sasl_default ON)
endif (HAVE_SASL AND HAVE_SASL_H)

option(BUILD_SASL "Build with Cyrus SASL support" ${sasl_default})
if (BUILD_SASL)
  if (NOT HAVE_SASL)
    message(FATAL_ERROR "Cyrus SASL support requested but libsasl2 not found")
  endif (NOT HAVE_SASL)
  if (NOT HAVE_SASL_H)
    message(FATAL_ERROR "Cyrus SASL support requested but sasl.h not found")
  endif (NOT HAVE_SASL_H)

  find_program(SASLPASSWD2 saslpasswd2 DOC "Location of the saslpasswd2 program")
  set(BROKER_SASL_NAME "qpidd" CACHE STRING "SASL app name for the qpid broker")
  set(qpidcommon_sasl_source
      qpid/sys/cyrus/CyrusSecurityLayer.h
      qpid/sys/cyrus/CyrusSecurityLayer.cpp
     )
  set(qpidcommon_sasl_lib sasl2)
endif (BUILD_SASL)

# Check for optional RDMA support requirements
include (rdma.cmake)

# Check for optional SSL support requirements
include (ssl.cmake)

# Check for optional AMQP 1.0 support requirements
include (amqp.cmake)

# Check for syslog capabilities not present on all systems
check_symbol_exists (LOG_AUTHPRIV "sys/syslog.h" HAVE_LOG_AUTHPRIV)
check_symbol_exists (LOG_FTP "sys/syslog.h" HAVE_LOG_FTP)
  
if (CMAKE_SYSTEM_NAME STREQUAL Windows)
  set (qpidtypes_platform_SOURCES
    qpid/sys/windows/uuid.cpp
  )
  set (qpidtypes_platform_LIBS
    rpcrt4
  )

  set (qpidcommon_platform_SOURCES
    qpid/log/windows/SinkOptions.cpp
    qpid/sys/windows/AsynchIO.cpp
    qpid/sys/windows/FileSysDir.cpp
    qpid/sys/windows/IocpPoller.cpp
    qpid/sys/windows/IOHandle.cpp
    qpid/sys/windows/LockFile.cpp
    qpid/sys/windows/MemoryMappedFile.cpp
    qpid/sys/windows/PipeHandle.cpp
    qpid/sys/windows/PollableCondition.cpp
    qpid/sys/windows/Shlib.cpp
    qpid/sys/windows/WinSocket.cpp
    qpid/sys/windows/SocketAddress.cpp
    qpid/sys/windows/StrError.cpp
    qpid/sys/windows/SystemInfo.cpp
    qpid/sys/windows/Thread.cpp
    qpid/sys/windows/Time.cpp
    qpid/client/windows/SaslFactory.cpp
  )

  set (qpidcommon_platform_LIBS
    ${Boost_THREAD_LIBRARY} ${windows_ssl_libs} ${Boost_PROGRAM_OPTIONS_LIBRARY} ${Boost_DATE_TIME_LIBRARY} ${Boost_SYSTEM_LIBRARY} ws2_32  )

  set (qpidclient_platform_SOURCES
    qpid/client/windows/ClientDllMain.cpp
  )

  set (qpidclient_platform_LIBS
    ${windows_ssl_libs}
  )

  set (qpidmessaging_platform_SOURCES
     qpid/messaging/HandleInstantiator.cpp
  )

else (CMAKE_SYSTEM_NAME STREQUAL Windows)

  # POSIX (Non-Windows) platforms have a lot of overlap in sources; the only
  # major difference is the poller module.
  if (POLLER STREQUAL poll)
    set (qpid_poller_module
      qpid/sys/posix/PosixPoller.cpp
    )
  elseif (POLLER STREQUAL epoll)
    set (qpid_poller_module
      qpid/sys/epoll/EpollPoller.cpp
    )
  endif (POLLER STREQUAL poll)

  # Set default System Info module
  set (qpid_system_module
    qpid/sys/posix/SystemInfo.cpp
  )

  if (CMAKE_SYSTEM_NAME STREQUAL Linux)
    add_definitions(-pthread)
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread")
    set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${CATCH_UNDEFINED} -pthread")
    set (CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -pthread")
  endif (CMAKE_SYSTEM_NAME STREQUAL Linux)

  if (CMAKE_SYSTEM_NAME STREQUAL SunOS)
    # On Solaris override the system info module
    set (qpid_system_module
      qpid/sys/solaris/SystemInfo.cpp
    )
    # On Sun we want -lpthread -lthread as the 2nd last and last libs passed to linker
    set (qpidtypes_platform_LIBS ${qpidtypes_platform_LIBS}
      pthread
      thread
    )
  endif (CMAKE_SYSTEM_NAME STREQUAL SunOS)

  if (CMAKE_CXX_COMPILER_ID STREQUAL SunPro)
    # -lmalloc needed for mallinfo.
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lmalloc")
    set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -lmalloc")
  endif (CMAKE_CXX_COMPILER_ID STREQUAL SunPro)

  set (qpidtypes_platform_SOURCES)
  set (qpidtypes_platform_LIBS
       uuid
       ${Boost_SYSTEM_LIBRARY}
  )

  set (qpidcommon_platform_SOURCES
    qpid/sys/posix/AsynchIO.cpp
    qpid/sys/posix/Fork.cpp
    qpid/sys/posix/FileSysDir.cpp
    qpid/sys/posix/IOHandle.cpp
    qpid/sys/posix/LockFile.cpp
    qpid/sys/posix/MemoryMappedFile.cpp
    qpid/sys/posix/Mutex.cpp
    qpid/sys/posix/PipeHandle.cpp
    qpid/sys/posix/PollableCondition.cpp
    qpid/sys/posix/Shlib.cpp
    qpid/log/posix/SinkOptions.cpp
    qpid/sys/posix/BSDSocket.cpp
    qpid/sys/posix/SocketAddress.cpp
    qpid/sys/posix/StrError.cpp
    qpid/sys/posix/Thread.cpp
    qpid/sys/posix/Time.cpp
    qpid/SaslFactory.cpp

    ${qpid_system_module}
    ${qpid_poller_module}
  )
  set (qpidcommon_platform_LIBS
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    ${CMAKE_DL_LIBS}
  )

  set (qpidclient_platform_SOURCES
  )

  set (qpidmessaging_platform_SOURCES
  )
endif (CMAKE_SYSTEM_NAME STREQUAL Windows)

set (qpidcommon_SOURCES
     ${rgen_framing_srcs}
     ${qpidcommon_platform_SOURCES}
     ${qpidcommon_sasl_source}
     ${sslcommon_SOURCES}
     qpid/assert.cpp
     qpid/Address.cpp
     qpid/DataDir.cpp
     qpid/Exception.cpp
     qpid/Modules.cpp
     qpid/Options.cpp
     qpid/Plugin.cpp
     qpid/RefCountedBuffer.cpp
     qpid/SessionState.cpp
     qpid/SessionId.cpp
     qpid/StringUtils.cpp
     qpid/Url.cpp
     qpid/UrlArray.cpp
     qpid/NullSaslServer.cpp
     qpid/amqp_0_10/SessionHandler.cpp
     qpid/framing/AccumulatedAck.cpp
     qpid/framing/AMQBody.cpp
     qpid/framing/AMQMethodBody.cpp
     qpid/framing/AMQContentBody.cpp
     qpid/framing/AMQFrame.cpp
     qpid/framing/AMQHeaderBody.cpp
     qpid/framing/AMQHeartbeatBody.cpp
     qpid/framing/Array.cpp
     qpid/framing/Buffer.cpp
     qpid/framing/Endian.cpp
     qpid/framing/FieldTable.cpp
     qpid/framing/FieldValue.cpp
     qpid/framing/FrameSet.cpp
     qpid/framing/FrameDecoder.cpp
     qpid/framing/List.cpp
     qpid/framing/ProtocolInitiation.cpp
     qpid/framing/ProtocolVersion.cpp
     qpid/framing/SendContent.cpp
     qpid/framing/SequenceNumber.cpp
     qpid/framing/SequenceNumberSet.cpp
     qpid/framing/SequenceSet.cpp
     qpid/framing/Proxy.cpp
     qpid/framing/Uuid.cpp
     qpid/framing/TransferContent.cpp
     qpid/log/Logger.cpp
     qpid/log/Options.cpp
     qpid/log/OstreamOutput.cpp
     qpid/log/Selector.cpp
     qpid/log/Statement.cpp
     qpid/sys/AggregateOutput.cpp
     qpid/sys/AsynchIOHandler.cpp
     qpid/sys/Dispatcher.cpp
     qpid/sys/DispatchHandle.cpp
     qpid/sys/Runnable.cpp
     qpid/sys/Shlib.cpp
     qpid/sys/Timer.cpp
     qpid/sys/TimerWarnings.cpp
     qpid/amqp_0_10/Codecs.cpp
     qpid/amqp/CharSequence.h
     qpid/amqp/CharSequence.cpp
     qpid/amqp/Decoder.h
     qpid/amqp/Decoder.cpp
     qpid/amqp/Descriptor.h
     qpid/amqp/Descriptor.cpp
     qpid/amqp/Encoder.h
     qpid/amqp/Encoder.cpp
     qpid/amqp/MapReader.h
     qpid/amqp/MapReader.cpp
     qpid/amqp/MessageEncoder.h
     qpid/amqp/MessageEncoder.cpp
     qpid/amqp/MessageId.h
     qpid/amqp/MessageId.cpp
     qpid/amqp/MessageReader.h
     qpid/amqp/MessageReader.cpp
     qpid/amqp/Reader.h
     qpid/amqp/Sasl.h
     qpid/amqp/Sasl.cpp
     qpid/amqp/SaslClient.h
     qpid/amqp/SaslClient.cpp
     qpid/amqp/SaslServer.h
     qpid/amqp/SaslServer.cpp
     qpid/messaging/amqp/Transport.h
     qpid/messaging/amqp/Transport.cpp
     qpid/messaging/amqp/TransportContext.h
)
add_msvc_version (qpidcommon library dll)

add_library (qpidcommon SHARED ${qpidcommon_SOURCES})
if (CLOCK_GETTIME_IN_RT)
  set (qpidcommon_platform_LIBS ${qpidcommon_platform_LIBS} rt)
endif (CLOCK_GETTIME_IN_RT)
target_link_libraries (qpidcommon qpidtypes
                       ${qpidcommon_platform_LIBS}
                       ${qpidcommon_sasl_lib})
set_target_properties (qpidcommon PROPERTIES
                       VERSION ${qpidcommon_version}
                       LINK_FLAGS ${NSS_LINK_FLAGS})
install (TARGETS qpidcommon
         DESTINATION ${QPID_INSTALL_LIBDIR}
         COMPONENT ${QPID_COMPONENT_COMMON})
install_pdb (qpidcommon ${QPID_COMPONENT_COMMON})

set(qpidtypes_SOURCES
  qpid/types/Exception.cpp
  qpid/types/Uuid.cpp
  qpid/types/Variant.cpp
  ${qpidtypes_platform_SOURCES}
)
add_msvc_version (qpidtypes library dll)
add_library(qpidtypes SHARED ${qpidtypes_SOURCES})
target_link_libraries(qpidtypes ${qpidtypes_platform_LIBS})
set_target_properties (qpidtypes PROPERTIES VERSION ${qpidtypes_version})
install(TARGETS qpidtypes
  DESTINATION ${QPID_INSTALL_LIBDIR}
  COMPONENT ${QPID_COMPONENT_COMMON})
install_pdb (qpidtypes ${QPID_COMPONENT_COMMON})

set (qpidmessaging_SOURCES_hidden
     ${amqpc_SOURCES}
     ${rgen_client_srcs}
     qpid/client/TCPConnector.cpp

     qpid/messaging/amqp/EncodedMessage.h
     qpid/messaging/amqp/EncodedMessage.cpp
     qpid/messaging/AddressParser.h
     qpid/messaging/AddressImpl.h
     qpid/messaging/ConnectionImpl.h
     qpid/messaging/ConnectionOptions.h
     qpid/messaging/ConnectionOptions.cpp
     qpid/messaging/MessageImpl.h
     qpid/messaging/MessageImpl.cpp
     qpid/messaging/ReceiverImpl.h
     qpid/messaging/ProtocolRegistry.cpp
     qpid/messaging/SessionImpl.h
     qpid/messaging/SenderImpl.h
     qpid/client/amqp0_10/AcceptTracker.h
     qpid/client/amqp0_10/AcceptTracker.cpp
     qpid/client/amqp0_10/AddressResolution.h
     qpid/client/amqp0_10/AddressResolution.cpp
     qpid/client/amqp0_10/ConnectionImpl.h
     qpid/client/amqp0_10/ConnectionImpl.cpp
     qpid/client/amqp0_10/IncomingMessages.h
     qpid/client/amqp0_10/IncomingMessages.cpp
     qpid/client/amqp0_10/MessageSink.h
     qpid/client/amqp0_10/MessageSource.h
     qpid/client/amqp0_10/OutgoingMessage.h
     qpid/client/amqp0_10/OutgoingMessage.cpp
     qpid/client/amqp0_10/ReceiverImpl.h
     qpid/client/amqp0_10/ReceiverImpl.cpp
     qpid/client/amqp0_10/SessionImpl.h
     qpid/client/amqp0_10/SessionImpl.cpp
     qpid/client/amqp0_10/SenderImpl.h
     qpid/client/amqp0_10/SenderImpl.cpp

     qpid/client/Bounds.cpp
     qpid/client/Completion.cpp
     qpid/client/Connection.cpp
     qpid/client/ConnectionHandler.cpp
     qpid/client/ConnectionImpl.cpp
     qpid/client/ConnectionSettings.cpp
     qpid/client/Connector.cpp
     qpid/client/Demux.cpp
     qpid/client/Future.cpp
     qpid/client/FutureCompletion.cpp
     qpid/client/FutureResult.cpp
     qpid/client/LoadPlugins.cpp
     qpid/client/Message.cpp
     qpid/client/MessageImpl.cpp
     qpid/client/Results.cpp
     qpid/client/SessionBase_0_10.cpp
     qpid/client/SessionImpl.cpp
     qpid/client/StateManager.cpp
)

set_source_files_properties(
     ${qpidmessaging_SOURCES_hidden}
     PROPERTIES
     COMPILE_FLAGS "${HIDE_SYMBOL_FLAGS}")

set (qpidmessaging_SOURCES
     ${qpidmessaging_platform_SOURCES}
     ${qpidmessaging_SOURCES_hidden}
     ${sslconnector_SOURCES}
     qpid/messaging/Address.cpp
     qpid/messaging/Connection.cpp
     qpid/messaging/Duration.cpp
     qpid/messaging/exceptions.cpp
     qpid/messaging/FailoverUpdates.cpp
     qpid/messaging/Message.cpp
     qpid/messaging/Receiver.cpp
     qpid/messaging/Session.cpp
     qpid/messaging/Sender.cpp
     # Not in the public interface, but qmf uses them
     qpid/messaging/AddressParser.cpp
)
add_msvc_version (qpidmessaging library dll)

add_library (qpidmessaging SHARED ${qpidmessaging_SOURCES})
target_link_libraries (qpidmessaging qpidcommon qpid-proton)
set_target_properties (qpidmessaging PROPERTIES VERSION ${qpidmessaging_version})
install (TARGETS qpidmessaging
         DESTINATION ${QPID_INSTALL_LIBDIR}
         COMPONENT ${QPID_COMPONENT_CLIENT})
install_pdb (qpidmessaging ${QPID_COMPONENT_CLIENT})

add_subdirectory(tests)

# Now create the config file from all the info learned above.
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/config.h)

# Support for pkg-config

# Compatible variable names used in the pkg config files also for autoconf
set (prefix ${CMAKE_INSTALL_PREFIX})
set (exec_prefix ${CMAKE_INSTALL_PREFIX})
set_absolute_install_path (libdir ${LIB_INSTALL_DIR})
set_absolute_install_path (includedir ${INCLUDE_INSTALL_DIR})
set (VERSION ${QPID_VERSION})

#add_custom_target(pkgconfig ALL echo DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/qpid.pc)
#add_dependencies(pkgconfig ${CMAKE_CURRENT_BINARY_DIR}/qmf2.pc)
configure_file(qpid.pc.in ${CMAKE_CURRENT_BINARY_DIR}/qpid.pc @ONLY)
install (FILES ${CMAKE_CURRENT_BINARY_DIR}/qpid.pc
         DESTINATION ${LIB_INSTALL_DIR}/pkgconfig
         COMPONENT ${QPID_COMPONENT_COMMON})
