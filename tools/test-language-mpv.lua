-- Run from the repository root with mpv --no-config --idle=yes --vo=null --ao=null
-- --script=tools/test-language-mpv.lua. Uses mpv's Lua runtime when Lua is not installed.
local real_mp = require 'mp'
local real_utils = require 'mp.utils'
local original_exit = os.exit
local exit_code = 1
os.exit = function(code) exit_code = code end
package.loaded['mp'], package.loaded['mp.utils'] = nil, nil
local ok, err = pcall(dofile, 'tests/run.lua')
os.exit = original_exit
if not ok then print(err); exit_code = 1 end
package.loaded['mp'], package.loaded['mp.utils'] = real_mp, real_utils
package.loaded.config = nil

-- Exercise the real OS replacement path as well as the mocked failure cases.
if exit_code == 0 then
    local config = require 'config'
    local path = real_mp.command_native({ 'expand-path', '.out/language-test-quote\'.conf' })
    path = real_utils.join_path(real_utils.getcwd(), path)
    local function verify()
        local f = assert(io.open(path, 'wb'))
        assert(f:write('# keep\r\nlanguage=tr\r\ngain_max=4\r\n')); assert(f:close())
        assert(config.set_language(path, 'en'))
        f = assert(io.open(path, 'rb'))
        assert(f:read('*a') == '# keep\r\nlanguage=en\r\ngain_max=4\r\n'); assert(f:close())
        assert(os.remove(path))
        assert(config.set_language(path, 'tr'))
        f = assert(io.open(path, 'rb'))
        assert(f:read('*a') == 'language=tr\n'); assert(f:close())
    end
    ok, err = pcall(verify)
    os.remove(path)
    if not ok then print(err); exit_code = 1
    else print('Real filesystem language replacement and creation passed.') end
end
print('Language test exit code: ' .. exit_code)
real_mp.commandv('quit', tostring(exit_code))
