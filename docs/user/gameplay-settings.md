# Gameplay and Audio Settings

> [!NOTE]
> Inherited from openQ4 and not yet checked against PREY. This page describes what the engine code does; PREY behavior stays validation-pending until the [rebase status ledger](../dev/prey-rebase/status-ledger.md) records evidence for it.

This guide covers PREY's own game options and the audio settings openPR06's engine adds.

openQ4's other gameplay toggles (corpse cleanup and sinking, the hit marker, damage numbers and the weapon-wheel offset) live in Quake 4 game code. PREY's game module doesn't have them, so they aren't covered here.

## Game Options

**Options -> Game Options** in the stock PREY menu sets these game cvars:

| Setting | Default | What it does |
|---|---:|---|
| `g_decals` | `1` | Show decals such as bullet holes. |
| `g_showHud` | `1` | Show the HUD. |
| `ui_showGun` | `1` | Show the first-person weapon. |
| `g_subtitles` | `1` | Show subtitles. |
| `com_profanity` | `0` | `0` censors profanity; `1` plays and displays uncensored dialogue. |
| `g_crosshair` | `1` | Which crosshair to use (`0` = off, `1`-`8`). |
| `g_useDDA` | `1` | Use PREY's dynamic difficulty adjustment. |

The same page also has mouse options (`m_pitch` for inverted mouse, `m_smooth`, `sensitivity`, `in_freelook` and `in_toggleZoom`); see [Input Settings](input-settings.md).

## openPR06 Game Cvars

| Setting | Default | What it does |
|---|---:|---|
| `g_autoSkipCinematics` | `0` | Single-player only. Skips cinematics and other scripted sections where the player has no control, as soon as they begin. Ignored in multiplayer. |
| `hud_graphicalObits` | `1` | Multiplayer only. Shows deaths as a compact feed at the HUD's top left: the killer's name, an icon for the weapon (with a small badge for an alternate fire, a skull for a suicide) and the victim's name. Up to five lines show at once, each for three seconds; lines naming you are lit. The death messages then stay out of the chat lines, though the console and the in-game menu's chat history still list them. `0` restores the stock chat-line messages. |
| `g_showPlayerShadow` | `1` | Shows your own player model's shadow, with the weapon in your hand, in your own first-person view. Other players' shadows always show, in single-player and multiplayer. Retail PREY shipped `0` and gave multiplayer players no shadow at all; a config saved with the retail `0` is switched on once (`g_showPlayerShadowMigrated`). |

## Quick Reference: Audio

| Setting | Default | What it does |
|---|---:|---|
| `s_volume` / `s_volume_dB` | `1` / `0` | Volume of every sound except the score, as a linear value or in decibels. The two stay in sync. |
| `s_musicVolume` / `s_musicvolume_dB` | `1` / `0` | Volume of the score, used instead of the master volume. The two stay in sync. |
| `s_voiceDucking` | `1` | Lower the rest of the mix while a dialogue line plays that asks for it. Not saved. |
| `s_voiceDuckVolume` | `-4.5` | How far, in decibels, voice ducking lowers the rest of the mix. Not saved. |
| `s_dialogueVolume` | `1` | Spoken dialogue volume, from `0` (mute) to `1` (full), independently of effects and music. |
| `g_levelloadmusic` | `1` | Play map-defined music while levels load. |
| `s_useEAXReverb` | `1` | The map's per-area room reverb. See [Room Reverb](#room-reverb). |
| `s_useEAXOcclusion` | `1` | Closed doors muffle the sounds behind them. Works while `s_useEAXReverb` is on. |
| `s_numberOfSpeakers` | `0` | Speaker layout. `0` follows your output device, `2` forces stereo, `6` forces 5.1. See [Speaker Layout](#speaker-layout). |
| `s_reverse` | `0` | Swap the left and right channels. |

**Options -> Audio** in the stock menu sets the master and music volume, level-load music, OpenAL, the speaker layout, reversed stereo, the output device and EAX reverb.

## Music Volume

`s_musicVolume` sets the score's level. As in retail PREY, it replaces the master volume for the score, so the master volume does not change the score and the music volume changes nothing else. The stock menu's music slider sets the matching `s_musicvolume_dB`, and the two cvars follow each other.

- `0`: mute the score.
- `1`: full level, the default (retail's 0 dB).

It applies to sound shaders in PREY's music sound class (`soundClass SC_MUSIC`): the main menu's overture and the in-game score. Other sounds that are music in the story, such as the Roadhouse jukebox and the level-load music, use the master volume, as in retail. The setting is live and can be adjusted while the game is running.

While a dialogue line plays that asks for it, the game lowers everything but speech and the score by `s_voiceDuckVolume` (4.5 dB), over 0.2 s, and brings it back over 0.5 s once the line ends. Retail PREY does the same. In the Roadhouse, for example, the jukebox dips while Tommy speaks.

```cfg
seta s_musicVolume 0.2
```

## Dialogue Volume

`s_dialogueVolume` changes spoken dialogue without lowering weapons or music. It takes effect immediately and is saved. `seta s_dialogueVolume 0.6` sets it to 60% of the authored level; `0` mutes dialogue. Master volume still affects everything. The stock PREY menu has no slider for it; the retained interface (`ui_retained 1`) does.

Lip-synced speech and sounds on the voice-over asset paths share this control. Multiplayer voice chat has its own settings. A mod that mixes speech and music into one recording can't separate them with this setting.

## Room Reverb

A map's reverb table tells the game which reverb each of its rooms, corridors and outdoor areas uses. openPR06 plays these the way EAX sound hardware did:

- the reverb follows you from area to area, and the areas next to you keep their own reverb, so a gunshot in the next room still sounds like that room;
- each nearby area's reverb comes from the doorway that leads to it;
- closed doors muffle the sounds behind them, and their reverb with them;
- radio chatter stays dry and voice-over sits a little further back.

It needs OpenAL's EFX extension. OpenAL Soft provides it: Windows builds bundle it, and Linux distributions install it as their OpenAL.

- `s_useEAXReverb 1` (default): room reverb on, in every performance preset. `0` turns it off, along with door muffling. Changing it from the console takes effect after `s_restart`.
- `s_useEAXOcclusion 1` (default): closed doors muffle sound while reverb is on.
- `s_muteEAXReverb 1`: silences the reverb of the area you're standing in, for comparison.
- Maps without a reverb table use the Generic reverb everywhere.
- `listReverbs` lists the reverbs, the current map's areas and the reverbs playing around you. `s_showReverb 1` prints a line whenever they change.

## Speaker Layout

openPR06 follows your audio device by default. Headphones and stereo speakers get stereo, and a 5.1 or 7.1 system gets all of its channels. OpenAL Soft's 3D headphone audio (HRTF) is off unless you choose it, except in a VR headset.

- `s_numberOfSpeakers 0` (default): use the output device's own layout. Set it from the console.
- `s_numberOfSpeakers 2`: always stereo.
- `s_numberOfSpeakers 6`: always 5.1. On a stereo device the system downmixes it and headphone HRTF can't engage, so pick this only for a 5.1 system that misreports its layout.

The stock **Options -> Audio** speaker choice switches between `2` and `6`. Choosing either there replaces the automatic default; run `seta s_numberOfSpeakers 0` to return to it.

- `s_openALHRTF` chooses headphone HRTF: `1` off (default), `2` on, `0` automatic (on when the system reports headphones, which many laptop and front-panel outputs claim whatever is plugged in). A config saved with the old automatic default moves to off once.
- `vr_hrtf 1` (default) turns HRTF on while a VR headset session runs, whatever `s_openALHRTF` says; `vr_hrtf 0` makes the headset follow `s_openALHRTF`.
- Unplugging a headset or switching the default device keeps the sound going, including music and looping ambience, and keeps your speaker and HRTF choices.
- Changing `s_numberOfSpeakers`, `s_openALHRTF` or `s_outputLimiter` applies to the open device in place: sounds keep playing without reloading.

## Audio Quality

These are on by default and rarely need changing. Each takes effect after `s_restart`.

- `s_sourceRadiusScale 0.5`: a sound has a size, half of its shader's `minDistance`. Standing next to a machine or a fire, it surrounds you instead of jumping from ear to ear as you turn. `0` makes every positioned sound a point.
- `s_directStereo 1`: music and other stereo sounds play straight to your speakers or headphones instead of passing through 3D headphone audio as two virtual speakers.
- `s_resampler auto`: sounds recorded at a low rate such as 22 kHz (nearly all of Prey's) are upsampled with a 23rd order sinc filter, which keeps their treble and adds no aliasing. `default` keeps OpenAL's own, and `listResamplers` lists the choices.
- `s_outputLimiter 1`: loud scenes compress instead of clipping.
- `s_airAbsorption 1`: distant sounds lose a little treble, by the air absorption each room's reverb preset sets.

## Troubleshooting

- If music doesn't respond to `s_musicVolume`, test on a map or menu that's actively playing music rather than ambience or voice-over.
- If there's no room reverb, check that `s_useEAXReverb` is `1` and that your OpenAL is OpenAL Soft (the log reports the EFX extension at startup), then run `s_restart`.
- If 5.1 output sounds wrong on headphones, set `s_numberOfSpeakers 0` so the device's own layout and HRTF apply.
