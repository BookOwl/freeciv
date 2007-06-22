
require("tlx_compat51")
require("tlx_compat")
require("tlx_basic")
require("tlx_feature")
require("tlx_verbatim")
require("tlx_code")
require("tlx_typedef")
require("tlx_container")
require("tlx_package")
require("tlx_module")
require("tlx_namespace")
require("tlx_define")
require("tlx_enumerate")
require("tlx_declaration")
require("tlx_variable")
require("tlx_array")
require("tlx_function")
require("tlx_operator")
require("tlx_class")
require("tlx_clean")
--require("tlx_custom")
require("tlx_doit")

local err,msg = xpcall(doit, debug.traceback)
if not err then
--print("**** msg is "..tostring(msg))
 local _,_,label,msg = strfind(msg,"(.-:.-:%s*)(.*)")
 tolua_error(msg,label)
end
