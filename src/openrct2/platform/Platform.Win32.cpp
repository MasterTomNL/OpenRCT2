/*****************************************************************************
 * Copyright (c) 2014-2024 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#ifdef _WIN32

// Windows.h needs to be included first
#    ifndef WIN32_LEAN_AND_MEAN
#        define WIN32_LEAN_AND_MEAN
#    endif
#    include "../Diagnostic.h"

#    include <cassert>
#    include <windows.h>

// Then the rest
#    include "../Version.h"

#    include <datetimeapi.h>
#    include <lmcons.h>
#    include <memory>
#    include <shlobj.h>
#    undef GetEnvironmentVariable

#    include "../Date.h"
#    include "../OpenRCT2.h"
#    include "../core/Path.hpp"
#    include "../core/String.hpp"
#    include "../localisation/Language.h"
#    include "../localisation/Localisation.Date.h"
#    include "Platform.h"

#    include <cstring>
#    include <iterator>
#    include <locale>

// Native resource IDs
#    include "../../../resources/resource.h"

// Enable visual styles
#    pragma comment(                                                                                                           \
        linker,                                                                                                                \
        "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
// The name of the mutex used to prevent multiple instances of the game from running
static constexpr wchar_t SINGLE_INSTANCE_MUTEX_NAME[] = L"RollerCoaster Tycoon 2_GSKMUTEX";

#    define SOFTWARE_CLASSES L"Software\\Classes"
#    define MUI_CACHE L"Local Settings\\Software\\Microsoft\\Windows\\Shell\\MuiCache"

namespace OpenRCT2::Platform
{
#    if (_WIN32_WINNT >= 0x0600)
    static std::string WIN32_GetKnownFolderPath(REFKNOWNFOLDERID rfid);
#    else
    static std::string WIN32_GetKnownFolderPath(int csidl);
#        define FOLDERID_Documents CSIDL_MYDOCUMENTS
#        define FOLDERID_Fonts CSIDL_FONTS
#        define FOLDERID_LocalAppData CSIDL_LOCAL_APPDATA
#        define FOLDERID_Profile CSIDL_PROFILE
#    endif
    static std::string WIN32_GetModuleFileNameW(HMODULE hModule);
    static void RemoveFileAssociation(const utf8* extension);

    std::string GetEnvironmentVariable(std::string_view name)
    {
        std::wstring result;
        auto wname = String::ToWideChar(name);
        wchar_t wvalue[256];
        auto valueSize = GetEnvironmentVariableW(wname.c_str(), wvalue, static_cast<DWORD>(std::size(wvalue)));
        if (valueSize < std::size(wvalue))
        {
            result = wvalue;
        }
        else
        {
            auto wlvalue = new wchar_t[valueSize];
            GetEnvironmentVariableW(wname.c_str(), wlvalue, valueSize);
            result = wlvalue;
            delete[] wlvalue;
        }
        return String::ToUtf8(result);
    }

    static std::string GetHomePathViaEnvironment()
    {
        std::string result;
        auto homedrive = GetEnvironmentVariable("HOMEDRIVE");
        auto homepath = GetEnvironmentVariable("HOMEPATH");
        if (!homedrive.empty() && !homepath.empty())
        {
            result = Path::Combine(homedrive, homepath);
        }
        return result;
    }

    std::string GetFolderPath(SPECIAL_FOLDER folder)
    {
        switch (folder)
        {
            // We currently store everything under Documents/OpenRCT2
            case SPECIAL_FOLDER::USER_CACHE:
            case SPECIAL_FOLDER::USER_CONFIG:
            case SPECIAL_FOLDER::USER_DATA:
            {
                auto path = WIN32_GetKnownFolderPath(FOLDERID_Documents);
                if (path.empty())
                {
                    path = GetFolderPath(SPECIAL_FOLDER::USER_HOME);
                }
                return path;
            }
            case SPECIAL_FOLDER::USER_HOME:
            {
                auto path = WIN32_GetKnownFolderPath(FOLDERID_Profile);
                if (path.empty())
                {
                    path = GetHomePathViaEnvironment();
                    if (path.empty())
                    {
                        path = "C:\\";
                    }
                }
                return path;
            }
            case SPECIAL_FOLDER::RCT2_DISCORD:
            {
                auto path = WIN32_GetKnownFolderPath(FOLDERID_LocalAppData);
                if (!path.empty())
                {
                    path = Path::Combine(path, u8"DiscordGames\\RollerCoaster Tycoon 2 Triple Thrill Pack\\content\\Game");
                }
                return path;
            }
            default:
                return std::string();
        }
    }

    std::string GetCurrentExecutableDirectory()
    {
        auto exePath = GetCurrentExecutablePath();
        auto exeDirectory = Path::GetDirectory(exePath);
        return exeDirectory;
    }

    std::string GetInstallPath()
    {
        auto path = std::string(gCustomOpenRCT2DataPath);
        if (!path.empty())
        {
            path = Path::GetAbsolute(path);
        }
        else
        {
            auto exeDirectory = GetCurrentExecutableDirectory();
            path = Path::Combine(exeDirectory, u8"data");
        }
        return path;
    }

    std::string GetCurrentExecutablePath()
    {
        return WIN32_GetModuleFileNameW(nullptr);
    }

    std::string GetDocsPath()
    {
        return GetCurrentExecutableDirectory();
    }

    static SYSTEMTIME TimeToSystemTime(std::time_t timestamp)
    {
        ULARGE_INTEGER time_value;
        time_value.QuadPart = (timestamp * 10000000LL) + 116444736000000000LL;

        FILETIME ft;
        ft.dwLowDateTime = time_value.LowPart;
        ft.dwHighDateTime = time_value.HighPart;

        SYSTEMTIME st;
        FileTimeToSystemTime(&ft, &st);
        return st;
    }

    bool IsOSVersionAtLeast(uint32_t major, uint32_t minor, uint32_t build)
    {
        bool result = false;
        auto hModule = GetModuleHandleW(L"ntdll.dll");
        if (hModule != nullptr)
        {
            using RtlGetVersionPtr = long(WINAPI*)(PRTL_OSVERSIONINFOW);
#    if defined(__GNUC__) && __GNUC__ >= 8
#        pragma GCC diagnostic push
#        pragma GCC diagnostic ignored "-Wcast-function-type"
#    endif
            auto fn = reinterpret_cast<RtlGetVersionPtr>(GetProcAddress(hModule, "RtlGetVersion"));
#    if defined(__GNUC__) && __GNUC__ >= 8
#        pragma GCC diagnostic pop
#    endif
            if (fn != nullptr)
            {
                RTL_OSVERSIONINFOW rovi{};
                rovi.dwOSVersionInfoSize = sizeof(rovi);
                if (fn(&rovi) == 0)
                {
                    if (rovi.dwMajorVersion > major
                        || (rovi.dwMajorVersion == major
                            && (rovi.dwMinorVersion > minor || (rovi.dwMinorVersion == minor && rovi.dwBuildNumber >= build))))
                    {
                        result = true;
                    }
                }
            }
        }
        return result;
    }

    bool IsRunningInWine()
    {
        HMODULE ntdllMod = GetModuleHandleW(L"ntdll.dll");

        if (ntdllMod && GetProcAddress(ntdllMod, "wine_get_version"))
        {
            return true;
        }
        return false;
    }

    /**
     * Checks if the current version of Windows supports ANSI colour codes.
     * From Windows 10, build 10586 ANSI escape colour codes can be used on stdout.
     */
    static bool HasANSIColourSupport()
    {
        return IsOSVersionAtLeast(10, 0, 10586);
    }

    static void EnableANSIConsole()
    {
        if (HasANSIColourSupport())
        {
            auto handle = GetStdHandle(STD_OUTPUT_HANDLE);
            DWORD mode;
            GetConsoleMode(handle, &mode);
            if (!(mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING))
            {
                mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                SetConsoleMode(handle, mode);
            }
        }
    }

    bool IsColourTerminalSupported()
    {
        static bool hasChecked = false;
        static bool isSupported = false;
        if (!hasChecked)
        {
            if (HasANSIColourSupport())
            {
                EnableANSIConsole();
                isSupported = true;
            }
            else
            {
                isSupported = false;
            }
            hasChecked = true;
        }
        return isSupported;
    }

    static std::string WIN32_GetModuleFileNameW(HMODULE hModule)
    {
        uint32_t wExePathCapacity = MAX_PATH;
        std::unique_ptr<wchar_t[]> wExePath;
        uint32_t size;
        do
        {
            wExePathCapacity *= 2;
            wExePath = std::make_unique<wchar_t[]>(wExePathCapacity);
            size = GetModuleFileNameW(hModule, wExePath.get(), wExePathCapacity);
        } while (size >= wExePathCapacity);
        return String::ToUtf8(wExePath.get());
    }

    u8string StrDecompToPrecomp(u8string_view input)
    {
        return u8string(input);
    }

    void SetUpFileAssociations()
    {
        // Setup file extensions
        SetUpFileAssociation(".park", "OpenRCT2 park (.park)", "Play", "\"%1\"", 0);
        SetUpFileAssociation(".sc4", "RCT1 Scenario (.sc4)", "Play", "\"%1\"", 0);
        SetUpFileAssociation(".sc6", "RCT2 Scenario (.sc6)", "Play", "\"%1\"", 0);
        SetUpFileAssociation(".sv4", "RCT1 Saved Game (.sc4)", "Play", "\"%1\"", 0);
        SetUpFileAssociation(".sv6", "RCT2 Saved Game (.sv6)", "Play", "\"%1\"", 0);
        SetUpFileAssociation(".sv7", "RCT Modified Saved Game (.sv7)", "Play", "\"%1\"", 0);
        SetUpFileAssociation(".sea", "RCTC Saved Game (.sea)", "Play", "\"%1\"", 0);
        SetUpFileAssociation(".td4", "RCT1 Track Design (.td4)", "Install", "\"%1\"", 0);
        SetUpFileAssociation(".td6", "RCT2 Track Design (.td6)", "Install", "\"%1\"", 0);

        // Refresh explorer
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    }

    static HMODULE _dllModule = nullptr;
    static HMODULE GetDLLModule()
    {
        if (_dllModule == nullptr)
        {
            _dllModule = GetModuleHandle(nullptr);
        }
        return _dllModule;
    }

    static std::wstring GetProdIDName(std::string_view extension)
    {
        auto progIdName = std::string(OPENRCT2_NAME) + std::string(extension);
        auto progIdNameW = String::ToWideChar(progIdName);
        return progIdNameW;
    }

    void RemoveFileAssociations()
    {
        // Remove file extensions
        RemoveFileAssociation(".park");
        RemoveFileAssociation(".sc4");
        RemoveFileAssociation(".sc6");
        RemoveFileAssociation(".sv4");
        RemoveFileAssociation(".sv6");
        RemoveFileAssociation(".sv7");
        RemoveFileAssociation(".sea");
        RemoveFileAssociation(".td4");
        RemoveFileAssociation(".td6");

        // Refresh explorer
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    }

    bool HandleSpecialCommandLineArgument(const char* argument)
    {
        return false;
    }

    bool FindApp(std::string_view app, std::string* output)
    {
        LOG_WARNING("FindApp() not implemented for Windows!");
        return false;
    }

    int32_t Execute(std::string_view command, std::string* output)
    {
        LOG_WARNING("Execute() not implemented for Windows!");
        return -1;
    }

    uint64_t GetLastModified(std::string_view path)
    {
        uint64_t lastModified = 0;
        auto pathW = String::ToWideChar(path);
        auto hFile = CreateFileW(pathW.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        if (hFile != INVALID_HANDLE_VALUE)
        {
            FILETIME ftCreate, ftAccess, ftWrite;
            if (GetFileTime(hFile, &ftCreate, &ftAccess, &ftWrite))
            {
                lastModified = (static_cast<uint64_t>(ftWrite.dwHighDateTime) << 32uLL)
                    | static_cast<uint64_t>(ftWrite.dwLowDateTime);
            }
            CloseHandle(hFile);
        }
        return lastModified;
    }

    uint64_t GetFileSize(std::string_view path)
    {
        uint64_t size = 0;
        auto pathW = String::ToWideChar(path);
        WIN32_FILE_ATTRIBUTE_DATA attributes;
        if (GetFileAttributesExW(pathW.c_str(), GetFileExInfoStandard, &attributes) != FALSE)
        {
            ULARGE_INTEGER fileSize;
            fileSize.LowPart = attributes.nFileSizeLow;
            fileSize.HighPart = attributes.nFileSizeHigh;
            size = fileSize.QuadPart;
        }
        return size;
    }

    bool ShouldIgnoreCase()
    {
        return true;
    }

    bool IsPathSeparator(char c)
    {
        return c == '\\' || c == '/';
    }

    std::string ResolveCasing(std::string_view path, bool fileExists)
    {
        std::string result;
        if (fileExists)
        {
            // Windows is case insensitive so it will exist and that is all that matters
            // for now. We can properly resolve the casing if we ever need to.
            result = std::string(path);
        }
        return result;
    }

    bool RequireNewWindow(bool openGL)
    {
        // Windows is apparently able to switch to hardware rendering on the fly although
        // using the same window in an unaccelerated and accelerated context is unsupported by SDL2
        return openGL;
    }

    std::string GetUsername()
    {
        std::string result;
        wchar_t usernameW[UNLEN + 1]{};
        DWORD usernameLength = UNLEN + 1;
        if (GetUserNameW(usernameW, &usernameLength))
        {
            result = String::ToUtf8(usernameW);
        }
        return result;
    }

    bool ProcessIsElevated()
    {
        BOOL isElevated = FALSE;
        HANDLE hToken = nullptr;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
        {
            TOKEN_ELEVATION Elevation;
            DWORD tokenSize = sizeof(TOKEN_ELEVATION);
            if (GetTokenInformation(hToken, TokenElevation, &Elevation, sizeof(Elevation), &tokenSize))
            {
                isElevated = Elevation.TokenIsElevated;
            }
        }
        if (hToken)
        {
            CloseHandle(hToken);
        }
        return isElevated;
    }

    std::string GetSteamPath()
    {
        wchar_t* wSteamPath;
        HKEY hKey;
        DWORD type, size;
        LRESULT result;

        if (RegOpenKeyW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", &hKey) != ERROR_SUCCESS)
            return {};

        // Get the size of the path first
        if (RegQueryValueExW(hKey, L"SteamPath", nullptr, &type, nullptr, &size) != ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            return {};
        }

        std::string outPath = "";
        wSteamPath = reinterpret_cast<wchar_t*>(malloc(size));
        result = RegQueryValueExW(hKey, L"SteamPath", nullptr, &type, reinterpret_cast<LPBYTE>(wSteamPath), &size);
        if (result == ERROR_SUCCESS)
        {
            auto utf8SteamPath = String::ToUtf8(wSteamPath);
            outPath = Path::Combine(utf8SteamPath, u8"steamapps", u8"common");
        }
        free(wSteamPath);
        RegCloseKey(hKey);
        return outPath;
    }

    std::string GetFontPath(const TTFFontDescriptor& font)
    {
        auto path = WIN32_GetKnownFolderPath(FOLDERID_Fonts);
        return !path.empty() ? Path::Combine(path, font.filename) : std::string();
    }

    bool LockSingleInstance()
    {
        // Check if operating system mutex exists
        HANDLE mutex = CreateMutexW(nullptr, FALSE, SINGLE_INSTANCE_MUTEX_NAME);
        if (mutex == nullptr)
        {
            LOG_ERROR("unable to create mutex");
            return true;
        }
        else if (GetLastError() == ERROR_ALREADY_EXISTS)
        {
            // Already running
            CloseHandle(mutex);
            return false;
        }
        return true;
    }

    int32_t GetDrives()
    {
        return GetLogicalDrives();
    }

    u8string GetRCT1SteamDir()
    {
        return u8"Rollercoaster Tycoon Deluxe";
    }

    u8string GetRCT2SteamDir()
    {
        return u8"Rollercoaster Tycoon 2";
    }

    time_t FileGetModifiedTime(u8string_view path)
    {
        WIN32_FILE_ATTRIBUTE_DATA data{};
        auto wPath = String::ToWideChar(path);
        auto result = GetFileAttributesExW(wPath.c_str(), GetFileExInfoStandard, &data);
        if (result != FALSE)
        {
            FILETIME localFileTime{};
            result = FileTimeToLocalFileTime(&data.ftLastWriteTime, &localFileTime);
            if (result != FALSE)
            {
                ULARGE_INTEGER ull{};
                ull.LowPart = localFileTime.dwLowDateTime;
                ull.HighPart = localFileTime.dwHighDateTime;
                return ull.QuadPart / 10000000uLL - 11644473600uLL;
            }
        }
        return 0;
    }

    datetime64 GetDatetimeNowUTC()
    {
        // Get file time
        FILETIME fileTime;
        GetSystemTimeAsFileTime(&fileTime);
        uint64_t fileTime64 = (static_cast<uint64_t>(fileTime.dwHighDateTime) << 32uLL)
            | (static_cast<uint64_t>(fileTime.dwLowDateTime));

        // File time starts from: 1601-01-01T00:00:00Z
        // Convert to start from: 0001-01-01T00:00:00Z
        datetime64 utcNow = fileTime64 - 504911232000000000uLL;
        return utcNow;
    }

} // namespace OpenRCT2::Platform

#endif
