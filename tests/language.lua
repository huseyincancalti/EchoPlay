-- Language persistence regressions, run by tests/run.lua with the existing mpv mocks.
return function(check)
    local mp = require 'mp'
    local utils = require 'mp.utils'
    local util = require 'util'
    local i18n = require 'i18n'
    local config = require 'config'
    print('language persistence')

    local original_read = util.read_json
    local translations = {
        en = { settings = 'Settings', lang_name = 'English' },
        tr = { settings = 'Ayarlar', lang_name = 'Turkish' },
    }
    util.read_json = function(path)
        return translations[path:match('echoplay%-(%w+)%.json$')]
    end
    check('initial language and displayed strings agree',
        i18n.init('opts', 'en') == 'en' and i18n.t('settings') == 'Settings')
    check('missing translation reports the actual fallback language',
        i18n.init('opts', 'missing') == 'tr' and i18n.t('settings') == 'Ayarlar')
    check('valid language changes immediately', i18n.set('en') and i18n.t('settings') == 'Settings')
    check('missing translation cannot silently change selection',
        not i18n.set('missing') and i18n.t('settings') == 'Settings')
    util.read_json = original_read

    -- Exercise file preservation and failures without platform-specific filesystem errors.
    local original_open, original_rename, original_remove, original_tmpname = io.open, os.rename, os.remove, os.tmpname
    local files, fail = {}, nil
    local path = 'opts/echoplay.conf'
    local original_platform = mp.native_props.platform
    mp.native_props.platform = 'linux'
    os.tmpname = function() return '/tmp/lua_language_test' end
    io.open = function(name, mode)
        local reading = mode:sub(1, 1) == 'r'
        if reading and fail == 'read-open' then return nil, 'permission denied', 13 end
        if reading and files[name] == nil then return nil, 'not found', 2 end
        if not reading and fail == 'write-open' then return nil, 'permission denied', 13 end
        local f = {}
        function f:read()
            if fail == 'read' then return nil, 'read failed' end
            return files[name]
        end
        function f:write(data)
            if fail == 'write' then return nil, 'disk full' end
            files[name] = data
            return self
        end
        function f:close()
            if not reading and fail == 'close' then return nil, 'flush failed' end
            return true
        end
        return f
    end
    os.rename = function(from, to)
        if fail == 'rename' then return nil, 'permission denied' end
        files[to], files[from] = files[from], nil
        return true
    end
    os.remove = function(name) files[name] = nil; return true end
    local function update(name, before, after)
        files, fail = { [path] = before }, nil
        local ok, err = config.set_language(path, 'en')
        check(name, ok and files[path] == after, err)
    end
    update('preserves CRLF, whitespace, comments and unrelated options',
        '# Language\r\n language = tr  # user comment\r\ngain_max=4\r\n',
        '# Language\r\n language = en  # user comment\r\ngain_max=4\r\n')
    update('updates first and duplicate active entries, leaving commented entries intact',
        'language=tr\n#language=tr\nlanguages=tr\nlanguage = tr',
        'language=en\n#language=tr\nlanguages=tr\nlanguage = en')
    update('appends absent option without losing unterminated last line',
        'gain_max=4', 'gain_max=4\nlanguage=en\n')
    update('appends using existing CRLF convention', '#language=tr\r\n',
        '#language=tr\r\nlanguage=en\r\n')
    update('creates a missing configuration file', nil, 'language=en\n')
    for _, failure in ipairs({ 'read-open', 'read', 'write-open', 'write', 'close', 'rename' }) do
        files, fail = { [path] = 'language=tr\nother=keep\n' }, failure
        local ok, err = config.set_language(path, 'en')
        check(failure .. ' failure is reported and leaves original untouched',
            not ok and type(err) == 'string' and files[path] == 'language=tr\nother=keep\n')
    end
    files, fail = { [path] = 'language=tr\n' }, nil
    check('rejects injected config lines', not config.set_language(path, 'en\nother=bad')
        and files[path] == 'language=tr\n')
    local mock_tmpname = os.tmpname
    os.tmpname = function() error('Cannot create temporary file') end
    local survived, result, failure = pcall(config.set_language, path, 'en')
    check('temporary filename failure is reported without modifying the original',
        survived and not result and type(failure) == 'string' and files[path] == 'language=tr\n')
    os.tmpname = mock_tmpname
    local original_native, request = mp.command_native, nil
    mp.native_props.platform = 'windows'
    mp.command_native = function(args) request = args; return { status = 0 } end
    path = "C:/user's opts/echoplay.conf"
    files = { [path] = 'language=tr\n' }
    local ok = config.set_language(path, 'en')
    local script = request and request.args[#request.args] or ''
    check('Windows replaces an existing file with literal escaped paths',
        ok and request.name == 'subprocess' and request.playback_only == false
            and script:find('[System.IO.File]::Replace', 1, true)
            and script:find("user''s opts", 1, true))
    files = {}
    ok = config.set_language(path, 'en')
    check('Windows creates a missing file using Move',
        ok and request.args[#request.args]:find('[System.IO.File]::Move', 1, true))
    files = { [path] = 'language=tr\n' }
    mp.command_native = function() return { status = 1, stderr = 'Access denied' } end
    local err
    ok, err = config.set_language(path, 'en')
    check('Windows replacement failure retains original config and reports error',
        not ok and err == 'Access denied' and files[path] == 'language=tr\n')
    mp.command_native = original_native
    io.open, os.rename, os.remove, os.tmpname = original_open, original_rename, original_remove, original_tmpname
    mp.native_props.platform = original_platform

    -- Load the actual main script with contradictory config/state values. Capturing menu
    -- data verifies both rendered text and the active radio, not just an internal variable.
    local original_options = package.loaded['mp.options']
    local original_read_options = _G.read_options
    local original_register = mp.register_script_message
    local original_format, original_parse, original_readdir = utils.format_json, utils.parse_json, utils.readdir
    local original_set_language = config.set_language
    local original_io_open = io.open
    local callbacks, menu, saved, persisted = {}, nil, nil, nil
    local state = { language = 'en' }
    package.loaded['mp.options'] = {}
    _G.read_options = function(o) o.language = 'tr' end
    util.read_json = function(name)
        if name:match('echoplay%-state%.json$') then return state end
        return translations[name:match('echoplay%-(%w+)%.json$')]
    end
    utils.readdir = function() return { 'echoplay-en.json', 'echoplay-tr.json' } end
    utils.format_json = function(data)
        if data.type == 'echoplay' then menu = data end
        if data._schema then saved = data end
        return '{}'
    end
    utils.parse_json = function(value) return { type = 'activate', value = value } end
    mp.register_script_message = function(name, fn) callbacks[name] = fn end
    config.set_language = function(_, code) persisted = code; return true end
    io.open = function(_, mode)
        if mode:sub(1, 1) == 'r' then return nil end
        return { write = function() return true end, close = function() return true end }
    end
    local function active_language(items)
        for _, item in ipairs(items or {}) do
            if item.active and item.value and item.value:match('^lang:') then return item.value end
            local nested = active_language(item.items)
            if nested then return nested end
        end
    end
    local loaded, err = pcall(dofile, 'mpv/scripts/echoplay-audio/main.lua')
    check('main script loads with saved language conflicting with config', loaded, err)
    if loaded then
        callbacks['echoplay-menu']()
        check('startup restores saved English strings and radio together',
            i18n.t('settings') == 'Settings' and active_language(menu.items) == 'lang:en')
        callbacks['menu-event']('lang:tr')
        check('selector applies, saves and mirrors new language',
            i18n.t('settings') == 'Ayarlar' and active_language(menu.items) == 'lang:tr'
                and saved.language == 'tr' and persisted == 'tr')
        state = saved
        loaded, err = pcall(dofile, 'mpv/scripts/echoplay-audio/main.lua')
        check('restart loads the chosen language', loaded and i18n.t('settings') == 'Ayarlar', err)
        callbacks['menu-event']('lang:missing')
        callbacks['echoplay-menu']()
        check('missing pack leaves displayed language and radio unchanged',
            i18n.t('settings') == 'Ayarlar' and active_language(menu.items) == 'lang:tr')
        local original_osd, original_warn = mp.osd_message, mp.msg.warn
        local warning, notification
        config.set_language = function() return nil, 'Access denied' end
        mp.msg.warn = function(text) warning = text end
        mp.osd_message = function(text) notification = text end
        callbacks['menu-event']('lang:en')
        check('config write failure is visible while saved state and UI agree',
            warning and notification and saved.language == 'en'
                and i18n.t('settings') == 'Settings' and active_language(menu.items) == 'lang:en')
        config.set_language = function(_, code) persisted = code; return true end
        for _, failure in ipairs({ 'open', 'write', 'close' }) do
            warning, notification, persisted = nil, nil, nil
            io.open = function(_, mode)
                if mode:sub(1, 1) == 'r' then return nil end
                if failure == 'open' then return nil, 'State open failed' end
                return {
                    write = function()
                        if failure == 'write' then return nil, 'State write failed' end
                        return true
                    end,
                    close = function()
                        if failure == 'close' then return nil, 'State close failed' end
                        return true
                    end,
                }
            end
            callbacks['menu-event']('lang:tr')
            check('state ' .. failure .. ' failure is reported while live language remains consistent',
                warning and warning:find('State ' .. failure .. ' failed', 1, true)
                    and notification and notification:find('state', 1, true)
                    and persisted == 'tr' and i18n.t('settings') == 'Ayarlar'
                    and active_language(menu.items) == 'lang:tr')
        end
        mp.osd_message, mp.msg.warn = original_osd, original_warn
    end
    package.loaded['mp.options'], _G.read_options = original_options, original_read_options
    mp.register_script_message = original_register
    utils.format_json, utils.parse_json, utils.readdir = original_format, original_parse, original_readdir
    util.read_json, config.set_language, io.open = original_read, original_set_language, original_io_open
end
