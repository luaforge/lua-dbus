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
#include <dbus/dbus.h>

#include <string.h>
#include <ctype.h>

#include "utils.h"

//################################################################################

char gValidElementCharacters[256];
void private_init_valid_element_charset( void)
{
	// names must only contain [A-Z][a-z][0-9]_ characters
	// some names are authorized to contain '-' too
	memset( gValidElementCharacters, 0, sizeof( gValidElementCharacters));
	int c;
	for ( c = 'a'; c <= 'z'; ++ c )
		gValidElementCharacters[c] = 0x3;
	for ( c = 'A'; c <= 'Z'; ++ c )
		gValidElementCharacters[c] = 0x3;
	for ( c = '0'; c <= '9'; ++ c )
		gValidElementCharacters[c] = 0x3;
	gValidElementCharacters['_'] = 0x3;
	gValidElementCharacters['-'] = 0x2;
}

//################################################################################

void utils_init( void)
{
	private_init_valid_element_charset();
}

//################################################################################
//################################################################################

int utils_check_nargs( lua_State * _L, int _nargs)
{
	if ( lua_gettop( _L) != _nargs )
		return luaL_error( _L, "wrong number of parameters (%d)", lua_gettop( _L));
	return 0;
}

//################################################################################

void utils_prepare_metatable( lua_State * _L, int *_metaRef)
{
	// register some object's metatable in the registry
	lua_newtable( _L);                                             // ... meta
	lua_pushvalue( _L, -1);                                        // ... meta meta
	*_metaRef = luaL_ref( _L, LUA_REGISTRYINDEX);                  // ... meta
	lua_pushliteral( _L, "__index");                               // ... meta "__index"
	lua_pushvalue( _L, -2);                                        // ... meta "__index" meta
	lua_settable( _L, -3);                                         // ... meta
	lua_pushliteral( _L, "__metatable");                           // ... meta "__mettable"
	lua_pushboolean( _L, 0);                                       // ... meta "__mettable" false
	lua_settable( _L, -3);                                         // ... meta
}

//################################################################################

void utils_register_upvalued_functions( lua_State * const _L, luaL_Reg const * _reg, int const _mtRef)
{
	// do the registration ourselves because we need to provide an upvalue
	while ( _reg->func != 0x0 )                            // meta
	{
		lua_pushstring( _L, _reg->name);                    // meta name
		lua_pushlightuserdata( _L, (void*) _mtRef);         // meta name ref
		lua_pushcclosure( _L, _reg->func, 1);               // meta name fn
		lua_settable( _L, -3);                              // meta
		++ _reg;
	}
}

//################################################################################
//################################################################################

DBusBusType utils_convert_to_bus_type( lua_State * _L, int _ndx)
{
	int equal = 0;
	_ndx = utils_to_absolute_stack_index( _ndx);
	luaL_argcheck( _L, lua_isstring( _L, _ndx), _ndx, "must provide type string");

	// session bus?
	lua_pushliteral( _L, "DBUS_BUS_SESSION");
	equal = lua_rawequal( _L, -1, _ndx);
	lua_pop( _L, 1);
	if ( equal )
		return DBUS_BUS_SESSION;

	// system bus?
	lua_pushliteral( _L, "DBUS_BUS_SYSTEM");
	equal = lua_rawequal( _L, -1, _ndx);
	lua_pop( _L, 1);
	if ( equal )
		return DBUS_BUS_SYSTEM;

	// started bus?
	lua_pushliteral( _L, "DBUS_BUS_STARTER");
	equal = lua_rawequal( _L, -1, _ndx);
	lua_pop( _L, 1);
	if ( equal )
		return DBUS_BUS_STARTER;

	// raise an error, we will never return
	return luaL_error( _L, "parameter is not a valid bus type (%s)", lua_tostring( _L, _ndx)), DBUS_BUS_SESSION;
}

//################################################################################

DBusHandlerResult utils_convert_to_handler_result( lua_State * _L, int _ndx)
{
	int equal = 0;
	_ndx = utils_to_absolute_stack_index( _ndx);
	luaL_argcheck( _L, lua_isstring( _L, _ndx), _ndx, "must provide type string");

	// session bus?
	lua_pushliteral( _L, "DBUS_HANDLER_RESULT_HANDLED");
	equal = lua_rawequal( _L, -1, _ndx);
	lua_pop( _L, 1);
	if ( equal )
		return DBUS_HANDLER_RESULT_HANDLED;

	// system bus?
	lua_pushliteral( _L, "DBUS_HANDLER_RESULT_NOT_YET_HANDLED");
	equal = lua_rawequal( _L, -1, _ndx);
	lua_pop( _L, 1);
	if ( equal )
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	// started bus?
	lua_pushliteral( _L, "DBUS_HANDLER_RESULT_NEED_MEMORY");
	equal = lua_rawequal( _L, -1, _ndx);
	lua_pop( _L, 1);
	if ( equal )
		return DBUS_HANDLER_RESULT_NEED_MEMORY;

	// raise an error, we will never return
	return luaL_error( _L, "parameter is not a valid handler result (%s)", lua_tostring( _L, _ndx)), DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

//################################################################################

int utils_convert_to_message_type( lua_State * _L, int _ndx)
{
	int equal = 0;
	_ndx = utils_to_absolute_stack_index( _ndx);
	luaL_argcheck( _L, lua_isstring( _L, _ndx), _ndx, "must provide type string");

	// invalid?
	lua_pushliteral( _L, "DBUS_MESSAGE_TYPE_INVALID");
	equal = lua_rawequal( _L, -1, _ndx);
	lua_pop( _L, 1);
	if ( equal )
		return DBUS_MESSAGE_TYPE_INVALID;

	// method call?
	lua_pushliteral( _L, "DBUS_MESSAGE_TYPE_METHOD_CALL ");
	equal = lua_rawequal( _L, -1, _ndx);
	lua_pop( _L, 1);
	if ( equal )
		return DBUS_MESSAGE_TYPE_METHOD_CALL ;

	// method return?
	lua_pushliteral( _L, "DBUS_MESSAGE_TYPE_METHOD_RETURN");
	equal = lua_rawequal( _L, -1, _ndx);
	lua_pop( _L, 1);
	if ( equal )
		return DBUS_MESSAGE_TYPE_METHOD_RETURN;


	// error?
	lua_pushliteral( _L, "DBUS_MESSAGE_TYPE_ERROR");
	equal = lua_rawequal( _L, -1, _ndx);
	lua_pop( _L, 1);
	if ( equal )
		return DBUS_MESSAGE_TYPE_ERROR;

	// method return?
	lua_pushliteral( _L, "DBUS_MESSAGE_TYPE_SIGNAL");
	equal = lua_rawequal( _L, -1, _ndx);
	lua_pop( _L, 1);
	if ( equal )
		return DBUS_MESSAGE_TYPE_SIGNAL;

	// raise an error, we will never return
	return luaL_error( _L, "parameter is not a valid message type (%s)", lua_tostring( _L, _ndx)), DBUS_MESSAGE_TYPE_INVALID;
}

//################################################################################
// userdata<->pointer conversions
//################################################################################

void * utils_push_mapped_userdata( lua_State * const _L, void * const _lud, int const _metaNdx, int const _udBlockSize)
{
	void * retval = 0;
	lua_getfield( _L, LUA_REGISTRYINDEX, "dbus_userdata_map");    // {udm}
	luaL_checktype( _L, -1, LUA_TTABLE);
	puts( "got the userdata map");
	lua_pushlightuserdata( _L, _lud);                             // {udm} _lud
	lua_gettable( _L, -2);                                        // {udm} U
	if ( lua_type( _L, -1) == LUA_TUSERDATA )
	{
		puts( "found an existing userdata");
		// we found a mapped userdata object
		lua_rawgeti( _L, LUA_REGISTRYINDEX, _metaNdx);             // {udm} U meta1
		lua_getmetatable( _L, -2);                                 // {udm} U meta1 meta2
		// metatable doesn't match the expected one: error (should not happen!)
		if ( !lua_rawequal( _L, -1, -2) )
			return luaL_error( _L, "internal error: wrong type!"), (void *) 0x0;
		lua_pop( _L, 2);                                           // {udm} U
		// by convention the userdata block always starts with the object pointer
		void ** userdata = retval = lua_touserdata( _L, -1);
		// object pointer doesn't match key: error (should not happen!)
		if ( *userdata != _lud )
			return luaL_error( _L, "wrong mapping (%p/%p)!", _lud, *userdata), (void *) 0x0;
		// everything is ok: keep only the userdata, we are done
		lua_remove( _L, -2);                                       // U
	}
	else if ( lua_type( _L, -1) == LUA_TNIL )
	{
		printf( "adding an entry in dbus_userdata_map (%d)\n", _metaNdx);
		// remove the nil we got when searching for an existing entry
		lua_pop( _L, 1);                                                    // {udm}
		// this is a new entry: create it and store the object pointer there
		void ** const block = retval = lua_newuserdata( _L, _udBlockSize);  // {udm} U
		*block = _lud;
		// lightuserdata key
		lua_pushlightuserdata( _L, _lud);                                   // {udm} U _lud
		// userdata value
		lua_pushvalue( _L, -2);                                             // {udm} U _lud U
		// store in the map
		lua_settable( _L, -4);                                              // {udm} U
		puts( "stored the new userdata in the map");
		// fetch the approriate metatable
		lua_rawgeti( _L, LUA_REGISTRYINDEX, _metaNdx);                      // {udm} U meta
		// check that we actually have it
		luaL_checktype( _L, -1, LUA_TTABLE);
		// give it to the userdata
		lua_setmetatable( _L, -2);                                          // {udm} U
		// remove the map from the stack, we are done
		lua_remove( _L, -2);                                                // U
	}
	else
	{
		return luaL_error( _L, "unexpected entry found in dbus_userdata_map"), (void *) 0x0;
	}
	return retval;
}

//################################################################################

void * utils_cast_userdata( lua_State * const _L, int _ndx, int const _metaNdx)
{
	_ndx = utils_to_absolute_stack_index( _ndx);
	luaL_argcheck( _L, lua_isuserdata( _L, _ndx), _ndx, "parameter is not a userdata");
	lua_getmetatable( _L, _ndx);                                // ... U ... meta1
	lua_rawgeti( _L, LUA_REGISTRYINDEX, _metaNdx);              // ... U ... meta1 meta2
	if ( !lua_rawequal( _L, -1, -2) )
		return luaL_error( _L, "parameter is not of the expected type"), (void *) 0x0;
	lua_pop( _L, 2);                                            // ... U ...
	// by convention the userdata block always starts with the object pointer
	void ** const block = lua_touserdata( _L, _ndx);
	if ( *block == 0x0 )
		return luaL_error( _L, "object is NULL (%d)", _ndx), (void *) 0x0;
	// return the userdata data, not the first pointer inside it
	return block;
}

//################################################################################

int utils_fetch_userdata( lua_State * const _L, void *_lud)
{
	lua_getfield( _L, LUA_REGISTRYINDEX, "dbus_userdata_map");    // {udm}
	luaL_checktype( _L, -1, LUA_TTABLE);
	puts( "got the userdata map");
	lua_pushlightuserdata( _L, _lud);                             // {udm} _lud
	lua_gettable( _L, -2);                                        // {udm} U
	if ( lua_type( _L, -1) != LUA_TUSERDATA )
	{
		return luaL_error( _L, "internal error: userdata should already exist for %p!", _lud);
	}
	lua_replace( _L, -2);                                         // U
	return 1;
}

//################################################################################
// generation of rules buffer from a table
//################################################################################

int private_utils_check_field( lua_State * const _L, int _ndx, char * const _field_name, int const _lua_type)
{
	lua_getfield( _L, _ndx, _field_name);                   // ... {rules} ... <field>
	if ( lua_type( _L, -1) == LUA_TNIL )                    // ... {rules} ... nil
	{
		// field not found,it's not important, juste ignore
		lua_pop( _L, 1);                                     // ... {rules} ...
		return 0;
	}
	if ( lua_type( _L, -1) != _lua_type )                   // ... {rules} ... ???
	{
		lua_pop( _L, 1);
		// this won't return in fact
		return luaL_error( _L, "'%s' is not a %s", _field_name, lua_typename( _L, _lua_type));
	}
	// we left a field of the expected type on the stack
	return 1;
}

//################################################################################

int private_utils_check_type( lua_State * const _L, int _ndx, char * const _rules_buffer, int _rules_buffer_caret, char const * const _separator)
{
	if ( private_utils_check_field( _L, _ndx, "type", LUA_TSTRING) == 0 )
		return 0;

	// type can be either 'signal', 'method_call', 'method_return', 'error'
	// if type matches one of these values, add the rule to the buffer
	char const * const type = lua_tostring( _L, -1);    // ... {rules} ... <type>
	if
	(
			strcmp( type, "signal") == 0
		|| strcmp( type, "method_call") == 0
		|| strcmp( type, "method_return") == 0
		|| strcmp( type, "error") == 0
	)
	{
		_rules_buffer_caret += sprintf( _rules_buffer + _rules_buffer_caret, "%stype='%s'", _separator, type);
	}
	else
	{
		return luaL_error( _L, "'%s' is not a valid type", type);
	}
	lua_pop( _L, 1);                                    // ... {rules} ...
	return _rules_buffer_caret;
}

//################################################################################

int private_utils_check_sender( lua_State * const _L, int _ndx, char * const _rules_buffer, int _rules_buffer_caret, char const * const _separator)
{
	if ( private_utils_check_field( _L, _ndx, "sender", LUA_TSTRING) == 0 )
		return 0;

	char const * const name = lua_tostring( _L, -1);    // ... {rules} ... <sender>
	if ( utils_bus_name_is_valid( _L, name) || utils_unique_connection_name_is_valid( _L, name) )
	{
		_rules_buffer_caret += sprintf( _rules_buffer + _rules_buffer_caret, "%ssender='%s'", _separator, name);
	}
	else
	{
		return luaL_error( _L, "'%s' is not a valid sender name", name);
	}
	lua_pop( _L, 1);                                    // ... {rules} ...
	return _rules_buffer_caret;
}

//################################################################################

int private_utils_check_interface( lua_State * const _L, int _ndx, char * const _rules_buffer, int _rules_buffer_caret, char const * const _separator)
{
	if ( private_utils_check_field( _L, _ndx, "interface", LUA_TSTRING) == 0 )
		return 0;

	char const * const name = lua_tostring( _L, -1);
	if ( utils_interface_name_is_valid( _L, name) )
	{
		_rules_buffer_caret += sprintf( _rules_buffer + _rules_buffer_caret, "%sinterface='%s'", _separator, name);
	}
	else
	{
		return luaL_error( _L, "'%s' is not a valid interface name", name);
	}
	lua_pop( _L, 1);                                    // ... {rules} ...
	return _rules_buffer_caret;
}

//################################################################################

int private_utils_check_member( lua_State * const _L, int _ndx, char * const _rules_buffer, int _rules_buffer_caret, char const * const _separator)
{
	if ( private_utils_check_field( _L, _ndx, "member", LUA_TSTRING) == 0 )
		return 0;

	char const * const name = lua_tostring( _L, -1);
	if ( utils_member_name_is_valid( _L, name) )
	{
		_rules_buffer_caret += sprintf( _rules_buffer + _rules_buffer_caret, "%smember='%s'", _separator, name);
	}
	else
	{
		return luaL_error( _L, "'%s' is not a valid interface name", name);
	}
	lua_pop( _L, 1);                                    // ... {rules} ...
	return _rules_buffer_caret;
}

//################################################################################

int private_utils_check_path( lua_State * const _L, int _ndx, char * const _rules_buffer, int _rules_buffer_caret, char const * const _separator)
{
	if ( private_utils_check_field( _L, _ndx, "path", LUA_TSTRING) == 0 )
		return 0;

	char const * const path = lua_tostring( _L, -1);
	if ( utils_object_path_name_is_valid( _L, path) )
	{
		_rules_buffer_caret += sprintf( _rules_buffer + _rules_buffer_caret, "%spath='%s'", _separator, path);
	}
	else
	{
		return luaL_error( _L, "'%s' is not a valid path", path);
	}
	lua_pop( _L, 1);                                    // ... {rules} ...
	return _rules_buffer_caret;
}

//################################################################################

int private_utils_check_destination( lua_State * const _L, int _ndx, char * const _rules_buffer, int _rules_buffer_caret, char const * const _separator)
{
	if ( private_utils_check_field( _L, _ndx, "destination", LUA_TSTRING) == 0 )
		return 0;

	char const * const name = lua_tostring( _L, -1);
	if ( utils_unique_connection_name_is_valid( _L, name) )
	{
		_rules_buffer_caret += sprintf( _rules_buffer + _rules_buffer_caret, "%sdestination='%s'", _separator, name);
	}
	else
	{
		return luaL_error( _L, "'%s' is not a valid unique connection name", name);
	}
	lua_pop( _L, 1);                                    // ... {rules} ...
	return _rules_buffer_caret;
}

//################################################################################

int private_utils_check_arg( lua_State * const _L, int _ndx, char * const _rules_buffer, int _rules_buffer_caret, char const * _separator)
{
	if ( private_utils_check_field( _L, _ndx, "arg", LUA_TTABLE) == 0 )
		return 0;

	int const argNdx = utils_to_absolute_stack_index( -1);
	lua_pushnil( _L);                                    // ... {rules} ... {arg} nil
	while ( lua_next( _L, argNdx) != 0 )                 // ... {rules} ... {arg} key value
	{
		// ignore non {number,string} pairs
		if ( lua_type( _L, -2) != LUA_TNUMBER || lua_type( _L, -1) != LUA_TSTRING )
		{
			return luaL_error( _L, "arg table contains a non (number, string) pair");
		}
		// ignore pairs when number isn't an integer in [0,DBUS_MAXIMUM_MATCH_RULE_ARG_NUMBER[ range
		lua_Number argNumAsNum = lua_tonumber( _L, -2);
		int argNumAsInt = (int) argNumAsNum;
		if ( (lua_Number) argNumAsInt != argNumAsNum || argNumAsInt < 0 || argNumAsInt >= DBUS_MAXIMUM_MATCH_RULE_ARG_NUMBER )
		{
			return luaL_error( _L, "arg table contains a non-scalar key");
		}
		char const * const string = lua_tostring( _L, -1);
		_rules_buffer_caret += sprintf( _rules_buffer + _rules_buffer_caret, "%sarg%d='%s'", _separator, argNumAsInt, string);
		_separator = ",";
		lua_pop( _L, 1);                                  // ... {rules} ... {arg} key
	}                                                    // ... {rules} ... {arg}
	lua_pop( _L, 1);                                    // ...  {rules} ...
	return _rules_buffer_caret;
}

//################################################################################

#define CHECK_RULE( _rule_name) \
{ \
	int new_caret = private_utils_check_##_rule_name( _L, _ndx, _rules_buffer, rules_buffer_caret, separator); \
	if ( new_caret != 0 ) \
	{ \
		rules_buffer_caret = new_caret; \
		separator = ","; \
	} \
}

void utils_fill_rule_buffer_from_table( lua_State * const _L, int _ndx, char * const _rules_buffer)
{
	// second argument should be a table containing the match rules
	_ndx = utils_to_absolute_stack_index( _ndx);
	luaL_argcheck( _L, lua_type( _L, _ndx) == LUA_TTABLE, _ndx, "match must be described by a rules table");

	// fill in the rules buffer from the rules table        // ... {rules}
	// TODO: make sure a malicious script won't exceed buffer capacity!!!
	char const * separator = "";
	int rules_buffer_caret = 0;
	_rules_buffer[0] = '\0';
	CHECK_RULE( type);
	CHECK_RULE( sender);
	CHECK_RULE( interface);
	CHECK_RULE( member);
	CHECK_RULE( path);
	CHECK_RULE( destination);
	CHECK_RULE( arg);
}

//################################################################################
// name validity (connection, bus, interface)
//################################################################################

enum EElementType
{
	EET_Bus,
	EET_UniqueConnectionName,
	EET_Interface,
	EET_Member,
	EET_ObjectPath
};
typedef enum EElementType EElementType;

char const * private_utils_element_is_valid( lua_State * const _L, char const * const _name, char const * const _what, EElementType _type)
{
	// name is valid if a sequence of at least 1 word matching the valid charset
	// if more than one word, the separator is '.'
	// if a valid element is encountered, return the pointer to the character following it, else NULL
	char const * p = _name;
	// when checking an object path or unique connection name, starting with a digit is authorized, else it is not
	int digit_is_valid = ((_type == EET_UniqueConnectionName) || (_type == EET_ObjectPath)) ? 1 : 0;
	// bus and unique connection names may contain a '-', the other elements types may not
	int const mask = 1 << (((_type == EET_Bus) || (_type == EET_UniqueConnectionName)) ? 1 : 0);
	char const separator = (_type == EET_ObjectPath) ? '/' : '.';
	int c;
	int raise_error = 0 ;
	while ( !raise_error && ((c = (int)(*p)), (c != '\0' && c != separator)) )
	{
		// when not allowed to start with a(n authorized) digit, the element is invalid
		if ( !digit_is_valid && isdigit( c))
		{
			raise_error = 1;
			break;
		}
		// if encountering an unauthorized character, the element is invalid
		if ( (gValidElementCharacters[c] & mask) == 0 )
		{
			raise_error = 1;
			break;
		}
		// once we have checked that we began with a valid character, digits are acceptable
		digit_is_valid = 1;
		++ p;
	}
	// a valid element must constain at least one character too
	if ( raise_error == 1 || p == _name )
		return luaL_error( _L, "'%s' is not a valid %s", _name - ((_type == EET_ObjectPath) ? 1 : 0), _what), p;
	return p;
}

//################################################################################

int private_utils_name_is_valid( lua_State * const _L, char const * const _name, char const * const _what, EElementType _type, int _max_length)
{
	char const *next_element = private_utils_element_is_valid( _L, _name, _what, _type);
	// bus names must have at least 2 elements, separated by a '.'
	if ( *next_element == '.' )
	{
		// found one valid element, check if there are more
		do
		{
			// skip the invalid character we stumbled upon
			next_element = private_utils_element_is_valid( _L, next_element + 1, _what, _type);
		} while ( *next_element == '.' );
		// only valid elements? name is valid only if length doesn't exceed authorized max
		int const name_length = next_element - _name;
		if ( name_length <= _max_length )
			return name_length;
	}
	return luaL_error( _L, "'%s' is not a valid %s", _name, _what);
}

//################################################################################

int utils_interface_name_is_valid( lua_State * const _L, char const * const _name)
{
	return private_utils_name_is_valid( _L, _name, "interface name", EET_Interface, DBUS_MAXIMUM_NAME_LENGTH);
}

//################################################################################

int utils_bus_name_is_valid( lua_State * const _L, char const * const _name)
{
	return private_utils_name_is_valid( _L, _name, "bus name", EET_Bus, DBUS_MAXIMUM_NAME_LENGTH);
}

//################################################################################

int utils_unique_connection_name_is_valid( lua_State * const _L, char const * const _name)
{
	// a unique connection name is a bus name starting with a ':'
	if ( _name[0] != ':' )
		return luaL_error( _L, "'%s' is not a valid %s", _name, "unique connection name");

	// and authorized to start name elements with a digit
	return private_utils_name_is_valid( _L, _name+1, "unique connection name", EET_UniqueConnectionName, DBUS_MAXIMUM_NAME_LENGTH-1);
}

//################################################################################

int utils_member_name_is_valid( lua_State * const _L, char const * const _name)
{
	char const * const next_element = private_utils_element_is_valid( _L, _name , "member name", EET_Member);
	// method/signal names can't contain more than 1 element (or invalid character either)
	if ( *next_element != '\0' )
		return luaL_error( _L, "'%s' is not a valid name", _name);
	// only valid elements? name is valid only if length doesn't exceed authorized max
	int const name_length = next_element - _name;
	if ( name_length > DBUS_MAXIMUM_NAME_LENGTH )
		return luaL_error( _L, "'%s' is too long (%d characters)", _name, name_length);
	return name_length;
}

//################################################################################
// object path validity
//################################################################################

int utils_object_path_name_is_valid( lua_State * const _L, char const * const _path)
{
	//get rid of the singular root path case
	if ( *_path == '/' && *(_path+1) == '\0' )
		return 1;

	char const * next_element = _path;
	// a path is a series of elements preceded by a '/'
	while ( *next_element == '/' )
	{
		// note that '/' being invalid, a consecutive '/' will yield an invalid element
		next_element = private_utils_element_is_valid( _L, next_element + 1, "path", EET_ObjectPath);
	}
	// any path not containing invalid elements, and not ending with a '/' is valid
	return 1;
}

//################################################################################
// message signature validity
//################################################################################

// TODO!!!

