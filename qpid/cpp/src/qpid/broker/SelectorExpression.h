#ifndef QPID_BROKER_SELECTOREXPRESSION_H
#define QPID_BROKER_SELECTOREXPRESSION_H

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

#include "qpid/sys/IntegerTypes.h"

#include <iosfwd>
#include <string>

namespace qpid {
namespace broker {

class SelectorEnv;
class Tokeniser;

class Value {
public:
    union {
        bool        b;
        uint64_t    i;
        double      x;
        std::string*     s;
    };
    enum {
        T_BOOL,
        T_STRING,
        T_EXACT,
        T_INEXACT
    } type;

    Value(const std::string& s0) :
        s(new std::string(s0)),
        type(T_STRING)
    {}

    Value(const uint64_t i0) :
        i(i0),
        type(T_EXACT)
    {}

    Value(const double x0) :
        x(x0),
        type(T_INEXACT)
    {}

    Value(bool b0) :
        b(b0)
    {}

    ~Value() {
        if ( type==T_STRING ) delete s;
    }
};

class Expression {
public:
    virtual ~Expression() {}
    virtual void repr(std::ostream&) const = 0;
    virtual const Value* eval(const SelectorEnv&) const = 0;
};

class BoolExpression {
public:
    virtual ~BoolExpression() {};
    virtual void repr(std::ostream&) const = 0;
    virtual bool eval(const SelectorEnv&) const = 0;
};

BoolExpression* parseTopBoolExpression(const std::string& exp);
BoolExpression* parseBoolExpression(Tokeniser&);
BoolExpression* parseOrExpression(Tokeniser&);
BoolExpression* parseAndExpression(Tokeniser&);
BoolExpression* parseNotExpression(Tokeniser&);
BoolExpression* parseEqualityExpression(Tokeniser&);
Expression* parsePrimaryExpression(Tokeniser&);

}}

#endif
