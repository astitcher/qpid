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

#include "qpid/broker/SelectorToken.h"

#include <string>
#include <algorithm>

// Tokeniserss always take string const_iterators to mark the beginning and end of the string being tokenised
// if the tokenise is successful then the start iterator is advanced, if the tokenise fails then the start
// iterator is unchanged.

// Not much of a parser...
void skipWS(std::string::const_iterator& s, std::string::const_iterator& e)
{
    while ( s!=e && std::isspace(*s) ) {
        ++s;
    }
}

inline bool isIdentifierStart(char c)
{
    return std::isalpha(c) || c=='_' || c=='$';
}

inline bool isIdentifierPart(char c)
{
    return std::isalnum(c) || c=='_' || c=='$';
}

// Parse reserved word like "IS", "NULL" etc. (case insensitive, terminated by ws or non alphanumeric)
bool tokeniseIdentifier(std::string::const_iterator& s, std::string::const_iterator& e, Token& tok)
{
    // Be sure that first char is alphanumeric or _ or $
    if ( s==e || !isIdentifierStart(*s) ) return false;
    
    std::string::const_iterator t = s;
    
    while ( s!=e && isIdentifierPart(*++s) );
    
    tok.type = T_IDENTIFIER;
    tok.val = std::string(t, s);

    return true;
}

// Lexically, reserved words are a subset of identifiers
// so we parse an identifier first then check if it is a reserved word and
// convert it if it is a reserved word
namespace {

struct RWEntry {
    const char* word;
    TokenType type;
};

bool caseless(const char* s1, const char* s2)
{
    while ( *s1 && *s2 ) {
        if (std::tolower(*s1++)>=std::tolower(*s2++))
            return false;
    }
    return true;
}

bool operator<(const RWEntry& r, const char* rhs) {
    return caseless(r.word, rhs);
}

bool operator<(const char* rhs, const RWEntry& r) {
    return caseless(rhs, r.word);
}

}

bool tokeniseReservedWord(Token& tok)
{
    // This must be sorted!!
    static const RWEntry reserved[] = {
        {"and", T_AND},
        {"between", T_BETWEEN},
        {"false", T_FALSE},
        {"in", T_IN},
        {"is", T_IS},
        {"like", T_LIKE},
        {"not", T_NOT},
        {"null", T_NULL},
        {"or", T_OR},
        {"true", T_TRUE}
    };
    
    const int reserved_size = sizeof(reserved)/sizeof(RWEntry);
        
    if ( tok.type != T_IDENTIFIER ) return false;
    
    std::pair<const RWEntry*, const RWEntry*> entry = std::equal_range(&reserved[0], &reserved[reserved_size], tok.val.c_str());

    if ( entry.first==entry.second ) return false;
    
    tok.type = entry.first->type;
    return true;
}

bool tokeniseIdentifierOrReservedWord(std::string::const_iterator& s, std::string::const_iterator& e, Token& tok)
{
    return tokeniseIdentifier(s, e, tok) && tokeniseReservedWord(tok);
}

// parsing strings is complicated by the need to allow "''" as an embedded single quote
bool tokeniseString(std::string::const_iterator& s, std::string::const_iterator& e, Token& tok)
{
    if ( s==e || *s != '\'' ) return false;
    
    std::string::const_iterator q = std::find(s+1, e, '\'');
    if ( q==e ) return false;
    
    std::string content(s+1, q);
    s = q; ++s;
    
    while ( s!=e && *s=='\'' ) {
        std::string::const_iterator q = std::find(s+1, e, '\'');
        if ( q==e ) return false;
        content += std::string(s, q);
        s = q; s++;
    }
    
    tok.type = T_STRING;
    tok.val = content;
    return true;
}

bool tokeniseBraces(std::string::const_iterator& s, std::string::const_iterator& e, Token& tok)
{
    if ( s==e) return false;
    if ( *s=='(' ) {
        tok.type = T_LBRACE;
        tok.val = *s++;
        return true;
    }
    if ( *s==')' ) {
        tok.type = T_RBRACE;
        tok.val = *s++;
        return true;
    }
    return false;
}

inline bool isOperatorPart(char c)
{
    return !std::isalnum(c) && !std::isspace(c) && c!='_' && c!='$' && c!='(' && c!=')';
}

// These lexical tokens contain no alphanumerics - this is broader than actual operators but
// works.
bool tokeniseOperator(std::string::const_iterator& s, std::string::const_iterator& e, Token& tok)
{
    if ( s==e || !isOperatorPart(*s) ) return false;
    
    std::string::const_iterator t = s;
    
    while (s!=e && isOperatorPart(*++s));
    
    tok.type = T_OPERATOR;
    tok.val = std::string(t, s);
    
    return true;   
}
