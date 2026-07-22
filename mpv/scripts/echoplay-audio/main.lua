-- EchoPlay: one searchable, fully localized Settings menu (right-click, or the settings button).
--   * Audio mixer lives in its own collapsed submenu (toggle + gain + per-track Mono),
--     so a 9-track file doesn't dump everything on screen. Mixing is live (lavfi-complex).
--   * Sectioned settings (Audio Sources / Video / Picture Quality / Appearance / File /
--     General / Help) with headings + dividers.
--   * Appearance = Dark/Light mode + an accent color, computed in-script (clean live
--     switching). Community theme packs (themes/*.conf) still appear as advanced presets.
--   * Languages are community-extensible: drop a script-opts/echoplay-<code>.json.
-- Mixer state / mono / language / accent persist across runs.
-- Track names come from file metadata (e.g. ShadowPlay "System sounds"/"Microphone").
--
-- Split across this directory (mpv's own convention: a script can be a folder with a
-- main.<ext> entry point, and mpv prepends that folder to Lua's package.path so sibling
-- files resolve via plain require()): util.lua (JSON helpers), i18n.lua (strings),
-- quality.lua (picture-quality tiers), resume.lua (resume-position prompt). Menu building,
-- the audio mixer, and the keyboard-shortcut system stay here - they share too much
-- module-level state (tracks/gain/mode/accent/etc.) and funnel through one central
-- handle() dispatcher to split further without adding real complexity for no real gain.

local mp = require 'mp'
local utils = require 'mp.utils'
require 'mp.options'

local util = require 'util'
local i18n = require 'i18n'
local quality = require 'quality'
local resume = require 'resume'
local to_json = util.to_json

local o = {
    language = 'tr',
    default_on = '1',
    mix_normalize = 0,
    gain_step = 0.10,
    gain_max = 4.0,
    speed_factor = 2.0,
    osd = true,
    osd_duration = 2,
    labels = '',
}
read_options(o, 'echoplay')

local SCRIPT = mp.get_script_name()
local MENU_TYPE = 'echoplay'
local MENU_TYPE_MIX = 'echoplay-mixer'
local OPTS_DIR = mp.command_native({ 'expand-path', '~~/script-opts' })
local THEMES_DIR = mp.command_native({ 'expand-path', '~~/themes' })
local STATE_PATH = mp.command_native({ 'expand-path', '~~/echoplay-state.json' })

i18n.init(OPTS_DIR, o.language)
local t = i18n.t

-- Open a folder in the OS file manager (Explorer/Finder/whatever handles xdg-open).
local function open_folder(path)
    local platform = mp.get_property_native('platform')
    if platform == 'windows' then
        mp.commandv('run', 'explorer', path)
    elseif platform == 'darwin' then
        mp.commandv('run', 'open', path)
    else
        mp.commandv('run', 'xdg-open', path)
    end
end

-- ---------- appearance: built-in Dark/Light modes + accent ----------
-- Accent swatches shown in the Theme submenu. Higher-saturation picks chosen to pop
-- against the warm near-black/off-white backgrounds below. Add more freely.
local ACCENTS = {
    { key = 'accent_orange', hex = 'ff7d26' },
    { key = 'accent_blue',   hex = '3b9dff' },
    { key = 'accent_green',  hex = '30c96e' },
    { key = 'accent_purple', hex = '9a6bff' },
    { key = 'accent_pink',   hex = 'ff5fa2' },
    { key = 'accent_red',    hex = 'ff4d5d' },
}
-- Saved states from before the palette refresh hold the old hex values - remap on load.
local ACCENT_MIGRATE = {
    ee7733 = 'ff7d26', ['4aa3ff'] = '3b9dff', ['7ec699'] = '30c96e',
    a78bfa = '9a6bff', ef7fae = 'ff5fa2', e0707a = 'ff4d5d',
}
local mode = 'dark'        -- 'dark' | 'light'
local accent = 'ff7d26'

-- ---------- new-feature state (performance mode / screenshot location / first-run hint) ----------
local screenshot_preset = 'desktop'  -- 'desktop' | 'videos' | 'pictures' | 'video_folder' | 'custom'
local screenshot_custom_path = ''
local menu_hint_shown = false
local speed_step = 0.1               -- s/d fine speed-adjust step, user-configurable
-- Overall output volume (mpv's `volume` property, distinct from per-track `gain` in the
-- mixer). Each file opened via "Open with" is a brand new process, so without this it
-- resets to mpv.conf's volume=100 every time. Mirrors mpv.conf's own default rather than
-- starting nil: the volume-change observer's first callback isn't guaranteed to land before
-- file-loaded fires (caught live in CI - a nil here made save_state() crash on first file
-- load, since utils.format_json choked on it).
local saved_volume = 100

-- Every keyboard shortcut EchoPlay ships is in this list and rebindable from
-- Settings -> Yardım -> Klavye Kısayolları. The corresponding keys carry `ignore` stubs
-- in input.conf so mpv's core defaults don't resurrect on a key after it's rebound away.
-- Only right-click stays fixed: it's the guaranteed way into the menu.
-- `repeatable` = holding the key keeps firing (volume/seek/speed nudges).
local KEY_ACTIONS = {
    { id = 'speed_toggle', default_key = 'g', label_key = 'sc_speed' },
    { id = 'speed_down', default_key = 's', label_key = 'sc_speed_down', repeatable = true },
    { id = 'speed_up', default_key = 'd', label_key = 'sc_speed_up', repeatable = true },
    { id = 'screenshot', default_key = 'F1', label_key = 'sc_screenshot' },
    { id = 'shot_video', default_key = 'S', label_key = 'sc_screenshot_video' },
    { id = 'shot_window', default_key = 'Ctrl+s', label_key = 'sc_screenshot_window' },
    { id = 'track1', default_key = 'Ctrl+1', label_key = 'sc_track1' },
    { id = 'track2', default_key = 'Ctrl+2', label_key = 'sc_track2' },
    { id = 'track3', default_key = 'Ctrl+3', label_key = 'sc_track3' },
    { id = 'mute', default_key = 'm', label_key = 'sc_mute' },
    { id = 'pause', default_key = 'Space', label_key = 'sc_pause' },
    { id = 'vol_up', default_key = 'Up', label_key = 'sc_vol_up', repeatable = true },
    { id = 'vol_down', default_key = 'Down', label_key = 'sc_vol_down', repeatable = true },
    { id = 'seek_fwd', default_key = 'Right', label_key = 'sc_seek_fwd', repeatable = true },
    { id = 'seek_back', default_key = 'Left', label_key = 'sc_seek_back', repeatable = true },
}
local custom_keys = {} -- id -> user-chosen key, only set for actions the user has rebound
local rebind_capture -- forward-declared; assigned once open_menu/speed_nudge/handle etc. exist

-- Full color override (latest append wins, so switching modes resets cleanly in-session).
-- Backgrounds avoid pure #000/#fff on purpose: warm broken tones (charcoal-brown dark,
-- paper-warm light) read as designed rather than default. The curtain (uosc dims the video
-- behind any open menu) doubles as the "spotlight" effect: menu lit, everything else falls
-- away - without touching the video's own rendering.
local function color_string()
    if mode == 'light' then
        return ('foreground=%s,foreground_text=ffffff,background=f5f1ea,background_text=26211b,curtain=b9b0a4'):format(accent)
    end
    return ('foreground=%s,foreground_text=141210,background=171412,background_text=e9e4dc,curtain=0b0805'):format(accent)
end
local function accent_name(hex)
    for _, a in ipairs(ACCENTS) do if a.hex == hex then return a.key end end
    return nil
end

-- ---------- state persistence ----------
-- Bumped whenever a saved field's meaning/format changes in a way old saves won't match
-- (e.g. perf_pref's value set changing). load_state()'s migration block below is the only
-- place that needs to know about a previous schema's format - saves always write CURRENT.
local CURRENT_SCHEMA = 2
local saved_gains, saved_mono = {}, {}
local function load_state()
    local s = util.read_json(STATE_PATH); if not s then return end
    local schema = tonumber(s._schema) or 1 -- saves before this field existed are schema 1

    if s.default_on then o.default_on = s.default_on end
    if s.language then o.language = s.language end
    if type(s.accent) == 'string' and s.accent ~= '' then accent = ACCENT_MIGRATE[s.accent] or s.accent end
    if tonumber(s.speed_factor) then o.speed_factor = tonumber(s.speed_factor) end
    if type(s.gains) == 'table' then saved_gains = s.gains end
    if type(s.mono) == 'table' then saved_mono = s.mono end
    if type(s.screenshot_preset) == 'string' and s.screenshot_preset ~= '' then screenshot_preset = s.screenshot_preset end
    if type(s.screenshot_custom_path) == 'string' then screenshot_custom_path = s.screenshot_custom_path end
    if s.menu_hint_shown then menu_hint_shown = true end
    if tonumber(s.speed_step) then speed_step = tonumber(s.speed_step) end
    if tonumber(s.volume) then saved_volume = tonumber(s.volume) end
    -- Copy entries instead of adopting the parsed table: utils.parse_json tags tables that
    -- came from a JSON array (and an EMPTY table always round-trips as []), and format_json
    -- then ignores string keys added to such a table forever - saves would silently lose
    -- every rebind (found via live IPC test: binding worked in-memory, file always had []).
    -- Not schema-gated: this JSON array/object quirk can recur regardless of version.
    if type(s.custom_keys) == 'table' then
        for k, v in pairs(s.custom_keys) do
            if type(k) == 'string' and type(v) == 'string' then custom_keys[k] = v end
        end
    end

    -- Current-format reads (apply regardless of schema - a field already in its current
    -- shape doesn't need migrating just because other fields in the same save are old).
    local mode_matched = s.mode == 'light' or s.mode == 'dark'
    if mode_matched then mode = s.mode end
    local perf_matched = s.perf_pref == 'auto' or s.perf_pref == 'ultra' or s.perf_pref == 'normal' or s.perf_pref == 'low'
    if perf_matched then quality.pref = s.perf_pref end
    -- Schema 1 -> 2: `theme` was renamed `mode`; perf_pref went through a binary on/off
    -- toggle, then a 5-tier set (high/lowest), before landing on the current 4-way scheme.
    if schema < 2 then
        if not mode_matched and (s.theme == 'light' or s.theme == 'dark') then mode = s.theme end
        if not perf_matched then
            if s.perf_pref == 'on' or s.perf_pref == 'lowest' then quality.pref = 'low'
            elseif s.perf_pref == 'off' or s.perf_pref == 'high' then quality.pref = 'normal' end
        end
    end
end
load_state()
-- Applied here (top-level, before any file loads) rather than per-file: volume is one
-- mpv session's global property, not something to re-snap on every playlist entry.
mp.set_property_number('volume', saved_volume)

local custom = {}
for pair in o.labels:gmatch('[^,]+') do
    local k, v = pair:match('^%s*(%d+)%s*=%s*(.-)%s*$')
    if k then custom[tonumber(k)] = v end
end

local CHECK = { [true] = 'check_box', [false] = 'check_box_outline_blank' }
local RADIO = { [true] = 'radio_button_checked', [false] = 'radio_button_unchecked' }
local PLAYER = { ['open-file'] = 'open-file', playlist = 'playlist', subtitles = 'subtitles',
    ['audio-device'] = 'audio-device', ['config-dir'] = 'open-config-directory' }

local tracks, info = {}, {}
local enabled, gain, mono = {}, {}, {}
local appearance_applied = false

-- ---------- helpers ----------
local function label(aid)
    if custom[aid] then return custom[aid] end
    local tr = info[aid] or {}
    local md = tr.metadata or {}
    local name = tr.title or md.title or md.name or md.NAME
    if name and name ~= '' then return name end
    if tr.lang and tr.lang ~= '' and tr.lang ~= 'und' then return tr.lang:upper() end
    return 'Ses ' .. aid
end
-- With the common 2-track case (e.g. ShadowPlay/OBS system+mic), show both source
-- names right on the menu row instead of the generic "Ses Kaynakları" label.
local function mixer_title()
    if #tracks == 2 then return label(tracks[1]) .. ' / ' .. label(tracks[2]) end
    return t('mixer')
end
local function pct(aid) return math.floor((gain[aid] or 1) * 100 + 0.5) end
-- Mono is already shown in the title's " · Mono" suffix - not repeated here, to leave the
-- action-button cluster (down/up/mono, see mixer_data) enough room to render without clipping.
-- Percentage is padded to a fixed width: uosc lays its action buttons out relative to the
-- menu's overall content width, so an unpadded "9%" -> "10%" -> "100%" transition on every
-- +/- click reflows that width and shifts the buttons out from under a stationary cursor
-- (same class of bug menu_data() already avoids for the speed rows - see its comment on
-- item_actions_place below).
local function hint(aid)
    local parts = { string.format('%3d%%', pct(aid)) }
    if (info[aid] or {}).codec then parts[#parts + 1] = info[aid].codec end
    return table.concat(parts, ' · ')
end
local function scan()
    tracks, info = {}, {}
    for _, tr in ipairs(mp.get_property_native('track-list') or {}) do
        if tr.type == 'audio' then tracks[#tracks + 1] = tr.id; info[tr.id] = tr end
    end
end
local function on_list()
    local r = {}
    for _, aid in ipairs(tracks) do if enabled[aid] then r[#r + 1] = aid end end
    return r
end
local function osd(text) if o.osd then mp.osd_message(text, o.osd_duration) end end
-- `keep-open=yes` only stops mpv from exiting once there's truly nothing left to play - it
-- does NOT stop mid-playlist auto-advance (proven live: with autocreate-playlist=same,
-- "Sonda dur" still jumped to the next file in the folder; a pause-on-eof-reached hook
-- didn't help either - by the time the Lua callback ran, mpv had already committed to the
-- next file). `keep-open=always` is the real fix - also verified live: it blocks the
-- advance entirely and pauses on the current file's last frame.
local function cur_end()
    if mp.get_property('loop-file') == 'inf' then return 'loop' end
    if mp.get_property('keep-open') == 'always' then return 'stop' end
    return 'next'
end
local function speed_on() return (mp.get_property_number('speed') or 1) > 1.01 end

local function available_langs() return i18n.available() end
-- Community theme packs (full uosc-option overrides) shown as advanced presets.
local function available_themes()
    local list, files = {}, utils.readdir(THEMES_DIR, 'files') or {}
    table.sort(files)
    for _, fn in ipairs(files) do
        local name = fn:match('^(.+)%.conf$')
        if name then list[#list + 1] = name end
    end
    return list
end
local function theme_display(name)
    local f = io.open(THEMES_DIR .. '/' .. name .. '.conf', 'r')
    if f then
        local first_line = f:read('*l')
        f:close()
        local h = first_line and first_line:match('^#%s*(.+)$')
        if h then return h end
    end
    return (name:gsub('^%l', string.upper))
end

local function save_state()
    local on, list = {}, {}
    for _, aid in ipairs(tracks) do if enabled[aid] then on[#on + 1] = aid end end
    local don
    if #tracks > 0 and #on == #tracks then don = 'all'
    elseif #on == 0 then don = 'none'
    else for _, a in ipairs(on) do list[#list + 1] = tostring(a) end; don = table.concat(list, ',') end
    local gains, monos = {}, {}
    for aid, g in pairs(gain) do if g ~= 1 then gains[tostring(aid)] = g end end
    for aid, v in pairs(mono) do if v then monos[tostring(aid)] = true end end
    local f = io.open(STATE_PATH, 'w')
    if f then
        f:write(to_json({ _schema = CURRENT_SCHEMA, default_on = don, gains = gains, mono = monos,
            language = o.language, mode = mode, accent = accent, speed_factor = o.speed_factor,
            perf_pref = quality.pref, screenshot_preset = screenshot_preset, menu_hint_shown = menu_hint_shown,
            screenshot_custom_path = screenshot_custom_path, speed_step = speed_step, custom_keys = custom_keys,
            volume = saved_volume }))
        f:close()
    end
end
-- Debounced so dragging uosc's volume slider (many rapid property changes) doesn't hammer
-- the disk with a synchronous write on every pixel of movement.
local volume_save_timer = nil
mp.observe_property('volume', 'number', function(_, v)
    if type(v) ~= 'number' then return end
    saved_volume = v
    if volume_save_timer then volume_save_timer:kill() end
    volume_save_timer = mp.add_timeout(0.5, save_state)
end)

-- ---------- menus ----------
-- Section header row: bold + an anchoring icon (uosc renders item icons on the right edge).
-- Not muted - these are the visual signposts of the menu, they should pop, not fade.
local function heading(text, icon) return { title = text, icon = icon, selectable = false, bold = true } end
-- Top-level Settings sections only (menu_data): same as heading(), plus `active = true`,
-- which makes uosc paint the row as a full-width accent-colored bar even though it's not
-- selectable - a real uosc rendering rule (not gated on selectable), with text color
-- resolving to foreground_text automatically for contrast in both light/dark mode. Not
-- used for in-submenu group labels (theme_items' Mod/Vurgu Rengi/Özel Temalar), since those
-- submenus already use `active` for the real currently-selected radio row - reusing the same
-- treatment for a group label there would make "heading" and "selected value" look identical.
local function section_heading(text, icon)
    return { title = text, icon = icon, selectable = false, bold = true, active = true }
end

-- Value labels shown as the muted right-side hint on rows that open submenus, so the
-- current choice is readable without opening anything ("what will I find in here?").
local function theme_label()
    local mode_name = mode == 'dark' and t('theme_dark') or t('theme_light')
    local key = accent_name(accent)
    return mode_name .. ' · ' .. (key and t(key) or accent)
end
local function screenshot_label()
    if screenshot_preset == 'custom' then
        -- Show only the last path segment; a full path is too wide for a hint.
        return screenshot_custom_path:match('([^/\\]+)[/\\]*$') or screenshot_custom_path
    end
    return t('screenshot_' .. screenshot_preset)
end
local function separator() return { title = '', separator = true, selectable = false } end

local function theme_items()
    local items = {}
    items[#items + 1] = heading(t('theme_mode'), 'brightness_6')
    items[#items + 1] = { title = t('theme_dark'), icon = RADIO[mode == 'dark'], active = mode == 'dark',
        value = 'mode:dark', keep_open = true }
    items[#items + 1] = { title = t('theme_light'), icon = RADIO[mode == 'light'], active = mode == 'light',
        value = 'mode:light', keep_open = true }
    items[#items + 1] = separator()
    items[#items + 1] = heading(t('accent'), 'palette')
    for _, a in ipairs(ACCENTS) do
        items[#items + 1] = { title = t(a.key), icon = RADIO[accent == a.hex], active = accent == a.hex,
            value = 'accent:' .. a.hex, keep_open = true }
    end
    local customs = available_themes()
    if #customs > 0 then
        items[#items + 1] = separator()
        items[#items + 1] = heading(t('custom_themes'), 'style')
        for _, name in ipairs(customs) do
            items[#items + 1] = { title = theme_display(name), icon = 'palette', value = 'theme:' .. name, keep_open = true }
        end
    end
    items[#items + 1] = separator()
    items[#items + 1] = { title = t('open_theme'), icon = 'folder_open', value = 'theme-folder' }
    return items
end

local function lang_items()
    local items = {}
    for _, l in ipairs(available_langs()) do
        items[#items + 1] = { title = l.name, icon = RADIO[o.language == l.code], active = o.language == l.code,
            value = 'lang:' .. l.code }
    end
    items[#items + 1] = separator()
    items[#items + 1] = { title = t('open_lang'), icon = 'folder_open', value = 'lang-folder' }
    return items
end

-- Speed step (used by the s/d fine speed-adjust keys) - radio presets, same shape
-- as theme_items()'s accent-color radio.
local function speed_step_items()
    local items = {}
    for _, step in ipairs({ 0.05, 0.1, 0.2, 0.5 }) do
        items[#items + 1] = { title = string.format('%.2gx', step), icon = RADIO[speed_step == step],
            active = speed_step == step, value = 'speed-step:' .. step, keep_open = true }
    end
    return items
end

-- Performance Mode: Auto (title carries its current pick in parentheses) + 3 forced tiers,
-- each with a one-line plain-language hint of who it's for.
local function perf_items()
    local items = {}
    local is_auto = quality.pref == 'auto'
    items[#items + 1] = {
        title = is_auto and string.format('%s (%s)', t('perf_auto'), t('perf_tier_' .. quality.tier_active)) or t('perf_auto'),
        hint = t('perf_auto_hint'), icon = RADIO[is_auto], active = is_auto, value = 'perf:auto', keep_open = true }
    for _, tier in ipairs(quality.TIERS) do
        items[#items + 1] = { title = t('perf_tier_' .. tier), hint = t('perf_' .. tier .. '_hint'),
            icon = RADIO[quality.pref == tier], active = quality.pref == tier, value = 'perf:' .. tier, keep_open = true }
    end
    return items
end

-- Read-only keyboard shortcuts reference: EchoPlay's own bindings (input.conf) plus the
-- mpv screenshot-variant defaults we deliberately don't override.
-- One continuous list, same shape for every row: description left, key right.
-- Every row is click-to-rebind except right-click, which stays locked as the guaranteed
-- way into this very menu (rebind it wrong and there's no way back in).
local function shortcuts_items()
    local items = {}
    items[#items + 1] = { title = t('sc_rebind_hint'), selectable = false, muted = true }
    items[#items + 1] = { title = t('sc_menu'), hint = t('sc_rclick'), icon = 'lock', selectable = false }
    for _, action in ipairs(KEY_ACTIONS) do
        items[#items + 1] = { title = t(action.label_key), hint = custom_keys[action.id] or action.default_key,
            icon = 'edit', value = 'rebind:' .. action.id }
    end
    return items
end

-- Screenshot location: preset radio (Desktop / Videos / Pictures / same folder as video).
local function screenshot_items()
    local items = {}
    items[#items + 1] = { title = t('screenshot_desktop'), icon = RADIO[screenshot_preset == 'desktop'],
        active = screenshot_preset == 'desktop', value = 'screenshot:desktop', keep_open = true }
    items[#items + 1] = { title = t('screenshot_videos'), icon = RADIO[screenshot_preset == 'videos'],
        active = screenshot_preset == 'videos', value = 'screenshot:videos', keep_open = true }
    items[#items + 1] = { title = t('screenshot_pictures'), icon = RADIO[screenshot_preset == 'pictures'],
        active = screenshot_preset == 'pictures', value = 'screenshot:pictures', keep_open = true }
    items[#items + 1] = { title = t('screenshot_video_folder'), icon = RADIO[screenshot_preset == 'video_folder'],
        active = screenshot_preset == 'video_folder', value = 'screenshot:video_folder', keep_open = true }
    if screenshot_preset == 'custom' then
        items[#items + 1] = { title = screenshot_custom_path, icon = RADIO[true], active = true,
            value = 'screenshot:custom', keep_open = true }
    end
    if mp.get_property_native('platform') == 'windows' then
        items[#items + 1] = separator()
        items[#items + 1] = { title = t('screenshot_custom'), icon = 'folder_open', value = 'screenshot-pick' }
    end
    return items
end

-- Main settings menu: grouped into Audio Sources / Video / Picture Quality / Appearance / File / General / Help.
local function menu_data()
    local items = {}
    -- Audio sources (the core dual-track feature - kept first and on its own)
    -- Layout rules (approved mockup): rows that open a submenu always show the muted
    -- current value + a chevron_right; only leaf/action rows carry a functional icon.
    -- uosc draws one icon slot per row (right edge), so it's chevron OR icon, never both.
    items[#items + 1] = section_heading(t('sec_audio'), 'graphic_eq')
    items[#items + 1] = { title = mixer_title(), icon = 'chevron_right',
        hint = #tracks > 0 and string.format(t('tracks_on'), #on_list(), #tracks) or nil, value = 'open-mixer' }
    -- Video settings (speed and playback flow)
    items[#items + 1] = separator()
    items[#items + 1] = section_heading(t('sec_video'), 'play_circle')
    items[#items + 1] = { title = string.format('%s: %.2gx', t('speed_fine'), mp.get_property_number('speed') or 1),
        icon = 'speed', value = 'speed-fine', keep_open = true,
        actions = { { name = 'down', icon = 'remove', label = t('slower') },
            { name = 'up', icon = 'add', label = t('faster') } } }
    items[#items + 1] = { title = t('speed_step_label'), icon = 'chevron_right',
        hint = string.format('%.2gx', speed_step), items = speed_step_items() }
    items[#items + 1] = { title = string.format('%s: %.2gx', t('speed'), o.speed_factor),
        icon = CHECK[speed_on()], value = 'speed', keep_open = true,
        actions = { { name = 'down', icon = 'remove', label = t('slower') }, { name = 'up', icon = 'add', label = t('faster') } } }
    local e = cur_end()
    items[#items + 1] = { title = t('video_end'), icon = 'chevron_right', hint = t('end_' .. e), items = {
        { title = t('end_next'), icon = RADIO[e == 'next'], active = e == 'next', value = 'end:next' },
        { title = t('end_loop'), icon = RADIO[e == 'loop'], active = e == 'loop', value = 'end:loop' },
        { title = t('end_stop'), icon = RADIO[e == 'stop'], active = e == 'stop', value = 'end:stop' },
    } }
    -- Picture quality (rendering)
    items[#items + 1] = separator()
    items[#items + 1] = section_heading(t('sec_quality'), 'high_quality')
    items[#items + 1] = { title = t('perf_mode'), icon = 'chevron_right',
        hint = quality.pref == 'auto'
            and string.format('%s (%s)', t('perf_auto'), t('perf_tier_' .. quality.tier_active))
            or t('perf_tier_' .. quality.pref),
        items = perf_items() }
    -- Appearance
    items[#items + 1] = separator()
    items[#items + 1] = section_heading(t('sec_appearance'), 'palette')
    items[#items + 1] = { title = t('theme'), icon = 'chevron_right', hint = theme_label(), items = theme_items() }
    items[#items + 1] = { title = t('language'), icon = 'chevron_right', hint = t('lang_name'), items = lang_items() }
    -- File
    items[#items + 1] = separator()
    items[#items + 1] = section_heading(t('sec_file'), 'folder')
    items[#items + 1] = { title = t('open_file'), icon = 'folder_open', value = 'open-file' }
    items[#items + 1] = { title = t('playlist'), icon = 'list', value = 'playlist' }
    items[#items + 1] = { title = t('subtitles'), icon = 'subtitles', value = 'subtitles' }
    items[#items + 1] = { title = t('screenshot_location'), icon = 'chevron_right',
        hint = screenshot_label(), items = screenshot_items() }
    -- General
    items[#items + 1] = separator()
    items[#items + 1] = section_heading(t('sec_general'), 'settings')
    items[#items + 1] = { title = t('audio_device'), icon = 'speaker', value = 'audio-device' }
    items[#items + 1] = { title = t('config_dir'), icon = 'folder_special', value = 'config-dir' }
    -- Help
    items[#items + 1] = separator()
    items[#items + 1] = section_heading(t('sec_help'), 'help_outline')
    items[#items + 1] = { title = t('shortcuts'), icon = 'chevron_right', items = shortcuts_items() }
    -- Actions stay inside the row ('outside' floats them detached from the menu, which both
    -- looks odd and breaks rapid repeat-clicking of +/- on the speed rows).
    return { type = MENU_TYPE, title = t('settings'), search_style = 'palette',
        callback = { SCRIPT, 'menu-event' }, items = items }
end

-- Audio mixer (its own menu so live refresh keeps us in place).
local function mixer_data()
    local actions = {
        { name = 'down', icon = 'remove', label = t('gain_down') },
        { name = 'up', icon = 'add', label = t('gain_up') },
        { name = 'mono', icon = 'surround_sound', label = t('mono') },
    }
    local items = {}
    for _, aid in ipairs(tracks) do
        items[#items + 1] = { title = label(aid) .. (mono[aid] and (' · ' .. t('mono_short')) or ''),
            hint = hint(aid), icon = CHECK[enabled[aid] == true], value = aid, actions = actions, keep_open = true }
    end
    if #tracks == 0 then items[#items + 1] = { title = t('single_track'), selectable = false, muted = true } end
    items[#items + 1] = separator()
    items[#items + 1] = { title = t('all_on'), icon = 'select_all', value = 'all', keep_open = true }
    items[#items + 1] = { title = t('mute'), icon = 'volume_off', value = 'none', keep_open = true }
    items[#items + 1] = separator()
    items[#items + 1] = { title = t('back'), icon = 'arrow_back', value = 'back' }
    -- No item_actions_place='outside' here (same reasoning as menu_data()'s speed rows,
    -- see its comment): 'outside' anchors the action buttons off the menu's overall content
    -- width, so refresh_mixer()'s live text updates (gain %, above) shifted them out from
    -- under the cursor on every click, forcing a re-hover before the next click registered.
    return { type = MENU_TYPE_MIX, title = t('mixer'), search_style = 'palette',
        callback = { SCRIPT, 'menu-event' }, items = items }
end

local function open_menu() mp.commandv('script-message-to', 'uosc', 'open-menu', to_json(menu_data())) end
local function refresh_menu() mp.commandv('script-message-to', 'uosc', 'update-menu', to_json(menu_data())) end
local function close_menu() mp.commandv('script-message-to', 'uosc', 'close-menu', MENU_TYPE) end
local function open_mixer() mp.commandv('script-message-to', 'uosc', 'open-menu', to_json(mixer_data())) end
local function refresh_mixer() mp.commandv('script-message-to', 'uosc', 'update-menu', to_json(mixer_data())) end

local function update_button()
    local n = #on_list()
    mp.commandv('script-message-to', 'uosc', 'set-button', 'echoplay', to_json({
        icon = 'settings', badge = #tracks > 1 and tostring(n) or nil,
        tooltip = t('settings'), active = n > 1, command = 'script-message echoplay-menu',
    }))
end

-- ---------- audio ----------
local function track_chain(aid, out)
    local f = string.format('[aid%d]volume=%.2f', aid, gain[aid] or 1)
    if mono[aid] then f = f .. ',pan=stereo|c0=0.5*c0+0.5*c1|c1=0.5*c0+0.5*c1' end
    return f .. out
end
local function apply_audio()
    local on = on_list()
    if #on == 0 then
        mp.set_property('lavfi-complex', ''); mp.set_property('aid', 'no')
    elseif #on == 1 and (gain[on[1]] or 1) == 1 and not mono[on[1]] then
        mp.set_property('lavfi-complex', ''); mp.set_property_number('aid', on[1])
    elseif #on == 1 then
        mp.set_property('lavfi-complex', track_chain(on[1], '[ao]')); mp.set_property('aid', 'no')
    else
        local pre, labels = {}, {}
        for _, aid in ipairs(on) do
            pre[#pre + 1] = track_chain(aid, string.format('[ev%d]', aid))
            labels[#labels + 1] = string.format('[ev%d]', aid)
        end
        mp.set_property('lavfi-complex', table.concat(pre, ';') .. ';' .. table.concat(labels) ..
            'amix=inputs=' .. #on .. ':normalize=' .. o.mix_normalize .. '[ao]')
        mp.set_property('aid', 'no')
    end
end
local function osd_combo()
    local on = on_list()
    if #on == 0 then return '🔊 ' .. t('silent') end
    local names = {}
    for _, aid in ipairs(on) do
        local g = pct(aid)
        names[#names + 1] = label(aid) .. (g ~= 100 and (' ' .. g .. '%') or '') .. (mono[aid] and (' ' .. t('mono_short')) or '')
    end
    return '🔊 ' .. table.concat(names, ' + ')
end

-- ---------- actions ----------
local function mixer_changed(text) apply_audio(); save_state(); osd(text); update_button(); refresh_mixer() end
local function adjust(aid, delta)
    local g = (gain[aid] or 1) + delta
    if g < 0 then g = 0 elseif g > o.gain_max then g = o.gain_max end
    gain[aid] = g; enabled[aid] = true
    mixer_changed(string.format('%s  %d%%', label(aid), pct(aid)))
end

local function speed_toggle()
    mp.set_property_number('speed', speed_on() and 1 or o.speed_factor)
    osd(string.format(t('speed_osd'), tostring(mp.get_property_number('speed') or 1)))
    refresh_menu()
end
local function speed_adjust(delta)
    o.speed_factor = math.max(1.25, math.min(4.0, o.speed_factor + delta))
    if speed_on() then mp.set_property_number('speed', o.speed_factor) end
    save_state(); osd(string.format(t('speed_osd'), tostring(o.speed_factor))); refresh_menu()
end

-- Continuous live-speed nudge via s/d, independent of the g/speed_factor toggle above.
local function speed_nudge(delta)
    local cur = mp.get_property_number('speed') or 1
    local new = math.max(0.1, math.min(100, cur + delta))
    mp.set_property_number('speed', new)
    osd(string.format(t('speed_osd'), tostring(new)))
    refresh_menu()
end
local function speed_reset()
    mp.set_property_number('speed', 1)
    osd(string.format(t('speed_osd'), '1'))
    refresh_menu()
end
local function set_speed_step(step)
    speed_step = step
    save_state()
    refresh_menu()
end
local function set_end(mode_)
    if mode_ == 'loop' then mp.set_property('loop-file', 'inf'); mp.set_property('keep-open', 'no')
    elseif mode_ == 'stop' then mp.set_property('loop-file', 'no'); mp.set_property('keep-open', 'always')
    else mp.set_property('loop-file', 'no'); mp.set_property('keep-open', 'no') end
    osd(string.format(t('end_osd'), t('end_' .. mode_))); close_menu()
end

local function apply_appearance()
    mp.commandv('change-list', 'script-opts', 'append', 'uosc-color=' .. color_string())
end
-- refresh_menu() rebuilds the whole tree; uosc 5.x keeps us in the same submenu
-- (matched by its stable title id) and preserves the selection, so the radio
-- icons update live without bouncing back to the menu root.
local function set_mode(m)
    mode = m; apply_appearance(); save_state()
    osd(string.format(t('theme_osd'), m == 'light' and t('theme_light') or t('theme_dark')))
    refresh_menu()
end
local function set_accent(hex)
    accent = hex; apply_appearance(); save_state()
    local key = accent_name(hex)
    osd(string.format(t('accent_osd'), key and t(key) or hex))
    refresh_menu()
end
-- Community theme pack: applies its raw uosc options live (advanced preview, not persisted).
local function apply_theme_pack(name)
    local f = io.open(THEMES_DIR .. '/' .. name .. '.conf', 'r'); if not f then return end
    for line in f:lines() do
        if line:sub(1, 1) ~= '#' then
            local k, v = line:match('^%s*([%w_]+)%s*=%s*(.-)%s*$')
            if k then mp.commandv('change-list', 'script-opts', 'append', 'uosc-' .. k .. '=' .. v) end
        end
    end
    f:close()
    osd(string.format(t('theme_osd'), theme_display(name)))
end

local function persist_uosc_language(code)
    local path = OPTS_DIR .. '/uosc.conf'
    local f = io.open(path, 'r'); if not f then return end
    local c = f:read('*a'); f:close()
    c = c:gsub('\n#?languages=[^\n]*', '\nlanguages=' .. code .. ',slang,en')
    local w = io.open(path, 'w'); if w then w:write(c); w:close() end
end
local function set_language(code)
    o.language = code
    i18n.set(code)
    mp.commandv('change-list', 'script-opts', 'append', 'uosc-languages=' .. code .. ',slang,en')
    persist_uosc_language(code)
    save_state(); osd(string.format(t('lang_osd'), t('lang_name')))
    update_button(); open_menu()
end

-- ---------- screenshot location ----------
-- mpv only has a built-in '~~desktop/' alias; Videos/Pictures need resolving per platform.
local resolved_videos_dir, resolved_pictures_dir = nil, nil
local function platform_special_dir(kind) -- kind = 'MyVideos' | 'MyPictures'
    local platform = mp.get_property_native('platform')
    if platform == 'windows' then
        -- playback_only=false: this can be invoked from the idle screen (no file loaded), where
        -- mpv would otherwise kill a playback-tied subprocess immediately.
        local res = mp.command_native({ name = 'subprocess', playback_only = false, args = {
            'powershell', '-NoProfile', '-Command', '[Environment]::GetFolderPath("' .. kind .. '")'
        }, capture_stdout = true })
        local out = res and res.stdout and res.stdout:gsub('%s+$', '')
        return (out and out ~= '') and out or nil
    end
    local name = (kind == 'MyVideos') and '~/Videos' or '~/Pictures'
    return mp.command_native({ 'expand-path', name })
end
local function apply_screenshot_preset()
    local dir
    if screenshot_preset == 'videos' then
        resolved_videos_dir = resolved_videos_dir or platform_special_dir('MyVideos')
        dir = resolved_videos_dir
    elseif screenshot_preset == 'pictures' then
        resolved_pictures_dir = resolved_pictures_dir or platform_special_dir('MyPictures')
        dir = resolved_pictures_dir
    elseif screenshot_preset == 'video_folder' then
        local path = mp.get_property('path')
        if path then dir = utils.split_path(path) end
    elseif screenshot_preset == 'custom' then
        dir = screenshot_custom_path
    else
        dir = mp.command_native({ 'expand-path', '~~desktop/' })
    end
    if dir and dir ~= '' then mp.set_property('screenshot-directory', dir) end
end
local function set_screenshot_preset(preset)
    screenshot_preset = preset
    apply_screenshot_preset()
    save_state()
    osd(string.format(t('screenshot_osd'), preset == 'custom' and screenshot_custom_path or t('screenshot_' .. preset)))
    refresh_menu()
end

-- Real Windows folder-browser dialog (Klasör Seç...), for a fully custom path beyond the presets.
-- Async: the dialog can stay open for a while, and a synchronous mp.command_native() call would
-- block this script (and menu interactions) until the user closes it.
local function pick_custom_screenshot_dir()
    mp.command_native_async({ name = 'subprocess', playback_only = false, capture_stdout = true, args = {
        'powershell', '-NoProfile', '-Command',
        'Add-Type -AssemblyName System.Windows.Forms; $f=New-Object System.Windows.Forms.FolderBrowserDialog; if($f.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK){$f.SelectedPath}'
    } }, function(_, res)
        local out = res and res.stdout and res.stdout:gsub('%s+$', '')
        if out and out ~= '' then
            screenshot_custom_path = out
            set_screenshot_preset('custom')
        end
    end)
end

quality.init({ t = t, osd = osd, refresh_menu = refresh_menu, save_state = save_state })
resume.init({ t = t })

local function handle(value, action)
    if value == 'open-mixer' then open_mixer()
    elseif value == 'back' then open_menu()
    elseif value == 'all' then
        for _, aid in ipairs(tracks) do enabled[aid] = true end; mixer_changed(osd_combo())
    elseif value == 'none' then
        for _, aid in ipairs(tracks) do enabled[aid] = false end; mixer_changed(osd_combo())
    elseif value == 'speed' then
        if action == 'up' then speed_adjust(0.25)
        elseif action == 'down' then speed_adjust(-0.25)
        else speed_toggle() end
    elseif value == 'speed-fine' then
        if action == 'up' then speed_nudge(speed_step)
        elseif action == 'down' then speed_nudge(-speed_step)
        else speed_reset() end
    elseif type(value) == 'string' and value:sub(1, 11) == 'speed-step:' then set_speed_step(tonumber(value:sub(12)))
    elseif type(value) == 'string' and value:sub(1, 4) == 'end:' then set_end(value:sub(5))
    elseif type(value) == 'string' and value:sub(1, 5) == 'mode:' then set_mode(value:sub(6))
    elseif type(value) == 'string' and value:sub(1, 7) == 'accent:' then set_accent(value:sub(8))
    elseif type(value) == 'string' and value:sub(1, 6) == 'theme:' then apply_theme_pack(value:sub(7))
    elseif type(value) == 'string' and value:sub(1, 5) == 'lang:' then set_language(value:sub(6))
    elseif type(value) == 'string' and value:sub(1, 5) == 'perf:' then quality.set(value:sub(6))
    elseif type(value) == 'string' and value:sub(1, 11) == 'screenshot:' then set_screenshot_preset(value:sub(12))
    elseif value == 'screenshot-pick' then pick_custom_screenshot_dir()
    elseif type(value) == 'string' and value:sub(1, 7) == 'rebind:' then
        close_menu() -- the open menu's key grab would eat the key we're about to capture
        local id = value:sub(8)
        for _, key_action in ipairs(KEY_ACTIONS) do if key_action.id == id then rebind_capture(key_action) end end
    elseif value == 'lang-folder' then close_menu(); open_folder(OPTS_DIR)
    elseif value == 'theme-folder' then close_menu(); open_folder(THEMES_DIR)
    elseif PLAYER[value] then close_menu(); mp.commandv('script-binding', 'uosc/' .. PLAYER[value])
    else
        local aid = tonumber(value)
        if not aid then return end
        if action == 'up' then adjust(aid, o.gain_step)
        elseif action == 'down' then adjust(aid, -o.gain_step)
        elseif action == 'mono' then
            mono[aid] = not mono[aid]; enabled[aid] = true; mixer_changed(osd_combo())
        else enabled[aid] = not enabled[aid]; mixer_changed(osd_combo()) end
    end
end

-- ---------- rebindable keyboard shortcuts ----------
-- KEY_ACTIONS is declared near the top; the actual functions can only be wired up here,
-- once everything they call (open_menu, speed_nudge, handle, ...) already exists.
local KEY_ACTION_FN = {
    speed_toggle = speed_toggle,
    speed_down = function() speed_nudge(-speed_step) end,
    speed_up = function() speed_nudge(speed_step) end,
    screenshot = function() mp.commandv('screenshot') end,
    shot_video = function() mp.commandv('screenshot', 'video') end,
    shot_window = function() mp.commandv('screenshot', 'window') end,
    track1 = function() handle(1, nil) end,
    track2 = function() handle(2, nil) end,
    track3 = function() handle(3, nil) end,
    mute = function() mp.commandv('cycle', 'mute') end,
    pause = function() mp.commandv('cycle', 'pause') end,
    vol_up = function() mp.command('no-osd add volume 5'); mp.commandv('script-binding', 'uosc/flash-volume') end,
    vol_down = function() mp.command('no-osd add volume -5'); mp.commandv('script-binding', 'uosc/flash-volume') end,
    seek_fwd = function() mp.command('no-osd seek 10'); mp.commandv('script-binding', 'uosc/flash-timeline') end,
    seek_back = function() mp.command('no-osd seek -10'); mp.commandv('script-binding', 'uosc/flash-timeline') end,
}
local function bind_action(action)
    mp.add_forced_key_binding(custom_keys[action.id] or action.default_key, action.id, KEY_ACTION_FN[action.id],
        action.repeatable and { repeatable = true } or nil)
end
local function rebind_action(action, new_key)
    mp.remove_key_binding(action.id)
    custom_keys[action.id] = new_key
    save_state()
    mp.add_forced_key_binding(new_key, action.id, KEY_ACTION_FN[action.id],
        action.repeatable and { repeatable = true } or nil)
    osd(string.format(t('sc_rebind_osd'), t(action.label_key), new_key))
    refresh_menu()
end

-- mpv silently drops a binding whose key name it can't parse - and since the OLD binding
-- is removed first, garbage wouldn't just fail, it would kill the shortcut entirely (and
-- persist that way). Captured keys are trusted names, but saved state from older builds
-- gets sanitized through this on load.
local NAMED_KEYS = { tab = true, bs = true, del = true, ins = true, home = true, ['end'] = true,
    pgup = true, pgdwn = true, kp_enter = true, space = true, enter = true, esc = true,
    up = true, down = true, left = true, right = true }
local function key_is_valid(key)
    local base = key:lower()
    while true do
        local rest = base:match('^ctrl%+(.+)$') or base:match('^alt%+(.+)$')
            or base:match('^shift%+(.+)$') or base:match('^meta%+(.+)$')
        if rest then base = rest else break end
    end
    if base:match('^f%d%d?$') or NAMED_KEYS[base] then return true end
    local chars = 0
    for _ in base:gmatch('[%z\1-\127\194-\244][\128-\191]*') do chars = chars + 1 end
    return chars == 1
end

-- ---------- press-to-rebind key capture ----------
-- Clicking a pencil row closes the menu and enters capture mode: a centered overlay shows
-- the key live as it's held (combos arrive as one folded event, e.g. holding ctrl+alt and
-- pressing x is a single "ctrl+alt+x" - exactly the "press up to 3 keys together" behavior
-- wanted), then asks for an explicit Enter to confirm or Esc to cancel once released - a
-- crash, timeout, or Esc at any point before that leaves the old shortcut untouched.
-- Every candidate key is routed to ONE complex script-binding via a temporary forced input
-- section (same define-section mechanism console.lua uses for its own key grab), which,
-- being the most recently enabled forced section, outranks every other binding while active.
local CAPTURE_SECTION = 'echoplay-capture'
-- enter/esc are the capture UI's own confirm/cancel keys; esc cancels, enter confirms.
local RESERVED_INFO = { enter = 'sc_reserved_player' }
local capture_action, capture_timeout = nil, nil
local capture_stage, capture_pending = 'wait', nil -- 'wait' -> 'held' -> 'confirm'

local function capture_candidates()
    local bases = {}
    for c in ('abcdefghijklmnopqrstuvwxyz'):gmatch('.') do bases[#bases + 1] = c end
    for c in ('0123456789'):gmatch('.') do bases[#bases + 1] = c end
    for i = 1, 12 do bases[#bases + 1] = 'f' .. i end
    for _, k in ipairs({ 'tab', 'ins', 'del', 'home', 'end', 'pgup', 'pgdwn', 'space', 'enter',
        'up', 'down', 'left', 'right', 'ö', 'ç', 'ş', 'ğ', 'ü', 'ı',
        ',', '.', ';', '[', ']', '-', '=' }) do bases[#bases + 1] = k end
    local mods = { '', 'ctrl+', 'alt+', 'shift+', 'ctrl+alt+', 'ctrl+shift+', 'alt+shift+' }
    local keys = { 'esc' } -- bare esc = cancel; never offered with modifiers
    for _, base in ipairs(bases) do
        for _, mod in ipairs(mods) do
            -- shift+letter arrives as the uppercase letter itself, added below instead
            if not (mod:find('shift') and base:match('^%l$')) then keys[#keys + 1] = mod .. base end
        end
    end
    for c in ('abcdefghijklmnopqrstuvwxyz'):gmatch('.') do
        local u = c:upper()
        keys[#keys + 1] = u
        keys[#keys + 1] = 'ctrl+' .. u
        keys[#keys + 1] = 'alt+' .. u
        keys[#keys + 1] = 'ctrl+alt+' .. u
    end
    return keys
end

-- 'ctrl+K' -> 'Ctrl+K' for display only; stored/bound names stay exactly as delivered.
local function pretty_key(key)
    local MODS = { ctrl = 'Ctrl', alt = 'Alt', shift = 'Shift', meta = 'Meta' }
    return (key:gsub('([^+]+)', function(part) return MODS[part:lower()] or part end))
end
local function combo_size(key)
    local n = 0
    for _ in key:gmatch('[^+]+') do n = n + 1 end
    return n
end

local function capture_set_text(error_line)
    if not capture_action then return end
    local parts = {
        string.format(t('sc_capture_title'), t(capture_action.label_key)),
        string.format('{\\fs46\\b1}%s{\\b0}', capture_pending and pretty_key(capture_pending) or '...'),
        '{\\fs20}' .. (capture_stage == 'confirm' and t('sc_capture_confirm') or t('sc_capture_sub')),
    }
    if error_line then parts[#parts + 1] = '{\\fs20}' .. error_line end
    util.toast_show('rebind', parts)
end

local function capture_stop()
    if not capture_action then return end
    capture_action = nil
    capture_stage, capture_pending = 'wait', nil
    mp.commandv('disable-section', CAPTURE_SECTION)
    util.toast_hide('rebind')
    if capture_timeout then capture_timeout:kill(); capture_timeout = nil end
end

-- Complex script-binding handler: gets down/up events with the full folded key name
-- (holding ctrl+alt and pressing X arrives as ONE event named "ctrl+alt+x", so the live
-- display updates as the user builds the combo, modifiers already ordered first). The
-- rebind is applied ONLY on the final Enter confirm - a crash, timeout, or Esc anywhere
-- before that leaves the old shortcut untouched.
local function capture_handler(e)
    if not capture_action then return end
    local key, ev = e.key_name, e.event
    if not key or key == '' then return end
    if capture_timeout then capture_timeout:kill() end
    capture_timeout = mp.add_timeout(20, capture_stop)
    -- Named keys (ENTER, ESC, SPACE, ...) arrive in mpv's own canonical uppercase
    -- regardless of how they were spelled in define-section; plain letters keep
    -- pressed case (shift+x -> 'X', bare x -> 'x'). Compare case-insensitively.
    if ev == 'down' or ev == 'press' then
        if key:lower() == 'esc' then capture_stop(); return end
        if capture_stage == 'confirm' and key:lower() == 'enter' then
            local lower = capture_pending:lower()
            for _, other in ipairs(KEY_ACTIONS) do
                if other.id ~= capture_action.id and (custom_keys[other.id] or other.default_key):lower() == lower then
                    capture_stage, capture_pending = 'wait', nil
                    capture_set_text(string.format(t('sc_rebind_conflict'), pretty_key(lower), t(other.label_key)))
                    return
                end
            end
            local action, new_key = capture_action, capture_pending
            capture_stop()
            rebind_action(action, new_key)
            return
        end
        local reserved = RESERVED_INFO[key:lower()]
        if reserved then
            capture_stage, capture_pending = 'wait', nil
            capture_set_text(string.format(t('sc_rebind_conflict'), pretty_key(key), t(reserved)))
            return
        end
        if combo_size(key) > 3 then
            capture_stage, capture_pending = 'wait', nil
            capture_set_text(t('sc_capture_toobig'))
            return
        end
        capture_pending = key
        capture_stage = ev == 'press' and 'confirm' or 'held' -- 'press' = down+up in one event
        capture_set_text(nil)
    elseif ev == 'up' and capture_stage == 'held' then
        capture_stage = 'confirm'
        capture_set_text(nil)
    end
end
mp.add_key_binding(nil, 'echoplay-cap', capture_handler, { complex = true })

rebind_capture = function(action)
    capture_stop()
    capture_action = action
    capture_stage, capture_pending = 'wait', nil
    local lines = {}
    for _, key in ipairs(capture_candidates()) do
        lines[#lines + 1] = key .. ' script-binding ' .. SCRIPT .. '/echoplay-cap'
    end
    mp.commandv('define-section', CAPTURE_SECTION, table.concat(lines, '\n'), 'force')
    mp.commandv('enable-section', CAPTURE_SECTION)
    capture_set_text(nil)
    capture_timeout = mp.add_timeout(20, capture_stop)
end

for _, action in ipairs(KEY_ACTIONS) do
    -- Drop any invalid key that an older build let through, or the action stays dead forever.
    if custom_keys[action.id] and not key_is_valid(custom_keys[action.id]) then custom_keys[action.id] = nil end
    bind_action(action)
end

-- ---------- bindings ----------
mp.register_script_message('menu-event', function(json)
    local e = utils.parse_json(json)
    if e and e.type == 'activate' then handle(e.value, e.action) end
end)
mp.register_script_message('echoplay-menu', open_menu)
mp.register_script_message('echoplay-mixer', open_mixer)
mp.register_script_message('echoplay-toggle', function(id) handle(tonumber(id), nil) end)
mp.register_script_message('echoplay-all', function() handle('all', nil) end)
mp.register_script_message('echoplay-none', function() handle('none', nil) end)
mp.register_script_message('echoplay-speed', speed_toggle)
mp.register_script_message('echoplay-speed-down', function() speed_nudge(-speed_step) end)
mp.register_script_message('echoplay-speed-up', function() speed_nudge(speed_step) end)

-- file-loaded can fire before mpv has finished registering every track (observed live: on a
-- freshly opened file, track-list sometimes only contains the video track for the first tick
-- or two, especially while hwdec negotiation is still running - the audio track appears a
-- moment later). Scanning synchronously in file-loaded then found 0 audio tracks and
-- apply_audio() committed to aid=no permanently, since nothing re-ran afterward - the file
-- played back silent until the user manually opened the mixer. Guarded by
-- audio_defaults_applied (reset per file) so this only actually applies once real tracks
-- exist, and observe_property's own immediate first call covers the common case where
-- track-list is already complete by the time file-loaded runs.
local audio_defaults_applied = false
local function apply_audio_defaults()
    if audio_defaults_applied then return end
    scan()
    if #tracks == 0 then return end -- not ready yet; the track-list observer below will retry
    audio_defaults_applied = true
    enabled, gain, mono = {}, {}, {}
    if o.default_on == 'all' then
        for _, aid in ipairs(tracks) do enabled[aid] = true end
    elseif o.default_on ~= 'none' then
        for s in tostring(o.default_on):gmatch('%d+') do enabled[tonumber(s)] = true end
    end
    for k, v in pairs(saved_gains) do local a = tonumber(k); if a then gain[a] = v end end
    for k, v in pairs(saved_mono) do local a = tonumber(k); if a then mono[a] = v end end
    if o.default_on ~= 'none' and #on_list() == 0 and #tracks > 0 then enabled[tracks[1]] = true end
    apply_audio(); update_button()
end
mp.observe_property('track-list', 'native', apply_audio_defaults)

mp.register_event('start-file', function() mp.set_property('lavfi-complex', '') end)
mp.register_event('file-loaded', function()
    resume.dismiss()
    local saved = resume.check(mp.get_property('path'))
    if saved then resume.maybe_show(saved) end
    if not appearance_applied then appearance_applied = true; apply_appearance() end
    quality.apply_initial()
    apply_screenshot_preset() -- static presets are idempotent; 'video_folder' needs this per file
    if not menu_hint_shown then
        menu_hint_shown = true
        save_state()
        mp.add_timeout(1.5, function() osd(t('menu_hint')) end)
    end
    quality.reset_for_new_file()
    audio_defaults_applied = false
    apply_audio_defaults()
end)
