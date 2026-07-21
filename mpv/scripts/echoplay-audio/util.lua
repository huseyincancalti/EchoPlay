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

-- Renders `lines` (already-styled ASS text, one string per line) centered inside a small,
-- semi-opaque box near the bottom of the frame, and sets it on `overlay`. Shared by
-- resume.lua's "continue?" prompt and main.lua's key-rebind capture prompt so both read as
-- one visual language instead of each drawing bare, backgroundless {\an5} text - unreadable
-- over bright video, and a full-screen-centered wall of text draws far more attention than a
-- two-line question warrants. A fixed 1280x720 ASS script resolution (overlay.res_x/res_y)
-- keeps the box's position/size stable regardless of the actual video's resolution or aspect
-- ratio - libass scales the whole canvas to fit, same as it does for real subtitles.
-- Colors match the app's own dark-mode palette (see color_string() in main.lua) so the toast
-- looks like it belongs to EchoPlay even when the video is playing in light uosc mode - a
-- dark, readable toast over arbitrary video content is the safer default either way.
function M.toast(overlay, lines)
    overlay.res_x, overlay.res_y = 1280, 720
    local base_fs = 28
    local pad_v, pad_h, min_w = 22, 40, 460
    -- Width/height estimates scale per-line with that line's own \fsNN override (if any),
    -- since a line rendered much larger than the base size (e.g. the big captured-key line
    -- below) needs proportionally more room, not the base size's estimate.
    local widest_px, total_h = 0, 0
    for _, l in ipairs(lines) do
        local fs = tonumber(l:match('\\fs(%d+)')) or base_fs
        local plain = l:gsub('{\\[^}]*}', '')
        widest_px = math.max(widest_px, plain:len() * fs * 0.56)
        total_h = total_h + fs * 1.25
    end
    local box_w = math.max(min_w, widest_px + pad_h * 2)
    local box_h = total_h + pad_v * 2
    local cx, cy = 640, 640
    local left, top = cx - box_w / 2, cy - box_h / 2
    local right, bottom = cx + box_w / 2, cy + box_h / 2
    local box = string.format(
        '{\\pos(0,0)\\bord0\\shad0\\1c&H121417&\\1a&H26&\\p1}m %d %d l %d %d l %d %d l %d %d{\\p0}',
        left, top, right, top, right, bottom, left, bottom)
    local text = string.format('{\\an5\\pos(%d,%d)\\fs%d\\1c&HDCE4E9&}%s',
        cx, cy, base_fs, table.concat(lines, '\\N'))
    overlay.data = box .. '\n' .. text
end

return M
