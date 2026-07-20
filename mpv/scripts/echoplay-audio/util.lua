-- Tiny shared JSON helpers used by main.lua, i18n.lua, quality.lua, and resume.lua - kept
-- here once instead of duplicated four times.
local utils = require 'mp.utils'

local M = {}

function M.read_json(path)
    local f = io.open(path, 'r'); if not f then return nil end
    local d = f:read('*a'); f:close()
    local p = utils.parse_json(d or ''); return type(p) == 'table' and p or nil
end

-- utils.format_json can return nil (caught live in CI, twice, on an older mpv build than the
-- one this project targets: once in save_state's disk write, once in a mp.commandv call -
-- "argument N is not a string"). Every JSON-producing call site degrades to an empty object
-- instead of crashing the whole script.
function M.to_json(tbl)
    return utils.format_json(tbl) or '{}'
end

return M
