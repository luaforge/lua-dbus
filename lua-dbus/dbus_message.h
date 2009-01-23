#if ! defined ( __dbus_message_h__ )
#define __dbus_message_h__ 1

//################################################################################

extern int bind_dbus_message_new( lua_State * const _L);
extern int bind_dbus_message_new_error( lua_State * const _L);
extern int bind_dbus_message_new_method_call( lua_State * const _L);
extern int bind_dbus_message_new_method_return( lua_State * const _L);
extern int bind_dbus_message_new_signal( lua_State * const _L);
extern void register_message_stuff( lua_State * const _L);

//################################################################################

#endif // __dbus_message_h__




