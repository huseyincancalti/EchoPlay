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

-- Renders `lines` (already-styled ASS text, one string per line) as a compact block near the
-- bottom-center of the frame, and sets it on `overlay`. Shared by resume.lua's "continue?"
-- prompt and main.lua's key-rebind capture prompt so both read as one visual language instead
-- of each drawing bare {\an5} text dead-center on screen (a full-screen-centered wall of text
-- draws far more attention than a two-line question warrants, and \an5 with no border at all
-- is unreadable over bright video). Readability comes from a heavy black outline + shadow on
-- the text itself (\bord/\shad/\3c), not a separately-drawn background box - an earlier
-- version drew one via {\p1} vector paths, but that meant two {\...} blocks in one ASS event
-- each carrying their own \pos, and ASS only honors the *first* \pos per event; the second
-- (the text's own) was silently dropped, which is why real users saw the dynamic part of the
-- message (e.g. the resume timestamp) rendered in the wrong place / not visibly at all. A
-- single \pos, single alignment, no draw-mode mixing avoids that whole class of bug.
function M.toast(overlay, lines)
    local fs = 30
    -- Query the OSD's actual coordinate space rather than assuming 1280x720: mp.create_osd_overlay
    -- defaults res_y to 720 and auto-computes res_x from the video's aspect ratio, so a hardcoded
    -- cx would drift off-center for anything that isn't exactly 16:9.
    local w, h = mp.get_osd_size()
    w, h = (w and w > 0) and w or 1280, (h and h > 0) and h or 720
    overlay.data = string.format(
        '{\\an2\\pos(%d,%d)\\fs%d\\bord3\\shad1\\3c&H000000&\\4c&H000000&\\1c&HE9E4DC&}%s',
        math.floor(w / 2), math.floor(h - 60), fs, table.concat(lines, '\\N'))
end

return M
