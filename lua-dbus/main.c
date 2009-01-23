/************************************************************************
*
* Lua bindings for libdbus.
* Copyright (c) 2009 - BenoitGermain <bnt.germain@gmail.com>
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
************************************************************************/

#include <lua.h>
#include <lauxlib.h>

#include "utils.h"
#include "dbus_bus.h"
#include "dbus_connection.h"
#include "dbus_message.h"
#include "dbus_server.h"

//################################################################################
// centralize all the registry references here
//################################################################################

luaL_Reg gDBusAPI[] =
{
	{ "bus_get", bind_dbus_bus_get },
	{ "message_new", bind_dbus_message_new } ,
	{ "message_new_method_call", bind_dbus_message_new_method_call } ,
	{ "message_new_method_return", bind_dbus_message_new_method_return } ,
	{ "message_new_signal", bind_dbus_message_new_signal } ,
	{ "server_listen", bind_dbus_server_listen },
	{ 0x0, 0x0 },
};

//################################################################################

int luaopen_dbus( lua_State * const _L)
{
	utils_init();
	// add a table in the registry to hold all C pointer / userdata equivalents
	// the table has "weak values" mode
	lua_pushliteral( _L, "dbus_userdata_map");  // "dbus_userdata_map"
	lua_newtable( _L);                          // "dbus_userdata_map" {}
	lua_newtable( _L);                          // "dbus_userdata_map" {} map_meta
	lua_pushliteral( _L, "__mode");             // "dbus_userdata_map" {} map_meta "__mode"
	lua_pushliteral( _L, "v");                  // "dbus_userdata_map" {} map_meta "__mode" "v"
	lua_settable( _L, -3);                      // "dbus_userdata_map" {} map_meta
	lua_setmetatable( _L, -2);                  // "dbus_userdata_map" {}
	lua_settable( _L, LUA_REGISTRYINDEX);       //

	register_server_stuff( _L);                 //
	register_connection_stuff( _L);             //
	register_bus_stuff( _L);                    //
	register_message_stuff( _L);                //
	luaL_register( _L, "dbus", gDBusAPI);       // {dbus}

	return 1;
}
