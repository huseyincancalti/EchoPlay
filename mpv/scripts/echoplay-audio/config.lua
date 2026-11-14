-- Update only the language option; never truncate the user's live config.
local mp = require 'mp'
local M = {}

function M.set_language(path, code)
    if type(code) ~= 'string' or not code:match('^%w+$') then
        return nil, 'Invalid language code'
    end
    local f, err, errno = io.open(path, 'rb')
    local content = ''
    local exists = f ~= nil
    if f then
        content, err = f:read('*a')
        local closed, close_err = f:close()
        if not content then return nil, err end
        if not closed then return nil, close_err end
    elseif errno ~= 2 then -- ENOENT only: never replace a file we could not read.
        return nil, err
    end

    local found = false
    local updated = content:gsub('([^\r\n]+)', function(line)
        local prefix, value = line:match('^([ \t]*language[ \t]*=[ \t]*)(.*)$')
        if not prefix then return line end
        found = true
        local comment = value:find('#', 1, true)
        local option = comment and value:sub(1, comment - 1) or value
        return prefix .. code .. (option:match('[ \t]*$') or '')
            .. (comment and value:sub(comment) or '')
    end)
    if not found then
        local newline = content:find('\r\n', 1, true) and '\r\n' or '\n'
        if updated ~= '' and not updated:match('[\r\n]$') then updated = updated .. newline end
        updated = updated .. 'language=' .. code .. newline
    end
    if updated == content then return true end

    -- Reserve a unique name, then stage alongside the destination (same filesystem).
    local named, seed = pcall(os.tmpname)
    if not named or not seed then return nil, 'Cannot create temporary filename: ' .. tostring(seed) end
    os.remove(seed)
    local temporary = path .. '.' .. seed:match('[^/\\]+$') .. '.tmp'
    local w
    w, err = io.open(temporary, 'wb')
    if not w then return nil, err end
    local written, write_err = w:write(updated)
    local closed, close_err = w:close()
    if not written or not closed then
        os.remove(temporary)
        return nil, write_err or close_err
    end

    local ok
    if mp.get_property_native('platform') == 'windows' then
        -- Windows Lua's os.rename cannot replace an existing destination. File.Replace
        -- preserves it on failure; do not delete it or fall back to an in-place write.
        local function quote(s) return "'" .. s:gsub("'", "''") .. "'" end
        local operation = exists and ('[System.IO.File]::Replace(%s,%s,[NullString]::Value)')
            or ('[System.IO.File]::Move(%s,%s)')
        local result = mp.command_native({ name = 'subprocess', playback_only = false,
            args = { 'powershell', '-NoProfile', '-NonInteractive', '-Command',
                "$ErrorActionPreference='Stop'; " .. operation:format(quote(temporary), quote(path)) },
            capture_stdout = true, capture_stderr = true })
        ok = result and result.status == 0
        err = result and (result.stderr or result.error_string) or 'Cannot start PowerShell'
    else
        ok, err = os.rename(temporary, path)
    end
    if not ok then
        os.remove(temporary)
        return nil, err or 'Cannot replace config file'
    end
    return true
end

return M
