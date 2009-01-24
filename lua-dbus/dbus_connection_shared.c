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
#include <memory.h>

#include "utils.h"
#include "dbus_connection_shared.h"

//################################################################################
// contains bindings that are shared by the 'bus' and 'connection' types
// since a bus is just a connection with a few constraints and specific APIs
//################################################################################

extern int gBusMetatableRef;
extern int gConnectionMetatableRef;
extern DBusMessage * cast_to_dbus_message( lua_State * const _L,  int const _ndx);
extern int push_dbus_message( lua_State * const _L, DBusMessage * const _message);

//################################################################################
//################################################################################

DBusConnection * extract_dbus_connection_pointer( lua_State * const _L, int const _ndx, int const _whichMeta)
{
	// note that all this could be replaced by the following single line, but I like to check for errors...
	// in that case, I wouldn't even need to register an upvalue with the bound functions
	// FASTER VERSION: return *(DBusConnection **) lua_touserdata( _L, _ndx);
	lua_pushvalue( _L, lua_upvalueindex(1));
	luaL_checktype( _L, -1, LUA_TLIGHTUSERDATA);
	int const metaRef = (int) lua_touserdata( _L, -1);
	lua_pop( _L, 1);
	// raise an error if necessary
	if ( (_whichMeta < 0) ? (metaRef != gBusMetatableRef && metaRef != gConnectionMetatableRef) : (metaRef != _whichMeta) )
		return luaL_error( _L, "internal error, wrong metatable reference in upvalue"), (DBusConnection *) 0x0;
	// by convention, the C-side object pointer is always at the beginning of the userdata block
	return *(DBusConnection **) utils_cast_userdata( _L, _ndx, metaRef);
}

//################################################################################

static ConnectionUserdata * extract_dbus_connection_userdata( lua_State * const _L, int const _ndx, int const _whichMeta)
{
	// note that all this could be replaced by the following single line, but I like to check for errors...
	// in that case, I wouldn't even need to register an upvalue with the bound functions
	// FASTER VERSION: return (ConnectionUserdata *) lua_touserdata( _L, _ndx);
	lua_pushvalue( _L, lua_upvalueindex(1));
	luaL_checktype( _L, -1, LUA_TLIGHTUSERDATA);
	int const metaRef = (int) lua_touserdata( _L, -1);
	lua_pop( _L, 1);
	// raise an error if necessary
	if ( (_whichMeta < 0) ? (metaRef != gBusMetatableRef && metaRef != gConnectionMetatableRef) : (metaRef != _whichMeta) )
		return luaL_error( _L, "internal error, wrong metatable reference in upvalue"), (ConnectionUserdata *) 0x0;
	// by convention, the C-side object pointer is always at the beginning of the userdata block
	return (ConnectionUserdata *) lua_touserdata( _L, _ndx);
}

//################################################################################
//################################################################################

// filters can be added to a connection
// of course, la lua binding will want to add lua filters, and we need a C filter
// that will be in charge of invoking them
DBusHandlerResult private_call_lua_filters( DBusConnection *_connection, DBusMessage *_message, void *_user_data)
{
	lua_State * const L = (lua_State *) _user_data;
	// fetch the userdata object associated with this connection
	utils_fetch_userdata( L, _connection);                          // U
	// grab a pointer
	ConnectionUserdata * const ud = extract_dbus_connection_userdata( L, -1, -1);
	// create a userdata for the message object we got
	push_dbus_message( L, _message);                                // U msg
	// fetch the userdata's environment
	lua_getfenv( L, -2);                                            // U msg {env}
	// fetch the filters table
	lua_getfield( L, -1, "filters");                                // U msg {env} {filters}
	int index;
	for( index = 0; index < ud->nbRegisteredFilters; ++ index)
	{
		lua_rawgeti( L, -1, ud->filterCallSequence[index]);          // U msg {env} {filters} filter
		lua_pushvalue( L, -5);                                       // U msg {env} {filters} filter U
		lua_pushvalue( L, -5);                                       // U msg {env} {filters} filter U msg
		// call the filter with two arguments (the connection and the message), expect 1 return value, no error handler
		int const success = lua_pcall( L, 2, 1, 0);                  // U msg {env} {filters} retval
		if ( success == 0 )
		{
			DBusHandlerResult hresult = utils_convert_to_handler_result( L, -1);
			if ( hresult == DBUS_HANDLER_RESULT_NEED_MEMORY || hresult == DBUS_HANDLER_RESULT_HANDLED)
			{
				lua_pop( L, 5);                                        //
				return hresult;
			}
		}
		else
		{
			// some internal error occured with the pcall processing
			// tell the filter engine to stop filtering stuff
			lua_pop( L, 5);                                           //
			return luaL_error( L, "internal error will calling handlers!"), DBUS_HANDLER_RESULT_HANDLED;
		}
		lua_pop( L, 1);                                              // U msg {env} {filters}
	}
	// if we get here, this means that no filter handled the message so far
	lua_pop( L, 4);                                                 //
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

//################################################################################
//################################################################################

int bind_dbus_connection_add_filter( lua_State * const _L)
{
	// fetch the allocation function, we are going to need it
	void *allocUserData;
	lua_Alloc allocFunction = lua_getallocf( _L, &allocUserData);

	// should have two arguments: the connection, and the filter function
	utils_check_nargs( _L, 2);                                                      // U f
	// this will raise an error if argument #1 is not a connection or a bus
	ConnectionUserdata * const ud = extract_dbus_connection_userdata( _L, 1, -1);
	// filters are functions that we call in sequence
	// we store them in a table inside the userdata's environment table
	// first, create this infrastructure if it doesn't exist yet
	lua_getfenv( _L, -2);                                                           // U f {env}
	lua_getfield( _L, -1, "filters");                                               // U f {env} {nil/filters?}
	if ( lua_isnil( _L, -1) )
	{
		lua_pop( _L, 1);                                                             // U f {env}
		lua_newtable( _L);                                                           // U f {env} {filters}
		lua_pushvalue( _L, -1);                                                      // U f {env} {filters} {filters}
		lua_setfield( _L, -3, "filters");                                            // U f {env} {filters}
	}
	// here we are sure we have an environment, with a table named "filters"
	// grow our array of call sequence by one slot
	ud->filterCallSequence = allocFunction( allocUserData, ud->filterCallSequence, ud->nbRegisteredFilters * sizeof(int), (ud->nbRegisteredFilters + 1) * sizeof(int));
	lua_pushvalue( _L, 2);                                                          // U f {env} {filters} f
	ud->filterCallSequence[ud->nbRegisteredFilters] = luaL_ref( _L, -2);            // U f {env} {filters}
	++ ud->nbRegisteredFilters;
	lua_pop( _L, 4);                                                                //
	return 0;
}

//################################################################################

int bind_dbus_connection_borrow_message( lua_State * const _L)
{
	utils_check_nargs( _L, 1);
	DBusConnection * const connection = extract_dbus_connection_pointer( _L, 1, -1);
	DBusMessage * const message = dbus_connection_borrow_message( connection);
	if( message == 0x0)
	{
		lua_pushnil( _L);
		return 1;
	}
	else
	{
		return push_dbus_message( _L, message);
	}
}

//################################################################################

int bind_dbus_connection_dispatch( lua_State * const _L)
{
	utils_check_nargs( _L, 1);
	DBusConnection * const connection = extract_dbus_connection_pointer( _L, 1, -1);
	dbus_connection_dispatch( connection);
	return 0;
}

//################################################################################

int bind_dbus_connection_flush( lua_State * const _L)
{
	utils_check_nargs( _L, 1);
	DBusConnection * const connection = extract_dbus_connection_pointer( _L, 1, -1);
	dbus_connection_flush( connection);
	return 0;
}

//################################################################################

int bind_dbus_connection_get_dispatch_status( lua_State * const _L)
{
	utils_check_nargs( _L, 1);
	DBusConnection * const connection = extract_dbus_connection_pointer( _L, 1, -1);
	DBusDispatchStatus const status = dbus_connection_get_dispatch_status( connection);
	char const * string = 0x0;
	switch( status)
	{
		case DBUS_DISPATCH_DATA_REMAINS: string = "DBUS_DISPATCH_DATA_REMAINS"; break;
		case DBUS_DISPATCH_COMPLETE: string = "DBUS_DISPATCH_COMPLETE"; break;
		case DBUS_DISPATCH_NEED_MEMORY: string = "DBUS_DISPATCH_NEED_MEMORY"; break;
	}
	lua_pushstring( _L, string);
	return 1;
}

//################################################################################

int bind_dbus_connection_get_is_connected( lua_State * const _L)
{
	utils_check_nargs( _L, 1);
	DBusConnection * const connection = extract_dbus_connection_pointer( _L, 1, -1);
	lua_pushboolean( _L, dbus_connection_get_is_connected( connection) != 0);
	return 1;
}

//################################################################################

int bind_dbus_connection_get_is_authenticated( lua_State * const _L)
{
	utils_check_nargs( _L, 1);
	DBusConnection * const connection = extract_dbus_connection_pointer( _L, 1, -1);
	lua_pushboolean( _L, dbus_connection_get_is_authenticated( connection) != 0);
	return 1;
}

//################################################################################

int bind_dbus_connection_get_is_anonymous( lua_State * const _L)
{
	utils_check_nargs( _L, 1);
	DBusConnection * const connection = extract_dbus_connection_pointer( _L, 1, -1);
	lua_pushboolean( _L, dbus_connection_get_is_anonymous( connection) != 0);
	return 1;
}

//################################################################################

int bind_dbus_connection_get_server_id( lua_State * const _L)
{
	utils_check_nargs( _L, 1);
	DBusConnection * const connection = extract_dbus_connection_pointer( _L, 1, -1);
	char const * id = dbus_connection_get_server_id( connection);
	if ( id == 0x0 )
		return 0 ;
	lua_pushstring( _L, id);
	return 1;
}

//################################################################################

int bind_dbus_connection_pop_message( lua_State * const _L)
{
	utils_check_nargs( _L, 1);
	DBusConnection * const connection = extract_dbus_connection_pointer( _L, 1, -1);
	DBusMessage * const message = dbus_connection_pop_message( connection);
	if( message == 0x0)
	{
		lua_pushnil( _L);
		return 1;
	}
	else
	{
		return push_dbus_message( _L, message);
	}
}

//################################################################################

int bind_dbus_connection_read_write( lua_State * const _L)
{
	utils_check_nargs( _L, 2);
	DBusConnection * const connection = extract_dbus_connection_pointer( _L, 1, -1);
	int timeout = lua_tonumber( _L, 2);
	DBusDispatchStatus status = dbus_connection_read_write( connection, timeout);
	char const *string = 0x0;
	switch( status)
	{
		case DBUS_DISPATCH_DATA_REMAINS: string = "DBUS_DISPATCH_DATA_REMAINS"; break;
		case DBUS_DISPATCH_COMPLETE: string = "DBUS_DISPATCH_COMPLETE"; break;
		case DBUS_DISPATCH_NEED_MEMORY: string = "DBUS_DISPATCH_NEED_MEMORY"; break;
	}
	lua_pushstring( _L, string);
	return 1;
}

//################################################################################

int bind_dbus_connection_read_write_dispatch( lua_State * const _L)
{
	utils_check_nargs( _L, 2);
	DBusConnection * const connection = extract_dbus_connection_pointer( _L, 1, -1);
	int timeout = lua_tonumber( _L, 2);
	dbus_bool_t status = dbus_connection_read_write_dispatch( connection, timeout);
	lua_pushboolean( _L, status);
	return 1;
}

//################################################################################

int bind_dbus_connection_remove_filter( lua_State * const _L)
{
	// fetch the allocation function, we are going to need it
	void *allocUserData;
	lua_Alloc allocFunction = lua_getallocf( _L, &allocUserData);

	// should have two arguments: the connection, and the filter function
	utils_check_nargs( _L, 2);                                                      // U f
	// this will raise an error if argument #1 is not a connection or a bus
	ConnectionUserdata * const ud = extract_dbus_connection_userdata( _L, 1, -1);
	// search backwards in our call sequence for the specified function
	lua_getfenv( _L, -2);                                                           // U f {env}
	lua_getfield( _L, -1, "filters");                                               // U f {env} {nil/filters?}
	if ( lua_isnil( _L, -1) )
	{
		return luaL_error( _L, "it is not allowed to remove an unregistered filter!");
	}
	int i;
	for ( i = ud->nbRegisteredFilters-1; i>= 0; -- i)
	{
		int const filterRef = ud->filterCallSequence[i];
		lua_rawgeti( _L, -1, filterRef);                                             // U f {env} {filters} filter
		if ( lua_isnil( _L, -1) )
		{
			return luaL_error( _L, "internal error: sequence references an inexistent filter!");
		}
		int const matched = lua_rawequal( _L, -1, -4);
		lua_pop( _L, 1);                                                             // U f {env} {filters}
		if ( matched != 0 )
		{
			// found an occurence of the filter
			// remove it from the filter table
			luaL_unref( _L, -1, filterRef);
			// move the filter sequence contents to fill the hole
			memmove( ud->filterCallSequence+i, ud->filterCallSequence+i+1, (ud->nbRegisteredFilters-i-1)*sizeof(int));
			// shrink the memory block
			ud->filterCallSequence = allocFunction( allocUserData, ud->filterCallSequence, ud->nbRegisteredFilters * sizeof(int), (ud->nbRegisteredFilters-1) * sizeof(int));
			-- ud->nbRegisteredFilters;
			break;
		}
	}
	lua_pop( _L, 4);                                                                //
	return 0;
}

//################################################################################

int bind_dbus_connection_return_message( lua_State * const _L)
{
	utils_check_nargs( _L, 2);
	DBusConnection * const connection = extract_dbus_connection_pointer( _L, 1, -1);
	DBusMessage * const message = cast_to_dbus_message( _L, 2);
	dbus_connection_return_message( connection, message);
	return 0;
}

//################################################################################

int bind_dbus_connection_send( lua_State * const _L)
{
	utils_check_nargs( _L, 2);
	DBusConnection * const connection = extract_dbus_connection_pointer( _L, 1, -1);
	DBusMessage * const message = cast_to_dbus_message( _L,  2);
	lua_pushboolean( _L, dbus_connection_send( connection, message, 0x0) != 0);
	return 1;
}

//################################################################################

int bind_dbus_connection_steal_borrowed_message( lua_State * const _L)
{
	utils_check_nargs( _L, 2);
	DBusConnection * const connection = extract_dbus_connection_pointer( _L, 1, -1);
	DBusMessage * const message = cast_to_dbus_message( _L,  2);
	dbus_connection_steal_borrowed_message( connection, message);
	return 0;
}

//################################################################################
//################################################################################

luaL_Reg gSharedConnectionMeta[] =
{
	{ "add_filter", bind_dbus_connection_add_filter },
	{ "borrow_message", bind_dbus_connection_borrow_message },
	{ "dispatch", bind_dbus_connection_dispatch },
	{ "flush", bind_dbus_connection_flush },
	{ "get_dispatch_status", bind_dbus_connection_get_dispatch_status },
	{ "get_is_connected", bind_dbus_connection_get_is_connected },
	{ "get_is_authenticated", bind_dbus_connection_get_is_authenticated },
	{ "get_is_anonymous", bind_dbus_connection_get_is_anonymous },
	{ "get_server_id", bind_dbus_connection_get_server_id },
	{ "pop_message", bind_dbus_connection_pop_message },
	{ "read_write", bind_dbus_connection_read_write },
	{ "read_write_dispatch", bind_dbus_connection_read_write_dispatch },
	{ "remove_filter", bind_dbus_connection_remove_filter },
	{ "return_message", bind_dbus_connection_return_message },
	{ "send", bind_dbus_connection_send },
	{ "steal_borrowed_message", bind_dbus_connection_steal_borrowed_message },
	{ 0x0, 0x0 },
};

//################################################################################
// some additional stuff
//################################################################################

// called by the bus and connection __gc finalizers
void finalize_filter_data( lua_State * const _L, ConnectionUserdata * const _ud)
{
	// fetch the allocation function, we are going to need it
	void *allocUserData;
	lua_Alloc allocFunction = lua_getallocf( _L, &allocUserData);

	_ud->filterCallSequence = allocFunction( allocUserData, _ud->filterCallSequence, _ud->nbRegisteredFilters * sizeof(int), 0);
	_ud->nbRegisteredFilters = 0;
}
