std = "lua51+lua52"
max_line_length = false

-- mpv's `mp.options` module has a side effect, not a return value: requiring it defines
-- `read_options` in global scope (echoplay-audio.lua does `require 'mp.options'` without
-- assigning a local, exactly for this side effect).
read_globals = {
    "read_options",
}
