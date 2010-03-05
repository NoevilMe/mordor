// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/pch.h"

#include "config.h"

#include <algorithm>

#include "mordor/string.h"
#include "util.h"

#ifndef WINDOWS
#ifndef OSX
extern char **environ;
#endif
#endif

#ifdef OSX
#include <crt_externs.h>
#endif

namespace Mordor {

void
Config::loadFromEnvironment()
{
#ifdef WINDOWS
    wchar_t *enviro = GetEnvironmentStringsW();
    if (!enviro)
        return;
    boost::shared_ptr<wchar_t> environScope(enviro, &FreeEnvironmentStringsW);
    for (const wchar_t *env = enviro; *env; env += wcslen(env) + 1) {
        const wchar_t *equals = wcschr(env, '=');
        if (!equals)
            continue;
        if (equals == env)
            continue;
        std::string key(toUtf8(env, equals - env));
        std::string value(toUtf8(equals + 1));
#else
#ifdef OSX
	char **environ = *_NSGetEnviron();
#endif
    if (!environ)
        return;
    for (const char *env = *environ; *env; env += strlen(env) + 1) {
        const char *equals = strchr(env, '=');
        if (!equals)
            continue;
        if (equals == env)
            continue;
        std::string key(env, equals - env);
        std::string value(equals + 1);
#endif
        std::transform(key.begin(), key.end(), key.begin(), tolower);
        replace(key, '_', '.');
        if (key.find_first_not_of("abcdefghijklmnopqrstuvwxyz.") != std::string::npos)
            continue;
        ConfigVarBase::ptr var = lookup(key);
        if (var)
            var->fromString(value);
    }
}

ConfigVarBase::ptr
Config::lookup(const std::string &name)
{
    ConfigVarBase var(name);
    ConfigVarBase::ptr ptr(&var, &nop<ConfigVarBase *>);
    std::set<ConfigVarBase::ptr, ConfigVarBase::Comparator>::iterator it = vars().find(ptr);
    if (it != vars().end())
        return *it;
    return ConfigVarBase::ptr();
}

void
Config::visit(boost::function<void (ConfigVarBase::ptr)> dg)
{
    for (std::set<ConfigVarBase::ptr,
            ConfigVarBase::Comparator>::const_iterator it = vars().begin();
        it != vars().end();
        ++it) {
        dg(*it);
    }
}

bool
ConfigVarBase::Comparator::operator()(const ConfigVarBase::ptr &lhs,
                                      const ConfigVarBase::ptr &rhs) const
{
    MORDOR_ASSERT(lhs);
    MORDOR_ASSERT(rhs);
    return lhs->m_name < rhs->m_name;
}

}
