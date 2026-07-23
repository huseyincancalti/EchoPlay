-- Tells existing users a newer EchoPlay build exists. mpv scripts have no auto-updater and
-- EchoPlay isn't installed through a package manager that would notice on its own, so without
-- this a user who installed once has no signal a fix (e.g. the picture-quality stutter fix)
-- even exists - they'd only find out by stumbling onto the GitHub page.
--
-- Runs at most once every CHECK_INTERVAL, gated by last_checked persisted in
-- echoplay-state.json (main.lua owns the file; this module just exposes the fields, same
-- pattern as quality.pref). Checks GitHub's "latest release" API via curl (present by default
-- on Windows 10 1803+, macOS, and virtually every Linux distro - no added prerequisite) and
-- compares its tag against version.lua, which release.yml overwrites with the real tag at
-- build time. Toasts at most once per newer version - seen_version persists so relaunching
-- the same file/session doesn't nag again for a version already surfaced.
local mp = require 'mp'
local utils = require 'mp.utils'

local M = {}
local t, osd, refresh_menu, save_state -- injected via M.init
local CURRENT_VERSION = require 'version'
local REPO = 'huseyincancalti/EchoPlay'
local CHECK_INTERVAL = 24 * 60 * 60 -- seconds

M.last_checked = 0  -- unix seconds, persisted
M.seen_version = '' -- newest version already surfaced to the user, persisted

local function parse_semver(v)
    local maj, min, pat = tostring(v):match('^(%d+)%.(%d+)%.(%d+)')
    if not maj then return nil end
    return tonumber(maj), tonumber(min), tonumber(pat)
end

-- Exposed (not local) so tests/run.lua can exercise the comparison directly.
function M.is_newer(a, b)
    local a1, a2, a3 = parse_semver(a)
    local b1, b2, b3 = parse_semver(b)
    if not a1 or not b1 then return false end
    if a1 ~= b1 then return a1 > b1 end
    if a2 ~= b2 then return a2 > b2 end
    return a3 > b3
end

local function on_result(_, res)
    if not res or res.status ~= 0 or not res.stdout then return end
    local rel = utils.parse_json(res.stdout)
    local tag = rel and rel.tag_name
    if type(tag) ~= 'string' then return end
    local latest = tag:gsub('^v', '')
    if latest ~= M.seen_version and M.is_newer(latest, CURRENT_VERSION) then
        M.seen_version = latest
        save_state()
        osd(string.format(t('update_available'), latest))
        refresh_menu()
    end
end

function M.check()
    M.last_checked = os.time()
    save_state()
    mp.command_native_async({ name = 'subprocess', playback_only = false, capture_stdout = true, args = {
        'curl', '-s', '-m', '10', '-H', 'User-Agent: EchoPlay', 'https://api.github.com/repos/' .. REPO .. '/releases/latest',
    } }, on_result)
end

-- A raw dev checkout (version.lua's untouched placeholder) never checks - there's no
-- meaningful "current version" to compare against, and it would just nag on every launch.
function M.init(deps)
    t, osd, refresh_menu, save_state = deps.t, deps.osd, deps.refresh_menu, deps.save_state
    if CURRENT_VERSION == '0.0.0-dev' then return end
    if os.time() - M.last_checked >= CHECK_INTERVAL then M.check() end
end

return M
