/*
===========================================================================

openPR06 weapon wheel.

Holding _weaponWheel (Q, a controller's LB) opens a ring of Tommy's seven
weapons; pointing at one and letting go equips it. The ring is the retained
document guis/hud/weapon_wheel.rui (tools/ui/build_retained_weapon_wheel.py,
docs/dev/prey-ui-visual-design.md section 12.1). The behaviour follows
openQ4's weapon wheel (its docs/dev/ui/weapon-wheels.md), written here for
Prey's player:

- A tap shorter than WHEEL_TAP_MSEC swaps back to the previous weapon without
  drawing the wheel, softening the view or slowing the game.
- The mouse steers a selector dot; a controller's stick and a touch finger
  point by direction past a dead zone; a tracked VR hand turns it. The
  slot changes with a few degrees of hysteresis, and pointing at a weapon
  Tommy does not carry keeps the current choice.
- While it is open the weapon controls choose on it: a slot key picks its
  slot, next and previous step to the next carried weapon.
- Letting go equips the choice through the ordinary weapon impulse, so a
  multiplayer client predicts it and the server hears it as before. An
  empty weapon is refused. Escape, a controller's Back and a second press of
  the wheel control cancel and keep the current weapon; a menu, a camera, a
  death or spirit walk taking over dismisses it until the control is let go.
- In single player the game slows while the wheel shows; in multiplayer it
  does not, and the player can still move.
- The world behind the view weapon goes out of focus (hhPlayerView,
  idRenderSystem::SetViewDepthOfField with VIEW_DOF_FOCUS_VIEW_WEAPON).

Every presentation value here is local and never saved or sent. What the
held control does to the usercmd (the masked buttons, the held view, the
weapon impulses taken by the wheel) is decided from the usercmd itself, so
a server and its clients agree.

===========================================================================
*/

#include "../idlib/precompiled.h"
#pragma hdrstop

#include "prey_local.h"

static const char *const	WHEEL_GUI				= "guis/hud/weapon_wheel.rui";
static const int			WHEEL_SLOTS				= 7;		// def_weapon1..7, wrench to launcher
static const int			WHEEL_TAP_MSEC			= 180;
static const float			WHEEL_CURSOR_RADIUS		= 118.0f;	// the selector's travel, in its own units
static const float			WHEEL_DOCUMENT_RIM		= 228.0f;	// the tiles' outer edge in the document, dp
static const float			WHEEL_MOUSE_SCALE		= 0.70f;	// selector units per mouse count
static const float			WHEEL_DEADZONE_POINTER	= 32.0f;
static const float			WHEEL_DEADZONE_CLASSIC	= 24.0f;
static const float			WHEEL_DEADZONE_STICK	= 0.35f;	// of the selector's travel
static const float			WHEEL_HYSTERESIS		= DEG2RAD( 4.0f );
static const float			WHEEL_TIME_SCALE		= 0.18f;	// single player, fully open
static const float			WHEEL_BLEND_IN			= 8.0f;		// per second
static const float			WHEEL_BLEND_OUT			= 6.25f;
static const float			WHEEL_BLEND_REDUCED		= 12.5f;	// both ways under reduced motion
static const float			WHEEL_DOCUMENT_HALF		= 280.0f;	// the document's 560 dp square
static const float			WHEEL_DOCUMENT_REACH	= 264.0f;	// its widest art from the centre, dp
static const float			WHEEL_CANVAS_HEIGHT		= 720.0f;

static float WheelLerp( float a, float b, float t ) {
	return a + ( b - a ) * t;
}

static float WheelNormalize( float angle ) {
	while ( angle > idMath::PI ) {
		angle -= idMath::TWO_PI;
	}
	while ( angle < -idMath::PI ) {
		angle += idMath::TWO_PI;
	}
	return angle;
}

static bool WheelCVarBool( const char *name ) {
	const idCVar *cvar = cvarSystem->Find( name );
	return cvar != NULL && cvar->GetBool();
}

static float WheelCVarFloat( const char *name, float fallback ) {
	const idCVar *cvar = cvarSystem->Find( name );
	return cvar != NULL ? cvar->GetFloat() : fallback;
}

// The selector points by direction (a stick or a finger) rather than moving
// a dot the player steers (the mouse, or Classic's look-driven cursor).
static bool WheelPointsByDirection( const usercmd_t &cmd ) {
	if ( WheelCVarBool( "ui_radialTouchActive" ) ) {
		return true;
	}
	return ( cmd.buttons & BUTTON_VRAIM ) == 0 && WheelCVarBool( "ui_retained" ) && WheelCVarBool( "ui_radialStickActive" );
}

static float WheelDeadzone( const usercmd_t &cmd ) {
	if ( WheelCVarBool( "ui_radialTouchActive" ) ) {
		return WHEEL_DEADZONE_POINTER;
	}
	if ( !WheelCVarBool( "ui_retained" ) ) {
		return WHEEL_DEADZONE_CLASSIC;
	}
	return WheelCVarBool( "ui_radialStickActive" ) ? WHEEL_CURSOR_RADIUS * WHEEL_DEADZONE_STICK : WHEEL_DEADZONE_POINTER;
}

static void WheelBoundCursor( idVec2 &cursor ) {
	// Held on the rim, so reversing starts back at once.
	const float length = cursor.Length();
	if ( length > WHEEL_CURSOR_RADIUS ) {
		cursor *= WHEEL_CURSOR_RADIUS / length;
	}
}

/*
================
hhPlayer::IsWeaponWheelImpulse

The weapon controls that choose on an open wheel: a slot key, next, previous.
================
*/
bool hhPlayer::IsWeaponWheelImpulse( int impulse ) {
	return ( impulse >= IMPULSE_1 && impulse <= IMPULSE_12 ) || impulse == IMPULSE_14 || impulse == IMPULSE_15;
}

void hhPlayer::InitWeaponWheel( void ) {
	wheelActive = wheelCanceled = wheelDebug = wheelDebugHeld = wheelGuiOpen = wheelGuiFailed = wheelTimeScaled = false;
	wheelSlot = wheelGuiSlot = -1;
	wheelHeldWeapon = wheelPreviousWeapon = -1;
	wheelOpenedTime = wheelLastTime = 0;
	wheelLastMouseX = wheelLastMouseY = 0;
	wheelBlend = 0.0f;
	wheelCursor.Zero();
	wheelLastAngles.Zero();
	wheelGui = NULL;
}

/*
================
hhPlayer::PrecacheWeaponWheelMedia

The retained runtime's name for each picture. Resolved while a level load is
the precache window: in multiplayer the local player spawns after that window
closes, and loading them then warns and parses the wheel on the first frames.
================
*/
void hhPlayer::PrecacheWeaponWheelMedia( void ) {
	static const char *const icons[WHEEL_SLOTS] = {
		"wrench", "rifle", "sw_crawler", "soulstripper", "autocannon", "acidsprayer", "rocketlauncher"
	};
	for ( int i = 0; i < WHEEL_SLOTS; i++ ) {
		declManager->FindMaterial( va( "_retained/textures/interface/icons/%s", icons[i] ) );
	}
}

/*
================
hhPlayer::PrecacheWeaponWheel

The local player's wheel and its icons load as the player spawns. In single
player that is still inside the level load. In multiplayer the icons were
already resolved from the map load, and this attaches the local player's copy.
================
*/
void hhPlayer::PrecacheWeaponWheel( void ) {
	if ( gameLocal.localClientNum < 0 || entityNumber != gameLocal.localClientNum ) {
		return;
	}
	PrecacheWeaponWheelMedia();
	if ( wheelGui == NULL && !wheelGuiFailed ) {
		wheelGui = uiManager->FindGui( WHEEL_GUI, true, true );
		wheelGuiFailed = wheelGui == NULL;
		wheelGuiOpen = false;
		wheelGuiSlot = -1;
	}
}

void hhPlayer::FreeWeaponWheelGui( void ) {
	if ( wheelGui != NULL ) {
		uiManager->DeAlloc( wheelGui );
	}
	wheelGui = NULL;
	wheelGuiOpen = false;
	wheelGuiSlot = -1;
}

/*
================
hhPlayer::WeaponWheelAllowed

Whether the held control acts as a wheel at all. Built from replicated state
only, so the server and every client decide the same way for one usercmd.
================
*/
bool hhPlayer::WeaponWheelAllowed( void ) {
	if ( spectating || health <= 0 || IsDead() || !weaponEnabled || hiddenWeapon || bFrozen ) {
		return false;
	}
	if ( InVehicle() || IsSpiritOrDeathwalking() || IsPossessed() ) {
		return false;	// spirit walk holds the bow alone; the shuttle has its own weapons
	}
	if ( gameLocal.inCinematic || InCinematic() || privateCameraView || gameLocal.GetCamera() != NULL ) {
		return false;
	}
	if ( weaponFlags == 0 || gameLocal.world == NULL || gameLocal.world->spawnArgs.GetBool( "no_Weapons" ) ) {
		return false;
	}
	return true;
}

bool hhPlayer::WeaponWheelHeld( void ) {
	return ( usercmd.buttons & BUTTON_WEAPONWHEEL ) != 0 && WeaponWheelAllowed();
}

/*
================
hhPlayer::WeaponWheelHoldsView

The view stays where it was while the control is held: the mouse and stick
drive the selector instead. A tracked VR hand turns the selector with the
hand, so the head keeps the view.
================
*/
bool hhPlayer::WeaponWheelHoldsView( void ) {
	return WeaponWheelHeld() && ( usercmd.buttons & BUTTON_VRAIM ) == 0;
}

/*
================
hhPlayer::ApplyWeaponWheelInputMask

The weapon stays raised but cannot fire, zoom or open the scores while the
control is held; a button already down when the wheel closes waits for its
release. Single player also stands still.
================
*/
void hhPlayer::ApplyWeaponWheelInputMask( void ) {
	if ( !WeaponWheelHeld() ) {
		return;
	}
	const int masked = BUTTON_ATTACK | BUTTON_ATTACK_ALT | BUTTON_ZOOM | BUTTON_SCORES;
	buttonMask |= usercmd.buttons & masked;
	usercmd.buttons &= ~masked;
	if ( !gameLocal.isMultiplayer ) {
		usercmd.forwardmove = 0;
		usercmd.rightmove = 0;
		usercmd.upmove = 0;
	}
}

/*
================
hhPlayer::WeaponWheelTakesImpulse

A weapon control pressed while the wheel's control is held belongs to the
wheel on every end: the local player's wheel chooses with it, and nothing
switches weapons until the release equips the choice.
================
*/
bool hhPlayer::WeaponWheelTakesImpulse( int impulse ) {
	if ( !IsWeaponWheelImpulse( impulse ) || !WeaponWheelHeld() ) {
		return false;
	}
	HandleWeaponWheelImpulse( impulse );
	return true;
}

int hhPlayer::WeaponWheelSlotWeapon( int slot ) const {
	return ( slot >= 0 && slot < WHEEL_SLOTS ) ? slot + 1 : -1;
}

int hhPlayer::WeaponWheelWeaponSlot( int weaponNum ) const {
	return ( weaponNum >= 1 && weaponNum <= WHEEL_SLOTS ) ? weaponNum - 1 : -1;
}

bool hhPlayer::WeaponWheelSlotOwned( int slot ) {
	const int weaponNum = WeaponWheelSlotWeapon( slot );
	if ( weaponNum < 0 || ( inventory.weapons & ( 1 << weaponNum ) ) == 0 || IsLocked( weaponNum ) ) {
		return false;
	}
	return spawnArgs.GetString( va( "def_weapon%d", weaponNum ) )[0] != '\0';
}

/*
================
hhPlayer::GetWeaponWheelAmmo

A slot's ammunition, as the weapon bar reads it (UpdateHudAmmo).
================
*/
void hhPlayer::GetWeaponWheelAmmo( int slot, weaponWheelAmmo_t &ammo ) {
	memset( &ammo, 0, sizeof( ammo ) );
	const int weaponNum = WeaponWheelSlotWeapon( slot );
	if ( weaponNum < 0 ) {
		return;
	}
	const weaponInfo_t &primary = weaponInfo[weaponNum];
	const weaponInfo_t &secondary = altWeaponInfo[weaponNum];
	const int type = primary.ammoType;
	const int altType = secondary.ammoType;
	ammo.infinite = type <= 1;			// none, or spirit power, which the bar does not show
	ammo.count = type > 1 ? inventory.ammo[type] : 0;
	ammo.altCount = altType > 1 ? inventory.ammo[altType] : 0;
	ammo.hasAlt = altType > 1 && altType != type;
	ammo.fill = primary.ammoMax > 0 ? idMath::ClampFloat( 0.0f, 1.0f, static_cast<float>( ammo.count ) / primary.ammoMax ) : 0.0f;
	ammo.altFill = ammo.hasAlt && secondary.ammoMax > 0 ? idMath::ClampFloat( 0.0f, 1.0f, static_cast<float>( ammo.altCount ) / secondary.ammoMax ) : 0.0f;
	ammo.low = ammo.count > 0 && ammo.count <= primary.ammoLow;
	ammo.altLow = ammo.hasAlt && ammo.altCount > 0 && ammo.altCount <= secondary.ammoLow;
	ammo.empty = !ammo.infinite && ammo.count == 0 && ammo.altCount == 0;
}

int hhPlayer::GetWeaponWheelDefaultSlot( void ) {
	const int current = WeaponWheelWeaponSlot( GetCurrentWeapon() );
	if ( WeaponWheelSlotOwned( current ) ) {
		return current;
	}
	for ( int slot = 0; slot < WHEEL_SLOTS; slot++ ) {
		if ( WeaponWheelSlotOwned( slot ) ) {
			return slot;
		}
	}
	return -1;
}

/*
================
hhPlayer::GetWeaponWheelSlotAtAngle

Each weapon keeps its place on the ring: pointing at one Tommy does not carry
holds the current choice, and the current choice holds a few degrees past
its edges so the selection does not flicker on a boundary.
================
*/
int hhPlayer::GetWeaponWheelSlotAtAngle( float angle ) {
	const float slice = idMath::TWO_PI / WHEEL_SLOTS;
	const int slot = static_cast<int>( angle / slice + 0.5f ) % WHEEL_SLOTS;
	if ( !WeaponWheelSlotOwned( slot ) ) {
		return wheelSlot;
	}
	if ( WeaponWheelSlotOwned( wheelSlot ) ) {
		const float delta = idMath::Fabs( WheelNormalize( angle - wheelSlot * slice ) );
		if ( delta <= slice * 0.5f + WHEEL_HYSTERESIS ) {
			return wheelSlot;
		}
	}
	return slot;
}

/*
================
hhPlayer::HandleWeaponWheelImpulse

A weapon control while the wheel is open: a slot key picks its slot, next and
previous step to the next carried weapon clockwise or back. A control for a
weapon Tommy does not carry is used up and leaves the choice.
================
*/
bool hhPlayer::HandleWeaponWheelImpulse( int impulse ) {
	if ( !wheelActive || entityNumber != gameLocal.localClientNum ) {
		return false;
	}
	int slot = -1;
	if ( impulse >= IMPULSE_1 && impulse <= IMPULSE_12 ) {
		slot = WeaponWheelWeaponSlot( impulse );
	} else if ( impulse == IMPULSE_14 || impulse == IMPULSE_15 ) {
		const int step = impulse == IMPULSE_14 ? 1 : WHEEL_SLOTS - 1;
		const int from = wheelSlot >= 0 ? wheelSlot : 0;
		for ( int i = 1; i <= WHEEL_SLOTS; i++ ) {
			const int candidate = ( from + step * i ) % WHEEL_SLOTS;
			if ( WeaponWheelSlotOwned( candidate ) ) {
				slot = candidate;
				break;
			}
		}
	} else {
		return false;
	}
	if ( !WeaponWheelSlotOwned( slot ) ) {
		return true;
	}
	wheelSlot = slot;
	// A selector out on the ring turns to the choice, or its angle would
	// take the selection straight back; one in the dead zone stays there.
	const float reach = wheelCursor.Length();
	if ( reach > WheelDeadzone( usercmd ) ) {
		const float angle = slot * idMath::TWO_PI / WHEEL_SLOTS;
		wheelCursor.Set( idMath::Sin( angle ) * reach, -idMath::Cos( angle ) * reach );
	}
	// A choice made with a control is deliberate: show the wheel at once,
	// and let the release equip it rather than count as a tap.
	wheelOpenedTime = Min( wheelOpenedTime, sys->Milliseconds() - WHEEL_TAP_MSEC );
	return true;
}

/*
================
hhPlayer::AdvanceWeaponWheelCursor

The mouse moves the selector; a stick or a finger sets it from its
deflection; a tracked hand and Classic (ui_retained 0) move it by the
command's look angles, at in_weaponWheelSensitivity.
================
*/
void hhPlayer::AdvanceWeaponWheelCursor( void ) {
	const int mouseDx = usercmd.mx - wheelLastMouseX;
	const int mouseDy = usercmd.my - wheelLastMouseY;
	idAngles angles;
	for ( int axis = 0; axis < 3; axis++ ) {
		angles[axis] = SHORT2ANGLE( usercmd.angles[axis] );
	}
	const float lookDx = idMath::AngleNormalize180( angles.yaw - wheelLastAngles.yaw );
	const float lookDy = idMath::AngleNormalize180( angles.pitch - wheelLastAngles.pitch );
	const bool trackedAim = ( usercmd.buttons & BUTTON_VRAIM ) != 0;
	wheelLastMouseX = usercmd.mx;
	wheelLastMouseY = usercmd.my;
	wheelLastAngles = angles;

	wheelCursor.x += mouseDx * WHEEL_MOUSE_SCALE;
	wheelCursor.y += mouseDy * WHEEL_MOUSE_SCALE;
	if ( WheelCVarBool( "ui_radialTouchActive" ) ) {
		wheelCursor.x = WheelCVarFloat( "ui_radialTouchX", 0.0f ) * WHEEL_CURSOR_RADIUS;
		wheelCursor.y = WheelCVarFloat( "ui_radialTouchY", 0.0f ) * WHEEL_CURSOR_RADIUS;
	} else if ( mouseDx == 0 && mouseDy == 0 && WheelPointsByDirection( usercmd ) ) {
		wheelCursor.x = WheelCVarFloat( "ui_radialStickX", 0.0f ) * WHEEL_CURSOR_RADIUS;
		wheelCursor.y = WheelCVarFloat( "ui_radialStickY", 0.0f ) * WHEEL_CURSOR_RADIUS;
	} else if ( mouseDx == 0 && mouseDy == 0 && ( trackedAim || !WheelCVarBool( "ui_retained" ) ) ) {
		const float sensitivity = idMath::ClampFloat( 0.25f, 16.0f, WheelCVarFloat( "in_weaponWheelSensitivity", 4.0f ) );
		// Turning right lowers the yaw; the selector's x grows to the right.
		wheelCursor.x -= lookDx * sensitivity;
		wheelCursor.y += lookDy * sensitivity;
	}
	WheelBoundCursor( wheelCursor );
}

void hhPlayer::ResetWeaponWheel( bool instant ) {
	wheelActive = false;
	wheelCanceled = ( usercmd.buttons & BUTTON_WEAPONWHEEL ) != 0;
	wheelDebug = false;
	wheelSlot = GetWeaponWheelDefaultSlot();
	wheelCursor.Zero();
	wheelLastMouseX = usercmd.mx;
	wheelLastMouseY = usercmd.my;
	for ( int axis = 0; axis < 3; axis++ ) {
		wheelLastAngles[axis] = SHORT2ANGLE( usercmd.angles[axis] );
	}
	wheelLastTime = sys->Milliseconds();
	if ( entityNumber == gameLocal.localClientNum && cvarSystem->GetCVarInteger( "ui_radialOwner" ) == 1 ) {
		cvarSystem->SetCVarInteger( "ui_radialOwner", 0 );
		cvarSystem->SetCVarBool( "ui_radialCancel", false );
	}
	if ( instant ) {
		wheelBlend = 0.0f;
		if ( wheelGui != NULL && wheelGuiOpen ) {
			wheelGui->HandleNamedEvent( "close" );
		}
		wheelGuiOpen = false;
		UpdateWeaponWheelEffects();
	}
}

/*
================
hhPlayer::CancelWeaponWheel

Escape: close the wheel and keep the current weapon. True when it was open.
================
*/
bool hhPlayer::CancelWeaponWheel( void ) {
	if ( !wheelActive && wheelBlend <= 0.0f ) {
		return false;
	}
	ResetWeaponWheel( false );
	return true;
}

float hhPlayer::GetWeaponWheelBlend( void ) const {
	return entityNumber == gameLocal.localClientNum ? idMath::ClampFloat( 0.0f, 1.0f, wheelBlend ) : 0.0f;
}

/*
================
hhPlayer::UpdateWeaponWheelEffects

Single player slows while the wheel shows. The view's depth of field is
hhPlayerView's, from GetWeaponWheelBlend.
================
*/
void hhPlayer::UpdateWeaponWheelEffects( void ) {
	if ( gameLocal.isMultiplayer || entityNumber != gameLocal.localClientNum ) {
		return;
	}
	const float blend = idMath::ClampFloat( 0.0f, 1.0f, wheelBlend );
	if ( blend > 0.0f ) {
		common->SetGameTimeScale( WheelLerp( 1.0f, WHEEL_TIME_SCALE, blend * blend * ( 3.0f - 2.0f * blend ) ) );
		wheelTimeScaled = true;
	} else if ( wheelTimeScaled ) {
		common->SetGameTimeScale( 1.0f );
		wheelTimeScaled = false;
	}
}

/*
================
hhPlayer::UpdateWeaponWheel

The local player's wheel, once a game frame (never on a prediction rerun).
Its times are sys->Milliseconds, real time, so the wheel keeps its own pace
while single player slows.
================
*/
void hhPlayer::UpdateWeaponWheel( void ) {
	if ( entityNumber != gameLocal.localClientNum ) {
		return;
	}
	// Prey keeps no previous weapon, so the wheel follows the weapon in hand:
	// the last of its own weapons before this one is the tap's choice and the
	// notch. Spirit walk's bow and the shuttle leave it alone.
	const int inHand = GetCurrentWeapon();
	if ( inHand != wheelHeldWeapon && WeaponWheelWeaponSlot( inHand ) >= 0 ) {
		if ( WeaponWheelWeaponSlot( wheelHeldWeapon ) >= 0 ) {
			wheelPreviousWeapon = wheelHeldWeapon;
		}
		wheelHeldWeapon = inHand;
	}
	if ( wheelActive && cvarSystem->GetCVarBool( "ui_radialCancel" ) ) {
		ResetWeaponWheel( false );
		return;
	}
	const bool canUse = WeaponWheelAllowed() && ActiveGui() == NULL;
	const bool requested = wheelDebug ? wheelDebugHeld : ( usercmd.buttons & BUTTON_WEAPONWHEEL ) != 0;
	if ( !requested ) {
		wheelCanceled = false;
	}
	const bool held = canUse && requested && !wheelCanceled;
	const bool wasBlended = wheelBlend > 0.0f;
	const int now = sys->Milliseconds();
	const bool reduced = WheelCVarBool( "ui_retainedReducedMotion" );
	const float blendIn = reduced ? WHEEL_BLEND_REDUCED : WHEEL_BLEND_IN;
	const float blendOut = reduced ? WHEEL_BLEND_REDUCED : WHEEL_BLEND_OUT;
	if ( wheelLastTime == 0 ) {
		wheelLastTime = now;
	}
	const float seconds = MS2SEC( idMath::ClampInt( 0, 100, now - wheelLastTime ) );
	wheelLastTime = now;

	if ( !canUse ) {
		// A menu, a camera, a death or spirit walk dismisses this opening: the
		// same held control does not reopen it as the wheel fades.
		if ( wheelActive && requested ) {
			wheelCanceled = true;
		}
		wheelActive = false;
		if ( cvarSystem->GetCVarInteger( "ui_radialOwner" ) == 1 ) {
			cvarSystem->SetCVarInteger( "ui_radialOwner", 0 );
		}
		wheelBlend = Max( 0.0f, wheelBlend - seconds * blendOut );
		if ( wheelBlend > 0.0f || wasBlended ) {
			UpdateWeaponWheelEffects();
		}
		if ( wheelBlend <= 0.0f && wasBlended ) {
			ResetWeaponWheel( true );
		}
		return;
	}

	if ( held && !wheelActive ) {
		wheelActive = true;
		// Touch qualified its hold before it set the button; its release is
		// never a tap.
		wheelOpenedTime = now - ( WheelCVarBool( "ui_radialTouchActive" ) ? WHEEL_TAP_MSEC : 0 );
		wheelCursor.Zero();
		wheelLastMouseX = usercmd.mx;
		wheelLastMouseY = usercmd.my;
		for ( int axis = 0; axis < 3; axis++ ) {
			wheelLastAngles[axis] = SHORT2ANGLE( usercmd.angles[axis] );
		}
		wheelSlot = GetWeaponWheelDefaultSlot();
		if ( wheelGui == NULL ) {
			wheelGuiSlot = -1;
		}
	}

	if ( wheelActive ) {
		if ( !wheelDebug ) {
			AdvanceWeaponWheelCursor();
		}
		if ( wheelCursor.Length() > WheelDeadzone( usercmd ) ) {
			float angle = atan2f( wheelCursor.x, -wheelCursor.y );
			if ( angle < 0.0f ) {
				angle += idMath::TWO_PI;
			}
			wheelSlot = GetWeaponWheelSlotAtAngle( angle );
		} else if ( wheelSlot < 0 ) {
			wheelSlot = GetWeaponWheelDefaultSlot();
		}
	} else {
		wheelLastMouseX = usercmd.mx;
		wheelLastMouseY = usercmd.my;
	}

	if ( !held && wheelActive ) {
		const bool tap = now - wheelOpenedTime < WHEEL_TAP_MSEC && !wheelDebug;
		const int weaponNum = tap ? wheelPreviousWeapon : WeaponWheelSlotWeapon( wheelSlot );
		const int slot = WeaponWheelWeaponSlot( weaponNum );
		wheelActive = false;
		wheelCursor.Zero();
		if ( WeaponWheelSlotOwned( slot ) && weaponNum != GetCurrentWeapon() && weaponNum != GetIdealWeapon() ) {
			weaponWheelAmmo_t ammo;
			GetWeaponWheelAmmo( slot, ammo );
			if ( !ammo.empty ) {
				// The ordinary weapon impulse: a multiplayer client predicts it
				// and sends it to the server as a weapon key would.
				PerformImpulse( weaponNum );
			}
		}
	}

	// Delay the presentation, not the selection: a tap swaps back without the
	// wheel flashing up or the game slowing.
	const bool showWheel = held && ( wheelDebug || now - wheelOpenedTime >= WHEEL_TAP_MSEC );
	const float target = showWheel ? 1.0f : 0.0f;
	if ( wheelBlend < target ) {
		wheelBlend = Min( 1.0f, wheelBlend + seconds * blendIn );
	} else if ( wheelBlend > target ) {
		wheelBlend = Max( 0.0f, wheelBlend - seconds * blendOut );
	}
	if ( wheelBlend > 0.0f || wasBlended ) {
		UpdateWeaponWheelEffects();
	}
	if ( wheelActive ) {
		cvarSystem->SetCVarInteger( "ui_radialOwner", 1 );
	} else if ( cvarSystem->GetCVarInteger( "ui_radialOwner" ) == 1 ) {
		cvarSystem->SetCVarInteger( "ui_radialOwner", 0 );
	}
}

/*
================
hhPlayer::DrawWeaponWheel

Publishes the wheel's state to its document and draws it over the HUD.
================
*/
void hhPlayer::DrawWeaponWheel( void ) {
	if ( wheelBlend <= 0.0f || entityNumber != gameLocal.localClientNum ) {
		return;
	}
	if ( wheelGui == NULL ) {
		if ( wheelGuiFailed ) {
			return;
		}
		wheelGui = uiManager->FindGui( WHEEL_GUI, true, true );
		if ( wheelGui == NULL ) {
			wheelGuiFailed = true;
			gameLocal.Warning( "weapon wheel: could not load %s", WHEEL_GUI );
			return;
		}
		wheelGuiOpen = false;
		wheelGuiSlot = -1;
	}

	// The document's 720 dp canvas: the wheel sits ui_weaponWheelOffset
	// percent of the view's width from the centre, scaled with the HUD and
	// kept inside its safe inset without changing the saved preference.
	const float screenWidth = static_cast<float>( Max( 1, renderSystem->GetScreenWidth() ) );
	const float screenHeight = static_cast<float>( Max( 1, renderSystem->GetScreenHeight() ) );
	const float width = WHEEL_CANVAS_HEIGHT * screenWidth / screenHeight;
	const float inset = idMath::ClampFloat( 0.0f, 10.0f, WheelCVarFloat( "ui_hudSafeInset", 0.0f ) ) * 0.01f;
	const float marginX = Max( 16.0f, width * inset );
	const float marginY = Max( 16.0f, WHEEL_CANVAS_HEIGHT * inset );
	const float scale = idMath::ClampFloat( 0.5f, 2.0f, WheelCVarFloat( "ui_hudScale", 1.0f ) );
	const float fit = Min( scale, Max( 0.1f, Min( ( width - 2.0f * marginX ) / ( 2.0f * WHEEL_DOCUMENT_HALF ),
		( WHEEL_CANVAS_HEIGHT - 2.0f * marginY ) / ( 2.0f * WHEEL_DOCUMENT_HALF ) ) ) );
	const float reach = Max( 0.0f, width * 0.5f - WHEEL_DOCUMENT_REACH * fit - marginX );
	const float requested = width * idMath::ClampFloat( -40.0f, 40.0f, WheelCVarFloat( "ui_weaponWheelOffset", 27.0f ) ) * 0.01f;

	wheelGui->SetStateFloat( "blend", wheelBlend );
	wheelGui->SetStateFloat( "offset", idMath::ClampFloat( -reach, reach, requested ) );
	wheelGui->SetStateFloat( "fit", fit );
	wheelGui->SetStateInt( "selected", Max( 0, wheelSlot ) );
	const int previousSlot = WeaponWheelWeaponSlot( wheelPreviousWeapon );
	wheelGui->SetStateInt( "previous", WeaponWheelSlotOwned( previousSlot ) ? previousSlot : -1 );
	wheelGui->SetStateBool( "mp", gameLocal.isMultiplayer );
	wheelGui->SetStateInt( "team", gameLocal.isMultiplayer && gameLocal.gameType == GAME_TDM ? team : -1 );
	const float rim = WHEEL_DOCUMENT_RIM / WHEEL_CURSOR_RADIUS;
	wheelGui->SetStateFloat( "cursorX", wheelCursor.x * rim );
	wheelGui->SetStateFloat( "cursorY", wheelCursor.y * rim );
	// The dot shows once the mouse moves it off the hub, where the name is.
	wheelGui->SetStateBool( "cursorShown", wheelActive && !WheelPointsByDirection( usercmd ) && wheelCursor.Length() >= 1.0f );
	for ( int slot = 0; slot < WHEEL_SLOTS; slot++ ) {
		weaponWheelAmmo_t ammo;
		GetWeaponWheelAmmo( slot, ammo );
		const bool owned = WeaponWheelSlotOwned( slot );
		wheelGui->SetStateBool( va( "slot%d.owned", slot ), owned );
		wheelGui->SetStateBool( va( "slot%d.empty", slot ), owned && ammo.empty );
		wheelGui->SetStateBool( va( "slot%d.low", slot ), ammo.low );
		wheelGui->SetStateBool( va( "slot%d.altLow", slot ), ammo.altLow );
		wheelGui->SetStateFloat( va( "slot%d.fill", slot ), ammo.fill );
		wheelGui->SetStateFloat( va( "slot%d.altFill", slot ), ammo.altFill );
		wheelGui->SetStateBool( va( "slot%d.hasAlt", slot ), ammo.hasAlt );
		wheelGui->SetStateBool( va( "slot%d.infinite", slot ), ammo.infinite );
		if ( slot == wheelSlot ) {
			wheelGui->SetStateInt( "ammo", ammo.count );
			wheelGui->SetStateInt( "alt", ammo.altCount );
			wheelGui->SetStateBool( "hasAlt", ammo.hasAlt );
			wheelGui->SetStateBool( "infinite", owned && ammo.infinite );
			wheelGui->SetStateBool( "empty", owned && ammo.empty );
			wheelGui->SetStateBool( "low", ammo.low );
		}
	}
	const int now = sys->Milliseconds();
	wheelGui->StateChanged( now );
	const bool justOpened = wheelActive && !wheelGuiOpen;
	if ( wheelGuiOpen != wheelActive ) {
		wheelGui->HandleNamedEvent( wheelActive ? "open" : "close" );
		wheelGuiOpen = wheelActive;
	}
	if ( wheelGuiSlot != wheelSlot ) {
		// The opening already brings the name in; a selection in the same
		// frame would cut it short.
		if ( !justOpened ) {
			wheelGui->HandleNamedEvent( "selection" );
		}
		wheelGuiSlot = wheelSlot;
	}
	wheelGui->Redraw( now );
}

/*
================
hhPlayer::DebugWeaponWheel

testWeaponWheel (cheat): drives the local player's wheel without input.
================
*/
void hhPlayer::DebugWeaponWheel( const idCmdArgs &args ) {
	const idStr action = args.Argv( 1 );
	if ( action == "open" ) {
		wheelDebug = wheelDebugHeld = true;
		wheelCanceled = false;
		UpdateWeaponWheel();
	} else if ( action == "slot" && args.Argc() > 2 && wheelDebug ) {
		const int slot = idMath::ClampInt( 0, WHEEL_SLOTS - 1, atoi( args.Argv( 2 ) ) );
		if ( WeaponWheelSlotOwned( slot ) ) {
			wheelSlot = slot;
		}
	} else if ( action == "cursor" && args.Argc() > 3 && wheelDebug ) {
		// The selector at a point in its own units (rim 118), choosing as the
		// mouse would.
		wheelCursor.Set( static_cast<float>( atof( args.Argv( 2 ) ) ), static_cast<float>( atof( args.Argv( 3 ) ) ) );
		WheelBoundCursor( wheelCursor );
		if ( wheelCursor.Length() > WheelDeadzone( usercmd ) ) {
			float angle = atan2f( wheelCursor.x, -wheelCursor.y );
			if ( angle < 0.0f ) {
				angle += idMath::TWO_PI;
			}
			wheelSlot = GetWeaponWheelSlotAtAngle( angle );
		}
	} else if ( action == "step" && args.Argc() > 2 && wheelDebug ) {
		HandleWeaponWheelImpulse( atoi( args.Argv( 2 ) ) );
	} else if ( action == "release" && wheelDebug ) {
		wheelDebugHeld = false;
		UpdateWeaponWheel();
		wheelDebug = false;
	} else if ( action == "cancel" ) {
		ResetWeaponWheel( true );
	} else if ( action == "empty" && args.Argc() > 2 ) {
		const int weaponNum = WeaponWheelSlotWeapon( idMath::ClampInt( 0, WHEEL_SLOTS - 1, atoi( args.Argv( 2 ) ) ) );
		inventory.ammo[weaponInfo[weaponNum].ammoType] = 0;
		inventory.ammo[altWeaponInfo[weaponNum].ammoType] = 0;
	} else if ( action != "probe" && action.Length() ) {
		gameLocal.Printf( "usage: testWeaponWheel open | slot N | cursor X Y | step IMPULSE | release | cancel | empty N | probe\n" );
	}
	// gate: what holds the wheel shut, in WeaponWheelAllowed's order, then a focused GUI
	gameLocal.Printf( "WEAPON_WHEEL active=%d slot=%d current=%d ideal=%d previous=%d blend=%.2f timescale=%.3f offset=%d gui=%s "
		"gate=%d%d%d%d%d%d%d%d%d%d%d%d\n",
		wheelActive, wheelSlot, GetCurrentWeapon(), GetIdealWeapon(), wheelPreviousWeapon, wheelBlend, common->GetGameTimeScale(),
		cvarSystem->GetCVarInteger( "ui_weaponWheelOffset" ), wheelGui != NULL ? wheelGui->Name() : "none",
		spectating, health <= 0, IsDead(), !weaponEnabled, hiddenWeapon, bFrozen, InVehicle(), IsSpiritOrDeathwalking(),
		IsPossessed(), gameLocal.inCinematic || InCinematic() || privateCameraView || gameLocal.GetCamera() != NULL,
		weaponFlags == 0, ActiveGui() != NULL );
}
