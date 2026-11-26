/*
 This file is part of Banksia, a chess and xiangqi tournament manager, and is
 used by Open Chess Game Database Standard (OCGDB).

 Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)

 In OCGDB this file is distributed under the MIT License (MIT), as the rest of
 OCGDB (see the file LICENSE, or http://opensource.org/licenses/MIT). The
 copyright holder relicensed the copy taken from Banksia (GPL-3) for OCGDB.
 */

#ifndef BANKSIA_BASE_FILEUTIL_H
#define BANKSIA_BASE_FILEUTIL_H

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

namespace banksia {

    /// A filesystem path from a UTF-8 std::string.
    ///
    /// std::filesystem::u8path is deprecated in C++20 -- every call to it was a
    /// warning -- and constructing a path straight from a narrow string means
    /// the active code page on Windows rather than UTF-8. Going through
    /// std::u8string says what is meant and warns about nothing.
    inline std::filesystem::path fsPath(const std::string& path)
    {
        return std::filesystem::path(std::u8string(path.begin(), path.end()));
    }

    /// Path splitting. All three accept either slash, on every platform,
    /// because paths arrive from config files written on other machines.
    std::string getFileName(const std::string& path); /// no folder, no extension
    std::string getFileNameAndExt(const std::string& path); /// no folder
    std::string getFolder(const std::string& path); /// no trailing slash

    std::string currentWorkingFolder();

    /// The folder a run keeps its own files in -- the resume file, the match
    /// records, the backups. Set once at startup from -configpath, the folder
    /// holding the tournament json, or the working directory; read wherever a
    /// path is built. Carries a trailing separator.
    extern std::string configFolderPath;
    std::string getFullPath(const char* path);

    /// Every file under `dirname`, recursively, as full paths. Folders are
    /// descended into but not themselves listed.
    std::vector<std::string> listdir(std::string dirname);

    /// 0 when the file cannot be stated -- which is indistinguishable from an
    /// empty file, so use fileExists() when the difference matters.
    int64_t getFileSize(const std::string& path);
    int64_t getFileLastWriteTime(const std::string& path);

    /// True if the path exists and is a regular file.
    bool fileExists(const std::string& path);

    /// Windows: judged by extension, since there is no execute bit.
    /// POSIX: the actual execute permission for this user.
    bool isExecutable(const std::string& path);

    /// True if a process with this id exists. Not "is our child", and not
    /// "is alive and well" -- a zombie counts on POSIX.
    bool isRunning(int pid);

    int getNumberOfCores();

    /// Physical RAM in bytes; 0 when the platform is not recognised.
    size_t getMemorySize();

    /// Open for appending, handling non-ASCII paths on Windows.
    std::ofstream openOfstream2write(const std::string& path);

    std::vector<std::string> readTextFileToArray(const std::string& path);

    /// Counts up to two games in a PGN, which is all any caller asks: they want
    /// "one game" or "more than one". Stops at two.
    int checkMultiGames(const std::string& path);
    int checkMultiGamesWithContent(const std::string& content);

    /// Filesystem hooks, installed by the host application (see main.cpp).
    ///
    /// The GUI routes these through Qt so they honour its file dialogs, sandbox
    /// and undo; the console binaries install std::filesystem versions. They
    /// return bool so callers can tell success from failure.
    extern std::function<bool(const std::string& sourcePath, const std::string& targetPath)> copyFile;
    extern std::function<bool(const std::string& sourcePath)> createFolder;
    extern std::function<bool(const std::string& sourcePath, const std::string& targetPath)> renameFile;
    extern std::function<bool(const std::string& sourcePath)> removeFile;

    /// Null-safe wrappers around the hooks above, and the form every caller
    /// should use. Calling an unassigned std::function throws
    /// std::bad_function_call; these return false instead, so early-startup or
    /// embedded use cannot terminate the process.
    bool safeCopyFile(const std::string& sourcePath, const std::string& targetPath);
    bool safeCreateFolder(const std::string& path);
    bool safeRenameFile(const std::string& sourcePath, const std::string& targetPath);
    bool safeRemoveFile(const std::string& path);

    /// A tournament's own 'logs.engine'/'logs.pgn'/'logs.result' paths can name
    /// a folder that doesn't exist yet
    void ensureParentDirExists(const std::string& path);

} // namespace banksia

#endif /* BANKSIA_BASE_FILEUTIL_H */
