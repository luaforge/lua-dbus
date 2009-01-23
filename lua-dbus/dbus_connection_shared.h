#if ! defined ( __dbus_connection_shared_h__ )
#define __dbus_connection_shared_h__ 1

//################################################################################

struct ConnectionUserdata
{
	DBusConnection *connection;
	int closeOnFinalize;
	int nbRegisteredFilters;
	int *filterCallSequence;
};
typedef struct ConnectionUserdata ConnectionUserdata;

extern DBusConnection * extract_dbus_connection_pointer( lua_State * const _L, int const _ndx, int const _whichMeta);
extern void finalize_filter_data( lua_State * const _L, ConnectionUserdata * const _ud);
extern luaL_Reg gSharedConnectionMeta[];

//################################################################################

#endif // __dbus_connection_shared_h__


