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
#include "qpid/client/Connector.h"

#include "qpid/client/Bounds.h"
#include "qpid/framing/AMQFrame.h"
#include "qpid/framing/InitiationHandler.h"
#include "qpid/framing/InputHandler.h"
#include "qpid/log/Statement.h"
#include "qpid/sys/AsynchIO.h"
#include "qpid/sys/Codec.h"
#include "qpid/sys/SecurityLayer.h"
#include "qpid/sys/SecuritySettings.h"
#include "qpid/sys/ShutdownHandler.h"
#include "qpid/sys/Socket.h"

#include <deque>
#include <iostream>

#include <boost/bind.hpp>
#include <boost/format.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>

namespace qpid {

namespace sys {
class Poller;
}

namespace client {

using namespace qpid::sys;
using namespace qpid::framing;
using boost::format;
using boost::str;

class SocketConnector : public Connector, public Codec
{
    typedef std::deque<framing::AMQFrame> Frames;

    const uint16_t maxFrameSize;

    sys::Mutex lock;
    Frames frames;
    size_t lastEof; // Position after last EOF in frames
    uint64_t currentSize;
    Bounds* bounds;

    framing::ProtocolVersion version;
    bool initiated;
    bool closed;

    sys::ShutdownHandler* shutdownHandler;
    framing::InputHandler* input;

    boost::scoped_ptr<sys::Socket> socket;

    sys::AsynchConnector* connector;
    sys::AsynchIO* aio;
    std::string identifier;
    boost::shared_ptr<Poller> poller;
    std::auto_ptr<qpid::sys::SecurityLayer> securityLayer;
    sys::Codec* codec;
    SecuritySettings securitySettings;

    ~SocketConnector();

    void readbuff(AsynchIO&, AsynchIOBufferBase*);
    void writebuff(AsynchIO&);
    void writeDataBlock(const framing::AMQDataBlock& data);
    void eof(AsynchIO&);
    void disconnected(AsynchIO&);

    void connect(const std::string& host, const std::string& port);
    void connected(const sys::Socket&);
    void start(sys::AsynchIO* aio_);
    void initAmqp();
    void connectFailed(const std::string& msg);

    void close();
    void handle(framing::AMQFrame& frame);
    void abort();
    void connectAborted();

    void setInputHandler(framing::InputHandler* handler);
    void setShutdownHandler(sys::ShutdownHandler* handler);
    const std::string& getIdentifier() const;
    void activateSecurityLayer(std::auto_ptr<qpid::sys::SecurityLayer>);
    const SecuritySettings* getSecuritySettings();
    void socketClosed(AsynchIO&, const Socket&);

    size_t decode(const char* buffer, size_t size);
    size_t encode(char* buffer, size_t size);
    bool canEncode();

public:
    SocketConnector(boost::shared_ptr<Poller> p,
                    Socket*,
                    framing::ProtocolVersion pVersion,
                    uint16_t,
                    Bounds*);
};

SocketConnector::SocketConnector(boost::shared_ptr<Poller> p,
                     Socket* s,
                     ProtocolVersion ver,
                     uint16_t m,
                     Bounds* cimpl)
    : maxFrameSize(m),
      lastEof(0),
      currentSize(0),
      bounds(cimpl),
      version(ver),
      initiated(false),
      closed(true),
      shutdownHandler(0),
      input(0),
      socket(s),
      connector(0),
      aio(0),
      poller(p),
      codec(this)
{
}

SocketConnector::~SocketConnector() {
    close();
}

void SocketConnector::connect(const std::string& host, const std::string& port) {
    Mutex::ScopedLock l(lock);
    assert(closed);
    connector = AsynchConnector::create(
        *socket,
        host, port,
        boost::bind(&SocketConnector::connected, this, _1),
        boost::bind(&SocketConnector::connectFailed, this, _3));
    closed = false;

    connector->start(poller);
}

void SocketConnector::connected(const Socket&) {
    connector = 0;
    aio = AsynchIO::create(*socket,
                           boost::bind(&SocketConnector::readbuff, this, _1, _2),
                           boost::bind(&SocketConnector::eof, this, _1),
                           boost::bind(&SocketConnector::disconnected, this, _1),
                           boost::bind(&SocketConnector::socketClosed, this, _1, _2),
                           0, // nobuffs
                           boost::bind(&SocketConnector::writebuff, this, _1));

    start(aio);
    initAmqp();
    aio->start(poller);
}

void SocketConnector::start(sys::AsynchIO* aio_) {
    aio = aio_;

    aio->createBuffers(maxFrameSize);

    identifier = str(format("[%1%]") % socket->getFullAddress());
}

void SocketConnector::initAmqp() {
    ProtocolInitiation init(version);
    writeDataBlock(init);
}

void SocketConnector::connectFailed(const std::string& msg) {
    connector = 0;
    QPID_LOG(warning, "Connect failed: " << msg);
    socket->close();
    if (!closed)
        closed = true;
    if (shutdownHandler)
        shutdownHandler->shutdown();
}

void SocketConnector::close() {
    Mutex::ScopedLock l(lock);
    if (!closed) {
        closed = true;
        if (aio)
            aio->queueWriteClose();
    }
}

void SocketConnector::socketClosed(AsynchIO&, const Socket&) {
    if (aio)
        aio->queueForDeletion();
    if (shutdownHandler)
        shutdownHandler->shutdown();
}

void SocketConnector::connectAborted() {
    connector->stop();
    connectFailed("Connection timedout");
}

void SocketConnector::abort() {
    // Can't abort a closed connection
    if (!closed) {
        if (aio) {
            // Established connection
            aio->requestCallback(boost::bind(&SocketConnector::disconnected, this, _1));
        } else if (connector) {
            // We're still connecting
            connector->requestCallback(boost::bind(&SocketConnector::connectAborted, this));
        }
    }
}

void SocketConnector::setInputHandler(InputHandler* handler){
    input = handler;
}

void SocketConnector::setShutdownHandler(ShutdownHandler* handler){
    shutdownHandler = handler;
}

const std::string& SocketConnector::getIdentifier() const {
    return identifier;
}

void SocketConnector::handle(AMQFrame& frame) {
    bool notifyWrite = false;
    {
    Mutex::ScopedLock l(lock);
    frames.push_back(frame);
    //only ask to write if this is the end of a frameset or if we
    //already have a buffers worth of data
    currentSize += frame.encodedSize();
    if (frame.getEof()) {
        lastEof = frames.size();
        notifyWrite = true;
    } else {
        notifyWrite = (currentSize >= maxFrameSize);
    }
    /*
      NOTE: Moving the following line into this mutex block
            is a workaround for BZ 570168, in which the test
            testConcurrentSenders causes a hang about 1.5%
            of the time.  ( To see the hang much more frequently
            leave this line out of the mutex block, and put a
            small usleep just before it.)

            TODO mgoulish - fix the underlying cause and then
                            move this call back outside the mutex.
    */
    if (notifyWrite && !closed) aio->notifyPendingWrite();
    }
}

void SocketConnector::writebuff(AsynchIO& /*aio*/)
{
    // It's possible to be disconnected and be writable
    if (closed)
        return;

    if (!codec->canEncode()) {
        return;
    }

    AsynchIOBufferBase* buffer = aio->getQueuedBuffer();
    if (buffer) {

        size_t encoded = codec->encode(buffer->bytes, buffer->byteCount);

        buffer->dataStart = 0;
        buffer->dataCount = encoded;
        aio->queueWrite(buffer);
    }
}

// Called in IO thread.
bool SocketConnector::canEncode()
{
    Mutex::ScopedLock l(lock);
    //have at least one full frameset or a whole buffers worth of data
    return lastEof || currentSize >= maxFrameSize;
}

// Called in IO thread.
size_t SocketConnector::encode(char* buffer, size_t size)
{
    framing::Buffer out(buffer, size);
    size_t bytesWritten(0);
    {
        Mutex::ScopedLock l(lock);
        while (!frames.empty() && out.available() >= frames.front().encodedSize() ) {
            frames.front().encode(out);
            QPID_LOG(trace, "SENT [" << identifier << "]: " << frames.front());
            frames.pop_front();
            if (lastEof) --lastEof;
        }
        bytesWritten = size - out.available();
        currentSize -= bytesWritten;
    }
    if (bounds) bounds->reduce(bytesWritten);
    return bytesWritten;
}

void SocketConnector::readbuff(AsynchIO& aio, AsynchIOBufferBase* buff)
{
    int32_t decoded = codec->decode(buff->bytes+buff->dataStart, buff->dataCount);
    // TODO: unreading needs to go away, and when we can cope
    // with multiple sub-buffers in the general buffer scheme, it will
    if (decoded < buff->dataCount) {
        // Adjust buffer for used bytes and then "unread them"
        buff->dataStart += decoded;
        buff->dataCount -= decoded;
        aio.unread(buff);
    } else {
        // Give whole buffer back to aio subsystem
        aio.queueReadBuffer(buff);
    }
}

size_t SocketConnector::decode(const char* buffer, size_t size)
{
    framing::Buffer in(const_cast<char*>(buffer), size);
    if (!initiated) {
        framing::ProtocolInitiation protocolInit;
        if (protocolInit.decode(in)) {
            QPID_LOG(debug, "RECV [" << identifier << "]: INIT(" << protocolInit << ")");
            if(!(protocolInit==version)){
                throw Exception(QPID_MSG("Unsupported version: " << protocolInit
                                         << " supported version " << version));
            }
        }
        initiated = true;
    }
    AMQFrame frame;
    while(frame.decode(in)){
        QPID_LOG(trace, "RECV [" << identifier << "]: " << frame);
        input->received(frame);
    }
    return size - in.available();
}

void SocketConnector::writeDataBlock(const AMQDataBlock& data) {
    AsynchIOBufferBase* buff = aio->getQueuedBuffer();
    assert(buff);
    framing::Buffer out(buff->bytes, buff->byteCount);
    data.encode(out);
    buff->dataCount = data.encodedSize();
    aio->queueWrite(buff);
}

void SocketConnector::eof(AsynchIO&) {
    close();
}

void SocketConnector::disconnected(AsynchIO&) {
    close();
    socketClosed(*aio, *socket);
}

void SocketConnector::activateSecurityLayer(std::auto_ptr<qpid::sys::SecurityLayer> sl)
{
    securityLayer = sl;
    securityLayer->init(this);
    codec = securityLayer.get();
}

const SecuritySettings* SocketConnector::getSecuritySettings()
{
    if ( socket->getKeyLen()==0 ) return 0;

    securitySettings.ssf = socket->getKeyLen();
    securitySettings.authid = "dummy";//set to non-empty string to enable external authentication
    return &securitySettings;
}

Connector* createSocketConnector(boost::shared_ptr<Poller> p,
                                 Socket* s,
                                 framing::ProtocolVersion v,
                                 uint16_t m,
                                 Bounds* b)
{
    return new SocketConnector(p, s, v, m, b);
}

}} // namespace qpid::client
