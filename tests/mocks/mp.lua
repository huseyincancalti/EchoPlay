-- Minimal stand-in for mpv's `mp` module, just enough surface for EchoPlay's Lua modules to
-- `require 'mp'` and register callbacks outside a real mpv process. Lets tests/run.lua drive
-- pure logic (e.g. quality.lua's auto-tier stepping) by calling the captured timer callback
-- directly, with synthetic property values, instead of needing a real mpv instance.
local M = {}

M.native_props = {}
function M.get_property_number(name) return M.native_props[name] end
function M.get_property(name) return M.native_props[name] end
function M.get_property_native(name) return M.native_props[name] end
function M.set_property(name, v) M.native_props[name] = v end
function M.set_property_number(name, v) M.native_props[name] = v end

function M.commandv() end
function M.command() end
function M.command_native(t)
    if type(t) == 'table' and t[1] == 'expand-path' then return t[2] end
    return nil
end
function M.command_native_async() end

M.timers = {} -- {interval, fn} - tests call timers[n].fn() directly to drive periodic logic
function M.add_periodic_timer(interval, fn)
    local timer = { interval = interval, fn = fn }
    M.timers[#M.timers + 1] = timer
    return { kill = function() end }
end
function M.add_timeout(_, fn) return { kill = function() end }, fn end

function M.register_event() end
function M.register_script_message() end
function M.add_key_binding() end
function M.add_forced_key_binding() end
function M.remove_key_binding() end
function M.observe_property() end
function M.create_osd_overlay()
    return { data = '', res_x = 0, res_y = 0, z = 0, update = function() end, remove = function() end }
end
function M.osd_message() end
function M.get_script_name() return 'echoplay_audio' end
function M.get_osd_size() return 1280, 720 end

M.msg = { info = function() end, warn = function() end, error = function() end, verbose = function() end }

return M
