/*
 * Copyright (C) 2011-2012 Project SkyFire <http://www.projectskyfire.org/>
 * Copyright (C) 2008-2012 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2011-2012 Project StudioMirage <http://www.studio-mirage.fr/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "ScriptPCH.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"

class player_scripts_spec : public PlayerScript
{
public:
    player_scripts_spec() : PlayerScript("player_scripts_spec") { }

    void OnAddSpell(Player* player, uint32 spell_id, bool learning)
    {
        // learning Mastery at level 80
        switch (spell_id)
        {
            case 86470: // 87491 Druid
            case 86471: // 87492 Death Knight
            case 86472: // 87493 Hunter
            case 86473: // 86467 Mage
            case 86474: // 87494 Paladin
            case 86475: // 87495 Priest
            case 86476: // 87496 Rogue
            case 86477: // 87497 Shaman
            case 86478: // 87498 Warlock
            case 86479: // 87500 Warrior
                    player->UpdateMasteryAuras(player->GetTalentBranchSpec(player->GetActiveSpec()));
                break;
        }
    }

    void OnActivateSpec(Player* player, uint8 spec)
    {
        player->UpdateMasteryAuras(player->GetTalentBranchSpec(spec));
    }

    void OnTalentBranchSpecChanged(Player* player, uint8 spec, uint32 specID)
    {
        if (spec == player->GetActiveSpec())
            player->UpdateMasteryAuras(specID);
    }

    void OnUpdateRating(Player* player, CombatRating cr, int32& amount)
    {
        if (cr != CR_MASTERY)
            return;

        player->RecalculateMasteryAuraEffects(player->GetTalentBranchSpec(player->GetActiveSpec()));
    }
};

// Warrior masteries
// 76838 Strikes of Opportunity
class spell_war_strikes_of_opportunity : public SpellScriptLoader
{
public:
    spell_war_strikes_of_opportunity() : SpellScriptLoader("spell_war_strikes_of_opportunity") { }

    class spell_war_strikes_of_opportunity_AuraScript : public MasteryScript
    {
    public:
        void OnProc(AuraEffect const* aurEff, Unit* pUnit, Unit* victim, uint32 damage, SpellInfo const* procSpell, uint32 procFlag, uint32 procExtra, WeaponAttackType attType, int32 cooldown)
        {
            PreventDefaultAction();
            if (attType != BASE_ATTACK && attType != OFF_ATTACK)
                return;
            uint32 procSpellId = procSpell ? procSpell->Id : 0;
            //sLog->outBasic("Strikes of Opportunity : attType %u, damage %u, procSpell %u, chance %d", attType, damage, procSpellId, aurEff->GetAmount());
            if ((procSpellId != 76858) && roll_chance_i(aurEff->GetAmount()))
            {
                pUnit->CastSpell(victim, 76858, true); // Opportunity Strike on melee attack
                if (cooldown && pUnit->GetTypeId() == TYPEID_PLAYER)
                    pUnit->ToPlayer()->AddSpellCooldown(76858, 0, time(NULL) + cooldown);
            }
        }
    };

    AuraScript* GetAuraScript() const
    {
        spell_war_strikes_of_opportunity_AuraScript* script = new spell_war_strikes_of_opportunity_AuraScript();
        script->SetMasteryAura(EFFECT_0, SPELL_AURA_DUMMY, true);
        script->SetMasteryBaseAmount(EFFECT_1, 220);
        return script;
    }
};

// 76856 Unshackled Fury
class spell_war_unshackled_fury : public SpellScriptLoader
{
public:
    spell_war_unshackled_fury() : SpellScriptLoader("spell_war_unshackled_fury") { }

    AuraScript* GetAuraScript() const
    {
        MasteryScript* script = new MasteryScript();
        script->SetMasteryAura(EFFECT_0, SPELL_AURA_ADD_PCT_MODIFIER);
        script->SetMasteryAura(EFFECT_1, SPELL_AURA_ADD_PCT_MODIFIER);
        script->SetMasteryBaseAmount(EFFECT_2, 560);
        return script;
    }
};

// 76857 Critical Block
class spell_war_critical_block : public SpellScriptLoader
{
public:
    spell_war_critical_block() : SpellScriptLoader("spell_war_critical_block") { }

    AuraScript* GetAuraScript() const
    {
        MasteryScript* script = new MasteryScript();
        script->SetMasteryAura(EFFECT_0, SPELL_AURA_MOD_BLOCK_CRIT_CHANCE);
        script->SetMasteryAura(EFFECT_1, SPELL_AURA_MOD_BLOCK_PERCENT);
        script->SetMasteryBaseAmount(EFFECT_2, 150);
        return script;
    }
};

// Mage masteries
// 76547 Mana Adept
class spell_mage_mana_adept : public SpellScriptLoader
{
public:
    spell_mage_mana_adept() : SpellScriptLoader("spell_mage_mana_adept") { }

    class spell_mage_mana_adept_AuraScript : public MasteryScript
    {
        void CalcSpellMod(AuraEffect const* aurEff, SpellModifier*& spellMod, SpellInfo const* /*spellInfo*/, Unit* /*target*/)
        {
            int32 bonus = 0;
            if (Unit* caster = GetCaster())
            {
                if (Player* player = caster->ToPlayer())
                {
                    uint32 maxMana = player->GetMaxPower(POWER_MANA);
                    if (!maxMana)
                        return;
                    uint32 amount = aurEff->GetAmount(); // 150 * mastery / 100
                    float manaPercent = float(player->GetPower(POWER_MANA)) / float(maxMana);
                    //float mastery = player->GetMasteryPoints();
                    bonus = int32(manaPercent * (/*(mastery + 8.0f) **/ amount)/* / 100.0f*/);
                    //if (!spellMod || bonus != spellMod->value)
                    //    sLog->outBasic("Mana Adept : manaPct %.2f, amount %u, bonus %d", manaPercent, amount, bonus);
                }
            }

            if (!spellMod)
                spellMod = new SpellModifier(GetAura(), const_cast<AuraEffect*>(aurEff));

            spellMod->op = SPELLMOD_DAMAGE/*SPELLMOD_ALL_EFFECTS*/;

            spellMod->type = SPELLMOD_PCT;
            spellMod->spellId = aurEff->GetId(); // 12042 Arcane Power : 685904631, 102472, 0
            spellMod->mask = flag96(0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF);//GetSpellProto()->EffectSpellClassMask[aurEff->GetEffIndex()];
            spellMod->charges = GetAura()->GetCharges();

            //spellMod->spellId = aurEff->GetId(); // 12042
            spellMod->value = bonus;
        }
    };

    AuraScript* GetAuraScript() const
    {
        spell_mage_mana_adept_AuraScript *script = new spell_mage_mana_adept_AuraScript();
        script->SetMasteryAura(EFFECT_0, SPELL_AURA_ADD_PCT_MODIFIER/*SPELL_AURA_DUMMY*/); // changed in SpellMgr
        script->SetMasteryBaseAmount(EFFECT_1, 150);
        return script;
    }
};

// 76595 Flashburn
class spell_mage_flashburn : public SpellScriptLoader
{
public:
    spell_mage_flashburn() : SpellScriptLoader("spell_mage_flashburn") { }

    AuraScript* GetAuraScript() const
    {
        MasteryScript* script = new MasteryScript();
        script->SetMasteryAura(EFFECT_0, SPELL_AURA_ADD_PCT_MODIFIER);
        script->SetMasteryBaseAmount(EFFECT_1, 280);
        return script;
    }
};

// 76613 Frostburn
class spell_mage_frostburn : public SpellScriptLoader
{
public:
    spell_mage_frostburn() : SpellScriptLoader("spell_mage_frostburn") { }

    class spell_mage_frostburn_AuraScript : public MasteryScript
    {
        void CalcSpellMod(AuraEffect const * aurEff, SpellModifier *& spellMod, SpellInfo const *spellInfo, Unit * target)
        {
            int32 bonus = 0;
            if (target && target->HasAuraState(AURA_STATE_FROZEN, spellInfo, GetCaster()))
            {
                bonus = aurEff->GetAmount(); // 250 * mastery / 100
                //if (!spellMod || bonus != spellMod->value)
                //    sLog->outBasic("Frostburn : affected spell %u bonus %d", spellInfo ? spellInfo->Id : 0, bonus);
            }

            if (!spellMod)
                spellMod = new SpellModifier(GetAura(), const_cast<AuraEffect*>(aurEff));

            spellMod->op = SPELLMOD_DAMAGE/*SPELLMOD_ALL_EFFECTS*/;
            spellMod->type = SPELLMOD_PCT;
            spellMod->spellId = aurEff->GetId(); // 12042 Arcane Power : 685904631, 102472, 0
            spellMod->mask = flag96(0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF);//GetSpellProto()->EffectSpellClassMask[aurEff->GetEffIndex()];
            spellMod->charges = GetAura()->GetCharges();
            //spellMod->spellId = aurEff->GetId(); // 12042
            spellMod->value = bonus;
        }
    };

    AuraScript* GetAuraScript() const
    {
        spell_mage_frostburn_AuraScript *script = new spell_mage_frostburn_AuraScript();
        script->SetMasteryAura(EFFECT_0, SPELL_AURA_ADD_PCT_MODIFIER/*SPELL_AURA_DUMMY*/); // changed in SpellMgr
        script->SetMasteryBaseAmount(EFFECT_1, 250);
        return script;
    }
};

// Shaman masteries
// 77222 Elemental Overlord
class spell_sha_elemental_overlord : public SpellScriptLoader
{
public:
    spell_sha_elemental_overlord() : SpellScriptLoader("spell_sha_elemental_overlord") { }

    class spell_sha_elemental_overlord_AuraScript : public MasteryScript
    {
    public:
        void OnProc(AuraEffect const* aurEff, Unit* unit, Unit* victim, uint32 damage, SpellInfo const* procSpell, uint32 procFlag, uint32 procExtra, WeaponAttackType attType, int32 cooldown)
        {
            PreventDefaultAction();
            if (!(procFlag & PROC_FLAG_DONE_SPELL_MAGIC_DMG_CLASS_NEG))
                return;

            Player* player = unit->ToPlayer();
            if (!player)
                return;

            // custom cooldown processing case
            uint32 auraSpellId = GetAura()->GetId();
            if (cooldown && player->HasSpellCooldown(auraSpellId))
                return;

            uint32 procSpellId = procSpell ? procSpell->Id : 0;
            uint32 spellId = 0;
            switch (procSpellId)
            {
                // Lightning Bolt
                case 403:
                    spellId = 45284;
                    break;
                // Chain Lightning
                case 421:
                    spellId = 45297;
                    break;
                // Lava Burst
                case 51505:
                    spellId = 77451;
                    break;
                default:
                    return;
            }

            //sLog->outDetail("Elemental Overlord : attType %u, damage %u, procSpell %u, chance %d", attType, damage, procSpellId, aurEff->GetAmount());

            // Chain Lightning
            if (procSpell->SpellFamilyFlags[0] & 0x2)
            {
                // Chain lightning has [LightOverload_Proc_Chance] / [Max_Number_of_Targets] chance to proc of each individual target hit.
                // A maxed LO would have a 33% / 3 = 11% chance to proc of each target.
                // LO chance was already "accounted" at the proc chance roll, now need to divide the chance by [Max_Number_of_Targets]
                float chance = 100.0f / procSpell->Effects[EFFECT_0].ChainTarget;
                if (!roll_chance_f(chance))
                    return;

                // Remove cooldown (Chain Lightning - have Category Recovery time)
                player->RemoveSpellCooldown(spellId);
            }

            if (roll_chance_i(aurEff->GetAmount()))
            {
                unit->CastSpell(victim, spellId, true, NULL, aurEff);
                if (cooldown)
                    player->AddSpellCooldown(auraSpellId, 0, time(NULL) + cooldown);
            }
        }
    };

    AuraScript* GetAuraScript() const
    {
        spell_sha_elemental_overlord_AuraScript* script = new spell_sha_elemental_overlord_AuraScript();
        script->SetMasteryAura(EFFECT_0, SPELL_AURA_DUMMY, true);
        script->SetMasteryBaseAmount(EFFECT_1, 250);
        return script;
    }
};

// 77223 Enhanced Elements
class spell_sha_enhanced_elements : public SpellScriptLoader
{
public:
    spell_sha_enhanced_elements() : SpellScriptLoader("spell_sha_enhanced_elements") { }

    AuraScript *GetAuraScript() const
    {
        MasteryScript* script = new MasteryScript();
        script->SetMasteryAura(EFFECT_0, SPELL_AURA_MOD_DAMAGE_PERCENT_DONE);
        script->SetMasteryBaseAmount(EFFECT_1, 250);
        return script;
    }
};

// 77226 Deep Healing
class spell_sha_deep_healing : public SpellScriptLoader
{
public:
    spell_sha_deep_healing() : SpellScriptLoader("spell_sha_deep_healing") { }

    class spell_sha_deep_healing_AuraScript : public MasteryScript
    {
    public:
        void CalcSpellMod(AuraEffect const* aurEff, SpellModifier*& spellMod, SpellInfo const* /*spellInfo*/, Unit* target)
        {
            int32 bonus = 0;
            float pct = 0.0f;
            if (target)
            {
                pct = target->GetHealthPct();
                uint32 amount = aurEff->GetAmount(); // 300
                //float mastery = player->GetMasteryPoints();
                bonus = int32((100.0f - pct) * amount/*((mastery + 8.0f) * amount)*/ / 100.0f);
            }

            if (!spellMod)
                spellMod = new SpellModifier(GetAura(), const_cast<AuraEffect*>(aurEff));

            spellMod->op = SPELLMOD_DAMAGE/*SPELLMOD_ALL_EFFECTS*/;

            spellMod->type = SPELLMOD_PCT;
            spellMod->spellId = aurEff->GetId(); // 16188 Nature's Swiftness
            spellMod->mask = GetSpellInfo()->Effects[aurEff->GetEffIndex()].SpellClassMask;
            spellMod->charges = GetAura()->GetCharges();

            spellMod->value = bonus;
        }
    };

    AuraScript *GetAuraScript() const
    {
        spell_sha_deep_healing_AuraScript* script = new spell_sha_deep_healing_AuraScript();
        script->SetMasteryAura(EFFECT_0, SPELL_AURA_ADD_PCT_MODIFIER/*SPELL_AURA_DUMMY*/); // changed in SpellMgr
        script->SetMasteryBaseAmount(EFFECT_1, 300);
        return script;
    }
};

// Hunter masteries
// 76659 Wild Quiver
class spell_hun_wild_quiver : public SpellScriptLoader
{
public:
    spell_hun_wild_quiver() : SpellScriptLoader("spell_hun_wild_quiver") { }

    class spell_hun_wild_quiver_AuraScript : public MasteryScript
    {
    public:
        void OnProc(AuraEffect const* aurEff, Unit* unit, Unit* victim, uint32 damage, SpellInfo const* procSpell, uint32 procFlag, uint32 procExtra, WeaponAttackType attType, int32 cooldown)
        {
            PreventDefaultAction();
            if (attType != RANGED_ATTACK && attType != OFF_ATTACK)
                return;
            uint32 procSpellId = procSpell ? procSpell->Id : 0;
            if ((procSpellId != 76663) && roll_chance_i(aurEff->GetAmount()))
            {
                unit->CastSpell(victim, 76663, true); // Wild Quiver on auto-shot
                if (cooldown && unit->GetTypeId() == TYPEID_PLAYER)
                    unit->ToPlayer()->AddSpellCooldown(76663, 0, time(NULL) + cooldown);
            }
        }
    };

    AuraScript* GetAuraScript() const
    {
        spell_hun_wild_quiver_AuraScript* script = new spell_hun_wild_quiver_AuraScript();
        script->SetMasteryAura(EFFECT_0, SPELL_AURA_DUMMY, true);
        script->SetMasteryBaseAmount(EFFECT_1, 180);
        return script;
    }
};

// 76657 Master of Beasts
class spell_hun_master_of_beasts : public SpellScriptLoader
{
public:
    spell_hun_master_of_beasts() : SpellScriptLoader("spell_hun_master_of_beasts") { }

    AuraScript *GetAuraScript() const
    {
        MasteryScript* script = new MasteryScript();
        script->SetMasteryAura(EFFECT_0, SPELL_AURA_ADD_FLAT_MODIFIER);
        script->SetMasteryBaseAmount(EFFECT_1, 170);
        return script;
    }
};

// 76658 Essence of the Viper
class spell_hun_essence_of_the_viper : public SpellScriptLoader
{
public:
    spell_hun_essence_of_the_viper() : SpellScriptLoader("spell_hun_essence_of_the_viper") { }

    AuraScript* GetAuraScript() const
    {
        MasteryScript* script = new MasteryScript();
        script->SetMasteryAura(EFFECT_0, SPELL_AURA_MOD_DAMAGE_PERCENT_DONE);
        script->SetMasteryBaseAmount(EFFECT_1, 100);
        return script;
    }
};

// Rogue masteries
// 76803 Potent Poisons
class spell_rog_potent_poisons : public SpellScriptLoader
{
public:
    spell_rog_potent_poisons() : SpellScriptLoader("spell_rog_potent_poisons") { }

    AuraScript* GetAuraScript() const
    {
        MasteryScript* script = new MasteryScript();
        script->SetMasteryAura(EFFECT_0, SPELL_AURA_ADD_PCT_MODIFIER);
        script->SetMasteryAura(EFFECT_1, SPELL_AURA_ADD_PCT_MODIFIER);
        script->SetMasteryBaseAmount(EFFECT_2, 350);
        return script;
    }
};

// 76806 Main Gauche
class spell_rog_main_gauche : public SpellScriptLoader
{
public:
    spell_rog_main_gauche() : SpellScriptLoader("spell_rog_main_gauche") { }

    class spell_rog_main_gauche_AuraScript : public MasteryScript
    {
    public:
        void OnProc(AuraEffect const* aurEff, Unit* unit, Unit* victim, uint32 damage, SpellInfo const* procSpell, uint32 procFlag, uint32 procExtra, WeaponAttackType attType, int32 cooldown)
        {
            PreventDefaultAction();
            if (attType != BASE_ATTACK)
                return;
            uint32 procSpellId = procSpell ? procSpell->Id : 0;
            if ((procSpellId != 86392) && roll_chance_i(aurEff->GetAmount()))
            {
                unit->CastSpell(victim, 86392, true); // Main Gauche on melee
                if (cooldown && unit->GetTypeId() == TYPEID_PLAYER)
                    unit->ToPlayer()->AddSpellCooldown(86392, 0, time(NULL) + cooldown);
            }
        }
    };

    AuraScript* GetAuraScript() const
    {
        spell_rog_main_gauche_AuraScript* script = new spell_rog_main_gauche_AuraScript();
        script->SetMasteryAura(EFFECT_0, SPELL_AURA_DUMMY, true);
        script->SetMasteryBaseAmount(EFFECT_1, 200);
        return script;
    }
};

// 76808 Executioner
class spell_rog_executioner : public SpellScriptLoader
{
public:
    spell_rog_executioner() : SpellScriptLoader("spell_rog_executioner") { }

    AuraScript* GetAuraScript() const
    {
        MasteryScript* script = new MasteryScript();
        script->SetMasteryAura(EFFECT_0, SPELL_AURA_ADD_PCT_MODIFIER);
        script->SetMasteryAura(EFFECT_1, SPELL_AURA_ADD_PCT_MODIFIER);
        script->SetMasteryAura(EFFECT_2, SPELL_AURA_ADD_PCT_MODIFIER);
        script->SetMasteryBaseAmount(EFFECT_0, 250);
        return script;
    }
};

// DeathKnight masteries
// 77514 Frozen Heart
class spell_dk_frozen_heart : public SpellScriptLoader
{
public:
    spell_dk_frozen_heart() : SpellScriptLoader("spell_dk_frozen_heart") { }

    AuraScript* GetAuraScript() const
    {
        MasteryScript* script = new MasteryScript();
        script->SetMasteryAura(EFFECT_0, SPELL_AURA_MOD_DAMAGE_PERCENT_DONE);
        script->SetMasteryBaseAmount(EFFECT_1, 200);
        return script;
    }
};

// 77515 Dreadblade
class spell_dk_dreadblade : public SpellScriptLoader
{
public:
    spell_dk_dreadblade() : SpellScriptLoader("spell_dk_dreadblade") { }

    AuraScript* GetAuraScript() const
    {
        MasteryScript* script = new MasteryScript();
        script->SetMasteryAura(EFFECT_0, SPELL_AURA_MOD_DAMAGE_PERCENT_DONE);
        script->SetMasteryBaseAmount(EFFECT_1, 250);
        return script;
    }
};

// 77513 Blood Shield
class spell_dk_blood_shield : public SpellScriptLoader
{
public:
    spell_dk_blood_shield() : SpellScriptLoader("spell_dk_blood_shield") { }

    AuraScript* GetAuraScript() const
    {
        MasteryScript* script = new MasteryScript();
        script->SetMasteryAura(EFFECT_0, SPELL_AURA_DUMMY); // Test
        script->SetMasteryBaseAmount(EFFECT_1, 625);
        return script;
    }
};

// Druid masteries
// 77492 Total Eclipse
class spell_dru_total_eclipse : public SpellScriptLoader
{
public:
    spell_dru_total_eclipse() : SpellScriptLoader("spell_dru_total_eclipse") { }

    AuraScript* GetAuraScript() const
    {
        MasteryScript* script = new MasteryScript();
        script->SetMasteryAura(EFFECT_0, SPELL_AURA_ADD_FLAT_MODIFIER);
        script->SetMasteryBaseAmount(EFFECT_1, 200);
        return script;
    }
};

// 77495 Harmony
class spell_dru_harmony : public SpellScriptLoader
{
public:
    spell_dru_harmony() : SpellScriptLoader("spell_dru_harmony") { }

    AuraScript* GetAuraScript() const
    {
        MasteryScript* script = new MasteryScript();
        script->SetMasteryAura(EFFECT_0, SPELL_AURA_ADD_PCT_MODIFIER);
        script->SetMasteryBaseAmount(EFFECT_1, 125);
        return script;
    }
};

// 77493 Razor Claws
class spell_dru_razor_claws : public SpellScriptLoader
{
public:
    spell_dru_razor_claws() : SpellScriptLoader("spell_dru_razor_claws") { }

    AuraScript* GetAuraScript() const
    {
        MasteryScript* script = new MasteryScript();
        script->SetMasteryAura(EFFECT_0, SPELL_AURA_MOD_DAMAGE_DONE_FOR_MECHANIC);
        script->SetMasteryBaseAmount(EFFECT_1, 313);
        return script;
    }
};

// 77494 Savage Defender
class spell_dru_savage_defender : public SpellScriptLoader
{
public:
    spell_dru_savage_defender() : SpellScriptLoader("spell_dru_savage_defender") { }

    AuraScript* GetAuraScript() const
    {
        MasteryScript* script = new MasteryScript();
        script->SetMasteryAura(EFFECT_0, SPELL_AURA_DUMMY); // Test
        script->SetMasteryBaseAmount(EFFECT_1, 400);
        return script;
    }
};

// Paladin masteries
// 76671 Divine Bulwark
class spell_pal_divine_bulwark : public SpellScriptLoader
{
public:
    spell_pal_divine_bulwark() : SpellScriptLoader("spell_pal_divine_bulwark") { }

    AuraScript* GetAuraScript() const
    {
        MasteryScript* script = new MasteryScript();
        script->SetMasteryAura(EFFECT_0, SPELL_AURA_MOD_BLOCK_PERCENT);
        script->SetMasteryBaseAmount(EFFECT_1, 225);
        return script;
    }
};

// 76669 Illuminated Healing
class spell_pal_illuminated_healing : public SpellScriptLoader
{
public:
    spell_pal_illuminated_healing() : SpellScriptLoader("spell_pal_illuminated_healing") { }

    AuraScript* GetAuraScript() const
    {
        MasteryScript* script = new MasteryScript();
        script->SetMasteryAura(EFFECT_0, SPELL_AURA_SCHOOL_HEAL_ABSORB);
        script->SetMasteryBaseAmount(EFFECT_1, 150);
        return script;
    }
};

// 76672 Hand of Light
class spell_pal_hang_of_light : public SpellScriptLoader
{
public:
    spell_pal_hang_of_light() : SpellScriptLoader("spell_pal_hang_of_light") { }

    AuraScript* GetAuraScript() const
    {
        MasteryScript* script = new MasteryScript();
        script->SetMasteryAura(EFFECT_0, SPELL_AURA_PROC_TRIGGER_SPELL);
        script->SetMasteryBaseAmount(EFFECT_1, 210);
        return script;
    }
};

// Warlock masteries
// 77219 Master Demonologist
class spell_warl_master_demonologist : public SpellScriptLoader
{
public:
    spell_warl_master_demonologist() : SpellScriptLoader("spell_warl_master_demonologist") { }

    AuraScript* GetAuraScript() const
    {
        MasteryScript* script = new MasteryScript();
        script->SetMasteryAura(EFFECT_0, SPELL_AURA_ADD_FLAT_MODIFIER);
        script->SetMasteryAura(EFFECT_1, SPELL_AURA_ADD_FLAT_MODIFIER);
        script->SetMasteryBaseAmount(EFFECT_2, 200);
        return script;
    }
};

// 77220 Fiery Apocalypse
class spell_warl_fiery_apocalypse : public SpellScriptLoader
{
public:
    spell_warl_fiery_apocalypse() : SpellScriptLoader("spell_warl_fiery_apocalypse") { }

    AuraScript* GetAuraScript() const
    {
        MasteryScript* script = new MasteryScript();
        script->SetMasteryAura(EFFECT_0, SPELL_AURA_MOD_DAMAGE_PERCENT_DONE);
        script->SetMasteryBaseAmount(EFFECT_1, 135);
        return script;
    }
};

// 77215 potent Afflictions
class spell_warl_potent_afflictions : public SpellScriptLoader
{
public:
    spell_warl_potent_afflictions() : SpellScriptLoader("spell_warl_potent_afflictions") { }

    AuraScript* GetAuraScript() const
    {
        MasteryScript* script = new MasteryScript();
        script->SetMasteryAura(EFFECT_0, SPELL_AURA_ADD_PCT_MODIFIER);
        script->SetMasteryBaseAmount(EFFECT_2, 163);
        return script;
    }
};

// Priest masteries
// 77486 Shadow Orb Power
class spell_pri_shadow_orb_power : public SpellScriptLoader
{
public:
    spell_pri_shadow_orb_power() : SpellScriptLoader("spell_pri_shadow_orb_power") { }

    AuraScript* GetAuraScript() const
    {
        MasteryScript* script = new MasteryScript();
        script->SetMasteryAura(EFFECT_0, SPELL_AURA_PROC_TRIGGER_SPELL);
        script->SetMasteryBaseAmount(EFFECT_1, 145);
        return script;
    }
};

// 77484 Shield Discipline
class spell_pri_shield_discipline : public SpellScriptLoader
{
public:
    spell_pri_shield_discipline() : SpellScriptLoader("spell_pri_shield_discipline") { }

    AuraScript* GetAuraScript() const
    {
        MasteryScript* script = new MasteryScript();
        script->SetMasteryAura(EFFECT_0, SPELL_AURA_DUMMY); // Test
        script->SetMasteryBaseAmount(EFFECT_1, 250);
        return script;
    }
};

// 77485 Echo of Light
class spell_pri_echo_of_light : public SpellScriptLoader
{
public:
    spell_pri_echo_of_light() : SpellScriptLoader("spell_pri_echo_of_light") { }

    AuraScript* GetAuraScript() const
    {
        MasteryScript* script = new MasteryScript();
        script->SetMasteryAura(EFFECT_0, SPELL_AURA_DUMMY); // Test
        script->SetMasteryBaseAmount(EFFECT_1, 125);
        return script;
    }
};

void AddSC_mastery_spells()
{
    new player_scripts_spec;

    // Warrior masteries
    new spell_war_strikes_of_opportunity;
    new spell_war_unshackled_fury;
    new spell_war_critical_block;

    // Mage masteries
    new spell_mage_mana_adept;
    new spell_mage_flashburn;
    new spell_mage_frostburn;

    // Shaman masteries
    new spell_sha_elemental_overlord;
    new spell_sha_enhanced_elements;
    new spell_sha_deep_healing;

    // Hunter masteries
    new spell_hun_wild_quiver;
    new spell_hun_master_of_beasts;
    new spell_hun_essence_of_the_viper;

    // Rogue masteries
    new spell_rog_potent_poisons;
    new spell_rog_main_gauche;
    new spell_rog_executioner;

    // Death Knight masteries
    new spell_dk_frozen_heart;
    new spell_dk_dreadblade;
    new spell_dk_blood_shield;

    // Druid masteries
    new spell_dru_total_eclipse;
    new spell_dru_harmony;
    new spell_dru_razor_claws;
    new spell_dru_savage_defender;

    // Paladin masteries
    new spell_pal_divine_bulwark;
    new spell_pal_illuminated_healing;
    new spell_pal_hang_of_light;

    // Warlock masteries
    new spell_warl_master_demonologist;
    new spell_warl_fiery_apocalypse;
    new spell_warl_potent_afflictions;

    // Priest  masteries
    new spell_pri_shield_discipline;
    new spell_pri_shadow_orb_power;
    new spell_pri_echo_of_light;
}