-- Turkish fallback strings + the active-language lookup. Community translations are
-- script-opts/echoplay-<code>.json files; any key missing from one falls back to this table.
local utils = require 'mp.utils'
local util = require 'util'

local M = {}

local FALLBACK = {
    title = 'EchoPlay', settings = 'Ayarlar',
    sec_audio = 'Ses Kaynakları', sec_video = 'Video Ayarları', sec_quality = 'Görüntü Kalitesi',
    sec_appearance = 'Görünüm', sec_file = 'Dosya', sec_general = 'Genel',
    mixer = 'Ses Kaynakları', back = 'Geri', tracks_on = '%d/%d açık',
    all_on = 'Hepsini aç', mute = 'Sustur',
    gain_down = 'Sesi azalt', gain_up = 'Sesi artır', silent = 'Sessiz',
    mono = 'Mono yap (L+R birleştir)', mono_short = 'Mono',
    mono_off = "Stereo'ya döndür", stereo_short = 'Stereo',
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

    -- Performance mode (Auto + Ultra/Normal/Low picture-quality ladder)
    perf_mode = 'Performans Modu', perf_auto = 'Otomatik',
    perf_tier_ultra = 'Ultra', perf_tier_normal = 'Normal', perf_tier_low = 'Düşük',
    perf_auto_hint = 'donanıma göre seçer',
    perf_ultra_hint = 'sıfır kalite kaybı', perf_normal_hint = 'dengeli (önerilen)',
    perf_low_hint = 'zayıf donanımlar için',
    perf_manual_auto = 'Otomatik mod: şu an %s', perf_manual_forced = 'Görüntü kalitesi: %s',
    perf_auto_step_down = 'Takılma tespit edildi, kaliteyi düşürüyorum: %s',
    perf_auto_step_up = 'Performans iyi, kaliteyi artırıyorum: %s',

    -- Keyboard shortcuts (pencil rows are rebindable: click, then PRESS the new key/combo)
    shortcuts = 'Klavye Kısayolları', sc_rclick = 'sağ tık',
    sc_rebind_hint = 'Kalemli satıra tıkla, sonra atamak istediğin tuşa bas',
    sc_rebind_osd = '%s artık: %s',
    sc_rebind_conflict = "'%s' zaten kullanımda: %s",
    sc_capture_title = "'%s' için yeni tuş",
    sc_capture_sub = 'Tuşa veya kombinasyona bas (en fazla 3 tuş)  ·  Esc: vazgeç',
    sc_capture_confirm = '✓ Enter: Onayla      ✗ Esc: İptal',
    sc_capture_toobig = 'En fazla 3 tuş birleştirilebilir',
    sc_pause = 'Duraklat / oynat', sc_reserved_player = 'oynatıcı kısayolu',
    sc_menu = 'Ayarlar menüsünü aç', sc_speed = 'Sabit hızı aç/kapat', sc_mute = 'Sesi kapat/aç',
    sc_vol_up = 'Sesi artır', sc_vol_down = 'Sesi azalt',
    sc_seek_fwd = '10 saniye ileri sar', sc_seek_back = '10 saniye geri sar',
    sc_track1 = '1. ses parçasını aç/kapat', sc_track2 = '2. ses parçasını aç/kapat',
    sc_track3 = '3. ses parçasını aç/kapat',
    sc_speed_down = 'Videoyu yavaşlat',
    sc_speed_up = 'Videoyu hızlandır',
    sc_screenshot = 'Ekran görüntüsü al (altyazı dahil)',
    sc_screenshot_video = 'Ekran görüntüsü al (yalnızca video)',
    sc_screenshot_window = 'Ekran görüntüsü al (tüm pencere)',

    -- Screenshot location
    screenshot_location = 'Ekran Görüntüsü Konumu', screenshot_desktop = 'Masaüstü',
    screenshot_videos = 'Videolar', screenshot_pictures = 'Resimler',
    screenshot_video_folder = 'Video ile aynı klasör', screenshot_custom = 'Klasör Seç...',
    screenshot_osd = 'Ekran görüntüsü klasörü: %s',

    -- Fine speed control (s/d keys)
    speed_fine = 'Video Hızı', speed_step_label = 'Hız Adımı',

    -- Discoverability
    menu_hint = 'İpucu: Ayarlar için sağ tıklayın', sec_help = 'Yardım',
    silent_warning = 'Ses kapalı - Ayarlar > Ses Kaynakları\'ndan açabilirsiniz',

    -- Resume prompt: position is remembered, but playback always starts from 0 and asks first.
    resume_prompt = "%s'ten devam edilsin mi?", resume_keys = 'Enter: Evet  ·  Esc: Baştan başla',

    -- Update check
    update_available = 'Yeni EchoPlay sürümü çıktı: %s',
    update_available_menu = 'Güncelleme mevcut: %s',
}

local opts_dir, STR

function M.load(code)
    return util.read_json(opts_dir .. '/echoplay-' .. code .. '.json')
end

function M.set(code)
    STR = M.load(code) or STR
end

function M.t(k) return STR[k] or FALLBACK[k] or k end

function M.available()
    local langs, files = {}, utils.readdir(opts_dir, 'files') or {}
    for _, fn in ipairs(files) do
        local code = fn:match('^echoplay%-(%w+)%.json$')
        if code then
            local data = M.load(code)
            langs[#langs + 1] = { code = code, name = (data and data.lang_name) or code:upper() }
        end
    end
    return langs
end

function M.init(dir, initial_lang)
    opts_dir = dir
    STR = M.load(initial_lang) or M.load('tr') or {}
end

return M
