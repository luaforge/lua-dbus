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
#include <string.h>

#include "utils.h"
#include "dbus_connection_shared.h"

//################################################################################
//################################################################################

int gBusMetatableRef = LUA_NOREF;

//################################################################################
//################################################################################

int push_dbus_bus( lua_State * const _L, DBusConnection * const _connection)
{
	if ( _connection == 0x0 )
		return luaL_error( _L, "push_dbus_bus: attempting to create a NULL connection");
	else
	{
		// create (or find an existing) fully functional userdata for our connection
		ConnectionUserdata * const block = (ConnectionUserdata *) utils_push_mapped_userdata( _L, _connection, gBusMetatableRef, sizeof(ConnectionUserdata));
		// not necessary because our finalizer implementation doesn't care...
		block->closeOnFinalize = 0;
		// create a news table and set it as the userdata's environment (it will be used to store filters)
		lua_newtable( _L);
		lua_setfenv( _L, -2);
		block->filterCallSequence = 0x0;
		block->nbRegisteredFilters = 0;
		// connection address is already stored at the beginning of the userdata block, just fill the rest
		printf( "push_dbus_bus: new connection contents: %p(%p)\n", _connection, block->connection);
		return 1;
	}
}

//################################################################################

static ConnectionUserdata * cast_to_dbus_bus_userdata( lua_State * const _L, int const _ndx)
{
	return (ConnectionUserdata *) utils_cast_userdata( _L, _ndx, gBusMetatableRef);
}
//################################################################################
//################################################################################

int bind_dbus_bus_get( lua_State * const _L)
{
	utils_check_nargs( _L, 1);
	DBusError error;
	dbus_error_init( &error);
	DBusBusType const type = utils_convert_to_bus_type( _L, 1);
	DBusConnection * const connection = dbus_bus_get( type, &error);
	if ( connection == 0x0 )
	{
		luaL_error( _L, "failed to connect to the D-BUS daemon: %s %s", error.name, error.message);
		dbus_error_free( &error);
		return 0;
	}
	push_dbus_bus( _L, connection);
	//TODO: register a fixed C filter that will be in charge of calling lua filters registered later
	//dbus_connection_add_filter( connection, handler_func, _L, (DBusFreeDataFunction) 0x0);
	return 1;
}

//################################################################################

int bind_dbus_bus_add_match( lua_State * const _L)
{
	// first argument should be a bus (which is just a special connection)
	DBusConnection * const connection = extract_dbus_connection_pointer( _L, 1, gBusMetatableRef);

	char rules_buffer[DBUS_MAXIMUM_MATCH_RULE_LENGTH];
	utils_fill_rule_buffer_from_table( _L, 2, rules_buffer);

	DBusError error;
	dbus_error_init( &error);
	dbus_bus_add_match( connection, rules_buffer, &error);
	if ( dbus_error_is_set( &error) )
	{
		luaL_error( _L, "failed to add the match rule %s: %s %s", rules_buffer, error.name, error.message);
		dbus_error_free( &error);
	}
	else
	{
		printf( "bind_dbus_bus_add_match: adding rule %s\n", rules_buffer);
	}
	return 0;
}

//################################################################################

int bind_dbus_bus_remove_match( lua_State * const _L)
{
	// first argument should be a bus (which is just a special connection)
	DBusConnection * const connection = extract_dbus_connection_pointer( _L, 1, gBusMetatableRef);

	char rules_buffer[DBUS_MAXIMUM_MATCH_RULE_LENGTH];
	utils_fill_rule_buffer_from_table( _L, 2, rules_buffer);

	DBusError error;
	dbus_error_init( &error);
	dbus_bus_remove_match( connection, rules_buffer, &error);
	if ( dbus_error_is_set( &error) )
	{
		luaL_error( _L, "failed to remove the rule %s: %s %s", rules_buffer, error.name, error.message);
		dbus_error_free( &error);
	}
	else
	{
		printf( "bind_dbus_bus_remove_match: removed rule %s\n", rules_buffer);
	}
	return 0;
}

//################################################################################

int finalize_dbus_bus( lua_State * const _L)
{
	utils_check_nargs( _L, 1);
	puts( "finalize_dbus_bus");
	ConnectionUserdata * const connectionUD = cast_to_dbus_bus_userdata( _L, 1);
	printf( "finalize_dbus_bus: connection=%p\n", connectionUD->connection);

	finalize_filter_data( _L, connectionUD);
	puts( "finalize_dbus_bus: unrefing connection");
	dbus_connection_unref( connectionUD->connection);
	puts( "finalize_dbus_bus; unref-ed");
	return 0;
}

//################################################################################
//################################################################################

static luaL_Reg gBusMeta[] =
{
	{ "add_match", bind_dbus_bus_add_match },
	{ "remove_match", bind_dbus_bus_remove_match },
	{ "__gc", finalize_dbus_bus },
	{ 0x0, 0x0 },
};

//################################################################################
//################################################################################

extern void register_shared_connection_stuff( lua_State * const _L, int const _mtRef);

// should be called with the "dbus" library table on the top of the stack
void register_bus_stuff( lua_State * const _L)
{
	// register the connection object metatable in the registry
	utils_prepare_metatable( _L, &gBusMetatableRef);              // {meta}
	utils_register_upvalued_functions( _L, gSharedConnectionMeta, gBusMetatableRef);
	// replace/add whatever is specific to a bus connection
	utils_register_upvalued_functions( _L, gBusMeta, gBusMetatableRef);
	lua_pop( _L, 1);                                                     //
}
