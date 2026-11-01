.pragma library

// 84 -> "1:24", 3725 -> "1:02:05".
function time(seconds) {
    const total = Math.max(0, Math.floor(seconds || 0))
    const h = Math.floor(total / 3600), m = Math.floor(total / 60) % 60, s = total % 60
    const ss = (s < 10 ? "0" : "") + s
    return h > 0 ? h + ":" + (m < 10 ? "0" : "") + m + ":" + ss : m + ":" + ss
}

// A track's name in the UI; a track without one is "Track N".
function trackName(track, strings) {
    return track.name || strings.track_n.arg(track.id)
}

// The first word of a track's name, with Microphone shortened: "System sounds" + "Microphone" = "System + Mic".
function shortName(track, strings) {
    const word = trackName(track, strings).split(" ")[0]
    return word === strings.microphone ? strings.mic : word
}

// The mixer chip: none = "No sound", one = its name, two = short names, more = "All N tracks" or "2 of 3".
function chip(tracks, strings) {
    const on = tracks.filter(t => t.enabled)
    if (on.length === 0) return strings.no_sound
    if (on.length === 1) return trackName(on[0], strings)
    if (on.length === 2 && tracks.length === 2) return shortName(on[0], strings) + " + " + shortName(on[1], strings)
    if (on.length === tracks.length) return strings.all_tracks.arg(tracks.length)
    return strings.n_of_m.arg(on.length).arg(tracks.length)
}
