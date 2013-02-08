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

#include "unit_test.h"

#include <string>

using std::string;

namespace qpid {
namespace tests {

QPID_AUTO_TEST_SUITE(SelectorSuite)

typedef bool (*Tokeniser)(string::const_iterator&,string::const_iterator&,Token&);

void verifyTokeniserSuccess(Tokeniser t, const char* ss, TokenType tt, const char* tv, const char* fs) {
    Token tok;
    string s(ss);
    string::const_iterator sb = s.begin();
    string::const_iterator se = s.end();
    BOOST_CHECK(t(sb, se, tok));
    BOOST_CHECK_EQUAL(tok.type, tt);
    BOOST_CHECK_EQUAL(tok.val, tv);
    BOOST_CHECK_EQUAL(string(sb, se), fs);
}

void verifyTokeniserFail(Tokeniser t, const char* c) {
    Token tok;
    string s(c);
    string::const_iterator sb = s.begin();
    string::const_iterator se = s.end();
    BOOST_CHECK(!t(sb, se, tok));
    BOOST_CHECK_EQUAL(string(sb, se), c);
}

void verifyTokeniserFailNoPositionCheck(Tokeniser t, const char* c) {
    Token tok;
    string s(c);
    string::const_iterator sb = s.begin();
    string::const_iterator se = s.end();
    BOOST_CHECK(!t(sb, se, tok));
}

QPID_AUTO_TEST_CASE(tokeniseSuccess)
{
    verifyTokeniserSuccess(&tokeniseIdentifier, "_123+blah", T_IDENTIFIER, "_123", "+blah");
    verifyTokeniserSuccess(&tokeniseIdentifier, "null_123+blah", T_IDENTIFIER, "null_123", "+blah");
    verifyTokeniserSuccess(&tokeniseIdentifierOrReservedWord, "null_123+blah", T_IDENTIFIER, "null_123", "+blah");
    verifyTokeniserSuccess(&tokeniseIdentifierOrReservedWord, "null+blah", T_NULL, "null", "+blah");
    verifyTokeniserSuccess(&tokeniseIdentifierOrReservedWord, "null+blah", T_NULL, "null", "+blah");
    verifyTokeniserSuccess(&tokeniseIdentifierOrReservedWord, "Is nOt null", T_IS, "Is", " nOt null");
}

QPID_AUTO_TEST_CASE(tokeniseFailure)
{
    verifyTokeniserFail(&tokeniseIdentifier, "123");
    verifyTokeniserFail(&tokeniseIdentifier, "'Embedded 123'");
    verifyTokeniserFailNoPositionCheck(&tokeniseReservedWord, "1.2e5");
    verifyTokeniserFailNoPositionCheck(&tokeniseReservedWord, "'Stringy thing'");
    verifyTokeniserFailNoPositionCheck(&tokeniseReservedWord, "oR_andsomething");
}

QPID_AUTO_TEST_SUITE_END()

}}
