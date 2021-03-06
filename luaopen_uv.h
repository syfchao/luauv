#ifndef LUAOPEN_UV_H
#define LUAOPEN_UV_H

/*
LuaUV

Copyright 2012 Graham Wakefield. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

extern "C" {
	#include "uv.h"
}

// lightweight C++ wrapper of Lua API:
#include "al_Lua.hpp"

#ifndef NDEBUG
	#define dprintf(args ...) fprintf(stderr, args)
#else
	#define dprintf(args ...) 
#endif

void init_handle_metatable(lua_State * L, int loopidx);
void init_fs_metatable(lua_State * L, int loopidx);
void init_stream_metatable(lua_State * L, int loopidx);
void init_tcp_metatable(lua_State * L, int loopidx);
void init_tty_metatable(lua_State * L, int loopidx);
void init_timer_metatable(lua_State * L, int loopidx);

int lua_uv_fs_open(lua_State * L);
int lua_uv_fs_readdir(lua_State * L);
int lua_uv_fs_stat(lua_State * L);
int lua_uv_tcp(lua_State * L);
int lua_uv_tty(lua_State * L);
int lua_uv_timer(lua_State * L);
int lua_uv_event(lua_State * L);
int lua_uv_handle_close(lua_State * L);

// gets the uv loop for a given lua_State
// (and leaves the userdatum on the stack)
uv_loop_t * lua_uv_main_loop(lua_State * L);

// grab the loop from the closure:
#define lua_uv_loop(L) (*(uv_loop_t **)luaL_checkudata(L, lua_upvalueindex(1), classname<uv_loop_t>()))

// using type traits to attach names to types:
template<typename T> const char * classname();
template<> inline const char * classname<uv_loop_t>() { return "uv.loop"; }
template<> inline const char * classname<uv_buf_t>() { return "uv.buf"; }
template<> inline const char * classname<uv_handle_t>() { return "uv.handle"; }
template<> inline const char * classname<uv_timer_t>() { return "uv.timer"; }
template<> inline const char * classname<uv_stream_t>() { return "uv.stream"; }
template<> inline const char * classname<uv_tty_t>() { return "uv.tty"; }
template<> inline const char * classname<uv_tcp_t>() { return "uv.tcp"; }
template<> inline const char * classname<uv_file>() { return "uv.file"; }


inline void lua_super_metatable(lua_State * L, int idx, const char * supername) {
	if (idx < 0) idx = lua_gettop(L) + idx + 1;
	luaL_getmetatable(L, supername);
	lua_setmetatable(L, idx);
}

// tests equality of metatable (recursing to super-metatables)
inline void * lua_super_checkudata(lua_State * L, int idx, const char * metaname) {
	if (idx < 0) idx = lua_gettop(L) + idx + 1;
	luaL_getmetatable(L, metaname);	// value to test against
	if (lua_isnoneornil(L, -1)) 
		luaL_error(L, "%s metatable not defined", metaname);
	lua_getmetatable(L, idx);
	while (!lua_isnoneornil(L, -1)) {
		if (lua_rawequal(L, -1, -2)) {
			lua_pop(L, 2);
			return lua_touserdata(L, idx);
		} else {
			if (!lua_getmetatable(L, -1)) break;
			lua_replace(L, -2);
		}
	}
	lua_pop(L, 2);	// fail
	return false;
}

template<typename T>
inline T * lua_generic_object(lua_State * L, int idx=1) {
	T * v = (T *)lua_super_checkudata(L, idx, classname<T>());
	if (v==0) luaL_error(L, "missing %s", classname<T>());
	return v;
}

template<typename T>
inline int lua_generic___tostring(lua_State * L) {
	lua_pushfstring(L, "%s(%p)", classname<T>(), lua_touserdata(L, 1));
	return 1;
}

// other utilities:

// signal on last error:
void lua_uv_ok(lua_State * L);
// allocate memory:
uv_buf_t lua_uv_alloc(uv_handle_t *, size_t suggested_size);
// allocates as a userdata
uv_buf_t * lua_uv_buf_init(lua_State * L, size_t len);
// allocates as a userdata & copies in data
uv_buf_t * lua_uv_buf_init(lua_State * L, char * data, size_t len);

/*
	Utilities for handling one-shot asynchronous callbacks via coroutines:
*/

// expects the coroutine itself to be at the top of the stack:
template<typename T>
inline T * callback_resume_before(lua_State * L, int settop) {
	// map coro to a pointer, to prevent gc:
	lua_pushlightuserdata(L, L);
	lua_rawset(L, LUA_REGISTRYINDEX);

	// choose how much to keep on the stack to prevent gc:
	lua_settop(L, settop);
	
	// allocate the request as a userdatum on the stack:
	T * req = (T *)lua_newuserdata(L, sizeof(T));
	luaL_getmetatable(L, "uv.request");
	lua_setmetatable(L, -2); // just for debugging really
	// request needs to know the lua_State ptr:
	req->data = L;	
	return req;
}

inline void callback_resume_after(lua_State * L) {
	// let stack be collected.
	lua_settop(L, 0);
	
	// un-map coro:
	lua_pushthread(L);
	lua_pushnil(L);
	lua_rawset(L, LUA_REGISTRYINDEX);
}

#endif
