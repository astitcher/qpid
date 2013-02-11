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
#include "qpid/broker/SelectorToken.h"

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
 * //LiteralExactNumeric ::= Digit+
 * //LiteralApproxNumeric ::= ( Digit "." Digit* [ "E" LiteralExactNumeric ] ) |
 * //                         ( "." Digit+ [ "E" LiteralExactNumeric ] ) |
 * //                         ( Digit+ "E" LiteralExactNumeric )
 * //LiteralBool ::= "TRUE" | "FALSE"
 * //
 *
 * Literal ::= LiteralBool | LiteralString //| LiteralApproxNumeric | LiteralExactNumeric
 *
 * // Currently only simple string comparison expressions + IS NULL or IS NOT NULL
 * //OpsLogical ::= "AND" | "OR"
 * OpsComparison ::= "=" | "<>" // | ">" | ">=" | "<" | "<="
 *
 * BoolExpression ::= (Identifier | Literal) OpsComparison (Identifier | Literal) |
 *                    Identifier "IS" "NULL" |
 *                    Identifier "IS "NOT" "NULL"
 */

#include <string>
#include <boost/scoped_ptr.hpp>

namespace qpid {
namespace broker {

class Expression;

using std::string;

class BoolExpression {
public:
    virtual bool eval(const SelectorEnv&) const = 0;

    static BoolExpression* parse(string::const_iterator& s, string::const_iterator& e);
    static BoolExpression* parse(const string& exp);
};

class EqualityOperator {
public:
    virtual bool eval(Expression&, Expression&, const SelectorEnv&) const = 0;
};

template <typename T>
class UnaryBooleanOperator {
public:
    virtual bool eval(T&, const SelectorEnv&) const = 0;
};

class EqualityExpression : public BoolExpression {
    friend class BoolExpression;

    EqualityOperator* op;
    boost::scoped_ptr<Expression> e1;
    boost::scoped_ptr<Expression> e2;

public:
    EqualityExpression(EqualityOperator* o, Expression* e, Expression* e_):
        op(o),
        e1(e),
        e2(e_)
    {}

    virtual bool eval(const SelectorEnv& env) const {
        return op->eval(*e1, *e2, env);
    }
};

template <typename T>
class UnaryBooleanExpression : public BoolExpression {
    friend class BoolExpression;

    UnaryBooleanOperator<T>* op;
    boost::scoped_ptr<T> e1;

public:
    UnaryBooleanExpression(UnaryBooleanOperator<T>* o, T* e) :
        op(o),
        e1(e)
    {}
    virtual bool eval(const SelectorEnv& env) const {
        return op->eval(*e1, env);
    }
};

class Expression {
public:
    virtual string eval(const SelectorEnv&) const = 0;

    static Expression* parse(string::const_iterator& s, string::const_iterator& e);
};

// Some conditional operators...

// "="
class Eq : public EqualityOperator {
    bool eval(Expression& e1, Expression& e2, const SelectorEnv& env) const {
        return e1.eval(env) == e2.eval(env);
    }
};

// "<>"
class Neq : public EqualityOperator {
    bool eval(Expression& e1, Expression& e2, const SelectorEnv& env) const {
        return e1.eval(env) != e2.eval(env);
    }
};

// Some expression types...

class Literal : public Expression {
    friend class Expression;

    string value;

public:
    Literal(const string& v) :
        value(v)
    {}

    string eval(const SelectorEnv&) const {
        return value;
    }
};

class Identifier : public Expression {
    friend class Expression;

    string identifier;

public:
    Identifier(const string& i) :
        identifier(i)
    {}
    
    string eval(const SelectorEnv& env) const {
        return env.value(identifier);
    }
    
    bool present(const SelectorEnv& env) const {
        return env.present(identifier);
    }
};

////////////////////////////////////////////////////

// "IS NULL"
class IsNull : public UnaryBooleanOperator<Identifier> {
    bool eval(Identifier& i, const SelectorEnv& env) const {
        return i.present(env);
    }
};

// "IS NOT NULL"
class IsNonNull : public UnaryBooleanOperator<Identifier> {
    bool eval(Identifier& i, const SelectorEnv& env) const {
        return !i.present(env);
    }
};

Eq eq;
Neq neq;
IsNull isNull;
IsNonNull isNonNull;

////////////////////////////////////////////////////

// Parsers always take string const_iterators to mark the beginning and end of the string being parsed
// if the parse is successful then the start iterator is advanced, if the parse fails then the start
// iterator is unchanged.

// Top level parser
BoolExpression* BoolExpression::parse(const string& exp)
{
    string::const_iterator s = exp.begin();
    string::const_iterator e = exp.end();
    BoolExpression* b = parse(s, e);
    if (nextToken(s, e).type != T_EOS) throw std::range_error("Illegal selector: too much input");
    return b;
}

BoolExpression* BoolExpression::parse(string::const_iterator& s, string::const_iterator& e)
{
    Token t;
    t = nextToken(s, e);

    Token op;
    op = nextToken(s,e);

    Expression* e1 = 0;
    switch (t.type) {
    case T_IDENTIFIER: {
        Identifier* i = new Identifier(t.val);
        
        // Check for "IS NULL" and "IS NOT NULL"
        if ( op.type==T_IS ) {
            // The rest must be T_NULL or T_NOT, T_NULL
            t = nextToken(s, e);
            switch (t.type) {
            case T_NULL:
                return new UnaryBooleanExpression<Identifier>(&isNull, i);
            case T_NOT:
                if ( nextToken(s, e).type == T_NULL)
                    return new UnaryBooleanExpression<Identifier>(&isNonNull, i);
            default:
                throw std::range_error("Illegal selector: illegal IS syntax");
                break;
            }
        }
        e1 = i;
        break;
    }
    case T_STRING:
        e1 = new Literal(t.val);
        break;
    default:
        throw std::range_error("Illegal selector: unexpected expression1");
    }
    
    if (op.type != T_OPERATOR)
        throw std::range_error("Illegal selector: unexpected operator");
    
    t = nextToken(s, e);
    Expression* e2 = 0;
    switch (t.type) {
    case T_IDENTIFIER:
        e1 = new Identifier(t.val);
        break;
    case T_STRING:
        e1 = new Literal(t.val);
        break;
    default:
        throw std::range_error("Illegal selector: unexpected expression2");
    }
    if (op.val == "=") return new EqualityExpression(&eq, e1, e2);
    if (op.val == "<>") return new EqualityExpression(&neq, e1, e2);

    throw std::range_error("Illegal selector: unknown operator");
}

////////////////////////////////////////////////////

Selector::Selector(const string& e) :
    parse(BoolExpression::parse(e)),
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
