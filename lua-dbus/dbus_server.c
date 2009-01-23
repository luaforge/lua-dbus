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

//################################################################################
//################################################################################

int gServerMetatableRef = LUA_NOREF;

//################################################################################
//################################################################################

int push_dbus_server( lua_State * const _L, DBusServer * const _server)
{
	if ( _server == 0x0 )
		return luaL_error( _L, "attempting to create a NULL server");
	else
	{
		// create (or find an existing) fully functional userdata for our connection
		(void) utils_push_mapped_userdata( _L, _server, gServerMetatableRef, sizeof(void*));
		return 1;
	}
}

//################################################################################

DBusServer * cast_to_dbus_server( lua_State * const _L,  int const _ndx)
{
	return * (DBusServer **) utils_cast_userdata( _L, _ndx, gServerMetatableRef);
}

//################################################################################
//################################################################################

int bind_dbus_server_listen( lua_State * const _L)
{
	// first, check that we have a single string argument, the server's address
	utils_check_nargs( _L, 1);
	luaL_argcheck( _L, lua_isstring( _L, 1), 1, "must provide server address");

	// create our server
	char const * const address = lua_tostring( _L, -1);
	DBusError error;
	dbus_error_init( &error);
	DBusServer * const server = dbus_server_listen( address, &error);
	// in case of error, return NIL instead of the server userdata, plus error messages
	if ( server == 0x0 )
	{
		if ( dbus_error_is_set( &error) )
		{
			lua_pushnil( _L);
			lua_pushstring( _L, error.name);
			lua_pushstring( _L, error.message);
			dbus_error_free( &error);
		}
		else
		{
			// somehow, we get a NULL server but no error reporting. should not happen, but who knows
			lua_pushnil( _L);
			lua_pushliteral( _L, "unknown error");
			lua_pushliteral( _L, "NULL server, but no error was reported");
		}
			return 3;
	}

	return push_dbus_server( _L, server);
}

//################################################################################

int bind_dbus_server_disconnect( lua_State * const _L)
{
	utils_check_nargs( _L, 1);
	// will raise an error if argument is not a dbus server userdata or if data is invalid
	DBusServer * server = cast_to_dbus_server( _L, 1);

	dbus_server_disconnect( server);
	return 0;
}

//################################################################################

int bind_dbus_server_get_is_connected( lua_State * const _L)
{
	utils_check_nargs( _L, 1);
	// will raise an error if argument is not a dbus server userdata or if data is invalid
	DBusServer * server = cast_to_dbus_server( _L, 1);
	lua_pushboolean( _L, dbus_server_get_is_connected( server) ? 1 : 0);
	return 1;
}

//################################################################################

int finalize_dbus_server( lua_State * const _L)
{
	utils_check_nargs( _L, 1);
	DBusServer * const server = cast_to_dbus_server( _L, 1);
	dbus_server_disconnect( server);
	dbus_server_unref( server);
	return 0;
}

//################################################################################
//################################################################################

luaL_Reg gServerMeta[] =
{
	{ "disconnect", bind_dbus_server_disconnect },
	{ "is_connected", bind_dbus_server_get_is_connected },
	{ "__gc", finalize_dbus_server },
	{ 0x0, 0x0 },
};

//################################################################################
//################################################################################

// should be called with the "dbus" library table on the top of the stack
void register_server_stuff( lua_State * const _L)
{
	// register the connection object metatable in the registry
	utils_prepare_metatable( _L, &gServerMetatableRef);                       // {meta}
	utils_register_upvalued_functions( _L, gServerMeta, gServerMetatableRef); // {meta}
	lua_pop( _L, 1);                                                          //
}
