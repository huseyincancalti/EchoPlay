-- EchoPlay: one searchable, fully localized Settings menu (right-click / `a` / button).
--   * Audio mixer lives in its own collapsed submenu (toggle + gain + per-track Mono),
--     so a 9-track file doesn't dump everything on screen. Mixing is live (lavfi-complex).
--   * Sectioned settings (Playback / Appearance / File) with headings + dividers.
--   * Appearance = Dark/Light mode + an accent color, computed in-script (clean live
--     switching). Community theme packs (themes/*.conf) still appear as advanced presets.
--   * Languages are community-extensible: drop a script-opts/echoplay-<code>.json.
-- Mixer state / mono / language / accent persist across runs.
-- Track names come from file metadata (e.g. ShadowPlay "System sounds"/"Microphone").

local mp = require 'mp'
local utils = require 'mp.utils'
require 'mp.options'

local o = {
    language = 'tr',
    default_on = '1',
    mix_normalize = 0,
    gain_step = 0.25,
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

-- ---------- appearance: built-in Dark/Light modes + accent ----------
-- Accent swatches shown in the Theme submenu. Add more freely.
local ACCENTS = {
    { key = 'accent_orange', hex = 'ee7733' },
    { key = 'accent_blue',   hex = '4aa3ff' },
    { key = 'accent_green',  hex = '7ec699' },
    { key = 'accent_purple', hex = 'a78bfa' },
    { key = 'accent_pink',   hex = 'ef7fae' },
    { key = 'accent_red',    hex = 'e0707a' },
}
local mode = 'dark'        -- 'dark' | 'light'
local accent = 'ee7733'

-- Full color override (latest append wins, so switching modes resets cleanly in-session).
local function color_string()
    if mode == 'light' then
        return ('foreground=%s,foreground_text=000000,background=f2f2f2,background_text=141414,curtain=cfcfcf'):format(accent)
    end
    return ('foreground=%s,foreground_text=000000,background=000000,background_text=ffffff,curtain=111111'):format(accent)
end
local function accent_name(hex)
    for _, a in ipairs(ACCENTS) do if a.hex == hex then return a.key end end
    return nil
end

-- ---------- state persistence ----------
local saved_gains, saved_mono = {}, {}
local function read_json(path)
    local f = io.open(path, 'r'); if not f then return nil end
    local d = f:read('*a'); f:close()
    local p = utils.parse_json(d or ''); return type(p) == 'table' and p or nil
end
local function load_state()
    local s = read_json(STATE_PATH); if not s then return end
    if s.default_on then o.default_on = s.default_on end
    if s.language then o.language = s.language end
    if s.mode == 'light' or s.mode == 'dark' then mode = s.mode
    elseif s.theme == 'light' or s.theme == 'dark' then mode = s.theme end   -- pre-1.0 field
    if type(s.accent) == 'string' and s.accent ~= '' then accent = s.accent end
    if tonumber(s.speed_factor) then o.speed_factor = tonumber(s.speed_factor) end
    if type(s.gains) == 'table' then saved_gains = s.gains end
    if type(s.mono) == 'table' then saved_mono = s.mono end
end
load_state()

-- ---------- i18n ----------
local FALLBACK = {
    title = 'EchoPlay', settings = 'Ayarlar',
    sec_playback = 'Oynatma', sec_appearance = 'Görünüm', sec_file = 'Dosya',
    mixer = 'Ses Karıştırıcı', back = 'Geri',
    all_on = 'Hepsini aç', mute = 'Sustur',
    gain_down = 'Sesi azalt', gain_up = 'Sesi artır', silent = 'Sessiz',
    mono = 'Mono yap (L+R birleştir)', mono_short = 'Mono',
    speed = 'Sabit Hız (g)', speed_osd = 'Hız: %sx', faster = 'Hızlandır', slower = 'Yavaşlat',
    video_end = 'Video Bitince', end_next = 'Sıradakine geç', end_loop = 'Tekrarla', end_stop = 'Sonda dur',
    end_osd = 'Video bitince: %s',
    theme = 'Tema', theme_mode = 'Mod', theme_dark = 'Koyu', theme_light = 'Açık', theme_osd = 'Tema: %s',
    accent = 'Vurgu Rengi', accent_osd = 'Vurgu: %s', custom_themes = 'Özel Temalar',
    accent_orange = 'Turuncu', accent_blue = 'Mavi', accent_green = 'Yeşil',
    accent_purple = 'Mor', accent_pink = 'Pembe', accent_red = 'Kırmızı',
    language = 'Dil', lang_osd = 'Dil: %s', open_lang = 'Dil klasörünü aç (çeviri ekle)',
    open_theme = 'Tema klasörünü aç (tema ekle)', lang_name = 'Türkçe',
    open_file = 'Dosya Aç', playlist = 'Oynatma Listesi', subtitles = 'Altyazı',
    audio_device = 'Ses Cihazı', config_dir = 'Program Klasörü', single_track = 'Bu videoda tek ses parçası var',
}
local function load_lang(code)
    return read_json(OPTS_DIR .. '/echoplay-' .. code .. '.json')
end
local STR = load_lang(o.language) or load_lang('tr') or {}
local function t(k) return STR[k] or FALLBACK[k] or k end

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
local function pct(aid) return math.floor((gain[aid] or 1) * 100 + 0.5) end
local function hint(aid)
    local parts = { pct(aid) .. '%' }
    if mono[aid] then parts[#parts + 1] = t('mono_short') end
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
local function cur_end()
    if mp.get_property('loop-file') == 'inf' then return 'loop' end
    if mp.get_property('keep-open') == 'yes' then return 'stop' end
    return 'next'
end
local function speed_on() return (mp.get_property_number('speed') or 1) > 1.01 end

local function available_langs()
    local langs, files = {}, utils.readdir(OPTS_DIR, 'files') or {}
    for _, fn in ipairs(files) do
        local code = fn:match('^echoplay%-(%w+)%.json$')
        if code then
            local data = load_lang(code)
            langs[#langs + 1] = { code = code, name = (data and data.lang_name) or code:upper() }
        end
    end
    return langs
end
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
        for line in f:lines() do
            local h = line:match('^#%s*(.+)$')
            f:close()
            return h or (name:gsub('^%l', string.upper))
        end
        f:close()
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
        f:write(utils.format_json({ default_on = don, gains = gains, mono = monos,
            language = o.language, mode = mode, accent = accent, speed_factor = o.speed_factor }))
        f:close()
    end
end

-- ---------- menus ----------
local function heading(text) return { title = text, selectable = false, muted = true, bold = true } end
local function separator() return { title = '', separator = true, selectable = false } end

local function theme_items()
    local items = {}
    items[#items + 1] = heading(t('theme_mode'))
    items[#items + 1] = { title = t('theme_dark'), icon = RADIO[mode == 'dark'], value = 'mode:dark', keep_open = true }
    items[#items + 1] = { title = t('theme_light'), icon = RADIO[mode == 'light'], value = 'mode:light', keep_open = true }
    items[#items + 1] = separator()
    items[#items + 1] = heading(t('accent'))
    for _, a in ipairs(ACCENTS) do
        items[#items + 1] = { title = t(a.key), icon = RADIO[accent == a.hex], value = 'accent:' .. a.hex, keep_open = true }
    end
    local customs = available_themes()
    if #customs > 0 then
        items[#items + 1] = separator()
        items[#items + 1] = heading(t('custom_themes'))
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
        items[#items + 1] = { title = l.name, icon = RADIO[o.language == l.code], value = 'lang:' .. l.code }
    end
    items[#items + 1] = separator()
    items[#items + 1] = { title = t('open_lang'), icon = 'folder_open', value = 'lang-folder' }
    return items
end

-- Main settings menu: grouped into Playback / Appearance / File.
local function menu_data()
    local items = {}
    -- Playback
    items[#items + 1] = heading(t('sec_playback'))
    items[#items + 1] = { title = t('mixer'), icon = 'graphic_eq',
        hint = #tracks > 0 and (#on_list() .. '/' .. #tracks) or nil, value = 'open-mixer' }
    items[#items + 1] = { title = string.format('%s: %.2gx', t('speed'), o.speed_factor),
        icon = CHECK[speed_on()], value = 'speed', keep_open = true,
        actions = { { name = 'down', icon = 'remove', label = t('slower') }, { name = 'up', icon = 'add', label = t('faster') } } }
    local e = cur_end()
    items[#items + 1] = { title = t('video_end'), icon = 'restart_alt', items = {
        { title = t('end_next'), icon = RADIO[e == 'next'], value = 'end:next' },
        { title = t('end_loop'), icon = RADIO[e == 'loop'], value = 'end:loop' },
        { title = t('end_stop'), icon = RADIO[e == 'stop'], value = 'end:stop' },
    } }
    -- Appearance
    items[#items + 1] = separator()
    items[#items + 1] = heading(t('sec_appearance'))
    items[#items + 1] = { title = t('theme'), icon = 'palette', items = theme_items() }
    items[#items + 1] = { title = t('language'), icon = 'translate', items = lang_items() }
    -- File
    items[#items + 1] = separator()
    items[#items + 1] = heading(t('sec_file'))
    items[#items + 1] = { title = t('open_file'), icon = 'folder_open', value = 'open-file' }
    items[#items + 1] = { title = t('playlist'), icon = 'list', value = 'playlist' }
    items[#items + 1] = { title = t('subtitles'), icon = 'subtitles', value = 'subtitles' }
    items[#items + 1] = { title = t('audio_device'), icon = 'speaker', value = 'audio-device' }
    items[#items + 1] = { title = t('config_dir'), icon = 'settings', value = 'config-dir' }
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
        items[#items + 1] = { title = label(aid), hint = hint(aid), icon = CHECK[enabled[aid] == true],
            value = aid, actions = actions, keep_open = true }
    end
    if #tracks == 0 then items[#items + 1] = { title = t('single_track'), selectable = false, muted = true } end
    items[#items + 1] = separator()
    items[#items + 1] = { title = t('all_on'), icon = 'select_all', value = 'all', keep_open = true }
    items[#items + 1] = { title = t('mute'), icon = 'volume_off', value = 'none', keep_open = true }
    items[#items + 1] = separator()
    items[#items + 1] = { title = t('back'), icon = 'arrow_back', value = 'back' }
    return { type = MENU_TYPE_MIX, title = t('mixer'), search_style = 'palette',
        callback = { SCRIPT, 'menu-event' }, items = items }
end

local function open_menu() mp.commandv('script-message-to', 'uosc', 'open-menu', utils.format_json(menu_data())) end
local function refresh_menu() mp.commandv('script-message-to', 'uosc', 'update-menu', utils.format_json(menu_data())) end
local function close_menu() mp.commandv('script-message-to', 'uosc', 'close-menu', MENU_TYPE) end
local function open_mixer() mp.commandv('script-message-to', 'uosc', 'open-menu', utils.format_json(mixer_data())) end
local function refresh_mixer() mp.commandv('script-message-to', 'uosc', 'update-menu', utils.format_json(mixer_data())) end

local function update_button()
    local n = #on_list()
    mp.commandv('script-message-to', 'uosc', 'set-button', 'echoplay', utils.format_json({
        icon = 'tune', badge = #tracks > 1 and tostring(n) or nil,
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

local function set_end(mode_)
    if mode_ == 'loop' then mp.set_property('loop-file', 'inf'); mp.set_property('keep-open', 'no')
    elseif mode_ == 'stop' then mp.set_property('loop-file', 'no'); mp.set_property('keep-open', 'yes')
    else mp.set_property('loop-file', 'no'); mp.set_property('keep-open', 'no') end
    osd(string.format(t('end_osd'), t('end_' .. mode_))); close_menu()
end

local function apply_appearance()
    mp.commandv('change-list', 'script-opts', 'append', 'uosc-color=' .. color_string())
end
local function set_mode(m)
    mode = m; apply_appearance(); save_state()
    osd(string.format(t('theme_osd'), m == 'light' and t('theme_light') or t('theme_dark')))
end
local function set_accent(hex)
    accent = hex; apply_appearance(); save_state()
    local key = accent_name(hex)
    osd(string.format(t('accent_osd'), key and t(key) or hex))
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
    STR = load_lang(code) or STR
    mp.commandv('change-list', 'script-opts', 'append', 'uosc-languages=' .. code .. ',slang,en')
    persist_uosc_language(code)
    save_state(); osd(string.format(t('lang_osd'), t('lang_name')))
    update_button(); open_menu()
end

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
    elseif type(value) == 'string' and value:sub(1, 4) == 'end:' then set_end(value:sub(5))
    elseif type(value) == 'string' and value:sub(1, 5) == 'mode:' then set_mode(value:sub(6))
    elseif type(value) == 'string' and value:sub(1, 7) == 'accent:' then set_accent(value:sub(8))
    elseif type(value) == 'string' and value:sub(1, 6) == 'theme:' then apply_theme_pack(value:sub(7))
    elseif type(value) == 'string' and value:sub(1, 5) == 'lang:' then set_language(value:sub(6))
    elseif value == 'lang-folder' then close_menu(); mp.commandv('run', 'explorer', OPTS_DIR)
    elseif value == 'theme-folder' then close_menu(); mp.commandv('run', 'explorer', THEMES_DIR)
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

mp.register_event('start-file', function() mp.set_property('lavfi-complex', '') end)
mp.register_event('file-loaded', function()
    if not appearance_applied then appearance_applied = true; apply_appearance() end
    scan()
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
end)
