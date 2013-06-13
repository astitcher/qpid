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

#include "qpid/client/SocketConnector.h"

#include "qpid/client/Connector.h"
#include "qpid/client/ConnectionImpl.h"
#include "qpid/client/ConnectionSettings.h"
#include "qpid/log/Statement.h"
#include "qpid/sys/Socket.h"

#include <boost/shared_ptr.hpp>

#include <memory>

namespace qpid {

namespace sys {
class Poller;
}

namespace client {

using namespace qpid::sys;

// Static constructor which registers connector here
namespace {
    Connector* create(boost::shared_ptr<Poller> p, framing::ProtocolVersion v, const ConnectionSettings& s, ConnectionImpl* c) {
        std::auto_ptr<Socket> socket(createSocket());
        s.configureSocket(*socket);
        std::auto_ptr<Connector> connector(createSocketConnector(p, socket.release(), v, s.maxFrameSize, c));
        QPID_LOG(debug, "TCPConnector created for " << v.toString());
        return connector.release();
    }

    struct StaticInit {
        StaticInit() {
            Connector::registerFactory("tcp", &create);
        };
    } init;
}

}} // namespace qpid::client
