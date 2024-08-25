/*****************************************************************************
 * Copyright (c) 2014-2024 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#if defined(_WIN32_WINNT) && (_WIN32_WINNT < 0x0600)
namespace OpenRCT2::Platform
{
    uint16_t GetLocaleLanguage()
    {
        return LANGUAGE_UNDEFINED;
    }

    CurrencyType GetLocaleCurrency()
    {
        wchar_t currCode[9];
        if (GetLocaleInfoW(LOCALE_USER_DEFAULT, LOCALE_SINTLSYMBOL, currCode, static_cast<int>(std::size(currCode))) == 0)
        {
            return Platform::GetCurrencyValue(nullptr);
        }

        return Platform::GetCurrencyValue(String::ToUtf8(currCode).c_str());
    }

    MeasurementFormat GetLocaleMeasurementFormat()
    {
        UINT measurement_system;
        if (GetLocaleInfoW(
                LOCALE_USER_DEFAULT, LOCALE_IMEASURE | LOCALE_RETURN_NUMBER, reinterpret_cast<LPWSTR>(&measurement_system),
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
        if (GetLocaleInfoW(LOCALE_USER_DEFAULT, LOCALE_SSHORTDATE, dateFormat, static_cast<int>(std::size(dateFormat))) == 0)
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
        if (swscanf(dateFormat, L"%l[dyM]%*l[^dyM]%l[dyM]%*l[^dyM]%*l[dyM]", first, second) != 2)
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
        if (GetLocaleInfoW(
                LOCALE_USER_DEFAULT, LOCALE_IMEASURE | LOCALE_RETURN_NUMBER, reinterpret_cast<LPWSTR>(&fahrenheit),
                sizeof(fahrenheit) / sizeof(wchar_t))
            == 0)
        {
            // Assume celsius by default if function call fails
            return TemperatureUnit::Celsius;
        }

        return fahrenheit == 1 ? TemperatureUnit::Fahrenheit : TemperatureUnit::Celsius;
    }

    static std::string WIN32_GetKnownFolderPath(int csidl)
    {
        std::string path;
        wchar_t* wpath = nullptr;

        *wpath = (PWSTR)CoTaskMemAlloc(MAX_PATH * sizeof(WCHAR));
        if (SUCCEEDED(SHGetFolderPathW(NULL, csidl, nullptr, SHGFP_TYPE_CURRENT, *wpath)))
        {
            path = String::ToUtf8(wpath);
        }
        CoTaskMemFree(wpath);
        return path;
    }

    bool SetUpFileAssociation(
        std::string_view extension, std::string_view fileTypeText, std::string_view commandText, std::string_view commandArgs,
        const uint32_t iconIndex)
    {
        return false;
    }

    static void RemoveFileAssociation(const utf8* extension)
    {
    }

    std::string FormatTime(std::time_t timestamp)
    {
        SYSTEMTIME st = TimeToSystemTime(timestamp);
        std::string result;

        wchar_t time[20];
        ptrdiff_t charsWritten = GetTimeFormatW(LOCALE_USER_DEFAULT, 0, &st, nullptr, time, static_cast<int>(std::size(time)));
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
        ptrdiff_t charsWritten = GetDateFormatW(
            LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, nullptr, date, static_cast<int>(std::size(date)), nullptr);
        if (charsWritten != 0)
        {
            result = String::ToUtf8(std::wstring_view(date, charsWritten - 1));
        }
        return result;
    }
} // namespace OpenRCT2::Platform
#endif
