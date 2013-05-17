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
#
# SSL/TLS CMake fragment, to be included in CMakeLists.txt
# 

# Optional SSL/TLS support. Requires Netscape Portable Runtime on Linux.

include(FindPkgConfig)

# According to some cmake docs this is not a reliable way to detect
# pkg-configed libraries, but it's no worse than what we did under
# autotools
pkg_check_modules(NSS nss)

set (ssl_default ${ssl_force})
if (CMAKE_SYSTEM_NAME STREQUAL Windows)
  set (ssl_default ON)
else (CMAKE_SYSTEM_NAME STREQUAL Windows)
  if (NSS_FOUND)
    set (ssl_default ON)
  endif (NSS_FOUND)
endif (CMAKE_SYSTEM_NAME STREQUAL Windows)

option(BUILD_SSL "Build with support for SSL" ${ssl_default})
if (BUILD_SSL)
    if (CMAKE_SYSTEM_NAME STREQUAL Windows)
      set (sslcommon_SOURCES
          qpid/sys/windows/SslAsynchIO.cpp
      )
      set (ssl_SOURCES
          qpid/broker/windows/SslProtocolFactory.cpp
      )
      set (sslconnector_SOURCES
          qpid/client/windows/SslConnector.cpp
      )
      set (windows_ssl_libs Secur32.lib)
      set (windows_ssl_server_libs Crypt32.lib)
    else (CMAKE_SYSTEM_NAME STREQUAL Windows)

    if (NOT NSS_FOUND)
      message(FATAL_ERROR "nss/nspr not found, required for ssl support")
    endif (NOT NSS_FOUND)

    foreach(f ${NSS_CFLAGS})
      set (NSS_COMPILE_FLAGS "${NSS_COMPILE_FLAGS} ${f}")
    endforeach(f)

    foreach(f ${NSS_LDFLAGS})
      set (NSS_LINK_FLAGS "${NSS_LINK_FLAGS} ${f}")
    endforeach(f)

    set (sslcommon_SOURCES
        qpid/sys/ssl/check.h
        qpid/sys/ssl/check.cpp
        qpid/sys/ssl/util.h
        qpid/sys/ssl/util.cpp
        qpid/sys/ssl/SslSocket.h
        qpid/sys/ssl/SslSocket.cpp
    )

    set (ssl_SOURCES
        qpid/sys/SslPlugin.cpp
    )

    set (sslconnector_SOURCES
        qpid/client/SslConnector.cpp
        qpid/messaging/amqp/SslTransport.cpp
    )

    set_source_files_properties (
        ${ssl_SOURCES}
        ${sslconnector_SOURCES}
        PROPERTIES
        COMPILE_FLAGS "${NSS_COMPILE_FLAGS} ${HIDE_SYMBOL_FLAGS}"
    )

    set_source_files_properties (
        ${sslcommon_SOURCES}
        PROPERTIES
        COMPILE_FLAGS "${NSS_COMPILE_FLAGS}"
    )
  endif (CMAKE_SYSTEM_NAME STREQUAL Windows)

endif (BUILD_SSL)
