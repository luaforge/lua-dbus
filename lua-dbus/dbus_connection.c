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
#include "dbus_connection_shared.h"

//################################################################################
//################################################################################

int gConnectionMetatableRef = LUA_NOREF;

//################################################################################
//################################################################################

int push_dbus_connection( lua_State * const _L, DBusConnection * const _connection, int _closeOnFinalize)
{
	if ( _connection == 0x0 )
		return luaL_error( _L, "attempting to create a NULL connection");
	else
	{
		// create (or find an existing) fully functional userdata for our connection
		ConnectionUserdata * const block = (ConnectionUserdata *) utils_push_mapped_userdata( _L, _connection, gConnectionMetatableRef, sizeof(ConnectionUserdata));
		// create a news table and set it as the userdata's environment (it will be used to store filters)
		lua_newtable( _L);
		lua_setfenv( _L, -2);
		block->filterCallSequence = 0x0;
		block->nbRegisteredFilters = 0;
		// connection address is already stored at the beginning of the userdata block, just fill the rest
		printf( "new connection contents: %p(%p),%d\n", _connection, block->connection, _closeOnFinalize);
		if ( _closeOnFinalize >= 0 )
		{
			block->closeOnFinalize = _closeOnFinalize;
		}
		return 1;
	}
}

//################################################################################

static ConnectionUserdata * cast_to_dbus_connection_userdata( lua_State * const _L, int const _ndx)
{
	return (ConnectionUserdata *) utils_cast_userdata( _L, _ndx, gConnectionMetatableRef);
}

//################################################################################
//################################################################################

int finalize_dbus_connection( lua_State * const _L)
{
	utils_check_nargs( _L, 1);
	puts( "finalize_dbus_connection");
	ConnectionUserdata * const connectionUD = cast_to_dbus_connection_userdata( _L, 1);

	printf( "finalize_dbus_connection: connection=%p\n", connectionUD->connection);

	finalize_filter_data( _L, connectionUD);
	if ( connectionUD->closeOnFinalize != 0 )
	{
		printf( "closing connection: %d\n", connectionUD->closeOnFinalize);
		dbus_connection_close( connectionUD->connection);
	}
	puts( "unrefing connection");
	dbus_connection_unref( connectionUD->connection);
	puts( "finalize_dbus_connection; unref-ed");
	return 0;
}

//################################################################################
//################################################################################

static luaL_Reg gConnectionMeta[] =
{
	{ "__gc", finalize_dbus_connection },
	{ 0x0, 0x0 },
};

//################################################################################
//################################################################################

// should be called with the "dbus" library table on the top of the stack
void register_connection_stuff( lua_State * const _L)
{
	// register the connection object metatable in the registry
	utils_prepare_metatable( _L, &gConnectionMetatableRef);              // {meta}
	utils_register_upvalued_functions( _L, gSharedConnectionMeta, gConnectionMetatableRef);
	// replace/add whatever is specific to a non-bus connection
	utils_register_upvalued_functions( _L, gConnectionMeta, gConnectionMetatableRef);
	lua_pop( _L, 1);                                                     //
}
