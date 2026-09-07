#include "AggressiveStance.h"
#include <EventClass.h>
#include <HouseClass.h>
#include <InfantryTypeClass.h>
#include <Fundamentals.h>
#include <Ext/Event/Body.h>
#include <Ext/TechnoType/Body.h>
#include <Utilities/GeneralUtils.h>

std::map<TechnoClass*, bool> AggressiveStanceClass::AggressiveStanceMap;
std::map<TechnoClass*, int>  AggressiveStanceClass::GrantExpiry;

bool AggressiveStanceClass::IsGrantActive(TechnoClass* pTechno)
{
    auto it = GrantExpiry.find(pTechno);
    if (it == GrantExpiry.end())
        return false;

    if (it->second == -1)               // indefinite
        return true;

    if (Unsorted::CurrentFrame < it->second)
        return true;

    GrantExpiry.erase(it);              // expired — prune
    return false;
}

void AggressiveStanceClass::ApplyGrant(TechnoClass* pTechno, int duration, bool cumulative)
{
    if (!pTechno)
        return;

    if (duration == 0)                  // clear / turn off
    {
        GrantExpiry.erase(pTechno);
        return;
    }

    if (duration < 0)                   // forever
    {
        GrantExpiry[pTechno] = -1;
        return;
    }

    const int now = Unsorted::CurrentFrame;

    if (cumulative)
    {
        auto it = GrantExpiry.find(pTechno);
        if (it != GrantExpiry.end())
        {
            if (it->second == -1)       // already indefinite — leave it
                return;

            // Extend from whichever is later: the current expiry or now.
            const int base = it->second > now ? it->second : now;
            GrantExpiry[pTechno] = base + duration;
            return;
        }
    }

    GrantExpiry[pTechno] = now + duration;   // set / overwrite
}

const char* AggressiveStanceClass::GetName() const
{
    return "AggressiveStance";
}

const wchar_t* AggressiveStanceClass::GetUIName() const
{
    return GeneralUtils::LoadStringUnlessMissing("TXT_AGGRESSIVE_STANCE", L"Aggressive Stance");
}

const wchar_t* AggressiveStanceClass::GetUICategory() const
{
    return CATEGORY_CONTROL;
}

const wchar_t* AggressiveStanceClass::GetUIDescription() const
{
    return GeneralUtils::LoadStringUnlessMissing("TXT_AGGRESSIVE_STANCE_DESC", L"Aggressive Stance");
}

static inline const wchar_t* GetToggleOnPopupMessage()
{
    return GeneralUtils::LoadStringUnlessMissing("MSG:AGGRESSIVE_STANCE_ON", L"%i unit(s) entered Aggressive Stance.");
}

static inline const wchar_t* GetToggleOffPopupMessage()
{
    return GeneralUtils::LoadStringUnlessMissing("MSG:AGGRESSIVE_STANCE_OFF", L"%i unit(s) ceased Aggressive Stance.");
}

static bool CanToggleAggressiveStance(TechnoClass* pTechno)
{
    auto pType = pTechno->GetTechnoType();

    // AggressiveStance.Always units are permanently locked — hotkey cannot touch them.
    if (TechnoTypeExt::IsAlwaysAggressiveStance(pType))
        return false;

    // Check explicit AggressiveStance.Togglable override first.
    int togglable = TechnoTypeExt::GetTogglable(pType);
    if (togglable == 0) return false;  // forced off
    if (togglable == 1) return true;   // forced on

    // Auto-detect (togglable == -1): mirror the original vanilla logic.
    if (!(pTechno->IsArmed() || pType->OpenTopped))
        return false;

    if (auto pInfType = abstract_cast<InfantryTypeClass*>(pType))
        if (pInfType->Engineer || pInfType->Agent)
            return false;

    return true;
}

void AggressiveStanceClass::Execute(WWKey eInput) const
{
    std::vector<TechnoClass*> TechnoVectorAggressive;
    std::vector<TechnoClass*> TechnoVectorNonAggressive;

    bool isAnySelectedUnitTogglable = false;
    bool isAllSelectedUnitAggressiveStance = true;

    for (const auto& pUnit : ObjectClass::CurrentObjects())
    {
        TechnoClass* pTechno = abstract_cast<TechnoClass*>(pUnit);
        if (!pTechno || pTechno->Berzerk || !pTechno->Owner->IsControlledByCurrentPlayer())
            continue;

        if (CanToggleAggressiveStance(pTechno))
        {
            isAnySelectedUnitTogglable = true;
            if (AggressiveStanceClass::AggressiveStanceMap[pTechno])
                TechnoVectorAggressive.push_back(pTechno);
            else
            {
                isAllSelectedUnitAggressiveStance = false;
                TechnoVectorNonAggressive.push_back(pTechno);
            }
        }
    }

    if (isAnySelectedUnitTogglable)
    {
        std::vector<TechnoClass*> TechnoVector;
        const wchar_t* Message;
        if (isAllSelectedUnitAggressiveStance)
        {
            TechnoVector = TechnoVectorAggressive;
            Message = GetToggleOffPopupMessage();
        }
        else
        {
            TechnoVector = TechnoVectorNonAggressive;
            Message = GetToggleOnPopupMessage();
        }

        for (auto pTechno : TechnoVector)
        {
            EventExt::RaiseToggleAggressiveStance(pTechno);

            auto pTechnoType = pTechno->GetTechnoType();
            int voiceIndex = 0;

            if (!isAllSelectedUnitAggressiveStance)
            {
                TypeList<int> voiceList = pTechnoType->VoiceAttack.Count
                    ? pTechnoType->VoiceAttack : pTechnoType->VoiceMove;
                if (voiceList.Count)
                {
                    unsigned int idxRandom = Randomizer::Global().Random();
                    voiceIndex = voiceList.GetItem(idxRandom % voiceList.Count);
                }
            }

            if (voiceIndex > 0)
                pTechno->QueueVoice(voiceIndex);
        }

        wchar_t buffer[0x1000];
        wsprintfW(buffer, Message, TechnoVector.size());
        MessageListClass::Instance->PrintMessage(buffer, RulesClass::Instance->MessageDelay,
            HouseClass::CurrentPlayer->ColorSchemeIndex);
    }
}
