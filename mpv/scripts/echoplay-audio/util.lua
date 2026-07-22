-- Tiny shared JSON helpers used by main.lua, i18n.lua, quality.lua, and resume.lua - kept
-- here once instead of duplicated four times.
local mp = require 'mp'
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

-- Renders `lines` (already-styled ASS text, one string per line) as a compact card with a
-- real dark background, centered near the bottom of the frame. Shared by resume.lua's
-- "continue?" prompt and main.lua's key-rebind capture prompt so both read as one visual
-- language instead of each drawing bare {\an5} text dead-center on screen with no background -
-- unreadable over bright video, and dead-center draws far more attention than a two-line
-- question warrants.
--
-- Two PREVIOUS attempts at this both shipped broken, each confirmed live via a real user's
-- screenshots before being caught:
--   1. Box (drawn via {\p1} vector paths) and text packed into ONE ass-events overlay as two
--      "lines" (joined by a raw \n), each carrying its own \pos - ASS only honors the *first*
--      \pos per event, so the text's \pos was silently dropped and its dynamic content (e.g.
--      the resume timestamp) rendered blank.
--   2. Dropped the box, kept text-only, computed \pos from mp.get_osd_size() - but that
--      queries the *video's* OSD scale, which doesn't match the coordinate space
--      mp.create_osd_overlay('ass-events') actually renders in by default; text landed around
--      mid-screen instead of near the bottom.
--
-- Fix: two SEPARATE overlay objects (mpv gives each its own independent ASS event - no shared
-- \pos to clobber), both with res_x/res_y explicitly pinned to the same fixed 1280x720 space
-- *by this code*, so the box's drawn coordinates and the text's \pos are guaranteed to agree -
-- nothing is queried or assumed. `toasts[key]` keeps one box+text pair per caller (resume.lua
-- and main.lua's rebind capture each get their own, so one can't stomp the other).
local toasts = {}

function M.toast_show(key, lines)
    local o = toasts[key]
    if not o then
        o = { box = mp.create_osd_overlay('ass-events'), text = mp.create_osd_overlay('ass-events') }
        o.box.res_x, o.box.res_y = 1280, 720
        o.text.res_x, o.text.res_y = 1280, 720
        o.box.z, o.text.z = 0, 1 -- text above box
        toasts[key] = o
    end
    local base_fs = 30
    local pad_v, pad_h, min_w = 20, 36, 420
    -- Width/height estimates scale per-line with that line's own \fsNN override (if any),
    -- since a line rendered much larger than the base size (e.g. the big captured-key line
    -- in main.lua's rebind prompt) needs proportionally more room, not the base size's estimate.
    local widest_px, total_h = 0, 0
    for _, l in ipairs(lines) do
        local fs = tonumber(l:match('\\fs(%d+)')) or base_fs
        local plain = l:gsub('{\\[^}]*}', '')
        widest_px = math.max(widest_px, plain:len() * fs * 0.56)
        total_h = total_h + fs * 1.3
    end
    local box_w = math.max(min_w, widest_px + pad_h * 2)
    local box_h = total_h + pad_v * 2
    local cx, cy = 640, 640
    local left, top = math.floor(cx - box_w / 2), math.floor(cy - box_h / 2)
    local right, bottom = math.floor(cx + box_w / 2), math.floor(cy + box_h / 2)
    o.box.data = string.format(
        '{\\pos(0,0)\\bord0\\shad0\\1c&H121417&\\1a&H20&\\p1}m %d %d l %d %d l %d %d l %d %d{\\p0}',
        left, top, right, top, right, bottom, left, bottom)
    o.text.data = string.format('{\\an5\\pos(%d,%d)\\fs%d\\1c&HE9E4DC&}%s',
        cx, cy, base_fs, table.concat(lines, '\\N'))
    o.box:update()
    o.text:update()
end

function M.toast_hide(key)
    local o = toasts[key]
    if not o then return end
    o.box:remove()
    o.text:remove()
    toasts[key] = nil
end

return M
