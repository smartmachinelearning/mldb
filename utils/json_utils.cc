// This file is part of MLDB. Copyright 2015 Datacratic. All rights reserved.

/** json_utils.cc
    Jeremy Barnes, 10 November 2013
    Copyright (c) 2013 Datacratic Inc.  All rights reserved.

    Utilities for JSON values.
*/

#include <algorithm>
#include "json_utils.h"
#include "mldb/jml/utils/string_functions.h"
#include "mldb/ext/siphash/csiphash.h"
#include "mldb/types/json_parsing.h"
#include "mldb/types/json_printing.h"
#include "mldb/base/exc_assert.h"

using namespace std;
using namespace ML;


namespace {

uint64_t siphash24(const void *src, unsigned long src_sz, Datacratic::HashSeed key)
{
    return ::mldb_siphash24(src, src_sz, key.b);
}

uint64_t siphash24(const std::string & str, Datacratic::HashSeed key)
{
    return ::mldb_siphash24(str.c_str(), str.length(), key.b);
}

template<typename T>
uint64_t siphash24_bin(const T & v, Datacratic::HashSeed key)
{
    return ::mldb_siphash24(&v, sizeof(v), key.b);
}

}


namespace Datacratic {

std::string
jsonPrintAbbreviatedString(const Json::Value & val,
                           int maxLength)
{
    string s = ML::trim(val.toString());
    if (maxLength < 0)
        return s;
    if (s.length() < maxLength)
        return s;

    // TODO: utf-8 strings

    string contents = val.asString();
    string reduced(contents, 0, maxLength);

    return ML::trim(Json::Value(reduced).toString()) + "...";
}

std::string
jsonPrintAbbreviatedObject(const Json::Value & val,
                           int maxLengthPerItem,
                           int maxLength)
{
    std::string result = "{";

    auto keys = val.getMemberNames();

    std::sort(keys.begin(), keys.end());

    bool first = true;
    for (auto & k: keys) {
        if (first)
            result += " ";
        else result += ", ";
        first = false;

        result += "\"";
        result += jsonEscape(k);
        result += "\": ";

        result += jsonPrintAbbreviated(val[k],
                                       maxLengthPerItem,
                                       maxLength);
    }

    result += " }";

    return result;
}

std::string
jsonPrintAbbreviatedArray(const Json::Value & val,
                          int maxLengthPerItem,
                          int maxLength)
{
    string result;
    result = "[";

    int i = 0;
    for (;  i < val.size() && result.size() < maxLengthPerItem;  ++i) {
        if (i != 0) {
            result += ",";
        }
        result += jsonPrintAbbreviated(val[i], maxLengthPerItem, maxLength);
    }

    if (i < val.size())
        result += ",...";
    result += "]";

    return result;
}

std::string
jsonPrintAbbreviated(const Json::Value & val,
                     int maxLengthPerItem,
                     int maxLength)
{
    switch (val.type()) {
    case Json::objectValue:
        return jsonPrintAbbreviatedObject(val, maxLengthPerItem, maxLength);
    case Json::arrayValue:
        return jsonPrintAbbreviatedArray(val, maxLengthPerItem, maxLength);
    case Json::stringValue:
        return jsonPrintAbbreviatedString(val, maxLengthPerItem);
    default:
        return ML::trim(val.toString());
    }
}

const HashSeed defaultSeedStable { .i64 = { 0x1958DF94340e7cbaULL, 0x8928Fc8B84a0ULL } };

uint64_t jsonHashObject(const Json::Value & val,
                        HashSeed seed)
{
    ExcAssertEqual(val.type(), Json::objectValue);

    auto keys = val.getMemberNames();

    std::sort(keys.begin(), keys.end());

    uint64_t subHashes[keys.size() * 2];

    for (unsigned i = 0;  i < keys.size();  ++i) {
        subHashes[i * 2] = siphash24(keys[i], seed);
        subHashes[i * 2 + 1] = jsonHash(val[keys[i]], seed);
    }

    return siphash24(subHashes, 16 * keys.size(), seed);
}

uint64_t jsonHashArray(const Json::Value & val,
                       HashSeed seed)
{
    ExcAssertEqual(val.type(), Json::arrayValue);

    uint64_t subHashes[val.size()];

    for (unsigned i = 0;  i < val.size();  ++i) {
        subHashes[i] = jsonHash(val[i], seed);
    }

    return siphash24(subHashes, 8 * val.size(), seed);
}

uint64_t jsonHash(const Json::Value & val,
                  HashSeed seed)
{
    switch (val.type()) {
    case Json::objectValue:
        return jsonHashObject(val, seed);
    case Json::arrayValue:
        return jsonHashArray(val, seed);
    case Json::stringValue:
        return siphash24(val.asString(), seed);
    case Json::booleanValue:
        return siphash24_bin(val.asBool(), seed);
    case Json::realValue:
        return siphash24_bin(val.asDouble(), seed);
    case Json::intValue:
        return siphash24_bin(val.asInt(), seed);
    case Json::uintValue:
        return siphash24_bin(val.asUInt(), seed);
    case Json::nullValue:
        return 1;
    default:
        throw ML::Exception("unknown value type for jsonHash");
    }
}

Json::Value flatten(const std::vector<Json::Value> & args)
{
    Json::Value result;

    for (auto & arg: args) {
        for (auto & val: arg) {
            for (auto & el: val) {
                result.append(el);
            }
        }
    }
    
    return result;
}

Json::Value jsonMin(const Json::Value & v1,
                    const Json::Value & v2)
{
    if (v1.isNumeric() && v1.isNumeric())
        return v1.asDouble() < v2.asDouble() ? v1 : v2;
    else if (v1.isString() && v2.isString())
        return v1.asString() < v2.asString() ? v1 : v2;
    else throw ML::Exception("cannot compare " + v1.toString() + " to "
                             + v2.toString());
}

Json::Value jsonMax(const Json::Value & v1,
                    const Json::Value & v2)
{
    if (v1.isNumeric() && v1.isNumeric())
        return v1.asDouble() < v2.asDouble() ? v2 : v1;
    else if (v1.isString() && v2.isString())
        return v1.asString() < v2.asString() ? v2 : v1;
    else throw ML::Exception("cannot compare " + v1.toString() + " to "
                             + v2.toString());
}

Json::Value jsonMaxVector(const std::vector<Json::Value> & args)
{
    if (args.empty())
        return Json::Value();
    Json::Value result = args[0];
    for (unsigned i = 1;  i < args.size();  ++i)
        result = jsonMax(result, args[i]);
    return result;
    
}

Json::Value jsonMinVector(const std::vector<Json::Value> & args)
{
    if (args.empty())
        return Json::Value();
    Json::Value result = args[0];
    for (unsigned i = 1;  i < args.size();  ++i)
        result = jsonMin(result, args[i]);
    return result;
}

} // namespace Datacratic

