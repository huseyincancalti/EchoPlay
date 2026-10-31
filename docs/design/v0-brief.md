# EchoPlay v2: UI/UX design brief for v0

The maintainer runs the prompts at the bottom in v0, one by one, in the same chat. The output is a visual
reference: the real app is built in Qt Quick/QML, so every screen must stay within the constraints below.

## 1. Product
EchoPlay is a desktop video player for game recordings that carry two or more audio tracks: NVIDIA
ShadowPlay / NVIDIA App and OBS Studio save the game sound and the microphone as separate tracks. Normal
players play one track at a time. EchoPlay plays any mix of them live, with a volume per track, without
reopening the video. Users: gamers and streamers reviewing their own clips, usually fullscreen, often
dozens of clips from one folder in a row.

The design must answer "why this instead of VLC?" at first sight: the track mixer is the hero, not a
setting buried in a menu.

## 2. Direction
- Neo-minimal glass: the video is the page. Controls float on frosted glass panels that blur the live
  video behind them; everything else disappears after 1 s of no mouse movement.
- YouTube-easy: anyone who has used YouTube knows where everything is. Bottom bar: play/pause, skip ±10 s,
  volume with a horizontal slider, time; right side: mixer chip, subtitles, settings gear, fullscreen.
- Popovers open directly above the button that opened them, with a small pointer. The video never pauses
  because a menu opened. A click outside or on the button closes them.
- Calm, not flashy: one accent color, generous spacing, no gradients on controls, no neon, no glow.
- Avoid: default shadcn look, purple-blue gradients, emoji, cards inside cards, tiny 11 px text.

## 3. Build constraints (must be possible in Qt Quick)
- Effects allowed: blur of what is behind a panel (one blur strength per panel), opacity, solid fills,
  rounded rectangles, 1 px borders, one soft drop shadow. Not allowed: mix-blend-mode, clip-path shapes,
  hue/saturate backdrop filters, gradient text, noise textures, CSS-only tricks.
- Icons: Material Symbols Rounded only (outlined, filled for active states).
- Font: one OFL family that we bundle (for example Geist, Manrope or Plus Jakarta Sans). Pick one.
- Window: borderless with our own title bar (title, minimize, maximize, close). Default 1280x720,
  minimum 640x360. Show the player screens at 1280x720 and once in fullscreen 1920x1080.
- Every color, radius, blur, spacing and font size comes from one `theme` object at the top of the file,
  nothing hard-coded in components. That object becomes EchoPlay's single theme file later.
- Every visible string comes from a `strings` object with `en` and `tr`, plus an EN/TR toggle in the
  corner of the page. That object becomes EchoPlay's one-file-per-language packs later.
- Text on glass stays readable over a bright frame (snow, sky): show every glass panel once over a very
  bright frame and once over a dark one. Target 4.5:1 contrast for body text.
- Keyboard focus is always visible. Nothing essential is hover-only.

## 4. Brand and tokens
- Logo: white sound-wave mark on an orange tile. Default accent: orange `#FF7D26`.
- Dark mode is the default; light mode and "follow system" also exist. Accent swatches: orange, red, pink,
  violet, blue, green.
- Deliver the `theme` object with at least: `bg`, `glass` (fill + opacity), `glassBorder`, `blur.panel`,
  `text`, `textMuted`, `accent`, `onAccent`, `success`, `warning`, `danger`, radius scale (sm, md, lg, xl),
  spacing scale, type scale (caption, body, title, display), icon sizes, motion durations (fast, base).

## 5. Screens and states
Use realistic data: file `Battlefield 6 2025.11.30 - 23.19.01.11.DVR.mp4`, 4:36 long, at 1:24; tracks
named "System sounds" and "Microphone" (the names ShadowPlay writes into the file); a third clip with three
tracks ("Game", "Discord", "Microphone").

1. **Player, playing**: title bar, bottom glass bar, the mixer chip shows which tracks are on
   ("System + Mic"). Seek bar hover shows a frame preview with its time.
2. **Player, idle**: after 1 s, everything hides except a 2 px progress line at the bottom.
3. **Mixer popover** (the hero): one row per track: on/off switch, track name, gain slider 0-400 %
   with value, Mono/Stereo toggle (fixes a mic recorded on one ear). Footer: "All on", "Mute all".
   A warning row when a track is silent ("Microphone is silent in this clip").
4. **Quick settings popover** (gear): speed (- 1.00x +), picture quality (Auto, Ultra, Normal, Low),
   when the video ends (play next, loop, stop), subtitles, "All settings".
5. **Settings overlay**: a large glass panel over the blurred, still-playing video. Left: category tabs
   with icons. Right: rows with title, one-line description and a control (switch, slider, segmented
   control, swatches, dropdown). Search field on top. Categories and content:
   - Audio: mixer defaults, loudness boost (on/off + strength), audio device, max volume (up to 300 %).
   - Playback: speed step, fixed-speed key (1.25x-4x), when video ends, resume behaviour.
   - Picture: quality mode (Auto + 3 tiers, with a one-line "who is this for" each).
   - Appearance: Dark / Light / System, accent swatches, glass strength, "Open theme file".
   - Language: list of installed languages, "Open languages folder" (a language = one file).
   - Screenshots: save location (Desktop, Videos, Pictures, next to the video, custom folder).
   - Shortcuts: list of actions with their key; click a key to rebind; reset to defaults.
   - About: logo, "EchoPlay 2.0.0", "Publisher: Hüseyin Can ÇALTI", update status ("You're up to date",
     last checked time, "Check now"), engine "mpv 0.41", source link, licenses, and a privacy line:
     "Checks GitHub once a day for a new version. Sends nothing else."
6. **Start screen** (EchoPlay opened without a file): drop zone with "Open file" and "Open folder",
   "Continue watching" cards (thumbnail, progress bar, date, track-count chip), recent folders.
7. **Playlist panel**: side panel listing the other clips in the same folder, current one highlighted.
8. **Small states**: resume toast ("Resume from 1:24?" with Resume / Start over), update-available card
   ("EchoPlay 2.1.0 is ready" with Update / Later), volume and seek flash, drag-over state, error state
   ("This file can't be opened" with the reason), first-run hint pointing at the mixer chip.

Optional, mark as optional so the maintainer can decide: equalizer presets, video colour adjustments
(brightness, contrast, saturation), subtitle position and size, mini player, sleep timer, A-B loop.

## 6. What the maintainer checks in the result
- Is the mixer the first thing you notice? Can you change the mic volume in two clicks?
- Does it look like nothing else on the market while staying YouTube-easy?
- Is text readable on the bright frame?
- Do all colors come from the `theme` object and all text from `strings`?

## 7. Prompts to paste into v0 (in order, same chat)

**Prompt 1: design system and player**
```
Design a desktop video player called EchoPlay as a single React page showing static screens side by side
(no real video; use full-bleed sample images as the video frame). Read these rules first:
- Neo-minimal glassmorphism: controls float on frosted glass panels that blur the video behind them.
  Calm, one accent color (#FF7D26 orange), dark mode default. No gradients on controls, no glow, no emoji,
  no default shadcn look.
- Must be rebuildable in Qt Quick: allowed effects are backdrop blur (one strength per panel), opacity,
  solid fills, rounded rectangles, 1px borders, one soft drop shadow. No mix-blend-mode, clip-path,
  gradient text or noise.
- Icons: Material Symbols Rounded only. One bundled OFL font (choose Geist, Manrope or Plus Jakarta Sans).
- All colors, radii, blur, spacing, font sizes and durations come from one `theme` object at the top of the
  file. All visible text comes from a `strings` object with `en` and `tr`; add an EN/TR toggle and a
  dark/light toggle in the page corner.
- Borderless window with a custom title bar (title, minimize, maximize, close), 1280x720.
Screens for this step:
1) Player while playing: file "Battlefield 6 2025.11.30 - 23.19.01.11.DVR.mp4", 4:36 long, at 1:24.
   Bottom glass bar like YouTube: play/pause, -10s, +10s, volume icon + horizontal slider, "1:24 / 4:36";
   right side: a mixer chip reading "System + Mic", subtitles, settings gear, fullscreen. Seek bar hover
   shows a frame preview with its time.
2) Player idle: everything hidden except a 2px progress line at the bottom.
3) The same playing screen over a very bright frame (snow) to prove text stays readable (4.5:1).
```

**Prompt 2: the mixer and quick settings popovers**
```
Add two popovers that open directly above the button that opened them, with a small pointer, over the
still-playing video:
1) Mixer popover (the product's main feature, make it feel special): one row per audio track with an
   on/off switch, the track name ("System sounds", "Microphone"), a gain slider 0-400% with its value,
   and a Mono/Stereo toggle. Footer buttons "All on" and "Mute all". Also show a variant for a clip with
   three tracks ("Game", "Discord", "Microphone") and a variant with a warning row
   "Microphone is silent in this clip".
2) Quick settings popover from the gear: speed (- 1.00x +), picture quality (Auto, Ultra, Normal, Low),
   when the video ends (Play next, Loop, Stop), Subtitles, and "All settings".
Keep using only the theme and strings objects.
```

**Prompt 3: settings overlay**
```
Add the full settings overlay: a large glass panel centered over the blurred, still-playing video.
Left: category tabs with icons (Audio, Playback, Picture, Appearance, Language, Screenshots, Shortcuts,
About) and a search field above them. Right: rows with a title, a one-line description and a control
(switch, slider, segmented control, color swatches, dropdown). Show three tabs filled in:
- Audio: loudness boost (switch + strength slider), audio device (dropdown), maximum volume (100-300%).
- Appearance: Dark / Light / System, accent swatches (orange, red, pink, violet, blue, green), glass
  strength slider, "Open theme file" button.
- About: logo (white sound-wave mark on an orange tile), "EchoPlay 2.0.0", "Publisher: Hüseyin Can ÇALTI",
  update status "You're up to date" with last-checked time and a "Check now" button, "Engine: mpv 0.41",
  source code link, licenses link, privacy line "Checks GitHub once a day for a new version. Sends
  nothing else."
```

**Prompt 4: start screen and playlist**
```
Add the start screen shown when EchoPlay opens without a file: a drop zone with "Open file" and
"Open folder" buttons; a "Continue watching" row of cards (thumbnail, progress bar, date, a chip with the
number of audio tracks); a short list of recent folders. Then add the playlist side panel over the player,
listing the other clips from the same folder with the current one highlighted.
```

**Prompt 5: small states**
```
Add the small states, each shown on top of the player: a resume toast "Resume from 1:24?" with
"Resume" and "Start over"; an update card "EchoPlay 2.1.0 is ready" with "Update" and "Later"; volume
and seek flash indicators; a drag-over state when a file is dragged onto the window; an error state
"This file can't be opened" with the reason; a first-run hint pointing at the mixer chip. Finally, list
the full `theme` object as a table at the bottom of the page.
```
