#include "Body.h"

#include <CCINIClass.h>
#include <ObjectClass.h>
#include <Helpers/Macro.h>
#include <Commands/AggressiveStance.h>

std::map<WarheadTypeClass*, WarheadTypeExt::GrantInfo> WarheadTypeExt::Cache;

// Parses a comma/space separated list of house categories into an ASHouse
// flag set. Returns `fallback` if the string is empty (key absent).
static unsigned char ParseAffectsHouses(char* str, unsigned char fallback)
{
    if (!str || str[0] == '\0')
        return fallback;

    unsigned char mask = ASHouse::None;
    bool sawToken = false;

    char* context = nullptr;
    for (char* tok = strtok_s(str, " ,\t", &context); tok; tok = strtok_s(nullptr, " ,\t", &context))
    {
        sawToken = true;
        if (!_strcmpi(tok, "owner") || !_strcmpi(tok, "self"))
            mask |= ASHouse::Owner;
        else if (!_strcmpi(tok, "allies") || !_strcmpi(tok, "ally") || !_strcmpi(tok, "allied"))
            mask |= ASHouse::Allies;
        else if (!_strcmpi(tok, "enemies") || !_strcmpi(tok, "enemy"))
            mask |= ASHouse::Enemies;
        else if (!_strcmpi(tok, "neutral") || !_strcmpi(tok, "neutrals") || !_strcmpi(tok, "civilian"))
            mask |= ASHouse::Neutral;
        else if (!_strcmpi(tok, "all"))
            mask |= ASHouse::All;
        else if (!_strcmpi(tok, "none"))
            mask |= ASHouse::None;
    }

    return sawToken ? mask : fallback;
}

const WarheadTypeExt::GrantInfo& WarheadTypeExt::GetAggressiveStance(WarheadTypeClass* pWH)
{
    auto it = Cache.find(pWH);
    if (it != Cache.end())
        return it->second;

    GrantInfo info;
    if (CCINIClass* pINI = CCINIClass::INI_Rules)
    {
        if (pWH && pWH->ID)
        {
            // The warhead grants the stance if either AggressiveStance=yes is set
            // or the AggressiveStance.Duration key is present. Duration controls the
            // timer; with only AggressiveStance=yes it defaults to indefinite (-1).
            bool enableFlag = pINI->ReadBool(pWH->ID, "AggressiveStance", false);
            char buf[16] = {};
            pINI->ReadString(pWH->ID, "AggressiveStance.Duration", "", buf, sizeof(buf));
            bool hasDuration = (buf[0] != '\0');

            if (enableFlag || hasDuration)
            {
                info.Has = true;
                info.Duration = hasDuration ? pINI->ReadInteger(pWH->ID, "AggressiveStance.Duration", 0) : -1;
                info.Cumulative = pINI->ReadBool(pWH->ID, "AggressiveStance.Cumulative", false);

                char houseBuf[64] = {};
                pINI->ReadString(pWH->ID, "AggressiveStance.AffectsHouses", "", houseBuf, sizeof(houseBuf));
                info.AffectsHouses = ParseAffectsHouses(houseBuf, ASHouse::All);
            }
        }
    }

    return Cache.emplace(pWH, info).first->second;
}

bool WarheadTypeExt::PassesHouseFilter(unsigned char mask, TechnoClass* pTarget, HouseClass* pFirerHouse)
{
    if (mask == ASHouse::None)
        return false;
    if (!pTarget || !pTarget->Owner)
        return false;

    HouseClass* pTargetHouse = pTarget->Owner;

    // Neutral (civilian / MultiplayPassive) is its own category, checked before
    // the owner/ally/enemy split so it is never lumped in with "enemies".
    if (pTargetHouse->IsNeutral())
        return (mask & ASHouse::Neutral) != 0;

    // Resolving owner/allies/enemies needs the firing house. Without it, only
    // honour the filter if all three non-neutral categories are requested.
    if (!pFirerHouse)
    {
        const unsigned char nonNeutral = ASHouse::Owner | ASHouse::Allies | ASHouse::Enemies;
        return (mask & nonNeutral) == nonNeutral;
    }

    if (pTargetHouse == pFirerHouse)
        return (mask & ASHouse::Owner) != 0;
    if (pFirerHouse->IsAlliedWith(pTargetHouse))
        return (mask & ASHouse::Allies) != 0;
    return (mask & ASHouse::Enemies) != 0;
}

// ---------------------------------------------------------------------------
// Hook: TechnoClass::ReceiveDamage (0x701900)
// Fires once for every unit a warhead's blast actually damages. If the warhead
// carries AggressiveStance.Duration and the unit passes the house filter, grant
// (or refresh / clear) the unit's warhead-driven Aggressive Stance.
// ECX = this (target). Entry stack args: pDamage[+0x4], distance[+0x8],
// pWH[+0xC], Attacker[+0x10], ignoreDefenses[+0x14], preventEscape[+0x18],
// pAttackingHouse[+0x1C].
// Stolen bytes: 81 EC B4 00 00 00 = sub esp,0xB4 (6 bytes)
// ---------------------------------------------------------------------------

DEFINE_HOOK(0x701900, TechnoClass_ReceiveDamage_AggressiveStanceGrant, 0x6)
{
    GET(TechnoClass*, pThis, ECX);
    GET_STACK(WarheadTypeClass*, pWH, 0xC);
    GET_STACK(ObjectClass*, pAttacker, 0x10);
    GET_STACK(HouseClass*, pAttackingHouse, 0x1C);

    if (pThis && pWH)
    {
        const auto& info = WarheadTypeExt::GetAggressiveStance(pWH);
        if (info.Has)
        {
            HouseClass* pFirerHouse = pAttackingHouse;
            if (!pFirerHouse && pAttacker)
                pFirerHouse = pAttacker->GetOwningHouse();

            if (WarheadTypeExt::PassesHouseFilter(info.AffectsHouses, pThis, pFirerHouse))
                AggressiveStanceClass::ApplyGrant(pThis, info.Duration, info.Cumulative);
        }
    }

    return 0;
}
