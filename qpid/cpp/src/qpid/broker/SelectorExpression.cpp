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

#include "qpid/broker/SelectorExpression.h"

#include "qpid/broker/Selector.h"
#include "qpid/broker/SelectorToken.h"

#include <string>
#include <boost/scoped_ptr.hpp>

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
 * EqOps ::= "=" | "<>"
 *
 * ComparisonOps ::= EqOps | ">" | ">=" | "<" | "<="
 *
 * BoolExpression ::= OrExpression
 *
 * OrExpression ::= AndExpression  ( "OR" AndExpression )*
 *
 * AndExpression :: = NotExpression ( "AND" NotExpression  )*
 *
 * NotExpression ::= "NOT" EqualityExpression |
 *                   EqualityExpression
 *
 * EqualityExpression ::= Identifier "IS" "NULL" |
 *                        Identifier "IS "NOT" "NULL" |
 *                        PrimaryExpression EqOps PrimaryExpression
 *                        PrimaryExpression
 *
 * PrimaryExpression :: = Identifier |
 *                        Literal |
 *                        "(" OrExpression ")"
 *
 */

namespace qpid {
namespace broker {

class Expression;

using std::string;

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
        return !i.present(env);
    }
};

// "IS NOT NULL"
class IsNonNull : public UnaryBooleanOperator<Identifier> {
    bool eval(Identifier& i, const SelectorEnv& env) const {
        return i.present(env);
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
BoolExpression* parseTopBoolExpression(const string& exp)
{
    string::const_iterator s = exp.begin();
    string::const_iterator e = exp.end();
    Tokeniser tokeniser(s,e);
    BoolExpression* b = parseEqualityExpression(tokeniser);
    if (!b) throw std::range_error("Illegal selector: couldn't parse");
    if (tokeniser.nextToken().type != T_EOS) throw std::range_error("Illegal selector: too much input");
    return b;
}

BoolExpression* parseEqualityExpression(Tokeniser& tokeniser)
{
    Token t;
    t = tokeniser.nextToken();

    Token op;
    op = tokeniser.nextToken();

    Expression* e1 = 0;
    switch (t.type) {
    case T_IDENTIFIER: {
        string val = t.val;
        // Check for "IS NULL" and "IS NOT NULL"
        if ( op.type==T_IS ) {
            // The rest must be T_NULL or T_NOT, T_NULL
            t = tokeniser.nextToken();
            switch (t.type) {
            case T_NULL:
                return new UnaryBooleanExpression<Identifier>(&isNull, new Identifier(val));
            case T_NOT:
                if ( tokeniser.nextToken().type == T_NULL)
                    return new UnaryBooleanExpression<Identifier>(&isNonNull, new Identifier(val));
            default:
                return 0;
                break;
            }
        }
        e1 = new Identifier(val);
        break;
    }
    case T_STRING:
        e1 = new Literal(t.val);
        break;
    default:
        return 0;
    }

    if (op.type != T_OPERATOR) {
        delete e1;
        return 0;
    }

    t = tokeniser.nextToken();
    Expression* e2 = 0;
    switch (t.type) {
    case T_IDENTIFIER:
        e2 = new Identifier(t.val);
        break;
    case T_STRING:
        e2 = new Literal(t.val);
        break;
    default:
        return 0;
    }
    if (op.val == "=") return new EqualityExpression(&eq, e1, e2);
    if (op.val == "<>") return new EqualityExpression(&neq, e1, e2);

    delete e2;
    delete e1;
    return 0;
}

}}
