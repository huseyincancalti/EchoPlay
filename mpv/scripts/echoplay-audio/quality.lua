-- Picture-quality tiers (Auto/Ultra/Normal/Low). Auto starts at Normal (the tuned lanczos
-- baseline in mpv.conf - no profile to apply/restore), steps down on sustained frame drops,
-- and only dares Ultra after a long clean stretch - never re-entering a tier that already
-- proved too heavy on the current file.
local mp = require 'mp'

local M = {}
local t, osd, refresh_menu, save_state -- injected via M.init

M.TIERS = { 'ultra', 'normal', 'low' }
local TIER_PROFILE = { ultra = 'echoplay-tier-ultra', low = 'echoplay-tier-low' }

M.pref = 'auto'        -- 'auto' | 'ultra' | 'normal' | 'low'
M.tier_active = 'normal' -- the tier currently in effect (what auto mode is driving)

local PERF_POLL_INTERVAL = 2
local PERF_DROP_THRESHOLD = 6
local PERF_BAD_STREAK_NEEDED = 3      -- ~6s of sustained drops before stepping down
local PERF_GOOD_STREAK_NEEDED = 5     -- ~10s clean before stepping back up
local PERF_ULTRA_STREAK_NEEDED = 15   -- ~30s clean before daring the expensive Ultra tier
local perf_last_count, perf_bad_streak, perf_good_streak = nil, 0, 0
local perf_blocked = {} -- tiers that already caused drops on this file - auto won't retry them
local applied_initial = false

local function tier_index(tier)
    for i, v in ipairs(M.TIERS) do if v == tier then return i end end
    return nil
end

-- Always restores whatever tier profile is currently applied before applying the new one
-- (skips both steps for "normal", since it has no profile - it IS the restored baseline).
local function apply_tier(tier)
    if tier == M.tier_active then return end
    if TIER_PROFILE[M.tier_active] then mp.commandv('apply-profile', TIER_PROFILE[M.tier_active], 'restore') end
    if TIER_PROFILE[tier] then mp.commandv('apply-profile', TIER_PROFILE[tier]) end
    M.tier_active = tier
    refresh_menu()
end

local function perf_poll()
    if M.pref ~= 'auto' then
        perf_last_count = nil; perf_bad_streak, perf_good_streak = 0, 0
        return
    end
    local count = mp.get_property_number('frame-drop-count') or 0
    if perf_last_count == nil then perf_last_count = count; return end
    local delta = count - perf_last_count
    perf_last_count = count
    if delta < 0 then delta = 0 end -- defensive: counter reset mid-window (new file, or wrapped)
    local idx = tier_index(M.tier_active) or 2
    if delta >= PERF_DROP_THRESHOLD then
        perf_bad_streak = perf_bad_streak + 1; perf_good_streak = 0
        if perf_bad_streak >= PERF_BAD_STREAK_NEEDED and idx < #M.TIERS then
            perf_bad_streak = 0
            perf_blocked[M.TIERS[idx]] = true -- proved too heavy here; don't oscillate back into it
            local next_tier = M.TIERS[idx + 1]
            apply_tier(next_tier)
            osd(string.format(t('perf_auto_step_down'), t('perf_tier_' .. next_tier)))
        end
    else
        perf_good_streak = perf_good_streak + 1; perf_bad_streak = 0
        local target = idx > 1 and M.TIERS[idx - 1] or nil
        if target and perf_blocked[target] then target = nil end
        local need = target == 'ultra' and PERF_ULTRA_STREAK_NEEDED or PERF_GOOD_STREAK_NEEDED
        if target and perf_good_streak >= need then
            perf_good_streak = 0
            apply_tier(target)
            osd(string.format(t('perf_auto_step_up'), t('perf_tier_' .. target)))
        end
    end
end

function M.set(pref)
    M.pref = pref
    save_state()
    if pref == 'auto' then
        perf_bad_streak, perf_good_streak = 0, 0
        osd(string.format(t('perf_manual_auto'), t('perf_tier_' .. M.tier_active)))
    else
        apply_tier(pref)
        osd(string.format(t('perf_manual_forced'), t('perf_tier_' .. pref)))
    end
    refresh_menu()
end

-- Called from file-loaded: new file = new decode/render cost, let auto re-probe every tier.
function M.reset_for_new_file()
    perf_last_count = nil
    perf_bad_streak, perf_good_streak = 0, 0
    perf_blocked = {}
end

-- One-shot: applies a forced (non-auto) tier once, on the very first file of the session.
function M.apply_initial()
    if applied_initial then return end
    applied_initial = true
    if M.pref ~= 'auto' then apply_tier(M.pref) end
end

function M.init(deps)
    t, osd, refresh_menu, save_state = deps.t, deps.osd, deps.refresh_menu, deps.save_state
    mp.add_periodic_timer(PERF_POLL_INTERVAL, perf_poll)
end

return M
