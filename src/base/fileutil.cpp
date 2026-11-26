/*
 This file is part of Banksia, a chess and xiangqi tournament manager, and is
 used by Open Chess Game Database Standard (OCGDB).

 Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)

 In OCGDB this file is distributed under the MIT License (MIT), as the rest of
 OCGDB (see the file LICENSE, or http://opensource.org/licenses/MIT). The
 copyright holder relicensed the copy taken from Banksia (GPL-3) for OCGDB.
 */

#include <cstring>
#include <set>
#include <utility>
#include <filesystem>
#include <iostream>
#include <system_error>

#if (defined(_WIN32) || defined(_WIN64))

#include <windows.h>
#include <direct.h> // _getcwd
#include <stdlib.h> // _fullpath
#include <sys/stat.h> // _stat64
#include <tlhelp32.h> // isRunning

#else

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>

#ifdef __APPLE__
#include <sys/param.h>
#include <sys/sysctl.h>
#endif

#if defined(BSD)
#include <sys/sysctl.h>
#endif

#endif

#include "fileutil.h"
#include "strutil.h"

namespace banksia {

    std::string configFolderPath;

    // ------------------------------------------------------------------------
    //  Paths
    // ------------------------------------------------------------------------

    std::string getFileNameAndExt(const std::string& path)
    {
        auto pos = path.find_last_of("/\\");
        return pos != std::string::npos ? path.substr(pos + 1) : path;
    }

    std::string getFileName(const std::string& path)
    {
        auto str = getFileNameAndExt(path);
        auto pos = str.find_last_of(".");
        if (pos != std::string::npos) {
            str = str.substr(0, pos);
        }
        return str;
    }

    std::string getFolder(const std::string& path)
    {
        auto pos = path.find_last_of("/\\");
        return pos != std::string::npos ? path.substr(0, pos) : "";
    }

    std::string currentWorkingFolder()
    {
        char buf[1024];

#if (defined(_WIN32) || defined(_WIN64))
        auto r = _getcwd(buf, sizeof(buf));
#else
        auto r = getcwd(buf, sizeof(buf));
#endif

        if (!r) {
            buf[0] = 0;
        }
        return buf;
    }

#if (defined(_WIN32) || defined(_WIN64))

    std::string getFullPath(const char* path)
    {
        if (!path) return "";
        char buff[_MAX_PATH];
        if (_fullpath(buff, path, _MAX_PATH) != NULL) {
            return buff;
        }
        return path;
    }

#else

    std::string getFullPath(const char* path)
    {
        if (!path) return "";

        char buff[PATH_MAX];
        char* ptr;

        if (*path == '~') {
            /// strcat(strcpy(s, home), path + 1) wrote into a PATH_MAX buffer
            /// with no length check at all: a long $HOME plus a long path
            /// overran the stack.
            const char* home = getenv("HOME");
            if (home) {
                std::string expanded = std::string(home) + (path + 1);
                if (expanded.size() >= PATH_MAX) {
                    return path;
                }
                ptr = realpath(expanded.c_str(), buff);
                return ptr ? ptr : path;
            }
        }

        ptr = realpath(path, buff);
        return ptr ? ptr : path;
    }

#endif

    // ------------------------------------------------------------------------
    //  Directory listing, file facts, processes
    // ------------------------------------------------------------------------

#if (defined(_WIN32) || defined(_WIN64))

    static void findFiles(std::vector<std::string>& names, const std::string& dirname)
    {
        std::string search_path = dirname + "/*.*";

        WIN32_FIND_DATAA file;
        HANDLE search_handle = FindFirstFileA(search_path.c_str(), &file);

        /// FindFirstFile reports failure with INVALID_HANDLE_VALUE (-1), not
        /// NULL, so `if (search_handle)` was true on failure and the loop below
        /// then called FindNextFile on an invalid handle.
        if (search_handle == INVALID_HANDLE_VALUE) {
            return;
        }

        do {
            std::string fullpath = dirname + "\\" + file.cFileName;
            /// This was `(attributes | DIRECTORY) == DIRECTORY`, which is true
            /// only when the attribute word is 0 or exactly DIRECTORY. A
            /// hidden or system folder (0x12, 0x14) therefore tested false and
            /// was listed as if it were a file, and never descended into.
            if ((file.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                if (file.cFileName[0] != '.') findFiles(names, fullpath);
            } else {
                names.push_back(fullpath);
            }
        } while (FindNextFileA(search_handle, &file));

        ::FindClose(search_handle);
    }

    std::vector<std::string> listdir(std::string dirname)
    {
        std::vector<std::string> pathVec;
        findFiles(pathVec, dirname);
        return pathVec;
    }

    int64_t getFileSize(const std::string& path)
    {
        struct __stat64 fileStat;
        if (_stat64(path.c_str(), &fileStat) != 0) return 0;
        return fileStat.st_size;
    }

    int64_t getFileLastWriteTime(const std::string& path)
    {
        struct __stat64 fileStat;
        if (_stat64(path.c_str(), &fileStat) != 0) return 0;
        return fileStat.st_mtime;
    }

    bool isExecutable(const std::string& path)
    {
        /// path.find(".exe") matched anywhere in the string, so a folder named
        /// "engines.exe.old" or a file "sf.exe.bak" counted as executable, and
        /// an upper-case ".EXE" did not.
        auto name = getFileNameAndExt(path);
        auto pos = name.find_last_of('.');
        if (pos == std::string::npos) {
            return false;
        }
        auto ext = name.substr(pos);
        toLower(ext);
        return ext == ".exe" || ext == ".bat" || ext == ".cmd" || ext == ".com";
    }

    bool isRunning(int pid)
    {
        HANDLE pss = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (pss == INVALID_HANDLE_VALUE) {
            return false;
        }

        PROCESSENTRY32 pe = { 0 };
        pe.dwSize = sizeof(pe);

        bool found = false;
        if (Process32First(pss, &pe)) {
            do {
                if (pe.th32ProcessID == static_cast<DWORD>(pid)) {
                    found = true;
                    break;
                }
            } while (Process32Next(pss, &pe));
        }

        /// The early `return true` from inside the loop leaked the snapshot
        /// handle -- once per call, and this is polled while engines run.
        CloseHandle(pss);
        return found;
    }

#else

    /// Each folder is entered once, by its device and inode: a symbolic link
    /// that points back up the tree would otherwise recurse without end.
    static void listdir_walk(std::string dirname, std::vector<std::string>& vec,
                             std::set<std::pair<dev_t, ino_t>>& visited)
    {
        struct stat dst;
        if (stat(dirname.c_str(), &dst) != 0 || !visited.insert({ dst.st_dev, dst.st_ino }).second) {
            return;
        }

        DIR* d_fh = opendir(dirname.c_str());
        if (d_fh == NULL) {
            return;
        }

        dirname += "/";

        struct dirent* entry;
        while ((entry = readdir(d_fh)) != NULL) {
            // Don't descend up the tree or include the current directory
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            auto isDir = entry->d_type == DT_DIR;
            if (entry->d_type == DT_UNKNOWN || entry->d_type == DT_LNK) {
                /// some file systems (e.g. XFS without ftype, NFS, FUSE mounts) don't fill d_type,
                /// ask stat, otherwise a subfolder would be listed as a file and never descended.
                /// A symbolic link to a folder is followed the same way.
                struct stat st;
                isDir = stat((dirname + entry->d_name).c_str(), &st) == 0 && S_ISDIR(st.st_mode);
            }

            if (isDir) {
                listdir_walk(dirname + entry->d_name, vec, visited);
            } else {
                vec.push_back(dirname + entry->d_name);
            }
        }

        closedir(d_fh);
    }

    std::vector<std::string> listdir(std::string dirname)
    {
        std::vector<std::string> vec;
        std::set<std::pair<dev_t, ino_t>> visited;
        listdir_walk(dirname, vec, visited);
        return vec;
    }

    int64_t getFileSize(const std::string& fileName)
    {
        struct stat st;
        if (stat(fileName.c_str(), &st) != 0) {
            return 0;
        }
        return st.st_size;
    }

    int64_t getFileLastWriteTime(const std::string& fileName)
    {
        struct stat st;
        if (stat(fileName.c_str(), &st) != 0) {
            return -1;
        }
        return st.st_mtime;
    }

    bool isExecutable(const std::string& path)
    {
        return !access(path.c_str(), X_OK);
    }

    bool isRunning(int pid)
    {
        return 0 == kill(pid, 0);
    }

#endif

    bool fileExists(const std::string& path)
    {
        if (path.empty()) {
            return false;
        }
        std::error_code ec;
        return std::filesystem::is_regular_file(fsPath(path), ec);
    }

    std::ofstream openOfstream2write(const std::string& path)
    {
#if (defined(_WIN32) || defined(_WIN64))
        return std::ofstream(fsPath(path), std::ios_base::out | std::ios_base::app);
#else
        return std::ofstream(path, std::ios_base::out | std::ios_base::app);
#endif
    }

    int getNumberOfCores()
    {
        /// This tested `WIN32`, which the compiler does not define -- only
        /// `_WIN32` is predefined. It compiled here only because <windows.h>
        /// happens to define WIN32 itself; without that include it would have
        /// fallen through to sysconf() and failed to build on MSVC.
#if (defined(_WIN32) || defined(_WIN64))
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        return sysinfo.dwNumberOfProcessors;
#elif defined(__APPLE__)
        int nm[2];
        size_t len = 4;
        uint32_t count;

        nm[0] = CTL_HW;
        nm[1] = HW_AVAILCPU;
        sysctl(nm, 2, &count, &len, NULL, 0);

        if (count < 1) {
            nm[1] = HW_NCPU;
            sysctl(nm, 2, &count, &len, NULL, 0);
            if (count < 1) {
                count = 1;
            }
        }
        return count;
#else
        return int(sysconf(_SC_NPROCESSORS_ONLN));
#endif
    }

    /**
     * http://nadeausoftware.com/articles/2012/09/c_c_tip_how_get_physical_memory_size_system
     * Returns the size of physical memory (RAM) in bytes.
     */
    size_t getMemorySize()
    {
#if defined(_WIN32) && (defined(__CYGWIN__) || defined(__CYGWIN32__))
        /* Cygwin under Windows. ------------------------------------ */
        /* New 64-bit MEMORYSTATUSEX isn't available.  Use old 32.bit */
        MEMORYSTATUS status;
        status.dwLength = sizeof(status);
        GlobalMemoryStatus(&status);
        return (size_t)status.dwTotalPhys;

#elif defined(_WIN32)
        /* Windows. ------------------------------------------------- */
        /* Use new 64-bit MEMORYSTATUSEX, not old 32-bit MEMORYSTATUS */
        MEMORYSTATUSEX status;
        status.dwLength = sizeof(status);
        GlobalMemoryStatusEx(&status);
        return (size_t)status.ullTotalPhys;

#elif defined(__unix__) || defined(__unix) || defined(unix) || (defined(__APPLE__) && defined(__MACH__))
        /* UNIX variants. ------------------------------------------- */
        /* Prefer sysctl() over sysconf() except sysctl() HW_REALMEM and HW_PHYSMEM */

#if defined(CTL_HW) && (defined(HW_MEMSIZE) || defined(HW_PHYSMEM64))
        int mib[2];
        mib[0] = CTL_HW;
#if defined(HW_MEMSIZE)
        mib[1] = HW_MEMSIZE; /* OSX. --------------------- */
#elif defined(HW_PHYSMEM64)
        mib[1] = HW_PHYSMEM64; /* NetBSD, OpenBSD. --------- */
#endif
        int64_t size = 0; /* 64-bit */
        size_t len = sizeof(size);
        if (sysctl(mib, 2, &size, &len, NULL, 0) == 0) return (size_t)size;
        return 0L; /* Failed? */

#elif defined(_SC_AIX_REALMEM)
        /* AIX. ----------------------------------------------------- */
        return (size_t)sysconf(_SC_AIX_REALMEM) * (size_t)1024L;

#elif defined(_SC_PHYS_PAGES) && defined(_SC_PAGESIZE)
        /* FreeBSD, Linux, OpenBSD, and Solaris. -------------------- */
        return (size_t)sysconf(_SC_PHYS_PAGES) * (size_t)sysconf(_SC_PAGESIZE);

#elif defined(_SC_PHYS_PAGES) && defined(_SC_PAGE_SIZE)
        /* Legacy. -------------------------------------------------- */
        return (size_t)sysconf(_SC_PHYS_PAGES) * (size_t)sysconf(_SC_PAGE_SIZE);

#elif defined(CTL_HW) && (defined(HW_PHYSMEM) || defined(HW_REALMEM))
        /* DragonFly BSD, FreeBSD, NetBSD, OpenBSD, and OSX. -------- */
        int mib[2];
        mib[0] = CTL_HW;
#if defined(HW_REALMEM)
        mib[1] = HW_REALMEM; /* FreeBSD. ----------------- */
#elif defined(HW_PYSMEM)
        mib[1] = HW_PHYSMEM; /* Others. ------------------ */
#endif
        unsigned int size = 0; /* 32-bit */
        size_t len = sizeof(size);
        if (sysctl(mib, 2, &size, &len, NULL, 0) == 0) return (size_t)size;
        return 0L; /* Failed? */
#endif /* sysctl and sysconf variants */

#else
        return 0L; /* Unknown OS. */
#endif
    }

    // ------------------------------------------------------------------------
    //  File content
    // ------------------------------------------------------------------------

    std::vector<std::string> readTextFileToArray(const std::string& path)
    {
#if (defined(_WIN32) || defined(_WIN64))
        std::ifstream inFile(fsPath(path));
#else
        std::ifstream inFile(path);
#endif

        std::string line;
        std::vector<std::string> vec;

        try {
            while (getline(inFile, line)) {
                vec.push_back(line);
            }
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
        return vec;
    }

    static constexpr char eventTag[] = "[Event";
#ifdef _XIANGQI_
    static constexpr char xqStartTag[] = "START{";
#endif

    int checkMultiGamesWithContent(const std::string& content)
    {
        auto cnt = 0;
        for (std::string::size_type p = 0;;) {
            auto q = content.find(eventTag, p);

#ifdef _XIANGQI_
            if (q == std::string::npos) {
                q = content.find(xqStartTag, p);
            }
#endif
            if (q == std::string::npos) {
                break;
            }

            if (q + 6 < content.length() && !std::isalnum(static_cast<unsigned char>(content.at(q + 6)))) {
                cnt++;
                if (cnt > 1) break;
            }

            p = q + 6;
        }
        return cnt;
    }

    int checkMultiGames(const std::string& path)
    {
        std::ifstream inFile(path);
        if (inFile.fail()) {
            return 0;
        }

        auto cnt = 0;
        std::string str;
        while (getline(inFile, str)) {
            auto p = str.find(eventTag);

#ifdef _XIANGQI_
            if (p == std::string::npos) {
                p = str.find(xqStartTag);
            }
#endif

            if (p != std::string::npos && p + 6 < str.length()
                && !std::isalnum(static_cast<unsigned char>(str.at(p + 6)))) {
                cnt++;
                if (cnt > 1) break;
            }
        }

        return cnt;
    }

    // ------------------------------------------------------------------------
    //  Host-installed filesystem hooks
    // ------------------------------------------------------------------------

    std::function<bool(const std::string& sourcePath, const std::string& targetPath)> copyFile = nullptr;
    std::function<bool(const std::string& sourcePath)> createFolder = nullptr;
    std::function<bool(const std::string& sourcePath, const std::string& targetPath)> renameFile = nullptr;
    std::function<bool(const std::string& sourcePath)> removeFile = nullptr;

    bool safeCopyFile(const std::string& sourcePath, const std::string& targetPath)
    {
        if (!copyFile || sourcePath.empty() || targetPath.empty()) {
            return false;
        }
        try {
            return copyFile(sourcePath, targetPath);
        } catch (const std::exception& e) {
            std::cerr << "Error: copyFile " << sourcePath << " -> " << targetPath << " failed: " << e.what()
                      << std::endl;
            return false;
        }
    }

    bool safeCreateFolder(const std::string& path)
    {
        if (!createFolder || path.empty()) {
            return false;
        }
        try {
            return createFolder(path);
        } catch (const std::exception& e) {
            std::cerr << "Error: createFolder " << path << " failed: " << e.what() << std::endl;
            return false;
        }
    }

    void ensureParentDirExists(const std::string& path)
    {
        auto folder = getFolder(path);
        if (!folder.empty()) {
            safeCreateFolder(folder);
            /// The return value is deliberately ignored: if the folder still
            /// isn't there afterward, the caller's file open fails and
            /// GameMng::reportWriteFailure says so.
        }
    }

    bool safeRenameFile(const std::string& sourcePath, const std::string& targetPath)
    {
        if (!renameFile || sourcePath.empty() || targetPath.empty()) {
            return false;
        }
        try {
            if (renameFile(sourcePath, targetPath)) {
                return true;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error: renameFile " << sourcePath << " -> " << targetPath << " failed: " << e.what()
                      << std::endl;
            return false;
        }
        // The hook reported failure. Confirm against the filesystem: some
        // backends report false for a rename that actually happened.
        return !fileExists(sourcePath) && fileExists(targetPath);
    }

    bool safeRemoveFile(const std::string& path)
    {
        if (path.empty()) {
            return false;
        }
        if (!removeFile) {
            return !fileExists(path);
        }
        try {
            if (removeFile(path)) {
                return true;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error: removeFile " << path << " failed: " << e.what() << std::endl;
            return false;
        }
        // Removing something that is already gone counts as success.
        return !fileExists(path);
    }

} // namespace banksia
