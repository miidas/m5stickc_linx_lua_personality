#define LUA_32BITS
#include "src/lua-5.3.4/lua.hpp"

int system(const char* string)
{
  // ISO/IEC 9899:TC2 p.317
  // Reference: http://www.open-std.org/jtc1/sc22/wg14/www/docs/n1124.pdf
  // 7.20.4.6 The system function
  // Returns
  // If the argument is a null pointer, 
  // the system function returns nonzero only if a command processor is available. 
  // If the argument is not a null pointer, 
  // and the system function does return, it returns an implementation-defined value.
  return 0;
}

int L_print(lua_State *l)
{
  Serial.println(luaL_checkstring(l, 1));
}
