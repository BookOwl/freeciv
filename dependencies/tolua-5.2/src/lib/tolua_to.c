/* tolua: funcitons to convert to C types
** Support code for Lua bindings.
** Written by Waldemar Celes
** TeCGraf/PUC-Rio
** Apr 2003
** $Id: tolua_to.c,v 1.5 2009/11/24 16:45:15 fabraham Exp $
*/

/* This code is free software; you can redistribute it and/or modify it. 
** The software provided hereunder is on an "as is" basis, and 
** the author has no obligation to provide maintenance, support, updates,
** enhancements, or modifications. 
*/

#include "tolua.h"

#include <string.h>
#include <stdlib.h>

TOLUA_API double tolua_tonumber (lua_State* L, int narg, double def)
{
 return lua_gettop(L)<abs(narg) ? def : lua_tonumber(L,narg);
}

TOLUA_API const char* tolua_tostring (lua_State* L, int narg, const char* def)
{
 return lua_gettop(L)<abs(narg) ? def : lua_tostring(L,narg);
}

TOLUA_API void* tolua_touserdata (lua_State* L, int narg, void* def)
{
 return lua_gettop(L)<abs(narg) ? def : lua_touserdata(L,narg);
}

TOLUA_API void* tolua_tousertype (lua_State* L, int narg, void* def)
{
 if (lua_gettop(L)<abs(narg))
  return def;
 else
 {
  void* u = lua_touserdata(L,narg);
  return (u==NULL) ? NULL : *((void**)u); /* nil represents NULL */
 }
}

TOLUA_API int tolua_tovalue (lua_State* L, int narg, int def)
{
 return lua_gettop(L)<abs(narg) ? def : narg;
}

TOLUA_API int tolua_toboolean (lua_State* L, int narg, int def)
{
 return lua_gettop(L)<abs(narg) ?  def : lua_toboolean(L,narg);
}

TOLUA_API double tolua_tofieldnumber (lua_State* L, int lo, int index, double def)
{
 double v;
 lua_pushnumber(L,index);
 lua_gettable(L,lo);
 v = lua_isnil(L,-1) ? def : lua_tonumber(L,-1);
 lua_pop(L,1);
 return v;
}

TOLUA_API int tolua_tofieldboolean (lua_State* L, int lo, int index, int def)
{
  int v;
  lua_pushnumber(L,index);
  lua_gettable(L,lo);
  v = lua_isnil(L,-1) ? def : lua_toboolean(L,-1);
  lua_pop(L,1);
  return v;
}

TOLUA_API const char* tolua_tofieldstring 
(lua_State* L, int lo, int index, const char* def)
{
 const char* v;
 lua_pushnumber(L,index);
 lua_gettable(L,lo);
 v = lua_isnil(L,-1) ? def : lua_tostring(L,-1);
 lua_pop(L,1);
 return v;
}

TOLUA_API void* tolua_tofielduserdata (lua_State* L, int lo, int index, void* def)
{
 void* v;
 lua_pushnumber(L,index);
 lua_gettable(L,lo);
 v = lua_isnil(L,-1) ? def : lua_touserdata(L,-1);
 lua_pop(L,1);
 return v;
}

TOLUA_API void* tolua_tofieldusertype (lua_State* L, int lo, int index, void* def)
{
 void* v;
 lua_pushnumber(L,index);
 lua_gettable(L,lo);
 v = lua_isnil(L,-1) ? def : (*(void **)(lua_touserdata(L, -1)));
 lua_pop(L,1);
 return v;
}

TOLUA_API int tolua_tofieldvalue (lua_State* L, int lo, int index, int def)
{
 int v;
 lua_pushnumber(L,index);
 lua_gettable(L,lo);
 v = lua_isnil(L,-1) ? def : lo;
 lua_pop(L,1);
 return v;
}

TOLUA_API int tolua_getfieldboolean (lua_State* L, int lo, int index, int def)
{
 int v;
 lua_pushnumber(L,index);
 lua_gettable(L,lo);
 v = lua_isnil(L,-1) ? 0 : lua_toboolean(L,-1);
 lua_pop(L,1);
 return v;
}
