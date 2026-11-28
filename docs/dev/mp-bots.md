# Multiplayer bot characters

openPR06 multiplayer ships a data-driven cast built on Prey's own artificial
player. Each bot is a normal multiplayer player entity with a retail model,
skill-scaled execution, a recognizable way of fighting, and a separate voice.
This guide describes the runtime contract, content format, operator controls,
and the limits that still matter.

The design follows the successful content process used upstream in openQ4:
skill, style, character mechanics, and chat remain separate and independently
validated. The runtime is deliberately Prey-specific. It uses
`hhArtificialPlayer`, `ui_modelNum`, Prey's `weaponobj_*` classes, and the
player's current gravity axis; it does not import Quake 4 client-slot, model,
weapon, or navigation assumptions.

## Architecture and API compatibility

The canonical implementation lives in the in-tree Prey game library:

```
src/game/bots/BotCharacter.h
src/game/bots/BotCharacter.cpp
src/Prey/game_player.h
src/Prey/game_player.cpp
```

`rvBotCharacterManager` owns styles, named characters, skill resolution,
weapon biases, chat banks, reply rules, and flood throttles. It is initialized
from `idGameLocal::Init` and shut down from `idGameLocal::Shutdown`, so reloads
and map changes do not leave content-owned pointers behind.

Prey already declares `player_artificial_mp`, whose spawn class is
`hhArtificialPlayer`. `idGameLocal::SpawnArtificialPlayer` places that entity
in a free player slot, fills ordinary user info, and lets the normal
multiplayer code spawn it. Each server frame `hhArtificialPlayer::Think`
generates a `usercmd_t`; the established snapshot path mirrors that command to
clients. Weapons, damage, respawn, scoring, team rules, and the scoreboard
therefore continue through the same player code used by people.

Artificial-player slots are not engine-owned network clients, so the game
publishes their canonical `bot_character`, `bot_skill`, name, model, and team
userinfo explicitly on spawn and reload. A late join receives that userinfo
immediately before the existing reliable player-spawn message, while the first
snapshot carries the same dictionary as a fallback. Team DM assigns a new bot
to the less-populated team once and preserves that team across a content reload.

This is an important compatibility choice. The old engine-side
`OPENPR06_ENABLE_BOTS` path is compiled out because it expects callbacks and
metadata absent from Prey's v7 game contract. The character system adds no
virtual to the engine/game boundary and keeps `GAME_API_VERSION` at 7.

`g_artificialPlayerCount` remains the compatibility count control. The server
reconciles toward it one bot per frame, adding or removing as needed
(`idGameLocal::ReconcileArtificialPlayers`, every frame; it ran in the debug
pass, which a retail-configured build skips without `developer 1`; a listen
server waits for the host's own player, whose slot a bot would otherwise
take). `bot_minPlayers` (archived, 0 to 32) asks for more where people are
few: the reconciler wants at least that many players less the people in
(a connection still being made counts as one), so a bot leaves as a person
arrives and comes back once they go; the larger of the two counts wins.
MultiPrey's Create Server sets `bot_minPlayers` from its Bots tab (Minimum
Players), beside `bot_skill`, `bot_skillVariance`, `bot_chat` and
`bot_characters`, and clears `g_artificialPlayerCount` as the server starts,
so a fixed count left from an earlier server does not add to it. The
named commands below update that count after a manual change so the automatic
reconciler does not immediately undo the command. It is explicitly non-cheat,
so those count updates work on an ordinary multiplayer server without enabling
developer cheats. A real network connection reserves its client slot from
`ServerClientConnect` until `ServerClientBegin` has created the human player;
manual and reconciled bot spawns both skip that pending slot. If the connection
claims a game-only bot slot, the server also lowers the desired count, tells
existing clients to delete the old bot entity, and only then gives the slot to
the human. The handshake therefore cannot race bot creation or retain the
wrong player class on another client.

## Movement and current navigation limit

Retail Prey multiplayer maps do not ship bot AAS files. This implementation
does not claim to provide a full replacement navigation graph.

The current bot movement is a bounded combat fallback:

- enemies must be in the player's PVS and pass a line-of-sight trace before
  they are treated as visible;
- idle bots may choose a visible item, or a visible game portal, and
  otherwise wander; a goal that makes less than 16 units of net progress in
  four seconds is ignored for five seconds, so line of sight alone cannot trap
  a bot on an unreachable pickup;
- aiming and movement use untransformed view angles, `GetEyeAxis()`, and bounds
  projection along the current gravity normal, so they remain meaningful when
  local gravity changes;
- combat range, retreat health, aggression, patience, strafing rhythm, dodge
  reaction, and jump chance come from the resolved personality; and
- every step is checked first (`hhArtificialPlayer::BotMoveBlocked`, below).

### Prey's walkways, gravity, portals and voids

The step check is built for Prey's arenas:

- **It keeps its shape in any gravity.** It sweeps the player's own clip
  model along the player's own axis, one step above the surface the bot stands
  on and in that surface's plane, so it is the right box on a wall, a ceiling
  or the inside of a sphere, and stairs or a pitched view do not read as
  walls. The original check swept a world-aligned box from the bot's feet,
  which started inside geometry on 13-50% of frames (on a ceiling, always)
  and then reported a wall.
- **A wall-walk surface ahead is a route.** Player physics carries a bot onto
  it, so the check lets the bot walk on instead of turning away.
- **It refuses a step off an edge.** It looks for floor beneath the step, near
  and far, and farther still at speed. No floor within 384 units, a death volume
  (`trigger_deathresurrection`) or an enabled `trigger_hurt` in the fall, or
  stepping off a walkway where the restored gravity would not hold the bot
  (off a ceiling or a wall), is a drop. A drop into an enabled gravity zone,
  or round an outside corner of a walkway, is a route. A bot at an edge backs
  away at full speed and turns once, and never takes a response step that is
  itself a drop.
- **It jumps only where the landing is level and safe.** A wall-contact jump
  needs floor about the bot's own level just past the obstacle and a running
  jump's length beyond it, and a wall-walking bot never jumps: in the air,
  player physics restores normal gravity and drops it off the wall.
- **It strafes on the side it checks.** Physics moves a positive `rightmove`
  along the right vector, which is the negation of `idAngles::ToMat3()` row 1;
  the original check swept the mirror side, so a strafing bot could walk off
  an edge the check reported clear.
- **It turns once per contact.** At a wall the bot turns 90 to 150 degrees and
  keeps its new strafe side for 0.4 s. It had turned 80 degrees and reversed
  its strafe every frame, which pinned it against the wall it was leaving.
- **Portals are routes.** An idle bot treats a visible teleporting game portal
  (`hhPortal::CanTeleport`) as a goal, weighted by its initiative. A portal
  keeps the player's untransformed view angles, so the bot walks on away from
  the exit; `Portalled` drops the goal and ignores that portal, and any within
  256 units of the exit, for eight seconds, so it does not turn straight back.

Measured on 22 November 2026 with six skill-3 bots (Tommy, Chuck, Trent, Roy,
Jen, Becky) for about five minutes on each of the twelve retail deathmatch
maps, from a dedicated server, with temporary counters in the game code.
These figures come from one or two runs per map, and they vary by about a
third between runs. `dmtunnelrat_8` has no portals, and in these runs no bot
went through one of `dmescher2`'s four:

| | Before | After |
| --- | --- | --- |
| Frames with the step check refusing a wall | 39-65% | 19-60% |
| Wall sweeps that start inside geometry | 13-50% | 1-19% |
| Falls per 100,000 bot-frames, `dmgravitylab_6` | 26 | 2-4 |
| Falls per 100,000 bot-frames, `dmshuttle2` | 36 | 3 |
| Falls per 100,000 bot-frames, `dmshuttle1` | 9 | 0-1 |
| Portal trips in a run | at most 5 per map | 3-53 per map, on 10 of 12 maps |

The time spent wall-walking or in redirected gravity rose on most wall-walk
maps, because bots walk onto walkways instead of turning away. In the same
runs, aim under redirected gravity was no worse than in normal gravity: the
view after `hhPlayer::Think` matched the angles the bot set to within 1.5
degrees on average, and the average firing error against the target's centre
was similar either way.

### Aim and hits

`hhArtificialPlayer::UpdateBotAimAndFire` builds the shot in four steps:

- **The aim point is the body.** It is the centre of the target's bounds,
  moved along the target's own up axis by `aimHeight` of its height. When a
  trace from the bot's eye to that point is blocked (sight is checked eye to
  eye, so a target behind a rail can be visible with its body hidden), the
  bot aims at the head instead. The original model aimed at the eye, the top
  of the head, and the characters' positive `aimHeight` raised it further.
- **It tracks with an estimate of the target's velocity.** The believed
  velocity follows the real one with `aimTrackTimeConst`, and the believed
  point moves with it while closing on the real point at the same rate. A
  steady runner is tracked exactly; a change of direction opens an error that
  peaks about one time constant later and then closes, so strafing beats a
  slow tracker. The original model let the believed point trail the target by
  its time constant and also subtracted a share of each frame's motion from
  it, which kept a skill-3 bot about 90 units behind a running target.
- **`aimTrackError` is a cross-view lag.** It holds the shot
  `aimTrackError` degrees behind per 100 degrees a second of line-of-sight
  rate, the bot's own movement included.
- **The lead uses the weapon's projectile.** `BotProjectileSpeed` reads the
  current weapon's fire info and its `def_projectile` (the `_mp` variant on a
  server): 7000 u/s for the Hunter Rifle, 7500 for the autocannon, 1600 for a
  multiplayer rocket, none for the wrench or the Leech Gun's beam. The
  original model led every weapon as a 1200 u/s projectile, about six times
  too far for the rifle. `aimLeadError` is drawn once per burst, not every
  frame.

The fire cone is measured from where the view ends up after the frame's turn,
and `holdFireTurnRate` counts only the slew beyond following the aim point,
so a bot can fire while tracking a crossing target. Each bot's tremor has its
own phase.

**Two player models could not be hit on a dedicated server.** Their
multiplayer skins, `jen_mp` and `human2_body_mp` (Jen, and the Abducted
model Victim uses), keep only a `skin.vfp` stage at shader level 3. A
dedicated server never creates GL programs, so the stage lost its
interaction role, the skin parsed as translucent, and
`idRenderWorld::ModelTrace`, which projectiles use against player models,
skipped it. Jen took 0 damage from 3908 firing frames aimed at her. The
material parser now keeps the interaction role of a program it scanned but
could not create, and both models take damage in proportion to the shots
that reach them.

Measured on 22 November 2026 from a dedicated server, with temporary counters
in the game code, in five-minute deathmatch runs on `dmwallwalk2`,
`dmtunnelrat_8`, `dmplanes_4`, `dmescher`, `dmsphere` and `dmgravitylab_6`.
"Equal" is six skill-3 bots (Tommy, Jen, Trent, Roy, Victim, Hunter); the
ladders mix skills 1-5 in one match, with the characters rotated between maps.
A hit is a firing frame whose forward trace from the view meets the target's
render model, which is what projectiles collide with. Before: 5 runs; after:
16 runs over two batches, which agreed to within about a fifth.

| | Before | After |
| --- | --- | --- |
| Equal skill 3: hits per firing frame | 9.7% | 26.4% |
| Equal skill 3: kills per bot per five minutes | 0.58 | 1.46 |
| Equal skill 3: mean error from the target's centre | 4.1 degrees | 2.7 degrees |
| Ladder hits per firing frame, skill 1 / 2 / 3 / 4 / 5 | 2.5 / 1.7 / 13.1 / 11.3 / 3.9% | 8.0 / 13.5 / 21.2 / 40.3 / 52.9% |
| Ladder damage per second an enemy is in sight, skill 1-5 | 0.3 / 0.1 / 3.3 / 7.9 / 3.8 | 1.0 / 3.4 / 6.8 / 15.8 / 24.8 |
| Ladder kills per minute an enemy is in sight, skill 1-5 | 0 / 0 / 1.7 / 3.7 / 0.9 | 0 / 1.3 / 2.2 / 5.8 / 11.8 |

Each level now beats the one below it, where skill 5 had trailed skill 4. A
skill-5 bot hits about half its firing frames and deals about 25 damage a
second while it has a target, so a player it sees loses a fight against it in
a few seconds. Skill 1 still kills almost nothing: its turn rate of about 70
degrees a second cannot follow a strafing target.

That is enough for bots to fight, dodge, collect exposed pickups, use
walkways and portals they meet, and stay out of Prey's voids. It is not route
planning: a bot cannot deliberately solve a multi-room path, choose a portal
sequence, climb to a walkway it cannot see, or pursue an objective through
unseen geometry. It never uses spirit walk, so spirit bridges and spirit-only
routes are closed to it, and it never boards the shuttles of `dmshuttle1`
and `dmshuttle2`. A future navigation layer must model Prey's changing
gravity, portals and spirit routes before it can honestly promise broad map
coverage. `mp_bot_navigation.py` remains deferred for this reason.

## Personality resolution

Every spawn resolves three layers into one flat `botTraits_t`:

1. **Skill, 1 through 5.** The baseline describes execution quality. A
   deterministic `bot_skillVariance` offset may place one bot between the five
   authored rows, and values are interpolated.
2. **Play style.** An archetype describes intent: preferred range, willingness
   to pursue or wait, movement rhythm, and weapon taste. A low-skill sniper and
   a high-skill sniper want the same kind of fight; only one executes it well.
3. **Character.** The identity adds a display name, retail model slot,
   preferred skill band, sparse personal modifiers, weapon preferences, and a
   separately authored voice.

Resolution order is significant:

```
baseline( effective skill )
  -> style body
  -> style skill <n> { } block
  -> character body
  -> character skill <n> { } block
```

`set`, `add`, and `scale` operations apply in file order. Every result is
clamped through the single trait field table immediately after application.
Weapon biases are named entries rather than scalar traits; they merge across
layers so a character's opinion about one weapon does not erase the rest of
its style.

`bot_characters 0` is supported. It leaves bots on the complete skill baseline
with a generated `Bot N` name, a slot-derived retail model, and no character
voice.

## The 48 traits

All scalar personality fields are floats, including values expressed in
milliseconds or degrees. This keeps parsing, sparse modifiers, interpolation,
and clamping on one path. Weapon biases are separate and are not counted among
the 48.

### Vision and reaction (8)

| Trait | Meaning |
| --- | --- |
| `sightRange` | Maximum distance at which a player can be noticed. |
| `fov` | Full vision cone in degrees. |
| `reactionMsec` | Delay paid on a fresh target acquisition. |
| `reactionVarianceMsec` | Per-acquisition random spread around that delay. |
| `reacquireMsec` | Time out of sight after which contact is considered fresh. |
| `reacquireFraction` | Fraction of reaction delay paid for a quick reappearance. |
| `peripheralAngle` | Off-axis angle at which the full peripheral penalty applies. |
| `peripheralPenaltyMsec` | Additional reaction delay for peripheral contact. |

### Aim (10)

| Trait | Meaning |
| --- | --- |
| `turnSpeed` | Hard view-slew cap in degrees per second. |
| `turnAccel` | Rate at which view slew reaches its cap. |
| `turnDamping` | Slew damping; low values visibly lag and hunt. |
| `aimTrackTimeConst` | Time constant with which the believed target velocity, and the point moving with it, follow the real ones. |
| `aimTremorDeg` | Continuous aim tremor amplitude. |
| `aimTremorRateHz` | Rate at which the tremor wanders. |
| `aimTrackError` | Degrees of cross-view lag per 100 degrees a second of line-of-sight rate, the bot's own movement included. |
| `aimSettleMsec` | Time aim must remain inside the firing cone. |
| `aimLead` | Fraction of the lead for the current weapon's projectile speed that is applied. |
| `aimLeadError` | Error applied to that lead, drawn once per burst. |

### Trigger discipline (5)

| Trait | Meaning |
| --- | --- |
| `fireConeDeg` | Maximum angular error, after this frame's turn, accepted before firing. |
| `holdFireTurnRate` | Maximum view slew, beyond following the aim point, allowed while firing. |
| `burstMinMsec` | Shortest automatic-weapon burst. |
| `burstMaxMsec` | Longest automatic-weapon burst. |
| `burstPauseMsec` | Pause between bursts. |

### Mistakes (2)

| Trait | Meaning |
| --- | --- |
| `mistakeChance` | Chance that a combat decision is deliberately spoiled. |
| `mistakeMsec` | Duration of the resulting mistake window. |

### Movement and engagement (10)

| Trait | Meaning |
| --- | --- |
| `strafeChance` | Likelihood of using a combat strafe. |
| `dodgeReactMsec` | Delay between taking damage and beginning a dodge. |
| `jumpChance` | Likelihood that obstacle response or a dodge includes a jump. |
| `combatRange` | Distance the bot tries to hold from an opponent. |
| `rangeDiscipline` | Strength with which it corrects distance from that range. |
| `aggression` | Preference for closing rather than yielding an even exchange. |
| `patience` | Preference for holding rather than wandering. |
| `retreatHealth` | Health fraction below which it tries to disengage. |
| `pursuit` | Strength and duration of commitment to a lost target. |
| `itemFocus` | Weight and scan distance given to visible pickup goals. |

### Tactical personality (9)

| Trait | Meaning |
| --- | --- |
| `initiative` | Willingness to move and take a less-than-perfect opportunity. |
| `targetStickiness` | Reluctance to abandon the current opponent. |
| `opportunism` | Preference for finishing a wounded opponent. |
| `vengefulness` | Preference for the opponent that last killed the bot. |
| `suppressionMsec` | Time it may fire at the last seen position through cover. |
| `strafeRhythmMsec` | Base interval between changes of strafe direction. |
| `strafeRhythmVarianceMsec` | Random spread added to that interval. |
| `weaponSwitchMsec` | Delay between weapon-choice re-evaluations. |
| `aimHeight` | Personal vertical bias from the centre of the target bounds. |

### Decision quality (2)

| Trait | Meaning |
| --- | --- |
| `weaponSkill` | Chance of choosing the range-appropriate available weapon. |
| `targetSelection` | Chance of choosing the best scored opponent rather than the nearest. |

### Chat (2)

| Trait | Meaning |
| --- | --- |
| `chatiness` | Chance that a chat-worthy event queues a line. |
| `chatDelayScale` | Multiplier on thinking and typing time before delivery. |

The counts are 8 + 10 + 5 + 2 + 10 + 9 + 2 + 2 = 48. Competence-facing
fields generally improve from skill 1 to skill 5. Taste fields such as
`combatRange`, `aggression`, `patience`, and `chatiness` stay available to
styles and characters so high difficulty does not collapse the cast into one
personality.

## Styles

Six styles ship under `content/basepr/pak0/botfiles/styles/`:

| Style | Behavior and weapon taste |
| --- | --- |
| `rusher` | Closes to brawling range, accepts imperfect shots, retreats late, and favors the wrench, Soul Leech, and autocannon. |
| `sniper` | Holds long range, moves less, settles carefully, and favors the rifle, Hider weapon, and bow. |
| `roamer` | Stays near the baseline, weights visible pickups heavily, and fights whatever it meets during the rotation. |
| `hunter` | Commits to one opponent, preserves contact through brief cover, pursues strongly, and discounts items. |
| `ambusher` | Holds a prepared route, watches the angle, reacts efficiently from that setup, and dodges less. |
| `skirmisher` | Strafes and jumps readily, uses short bursts, changes its gravity-relative line, and disengages sooner. |

Every style has skill-specific corrections where its intent would otherwise
make a low-skill bot nonfunctional. Those blocks do not turn preference into
accuracy; the baseline still owns how well the bot sees, aims, reacts, and
chooses.

## Roster and lore boundaries

Prey selects multiplayer appearances through the integer `ui_modelNum`, not a
Quake 4 `playerModel` declaration. The shipped character files use exactly the
retail-selectable slots:

| Slot | Character | Style | Band | Play and voice evidence |
| ---: | --- | --- | --- | --- |
| 0 | Tommy | hunter | 3-5 | Story identity. Relentless former soldier; blunt, humane, dryly restrained speech. |
| 1 | Mutilated Human | rusher | 1-3 | Retail role label. Close pressure and sparse, dignified fragments; no invented former identity. |
| 2 | Chuck | rusher | 1-3 | Roadhouse story NPC. Loud follower pressure and playful bravado without repeating the scene's abuse. |
| 3 | Dalton | rusher | 2-4 | Roadhouse story NPC. The barroom instigator turns close exchanges into grudges while keeping his swagger about the match. |
| 4 | Hider | ambusher | 2-4 | Hidden role label. Waits behind prepared cover and uses urgent, capable resistance coordination. |
| 5 | Grandfather | sniper | 2-4 | Story identity. Patient teacher who sees the line before the shot; spiritually grounded without imitation folklore. |
| 6 | Abducted | roamer | 1-3 | Circumstance label. Scavenges visible routes and speaks as a wary, self-directed survivor. |
| 7 | Teacher | roamer | 2-4 | Retail label. Methodical observation and learning, with no invented biography. |
| 8 | Edward | roamer | 1-3 | Retail named label. Cautious generalist with restrained, courteous speech. |
| 9 | Trent | skirmisher | 2-4 | Retail named label. Takes short trades, changes angles, and speaks concisely about routes. |
| 10 | Roy | ambusher | 2-4 | Retail named label. Dry lane-holder with an easygoing, sportsmanlike voice. |
| 11 | Mohawk Hider | skirmisher | 3-5 | Retail model label. Bold, mobile Hidden scout; never presented as a claim about a real-world nation or community. |
| 12 | Victim | ambusher | 2-4 | Circumstance label. Controls cover and exits with vigilance and agency; never a joke or a competence judgment. |
| 13 | Post-op | skirmisher | 3-5 | Circumstance label. Controlled movement and focused speech; the label is never played for ridicule. |
| 14 | Becky | skirmisher | 3-5 | Retail named label. Self-possessed marksman who relocates after an exchange; no invented biography. |
| 15 | Elite Hunter | sniper | 4-5 | Enemy role. Precise long-range execution and sparse translated combat traffic. |
| 16 | Elhuit | ambusher | 3-5 | Story identity. Hidden resistance leader who prepares ground and speaks with formal tactical authority. |
| 17 | Hunter | hunter | 2-4 | Enemy role. Marks and drives down one target; clipped functional squad traffic with no fake alien language. |
| 18 | Jen | skirmisher | 2-4 | Story identity. Mobile, grounded defender with practical and direct speech. |

Mother is intentionally excluded. Some code and assets suggest a partial model
19, but retail `player.def` does not expose a complete selectable `model_mp19`
slot. The bot roster does not invent that contract or substitute another
model.

Lore discipline is part of the content contract. Story identities may use
facts established by the game. Retail names, visual roles, and multiplayer-only
labels support restrained gameplay inference, not invented biographies,
relationships, accents, slurs, or fake alien vocabulary. Circumstance labels
such as Victim, Abducted, Mutilated Human, and Post-op never define a
character's worth. The stock phrase “Mohawk Hider” is treated only as the
retail model label.

Generic names such as Hunter, Hider, and Victim are also broad words in normal
conversation. Their low-priority direct reply rules require an addressed name,
and their responses stay restrained so an accidental match is not disruptive.

## File format

Personality files are text brace blocks parsed through `idLexer` with
`DECL_LEXER_FLAGS`. They are not decl types and require no engine registration.
A fresh lexer is used for each file, and all three file lists are freed through
the filesystem API.

```
content/basepr/pak0/botfiles/styles/<style>.style
content/basepr/pak0/botfiles/characters/<character>.bot
content/basepr/pak0/botfiles/chats/<character>.chat
```

Styles load first, characters second, and chat banks third. Inheritance and
character/chat ownership are resolved only after all relevant files have been
enumerated, so package enumeration order cannot change the result. Unknown
keys, traits, styles, owners, model slots, and malformed blocks warn and remain
contained to the affected content instead of terminating the server.

The grammar is:

```
style "<name>" {
    description "<text>"
    inherit "<other style>"              // optional

    set   <trait> <number>
    add   <trait> <number>
    scale <trait> <number>
    weapon "<weapon class>" <bias>

    skill <1..5> { <same statements> }
}

character "<display name>" {
    description "<text>"
    inherit "<style>"
    skillBand <min> <max>
    modelNum <0..18>

    set/add/scale <trait> <number>
    weapon "<weapon class>" <bias>
    skill <1..5> { <same statements> }
}

characterChat "<display name>" {
    chat <event> {
        "line"
    }

    reply <category> {
        priority <0..100>
        source any | player | bot
        addressed either | required | forbidden
        trigger "<whole word or phrase>"
        "response line"
    }
}
```

The supported multiplayer weapon classes are:

- `weaponobj_wrench`
- `weaponobj_rifle`
- `weaponobj_crawlergrenade`
- `weaponobj_soulstripper`
- `weaponobj_autocannon`
- `weaponobj_hiderweapon`
- `weaponobj_rocketlauncher`
- `weaponobj_bow`

Weapon preferences influence selection among weapons the player actually has;
they do not grant inventory. Named biases merge, and `1.0` is neutral.

## Chat

Chat is character-owned and server-side. Mechanics stay in `.bot` files while
the corresponding `.chat` file joins to the character name
case-insensitively. Event lines are sent through
`idMultiplayerGame::ProcessChatMessage` with the artificial player's own client
number, so display name, routing, and scoreboard identity remain consistent.

The events are: `entergame`, `levelstart`, `kill`, `killWrench`, `killStreak`, `revenge`, `death`, `deathAccident`, `itemDenied`, `leadTaken`, `leadLost`, `matchWin`, `matchLose`, `farewell`.

`itemDenied` fires from the authoritative multiplayer pickup path when another
player takes the visible item the bot was pursuing. This supplies both the
localized item name and the winning player's name before the item respawns or
is removed.

Every shipped character provides exactly eight alternatives for every event:
112 event lines per voice and 2,128 across the 19-character roster. Selection
excludes the immediately previous usable line when another is available.
`killWrench` is the Prey-specific close-combat event corresponding to the
upstream humiliation line.

Lines may use `$self`, `$other`, `$weapon`, `$map`, and `$item`. They name what a
player sees: `$weapon` is the weapon's retail `weaponname` ("Hunter Rifle",
"Crawler Launcher"), `$map` the mapDef's localized `name` ("Spindle Spheres",
"Shuttle 2") and `$item` the item's translated `inv_name`, never a class name
such as `weaponobj_rifle` or a path such as `game/dmsphere`. The loader and
validation contract limit tokens to events that can supply them. At runtime a
line whose required value is unavailable is skipped rather than sent as a
broken sentence. Authored lines are limited to 160 characters and may not
begin with `#`; substitution is checked again before delivery.

### Triggered replies

Each shipped voice has exactly nine reply categories: `help`, `goodGame`,
`challenge`, `greeting`, `thanks`, `praise`, `apology`, `farewell`, and
`direct`. Every category has exactly eight normalized triggers and four
responses.

Incoming accepted chat is stripped of color escapes, folded to lower case,
and matched as contiguous whole words. A higher priority wins, then a longer
trigger, then file order. `source` limits a rule to people, bots, or either;
`addressed` controls whether the character's whole display name must be
present. The longest addressed character name owns the message, so “Elite
Hunter” cannot accidentally call “Hunter”; if that character has no matching
reply, the message does not fall through to a shorter or general responder.
Otherwise at most one eligible bot answers. Team chat considers only bots on
the speaker's team and the reply remains team-only.

A generated reply marks its own delivery and cannot recursively trigger another
reply. A bot with a line already queued does not replace it. Delivery is
subject to a six-second per-bot throttle and a 1.2-second server-wide throttle;
chatty mode shortens both, and map init/restart resets both clocks. The delay
combines `bot_chatDelay`, visible text length at `bot_chatCPM`, and the
character's `chatDelayScale`.

Chat files are raw bot content, not language-dictionary strings. Retail
`<PROFANITY>` markup is not filtered through `idLangDict::GetString` here.
Shipped voices therefore avoid strong profanity, slurs, invented derogatory
language, and imitation accents even when a lore character is angry or
hostile.

## Adding or tuning a character

1. Add a mechanics file under
   `content/basepr/pak0/botfiles/characters/`. Use the exact display name,
   choose one existing style, set a truthful skill band, and choose a retail
   model slot from 0 through 18.
2. Bias only the traits that make the character distinct. The style already
   owns broad intent and the baseline owns competence.
3. Use only the eight supported `weaponobj_*` classes. Preference is taste,
   not inventory or skill.
4. Add the matching `characterChat` file under
   `content/basepr/pak0/botfiles/chats/`. A shipped voice requires eight lines
   for each of the 14 events and all nine reply categories with eight triggers
   and four responses.
5. Separate evidence from inference in comments. Cite a story role where one
   exists; for a generic or multiplayer-only label, limit characterization to
   visible role and gameplay behavior. Never manufacture a biography to make a
   voice easier to write.
6. Keep every line concise, self-contained, safe under token substitution, and
   free of localization/profanity markup.
7. Run the character contract test, build `pak0.pk4`, and use `botreload` on a
   multiplayer server to inspect the loaded roster without restarting.

## Commands

| Command | Effect |
| --- | --- |
| `addbot [character] [skill]` | Add one artificial player. The character is optional; the skill override must be 1..5 and becomes the center before configured variance. |
| `removebot [character]` | Remove the named bot, or the last artificial player when no name is supplied. |
| `removebots` | Remove every artificial player and set the compatibility bot count to zero. |
| `kickbots` | Alias for `removebots`. |
| `botlist` | List live bot slots, requested and effective skill, character, and style. |
| `botcharacters` | List every loaded character with style, skill band, model slot, and whether it is in use. |
| `botreload` | Re-read styles, mechanics, and chat, then always rebind live artificial players by character name or to the safe baseline when replacement content is unusable. |
| `spawnArtificialPlayer` | Legacy cheat command that adds one automatically selected artificial player and synchronizes the count. |

Bot mutation commands require a multiplayer server. `botcharacters` is also
useful before a match for checking content load. `botreload` warns when no
usable character content can be loaded and unconditionally rebinds live bots,
preventing them from retaining pointers into the discarded data.
Quote names that contain spaces, for example `addbot "Elite Hunter" 5` or
`removebot "Mutilated Human"`.

## Cvars

| Cvar | Default | Effect |
| --- | --- | --- |
| `bot_skill` | `3` | Baseline difficulty from 1 (easiest) through 5 (hardest). |
| `bot_characters` | `1` | Enable named style/model/voice content; 0 uses only the baseline curve. |
| `bot_forceCharacter` | `` | Session-only tuning name that makes automatic selection choose one character. |
| `bot_skillVariance` | `0` | Deterministic per-bot spread, from 0 through 2 skill levels around the requested skill. |
| `bot_chat` | `1` | 0 silences bots, 1 is normal, and 2 increases chat chance while shortening throttles. |
| `bot_chatDelay` | `600` | Initial thinking delay in milliseconds before visible-character typing time and character scaling. |
| `bot_chatCPM` | `900` | Base visible-character typing speed in characters per minute, clamped to 60..6000. |

All except `bot_forceCharacter` are archived game cvars so a server config can
retain them. The forced character is intentionally session-only: archiving it
would unexpectedly fill later matches with clones.

## Packaging and validation

Game-library edits are made in place under `src/game` and `src/Prey`; Meson
builds the unified module from those trees directly. The existing source
enumeration discovers new `.cpp` files recursively.

Bot content belongs under `content/basepr/pak0/botfiles/`. The normal pak build
recursively includes `.style`, `.bot`, and `.chat` files in `pak0.pk4`, which
is installed beneath `.install/basepr/`. Loose `botfiles` in the staged runtime
are a packaging error; the content must be inside the pak.

`tools/tests/mp_bot_characters.py` is active validation. It independently
checks:

- the v7 artificial-player architecture and manager lifetime;
- all 48 trait fields, field-table entries, clamps, and baseline rows;
- six usable styles and the exact 19-character/model-slot roster;
- known Prey weapon names and valid style/character inheritance;
- exactly eight lines for all 14 events in every shipped voice;
- all nine reply categories with eight triggers and four responses;
- allowed tokens, line limits, whole-word matching, throttles, team routing,
  and reply recursion protection;
- authoritative userinfo ordering for spawn, reload, late join, and the
  bot-to-human slot handoff, including the pending-connection reservation;
- gravity-relative aim/movement, usable-weapon filtering, bounded item-goal
  progress, round-state cleanup, and clone-safe character reservations;
- the step check: clip-model sweep in the bot's own plane, wall-walk
  transitions, near and far landing probes, death-volume, hurt-trigger and
  gravity-zone handling, safe jumps, the strafe sign and the once-per-contact
  turn;
- portal goals and the post-portal ignore window;
- display names for `$weapon` and `$map`;
- the seven documented cvars and the registered commands; and
- packed-content and non-fatal parser contracts.

`tools/tests/mp_bot_navigation.py` remains explicitly deferred. The current
runtime has gravity-aware steering and wall avoidance, not the full navigation
contract that the upstream test describes.

For live validation, use a windowed multiplayer server/client launch with
`+set r_fullscreen 0`. Exercise each style at low and high skill, verify every
model slot and scoreboard name, add/remove/reload bots, trigger combat and
typed-chat responses, then inspect the engine log for parser, snapshot,
reliable-message, and shutdown warnings. Do not treat successful fighting in
one room as evidence of general map navigation.

## Known limits

- Bots do not have full map routing, portal planning, wall-walk transition
  planning, or objective strategy. They use the walkways and portals they
  meet, not ones they would have to plan a route to.
- Bots never spirit walk or board a shuttle, so spirit bridges, spirit-only
  passages and the shuttle duels of `dmshuttle1` and `dmshuttle2` are out of
  their reach.
- Knockback can still throw a bot into a void; the step check prevents
  walking and jumping into one, not being blown into one.
- Bots meet rarely, so kill counts stay low even though their shots now land
  (see [Aim and hits](#aim-and-hits)): with no route planning, a bot in a
  six-bot match had an enemy in sight for 0-100 seconds of five minutes,
  typically 10-40, in the 22 November 2026 runs.
- Item goals are limited to currently visible candidates; a bot does not know
  a hidden pickup route.
- Aim lead uses the current weapon's primary projectile speed in a straight
  line. It does not allow for gravity arcs (thrown crawler grenades), and bots
  never use alternate fire.
- Personality content can make combat and communication distinct, but cannot
  compensate for unreachable geometry.
- The roster intentionally stops at retail-selectable model slot 18.

The implementation history and acceptance gates are recorded in
[`archive/2026-10-18/plans/2026-08-02-prey-bot-characters.md`](archive/2026-10-18/plans/2026-08-02-prey-bot-characters.md).
