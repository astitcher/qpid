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
#include "qpid/broker/PagedQueue.h"
#include "qpid/broker/QueueCursor.h"
#include "qpid/broker/Message.h"
#include "qpid/broker/amqp_0_10/MessageTransfer.h"
#include "qpid/log/Statement.h"
#include <string.h>
//TODO: move this into qpid::sys and allow for different implementations based on platform
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>

namespace qpid {
namespace broker {
namespace {

size_t encode(const Message& msg, char* data, size_t size)
{
    const amqp_0_10::MessageTransfer& transfer = amqp_0_10::MessageTransfer::get(msg);
    uint32_t encoded = transfer.encodedSize();
    uint32_t required = encoded  + 4/*content-size*/ +4/*sequence-number*/;
    if (required > size) return 0;
    qpid::framing::Buffer buffer(data, required);
    buffer.putLong(transfer.getContentSize());
    buffer.putLong(msg.getSequence());
    transfer.encode(buffer);
    assert(buffer.getPosition() == required);
    return required;
}

size_t decode(Message& msg, const char* data, size_t size)
{
    boost::intrusive_ptr<amqp_0_10::MessageTransfer> transfer(new amqp_0_10::MessageTransfer);
    qpid::framing::Buffer buffer(const_cast<char*>(data), size);
    uint32_t contentSize = buffer.getLong();
    uint32_t sequence = buffer.getLong();
    transfer->decodeHeader(buffer);
    transfer->decodeContent(buffer, contentSize);
    msg = Message(transfer, transfer);
    msg.setSequence(qpid::framing::SequenceNumber(sequence));
    return buffer.getPosition();
}

}

PagedQueue::PagedQueue(const std::string& p, int m) : path(p), pageSize(::sysconf(_SC_PAGE_SIZE)), maxLoaded(m), fd(0), offset(0)
{
    //open file
    int flags = O_CREAT | O_TRUNC | O_RDWR;
    fd = ::open(path.c_str(), flags, S_IRWXU);
    if (fd == -1) throw qpid::Exception(QPID_MSG("Failed to open file for paged queue " << path << ": " << qpid::sys::strError(errno) << " [flags=" << flags << "]"));
    QPID_LOG(debug, "PagedQueue[" << path << "]");
}

size_t PagedQueue::size()
{
    size_t total(0);
    for (Used::const_iterator i = used.begin(); i != used.end(); ++i) {
        total += i->second.available();
    }
    return total;
}

bool PagedQueue::deleted(const QueueCursor& cursor)
{
    if (cursor.valid) {
        Used::iterator page = findPage(cursor.position, false);
        if (page == used.end()) {
            return false;
        }
        page->second.deleted(cursor.position);
        if (page->second.empty()) {
            //move page to free list
            --loaded;
            page->second.clear();
            free.push_back(page->second);
            used.erase(page);
        }
        return true;
    } else {
        return false;
    }
}

void PagedQueue::publish(const Message& added)
{
    Used::reverse_iterator i = used.rbegin();
    if (i != used.rend()) {
        if (!i->second.isLoaded()) load(i->second);
        if (i->second.add(added)) return;
    }
    //used is empty or last page is full, need to add a new page
    newPage(added.getSequence()).add(added);
}

Message* PagedQueue::next(QueueCursor& cursor)
{
    Used::iterator i = used.begin();
    if (cursor.valid) {
        qpid::framing::SequenceNumber position(cursor.position);
        ++position;
        i = findPage(position, true);
        if (i == used.end() && !used.empty() && used.begin()->first > position) i = used.begin();
    }
    while (i != used.end()) {
        Message* m = i->second.next(version, cursor);
        QPID_LOG(debug, "PagedQueue::next(" << cursor.valid << ":" << cursor.position << "): " << m);
        if (m) return m;
        ++i;
    }
    QPID_LOG(debug, "PagedQueue::next(" << cursor.valid << ":" << cursor.position << ") returning 0 ");
    return 0;
}

Message* PagedQueue::release(const QueueCursor& cursor)
{
    if (cursor.valid) {
        Used::iterator i = findPage(cursor.position, true);
        if (i == used.end()) return 0;
        return i->second.release(cursor.position);
    } else {
        return 0;
    }
}

Message* PagedQueue::find(const framing::SequenceNumber& position, QueueCursor* cursor)
{
    Used::iterator i = findPage(position, true);
    if (i != used.end()) {
        Message* m = i->second.find(position);
        if (cursor) {
            cursor->setPosition(version, m ? m->getSequence() : position);
        }
        return m;
    } else {
        return 0;
    }
}

void PagedQueue::foreach(Functor)
{
    //TODO:
}

Message* PagedQueue::find(const QueueCursor& cursor)
{
    if (cursor.valid) return find(cursor.position, 0);
    else return 0;
}

PagedQueue::Page::Page(size_t s, size_t o) : size(s), offset(o), region(0), used(0)
{
    QPID_LOG(debug, "Created Page[" << offset << "], size=" << size);
}

void PagedQueue::Page::deleted(qpid::framing::SequenceNumber s)
{
    if (isLoaded()) {
        Message* message = find(s);
        assert(message);//could this ever legitimately be 0?
        message->setState(DELETED);
    }
    contents.remove(s);
    acquired.remove(s);
}

Message* PagedQueue::Page::release(qpid::framing::SequenceNumber s)
{
    Message* m = find(s);
    if (m) {
        m->setState(AVAILABLE);
    }
    acquired.remove(s);
    return m;
}

bool PagedQueue::Page::add(const Message& message)
{
    assert(region);
    assert (size >= used);
    size_t encoded = encode(message, region + used, size - used);
    QPID_LOG(debug, "Calling Page[" << offset << "]::add() used=" << used << ", size=" << size << ", encoded=" << encoded << ")");
    if (encoded) {
        used += encoded;
        messages.push_back(message);
        messages.back().setState(AVAILABLE);
        contents.add(message.getSequence());
        return true;
    } else {
        return false;
    }
}

bool PagedQueue::Page::empty() const
{
    return contents.empty();
}

bool PagedQueue::Page::isLoaded() const
{
    return region;
}

Message* PagedQueue::Page::next(uint32_t version, QueueCursor& cursor)
{
    if (messages.empty()) return 0;

    qpid::framing::SequenceNumber position;
    if (cursor.valid) {
        position = cursor.position + 1;
        if (position < messages.front().getSequence()) {
            position = messages.front().getSequence();
            cursor.setPosition(position, version);
        }
    } else {
        position = messages.front().getSequence();
        cursor.setPosition(position, version);
    }

    Message* m;
    do {
        m = find(position);
        if (m) cursor.setPosition(position, version);
        ++position;
    } while (m != 0 && !cursor.check(*m));
    return m;
    //if it is the first in the page, increment the hint count of the page
    //if it is the last in the page, decrement the hint count of the page
}

/**
 * Called before adding to the free list
 */
void PagedQueue::Page::clear()
{
    if (region) ::munmap(region, size);
    region = 0;
    used = 0;
    contents.clear();
    messages.clear();
}

size_t PagedQueue::Page::available() const
{
    return contents.size() - acquired.size();
}

Message* PagedQueue::Page::find(qpid::framing::SequenceNumber position)
{
    if (messages.size()) {
        assert(position >= messages.front().getSequence());

        size_t index = position - messages.front().getSequence();
        if (index < messages.size()) return &(messages[index]);
        else return 0;
    } else {
        //page is empty, is this an error?
        QPID_LOG(warning, "Could not find message at " << position << "; empty page.");
        return 0;
    }

    //if it is the first in the page, increment the hint count of the page
    //if it is the last in the page, decrement the hint count of the page
}

void PagedQueue::Page::load(int fd)
{
    QPID_LOG(debug, "Page[" << offset << "]::load" << " used=" << used << ", size=" << size);
    assert(region == 0);
    int protection = PROT_READ | PROT_WRITE;
    region = (char*) ::mmap(0, size, protection, MAP_SHARED, fd, offset);
    if (region == MAP_FAILED) {
        region = 0;
        throw qpid::Exception(QPID_MSG("Failed to map page into memory: " << qpid::sys::strError(errno)));
    }
    assert(region != 0);
    bool haveData = used > 0;
    used = 4;//first 4 bytes are the count
    if (haveData) {
        uint32_t count = *(reinterpret_cast<uint32_t*>(region));
        //decode messages into Page::messages
        for (size_t i = 0; i < count; ++i) {
            Message message;
            used += decode(message, region + used, size - used);
            if (!contents.contains(message.getSequence())) {
                message.setState(DELETED);
                QPID_LOG(debug, "Setting state to deleted for message loaded at " << message.getSequence());
            } else if (acquired.contains(message.getSequence())) {
                message.setState(ACQUIRED);
            } else {
                message.setState(AVAILABLE);
            }
            messages.push_back(message);
        }
        if (messages.size()) {
            QPID_LOG(debug, "Page[" << offset << "]::load " << messages.size() << " messages loaded from "
                     << messages.front().getSequence() << " to " << messages.back().getSequence());
        } else {
            QPID_LOG(debug, "Page[" << offset << "]::load no messages loaded");
        }
    }//else there is nothing we need to explicitly load, just needed to map region
}

void PagedQueue::Page::unload()
{
    if (messages.size()) {
        QPID_LOG(debug, "Page[" << offset << "]::unload " << messages.size() << " messages to unload from "
                 << messages.front().getSequence() << " to " << messages.back().getSequence());
    } else {
        QPID_LOG(debug, "Page[" << offset << "]::unload no messages to unload");
    }
    for (std::deque<Message>::iterator i = messages.begin(); i != messages.end(); ++i) {
        if (i->getState() == ACQUIRED) acquired.add(i->getSequence());
    }
    uint32_t count = messages.size();
    ::memcpy(region, &count, sizeof(count));
    ::msync(region, size, MS_ASYNC);//does this need to be synchronous?
    ::munmap(region, size);
    //remove messages from memory
    messages.clear();
    region = 0;
}

void PagedQueue::load(Page& page)
{
    //if needed, release another page
    if (loaded == maxLoaded) {
        //which page to select?
        Used::reverse_iterator i = used.rbegin();
        while (i != used.rend() && !i->second.isLoaded()) {
            ++i;
        }
        assert(i != used.rend());
        unload(i->second);
    }
    page.load(fd);
    ++loaded;
    QPID_LOG(debug, "PagedQueue[" << path << "] loaded page, " << loaded << " pages now loaded");
}

void PagedQueue::unload(Page& page)
{
    page.unload();
    --loaded;
    QPID_LOG(debug, "PagedQueue[" << path << "] unloaded page, " << loaded << " pages now loaded");
}


PagedQueue::Page& PagedQueue::newPage(qpid::framing::SequenceNumber id)
{
    if (loaded == maxLoaded) {
        //need to release a page from memory to make way for a new one

        //choose last one?
        Used::reverse_iterator i = used.rbegin();
        while (!i->second.isLoaded() && i != used.rend()) {
            ++i;
        }
        assert(i != used.rend());
        unload(i->second);
    }
    if (free.empty()) {
        //need to extend file and add some pages to the free list
        addPages(4/*arbitrary number, should this be config item?*/);
    }
    std::pair<Used::iterator, bool> result = used.insert(Used::value_type(id, free.front()));
    QPID_LOG(debug, "Added page for sequence starting from " << id);
    assert(result.second);
    free.pop_front();
    load(result.first->second);
    return result.first->second;
}

void PagedQueue::addPages(size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        free.push_back(Page(pageSize, offset));
        offset += pageSize;
        if ((::lseek(fd, offset - 1, SEEK_SET) == -1) || (::write(fd, "", 1) == -1)) {
            throw qpid::Exception(QPID_MSG("Failed to expand paged queue file " << path << ": " << qpid::sys::strError(errno)));
        }
    }
    QPID_LOG(debug, "Added " << count << " pages to free list; now have " << used.size() << " used, and " << free.size() << " free");
}

PagedQueue::Used::iterator PagedQueue::findPage(const QueueCursor& cursor)
{
    Used::iterator i = used.begin();
    if (cursor.valid) {
        i = findPage(cursor.position, true);
    } else if (i != used.end() && !i->second.isLoaded()) {
        load(i->second);
    }
    return i;
}

PagedQueue::Used::iterator PagedQueue::findPage(qpid::framing::SequenceNumber n, bool loadIfRequired)
{
    Used::iterator i = used.end();
    for (Used::iterator j = used.begin(); j != used.end() && j->first <= n; ++j) {
        i = j;
    }
    if (loadIfRequired && i != used.end() && !i->second.isLoaded()) {
        load(i->second);
    }
    return i;
}

}} // namespace qpid::broker
