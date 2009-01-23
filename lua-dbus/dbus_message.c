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

//################################################################################
//################################################################################

int gMessageMetatableRef = LUA_NOREF;

//################################################################################
//################################################################################

int push_dbus_message( lua_State * const _L, DBusMessage * const _message)
{
	if ( _message == 0x0 )
		return luaL_error( _L, "push_dbus_message: attempting to create a NULL message");
	else
	{
		// create (or find an existing) fully functional userdata for our connection
		DBusMessage ** const block = (DBusMessage **) utils_push_mapped_userdata( _L, _message, gMessageMetatableRef, sizeof( void *));
		// connection address is already stored at the beginning of the userdata block, just fill the rest
		printf( "push_dbus_message: new message contents: %p(%p)\n", _message, *block);
		return 1;
	}
}

//################################################################################

DBusMessage * cast_to_dbus_message( lua_State * const _L,  int const _ndx)
{
	return *(DBusMessage **) utils_cast_userdata( _L, _ndx, gMessageMetatableRef);
}

//################################################################################
//################################################################################

int bind_dbus_message_get_type( lua_State * const _L)
{
	utils_check_nargs( _L, 1);
	DBusMessage * const message = cast_to_dbus_message( _L, 1);
	char const *type = 0x0;
	switch( dbus_message_get_type( message))
	{
		case DBUS_MESSAGE_TYPE_METHOD_CALL: type = "DBUS_MESSAGE_TYPE_METHOD_CALL"; break;
		case DBUS_MESSAGE_TYPE_METHOD_RETURN: type = "DBUS_MESSAGE_TYPE_METHOD_RETURN"; break;
		case DBUS_MESSAGE_TYPE_ERROR: type = "DBUS_MESSAGE_TYPE_ERROR"; break;
		case DBUS_MESSAGE_TYPE_SIGNAL: type = "DBUS_MESSAGE_TYPE_SIGNAL"; break;
		case DBUS_MESSAGE_TYPE_INVALID: type = "DBUS_MESSAGE_TYPE_INVALID"; break;
		default: type = "unknown message type"; break;
	}
	lua_pushstring( _L, type);
	lua_pushstring( _L, dbus_message_type_to_string( dbus_message_get_type( message)));
	return 2;
}

//################################################################################

int bind_dbus_message_new( lua_State * const _L)
{
	utils_check_nargs( _L, 1);
	int message_type = utils_convert_to_message_type( _L, 1);
	DBusMessage *message = dbus_message_new( message_type);
	return push_dbus_message( _L, message);
}

//################################################################################

int bind_dbus_message_new_error( lua_State * const _L)
{
	utils_check_nargs( _L, 3);
	DBusMessage * const reply_to = cast_to_dbus_message( _L, 1);
	char const * const error_name = lua_tostring( _L, 2);
	char const * const error_message = lua_tostring( _L, 3);
	DBusMessage *message = dbus_message_new_error( reply_to, error_name, error_message);
	return push_dbus_message( _L, message);
}

//################################################################################

int bind_dbus_message_new_method_call( lua_State * const _L)
{ // TODO: check that the validation function are ok for the arguments
	// accept as argument either a table or direct strings
	char const *destination = 0x0, *path = 0x0, *interface = 0x0, *method = 0x0;
	if ( lua_istable( _L, 1) )
	{
		utils_check_nargs( _L, 1);                           // {message}
		// extract our strings from the table fields
		lua_pushnil( _L);                                    // {message} nil
		while ( lua_next( _L, -2) != 0 )                     // {message} key value
		{
			// ignore non {string,string} pairs
			if ( (lua_type( _L, -1) != LUA_TSTRING) || (lua_type( _L, -2) != LUA_TSTRING) )
			{
				return luaL_error( _L, "message signal table contains a non (string, string) pair");
			}
			char const * const key = lua_tostring( _L, -2);
			char const * const value = lua_tostring( _L, -1);
			if ( strcmp( key, "destination") == 0 && utils_object_path_name_is_valid( _L, value) )
				destination = value;
			else if ( strcmp( key, "path") == 0 && utils_object_path_name_is_valid( _L, value) )
				path = value;
			else if ( strcmp( key, "interface") == 0 && utils_interface_name_is_valid( _L, value) )
				interface = value;
			else if ( strcmp( key, "method") == 0 && utils_member_name_is_valid( _L, value) )
				method = value;
			lua_pop( _L, 1);                                  // {message} key
		}                                                    // {message}
		lua_pop( _L, 1);                                     //
	}
	else
	{
		utils_check_nargs( _L, 4);
		struct Info
		{
			int ndx;
			char const *name;
			char const **dest;
			int (* fn)(lua_State * const, char const*);
		};
		struct Info infos[4] =
		{
			{ 1, "destination", &destination, utils_object_path_name_is_valid},
			{ 2, "path", &path, utils_object_path_name_is_valid},
			{ 3, "interface", &interface, utils_interface_name_is_valid},
			{ 4, "method", &method, utils_member_name_is_valid},
		} ;
		int i;
		for( i = 0; i < 4 ; ++ i)
		{
			int const ndx = infos[i].ndx;
			if ( !lua_isstring( _L, ndx) )
			{
				char buffer[64];
				sprintf( buffer, "%s must be a string", infos[i].name);
				luaL_argcheck( _L, 0, ndx, buffer);
			}
			char const * const string = lua_tostring( _L, ndx);
			if ( infos[i].fn( _L, string) )
				*infos[i].dest = string;
		}
	}
	DBusMessage *message = dbus_message_new_method_call( destination, path, interface, method);
	if (message == 0x0)
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

int bind_dbus_message_new_method_return( lua_State * const _L)
{
	utils_check_nargs( _L, 1);
	DBusMessage * const method_call = cast_to_dbus_message( _L, 1);
	DBusMessage *message = dbus_message_new_method_return( method_call);
	if (message == 0x0)
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

int bind_dbus_message_new_signal( lua_State * const _L)
{
	// accept as argument either a table or direct strings
	char const * path = 0x0, * interface = 0x0, * name = 0x0;
	if ( lua_istable( _L, 1) )
	{
		utils_check_nargs( _L, 1);                           // {message}
		// extract our strings from the table fields
		lua_pushnil( _L);                                    // {message} nil
		while ( lua_next( _L, -2) != 0 )                     // {message} key value
		{
			// ignore non {string,string} pairs
			if ( (lua_type( _L, -1) != LUA_TSTRING) || (lua_type( _L, -2) != LUA_TSTRING) )
			{
				return luaL_error( _L, "message signal table contains a non (string, string) pair");
			}
			char const * const key = lua_tostring( _L, -2);
			char const * const value = lua_tostring( _L, -1);
			if ( strcmp( key, "path") == 0 && utils_object_path_name_is_valid( _L, value) )
				path = value;
			else if ( strcmp( key, "interface") == 0 && utils_interface_name_is_valid( _L, value) )
				interface = value;
			else if ( strcmp( key, "name") == 0 && utils_member_name_is_valid( _L, value) )
				name = value;
			lua_pop( _L, 1);                                  // {message} key
		}                                                    // {message}
		lua_pop( _L, 1);                                     //
	}
	else
	{
		utils_check_nargs( _L, 3);
		struct Info
		{
			int ndx;
			char const *name;
			char const **dest;
			int (* fn)(lua_State * const, char const*);
		};
		struct Info infos[3] =
		{
			{ 1, "path", &path, utils_object_path_name_is_valid},
			{ 2, "interface", &interface, utils_interface_name_is_valid},
			{ 3, "name", &name, utils_member_name_is_valid},
		} ;
		int i;
		for( i = 0; i < 3 ; ++ i)
		{
			int const ndx = infos[i].ndx;
			if ( !lua_isstring( _L, ndx) )
			{
				char buffer[64];
				sprintf( buffer, "%s must be a string", infos[i].name);
				luaL_argcheck( _L, 0, ndx, buffer);
			}
			char const * const string = lua_tostring( _L, ndx);
			if ( infos[i].fn( _L, string) )
				*infos[i].dest = string;
		}
	}
	DBusMessage *message = dbus_message_new_signal( path, interface, name);
	return push_dbus_message( _L, message);
}

//################################################################################

int finalize_dbus_message( lua_State * const _L)
{
	utils_check_nargs( _L, 1);
	puts( "finalize_dbus_message");
	DBusMessage * const message = cast_to_dbus_message( _L, 1);
	printf( "finalize_dbus_message: message=%p\n", message);

	puts( "finalize_dbus_message: unrefing message");
	dbus_message_unref( message);
	puts( "finalize_dbus_message; unref-ed");
	return 0;
}

//################################################################################
//################################################################################

static luaL_Reg gMessageMeta[] =
{
	{ "__gc", finalize_dbus_message },
	{ "get_type", bind_dbus_message_get_type } ,
	{ 0x0, 0x0 },
};

//################################################################################
//################################################################################

// should be called with the "dbus" library table on the top of the stack
void register_message_stuff( lua_State * const _L)
{
	// register the connection object metatable in the registry
	utils_prepare_metatable( _L, &gMessageMetatableRef);                      // {meta}
	utils_register_upvalued_functions( _L, gMessageMeta, gMessageMetatableRef);
	lua_pop( _L, 1);                                                         //
}
