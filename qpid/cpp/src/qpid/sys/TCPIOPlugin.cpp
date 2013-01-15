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

class AsynchIOProtocolFactory : public ProtocolFactory {
    boost::ptr_vector<Socket> listeners;
    boost::ptr_vector<AsynchAcceptor> acceptors;
    Timer& brokerTimer;
    const qpid::broker::Broker::Options& options;
    uint16_t listeningPort;

  public:
    AsynchIOProtocolFactory(const qpid::broker::Broker::Options& opts, Timer& timer, bool shouldListen);
    void accept(boost::shared_ptr<Poller>, ConnectionCodec::Factory*);
    void connect(boost::shared_ptr<Poller>, const std::string& name,
                 const std::string& host, const std::string& port,
                 ConnectionCodec::Factory*,
                 ConnectFailedCallback);

    uint16_t getPort() const;
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

            ProtocolFactory::shared_ptr protocolt(
                new AsynchIOProtocolFactory(opts, broker->getTimer(), shouldListen));

            if (shouldListen && protocolt->getPort()!=0 ) {
                QPID_LOG(notice, "Listening on TCP/TCP6 port " << protocolt->getPort());
            }

            broker->registerProtocolFactory("tcp", protocolt);
        }
    }
} tcpPlugin;

AsynchIOProtocolFactory::AsynchIOProtocolFactory(const qpid::broker::Broker::Options& opts, Timer& timer, bool shouldListen) :
    brokerTimer(timer),
    options(opts)
{
    if (!shouldListen) {
        listeningPort = boost::lexical_cast<uint16_t>(opts.port);
        return;
    }

    listeningPort =
        listenTo(opts.listenInterfaces, boost::lexical_cast<std::string>(opts.port), opts.connectionBacklog, 
                 &createSocket, listeners);
}

uint16_t AsynchIOProtocolFactory::getPort() const {
    return listeningPort; // Immutable no need for lock.
}

void AsynchIOProtocolFactory::accept(boost::shared_ptr<Poller> poller,
                                     ConnectionCodec::Factory* fact) {
    for (unsigned i = 0; i<listeners.size(); ++i) {
        acceptors.push_back(
            AsynchAcceptor::create(listeners[i],
                                   boost::bind(&establishedIncoming, poller, options, &brokerTimer, _1, fact)));
        acceptors[i].start(poller);
    }
}
                                     
void AsynchIOProtocolFactory::connect(
    boost::shared_ptr<Poller> poller,
    const std::string& name,
    const std::string& host, const std::string& port,
    ConnectionCodec::Factory* fact,
    ConnectFailedCallback failed)
{
    qpid::sys::connect(poller, options, &brokerTimer, &createSocket, name, host, port, fact, failed);
}

}} // namespace qpid::sys
