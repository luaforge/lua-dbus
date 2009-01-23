#if ! defined ( __utils_h__ )
#define __utils_h__ 1

#include <dbus/dbus.h>

extern int utils_check_nargs( lua_State * _L, int _nargs);
extern void utils_prepare_metatable( lua_State * _L, int *_metaRef);
extern void utils_register_upvalued_functions( lua_State * const _L, luaL_Reg const * _reg, int const _mtRef);
extern DBusBusType utils_convert_to_bus_type( lua_State * _L, int _ndx);
extern DBusHandlerResult utils_convert_to_handler_result( lua_State * _L, int _ndx);
extern int utils_convert_to_message_type( lua_State * _L, int _ndx);
extern void * utils_push_mapped_userdata( lua_State * const _L, void * const _lud, int const _metaNdx, int const _udBlockSize);
extern void * utils_cast_userdata( lua_State * const _L, int _ndx, int const _metaNdx);
extern int utils_fetch_userdata( lua_State * const _L, void *_lud);
extern void utils_fill_rule_buffer_from_table( lua_State * const _L, int _ndx, char * const _rules_buffer);
extern int utils_bus_name_is_valid( lua_State * const _L, char const * const _name);
extern int utils_unique_connection_name_is_valid( lua_State * const _L, char const * const _name);
extern int utils_interface_name_is_valid( lua_State * const _L, char const * const _name);
extern int utils_member_name_is_valid( lua_State * const _L, char const * const _name);
extern int utils_object_path_name_is_valid( lua_State * const _L, char const * const _path);
extern void utils_init( void);

#define utils_to_absolute_stack_index(_ndx) (((_ndx)>0)?(_ndx):(lua_gettop(_L)+1+(_ndx)))

#endif // __utils_h__
