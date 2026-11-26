/*
 This file is part of Banksia, a chess and xiangqi tournament manager, and is
 used by Open Chess Game Database Standard (OCGDB).

 Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)

 In OCGDB this file is distributed under the MIT License (MIT), as the rest of
 OCGDB (see the file LICENSE, or http://opensource.org/licenses/MIT). The
 copyright holder relicensed the copy taken from Banksia (GPL-3) for OCGDB.
 */

#include <atomic>
#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>

#include "jsonable.h"
#include "fileutil.h"
#include "strutil.h"

namespace banksia {

    // ------------------------------------------------------------------------
    //  Obj
    // ------------------------------------------------------------------------

    void Obj::printOut(const char* msg) const
    {
        if (msg) {
            std::cout << msg << std::endl;
        }
        std::cout << toString() << std::endl;
    }

    // ------------------------------------------------------------------------
    //  Type-tolerant field readers
    // ------------------------------------------------------------------------

    /// Returns the member if present, otherwise a null Value. Never throws, and
    /// tolerates obj not being an object at all.
    static const Json::Value& jsonMember(const Json::Value& obj, const char* key)
    {
        static const Json::Value nullValue;
        if (!key || !obj.isObject()) {
            return nullValue;
        }
        const auto* v = obj.find(key, key + std::strlen(key));
        return v ? *v : nullValue;
    }

    std::string jsonGetString(const Json::Value& obj, const char* key, const std::string& defaultValue)
    {
        const auto& v = jsonMember(obj, key);
        try {
            if (v.isString()) return v.asString();
            // Numbers and bools are commonly written unquoted by hand; accept them.
            if (v.isNumeric() || v.isBool()) return v.asString();
        } catch (const std::exception&) {
        }
        return defaultValue;
    }

    int jsonGetInt(const Json::Value& obj, const char* key, int defaultValue)
    {
        const auto& v = jsonMember(obj, key);
        try {
            if (v.isInt()) return v.asInt();
            if (v.isNumeric()) return static_cast<int>(v.asDouble());
            if (v.isBool()) return v.asBool() ? 1 : 0;
            if (v.isString()) return std::stoi(v.asString());
        } catch (const std::exception&) {
        }
        return defaultValue;
    }

    int64_t jsonGetInt64(const Json::Value& obj, const char* key, int64_t defaultValue)
    {
        const auto& v = jsonMember(obj, key);
        try {
            if (v.isIntegral()) return v.asInt64();
            if (v.isNumeric()) return static_cast<int64_t>(v.asDouble());
            if (v.isBool()) return v.asBool() ? 1 : 0;
            if (v.isString()) return static_cast<int64_t>(std::stoll(v.asString()));
        } catch (const std::exception&) {
        }
        return defaultValue;
    }

    bool jsonGetBool(const Json::Value& obj, const char* key, bool defaultValue)
    {
        const auto& v = jsonMember(obj, key);
        try {
            if (v.isBool()) return v.asBool();
            if (v.isNumeric()) return v.asDouble() != 0.0;
            if (v.isString()) {
                auto s = v.asString();
                toLower(s);
                if (s == "true" || s == "yes" || s == "1") return true;
                if (s == "false" || s == "no" || s == "0") return false;
            }
        } catch (const std::exception&) {
        }
        return defaultValue;
    }

    double jsonGetDouble(const Json::Value& obj, const char* key, double defaultValue)
    {
        const auto& v = jsonMember(obj, key);
        try {
            if (v.isNumeric()) return v.asDouble();
            if (v.isBool()) return v.asBool() ? 1.0 : 0.0;
            if (v.isString()) return std::stod(v.asString());
        } catch (const std::exception&) {
        }
        return defaultValue;
    }

    // ------------------------------------------------------------------------
    //  Jsonable
    // ------------------------------------------------------------------------

    void Jsonable::printOut(const Json::Value& obj, std::string prefix)
    {
        if (obj.isString()) {
            std::cout << "\"" << obj.asString() << "\"";
        } else if (obj.isInt()) {
            std::cout << obj.asInt();
        } else if (obj.isDouble()) {
            std::cout << obj.asDouble();
        } else if (obj.isBool()) {
            std::cout << (obj.asBool() ? "true" : "false");
        } else if (obj.isArray()) {
            std::cout << "[\n";
            for (Json::Value::const_iterator it = obj.begin(); it != obj.end(); ++it) {
                printOut(*it, prefix + "  ");
                std::cout << ",\n";
            }
            std::cout << prefix << "\n],\n";
        } else {
            std::cout << prefix << "{\n";
            auto names = obj.getMemberNames();
            for (auto&& name : names) {
                std::cout << prefix << "\"" << name << "\" : ";
                auto v = obj[name];
                printOut(v, prefix + "  ");
                std::cout << ",\n";
            }
            std::cout << prefix << "},\n";
        }
    }

    std::string Jsonable::json2String(const Json::Value& json, bool beautifulMode)
    {
        Json::StreamWriterBuilder builder;
        builder["indentation"] = beautifulMode ? "  " : "";
        return Json::writeString(builder, json);
    }

    void Jsonable::merge(Json::Value& main, const Json::Value& from, JsonMerge merge)
    {
        auto names = from.getMemberNames();
        for (auto&& name : names) {
            if (merge == JsonMerge::overwrite || !main.isMember(name)) {
                main[name] = from[name];
            }
        }
    }

    bool Jsonable::loadFromJsonString(const std::string& string)
    {
        if (string.empty()) {
            return false;
        }

        std::string errorString;
        Json::Value jsonData;
        return JsonSavable::loadFromJsonString(string, jsonData, errorString) && load(jsonData);
    }

    std::string Jsonable::saveToJsonString() const
    {
        return Jsonable::json2String(saveToJson(), false);
    }

    // ------------------------------------------------------------------------
    //  JsonSavable - loading
    // ------------------------------------------------------------------------

    bool JsonSavable::loadFromJsonFile(bool verbose)
    {
        assert(!jsonPath.empty());
        return loadFromJsonFile(jsonPath, verbose);
    }

    bool JsonSavable::loadFromJsonFile(const std::string& path, bool verbose)
    {
        try {
            Json::Value obj;
            return _loadFromJsonFile(path, obj, verbose) && parseJsonAfterLoading(obj);
        } catch (const std::exception& e) {
            // parseJsonAfterLoading() runs subclass parsing code, which can throw
            // more than Json::Exception: std::out_of_range from at(),
            // must not escape as an unhandled exception.
            if (verbose)
                std::cerr << "Error: exception parsing json file " << path << " - " << e.what() << std::endl;
        }
        return false;
    }

    bool JsonSavable::_loadFromJsonFile(const std::string& path, Json::Value& jsonData, bool verbose)
    {
        jsonPath = path;
        return loadFromJsonFile(path, jsonData, verbose);
    }

    bool JsonSavable::loadFromJsonFile(const std::string& path, Json::Value& jsonData, bool verbose)
    {
        if (path.empty()) {
            return false;
        }

        try {
#if (defined(_WIN32) || defined(_WIN64))
            std::ifstream ifs(fsPath(path), std::ifstream::binary);
#else
            std::ifstream ifs(path, std::ifstream::binary);
#endif

            // Distinguish "no such file" (normal on first run) from "broken file"
            // (needs the user's attention). Previously both were silent, which is
            // why parse problems were so hard to diagnose.
            if (!ifs.is_open()) {
                if (verbose) {
                    std::cerr << "Note: json file not found or unreadable: " << path << std::endl;
                }
                return false;
            }

            Json::CharReaderBuilder jsonReader;
            std::string errorString;
            if (Json::parseFromStream(jsonReader, ifs, &jsonData, &errorString)) {
                return true;
            }

            if (verbose) {
                std::cerr << "Error: broken json file " << path << " - " << errorString << std::endl;
            }
        } catch (const std::exception& e) {
            // Json::Exception plus std::bad_alloc / length_error, which a huge or
            // deeply nested document can raise.
            if (verbose)
                std::cerr << "Error: exception parsing json file " << path << " - " << e.what() << std::endl;
        }

        return false;
    }

    bool JsonSavable::loadFromJsonString(const std::string& jsonString, bool verbose)
    {
        try {
            Json::Value jsonData;
            return loadFromJsonString(jsonString, jsonData, verbose) && parseJsonAfterLoading(jsonData);
        } catch (const std::exception& e) {
            if (verbose) std::cerr << "Error: exception parsing json from string - " << e.what() << std::endl;
        }

        return false;
    }

    bool JsonSavable::loadFromJsonString(const std::string& string, Json::Value& jsonData,
                                         std::string& errorString)
    {
        if (string.empty()) {
            errorString = "empty string";
            return false;
        }

        try {
            std::stringstream ss;
            ss.str(string);

            Json::CharReaderBuilder jsonReader;
            if (Json::parseFromStream(jsonReader, ss, &jsonData, &errorString)) {
                return true;
            }
        } catch (const std::exception& e) {
            errorString = e.what();
            std::cerr << "Error: exception parsing json from string - " << e.what() << std::endl;
        }

        return false;
    }

    bool JsonSavable::loadFromJsonString(const std::string& string, Json::Value& jsonData, bool verbose)
    {
        (void)verbose;
        std::string errorString;
        return loadFromJsonString(string, jsonData, errorString);
    }

    bool JsonSavable::parseJsonString(const std::string& string, Json::Value& jsonData,
                                      std::string& errorString)
    {
        try {
            Json::CharReaderBuilder builder;
            // unique_ptr: nothing leaks when parse() throws.
            std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
            if (!reader) {
                errorString = "cannot create json reader";
                return false;
            }

            bool parsingSuccessful
                    = reader->parse(string.c_str(), string.c_str() + string.size(), &jsonData, &errorString);
            if (!parsingSuccessful) {
                std::cerr << "Error: parsing the string - " << errorString << std::endl;
            }
            return parsingSuccessful;
        } catch (const std::exception& e) {
            errorString = e.what();
            std::cerr << "Error: Exception when parsing json from string - " << e.what() << std::endl;
        }

        return false;
    }

    // ------------------------------------------------------------------------
    //  JsonSavable - saving
    // ------------------------------------------------------------------------

    namespace {

        /// Serialises the rename/replace step of JSON saves. The write itself goes
        /// to a per-save unique temp file, so only the final swap needs to be
        /// exclusive.
        static auto& jsonSaveMutex() // built on first use, not at load time (clazy non-pod-global-static, 2026-11-15)
        {
            static std::mutex v;
            return v;
        }

        /// Distinguishes temp files of concurrent saves targeting the same path.
        std::atomic<unsigned long long> jsonSaveCounter { 0 };

        std::string makeTempPath(const std::string& path)
        {
            // A temp name of its own for every save, so two threads saving the same
            // file at once cannot write into each other's temp file.
            std::ostringstream oss;
            oss << path << ".tmp" << std::this_thread::get_id() << '.'
                << jsonSaveCounter.fetch_add(1, std::memory_order_relaxed);
            return oss.str();
        }

    } // namespace

    bool JsonSavable::saveToJsonFile(bool backup)
    {
        auto jsonData = createJsonForSaving();
        std::string bkJsonPath = backup ? jsonPath + ".bk" : "";
        return saveToJsonFile(jsonPath, bkJsonPath, jsonData, jsonBeautifulMode);
    }

    bool JsonSavable::saveToJsonFile(bool backup, Json::Value& jsonData)
    {
        std::string bkJsonPath = backup ? jsonPath + ".bk" : "";
        return saveToJsonFile(jsonPath, bkJsonPath, jsonData, jsonBeautifulMode);
    }

    bool JsonSavable::saveToJsonFile(const std::string& path, const std::string& bkJsonPath,
                                     const Json::Value& jsonData, bool beautifulMode)
    {
        if (path.empty()) {
            return false;
        }

        const auto tempPath = makeTempPath(path);

        // Phase 1: write the new content to a temp file and verify it landed.
        // Every failure below must leave the existing file alone.
        {
#if (defined(_WIN32) || defined(_WIN64))
            std::ofstream outFile(fsPath(tempPath), std::ofstream::trunc | std::ofstream::binary);
#else
            std::ofstream outFile(tempPath, std::ofstream::trunc | std::ofstream::binary);
#endif
            if (!outFile.is_open()) {
                std::cerr << "Error: cannot open for writing: " << tempPath << std::endl;
                return false;
            }

            try {
                Json::StreamWriterBuilder builder;
                builder["indentation"] = beautifulMode ? "  " : "";
                builder["precision"] = 3;
                builder["precisionType"] = "decimal";
                std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
                if (!writer) {
                    outFile.close();
                    safeRemoveFile(tempPath);
                    return false;
                }
                writer->write(jsonData, &outFile);
            } catch (const std::exception& e) {
                std::cerr << "Error: exception while writing json to " << tempPath << " - " << e.what()
                          << std::endl;
                outFile.close();
                safeRemoveFile(tempPath);
                return false;
            }

            outFile.flush();
            const auto writeOk = static_cast<bool>(outFile);
            outFile.close();

            // Check again after close(): errors from the final flush (disk full,
            // quota, I/O error) only surface here.
            if (!writeOk || outFile.fail()) {
                std::cerr << "Error: failed writing json to " << tempPath << std::endl;
                safeRemoveFile(tempPath);
                return false;
            }
        }

        // A valid JSON document is never zero bytes; treat that as a failed write
        // rather than replacing good data with an empty file.
        if (getFileSize(tempPath) <= 0) {
            std::cerr << "Error: json write produced an empty file: " << tempPath << std::endl;
            safeRemoveFile(tempPath);
            return false;
        }

        // Phase 2: swap the temp file into place. Serialised so two threads cannot
        // interleave their rename sequences on the same target.
        std::lock_guard<std::mutex> lock(jsonSaveMutex());

        const auto hadOriginal = fileExists(path);

        // Preserve the previous contents before touching the target, so a failure
        // in the swap below is always recoverable.
        if (hadOriginal && !bkJsonPath.empty()) {
            safeRemoveFile(bkJsonPath);
            if (!safeRenameFile(path, bkJsonPath)) {
                // Could not set the backup aside; fall back to copying so we still
                // do not lose the original.
                safeCopyFile(path, bkJsonPath);
            }
        }

        if (safeRenameFile(tempPath, path)) {
            return true;
        }

        // The swap failed. Put the original back rather than leaving nothing.
        std::cerr << "Error: cannot move " << tempPath << " into place at " << path << std::endl;
        if (hadOriginal && !bkJsonPath.empty() && !fileExists(path)) {
            safeCopyFile(bkJsonPath, path);
        }
        safeRemoveFile(tempPath);
        return false;
    }

    std::string JsonSavable::getJsonString()
    {
        Json::Value obj = createJsonForSaving();
        return obj.toStyledString();
    }

    std::string JsonSavable::getJsonPath() const
    {
        return jsonPath;
    }

    void JsonSavable::setJsonPath(const std::string& _jsonPath)
    {
        jsonPath = _jsonPath;
    }

    // ------------------------------------------------------------------------
    //  JsonParsing
    // ------------------------------------------------------------------------

    void JsonParsing::parseJson(Json::Value& jsonData, int taskId, const std::string& /*extraString*/)
    {
        std::cout << "JsonParsing::parseJson, taskId: " << taskId << std::endl;
        Jsonable::printOut(jsonData);
    }

    bool JsonParsing::parseJsonString(const std::string& string, int taskId, const std::string& extraString)
    {
        /// Several JSON documents can arrive back to back with nothing between
        /// them. Detect that by watching for a '{' or '[' that directly follows
        /// a '}' or ']', and if it happens, wrap the whole thing in an array
        /// with commas inserted so it parses as one document.
        auto multiObject = false;
        std::string str = "[";
        char lastch = 0;
        for (auto ch : string) {
            if (ch > ' ') {
                if ((ch == '{' || ch == '[') && (lastch == '}' || lastch == ']')) {
                    str += ',';
                    multiObject = true;
                }
                lastch = ch;
            }
            str += ch;
        }
        str += "]";

        Json::Value jsonData;
        if (!JsonSavable::loadFromJsonString(multiObject ? str : string, jsonData, false)) {
            return false;
        }

        if (multiObject) {
            for (Json::Value::const_iterator it = jsonData.begin(); it != jsonData.end(); ++it) {
                auto v = *it;
                parseJson(v, taskId, extraString);
            }
        } else {
            parseJson(jsonData, taskId, extraString);
        }

        return true;
    }

} // namespace banksia
