/*
 This file is part of Banksia, a chess and xiangqi tournament manager, and is
 used by Open Chess Game Database Standard (OCGDB).

 Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)

 In OCGDB this file is distributed under the MIT License (MIT), as the rest of
 OCGDB (see the file LICENSE, or http://opensource.org/licenses/MIT). The
 copyright holder relicensed the copy taken from Banksia (GPL-3) for OCGDB.
 */

#ifndef BANKSIA_BASE_JSONABLE_H
#define BANKSIA_BASE_JSONABLE_H

#include <cstdint>
#include <string>

#include "3rdparty/json/json.h"

namespace banksia {

    /// The root of the printable-object hierarchy: something that can say
    /// whether it is valid and describe itself.
    class Obj
    {
    public:
        virtual ~Obj() { }
        virtual const char* className() const { return "Obj"; }

        virtual bool isValid() const { return false; }
        virtual std::string toString() const { return ""; }
        virtual void printOut(const char* msg = nullptr) const;
        virtual void printOut(const std::string& msg) const { printOut(msg.c_str()); }
    };

    enum class JsonMerge { overwrite, fillmissing, none };

    /// Type-tolerant field readers.
    ///
    /// jsoncpp's asString()/asInt()/asBool() THROW Json::LogicError when the
    /// stored value has an incompatible type (e.g. an object where a string was
    /// expected). Config files get hand-edited, so these convert what they
    /// sensibly can, fall back to a default otherwise, and never throw.
    ///
    /// jsonGetDouble() currently has no caller. It is kept so the family stays
    /// complete: the next double-valued setting should be read through it
    /// rather than through a bare asDouble() that can throw.
    std::string jsonGetString(const Json::Value& obj, const char* key, const std::string& defaultValue = "");
    int jsonGetInt(const Json::Value& obj, const char* key, int defaultValue = 0);
    int64_t jsonGetInt64(const Json::Value& obj, const char* key, int64_t defaultValue = 0);
    bool jsonGetBool(const Json::Value& obj, const char* key, bool defaultValue = false);
    double jsonGetDouble(const Json::Value& obj, const char* key, double defaultValue = 0.0);

    /// An object that converts to and from a Json::Value.
    class Jsonable : public Obj
    {
    public:
        virtual ~Jsonable() { }
        virtual bool load(const Json::Value& obj) = 0;
        virtual Json::Value saveToJson() const = 0;

        virtual bool loadFromJsonString(const std::string&);
        virtual std::string saveToJsonString() const;

        static void printOut(const Json::Value&, std::string prefix = "");
        static std::string json2String(const Json::Value&, bool beautifulMode = true);
        static void merge(Json::Value& main, const Json::Value& from, JsonMerge);
    };

    /// An object backed by a JSON file on disk.
    ///
    /// Subclasses override createJsonForSaving() and parseJsonAfterLoading().
    /// saveToJsonFile() is crash-safe: it writes a unique temp file, verifies
    /// it, sets the previous contents aside as a backup, and only then swaps
    /// the new file into place.
    class JsonSavable
    {
    public:
        virtual ~JsonSavable() { }
        virtual bool loadFromJsonFile(bool verbose = true);
        virtual bool loadFromJsonFile(const std::string& jsonPath, bool verbose = true);
        virtual bool loadFromJsonString(const std::string& jsonString, bool verbose = true);

        virtual bool saveToJsonFile(bool backup);

        virtual std::string getJsonPath() const;
        virtual void setJsonPath(const std::string&);

        static bool loadFromJsonFile(const std::string& jsonPath, Json::Value& jsonData, bool verbose = true);
        static bool saveToJsonFile(const std::string& jsonPath, const std::string& bkJsonPath,
                                   const Json::Value& jsonData, bool beautifulMode);
        static bool loadFromJsonString(const std::string& string, Json::Value& jsonData,
                                       std::string& errorString);
        static bool loadFromJsonString(const std::string& string, Json::Value& jsonData, bool verbose);
        static bool parseJsonString(const std::string& string, Json::Value& jsonData,
                                    std::string& errorString);

        virtual Json::Value createJsonForSaving() { return Json::Value(); }
        virtual std::string getJsonString();

    protected:
        virtual bool saveToJsonFile(bool backup, Json::Value& jsonData);
        virtual bool parseJsonAfterLoading(Json::Value&) { return false; }

        virtual bool _loadFromJsonFile(const std::string& jsonPath, Json::Value& jsonData, bool verbose);

        std::string jsonPath;
        bool jsonBeautifulMode = true;
    };

    /// A receiver of JSON arriving as text -- a network reply, a pipe -- which
    /// may hold several concatenated documents rather than one.
    class JsonParsing
    {
    public:
        virtual ~JsonParsing() { }
        virtual bool parseJsonString(const std::string& string, int taskId = 0,
                                     const std::string& extraString = "");
        virtual void parseJson(Json::Value& jsonData, int taskId, const std::string& extraString);
    };

} // namespace banksia

#endif /* BANKSIA_BASE_JSONABLE_H */
