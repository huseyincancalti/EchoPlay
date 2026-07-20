-- mpv's own save-position-on-quit/resume-playback resumes silently (turned off in mpv.conf) -
-- position is tracked here instead, in a small side file (kept separate from
-- echoplay-state.json so it can grow/shrink independently). Playback always starts at 0:00;
-- if a saved spot exists, an on-screen prompt asks whether to jump there.
local mp = require 'mp'
local util = require 'util'

local M = {}
local t -- injected via M.init

local RESUME_PATH = mp.command_native({ 'expand-path', '~~/echoplay-resume.json' })
local RESUME_MAX_ENTRIES = 300
local resume_map = {}

local function load_resume_map()
    local m = util.read_json(RESUME_PATH)
    if type(m) == 'table' then resume_map = m end
end
load_resume_map()

local function save_resume_map()
    local count, entries = 0, {}
    for path, e in pairs(resume_map) do count = count + 1; entries[#entries + 1] = { path = path, at = e.at or 0 } end
    if count > RESUME_MAX_ENTRIES then
        table.sort(entries, function(a, b) return a.at < b.at end)
        for i = 1, count - RESUME_MAX_ENTRIES do resume_map[entries[i].path] = nil end
    end
    local f = io.open(RESUME_PATH, 'w')
    if f then f:write(util.to_json(resume_map)); f:close() end
end

-- Skips near-start (nothing to resume) and near-end (finished - don't offer to "resume" 10s from the end).
local function update_position()
    local path = mp.get_property('path')
    local pos = mp.get_property_number('time-pos')
    local dur = mp.get_property_number('duration')
    if not path or not pos or not dur or dur <= 0 then return end
    if pos < 15 or pos > dur - 20 then resume_map[path] = nil
    else resume_map[path] = { pos = pos, dur = dur, at = os.time() } end
end
mp.add_periodic_timer(5, update_position)
mp.register_event('shutdown', function() update_position(); save_resume_map() end)

local function format_hms(sec)
    sec = math.floor(sec + 0.5)
    local h, m, s = math.floor(sec / 3600), math.floor((sec % 3600) / 60), sec % 60
    if h > 0 then return string.format('%d:%02d:%02d', h, m, s) end
    return string.format('%d:%02d', m, s)
end

local overlay, dismiss_timer = nil, nil
function M.dismiss()
    if overlay then overlay:remove(); overlay = nil end
    if dismiss_timer then dismiss_timer:kill(); dismiss_timer = nil end
    mp.remove_key_binding('resume-yes')
    mp.remove_key_binding('resume-no')
end

-- {\an5} centers in the middle of the frame (standard ASS alignment behavior without \pos).
function M.show(pos)
    overlay = mp.create_osd_overlay('ass-events')
    overlay.data = string.format('{\\an5\\fs36}%s\\N{\\fs22}%s',
        string.format(t('resume_prompt'), format_hms(pos)), t('resume_keys'))
    overlay:update()
    mp.add_forced_key_binding('Enter', 'resume-yes', function()
        M.dismiss()
        mp.set_property_number('time-pos', pos)
    end)
    mp.add_forced_key_binding('Esc', 'resume-no', function() M.dismiss() end)
    dismiss_timer = mp.add_timeout(8, M.dismiss)
end

-- Returns the saved {pos, dur, at} entry for a path, or nil if there's nothing to resume.
function M.check(path)
    return path and resume_map[path] or nil
end

function M.init(deps)
    t = deps.t
end

return M
