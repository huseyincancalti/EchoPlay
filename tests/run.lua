-- Plain-Lua test runner for EchoPlay's pure logic (no mpv process needed). Complements
-- ci.yml's headless smoke test, which only proves the script loads without a Lua error - it
-- can't catch a wrong threshold or an off-by-one in things like quality.lua's auto-tier
-- stepping. Run with: lua5.1 tests/run.lua

package.path = './tests/mocks/?.lua;./tests/mocks/?/init.lua;'
    .. './mpv/scripts/echoplay-audio/?.lua;' .. package.path

local failures = 0
local function check(name, ok, detail)
    if ok then
        print('  ok   ' .. name)
    else
        failures = failures + 1
        print('  FAIL ' .. name .. (detail and (' - ' .. detail) or ''))
    end
end

-- ---------- util.lua ----------
do
    print('util.lua')
    local mp_utils = require 'mp.utils'
    local util = require 'util'

    check('to_json falls back to {} when format_json returns nil',
        util.to_json({ a = 1 }) == '{}')

    local orig_format_json = mp_utils.format_json
    mp_utils.format_json = function() return '{"a":1}' end
    check('to_json passes through a real format_json result',
        util.to_json({ a = 1 }) == '{"a":1}')
    mp_utils.format_json = orig_format_json

    check('read_json returns nil for a nonexistent path',
        util.read_json('/nonexistent/path/that/should/not/exist.json') == nil)
end

-- ---------- i18n.lua ----------
do
    print('i18n.lua')
    local i18n = require 'i18n'
    i18n.init('/nonexistent/opts/dir', 'tr') -- no files on disk -> falls back to FALLBACK table
    check('t() falls back to the built-in Turkish string for a known key',
        i18n.t('settings') == 'Ayarlar')
    check('t() falls back to the key itself for an unknown key',
        i18n.t('this_key_does_not_exist') == 'this_key_does_not_exist')
end

-- ---------- quality.lua ----------
do
    print('quality.lua')
    local mp = require 'mp'
    local quality = require 'quality'
    quality.init({
        t = function(k) return k end,
        osd = function() end,
        refresh_menu = function() end,
        save_state = function() end,
    })
    local poll = mp.timers[1].fn

    check('starts in auto mode at the normal tier',
        quality.pref == 'auto' and quality.tier_active == 'normal')

    -- First tick just establishes the frame-drop-count baseline (no prior count to diff against).
    mp.set_property_number('frame-drop-count', 0)
    poll()

    -- 3 ticks with a sustained drop delta >= threshold (6) should step normal -> low.
    for i = 1, 3 do
        mp.set_property_number('frame-drop-count', i * 10)
        poll()
    end
    check('steps down to low after 3 consecutive bad ticks', quality.tier_active == 'low')

    -- The tier just stepped down FROM ('normal') is now blocked for the rest of this file -
    -- by design ("never re-entering a tier that already proved too heavy on the current
    -- file"), so no amount of clean ticks should step back up until reset_for_new_file() runs.
    local last = mp.get_property_number('frame-drop-count')
    for _ = 1, 10 do
        last = last + 1
        mp.set_property_number('frame-drop-count', last)
        poll()
    end
    check('stays at low all file long once normal is blocked (never flaps back up)',
        quality.tier_active == 'low')

    -- A new file (reset_for_new_file) clears the block and lets auto re-probe from scratch.
    quality.reset_for_new_file()
    mp.set_property_number('frame-drop-count', 0)
    poll() -- re-establish baseline post-reset
    for i = 1, 5 do
        last = i
        mp.set_property_number('frame-drop-count', last)
        poll()
    end
    check('steps back up to normal on a new file once the good-streak threshold is met',
        quality.tier_active == 'normal')

    check('manually forcing a tier is idempotent (no error) when already active', (function()
        local ok = pcall(quality.set, 'normal')
        return ok
    end)())
end

-- ---------- update_check.lua ----------
do
    print('update_check.lua')
    local update_check = require 'update_check'

    check('newer patch version is detected', update_check.is_newer('1.4.1', '1.4.0'))
    check('newer minor version is detected', update_check.is_newer('1.5.0', '1.4.9'))
    check('newer major version is detected', update_check.is_newer('2.0.0', '1.9.9'))
    check('equal versions are not "newer"', not update_check.is_newer('1.4.0', '1.4.0'))
    check('older version is not "newer"', not update_check.is_newer('1.3.0', '1.4.0'))
    check('unparsable version never counts as newer', not update_check.is_newer('nightly', '1.4.0'))

    -- version.lua's checked-in placeholder (a raw dev checkout) must never fire a real
    -- network check - init() should return immediately instead of calling M.check().
    local checked = false
    local orig_check = update_check.check
    update_check.check = function() checked = true end
    update_check.init({ t = function(k) return k end, osd = function() end,
        refresh_menu = function() end, save_state = function() end })
    check('a raw dev checkout (0.0.0-dev) never triggers a network check', not checked)
    update_check.check = orig_check
end

print('')
if failures == 0 then
    print('All checks passed.')
    os.exit(0)
else
    print(failures .. ' check(s) failed.')
    os.exit(1)
end
