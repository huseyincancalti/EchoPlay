-- Bench probe for the v1 engine path (plain mpv window), loaded by tools/bench.sh and
-- tools/shot-compare.sh, never shipped. Script options (--script-opts=):
--   bench-seconds=N   log the same BENCH line the v2 spike logs after N seconds, then quit
--   mix-toggle=yes    switch between the 1+2 mix and track 1 alone every 2 s (v1's apply_audio order)
--   shot-out=PATH     on the first playback restart, save a window screenshot to PATH, then quit
local utils = require 'mp.utils'

local seconds = tonumber(mp.get_opt('bench-seconds'))
local shot_out = mp.get_opt('shot-out')
local mix_graph = '[aid1][aid2]amix=inputs=2:normalize=0[ao]'
local keys = { 'mpv-version', 'current-vo', 'hwdec-current', 'width', 'height', 'display-fps',
    'estimated-vf-fps', 'playback-time', 'frame-drop-count', 'decoder-frame-drop-count',
    'vo-delayed-frame-count' }

-- Running total: mpv resets frame-drop-count whenever the audio chain is rebuilt (mixer switch).
local last_drops, total_drops = 0, 0
mp.observe_property('frame-drop-count', 'number', function(_, n)
    n = n or 0
    total_drops = total_drops + (n >= last_drops and n - last_drops or n)
    last_drops = n
end)

local restarted = false
mp.register_event('playback-restart', function()
    if restarted then return end
    restarted = true
    if shot_out then
        mp.add_timeout(0.5, function()
            mp.commandv('screenshot-to-file', shot_out, 'window')
            mp.command('quit')
        end)
    end
    if mp.get_opt('mix-toggle') == 'yes' then
        local on = mp.get_property('lavfi-complex', '') ~= ''
        mp.add_periodic_timer(2, function()
            on = not on
            mp.set_property('lavfi-complex', on and mix_graph or '')
            mp.set_property('aid', on and 'no' or '1')
        end)
    end
end)

if seconds then
    mp.add_timeout(seconds, function()
        local r = { glass = false, mix = mp.get_property('lavfi-complex', '') ~= '', ['frame-drop-total'] = total_drops }
        for _, k in ipairs(keys) do r[k] = mp.get_property_native(k) end
        mp.msg.info('BENCH ' .. utils.format_json(r))
        mp.command('quit')
    end)
end
