#include "nuperf/nuperf-api.h"

#include <sol/sol.hpp>

namespace nuperf::lua_bindings {
void bind(sol::state_view lua);
}

extern "C" int luaopen_lnuperf(lua_State *L) {
    try {
        if (L == nullptr) {
            return 0;
        }

        nuperf_status_t st = nuperf_init();
        if (st != NUPERF_OK) {
            lua_pushnil(L);
            lua_pushstring(L, nuperf_strerror(st));
            return 2;
        }

        sol::state_view lua(L);
        nuperf::lua_bindings::bind(lua);
        lua_getglobal(L, "nuperf");
        return 1;
    } catch (const std::exception &e) {
        lua_pushnil(L);
        lua_pushstring(L, e.what());
        return 2;
    } catch (...) {
        lua_pushnil(L);
        lua_pushstring(L, "unknown lnuperf error");
        return 2;
    }
}
