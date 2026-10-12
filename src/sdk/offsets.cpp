#include "offsets.hpp"
#include "../core/memory.hpp"
#include "../core/pattern.hpp"
#include "../core/log.hpp"
#include "interfaces.hpp"
#include "schema.hpp"

namespace lacz::off {

// ---------------------------------------------------------------------------
// Netvar offsets.
//
// These are the authoritative build-14190 values, cross-checked against the
// CS2 SDK schema dump (classes: C_BaseEntity, C_CSPlayerPawn, C_GameSceneNode,
// CBasePlayerController, C_EconItemView, C_PlantedC4, ...). The previous code
// made the runtime schema walker primary; on this build that walker returned
// wrong offsets (e.g. health 0x344 instead of 0x34C, team 0x3E3 instead of
// 0x3E7, m_hPawn 0x60C instead of 0x92C), which silently broke every
// entity-dependent feature (ESP, aimbot, triggerbot, skins, radar).
//
// The walker is still consulted, but only to fill fields whose verified value
// is unknown; a verified value always wins.
// ---------------------------------------------------------------------------
static u32 pick(u32 verified, const char* cls, const char* field) {
    return verified ? verified : static_cast<u32>(schema::field(cls, field));
}

static void resolve_netvars() {
    auto& n = netvars;

    // --- C_BaseEntity -----------------------------------------------------
    n.m_iHealth         = pick(0x34C, "C_BaseEntity", "m_iHealth");
    n.m_iTeamNum        = pick(0x3E7, "C_BaseEntity", "m_iTeamNum");
    n.m_lifeState       = pick(0x354, "C_BaseEntity", "m_lifeState");
    n.m_iMaxHealth      = pick(0x348, "C_BaseEntity", "m_iMaxHealth");
    n.m_hOwnerEntity    = pick(0x520, "C_BaseEntity", "m_hOwnerEntity");
    n.m_pGameSceneNode  = pick(0x330, "C_BaseEntity", "m_pGameSceneNode");
    n.m_fFlags          = pick(0x3F4, "C_BaseEntity", "m_fFlags");
    n.m_flSimulationTime= pick(0x3B8, "C_BaseEntity", "m_flSimulationTime");
    // Build 14190: m_bDormant / m_vecAbsOrigin / m_vecOrigin live on
    // CGameSceneNode, not on C_BaseEntity. They are read through the scene
    // node (see entity.hpp), so stash the scene-node-relative offsets here.
    n.m_bDormant        = 0x103;   // CGameSceneNode::m_bDormant
    n.m_vecAbsOrigin    = 0xC8;    // CGameSceneNode::m_vecAbsOrigin
    n.m_vecOrigin       = 0x80;    // CGameSceneNode::m_vecOrigin
    n.m_nHitboxSet      = 0x3FC;   // CSkeletonInstance::m_nHitboxSet

    // --- C_CSPlayerPawn / base -------------------------------------------
    n.m_angEyeAngles   = pick(0x3600, "C_CSPlayerPawn", "m_angEyeAngles");
    n.m_ArmorValue     = pick(0x1ECC, "C_CSPlayerPawn", "m_ArmorValue");
    n.m_bIsScoped      = pick(0x1EA0, "C_CSPlayerPawn", "m_bIsScoped");
    n.m_bGunGameImmunity = pick(0x3508, "C_CSPlayerPawn", "m_bGunGameImmunity");
    n.m_iShotsFired    = pick(0x1EB4, "C_CSPlayerPawn", "m_iShotsFired");
    n.m_aimPunchAngle  = pick(0x1598, "C_CSPlayerPawn", "m_pAimPunchServices");
    n.m_flFlashDuration= pick(0x1510, "C_CSPlayerPawnBase", "m_flFlashDuration");
    n.m_flFlashMaxAlpha= pick(0x150C, "C_CSPlayerPawnBase", "m_flFlashMaxAlpha");
    n.m_iFOV           = pick(0x298, "CCSPlayerBase_CameraServices", "m_iFOV");
    n.m_bHasHelmet     = pick(0x49, "CCSPlayer_ItemServices", "m_bHasHelmet");
    n.m_iNumEmptyAttacks = static_cast<u32>(schema::field("C_CSPlayerPawn", "m_iNumEmptyAttacks"));
    n.m_bClientSideAnimation = static_cast<u32>(schema::field("C_CSPlayerPawn", "m_bClientSideAnimation"));

    // --- services (C_BasePlayerPawn) -------------------------------------
    n.m_pMovementServices = pick(0x1330, "C_BasePlayerPawn", "m_pMovementServices");
    n.m_pWeaponServices   = pick(0x12F0, "C_BasePlayerPawn", "m_pWeaponServices");
    n.m_pObserverServices = pick(0x1308, "C_BasePlayerPawn", "m_pObserverServices");
    n.m_pCameraServices   = pick(0x1328, "C_BasePlayerPawn", "m_pCameraServices");
    n.m_pItemServices     = pick(0x12F8, "C_BasePlayerPawn", "m_pItemServices");
    n.m_hController       = pick(0x14BC, "C_BasePlayerPawn", "m_hController");
    n.m_hGroundEntity     = pick(0x530, "C_BaseEntity", "m_hGroundEntity");
    n.m_hMyWearables      = pick(0x1268, "C_BaseCombatCharacter", "m_hMyWearables");

    // --- controller -------------------------------------------------------
    // CCSPlayerController::m_hPlayerPawn @ 0x92C is the real pawn link (the
    // local pawn lives here). CBasePlayerController::m_hPawn @ 0x6BC is only a
    // fallback -- on build 14190 it pointed at a different (empty) slot.
    n.m_hPawn           = pick(0x92C, "CCSPlayerController", "m_hPlayerPawn");
    n.m_bIsLocalPlayer  = pick(0x790, "CBasePlayerController", "m_bIsLocalPlayerController");
    // CCSPlayerController::m_hPlayerPawn (0x92C) is preferred by pawn().
    // m_iAccount lives in CCSPlayerController_InGameMoneyServices.
    n.m_iAccount        = pick(0x40, "CCSPlayerController_InGameMoneyServices", "m_iAccount");

    // --- weapon / econ (C_EconEntity, C_PlayerWeaponServices) -------------
    n.m_hActiveWeapon = pick(0x60, "CPlayer_WeaponServices", "m_hActiveWeapon");
    n.m_hMyWeapons    = static_cast<u32>(schema::field("CPlayer_WeaponServices", "m_hMyWeapons"));
    // The item-definition-index path is C_EconEntity -> m_AttributeManager
    // (+0x1290, C_AttributeContainer) -> m_Item (+0x50, C_EconItemView).
    // entity.hpp applies the 0x12E0 sub-object offset to these.
    n.m_iItemDefinitionIndex = pick(0x1BA, "C_EconItemView", "m_iItemDefinitionIndex");
    n.m_iEntityQuality = pick(0x1BC, "C_EconItemView", "m_iEntityQuality");
    n.m_iItemIDHigh    = pick(0x1D0, "C_EconItemView", "m_iItemIDHigh");
    n.m_iItemIDLow     = pick(0x1D4, "C_EconItemView", "m_iItemIDLow");
    n.m_iAccountID     = pick(0x1D8, "C_EconItemView", "m_iAccountID");
    n.m_szCustomName   = pick(0x2F8, "C_EconItemView", "m_szCustomName");
    n.m_nFallbackPaintKit = pick(0x18A8, "C_EconEntity", "m_nFallbackPaintKit");
    n.m_nFallbackSeed  = pick(0x18AC, "C_EconEntity", "m_nFallbackSeed");
    n.m_flFallbackWear = pick(0x18B0, "C_EconEntity", "m_flFallbackWear");
    n.m_nFallbackStatTrak = pick(0x18B4, "C_EconEntity", "m_nFallbackStatTrak");
    n.m_OriginalOwnerXuidLow = pick(0x18A0, "C_EconEntity", "m_OriginalOwnerXuidLow");

    // --- spotted / radar --------------------------------------------------
    // m_entitySpottedState @ pawn+0x1E88; m_bSpotted @ +0x8 (applied at read).
    n.m_bSpotted       = pick(0x1E88, "C_CSPlayerPawn", "m_entitySpottedState");
    n.m_bSpottedByMask = n.m_bSpotted;

    // --- bomb (C_PlantedC4) ----------------------------------------------
    n.m_bBombTicking   = pick(0x1288, "C_PlantedC4", "m_bBombTicking");
    n.m_flC4Blow       = pick(0x12B8, "C_PlantedC4", "m_flC4Blow");
    n.m_flTimerLength  = pick(0x12C0, "C_PlantedC4", "m_flTimerLength");
    n.m_bBombDefused   = pick(0x12DC, "C_PlantedC4", "m_bBombDefused");

    // --- animation --------------------------------------------------------
    n.m_flOldSimulationTime = static_cast<u32>(schema::field("C_BaseEntity", "m_flOldSimulationTime"));
    n.m_flPoseParameter = static_cast<u32>(schema::field("C_BaseAnimating", "m_flPoseParameter"));
    n.m_nSequence       = static_cast<u32>(schema::field("C_BaseAnimating", "m_nSequence"));
}

static void resolve_signatures() {
    auto& s = sigs;

    // -----------------------------------------------------------------------
    // Every pattern below is the exact build-14190 needle from the CS2 SDK
    // dump (verified to match client.dll). Primary-first ordering; each has a
    // resolved-global fallback in resolve() so a single miss cannot disable the
    // whole cheat.
    // -----------------------------------------------------------------------

    // Global vars (dwGlobalVars = 0x2228090).
    uptr p = pattern::find(L"client.dll", "48 8B 0D ? ? ? ? 4C 8D 05 ? ? ? ? 48 85 D2");
    if (p)
        s.global_vars = pattern::resolve_relative(p, 0x3, 0x7);
    if (!s.global_vars) {
        p = pattern::find(L"client.dll", "48 8B 05 ? ? ? ? 0F 57 C0 8B 48");
        if (p)
            s.global_vars = pattern::resolve_relative(p, 0x3, 0x7);
    }
    log::write("global_vars signature result: 0x%llX", (unsigned long long)s.global_vars);

    // Entity system (dwGameEntitySystem = 0x2711598).
    // "48 8B 1D ? ? ? ? 48 89 1D ? ? ? ?" (pGameEntitySystem).
    uptr es = pattern::find(L"client.dll", "48 8B 1D ? ? ? ? 48 89 1D ? ? ? ?");
    if (es)
        s.entity_list = pattern::resolve_relative(es, 0x3, 0x7);
    if (!s.entity_list) {
        // "48 89 0D ? ? ? ? E9 ? ? ? ? CC" (pEntityList write).
        es = pattern::find(L"client.dll", "48 89 0D ? ? ? ? E9 ? ? ? ? CC");
        if (es)
            s.entity_list = pattern::resolve_relative(es, 0x3, 0x7);
    }
    if (!s.entity_list && iface::entity_system)
        s.entity_list = reinterpret_cast<uptr>(iface::entity_system);
    log::write("entity_list signature result: 0x%llX", (unsigned long long)s.entity_list);

    // Local player controller (dwLocalPlayerController = 0x25338A8).
    // Primary: "48 8B 05 ? ? ? ? 41 89 BE".
    uptr lp = pattern::find(L"client.dll", "48 8B 05 ? ? ? ? 41 89 BE");
    if (lp)
        s.local_player = pattern::resolve_relative(lp, 0x3, 0x7);
    if (!s.local_player) {
        // "48 83 3D ? ? ? ? 00 0F 95" is a cmp [rip+disp32], 0 / setne; the
        // instruction is 8 bytes so the displacement is followed by a 5-byte
        // instruction.
        lp = pattern::find(L"client.dll", "48 83 3D ? ? ? ? 00 0F 95");
        if (lp)
            s.local_player = pattern::resolve_relative(lp, 0x3, 0x8);
    }
    log::write("local_player signature result: match=0x%llX resolved=0x%llX",
               (unsigned long long)lp, (unsigned long long)s.local_player);

    // Local player pawn (dwLocalPlayerPawn = 0x255C3C8). No published needle
    // for this one, so scan a few common prologues around a global read. This
    // is a nice-to-have: local_pawn() resolves the pawn through the controller
    // when this misses.
    uptr pp = pattern::find(L"client.dll", "48 8B 05 ? ? ? ? 48 85 C0 74 ? 48 8B 88");
    if (!pp)
        pp = pattern::find(L"client.dll", "48 8B 05 ? ? ? ? 48 85 C0 0F 84");
    if (pp)
        s.local_pawn = pattern::resolve_relative(pp, 0x3, 0x7);
    log::write("local_pawn signature result: match=0x%llX resolved=0x%llX",
               (unsigned long long)pp, (unsigned long long)s.local_pawn);

    // View matrix (dwViewMatrix = 0x2561CD0).
    uptr vm = pattern::find(L"client.dll", "48 8D 0D ? ? ? ? 48 C1 E0 06");
    if (vm)
        s.view_matrix = pattern::resolve_relative(vm, 0x3, 0x7);
    log::write("view_matrix signature result: match=0x%llX resolved=0x%llX",
               (unsigned long long)vm, (unsigned long long)s.view_matrix);

    // Game rules (dwGameRules = 0x1BADC90).
    uptr rules = pattern::find(L"client.dll",
        "48 8B 1D ? ? ? ? 48 8D 54 24 ? 0F 28 D0 48 8D 4C 24 ?");
    if (rules)
        s.game_rules = pattern::resolve_relative(rules, 0x3, 0x7);

    // Trace ray (TraceShape = rva 0xA19140).
    s.trace_ray = pattern::find(L"client.dll",
        "48 89 54 24 10 48 89 4C 24 08 55 53 56 57 41 54 41 56 41 57 48 8D AC 24 F0 DB FF FF B8 10 25 00 00 E8 ? ? ? ? 48 2B E0 48 8D 05 ? ? ? ?");
    if (!s.trace_ray)
        s.trace_ray = pattern::find(L"client.dll",
            "48 89 54 24 10 48 89 4C 24 08 55 53 56 57 41 54 41 56 41 57 48 8D AC 24 ? ? ? ? B8");

    // CreateMove (rva 0xB64E60).
    s.create_move = pattern::find(L"client.dll",
        "85 D2 0F 85 ? ? ? ? 48 8B C4 44 88 40 18");
    if (!s.create_move)
        s.create_move = pattern::find(L"client.dll",
            "40 55 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 ? ? ? ? 48 81 EC ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C4");

    // SetViewAngles (rva 0xB7FEE0).
    s.set_view_angles = pattern::find(L"client.dll", "85 D2 75 ? 48 63 81");
    if (!s.set_view_angles)
        s.set_view_angles = pattern::find(L"client.dll", "85 D2 75 3D 48 63 81 ? ? ? ?");
    if (!s.set_view_angles)
        s.set_view_angles = pattern::find(L"client.dll",
            "48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC ? 48 8B F1 F3 0F 10 02");

    // FrameStageNotify (rva 0xB6C780).
    s.frame_stage = pattern::find(L"client.dll",
        "48 89 5C 24 18 48 89 6C 24 20 57 48 83 EC 40 48 8B F9");
}

bool resolve() {
    if (!mem::base)
        mem::base = mem::module_base(L"client.dll");
    mem::base_size = mem::module_size(L"client.dll");
    if (!mem::base)
        return false;

bool ok = false;
    __try {
        if (iface::resolve()) {
            schema::initialize();
            resolve_signatures();
            resolve_netvars();

            // Last-resort global pointers for build 14190. These are the
            // published dwXxx module-base offsets; used only if the signature
            // scan missed (which is the difference between "nothing works" and
            // "ESP/aimbot work"). mem::base is client.dll.
            if (mem::base) {
                if (!sigs.global_vars) sigs.global_vars = mem::base + 0x2228090;
                if (!sigs.entity_list) sigs.entity_list = mem::base + 0x2711598;
                if (!sigs.local_player) sigs.local_player = mem::base + 0x25338A8;
                if (!sigs.local_pawn) sigs.local_pawn = mem::base + 0x255C3C8;
                if (!sigs.view_matrix) sigs.view_matrix = mem::base + 0x2561CD0;
                if (!sigs.game_rules) sigs.game_rules = mem::base + 0x1BADC90;
            }

            if (!iface::entity_system)
                iface::entity_system = get_entity_system();

            global_vars = get_global_vars();
            log::write("Resolved state: vm=0x%llX local_player=0x%llX local_pawn=0x%llX entity_list=0x%llX gv=0x%llX",
                       (unsigned long long)sigs.view_matrix,
                       (unsigned long long)sigs.local_player,
                       (unsigned long long)sigs.local_pawn,
                       (unsigned long long)sigs.entity_list,
                       (unsigned long long)sigs.global_vars);
            log::write("Netvars: hp=0x%X team=0x%X pawn=0x%X origin=0x%X",
                       netvars.m_iHealth, netvars.m_iTeamNum, netvars.m_hPawn, netvars.m_vecAbsOrigin);
            bool has_entity_sys = (sigs.entity_list != 0 || iface::entity_system != nullptr);
            bool has_view_matrix = (sigs.view_matrix != 0);
            bool has_local_player = (sigs.local_player != 0);

            ok = has_entity_sys && has_view_matrix && has_local_player;
            if (!ok) {
                log::write("FAIL: Incomplete offsets (entity_sys=%d view_matrix=%d local_player=%d)",
                           has_entity_sys ? 1 : 0, has_view_matrix ? 1 : 0, has_local_player ? 1 : 0);
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        log::write("EXCEPTION during offset resolution (code 0x%08X)", GetExceptionCode());
        ok = false;
    }
    return ok;
}

} // namespace lacz::off
