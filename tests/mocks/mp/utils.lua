-- Minimal stand-in for mpv's `mp.utils` module. Real JSON parsing isn't needed for the tests
-- that use this mock (they exercise nil-fallback paths and quality.lua's tier logic, neither
-- of which depends on actual JSON content) - kept deliberately trivial rather than
-- reimplementing a JSON parser that would just duplicate what mpv itself already provides.
local M = {}

function M.parse_json() return nil end
function M.format_json() return nil end
function M.readdir() return {} end
function M.split_path(p) return p, '' end

return M
