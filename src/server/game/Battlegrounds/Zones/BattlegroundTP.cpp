/*
 * Copyright (C) 2011-2012 Project SkyFire <http://www.projectskyfire.org/>
 * Copyright (C) 2008-2012 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2005-2012 MaNGOS <http://getmangos.com/>
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

#include "Battleground.h"
#include "BattlegroundTP.h"
#include "Creature.h"
#include "GameObject.h"
#include "Language.h"
#include "Object.h"
#include "ObjectMgr.h"
#include "BattlegroundMgr.h"
#include "Player.h"
#include "World.h"
#include "WorldPacket.h"

// these variables aren't used outside of this file, so declare them only here
enum BG_TP_Rewards
{
    BG_TP_WIN = 0,
    BG_TP_FLAG_CAP,
    BG_TP_MAP_COMPLETE,
    BG_TP_REWARD_NUM
};

uint32 BG_TP_Honor[BG_HONOR_MODE_NUM][BG_TP_REWARD_NUM] =
{
    {20, 40, 40}, // normal honor
    {60, 40, 80}  // holiday
};

uint32 BG_TP_Reputation[BG_HONOR_MODE_NUM][BG_TP_REWARD_NUM] =
{
    {0, 35, 0}, // normal honor
    {0, 45, 0}  // holiday
};

BattlegroundTP::BattlegroundTP()
{
    BgObjects.resize(BG_TP_OBJECT_MAX);
    BgCreatures.resize(BG_CREATURES_MAX_TP);

    StartMessageIds[BG_STARTING_EVENT_FIRST]  = LANG_BG_TP_START_TWO_MINUTES;
    StartMessageIds[BG_STARTING_EVENT_SECOND] = LANG_BG_TP_START_ONE_MINUTE;
    StartMessageIds[BG_STARTING_EVENT_THIRD]  = LANG_BG_TP_START_HALF_MINUTE;
    StartMessageIds[BG_STARTING_EVENT_FOURTH] = LANG_BG_TP_HAS_BEGUN;
    _flagDebuffState = 0;
}

BattlegroundTP::~BattlegroundTP()
{
}

void BattlegroundTP::PostUpdateImpl(uint32 diff)
{
    if (GetStatus() == STATUS_IN_PROGRESS)
    {
        if (GetStartTime() >= 25*MINUTE*IN_MILLISECONDS)
        {
            if (GetTeamScore(ALLIANCE) == 0)
            {
                if (GetTeamScore(HORDE) == 0)        // No one scored - result is tie
                    EndBattleground(0);
                else                                 // Horde has more points and thus wins
                    EndBattleground(HORDE);
            }
            else if (GetTeamScore(HORDE) == 0)
                EndBattleground(ALLIANCE);           // Alliance has > 0, Horde has 0, alliance wins
            else if (GetTeamScore(HORDE) == GetTeamScore(ALLIANCE)) // Team score equal, winner is team that scored the last flag
                EndBattleground(_lastFlagCaptureTeam);
            else if (GetTeamScore(HORDE) > GetTeamScore(ALLIANCE))  // Last but not least, check who has the higher score
                EndBattleground(HORDE);
            else
                EndBattleground(ALLIANCE);
        }
        else if (GetStartTime() > uint32(_minutesElapsed * MINUTE * IN_MILLISECONDS))
        {
            ++_minutesElapsed;
            UpdateWorldState(BG_TP_STATE_TIMER, 28 - _minutesElapsed);
        }

        if (_flagState[BG_TEAM_ALLIANCE] == BG_TP_FLAG_STATE_WAIT_RESPAWN)
        {
            _flagsTimer[BG_TEAM_ALLIANCE] -= diff;

            if (_flagsTimer[BG_TEAM_ALLIANCE] < 0)
            {
                _flagsTimer[BG_TEAM_ALLIANCE] = 0;
                RespawnFlag(ALLIANCE, true);
            }
        }
        if (_flagState[BG_TEAM_ALLIANCE] == BG_TP_FLAG_STATE_ON_GROUND)
        {
            _flagsDropTimer[BG_TEAM_ALLIANCE] -= diff;

            if (_flagsDropTimer[BG_TEAM_ALLIANCE] < 0)
            {
                _flagsDropTimer[BG_TEAM_ALLIANCE] = 0;
                RespawnFlagAfterDrop(ALLIANCE);
                _bothFlagsKept = false;
            }
        }
        if (_flagState[BG_TEAM_HORDE] == BG_TP_FLAG_STATE_WAIT_RESPAWN)
        {
            _flagsTimer[BG_TEAM_HORDE] -= diff;

            if (_flagsTimer[BG_TEAM_HORDE] < 0)
            {
                _flagsTimer[BG_TEAM_HORDE] = 0;
                RespawnFlag(HORDE, true);
            }
        }
        if (_flagState[BG_TEAM_HORDE] == BG_TP_FLAG_STATE_ON_GROUND)
        {
            _flagsDropTimer[BG_TEAM_HORDE] -= diff;

            if (_flagsDropTimer[BG_TEAM_HORDE] < 0)
            {
                _flagsDropTimer[BG_TEAM_HORDE] = 0;
                RespawnFlagAfterDrop(HORDE);
                _bothFlagsKept = false;
            }
        }
        if (_bothFlagsKept)
        {
            _flagSpellForceTimer += diff;
            if (_flagDebuffState == 0 && _flagSpellForceTimer >= 10*MINUTE*IN_MILLISECONDS)  //10 minutes
            {
                if (Player* player = ObjectAccessor::FindPlayer(_flagKeepers[0]))
                    player->CastSpell(player, TP_SPELL_FOCUSED_ASSAULT, true);
                if (Player* player = ObjectAccessor::FindPlayer(_flagKeepers[1]))
                    player->CastSpell(player, TP_SPELL_FOCUSED_ASSAULT, true);
                _flagDebuffState = 1;
            }
            else if (_flagDebuffState == 1 && _flagSpellForceTimer >= 900000) //15 minutes
            {
                if (Player* player = ObjectAccessor::FindPlayer(_flagKeepers[0]))
                {
                    player->RemoveAurasDueToSpell(TP_SPELL_FOCUSED_ASSAULT);
                    player->CastSpell(player, TP_SPELL_BRUTAL_ASSAULT, true);
                }
                if (Player* player = ObjectAccessor::FindPlayer(_flagKeepers[1]))
                {
                    player->RemoveAurasDueToSpell(TP_SPELL_FOCUSED_ASSAULT);
                    player->CastSpell(player, TP_SPELL_BRUTAL_ASSAULT, true);
                }
                _flagDebuffState = 2;
            }
        }
        else
        {
            _flagSpellForceTimer = 0; //reset timer.
            _flagDebuffState = 0;
        }
    }
}

void BattlegroundTP::StartingEventCloseDoors()
{
    DoorClose(BG_TP_OBJECT_DOOR_A_1);
    DoorClose(BG_TP_OBJECT_DOOR_A_2);
    SpawnBGObject(BG_TP_OBJECT_DOOR_A_3, RESPAWN_IMMEDIATELY);
    SpawnBGObject(BG_TP_OBJECT_DOOR_A_4, RESPAWN_IMMEDIATELY);
    DoorClose(BG_TP_OBJECT_DOOR_H_1);
    DoorClose(BG_TP_OBJECT_DOOR_H_2);
    SpawnBGObject(BG_TP_OBJECT_DOOR_H_3, RESPAWN_ONE_DAY);
    SpawnBGObject(BG_TP_OBJECT_DOOR_H_4, RESPAWN_ONE_DAY);

    for (uint32 i = BG_TP_OBJECT_A_FLAG; i <= BG_TP_OBJECT_BERSERKBUFF_2; ++i)
        SpawnBGObject(i, RESPAWN_ONE_DAY);

    UpdateWorldState(BG_TP_STATE_TIMER_ACTIVE, 1);
    UpdateWorldState(BG_TP_STATE_TIMER, 25);
}

void BattlegroundTP::StartingEventOpenDoors()
{
    DoorOpen(BG_TP_OBJECT_DOOR_A_1);
    DoorOpen(BG_TP_OBJECT_DOOR_A_2);
    DoorOpen(BG_TP_OBJECT_DOOR_A_3);
    DoorOpen(BG_TP_OBJECT_DOOR_A_4);
    DoorOpen(BG_TP_OBJECT_DOOR_H_1);
    DoorOpen(BG_TP_OBJECT_DOOR_H_2);
    DoorOpen(BG_TP_OBJECT_DOOR_H_3);
    DoorOpen(BG_TP_OBJECT_DOOR_H_4);

    for (uint32 i = BG_TP_OBJECT_A_FLAG; i <= BG_TP_OBJECT_BERSERKBUFF_2; ++i)
        SpawnBGObject(i, RESPAWN_IMMEDIATELY);

    // players joining later are not eligible
    StartTimedAchievement(ACHIEVEMENT_TIMED_TYPE_EVENT, TP_EVENT_START_BATTLE);
}

void BattlegroundTP::AddPlayer(Player* player)
{
    Battleground::AddPlayer(player);
    //create score and add it to map, default values are set in constructor
    BattlegroundTPScore* sc = new BattlegroundTPScore;

    PlayerScores[player->GetGUID()] = sc;
}

void BattlegroundTP::RespawnFlag(uint32 Team, bool captured)
{
    if (Team == ALLIANCE)
    {
        sLog->outDebug(LOG_FILTER_BATTLEGROUND, "Respawn Alliance flag");
        _flagState[BG_TEAM_ALLIANCE] = BG_TP_FLAG_STATE_ON_BASE;
    }
    else
    {
        sLog->outDebug(LOG_FILTER_BATTLEGROUND, "Respawn Horde flag");
        _flagState[BG_TEAM_HORDE] = BG_TP_FLAG_STATE_ON_BASE;
    }

    if (captured)
    {
        //when map_update will be allowed for battlegrounds this code will be useless
        SpawnBGObject(BG_TP_OBJECT_H_FLAG, RESPAWN_IMMEDIATELY);
        SpawnBGObject(BG_TP_OBJECT_A_FLAG, RESPAWN_IMMEDIATELY);
        SendMessageToAll(LANG_BG_TP_F_PLACED, CHAT_MSG_BG_SYSTEM_NEUTRAL);
        PlaySoundToAll(BG_TP_SOUND_FLAGS_RESPAWNED);        // flag respawned sound...
    }
    _bothFlagsKept = false;
}

void BattlegroundTP::RespawnFlagAfterDrop(uint32 team)
{
    if (GetStatus() != STATUS_IN_PROGRESS)
        return;

    RespawnFlag(team, false);
    if (team == ALLIANCE)
    {
        SpawnBGObject(BG_TP_OBJECT_A_FLAG, RESPAWN_IMMEDIATELY);
        SendMessageToAll(LANG_BG_TP_ALLIANCE_FLAG_RESPAWNED, CHAT_MSG_BG_SYSTEM_NEUTRAL);
    }
    else
    {
        SpawnBGObject(BG_TP_OBJECT_H_FLAG, RESPAWN_IMMEDIATELY);
        SendMessageToAll(LANG_BG_TP_HORDE_FLAG_RESPAWNED, CHAT_MSG_BG_SYSTEM_NEUTRAL);
    }

    PlaySoundToAll(BG_TP_SOUND_FLAGS_RESPAWNED);

    if (GameObject* obj = GetBgMap()->GetGameObject(GetDroppedFlagGUID(team)))
        obj->Delete();
    else
        sLog->outError("unknown droped flag bg, guid: %u", GUID_LOPART(GetDroppedFlagGUID(team)));

    SetDroppedFlagGUID(0, team);
    _bothFlagsKept = false;
}

void BattlegroundTP::EventPlayerCapturedFlag(Player* Source)
{
    if (GetStatus() != STATUS_IN_PROGRESS)
        return;

    uint32 winner = 0;

    Source->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_ENTER_PVP_COMBAT);
    if (Source->GetTeam() == ALLIANCE)
    {
        if (!this->IsHordeFlagPickedup())
            return;
        SetHordeFlagPicker(0);                              // must be before aura remove to prevent 2 events (drop+capture) at the same time
                                                            // horde flag in base (but not respawned yet)
        _flagState[BG_TEAM_HORDE] = BG_TP_FLAG_STATE_WAIT_RESPAWN;
                                                            // Drop Horde Flag from Player
        Source->RemoveAurasDueToSpell(BG_TP_SPELL_HORDE_FLAG);
        if (_flagDebuffState == 1)
          Source->RemoveAurasDueToSpell(TP_SPELL_FOCUSED_ASSAULT);
        if (_flagDebuffState == 2)
          Source->RemoveAurasDueToSpell(TP_SPELL_BRUTAL_ASSAULT);
        if (GetTeamScore(ALLIANCE) < BG_TP_MAX_TEAM_SCORE)
            AddPoint(ALLIANCE, 1);
        PlaySoundToAll(BG_TP_SOUND_FLAG_CAPTURED_ALLIANCE);
        RewardReputationToTeam(1174, m_ReputationCapture, ALLIANCE);
    }
    else
    {
        if (!this->IsAllianceFlagPickedup())
            return;
        SetAllianceFlagPicker(0);                           // must be before aura remove to prevent 2 events (drop+capture) at the same time
                                                            // alliance flag in base (but not respawned yet)
        _flagState[BG_TEAM_ALLIANCE] = BG_TP_FLAG_STATE_WAIT_RESPAWN;
                                                            // Drop Alliance Flag from Player
        Source->RemoveAurasDueToSpell(BG_TP_SPELL_ALLIANCE_FLAG);
        if (_flagDebuffState == 1)
          Source->RemoveAurasDueToSpell(TP_SPELL_FOCUSED_ASSAULT);
        if (_flagDebuffState == 2)
          Source->RemoveAurasDueToSpell(TP_SPELL_BRUTAL_ASSAULT);
        if (GetTeamScore(HORDE) < BG_TP_MAX_TEAM_SCORE)
            AddPoint(HORDE, 1);
        PlaySoundToAll(BG_TP_SOUND_FLAG_CAPTURED_HORDE);
        RewardReputationToTeam(1172, m_ReputationCapture, HORDE);
    }
    //for flag capture is reward 2 honorable kills
    RewardHonorToTeam(GetBonusHonorFromKill(2), Source->GetTeam());

    SpawnBGObject(BG_TP_OBJECT_H_FLAG, BG_TP_FLAG_RESPAWN_TIME);
    SpawnBGObject(BG_TP_OBJECT_A_FLAG, BG_TP_FLAG_RESPAWN_TIME);

    if (Source->GetTeam() == ALLIANCE)
        SendMessageToAll(LANG_BG_TP_CAPTURED_HF, CHAT_MSG_BG_SYSTEM_ALLIANCE, Source);
    else
        SendMessageToAll(LANG_BG_TP_CAPTURED_AF, CHAT_MSG_BG_SYSTEM_HORDE, Source);

    UpdateFlagState(Source->GetTeam(), 1);                  // flag state none
    UpdateTeamScore(Source->GetTeam());
    // only flag capture should be updated
    UpdatePlayerScore(Source, SCORE_FLAG_CAPTURES, 1);      // +1 flag captures

    // update last flag capture to be used if teamscore is equal
    SetLastFlagCapture(Source->GetTeam());

    if (GetTeamScore(ALLIANCE) == BG_TP_MAX_TEAM_SCORE)
        winner = ALLIANCE;

    if (GetTeamScore(HORDE) == BG_TP_MAX_TEAM_SCORE)
        winner = HORDE;

    if (winner)
    {
        UpdateWorldState(BG_TP_FLAG_UNK_ALLIANCE, 0);
        UpdateWorldState(BG_TP_FLAG_UNK_HORDE, 0);
        UpdateWorldState(BG_TP_FLAG_STATE_ALLIANCE, 1);
        UpdateWorldState(BG_TP_FLAG_STATE_HORDE, 1);
        UpdateWorldState(BG_TP_STATE_TIMER_ACTIVE, 0);

        RewardHonorToTeam(BG_TP_Honor[_HonorMode][BG_TP_WIN], winner);
        EndBattleground(winner);
    }
    else
    {
        _flagsTimer[GetTeamIndexByTeamId(Source->GetTeam()) ? 0 : 1] = BG_TP_FLAG_RESPAWN_TIME;
    }
}

void BattlegroundTP::EventPlayerDroppedFlag(Player* Source)
{
    if (GetStatus() != STATUS_IN_PROGRESS)
    {
        // if not running, do not cast things at the dropper player (prevent spawning the "dropped" flag), neither send unnecessary messages
        // just take off the aura
        if (Source->GetTeam() == ALLIANCE)
        {
            if (!this->IsHordeFlagPickedup())
                return;
            if (GetFlagPickerGUID(BG_TEAM_HORDE) == Source->GetGUID())
            {
                SetHordeFlagPicker(0);
                Source->RemoveAurasDueToSpell(BG_TP_SPELL_HORDE_FLAG);
            }
        }
        else
        {
            if (!this->IsAllianceFlagPickedup())
                return;
            if (GetFlagPickerGUID(BG_TEAM_ALLIANCE) == Source->GetGUID())
            {
                SetAllianceFlagPicker(0);
                Source->RemoveAurasDueToSpell(BG_TP_SPELL_ALLIANCE_FLAG);
            }
        }
        return;
    }

    bool set = false;

    if (Source->GetTeam() == ALLIANCE)
    {
        if (!IsHordeFlagPickedup())
            return;
        if (GetFlagPickerGUID(BG_TEAM_HORDE) == Source->GetGUID())
        {
            SetHordeFlagPicker(0);
            Source->RemoveAurasDueToSpell(BG_TP_SPELL_HORDE_FLAG);
            if (_flagDebuffState == 1)
              Source->RemoveAurasDueToSpell(TP_SPELL_FOCUSED_ASSAULT);
            if (_flagDebuffState == 2)
              Source->RemoveAurasDueToSpell(TP_SPELL_BRUTAL_ASSAULT);
            _flagState[BG_TEAM_HORDE] = BG_TP_FLAG_STATE_ON_GROUND;
            Source->CastSpell(Source, BG_TP_SPELL_HORDE_FLAG_DROPPED, true);
            set = true;
        }
    }
    else
    {
        if (!IsAllianceFlagPickedup())
            return;
        if (GetFlagPickerGUID(BG_TEAM_ALLIANCE) == Source->GetGUID())
        {
            SetAllianceFlagPicker(0);
            Source->RemoveAurasDueToSpell(BG_TP_SPELL_ALLIANCE_FLAG);
            if (_flagDebuffState == 1)
              Source->RemoveAurasDueToSpell(TP_SPELL_FOCUSED_ASSAULT);
            if (_flagDebuffState == 2)
              Source->RemoveAurasDueToSpell(TP_SPELL_BRUTAL_ASSAULT);
            _flagState[BG_TEAM_ALLIANCE] = BG_TP_FLAG_STATE_ON_GROUND;
            Source->CastSpell(Source, BG_TP_SPELL_ALLIANCE_FLAG_DROPPED, true);
            set = true;
        }
    }

    if (set)
    {
        Source->CastSpell(Source, SPELL_RECENTLY_DROPPED_FLAG, true);
        UpdateFlagState(Source->GetTeam(), 1);

        if (Source->GetTeam() == ALLIANCE)
        {
            SendMessageToAll(LANG_BG_TP_DROPPED_HF, CHAT_MSG_BG_SYSTEM_HORDE, Source);
            UpdateWorldState(BG_TP_FLAG_UNK_HORDE, uint32(-1));
        }
        else
        {
            SendMessageToAll(LANG_BG_TP_DROPPED_AF, CHAT_MSG_BG_SYSTEM_ALLIANCE, Source);
            UpdateWorldState(BG_TP_FLAG_UNK_ALLIANCE, uint32(-1));
        }

        _flagsDropTimer[GetTeamIndexByTeamId(Source->GetTeam()) ? 0 : 1] = BG_TP_FLAG_DROP_TIME;
    }
}

void BattlegroundTP::EventPlayerClickedOnFlag(Player* Source, GameObject* target_obj)
{
    if (GetStatus() != STATUS_IN_PROGRESS)
        return;

    int32 message_id = 0;
    ChatMsg type = CHAT_MSG_BG_SYSTEM_NEUTRAL;

    //alliance flag picked up from base
    if (Source->GetTeam() == HORDE && GetFlagState(ALLIANCE) == BG_TP_FLAG_STATE_ON_BASE
        && BgObjects[BG_TP_OBJECT_A_FLAG] == target_obj->GetGUID())
    {
        message_id = LANG_BG_TP_PICKEDUP_AF;
        type = CHAT_MSG_BG_SYSTEM_HORDE;
        PlaySoundToAll(BG_TP_SOUND_ALLIANCE_FLAG_PICKED_UP);
        SpawnBGObject(BG_TP_OBJECT_A_FLAG, RESPAWN_ONE_DAY);
        SetAllianceFlagPicker(Source->GetGUID());
        _flagState[BG_TEAM_ALLIANCE] = BG_TP_FLAG_STATE_ON_PLAYER;
        //update world state to show correct flag carrier
        UpdateFlagState(HORDE, BG_TP_FLAG_STATE_ON_PLAYER);
        UpdateWorldState(BG_TP_FLAG_UNK_ALLIANCE, 1);
        Source->CastSpell(Source, BG_TP_SPELL_ALLIANCE_FLAG, true);
        Source->GetAchievementMgr().StartTimedAchievement(ACHIEVEMENT_TIMED_TYPE_SPELL_TARGET, BG_TP_SPELL_ALLIANCE_FLAG_PICKED);
        if (_flagState[1] == BG_TP_FLAG_STATE_ON_PLAYER)
          _bothFlagsKept = true;
    }

    //horde flag picked up from base
    if (Source->GetTeam() == ALLIANCE && GetFlagState(HORDE) == BG_TP_FLAG_STATE_ON_BASE
        && BgObjects[BG_TP_OBJECT_H_FLAG] == target_obj->GetGUID())
    {
        message_id = LANG_BG_TP_PICKEDUP_HF;
        type = CHAT_MSG_BG_SYSTEM_ALLIANCE;
        PlaySoundToAll(BG_TP_SOUND_HORDE_FLAG_PICKED_UP);
        SpawnBGObject(BG_TP_OBJECT_H_FLAG, RESPAWN_ONE_DAY);
        SetHordeFlagPicker(Source->GetGUID());
        _flagState[BG_TEAM_HORDE] = BG_TP_FLAG_STATE_ON_PLAYER;
        //update world state to show correct flag carrier
        UpdateFlagState(ALLIANCE, BG_TP_FLAG_STATE_ON_PLAYER);
        UpdateWorldState(BG_TP_FLAG_UNK_HORDE, 1);
        Source->CastSpell(Source, BG_TP_SPELL_HORDE_FLAG, true);
        Source->GetAchievementMgr().StartTimedAchievement(ACHIEVEMENT_TIMED_TYPE_SPELL_TARGET, BG_TP_SPELL_HORDE_FLAG_PICKED);
        if (_flagState[0] == BG_TP_FLAG_STATE_ON_PLAYER)
          _bothFlagsKept = true;
    }

    //Alliance flag on ground(not in base) (returned or picked up again from ground!)
    if (GetFlagState(ALLIANCE) == BG_TP_FLAG_STATE_ON_GROUND && Source->IsWithinDistInMap(target_obj, 10))
    {
        if (Source->GetTeam() == ALLIANCE)
        {
            message_id = LANG_BG_TP_RETURNED_AF;
            type = CHAT_MSG_BG_SYSTEM_ALLIANCE;
            UpdateFlagState(HORDE, BG_TP_FLAG_STATE_WAIT_RESPAWN);
            RespawnFlag(ALLIANCE, false);
            SpawnBGObject(BG_TP_OBJECT_A_FLAG, RESPAWN_IMMEDIATELY);
            PlaySoundToAll(BG_TP_SOUND_FLAG_RETURNED);
            UpdatePlayerScore(Source, SCORE_FLAG_RETURNS, 1);
            _bothFlagsKept = false;
        }
        else
        {
            message_id = LANG_BG_TP_PICKEDUP_AF;
            type = CHAT_MSG_BG_SYSTEM_HORDE;
            PlaySoundToAll(BG_TP_SOUND_ALLIANCE_FLAG_PICKED_UP);
            SpawnBGObject(BG_TP_OBJECT_A_FLAG, RESPAWN_ONE_DAY);
            SetAllianceFlagPicker(Source->GetGUID());
            Source->CastSpell(Source, BG_TP_SPELL_ALLIANCE_FLAG, true);
            _flagState[BG_TEAM_ALLIANCE] = BG_TP_FLAG_STATE_ON_PLAYER;
            UpdateFlagState(HORDE, BG_TP_FLAG_STATE_ON_PLAYER);
            if (_flagDebuffState == 1)
              Source->CastSpell(Source, TP_SPELL_FOCUSED_ASSAULT, true);
            if (_flagDebuffState == 2)
              Source->CastSpell(Source, TP_SPELL_BRUTAL_ASSAULT, true);
            UpdateWorldState(BG_TP_FLAG_UNK_ALLIANCE, 1);
        }
        //called in HandleGameObjectUseOpcode:
        //target_obj->Delete();
    }

    //Horde flag on ground(not in base) (returned or picked up again)
    if (GetFlagState(HORDE) == BG_TP_FLAG_STATE_ON_GROUND && Source->IsWithinDistInMap(target_obj, 10))
    {
        if (Source->GetTeam() == HORDE)
        {
            message_id = LANG_BG_TP_RETURNED_HF;
            type = CHAT_MSG_BG_SYSTEM_HORDE;
            UpdateFlagState(ALLIANCE, BG_TP_FLAG_STATE_WAIT_RESPAWN);
            RespawnFlag(HORDE, false);
            SpawnBGObject(BG_TP_OBJECT_H_FLAG, RESPAWN_IMMEDIATELY);
            PlaySoundToAll(BG_TP_SOUND_FLAG_RETURNED);
            UpdatePlayerScore(Source, SCORE_FLAG_RETURNS, 1);
            _bothFlagsKept = false;
        }
        else
        {
            message_id = LANG_BG_TP_PICKEDUP_HF;
            type = CHAT_MSG_BG_SYSTEM_ALLIANCE;
            PlaySoundToAll(BG_TP_SOUND_HORDE_FLAG_PICKED_UP);
            SpawnBGObject(BG_TP_OBJECT_H_FLAG, RESPAWN_ONE_DAY);
            SetHordeFlagPicker(Source->GetGUID());
            Source->CastSpell(Source, BG_TP_SPELL_HORDE_FLAG, true);
            _flagState[BG_TEAM_HORDE] = BG_TP_FLAG_STATE_ON_PLAYER;
            UpdateFlagState(ALLIANCE, BG_TP_FLAG_STATE_ON_PLAYER);
            if (_flagDebuffState == 1)
              Source->CastSpell(Source, TP_SPELL_FOCUSED_ASSAULT, true);
            if (_flagDebuffState == 2)
              Source->CastSpell(Source, TP_SPELL_BRUTAL_ASSAULT, true);
            UpdateWorldState(BG_TP_FLAG_UNK_HORDE, 1);
        }
        //called in HandleGameObjectUseOpcode:
        //target_obj->Delete();
    }

    if (!message_id)
        return;

    SendMessageToAll(message_id, type, Source);
    Source->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_ENTER_PVP_COMBAT);
}

void BattlegroundTP::RemovePlayer(Player* player, uint64 guid, uint32 /*team*/)
{
    // sometimes flag aura not removed :(
    if (IsAllianceFlagPickedup() && _flagKeepers[BG_TEAM_ALLIANCE] == guid)
    {
        if (!player)
        {
            sLog->outError("BattlegroundTP: Removing offline player who has the FLAG!!");
            this->SetAllianceFlagPicker(0);
            this->RespawnFlag(ALLIANCE, false);
        }
        else
            this->EventPlayerDroppedFlag(player);
    }
    if (IsHordeFlagPickedup() && _flagKeepers[BG_TEAM_HORDE] == guid)
    {
        if (!player)
        {
            sLog->outError("BattlegroundTP: Removing offline player who has the FLAG!!");
            this->SetHordeFlagPicker(0);
            this->RespawnFlag(HORDE, false);
        }
        else
            this->EventPlayerDroppedFlag(player);
    }
}

void BattlegroundTP::UpdateFlagState(uint32 team, uint32 value)
{
    if (team == ALLIANCE)
        UpdateWorldState(BG_TP_FLAG_STATE_ALLIANCE, value);
    else
        UpdateWorldState(BG_TP_FLAG_STATE_HORDE, value);
}

void BattlegroundTP::UpdateTeamScore(uint32 team)
{
    if (team == ALLIANCE)
        UpdateWorldState(BG_TP_FLAG_CAPTURES_ALLIANCE, GetTeamScore(team));
    else
        UpdateWorldState(BG_TP_FLAG_CAPTURES_HORDE, GetTeamScore(team));
}

void BattlegroundTP::HandleAreaTrigger(Player *Source, uint32 Trigger)
{
    // this is wrong way to implement these things. On official it done by gameobject spell cast.
    if (GetStatus() != STATUS_IN_PROGRESS)
        return;

    //uint32 SpellId = 0;
    //uint64 buff_guid = 0;
    switch (Trigger)
    {
        case 5904:                                          // Alliance Flag spawn
            if (_flagState[BG_TEAM_HORDE] && !_flagState[BG_TEAM_ALLIANCE])
                if (GetHordeFlagPickerGUID() == Source->GetGUID())
                    EventPlayerCapturedFlag(Source);
            break;
        case 5905:                                          // Horde Flag spawn
            if (_flagState[BG_TEAM_ALLIANCE] && !_flagState[BG_TEAM_HORDE])
                if (GetAllianceFlagPickerGUID() == Source->GetGUID())
                    EventPlayerCapturedFlag(Source);
            break;
        case 5908:                                          // Horde Tower
        case 5909:                                          // Twin Peak House big
        case 5910:                                          // Horde House
        case 5911:                                          // Twin Peak House small
        case 5914:                                          // Alliance Start right
        case 5916:                                          // Alliance Start
        case 5917:                                          // Alliance Start left
        case 5918:                                          // Horde Start
        case 5920:                                          // Horde Start Front entrance
        case 5921:                                          // Horde Start left Water channel
            break;
        default:
            sLog->outError("WARNING: Unhandled AreaTrigger in Battleground: %u", Trigger);
            Source->GetSession()->SendAreaTriggerMessage("Warning: Unhandled AreaTrigger in Battleground: %u", Trigger);
            break;
    }

    //if (buff_guid)
    //    HandleTriggerBuff(buff_guid, Source);
}

bool BattlegroundTP::SetupBattleground()
{
    // flags X Y Z Orientation Rotation2 Rotation3
    if (!AddObject(BG_TP_OBJECT_A_FLAG, BG_OBJECT_A_FLAG_TP_ENTRY, 2118.210f, 191.621f, 44.052f, 5.741259f, 0, 0, 0.9996573f, 0.02617699f, BG_TP_FLAG_RESPAWN_TIME/1000)
        || !AddObject(BG_TP_OBJECT_H_FLAG, BG_OBJECT_H_FLAG_TP_ENTRY, 1578.380f, 344.037f, 2.419f, 3.055978f, 0, 0, 0.008726535f, 0.9999619f, BG_TP_FLAG_RESPAWN_TIME/1000)
        // buffs
        || !AddObject(BG_TP_OBJECT_SPEEDBUFF_1, BG_OBJECTID_SPEEDBUFF_ENTRY, 1545.402f, 304.028f, 0.5923f, -1.64061f, 0, 0, 0.7313537f, -0.6819983f, BUFF_RESPAWN_TIME)
        || !AddObject(BG_TP_OBJECT_SPEEDBUFF_2, BG_OBJECTID_SPEEDBUFF_ENTRY, 2171.279f, 222.334f, 43.8001f, 2.663309f, 0, 0, 0.7313537f, 0.6819984f, BUFF_RESPAWN_TIME)
        || !AddObject(BG_TP_OBJECT_REGENBUFF_1, BG_OBJECTID_REGENBUFF_ENTRY, 1753.957f, 242.092f, -14.1170f, 1.105848f, 0, 0, 0.1305263f, -0.9914448f, BUFF_RESPAWN_TIME)
        || !AddObject(BG_TP_OBJECT_REGENBUFF_2, BG_OBJECTID_REGENBUFF_ENTRY, 1952.121f, 383.857f, -10.2870f, 4.192612f, 0, 0, 0.333807f, -0.9426414f, BUFF_RESPAWN_TIME)
        || !AddObject(BG_TP_OBJECT_BERSERKBUFF_1, BG_OBJECTID_BERSERKERBUFF_ENTRY, 1934.369f, 226.064f, -17.0441f, 2.499154f, 0, 0, 0.5591929f, 0.8290376f, BUFF_RESPAWN_TIME)
        || !AddObject(BG_TP_OBJECT_BERSERKBUFF_2, BG_OBJECTID_BERSERKERBUFF_ENTRY, 1725.240f, 446.431f, -7.8327f, 5.709677f, 0, 0, 0.9396926f, -0.3420201f, BUFF_RESPAWN_TIME)
        // alliance gates
        || !AddObject(BG_TP_OBJECT_DOOR_A_1, BG_OBJECT_DOOR_A_1_TP_ENTRY, 2115.399f, 150.175f, 43.526f, 2.544690f, 0, 0, 0, 0, RESPAWN_IMMEDIATELY)
        || !AddObject(BG_TP_OBJECT_DOOR_A_2, BG_OBJECT_DOOR_A_2_TP_ENTRY, 2156.803f, 220.331f, 43.482f, 2.544690f, 0, 0, 0, 0, RESPAWN_IMMEDIATELY)
        || !AddObject(BG_TP_OBJECT_DOOR_A_3, BG_OBJECT_DOOR_A_3_TP_ENTRY, 2127.512f, 223.711f, 43.640f, 2.544690f, 0, 0, 0, 0, RESPAWN_IMMEDIATELY)
        || !AddObject(BG_TP_OBJECT_DOOR_A_4, BG_OBJECT_DOOR_A_4_TP_ENTRY, 2096.102f, 166.920f, 54.230f, 2.544690f, 0, 0, 0, 0, RESPAWN_IMMEDIATELY)
        // horde gates
        || !AddObject(BG_TP_OBJECT_DOOR_H_1, BG_OBJECT_DOOR_H_1_TP_ENTRY, 1556.595f, 314.502f, 1.2230f, 6.179126f, 0, 0, 0, 0, RESPAWN_IMMEDIATELY)
        || !AddObject(BG_TP_OBJECT_DOOR_H_2, BG_OBJECT_DOOR_H_2_TP_ENTRY, 1587.093f, 319.853f, 1.5233f, 6.179126f, 0, 0, 0, 0, RESPAWN_IMMEDIATELY)
        || !AddObject(BG_TP_OBJECT_DOOR_H_3, BG_OBJECT_DOOR_H_3_TP_ENTRY, 1591.463f, 365.732f, 13.494f, 6.179126f, 0, 0, 0, 0, RESPAWN_IMMEDIATELY)
        || !AddObject(BG_TP_OBJECT_DOOR_H_4, BG_OBJECT_DOOR_H_4_TP_ENTRY, 1558.315f, 372.709f, 1.4840f, 6.179126f, 0, 0, 0, 0, RESPAWN_IMMEDIATELY)
       )
    {
        sLog->outErrorDb("BatteGroundTP: Failed to spawn some objects Battleground not created!");
        return false;
    }

    WorldSafeLocsEntry const *sg = sWorldSafeLocsStore.LookupEntry(TP_GRAVEYARD_MIDDLE_ALLIANCE);
    if (!sg || !AddSpiritGuide(TP_SPIRIT_ALLIANCE, sg->x, sg->y, sg->z, 3.641396f, ALLIANCE))
    {
        sLog->outErrorDb("BatteGroundTP: Failed to spawn Alliance spirit guides! Battleground not created!");
        return false;
    }

    sg = sWorldSafeLocsStore.LookupEntry(TP_GRAVEYARD_START_ALLIANCE);
    if (!sg || !AddSpiritGuide(TP_SPIRIT_ALLIANCE, sg->x, sg->y, sg->z, 3.641396f, ALLIANCE))
    {
        sLog->outErrorDb("BatteGroundTP: Failed to spawn Alliance start spirit guides! Battleground not created!");
        return false;
    }

    sg = sWorldSafeLocsStore.LookupEntry(TP_GRAVEYARD_MIDDLE_HORDE);
    if (!sg || !AddSpiritGuide(TP_SPIRIT_HORDE, sg->x, sg->y, sg->z, 3.641396f, HORDE))
    {
        sLog->outErrorDb("BatteGroundTP: Failed to spawn Horde spirit guides! Battleground not created!");
        return false;
    }

    sg = sWorldSafeLocsStore.LookupEntry(TP_GRAVEYARD_START_HORDE);
    if (!sg || !AddSpiritGuide(TP_SPIRIT_ALLIANCE, sg->x, sg->y, sg->z, 3.641396f, HORDE))
    {
        sLog->outErrorDb("BatteGroundTP: Failed to spawn Horde start spirit guides! Battleground not created!");
        return false;
    }

    sLog->outDebug(LOG_FILTER_BATTLEGROUND, "BatteGroundTP: BG objects and spirit guides spawned");

    return true;
}

void BattlegroundTP::Reset()
{
    //call parent's class reset
    Battleground::Reset();

    _flagKeepers[BG_TEAM_ALLIANCE]     = 0;
    _flagKeepers[BG_TEAM_HORDE]        = 0;
    _DroppedFlagGUID[BG_TEAM_ALLIANCE] = 0;
    _DroppedFlagGUID[BG_TEAM_HORDE]    = 0;
    _flagState[BG_TEAM_ALLIANCE]       = BG_TP_FLAG_STATE_ON_BASE;
    _flagState[BG_TEAM_HORDE]          = BG_TP_FLAG_STATE_ON_BASE;
    _TeamScores[BG_TEAM_ALLIANCE]      = 0;
    _TeamScores[BG_TEAM_HORDE]         = 0;
    bool isBGWeekend = sBattlegroundMgr->IsBGWeekend(GetTypeID());
    m_ReputationCapture = (isBGWeekend) ? 45 : 35;
    m_HonorWinKills = (isBGWeekend) ? 3 : 1;
    m_HonorEndKills = (isBGWeekend) ? 4 : 2;
    // For WorldState
    _minutesElapsed                    = 0;
    _lastFlagCaptureTeam              = 0;

    /* Spirit nodes is static at this BG and then not required deleting at BG reset.
    if (BgCreatures[TP_SPIRIT_MAIN_ALLIANCE])
        DelCreature(TP_SPIRIT_MAIN_ALLIANCE);
    if (BgCreatures[TP_SPIRIT_MAIN_HORDE])
        DelCreature(TP_SPIRIT_MAIN_HORDE);
    */
}

void BattlegroundTP::EndBattleground(uint32 winner)
{
    //win reward
    if (winner == ALLIANCE)
        RewardHonorToTeam(GetBonusHonorFromKill(m_HonorWinKills), ALLIANCE);
    if (winner == HORDE)
        RewardHonorToTeam(GetBonusHonorFromKill(m_HonorWinKills), HORDE);
    //complete map_end rewards (even if no team wins)
    RewardHonorToTeam(GetBonusHonorFromKill(m_HonorEndKills), ALLIANCE);
    RewardHonorToTeam(GetBonusHonorFromKill(m_HonorEndKills), HORDE);

    Battleground::EndBattleground(winner);
}

void BattlegroundTP::HandleKillPlayer(Player* player, Player* killer)
{
    if (GetStatus() != STATUS_IN_PROGRESS)
        return;

    EventPlayerDroppedFlag(player);

    Battleground::HandleKillPlayer(player, killer);
}

void BattlegroundTP::UpdatePlayerScore(Player* Source, uint32 type, uint32 value, bool doAddHonor)
{
    BattlegroundScoreMap::iterator itr = PlayerScores.find(Source->GetGUID());
    if (itr == PlayerScores.end())                         // player not found
        return;

    switch (type)
    {
        case SCORE_FLAG_CAPTURES:                           // flags captured
            ((BattlegroundTPScore*)itr->second)->FlagCaptures += value;
            Source->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_BG_OBJECTIVE_CAPTURE, TP_OBJECTIVE_CAPTURE_FLAG);
            break;
        case SCORE_FLAG_RETURNS:                            // flags returned
            ((BattlegroundTPScore*)itr->second)->FlagReturns += value;
            Source->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_BG_OBJECTIVE_CAPTURE, TP_OBJECTIVE_RETURN_FLAG);
            break;
        default:
            Battleground::UpdatePlayerScore(Source, type, value, doAddHonor);
            break;
    }
}

WorldSafeLocsEntry const* BattlegroundTP::GetClosestGraveYard(Player* player)
{
    // if status in progress, it returns main or start graveyards with spiritguides
    // else it will return the graveyard in the flagroom - this is especially good
    // if a player dies in preparation phase - then the player can't cheat
    // and teleport to the graveyard outside the flagroom
    // and start running around, while the doors are still closed
    if (player->GetTeam() == ALLIANCE)
    {
        if (GetStatus() == STATUS_IN_PROGRESS)
        {
            WorldSafeLocsEntry const* ret;
            WorldSafeLocsEntry const* closest;
            float dist, nearest;
            float x, y, z;

            player->GetPosition(x, y, z);

            closest = sWorldSafeLocsStore.LookupEntry(TP_GRAVEYARD_MIDDLE_ALLIANCE);
            nearest = sqrt((closest->x - x)*(closest->x - x) + (closest->y - y)*(closest->y - y)+(closest->z - z)*(closest->z - z));

            ret = sWorldSafeLocsStore.LookupEntry(TP_GRAVEYARD_START_ALLIANCE);
            dist = sqrt((ret->x - x)*(ret->x - x) + (ret->y - y)*(ret->y - y)+(ret->z - z)*(ret->z - z));

            if (dist < nearest)
            {
                closest = ret;
                nearest = dist;
            }

            return closest;
        }
        else
            return sWorldSafeLocsStore.LookupEntry(TP_GRAVEYARD_FLAGROOM_ALLIANCE);
    }
    else
    {
        if (GetStatus() == STATUS_IN_PROGRESS)
            return sWorldSafeLocsStore.LookupEntry(TP_GRAVEYARD_MIDDLE_HORDE);
        else
            return sWorldSafeLocsStore.LookupEntry(TP_GRAVEYARD_FLAGROOM_HORDE);
    }
}

void BattlegroundTP::FillInitialWorldStates(WorldPacket& data)
{
    data << uint32(BG_TP_FLAG_CAPTURES_ALLIANCE) << uint32(GetTeamScore(ALLIANCE));
    data << uint32(BG_TP_FLAG_CAPTURES_HORDE) << uint32(GetTeamScore(HORDE));

    if (_flagState[BG_TEAM_ALLIANCE] == BG_TP_FLAG_STATE_ON_GROUND)
        data << uint32(BG_TP_FLAG_UNK_ALLIANCE) << uint32(-1);
    else if (_flagState[BG_TEAM_ALLIANCE] == BG_TP_FLAG_STATE_ON_PLAYER)
        data << uint32(BG_TP_FLAG_UNK_ALLIANCE) << uint32(1);
    else
        data << uint32(BG_TP_FLAG_UNK_ALLIANCE) << uint32(0);

    if (_flagState[BG_TEAM_HORDE] == BG_TP_FLAG_STATE_ON_GROUND)
        data << uint32(BG_TP_FLAG_UNK_HORDE) << uint32(-1);
    else if (_flagState[BG_TEAM_HORDE] == BG_TP_FLAG_STATE_ON_PLAYER)
        data << uint32(BG_TP_FLAG_UNK_HORDE) << uint32(1);
    else
        data << uint32(BG_TP_FLAG_UNK_HORDE) << uint32(0);

    data << uint32(BG_TP_FLAG_CAPTURES_MAX) << uint32(BG_TP_MAX_TEAM_SCORE);

    if (GetStatus() == STATUS_IN_PROGRESS)
    {
        data << uint32(BG_TP_STATE_TIMER_ACTIVE) << uint32(1);
        data << uint32(BG_TP_STATE_TIMER) << uint32(25-_minutesElapsed);
    }
    else
        data << uint32(BG_TP_STATE_TIMER_ACTIVE) << uint32(0);

    if (_flagState[BG_TEAM_HORDE] == BG_TP_FLAG_STATE_ON_PLAYER)
        data << uint32(BG_TP_FLAG_STATE_ALLIANCE) << uint32(2);
    else
        data << uint32(BG_TP_FLAG_STATE_ALLIANCE) << uint32(1);

    if (_flagState[BG_TEAM_ALLIANCE] == BG_TP_FLAG_STATE_ON_PLAYER)
        data << uint32(BG_TP_FLAG_STATE_HORDE) << uint32(2);
    else
        data << uint32(BG_TP_FLAG_STATE_HORDE) << uint32(1);
}
