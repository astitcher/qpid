/*
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 *
 */

#ifndef QPID_CLIENT_SOCKETCONNECTOR_H
#define QPID_CLIENT_SOCKETCONNECTOR_H

#include "qpid/sys/IntegerTypes.h"

#include <boost/shared_ptr.hpp>

namespace qpid {

namespace framing {
class ProtocolVersion;
}

namespace sys {
class Poller;
class Socket;
}

namespace client {

class Connector;
class Bounds;

Connector* createSocketConnector(boost::shared_ptr<sys::Poller> p,
                                 sys::Socket* s,
                                 framing::ProtocolVersion v,
                                 uint16_t m,
                                 Bounds* b);

}}   // namespace qpid::client

#endif  /* QPID_CLIENT_SOCKETCONNECTOR_H */
