/*****************************************************************************
 * Copyright (c) 2014-2024 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#if defined(_WIN32_WINNT) && (_WIN32_WINNT >= 0x0600)
namespace OpenRCT2::Platform
{
    static std::string WIN32_GetKnownFolderPath(REFKNOWNFOLDERID rfid)
    {
        std::string path;
        wchar_t* wpath = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(rfid, KF_FLAG_CREATE, nullptr, &wpath)))
        {
            path = String::ToUtf8(wpath);
        }
        CoTaskMemFree(wpath);
        return path;
    }

    bool SetupUriProtocol()
    {
        LOG_VERBOSE("Setting up URI protocol...");

        // [HKEY_CURRENT_USER\Software\Classes]
        HKEY hRootKey;
        if (RegOpenKeyW(HKEY_CURRENT_USER, SOFTWARE_CLASSES, &hRootKey) == ERROR_SUCCESS)
        {
            // [hRootKey\openrct2]
            HKEY hClassKey;
            if (RegCreateKeyW(hRootKey, L"openrct2", &hClassKey) == ERROR_SUCCESS)
            {
                if (RegSetValueW(hClassKey, nullptr, REG_SZ, L"URL:openrct2", 0) == ERROR_SUCCESS)
                {
                    if (RegSetKeyValueW(hClassKey, nullptr, L"URL Protocol", REG_SZ, "", 0) == ERROR_SUCCESS)
                    {
                        // [hRootKey\openrct2\shell\open\command]
                        wchar_t exePath[MAX_PATH];
                        GetModuleFileNameW(nullptr, exePath, MAX_PATH);

                        wchar_t buffer[512];
                        swprintf_s(buffer, std::size(buffer), L"\"%s\" handle-uri \"%%1\"", exePath);
                        if (RegSetValueW(hClassKey, L"shell\\open\\command", REG_SZ, buffer, 0) == ERROR_SUCCESS)
                        {
                            // Not compulsory, but gives the application a nicer name
                            // [HKEY_CURRENT_USER\SOFTWARE\Classes\Local Settings\Software\Microsoft\Windows\Shell\MuiCache]
                            HKEY hMuiCacheKey;
                            if (RegCreateKeyW(hRootKey, MUI_CACHE, &hMuiCacheKey) == ERROR_SUCCESS)
                            {
                                swprintf_s(buffer, std::size(buffer), L"%s.FriendlyAppName", exePath);
                                // mingw-w64 used to define RegSetKeyValueW's signature incorrectly
                                // You need at least mingw-w64 5.0 including this commit:
                                //   https://sourceforge.net/p/mingw-w64/mingw-w64/ci/da9341980a4b70be3563ac09b5927539e7da21f7/
                                RegSetKeyValueW(hMuiCacheKey, nullptr, buffer, REG_SZ, L"OpenRCT2", sizeof(L"OpenRCT2"));
                            }

                            LOG_VERBOSE("URI protocol setup successful");
                            return true;
                        }
                    }
                }
            }
        }

        LOG_VERBOSE("URI protocol setup failed");
        return false;
    }

    uint16_t GetLocaleLanguage()
    {
        wchar_t langCode[LOCALE_NAME_MAX_LENGTH];
        if (GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_SNAME, langCode, static_cast<int>(std::size(langCode))) == 0)
        {
            return LANGUAGE_UNDEFINED;
        }

        const std::pair<std::wstring_view, int16_t> supportedLocales[] = {
            { L"ar", /*LANGUAGE_ARABIC*/ LANGUAGE_UNDEFINED }, // Experimental, don't risk offering it by default yet
            { L"ca", LANGUAGE_CATALAN },
            { L"zh-Hans", LANGUAGE_CHINESE_SIMPLIFIED },  // May not be accurate enough
            { L"zh-Hant", LANGUAGE_CHINESE_TRADITIONAL }, // May not be accurate enough
            { L"cs", LANGUAGE_CZECH },
            { L"da", LANGUAGE_DANISH },
            { L"de", LANGUAGE_GERMAN },
            { L"en-GB", LANGUAGE_ENGLISH_UK },
            { L"en-US", LANGUAGE_ENGLISH_US },
            { L"eo", LANGUAGE_ESPERANTO },
            { L"es", LANGUAGE_SPANISH },
            { L"fr", LANGUAGE_FRENCH },
            { L"fr-CA", LANGUAGE_FRENCH_CA },
            { L"it", LANGUAGE_ITALIAN },
            { L"ja", LANGUAGE_JAPANESE },
            { L"ko", LANGUAGE_KOREAN },
            { L"hu", LANGUAGE_HUNGARIAN },
            { L"nl", LANGUAGE_DUTCH },
            { L"no", LANGUAGE_NORWEGIAN },
            { L"pl", LANGUAGE_POLISH },
            { L"pt-BR", LANGUAGE_PORTUGUESE_BR },
            { L"ru", LANGUAGE_RUSSIAN },
            { L"fi", LANGUAGE_FINNISH },
            { L"sv", LANGUAGE_SWEDISH },
            { L"tr", LANGUAGE_TURKISH },
            { L"uk", LANGUAGE_UKRAINIAN },
            { L"vi", LANGUAGE_VIETNAMESE },
        };
        static_assert(
            std::size(supportedLocales) == LANGUAGE_COUNT - 1, "GetLocaleLanguage: List of languages does not match the enum!");

        for (const auto& locale : supportedLocales)
        {
            if (wcsncmp(langCode, locale.first.data(), locale.first.length()) == 0)
            {
                return locale.second;
            }
        }
        return LANGUAGE_UNDEFINED;
    }

    CurrencyType GetLocaleCurrency()
    {
        wchar_t currCode[9];
        if (GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_SINTLSYMBOL, currCode, static_cast<int>(std::size(currCode))) == 0)
        {
            return Platform::GetCurrencyValue(nullptr);
        }

        return Platform::GetCurrencyValue(String::ToUtf8(currCode).c_str());
    }

    MeasurementFormat GetLocaleMeasurementFormat()
    {
        UINT measurement_system;
        if (GetLocaleInfoEx(
                LOCALE_NAME_USER_DEFAULT, LOCALE_IMEASURE | LOCALE_RETURN_NUMBER, reinterpret_cast<LPWSTR>(&measurement_system),
                sizeof(measurement_system) / sizeof(wchar_t))
            == 0)
        {
            return MeasurementFormat::Metric;
        }

        return measurement_system == 1 ? MeasurementFormat::Imperial : MeasurementFormat::Metric;
    }

    uint8_t GetLocaleDateFormat()
    {
        // Retrieve short date format, eg "MM/dd/yyyy"
        wchar_t dateFormat[80];
        if (GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_SSHORTDATE, dateFormat, static_cast<int>(std::size(dateFormat)))
            == 0)
        {
            return DATE_FORMAT_DAY_MONTH_YEAR;
        }

        // The only valid characters for format types are: dgyM
        // We try to find 3 strings of format types, ignore any characters in between.
        // We also ignore 'g', as it represents 'era' and we don't have that concept
        // in our date formats.
        // https://msdn.microsoft.com/en-us/library/windows/desktop/dd317787(v=vs.85).aspx
        //
        wchar_t first[std::size(dateFormat)];
        wchar_t second[std::size(dateFormat)];
        if (swscanf_s(
                dateFormat, L"%l[dyM]%*l[^dyM]%l[dyM]%*l[^dyM]%*l[dyM]", first, static_cast<uint32_t>(std::size(first)), second,
                static_cast<uint32_t>(std::size(second)))
            != 2)
        {
            return DATE_FORMAT_DAY_MONTH_YEAR;
        }

        if (first[0] == L'd')
        {
            return DATE_FORMAT_DAY_MONTH_YEAR;
        }
        if (first[0] == L'M')
        {
            return DATE_FORMAT_MONTH_DAY_YEAR;
        }
        if (first[0] == L'y')
        {
            if (second[0] == 'd')
            {
                return DATE_FORMAT_YEAR_DAY_MONTH;
            }

            // Closest possible option
            return DATE_FORMAT_YEAR_MONTH_DAY;
        }

        // Default fallback
        return DATE_FORMAT_DAY_MONTH_YEAR;
    }

    TemperatureUnit GetLocaleTemperatureFormat()
    {
        UINT fahrenheit;

        // GetLocaleInfoEx will set fahrenheit to 1 if the locale on this computer
        // uses the United States measurement system or 0 otherwise.
        if (GetLocaleInfoEx(
                LOCALE_NAME_USER_DEFAULT, LOCALE_IMEASURE | LOCALE_RETURN_NUMBER, reinterpret_cast<LPWSTR>(&fahrenheit),
                sizeof(fahrenheit) / sizeof(wchar_t))
            == 0)
        {
            // Assume celsius by default if function call fails
            return TemperatureUnit::Celsius;
        }

        return fahrenheit == 1 ? TemperatureUnit::Fahrenheit : TemperatureUnit::Celsius;
    }

    bool SetUpFileAssociation(
        std::string_view extension, std::string_view fileTypeText, std::string_view commandText, std::string_view commandArgs,
        const uint32_t iconIndex)
    {
        wchar_t exePathW[MAX_PATH];
        wchar_t dllPathW[MAX_PATH];

        [[maybe_unused]] int32_t printResult;

        GetModuleFileNameW(nullptr, exePathW, static_cast<DWORD>(std::size(exePathW)));
        GetModuleFileNameW(GetDLLModule(), dllPathW, static_cast<DWORD>(std::size(dllPathW)));

        auto extensionW = String::ToWideChar(extension);
        auto fileTypeTextW = String::ToWideChar(fileTypeText);
        auto commandTextW = String::ToWideChar(commandText);
        auto commandArgsW = String::ToWideChar(commandArgs);
        auto progIdNameW = GetProdIDName(extension);

        HKEY hKey = nullptr;
        HKEY hRootKey = nullptr;

        // [HKEY_CURRENT_USER\Software\Classes]
        if (RegOpenKeyW(HKEY_CURRENT_USER, SOFTWARE_CLASSES, &hRootKey) != ERROR_SUCCESS)
        {
            RegCloseKey(hRootKey);
            return false;
        }

        // [hRootKey\.ext]
        if (RegSetValueW(hRootKey, extensionW.c_str(), REG_SZ, progIdNameW.c_str(), 0) != ERROR_SUCCESS)
        {
            RegCloseKey(hRootKey);
            return false;
        }

        if (RegCreateKeyW(hRootKey, progIdNameW.c_str(), &hKey) != ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            RegCloseKey(hRootKey);
            return false;
        }

        // [hRootKey\OpenRCT2.ext]
        if (RegSetValueW(hKey, nullptr, REG_SZ, fileTypeTextW.c_str(), 0) != ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            RegCloseKey(hRootKey);
            return false;
        }
        // [hRootKey\OpenRCT2.ext\DefaultIcon]
        wchar_t szIconW[MAX_PATH];
        printResult = swprintf_s(szIconW, MAX_PATH, L"\"%s\",%d", dllPathW, iconIndex);
        assert(printResult >= 0);
        if (RegSetValueW(hKey, L"DefaultIcon", REG_SZ, szIconW, 0) != ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            RegCloseKey(hRootKey);
            return false;
        }

        // [hRootKey\OpenRCT2.sv6\shell]
        if (RegSetValueW(hKey, L"shell", REG_SZ, L"open", 0) != ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            RegCloseKey(hRootKey);
            return false;
        }

        // [hRootKey\OpenRCT2.sv6\shell\open]
        if (RegSetValueW(hKey, L"shell\\open", REG_SZ, commandTextW.c_str(), 0) != ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            RegCloseKey(hRootKey);
            return false;
        }

        // [hRootKey\OpenRCT2.sv6\shell\open\command]
        wchar_t szCommandW[MAX_PATH];
        printResult = swprintf_s(szCommandW, MAX_PATH, L"\"%s\" %s", exePathW, commandArgsW.c_str());
        assert(printResult >= 0);
        if (RegSetValueW(hKey, L"shell\\open\\command", REG_SZ, szCommandW, 0) != ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            RegCloseKey(hRootKey);
            return false;
        }
        return true;
    }

    static void RemoveFileAssociation(const utf8* extension)
    {
        // [HKEY_CURRENT_USER\Software\Classes]
        HKEY hRootKey;
        if (RegOpenKeyW(HKEY_CURRENT_USER, SOFTWARE_CLASSES, &hRootKey) == ERROR_SUCCESS)
        {
            // [hRootKey\.ext]
            RegDeleteTreeW(hRootKey, String::ToWideChar(extension).c_str());

            // [hRootKey\OpenRCT2.ext]
            auto progIdName = GetProdIDName(extension);
            RegDeleteTreeW(hRootKey, progIdName.c_str());

            RegCloseKey(hRootKey);
        }
    }

    std::string FormatTime(std::time_t timestamp)
    {
        SYSTEMTIME st = TimeToSystemTime(timestamp);
        std::string result;

        wchar_t time[20];
        ptrdiff_t charsWritten = GetTimeFormatEx(
            LOCALE_NAME_USER_DEFAULT, 0, &st, nullptr, time, static_cast<int>(std::size(time)));
        if (charsWritten != 0)
        {
            result = String::ToUtf8(std::wstring_view(time, charsWritten - 1));
        }
        return result;
    }

    std::string FormatShortDate(std::time_t timestamp)
    {
        SYSTEMTIME st = TimeToSystemTime(timestamp);
        std::string result;

        wchar_t date[20];
        ptrdiff_t charsWritten = GetDateFormatEx(
            LOCALE_NAME_USER_DEFAULT, DATE_SHORTDATE, &st, nullptr, date, static_cast<int>(std::size(date)), nullptr);
        if (charsWritten != 0)
        {
            result = String::ToUtf8(std::wstring_view(date, charsWritten - 1));
        }
        return result;
    }
} // namespace OpenRCT2::Platform
#endif
