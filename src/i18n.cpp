#include <acul/string/utils.hpp>
#include <alwf/i18n.hpp>
#include <clocale>
#include <cstdlib>
#ifdef _WIN32
    #include <winnls.h>
#endif

namespace alwf::locales
{
    using acul::string;
    void setup_i18n(const string &locale)
    {
#ifdef _WIN32
        // Synchronize Win32 and CRT environments; inherited LC_ALL must not override the catalog.
        SetEnvironmentVariableA("LC_ALL", nullptr);
        _putenv_s("LC_ALL", "");
        SetEnvironmentVariableA("LC_MESSAGES", locale.c_str());
        _putenv_s("LC_MESSAGES", locale.c_str());
        SetEnvironmentVariableA("LANGUAGE", locale.c_str());
        _putenv_s("LANGUAGE", locale.c_str());

        const auto wlocale = acul::utf8_to_utf16(locale);
        _configthreadlocale(_ENABLE_PER_THREAD_LOCALE);
        const auto localeId = LocaleNameToLCID(reinterpret_cast<LPCWSTR>(wlocale.c_str()), LOCALE_ALLOW_NEUTRAL_NAMES);
        if (localeId != 0u) SetThreadLocale(localeId);
#else
        // Use the host libc locale; gettext catalogs need no separately installed system locale.
        setlocale(LC_MESSAGES, "");
        setenv("LANGUAGE", locale.c_str(), 1);
#endif
    }

    void bind_domain(const char *domain, const string &directory)
    {
#ifdef _WIN32
        const auto wide = acul::utf8_to_utf16(directory);
        wbindtextdomain(domain, reinterpret_cast<LPCWSTR>(wide.c_str()));
#else
        bindtextdomain(domain, directory.c_str());
#endif
        bind_textdomain_codeset(domain, "UTF-8");
        textdomain(domain);
    }

    string get_user_language(const char **pLanguages, size_t count)
    {
        auto find_supported = [pLanguages, count](const string &language) -> string {
            for (size_t i = 0; i < count; ++i)
                if (language == pLanguages[i]) return pLanguages[i];
            return {};
        };

#ifdef _WIN32
        wchar_t locale_name[LOCALE_NAME_MAX_LENGTH]{};
        wchar_t language_name[LOCALE_NAME_MAX_LENGTH]{};
        if (GetUserDefaultLocaleName(locale_name, LOCALE_NAME_MAX_LENGTH) > 0 &&
            GetLocaleInfoEx(locale_name, LOCALE_SISO639LANGNAME, language_name, LOCALE_NAME_MAX_LENGTH) > 0)
        {
            const auto language =
                acul::utf16_to_utf8(acul::u16string(reinterpret_cast<const char16_t *>(language_name)));
            if (auto supported = find_supported(language); !supported.empty()) return supported;
        }
#endif

        const char *resolved_locale = setlocale(LC_MESSAGES, "");
        string shell_locale = resolved_locale ? resolved_locale : "";
        size_t language_end = shell_locale.size();
        for (const char *separator : {"_", ".", "-"})
        {
            const size_t position = shell_locale.find(separator);
            if (position != SIZE_MAX && position < language_end) language_end = position;
        }
        const string short_locale = shell_locale.substr(0, language_end);
        if (auto supported = find_supported(short_locale); !supported.empty()) return supported;
        return count > 0 ? pLanguages[0] : string{};
    }
} // namespace alwf::locales