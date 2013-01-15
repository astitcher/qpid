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

#include "qpid/sys/ProtocolFactory.h"

#include "qpid/broker/NameGenerator.h"
#include "qpid/log/Statement.h"
#include "qpid/sys/AsynchIOHandler.h"
#include "qpid/sys/AsynchIO.h"
#include "qpid/sys/Socket.h"
#include "qpid/sys/SocketAddress.h"
#include "qpid/sys/SystemInfo.h"

namespace qpid {
namespace sys {

namespace {
    // Expand list of Interfaces and addresses to a list of addresses
    std::vector<std::string> expandInterfaces(const std::vector<std::string>& interfaces) {
        std::vector<std::string> addresses;
        // If there are no specific interfaces listed use a single "" to listen on every interface
        if (interfaces.empty()) {
            addresses.push_back("");
            return addresses;
        }
        for (unsigned i = 0; i < interfaces.size(); ++i) {
            const std::string& interface = interfaces[i];
            if (!(SystemInfo::getInterfaceAddresses(interface, addresses))) {
                // We don't have an interface of that name -
                // Check for IPv6 ('[' ']') brackets and remove them
                // then pass to be looked up directly
                if (interface[0]=='[' && interface[interface.size()-1]==']') {
                    addresses.push_back(interface.substr(1, interface.size()-2));
                } else {
                    addresses.push_back(interface);
                }
            }
        }
        return addresses;
    }
}

uint16_t listenTo(const std::vector<std::string>& interfaces, const std::string& port, int backlog,
                  const SocketFactory& factory,
                  /*out*/boost::ptr_vector<Socket>& listeners)
{
    uint16_t listeningPort = 0;
    std::vector<std::string> addresses = expandInterfaces(interfaces);
    if (addresses.empty()) {
        // We specified some interfaces, but couldn't find addresses for them
        QPID_LOG(warning, "TCP/TCP6: No specified network interfaces found: Not Listening");
        return listeningPort;
    }
    
    for (unsigned i = 0; i<addresses.size(); ++i) {
        QPID_LOG(debug, "Using interface: " << addresses[i]);
        SocketAddress sa(addresses[i], port);
        
        // We must have at least one resolved address
        QPID_LOG(info, "Listening to: " << sa.asString())
        Socket* s = factory();
        uint16_t lport = s->listen(sa, backlog);
        QPID_LOG(debug, "Listened to: " << lport);
        listeners.push_back(s);
        
        listeningPort = lport;
        
        // Try any other resolved addresses
        while (sa.nextAddress()) {
            // Hack to ensure that all listening connections are on the same port
            sa.setAddrInfoPort(listeningPort);
            QPID_LOG(info, "Listening to: " << sa.asString())
            Socket* s = factory();
            uint16_t lport = s->listen(sa, backlog);
            QPID_LOG(debug, "Listened to: " << lport);
            listeners.push_back(s);
        }
    }
    return listeningPort;
}

namespace {
    void establishedCommon(
        AsynchIOHandler* async,
        boost::shared_ptr<Poller> poller, const qpid::broker::Broker::Options& opts, Timer* timer,
        const Socket& s)
    {
        if (opts.tcpNoDelay) {
            s.setTcpNoDelay();
            QPID_LOG(info, "Set TCP_NODELAY on connection to " << s.getPeerAddress());
        }
        
        AsynchIO* aio = AsynchIO::create
        (s,
         boost::bind(&AsynchIOHandler::readbuff, async, _1, _2),
         boost::bind(&AsynchIOHandler::eof, async, _1),
         boost::bind(&AsynchIOHandler::disconnect, async, _1),
         boost::bind(&AsynchIOHandler::closedSocket, async, _1, _2),
         boost::bind(&AsynchIOHandler::nobuffs, async, _1),
         boost::bind(&AsynchIOHandler::idle, async, _1));
        
        async->init(aio, *timer, opts.maxNegotiateTime);
        aio->start(poller);
    }
}

void establishedIncoming(
    boost::shared_ptr<Poller> poller, const qpid::broker::Broker::Options& opts, Timer* timer,
    const Socket& s, ConnectionCodec::Factory* f)
{
    AsynchIOHandler* async = new AsynchIOHandler(broker::QPID_NAME_PREFIX+s.getFullAddress(), f, false, opts.nodict);
    establishedCommon(async, poller, opts, timer, s);
}

void establishedOutgoing(
    boost::shared_ptr<Poller> poller, const qpid::broker::Broker::Options& opts, Timer* timer,
    const Socket& s, ConnectionCodec::Factory* f, const std::string& name)
{
    AsynchIOHandler* async = new AsynchIOHandler(name, f, true, opts.nodict);
    establishedCommon(async, poller, opts, timer, s);
}

void connectFailed(
    const Socket& s, int ec, const std::string& emsg,
    ProtocolFactory::ConnectFailedCallback failedCb)
{
    failedCb(ec, emsg);
    s.close();
    delete &s;
}

void connect(
    boost::shared_ptr<Poller> poller, const qpid::broker::Broker::Options& opts, Timer* timer,
    const SocketFactory& factory,
    const std::string& name,
    const std::string& host, const std::string& port,
    ConnectionCodec::Factory* fact,
    ProtocolFactory::ConnectFailedCallback failed)
{
    // Note that the following logic does not cause a memory leak.
    // The allocated Socket is freed either by the AsynchConnector
    // upon connection failure or by the AsynchIO upon connection
    // shutdown.  The allocated AsynchConnector frees itself when it
    // is no longer needed.
    Socket* socket = factory();
    try {
        AsynchConnector* c = AsynchConnector::create(
            *socket,
            host,
            port,
            boost::bind(&establishedOutgoing, poller, opts, timer, _1, fact, name),
                                                     boost::bind(&connectFailed, _1, _2, _3, failed));
        c->start(poller);
    } catch (std::exception&) {
        // TODO: Design question - should we do the error callback and also throw?
        int errCode = socket->getError();
        connectFailed(*socket, errCode, strError(errCode), failed);
        throw;
    }
}

}} // namespace qpid::sys
