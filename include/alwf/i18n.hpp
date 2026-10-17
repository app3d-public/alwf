#pragma once
#include <acul/string/string.hpp>
#include <libintl.h>
#ifndef _
    #define _(STRING) gettext(STRING)
#endif

namespace alwf::locales
{
    void setup_i18n(const acul::string &locale);
    void bind_domain(const char *domain, const acul::string &directory);
    acul::string get_user_language(const char **languages, size_t count);
} // namespace alwf::locales
