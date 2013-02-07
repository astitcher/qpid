#ifndef QPID_BROKER_SELECTORTOKEN_H
#define QPID_BROKER_SELECTORTOKEN_H

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

#include <string>

//#include <boost/function.hpp>

typedef enum {
    T_NULL,
    T_TRUE,
    T_FALSE,
    T_NOT,
    T_AND,
    T_OR,
    T_IN,
    T_IS,
    T_BETWEEN,
    T_LIKE,
    T_IDENTIFIER,
    T_STRING,
    T_NUMERIC_EXACT,
    T_NUMERIC_APPROX,
    T_LBRACE,
    T_RBRACE,
    T_OPERATOR
} TokenType;

struct Token {
    TokenType type;
    std::string val;
};

// typedef boost::function3<bool,std::string::const_iterator&,std::string::const_iterator&,Token&> Tokeniser;

bool tokeniseIdentifier(std::string::const_iterator& s, std::string::const_iterator& e, Token& tok);

bool tokeniseIdentifierOrReservedWord(std::string::const_iterator& s, std::string::const_iterator& e, Token& tok);

bool tokeniseString(std::string::const_iterator& s, std::string::const_iterator& e, Token& tok);

bool tokeniseBraces(std::string::const_iterator& s, std::string::const_iterator& e, Token& tok);

bool tokeniseOperator(std::string::const_iterator& s, std::string::const_iterator& e, Token& tok);

bool tokeniseNumeric(std::string::const_iterator& s, std::string::const_iterator& e, Token& tok);

#endif