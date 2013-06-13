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

#include "config.h"

#include "qpid/client/Connector.h"
#include "qpid/client/ConnectionImpl.h"
#include "qpid/log/Statement.h"
#include "qpid/sys/ssl/util.h"
#include "qpid/sys/ssl/SslSocket.h"

namespace qpid {

namespace sys {
class Poller;
}

namespace client {

using namespace qpid::sys;
using namespace qpid::sys::ssl;

// Static constructor which registers connector here
namespace {
    Connector* create(boost::shared_ptr<Poller> p, framing::ProtocolVersion v, const ConnectionSettings& s, ConnectionImpl* c) {
        QPID_LOG(debug, "SslConnector created for " << v.toString());

        if (s.sslCertName != "") {
            QPID_LOG(debug, "ssl-cert-name = " << s.sslCertName);
        }
        return createSocketConnector(p, new SslSocket(s.sslCertName), v, s.maxFrameSize, c);
    }

    struct StaticInit {
        StaticInit() {
            try {
                CommonOptions common("", "", QPIDC_CONF_FILE);
                SslOptions options;
                common.parse(0, 0, common.clientConfig, true);
                options.parse (0, 0, common.clientConfig, true);
                if (options.certDbPath.empty()) {
                    QPID_LOG(info, "SSL connector not enabled, you must set QPID_SSL_CERT_DB to enable it.");
                } else {
                    initNSS(options);
                    Connector::registerFactory("ssl", &create);
                }
            } catch (const std::exception& e) {
                QPID_LOG(error, "Failed to initialise SSL connector: " << e.what());
            }
        };

        ~StaticInit() { shutdownNSS(); }
    } init;
}

}}
