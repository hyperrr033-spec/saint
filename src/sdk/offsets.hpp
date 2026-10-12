#pragma once
#include "../core/common.hpp"

namespace lacz::off {

struct netvar_holder {
    // client.dll netvars/schema fields
    u32 m_iHealth = 0;
    u32 m_iTeamNum = 0;
    u32 m_iAccount = 0;
    u32 m_lifeState = 0;
    u32 m_vecAbsOrigin = 0;
    u32 m_vecOrigin = 0;
    u32 m_angEyeAngles = 0;
    u32 m_iMaxHealth = 0;
    u32 m_bHasHelmet = 0;
    u32 m_ArmorValue = 0;
    u32 m_bIsScoped = 0;
    u32 m_bGunGameImmunity = 0;
    u32 m_fFlags = 0;
    u32 m_iShotsFired = 0;
    u32 m_aimPunchAngle = 0;
    u32 m_aimPunchTickFraction = 0;
    u32 m_flFlashDuration = 0;
    u32 m_flFlashMaxAlpha = 0;
    u32 m_iFOV = 0;
    u32 m_iFOVStart = 0;
    u32 m_bSpotted = 0;
    u32 m_bSpottedByMask = 0;
    u32 m_hActiveWeapon = 0;
    u32 m_hMyWeapons = 0;
    u32 m_hMyWearables = 0;
    u32 m_hController = 0;
    u32 m_hPawn = 0;
    u32 m_hOwnerEntity = 0;
    u32 m_bDormant = 0;
    u32 m_bBombTicking = 0;
    u32 m_flC4Blow = 0;
    u32 m_flTimerLength = 0;
    u32 m_bBombDefused = 0;
    u32 m_szLastPlaceName = 0;
    u32 m_pGameSceneNode = 0;
    u32 m_pMovementServices = 0;
    u32 m_pWeaponServices = 0;
    u32 m_pObserverServices = 0;
    u32 m_pCameraServices = 0;
    u32 m_pItemServices = 0;
    u32 m_pClippingWeapon = 0;
    u32 m_bWaitForNoAttack = 0;
    u32 m_iItemDefinitionIndex = 0;
    u32 m_iEntityQuality = 0;
    u32 m_iItemIDHigh = 0;
    u32 m_iItemIDLow = 0;
    u32 m_iAccountID = 0;
    u32 m_OriginalOwnerXuidLow = 0;
    u32 m_nFallbackPaintKit = 0;
    u32 m_nFallbackSeed = 0;
    u32 m_flFallbackWear = 0;
    u32 m_nFallbackStatTrak = 0;
    u32 m_szCustomName = 0;
    u32 m_iNumEmptyAttacks = 0;
    u32 m_bClientSideAnimation = 0;
    u32 m_flPoseParameter = 0;
    u32 m_nSequence = 0;
    u32 m_nHitboxSet = 0;
    u32 m_iBoneArray = 0;
    u32 m_bIsGrabbingHostage = 0;
    u32 m_bStartedArming = 0;
    u32 m_bBombPlacedAnimation = 0;
    u32 m_flSimulationTime = 0;
    u32 m_flOldSimulationTime = 0;
    u32 m_bNeedToReApplyGloves = 0;
    u32 m_bIsDefusing = 0;
    u32 m_bIsPlantingViaUse = 0;
    u32 m_pInGameMoneyServices = 0;
    u32 m_bIsLocalPlayer = 0;
    u32 m_hGroundEntity = 0;
    u32 m_bIsBuyMenuOpen = 0;
};

struct signatures {
    uptr global_vars = 0;
    uptr entity_list = 0;
    uptr view_matrix = 0;
    uptr local_player = 0;
    uptr local_pawn = 0;
    uptr game_rules = 0;
    uptr prediction = 0;
    uptr weapon_system = 0;
    uptr trace_ray = 0;
    uptr trace_filter = 0;
    uptr create_move = 0;
    uptr set_view_angles = 0;
    uptr get_view_angles = 0;
    uptr frame_stage = 0;
    uptr calc_viewmodel = 0;
    uptr update_skins = 0;
    uptr handle_buy = 0;
    uptr is_hltv = 0;
    uptr panic = 0;
    uptr hud_update = 0;
    uptr camera_think = 0;
    uptr input_accumulator = 0;
    uptr get_hitbox = 0;
    uptr get_bone = 0;
    uptr get_bone_by_name = 0;
    uptr model_info = 0;
    uptr setup_bones = 0;
    uptr clamp_bones = 0;
    uptr physics_run_think = 0;
    uptr interpolate = 0;
    uptr calc_spread = 0;
    uptr calc_inaccuracy = 0;
    uptr get_spread = 0;
    uptr get_inaccuracy = 0;
    uptr weapon_recoil = 0;
    uptr skin_changer = 0;
    uptr item_schema = 0;
    uptr weapon_info = 0;
    uptr get_hud_weapon = 0;
    uptr music_kit = 0;
    uptr hit_marker = 0;
    uptr should_skip_entity = 0;
};

inline netvar_holder netvars;
inline signatures sigs;

bool resolve();

} // namespace lacz::off
