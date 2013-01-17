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

#include "qpid/Plugin.h"
#include "qpid/broker/Broker.h"
#include "qpid/log/Statement.h"
#include "qpid/sys/AsynchIO.h"
#include "qpid/sys/Socket.h"

#include <boost/bind.hpp>
#include <boost/ptr_container/ptr_vector.hpp>

namespace qpid {
namespace sys {

class Timer;

class AsynchIOAcceptor : public TransportAcceptor {
    SocketAcceptor socketListener;

  public:
    AsynchIOAcceptor(const qpid::broker::Broker::Options& opts, Timer& timer);
    uint16_t listen(const std::vector<std::string>& interfaces, const std::string& port, int backlog);
    void accept(boost::shared_ptr<Poller>, ConnectionCodec::Factory*);
};

class AsynchIOConnector : public TransportConnectorFactory {
    SocketConnector socketConnector;
    
public:
    AsynchIOConnector(const qpid::broker::Broker::Options& opts, Timer& timer);
    void connect(boost::shared_ptr<Poller>, const std::string& name,
                 const std::string& host, const std::string& port,
                 ConnectionCodec::Factory*,
                 ConnectFailedCallback);
};

static bool sslMultiplexEnabled(void)
{
    Options o;
    Plugin::addOptions(o);

    if (o.find_nothrow("ssl-multiplex", false)) {
        // This option is added by the SSL plugin when the SSL port
        // is configured to be the same as the main port.
        QPID_LOG(notice, "SSL multiplexing enabled");
        return true;
    }
    return false;
}

// Static instance to initialise plugin
static class TCPIOPlugin : public Plugin {
    void earlyInitialize(Target&) {
    }

    void initialize(Target& target) {
        broker::Broker* broker = dynamic_cast<broker::Broker*>(&target);
        // Only provide to a Broker
        if (broker) {
            const broker::Broker::Options& opts = broker->getOptions();

            // Check for SSL on the same port
            bool shouldListen = !sslMultiplexEnabled();

            uint16_t port = opts.port;
            TransportAcceptor::shared_ptr ta;
            if (shouldListen) {
                AsynchIOAcceptor* aa = new AsynchIOAcceptor(opts, broker->getTimer());
                ta.reset(aa);
                port = aa->listen(opts.listenInterfaces, boost::lexical_cast<std::string>(opts.port), opts.connectionBacklog);
                if ( port!=0 ) {
                    QPID_LOG(notice, "Listening on TCP/TCP6 port " << port);
                }
            }

            TransportConnectorFactory::shared_ptr tc(new AsynchIOConnector(opts, broker->getTimer()));
            
            broker->registerTransport("tcp", ta, tc, port);
        }
    }
} tcpPlugin;

AsynchIOAcceptor::AsynchIOAcceptor(const qpid::broker::Broker::Options& opts, Timer& timer) :
    socketListener(opts.tcpNoDelay, false, opts.maxNegotiateTime, timer)
{
}

uint16_t AsynchIOAcceptor::listen(const std::vector< std::string >& interfaces, const std::string& port, int backlog)
{
    return socketListener.listen(interfaces, port, backlog, &createSocket);
}

void AsynchIOAcceptor::accept(boost::shared_ptr<Poller> poller,
                                     ConnectionCodec::Factory* fact) {
    socketListener.accept(poller, fact);
}
                                     
AsynchIOConnector::AsynchIOConnector(const qpid::broker::Broker::Options& opts, Timer& timer) :
    socketConnector(opts.tcpNoDelay, false, opts.maxNegotiateTime, timer)
{
}

void AsynchIOConnector::connect(
    boost::shared_ptr<Poller> poller,
    const std::string& name,
    const std::string& host, const std::string& port,
    ConnectionCodec::Factory* fact,
    ConnectFailedCallback failed)
{
    socketConnector.connect(poller, name, host, port, fact, failed, &createSocket);
}

}} // namespace qpid::sys
