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

#include "qpid/broker/Selector.h"

#include "qpid/broker/Message.h"

#include <boost/make_shared.hpp>

/*
 * Syntax for JMS style selector expressions (informal):
 *
 * Alpha ::= "a".."z" | "A".."Z"
 * Digit ::= "0".."9"
 * IdentifierInitial ::= Alpha | "_" | "$"
 * IdentifierPart ::= IdentifierInitial | Digit
 * Identifier ::= IdentifierInitial IdentifierPart*
 * Constraint : Identifier NOT IN ("NULL", "TRUE", "FALSE", "NOT", "AND", "OR", "BETWEEN", "LIKE", "IN", "IS") // Case insensitive
 *
 * LiteralString ::= ("'" ~[']* "'")+ // Repeats to cope with embedded single quote
 *
 * // Currently no numerics at all
 * //Sign ::= "-" | "+"
 * //LiteralExactNumeric ::= [Sign] Digit+
 * //LiteralApproxNumeric ::= ( [Sign] Digit+ "." Digit* [ "E" LiteralExactNumeric ] ) |
 * //                         ( [Sign] Digit+ "E" LiteralExactNumeric )
 * //LiteralBool ::= "TRUE" | "FALSE"
 * //
 *
 * Literal ::= LiteralBool | LiteralString //| LiteralApproxNumeric | LiteralExactNumeric
 *
 * // Currently only simple string comparison expressions + IS NULL or IS NOT NULL
 * //OpsLogical ::= "AND" | "OR"
 * OpsComparison ::= "=" | "<>" // | ">" | ">=" | "<" | "<="
 *
 * Comparison ::= (Identifier | Literal) OpsComparison (Identifier | Literal) |
 *                Identifier "IS" "NULL" |
 *                Identifier "IS "NOT" "NULL"
 */

#include <string>
#include <boost/scoped_ptr.hpp>

namespace qpid {
namespace broker {

class Expression;

using std::string;

class ConditionalExpression {
public:
    virtual bool eval() = 0;
};

class BinCondOperator {
public:
    virtual bool eval(Expression&, Expression&) = 0;
};

class UnCondOperator {
public:
    virtual bool eval(Expression&) = 0;
};

class BinaryConditionalExpression {
    boost::scoped_ptr<BinCondOperator> op;
    boost::scoped_ptr<Expression> e1;
    boost::scoped_ptr<Expression> e2;

public:
    virtual bool eval() {
        return op->eval(*e1, *e2);
    }
};

class UnaryConditionalExpression {
    boost::scoped_ptr<UnCondOperator> op;
    boost::scoped_ptr<Expression> e1;

public:
    virtual bool eval() {
        return op->eval(*e1);
    }
};

class Expression {
public:
    virtual std::string eval() = 0;
};

// Some conditional operators...

// "="
class Eq : public BinCondOperator {
    bool eval(Expression& e1, Expression& e2) {
        return e1.eval() == e2.eval();
    }
};

// "<>"
class Neq : public BinCondOperator {
    bool eval(Expression& e1, Expression& e2) {
        return e1.eval() != e2.eval();
    }
};

// "IS NULL"
class Null : public UnCondOperator {
    bool eval(Expression& e) {
        return e.eval().empty(); // TODO: This is wrong test!!
    }
};

// "IS NOT NULL"
class NonNull : public UnCondOperator {
    bool eval(Expression& e) {
        return !e.eval().empty(); // TODO: This is wrong test!!
    }
};

// Some expression types...

////////////////////////////////////////////////////

class SelectorParseState {
    const std::string& expression;

public:
    SelectorParseState(const std::string& e) :
        expression(e)
    {
    }
};

Selector::Selector(const string& e) :
    parseState(new SelectorParseState(e)),
    expression(e)
{
}

Selector::~Selector()
{
}

bool Selector::eval(const SelectorEnv& env)
{
    // Simple test - return true if expression is a non-empty property
    return env.present(expression);
}

bool Selector::filter(const Message& msg)
{
    return eval(MessageSelectorEnv(msg));
}

MessageSelectorEnv::MessageSelectorEnv(const Message& m) :
    msg(m)
{
}

bool MessageSelectorEnv::present(const string& identifier) const
{
    // If the value we get is void then most likely the property wasn't present
    return !msg.getProperty(identifier).isVoid();
}

string MessageSelectorEnv::value(const string& identifier) const
{
    // Just return property as string
    return msg.getPropertyAsString(identifier);
}


namespace {
const boost::shared_ptr<Selector> NULL_SELECTOR = boost::shared_ptr<Selector>();
}

boost::shared_ptr<Selector> returnSelector(const string& e)
{
    if (e.empty()) return NULL_SELECTOR;
    return boost::make_shared<Selector>(e);
}


}}
