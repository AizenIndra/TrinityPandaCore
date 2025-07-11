/*
 * Copyright (C) 2011-2016 Project SkyFire <http://www.projectskyfire.org/>
 * Copyright (C) 2008-2016 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2005-2016 MaNGOS <http://getmangos.com/>
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

#ifndef SCRIPTMGR_H
#define SCRIPTMGR_H

#include "Common.h"
#include <ace/Singleton.h>
#include <ace/Atomic_Op.h>

#include "DBCStores.h"
#include "SharedDefines.h"
#include "World.h"
#include "Weather.h"
#include "ItemPrototype.h"
#include "WorldStateBuilder.h"

class AreaTrigger;
class AuctionHouseObject;
class AuraScript;
class Battleground;
class BattlegroundMap;
class Channel;
class ChatCommand;
class Creature;
class CreatureAI;
class DynamicObject;
class GameObject;
class GameObjectAI;
class Guild;
class GridMap;
class Group;
class IAreaTrigger;
class InstanceMap;
class InstanceScript;
class Item;
class Map;
class OutdoorPvP;
class Player;
class Quest;
class ScriptMgr;
class Spell;
class SpellScript;
class SpellCastTargets;
class Transport;
class Unit;
class Vehicle;
class WorldPacket;
class WorldSocket;
class WorldObject;

struct AuctionEntry;
struct ConditionSourceInfo;
struct Condition;
struct ItemTemplate;
struct OutdoorPvPData;
struct SceneTemplate;
struct GameEventData;

#define VISIBLE_RANGE       166.0f                          //MAX visible range (size of grid)


/*
    @todo Add more script type classes.

    MailScript
    SessionScript
    CollisionScript
    ArenaTeamScript

*/

/*
    Standard procedure when adding new script type classes:

    First of all, define the actual class, and have it inherit from ScriptObject, like so:

    class MyScriptType : public ScriptObject
    {
        uint32 _someId;

        private:

            void RegisterSelf();

        protected:

            MyScriptType(const char* name, uint32 someId)
                : ScriptObject(name), _someId(someId)
            {
                ScriptRegistry<MyScriptType>::AddScript(this);
            }

        public:

            // If a virtual function in your script type class is not necessarily
            // required to be overridden, just declare it virtual with an empty
            // body. If, on the other hand, it's logical only to override it (i.e.
            // if it's the only method in the class), make it pure virtual, by adding
            // = 0 to it.
            virtual void OnSomeEvent(uint32 someArg1, std::string& someArg2) { }

            // This is a pure virtual function:
            virtual void OnAnotherEvent(uint32 someArg) = 0;
    }

    Next, you need to add a specialization for ScriptRegistry. Put this in the bottom of
    ScriptMgr.cpp:

    template class ScriptRegistry<MyScriptType>;

    Now, add a cleanup routine in ScriptMgr::~ScriptMgr:

    SCR_CLEAR(MyScriptType);

    Now your script type is good to go with the script system. What you need to do now
    is add functions to ScriptMgr that can be called from the core to actually trigger
    certain events. For example, in ScriptMgr.h:

    void OnSomeEvent(uint32 someArg1, std::string& someArg2);
    void OnAnotherEvent(uint32 someArg);

    In ScriptMgr.cpp:

    void ScriptMgr::OnSomeEvent(uint32 someArg1, std::string& someArg2)
    {
        FOREACH_SCRIPT(MyScriptType)->OnSomeEvent(someArg1, someArg2);
    }

    void ScriptMgr::OnAnotherEvent(uint32 someArg)
    {
        FOREACH_SCRIPT(MyScriptType)->OnAnotherEvent(someArg1, someArg2);
    }

    Now you simply call these two functions from anywhere in the core to trigger the
    event on all registered scripts of that type.
*/

class ScriptObject
{
    friend class ScriptMgr;

    public:

        // Do not override this in scripts; it should be overridden by the various script type classes. It indicates
        // whether or not this script type must be assigned in the database.
        virtual bool IsDatabaseBound() const { return false; }

        const std::string& GetName() const { return _name; }

    protected:

        ScriptObject(const char* name)
            : _name(name)
        {
        }

        virtual ~ScriptObject()
        {
        }

    private:

        const std::string _name;
};

template<class TObject> class UpdatableScript
{
    protected:

        UpdatableScript()
        {
        }

    public:

        virtual void OnUpdate(TObject* /*obj*/, uint32 /*diff*/) { }
};

class SpellScriptLoader : public ScriptObject
{
    protected:

        SpellScriptLoader(const char* name);

    public:

        bool IsDatabaseBound() const final { return true; }

        // Should return a fully valid SpellScript pointer.
        virtual SpellScript* GetSpellScript() const { return nullptr; }

        // Should return a fully valid AuraScript pointer.
        virtual AuraScript* GetAuraScript() const { return nullptr; }
};

class ServerScript : public ScriptObject
{
    protected:

        ServerScript(const char* name);

    public:

        // Called when reactive socket I/O is started (WorldSocketMgr).
        virtual void OnNetworkStart() { }

        // Called when reactive I/O is stopped.
        virtual void OnNetworkStop() { }

        // Called when a remote socket establishes a connection to the server. Do not store the socket object.
        virtual void OnSocketOpen(WorldSocket* /*socket*/) { }

        // Called when a socket is closed. Do not store the socket object, and do not rely on the connection
        // being open; it is not.
        virtual void OnSocketClose(WorldSocket* /*socket*/, bool /*wasNew*/) { }

        // Called when a packet is sent to a client. The packet object is a copy of the original packet, so reading
        // and modifying it is safe.
        virtual void OnPacketSend(WorldSocket* /*socket*/, WorldPacket& /*packet*/) { }

        // Called when a (valid) packet is received by a client. The packet object is a copy of the original packet, so
        // reading and modifying it is safe.
        virtual void OnPacketReceive(WorldSocket* /*socket*/, WorldPacket& /*packet*/) { }

        // Called when an invalid (unknown opcode) packet is received by a client. The packet is a reference to the orignal
        // packet; not a copy. This allows you to actually handle unknown packets (for whatever purpose).
        virtual void OnUnknownPacketReceive(WorldSocket* /*socket*/, WorldPacket& /*packet*/) { }
};

class WorldScript : public ScriptObject
{
    protected:

        WorldScript(const char* name);

    public:

        // Called when the open/closed state of the world changes.
        virtual void OnOpenStateChange(bool /*open*/) { }

        // Called after the world configuration is (re)loaded.
        virtual void OnConfigLoad(bool /*reload*/) { }

        // Called before the message of the day is changed.
        virtual void OnMotdChange(std::string& /*newMotd*/) { }

        // Called when a world shutdown is initiated.
        virtual void OnShutdownInitiate(ShutdownExitCode /*code*/, ShutdownMask /*mask*/) { }

        // Called when a world shutdown is cancelled.
        virtual void OnShutdownCancel() { }

        // Called on every world tick (don't execute too heavy code here).
        virtual void OnUpdate(uint32 /*diff*/) { }

        // Called when the world is started.
        virtual void OnStartup() { }

        // Called when the world is actually shut down.
        virtual void OnShutdown() { }

        // Called when confing of world reloading
        virtual void OnAfterConfigLoad(bool reload = false) { }

        // Callen before confing of world reloading
        virtual void OnBeforeConfigLoad(bool reload = false) { }
};

class FormulaScript : public ScriptObject
{
    protected:

        FormulaScript(const char* name);

    public:

        // Called after calculating honor.
        virtual void OnHonorCalculation(float& /*honor*/, uint8 /*level*/, float /*multiplier*/) { }

        // Called after gray level calculation.
        virtual void OnGrayLevelCalculation(uint8& /*grayLevel*/, uint8 /*playerLevel*/) { }

        // Called after calculating experience color.
        virtual void OnColorCodeCalculation(XPColorChar& /*color*/, uint8 /*playerLevel*/, uint8 /*mobLevel*/) { }

        // Called after calculating zero difference.
        virtual void OnZeroDifferenceCalculation(uint8& /*diff*/, uint8 /*playerLevel*/) { }

        // Called after calculating base experience gain.
        virtual void OnBaseGainCalculation(uint32& /*gain*/, uint8 /*playerLevel*/, uint8 /*mobLevel*/, ContentLevels /*content*/) { }

        // Called after calculating experience gain.
        virtual void OnGainCalculation(uint32& /*gain*/, Player* /*player*/, Unit* /*unit*/) { }

        // Called when calculating the experience rate for group experience.
        virtual void OnGroupRateCalculation(float& /*rate*/, uint32 /*count*/, bool /*isRaid*/) { }
};

template<class TMap> class MapScript : public UpdatableScript<TMap>
{
    MapEntry const* _mapEntry;

    protected:

        MapScript(uint32 mapId)
            : _mapEntry(sMapStore.LookupEntry(mapId))
        {
            if (!_mapEntry)
                TC_LOG_ERROR("scripts", "Invalid MapScript for %u; no such map ID.", mapId);
        }

    public:

        // Gets the MapEntry structure associated with this script. Can return NULL.
        MapEntry const* GetEntry() { return _mapEntry; }

        // Called when the map is created.
        virtual void OnCreate(TMap* /*map*/) { }

        // Called just before the map is destroyed.
        virtual void OnDestroy(TMap* /*map*/) { }

        // Called when a grid map is loaded.
        virtual void OnLoadGridMap(TMap* /*map*/, GridMap* /*gmap*/, uint32 /*gx*/, uint32 /*gy*/) { }

        // Called when a grid map is unloaded.
        virtual void OnUnloadGridMap(TMap* /*map*/, GridMap* /*gmap*/, uint32 /*gx*/, uint32 /*gy*/)  { }

        // Called when a player enters the map.
        virtual void OnPlayerEnter(TMap* /*map*/, Player* /*player*/) { }

        // Called when a player leaves the map.
        virtual void OnPlayerLeave(TMap* /*map*/, Player* /*player*/) { }
};

class WorldMapScript : public ScriptObject, public MapScript<Map>
{
    protected:

        WorldMapScript(const char* name, uint32 mapId);
};

class InstanceMapScript : public ScriptObject, public MapScript<InstanceMap>
{
    protected:

        InstanceMapScript(const char* name, uint32 mapId);

    public:

        bool IsDatabaseBound() const final { return true; }

        // Gets an InstanceScript object for this instance.
        virtual InstanceScript* GetInstanceScript(InstanceMap* /*map*/) const { return NULL; }
};

class BattlegroundMapScript : public ScriptObject, public MapScript<BattlegroundMap>
{
    protected:

        BattlegroundMapScript(const char* name, uint32 mapId);
};

class ItemScript : public ScriptObject
{
    protected:

        ItemScript(const char* name);

    public:

        bool IsDatabaseBound() const final { return true; }

        // Called when a dummy spell effect is triggered on the item.
        virtual bool OnDummyEffect(Unit* /*caster*/, uint32 /*spellId*/, SpellEffIndex /*effIndex*/, Item* /*target*/) { return false; }

        // Called when a player accepts a quest from the item.
        virtual bool OnQuestAccept(Player* /*player*/, Item* /*item*/, Quest const* /*quest*/) { return false; }

        // Called when a player uses the item.
        virtual bool OnUse(Player* /*player*/, Item* /*item*/, SpellCastTargets const& /*targets*/) { return false; }

        // Called when the item expires (is destroyed).
        virtual bool OnExpire(Player* /*player*/, ItemTemplate const* /*proto*/) { return false; }

        // Called when the item is obtained by the player.
        virtual bool OnPickup(Player* /*player*/, Item* /*item*/, ItemPickupSourceType /*sourceType*/, uint32 /*sourceId*/) { return false; }

        // Called when a player selects an option in an item gossip window
        virtual void OnGossipSelect(Player* /*player*/, Item* /*item*/, uint32 /*sender*/, uint32 /*action*/) { }

        // Called when a player selects an option in an item gossip window
        virtual void OnGossipSelectCode(Player* /*player*/, Item* /*item*/, uint32 /*sender*/, uint32 /*action*/, const char* /*code*/) { }
};

class UnitScript : public ScriptObject
{
    protected:

        UnitScript(const char* name, bool addToScripts = true);

    public:
        // Called when a unit deals healing to another unit
        virtual void OnHeal(Unit* /*healer*/, Unit* /*reciever*/, uint32& /*gain*/) { }

        // Called when a unit deals damage to another unit
        virtual void OnDamage(Unit* /*attacker*/, Unit* /*victim*/, uint32& /*damage*/) { }

        // Called when DoT's Tick Damage is being Dealt
        virtual void ModifyPeriodicDamageAurasTick(Unit* /*target*/, Unit* /*attacker*/, int32& /*damage*/) { }

        // Called when Melee Damage is being Dealt
        virtual void ModifyMeleeDamage(Unit* /*target*/, Unit* /*attacker*/, uint32& /*damage*/) { }

        // Called when Spell Damage is being Dealt
        virtual void ModifySpellDamageTaken(Unit* /*target*/, Unit* /*attacker*/, int32& /*damage*/) { }
};

class CreatureScript : public UnitScript, public UpdatableScript<Creature>
{
    protected:

        CreatureScript(const char* name);

    public:

        bool IsDatabaseBound() const final { return true; }

        // Called when a dummy spell effect is triggered on the creature.
        virtual bool OnDummyEffect(Unit* /*caster*/, uint32 /*spellId*/, SpellEffIndex /*effIndex*/, Creature* /*target*/) { return false; }

        // Called when a player opens a gossip dialog with the creature.
        virtual bool OnGossipHello(Player* /*player*/, Creature* /*creature*/) { return false; }

        // Called when a player selects a gossip item in the creature's gossip menu.
        virtual bool OnGossipSelect(Player* /*player*/, Creature* /*creature*/, uint32 /*sender*/, uint32 /*action*/) { return false; }

        // Called when a player selects a gossip with a code in the creature's gossip menu.
        virtual bool OnGossipSelectCode(Player* /*player*/, Creature* /*creature*/, uint32 /*sender*/, uint32 /*action*/, const char* /*code*/) { return false; }

        // Called when a player accepts a quest from the creature.
        virtual bool OnQuestAccept(Player* /*player*/, Creature* /*creature*/, Quest const* /*quest*/) { return false; }

        // Called when a player selects a quest in the creature's quest menu.
        virtual bool OnQuestSelect(Player* /*player*/, Creature* /*creature*/, Quest const* /*quest*/) { return false; }

        // Called when a player completes a quest with the creature.
        virtual bool OnQuestComplete(Player* /*player*/, Creature* /*creature*/, Quest const* /*quest*/) { return false; }

        // Called when a player selects a quest reward.
        virtual bool OnQuestReward(Player* /*player*/, Creature* /*creature*/, Quest const* /*quest*/, uint32 /*opt*/) { return false; }

        // Called when the dialog status between a player and the creature is requested.
        virtual uint32 GetDialogStatus(Player* /*player*/, Creature* /*creature*/) { return 100; }

        // Called when a CreatureAI object is needed for the creature.
        virtual CreatureAI* GetAI(Creature* /*creature*/) const { return NULL; }
};

class GameObjectScript : public ScriptObject, public UpdatableScript<GameObject>
{
    protected:

        GameObjectScript(const char* name);

    public:

        bool IsDatabaseBound() const final { return true; }

        // Called when a dummy spell effect is triggered on the gameobject.
        virtual bool OnDummyEffect(Unit* /*caster*/, uint32 /*spellId*/, SpellEffIndex /*effIndex*/, GameObject* /*target*/) { return false; }

        // Called when a player opens a gossip dialog with the gameobject.
        virtual bool OnGossipHello(Player* /*player*/, GameObject* /*go*/) { return false; }

        // Called when a player clicks a gameobject, before OnGossipHello.
        virtual bool OnReportUse(Player* /*player*/, GameObject* /*go*/) { return false; }

        // Called when a player selects a gossip item in the gameobject's gossip menu.
        virtual bool OnGossipSelect(Player* /*player*/, GameObject* /*go*/, uint32 /*sender*/, uint32 /*action*/) { return false; }

        // Called when a player selects a gossip with a code in the gameobject's gossip menu.
        virtual bool OnGossipSelectCode(Player* /*player*/, GameObject* /*go*/, uint32 /*sender*/, uint32 /*action*/, const char* /*code*/) { return false; }

        // Called when a player accepts a quest from the gameobject.
        virtual bool OnQuestAccept(Player* /*player*/, GameObject* /*go*/, Quest const* /*quest*/) { return false; }

        // Called when a player selects a quest reward.
        virtual bool OnQuestReward(Player* /*player*/, GameObject* /*go*/, Quest const* /*quest*/, uint32 /*opt*/) { return false; }

        // Called when the dialog status between a player and the gameobject is requested.
        virtual uint32 GetDialogStatus(Player* /*player*/, GameObject* /*go*/) { return 100; }

        // Called when the game object is destroyed (destructible buildings only).
        virtual void OnDestroyed(GameObject* /*go*/, Player* /*player*/) { }

        // Called when the game object is damaged (destructible buildings only).
        virtual void OnDamaged(GameObject* /*go*/, Player* /*player*/) { }

        // Called when the game object loot state is changed.
        virtual void OnLootStateChanged(GameObject* /*go*/, uint32 /*state*/, Unit* /*unit*/) { }

        // Called when the game object state is changed.
        virtual void OnGameObjectStateChanged(GameObject* /*go*/, uint32 /*state*/) { }

        // Called when a GameObjectAI object is needed for the gameobject.
        virtual GameObjectAI* GetAI(GameObject* /*go*/) const { return NULL; }
};

class AreaTriggerScript : public ScriptObject
{
    protected:

        AreaTriggerScript(const char* name);

    public:

        bool IsDatabaseBound() const final { return true; }

        // Called when the area trigger is activated by a player.
        virtual bool OnTrigger(Player* /*player*/, AreaTriggerEntry const* /*trigger*/) { return false; }
};

class SpellAreaTriggerScript : public ScriptObject
{
    protected:

        SpellAreaTriggerScript(const char* name);

    public:

        bool IsDatabaseBound() const final { return true; }

        // Called when the area trigger is created
        virtual IAreaTrigger* GetInterface() const { return NULL; }
};

class BattlegroundScript : public ScriptObject
{
    protected:

        BattlegroundScript(const char* name);

    public:

        bool IsDatabaseBound() const final { return true; }

        // Should return a fully valid Battleground object for the type ID.
        virtual Battleground* GetBattleground() const = 0;
};

class OutdoorPvPScript : public ScriptObject
{
    protected:

        OutdoorPvPScript(const char* name);

    public:

        bool IsDatabaseBound() const final { return true; }

        // Should return a fully valid OutdoorPvP object for the type ID.
        virtual OutdoorPvP* GetOutdoorPvP() const = 0;
};

class CommandScript : public ScriptObject
{
    protected:

        CommandScript(const char* name);

    public:

        // Should return a pointer to a valid command table (ChatCommand array) to be used by ChatHandler.
        virtual std::vector<ChatCommand> GetCommands() const = 0;
};

class WeatherScript : public ScriptObject, public UpdatableScript<Weather>
{
    protected:

        WeatherScript(const char* name);

    public:

        bool IsDatabaseBound() const final { return true; }

        // Called when the weather changes in the zone this script is associated with.
        virtual void OnChange(Weather* /*weather*/, WeatherState /*state*/, float /*grade*/) { }
};

class AuctionHouseScript : public ScriptObject
{
    protected:

        AuctionHouseScript(const char* name);

    public:

        // Called when an auction is added to an auction house.
        virtual void OnAuctionAdd(AuctionHouseObject* /*ah*/, AuctionEntry* /*entry*/) { }

        // Called when an auction is removed from an auction house.
        virtual void OnAuctionRemove(AuctionHouseObject* /*ah*/, AuctionEntry* /*entry*/) { }

        // Called when an auction was succesfully completed.
        virtual void OnAuctionSuccessful(AuctionHouseObject* /*ah*/, AuctionEntry* /*entry*/) { }

        // Called when an auction expires.
        virtual void OnAuctionExpire(AuctionHouseObject* /*ah*/, AuctionEntry* /*entry*/) { }
};

class ConditionScript : public ScriptObject
{
    protected:

        ConditionScript(const char* name);

    public:

        bool IsDatabaseBound() const final { return true; }

        // Called when a single condition is checked for a player.
        virtual bool OnConditionCheck(Condition* /*condition*/, ConditionSourceInfo& /*sourceInfo*/) { return true; }
};

class DynamicObjectScript : public ScriptObject, public UpdatableScript<DynamicObject>
{
    protected:

        DynamicObjectScript(const char* name);
};

class TransportScript : public ScriptObject, public UpdatableScript<Transport>
{
    protected:

        TransportScript(const char* name);

    public:

        bool IsDatabaseBound() const final { return true; }

        // Called when the transport is first created, before being added to map.
        virtual void OnCreate(Transport* transport) { }

        // Called when a player boards the transport.
        virtual void OnAddPassenger(Transport* /*transport*/, Player* /*player*/) { }

        // Called when a creature boards the transport.
        virtual void OnAddCreaturePassenger(Transport* /*transport*/, Creature* /*creature*/) { }

        // Called when a player exits the transport.
        virtual void OnRemovePassenger(Transport* /*transport*/, Player* /*player*/) { }

        // Called when a transport moves.
        virtual void OnRelocate(Transport* /*transport*/, uint32 /*waypointId*/, uint32 /*mapId*/, float /*x*/, float /*y*/, float /*z*/) { }
};

class AchievementCriteriaScript : public ScriptObject
{
    protected:

        AchievementCriteriaScript(const char* name);

    public:

        bool IsDatabaseBound() const final { return true; }

        // Called when an additional criteria is checked.
        virtual bool OnCheck(Player* source, Unit* target) = 0;
};

class PlayerScript : public UnitScript
{
    protected:

        PlayerScript(const char* name);

    public:

        // Called when a player kills another player
        virtual void OnPVPKill(Player* /*killer*/, Player* /*killed*/) { }

        // Called when a player kills a creature
        virtual void OnCreatureKill(Player* /*killer*/, Creature* /*killed*/) { }

        // Called when a player is killed by a creature
        virtual void OnPlayerKilledByCreature(Creature* /*killer*/, Player* /*killed*/) { }

        // Called when a player's level changes (after the level is applied)
        virtual void OnLevelChanged(Player* /*player*/, uint8 /*oldLevel*/) { }

        // Called when a player's item changes
        virtual void OnEquipChanged(Player* /*player*/, uint32 /*item*/) { }

        // Called when a player's free talent points change (right before the change is applied)
        virtual void OnFreeTalentPointsChanged(Player* /*player*/, uint32 /*points*/) { }

        // Called when a player's talent points are reset (right before the reset is done)
        virtual void OnTalentsReset(Player* /*player*/, bool /*noCost*/) { }

        // Called when a player's money is modified (before the modification is done)
        virtual void OnMoneyChanged(Player* /*player*/, int64& /*amount*/) { }

        // Called when a player gains XP (before anything is given)
        virtual void OnGiveXP(Player* /*player*/, uint32& /*amount*/, Unit* /*victim*/) { }

        // Called when a player's reputation changes (before it is actually changed)
        virtual void OnReputationChange(Player* /*player*/, uint32 /*factionId*/, float& /*standing*/, bool /*incremental*/) { }

        // Called when a player accepts a new quest
        virtual void OnQuestAdded(Player* /*player*/, const Quest* /*quest*/) { }

        // Called when a player abandons a quest
        virtual void OnQuestAbandoned(Player* /*player*/, const Quest* /*quest*/) { }

        // Called when a player completes a quest
        virtual void OnQuestCompleted(Player* /*player*/, const Quest* /*quest*/) { }

        // Called when a player fails a quest
        virtual void OnQuestFailed(Player* /*player*/, const Quest* /*quest*/) { }

        // Called when a player gets rewarded for a quest
        virtual void OnQuestRewarded(Player* /*player*/, const Quest* /*quest*/) { }

        // Called when a duel is requested
        virtual void OnDuelRequest(Player* /*target*/, Player* /*challenger*/) { }

        // Called when a duel starts (after 3s countdown)
        virtual void OnDuelStart(Player* /*player1*/, Player* /*player2*/) { }

        // Called when a duel ends
        virtual void OnDuelEnd(Player* /*winner*/, Player* /*loser*/, DuelCompleteType /*type*/) { }

        // The following methods are called when a player sends a chat message.
        virtual void OnChat(Player* /*player*/, uint32 /*type*/, uint32 /*lang*/, std::string& /*msg*/) { }

        virtual void OnChat(Player* /*player*/, uint32 /*type*/, uint32 /*lang*/, std::string& /*msg*/, Player* /*receiver*/) { }

        virtual void OnChat(Player* /*player*/, uint32 /*type*/, uint32 /*lang*/, std::string& /*msg*/, Group* /*group*/) { }

        virtual void OnChat(Player* /*player*/, uint32 /*type*/, uint32 /*lang*/, std::string& /*msg*/, Guild* /*guild*/) { }

        virtual void OnChat(Player* /*player*/, uint32 /*type*/, uint32 /*lang*/, std::string& /*msg*/, Channel* /*channel*/) { }

        // Both of the below are called on emote opcodes.
        virtual void OnEmote(Player* /*player*/, uint32 /*emote*/) { }

        virtual void OnTextEmote(Player* /*player*/, uint32 /*textEmote*/, uint32 /*emoteNum*/, uint64 /*guid*/) { }

        // Called in Spell::Cast.
        virtual void OnSpellCast(Player* /*player*/, Spell* /*spell*/, bool /*skipCheck*/) { }

        // Called when a player logs in.
        virtual void OnLogin(Player* /*player*/) { }

        // Called when a player logs out.
        virtual void OnLogout(Player* /*player*/) { }

        // Called when a player is created.
        virtual void OnCreate(Player* /*player*/) { }

        // Called when a player is deleted.
        virtual void OnDelete(uint64 /*guid*/) { }

        // Called when a player is about to be saved.
        virtual void OnSave(Player* /*player*/) { }

        // Called when a player is bound to an instance
        virtual void OnBindToInstance(Player* /*player*/, Difficulty /*difficulty*/, uint32 /*mapId*/, bool /*permanent*/) { }

        // Called when a player switches to a new zone
        virtual void OnUpdateZone(Player* /*player*/, uint32 /*newZone*/, uint32 /*newArea*/) { }

        // Called when a player changes to a new map (after moving to new map)
        virtual void OnMapChanged(Player* /*player*/) { }

		virtual void OnUpdate(Player* /*player*/, uint32 /*diff*/) { }

        // Called when a player selects an option in a player gossip window
        virtual void OnGossipSelect(Player* /*player*/, uint32 /*menu_id*/, uint32 /*sender*/, uint32 /*action*/) { }

        // Called when a player selects an option in a player gossip window
        virtual void OnGossipSelectCode(Player* /*player*/, uint32 /*menu_id*/, uint32 /*sender*/, uint32 /*action*/, const char* /*code*/) { }

        // Called when player accepts some quest
        virtual void OnQuestAccept(Player* /*player*/, Quest const* /*quest*/) {}
};

class GuildScript : public ScriptObject
{
    protected:

        GuildScript(const char* name);

    public:

        bool IsDatabaseBound() const final { return false; }

        // Called when a member is added to the guild.
        virtual void OnAddMember(Guild* /*guild*/, Player* /*player*/, uint8& /*plRank*/) { }

        // Called when a member is removed from the guild.
        virtual void OnRemoveMember(Guild* /*guild*/, Player* /*player*/, bool /*isDisbanding*/, bool /*isKicked*/) { }

        // Called when the guild MOTD (message of the day) changes.
        virtual void OnMOTDChanged(Guild* /*guild*/, const std::string& /*newMotd*/) { }

        // Called when the guild info is altered.
        virtual void OnInfoChanged(Guild* /*guild*/, const std::string& /*newInfo*/) { }

        // Called when a guild is created.
        virtual void OnCreate(Guild* /*guild*/, Player* /*leader*/, const std::string& /*name*/) { }

        // Called when a guild is disbanded.
        virtual void OnDisband(Guild* /*guild*/) { }

        // Called when a guild member withdraws money from a guild bank.
        virtual void OnMemberWitdrawMoney(Guild* /*guild*/, Player* /*player*/, uint64& /*amount*/, bool /*isRepair*/) { }

        // Called when a guild member deposits money in a guild bank.
        virtual void OnMemberDepositMoney(Guild* /*guild*/, Player* /*player*/, uint64& /*amount*/) { }

        // Called when a guild member moves an item in a guild bank.
        virtual void OnItemMove(Guild* /*guild*/, Player* /*player*/, Item* /*pItem*/, bool /*isSrcBank*/, uint8 /*srcContainer*/, uint8 /*srcSlotId*/,
            bool /*isDestBank*/, uint8 /*destContainer*/, uint8 /*destSlotId*/) { }

        virtual void OnEvent(Guild* /*guild*/, uint8 /*eventType*/, uint32 /*playerGuid1*/, uint32 /*playerGuid2*/, uint8 /*newRank*/) { }

        virtual void OnBankEvent(Guild* /*guild*/, uint8 /*eventType*/, uint8 /*tabId*/, uint32 /*playerGuid*/, uint32 /*itemOrMoney*/, uint16 /*itemStackCount*/, uint8 /*destTabId*/) { }
};

class GroupScript : public ScriptObject
{
    protected:

        GroupScript(const char* name);

    public:

        bool IsDatabaseBound() const final { return false; }

        // Called when a member is added to a group.
        virtual void OnAddMember(Group* /*group*/, uint64 /*guid*/) { }

        // Called when a member is invited to join a group.
        virtual void OnInviteMember(Group* /*group*/, uint64 /*guid*/) { }

        // Called when a member is removed from a group.
        virtual void OnRemoveMember(Group* /*group*/, uint64 /*guid*/, RemoveMethod /*method*/, uint64 /*kicker*/, const char* /*reason*/) { }

        // Called when the leader of a group is changed.
        virtual void OnChangeLeader(Group* /*group*/, uint64 /*newLeaderGuid*/, uint64 /*oldLeaderGuid*/) { }

        // Called when a group is disbanded.
        virtual void OnDisband(Group* /*group*/) { }
};

class SceneScript : public ScriptObject
{
    protected:

        SceneScript(const char* name);

    public:

        bool IsDatabaseBound() const final { return true; }

        // Called when a player start a scene
        virtual void OnSceneStart(Player* /*player*/, uint32 /*sceneInstanceID*/, SceneTemplate const* /*sceneTemplate*/) { }

        // Called when a player receive trigger from scene
        virtual void OnSceneTriggerEvent(Player* /*player*/, uint32 /*sceneInstanceID*/, SceneTemplate const* /*sceneTemplate*/, std::string const& /*triggerName*/) { }

        // Called when a scene is canceled
        virtual void OnSceneCancel(Player* /*player*/, uint32 /*sceneInstanceID*/, SceneTemplate const* /*sceneTemplate*/) { }

        // Called when a scene is completed
        virtual void OnSceneComplete(Player* /*player*/, uint32 /*sceneInstanceID*/, SceneTemplate const* /*sceneTemplate*/) { }
};

class GameEventScript : public ScriptObject
{
    protected:
        GameEventScript(const char* name);

    public:
        bool IsDatabaseBound() const { return true; }

        // Called on every GameEventMgr update to check if the event should be active.
        // shouldStart parameter specifies whether the time checks have passed.
        // Can be used (with care and external storage, e.g. WorldStates) to maintain a downtime-safe record of whether the event started or stopped.
        // The most bare-bone implementation should just return the shouldStart parameter.
        virtual bool CanStart(bool shouldStart) { return shouldStart; }

        // Called when the event is being started, before all other procedures are executed (e.g. spawns).
        virtual void OnStart() { }

        // Called when the event is being started, after all other procedures are executed (e.g. spawns).
        virtual void OnStarted() { }

        // Called when the event is being stopped, before all other procedures are executed (e.g. despawns).
        virtual void OnStop() { }

        // Called when the event is being stopped, after all other procedures are executed (e.g. despawns).
        virtual void OnStopped() { }

        // Called on every GameEventMgr update to check whether the event wants to receive periodic OnUpdate calls.
        // This method should return the minimum desired interval in milliseconds in-between OnUpdate calls.
        // It is not guaranteed that OnUpdate would be called at exactly this interval, it can be called earlier or up to 1 second later.
        virtual uint32 GetUpdateInterval() { return 0; }

        // Called periodically if GetUpdateInterval is overriden and returns non-zero values.
        // Refer to GetUpdateInterval comment to read notes on when OnUpdate is called.
        virtual void OnUpdate(uint32 diff) { }

        // Called when WorldStates need to be sent to a player, the implementation should append state-value uint32 pairs to the packet if necessary.
        virtual void FillWorldStates(Player* player, WorldStateBuilder& builder) { }
};

// following hooks can be used anywhere and are not db bounded
class GlobalScript : public ScriptObject
{
protected:

    GlobalScript(const char* name);

public:

    // items
    virtual void OnItemDelFromDB(SQLTransaction& /*trans*/, uint32 /*itemGuid*/) { }
    virtual void OnMirrorImageDisplayItem(const Item* /*item*/, uint32& /*display*/) { }
};

// Placed here due to ScriptRegistry::AddScript dependency.
#define sScriptMgr ACE_Singleton<ScriptMgr, ACE_Null_Mutex>::instance()

// Manages registration, loading, and execution of scripts.
class ScriptMgr
{
    friend class ACE_Singleton<ScriptMgr, ACE_Null_Mutex>;
    friend class ScriptObject;

    private:
        typedef void(*ScriptLoader)();
        ScriptLoader _scriptLoader;
        ScriptMgr();
        virtual ~ScriptMgr();
    public: /* Initialization */
        void SetLoader(ScriptLoader loader) { _scriptLoader = loader; }
        void Initialize();
        void LoadDatabase();
        void FillSpellSummary();

        const char* ScriptsVersion() const { return "Integrated Trinity Scripts"; }

        void IncrementScriptCount() { ++_scriptCount; }
        uint32 GetScriptCount() const { return _scriptCount; }

    public: /* Unloading */

        void Unload();

    public: /* SpellScriptLoader */

        void CreateSpellScripts(uint32 spellId, std::list<SpellScript*>& scriptVector);
        void CreateAuraScripts(uint32 spellId, std::list<AuraScript*>& scriptVector);
        void CreateSpellScriptLoaders(uint32 spellId, std::vector<std::pair<SpellScriptLoader*, std::multimap<uint32, uint32>::iterator> >& scriptVector);

    public: /* ServerScript */

        void OnNetworkStart();
        void OnNetworkStop();
        void OnSocketOpen(WorldSocket* socket);
        void OnSocketClose(WorldSocket* socket, bool wasNew);
        void OnPacketReceive(WorldSocket* socket, WorldPacket packet);
        void OnPacketSend(WorldSocket* socket, WorldPacket packet);
        void OnUnknownPacketReceive(WorldSocket* socket, WorldPacket packet);

    public: /* WorldScript */

        void OnOpenStateChange(bool open);
        void OnConfigLoad(bool reload);
        void OnMotdChange(std::string& newMotd);
        void OnShutdownInitiate(ShutdownExitCode code, ShutdownMask mask);
        void OnShutdownCancel();
        void OnWorldUpdate(uint32 diff);
        void OnStartup();
        void OnShutdown();

    public: /* FormulaScript */

        void OnHonorCalculation(float& honor, uint8 level, float multiplier);
        void OnGrayLevelCalculation(uint8& grayLevel, uint8 playerLevel);
        void OnColorCodeCalculation(XPColorChar& color, uint8 playerLevel, uint8 mobLevel);
        void OnZeroDifferenceCalculation(uint8& diff, uint8 playerLevel);
        void OnBaseGainCalculation(uint32& gain, uint8 playerLevel, uint8 mobLevel, ContentLevels content);
        void OnGainCalculation(uint32& gain, Player* player, Unit* unit);
        void OnGroupRateCalculation(float& rate, uint32 count, bool isRaid);

    public: /* MapScript */

        void OnCreateMap(Map* map);
        void OnDestroyMap(Map* map);
        void OnLoadGridMap(Map* map, GridMap* gmap, uint32 gx, uint32 gy);
        void OnUnloadGridMap(Map* map, GridMap* gmap, uint32 gx, uint32 gy);
        void OnPlayerEnterMap(Map* map, Player* player);
        void OnPlayerLeaveMap(Map* map, Player* player);
        void OnMapUpdate(Map* map, uint32 diff);

    public: /* InstanceMapScript */

        InstanceScript* CreateInstanceData(InstanceMap* map);

    public: /* ItemScript */

        bool OnDummyEffect(Unit* caster, uint32 spellId, SpellEffIndex effIndex, Item* target);
        bool OnQuestAccept(Player* player, Item* item, Quest const* quest);
        bool OnItemUse(Player* player, Item* item, SpellCastTargets const& targets);
        bool OnItemExpire(Player* player, ItemTemplate const* proto);
        bool OnItemPickup(Player* player, Item* item, ItemPickupSourceType sourceType, uint32 sourceId);
        void OnGossipSelect(Player* player, Item* item, uint32 sender, uint32 action);
        void OnGossipSelectCode(Player* player, Item* item, uint32 sender, uint32 action, const char* code);
        void OnQuestAccept(Player* player, const Quest* quest);

    public: /* CreatureScript */

        bool OnDummyEffect(Unit* caster, uint32 spellId, SpellEffIndex effIndex, Creature* target);
        bool OnGossipHello(Player* player, Creature* creature);
        bool OnGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 action);
        bool OnGossipSelectCode(Player* player, Creature* creature, uint32 sender, uint32 action, const char* code);
        bool OnQuestAccept(Player* player, Creature* creature, Quest const* quest);
        bool OnQuestSelect(Player* player, Creature* creature, Quest const* quest);
        bool OnQuestComplete(Player* player, Creature* creature, Quest const* quest);
        bool OnQuestReward(Player* player, Creature* creature, Quest const* quest, uint32 opt);
        uint32 GetDialogStatus(Player* player, Creature* creature);
        CreatureAI* GetCreatureAI(Creature* creature);
        void OnCreatureUpdate(Creature* creature, uint32 diff);

    public: /* GameObjectScript */

        bool OnDummyEffect(Unit* caster, uint32 spellId, SpellEffIndex effIndex, GameObject* target);
        bool OnGossipHello(Player* player, GameObject* go);
        bool OnReportUse(Player* player, GameObject* go);
        bool OnGossipSelect(Player* player, GameObject* go, uint32 sender, uint32 action);
        bool OnGossipSelectCode(Player* player, GameObject* go, uint32 sender, uint32 action, const char* code);
        bool OnQuestAccept(Player* player, GameObject* go, Quest const* quest);
        bool OnQuestReward(Player* player, GameObject* go, Quest const* quest, uint32 opt);
        uint32 GetDialogStatus(Player* player, GameObject* go);
        void OnGameObjectDestroyed(GameObject* go, Player* player);
        void OnGameObjectDamaged(GameObject* go, Player* player);
        void OnGameObjectLootStateChanged(GameObject* go, uint32 state, Unit* unit);
        void OnGameObjectStateChanged(GameObject* go, uint32 state);
        void OnGameObjectUpdate(GameObject* go, uint32 diff);
        GameObjectAI* GetGameObjectAI(GameObject* go);

    public: /* AreaTriggerScript */

        bool OnAreaTrigger(Player* player, AreaTriggerEntry const* trigger);

    public: /* SpellAreaTriggerScript */

        IAreaTrigger* CreateAreaTriggerInterface(uint32 entry);
        void OnSpellAreaTriggerEnter(AreaTrigger* areaTrigger, Unit* Target);
        void OnSpellAreaTriggerLeave(AreaTrigger* areaTrigger, Unit* Target);

    public: /* BattlegroundScript */

        Battleground* CreateBattleground(BattlegroundTypeId typeId);

    public: /* OutdoorPvPScript */

        OutdoorPvP* CreateOutdoorPvP(OutdoorPvPData const* data);

    public: /* CommandScript */

        std::vector<ChatCommand> GetChatCommands();

    public: /* WeatherScript */

        void OnWeatherChange(Weather* weather, WeatherState state, float grade);
        void OnWeatherUpdate(Weather* weather, uint32 diff);

    public: /* AuctionHouseScript */

        void OnAuctionAdd(AuctionHouseObject* ah, AuctionEntry* entry);
        void OnAuctionRemove(AuctionHouseObject* ah, AuctionEntry* entry);
        void OnAuctionSuccessful(AuctionHouseObject* ah, AuctionEntry* entry);
        void OnAuctionExpire(AuctionHouseObject* ah, AuctionEntry* entry);

    public: /* ConditionScript */

        bool OnConditionCheck(Condition* condition, ConditionSourceInfo& sourceInfo);

    public: /* DynamicObjectScript */

        void OnDynamicObjectUpdate(DynamicObject* dynobj, uint32 diff);

    public: /* TransportScript */

        void OnTransportCreate(Transport* transport);
        void OnAddPassenger(Transport* transport, Player* player);
        void OnAddCreaturePassenger(Transport* transport, Creature* creature);
        void OnRemovePassenger(Transport* transport, Player* player);
        void OnTransportUpdate(Transport* transport, uint32 diff);
        void OnRelocate(Transport* transport, uint32 waypointId, uint32 mapId, float x, float y, float z);

    public: /* AchievementCriteriaScript */

        bool OnCriteriaCheck(uint32 scriptId, Player* source, Unit* target);

    public: /* PlayerScript */

        void OnPVPKill(Player* killer, Player* killed);
        void OnCreatureKill(Player* killer, Creature* killed);
        void OnPlayerKilledByCreature(Creature* killer, Player* killed);
        void OnPlayerLevelChanged(Player* player, uint8 oldLevel);
        void OnPlayerEquipChanged(Player* player, uint32 item);
        void OnPlayerFreeTalentPointsChanged(Player* player, uint32 newPoints);
        void OnPlayerTalentsReset(Player* player, bool noCost);
        void OnPlayerMoneyChanged(Player* player, int64& amount);
        void OnGivePlayerXP(Player* player, uint32& amount, Unit* victim);
        void OnPlayerReputationChange(Player* player, uint32 factionID, float& standing, bool incremental);
        void OnPlayerQuestAdded(Player* player, const Quest* quest);
        void OnPlayerQuestAbandoned(Player* player, const Quest* quest);
        void OnPlayerQuestCompleted(Player* player, const Quest* quest);
        void OnPlayerQuestFailed(Player* player, const Quest* quest);
        void OnPlayerQuestRewarded(Player* player, const Quest* quest);
        void OnPlayerDuelRequest(Player* target, Player* challenger);
        void OnPlayerDuelStart(Player* player1, Player* player2);
        void OnPlayerDuelEnd(Player* winner, Player* loser, DuelCompleteType type);
        void OnPlayerChat(Player* player, uint32 type, uint32 lang, std::string& msg);
        void OnPlayerChat(Player* player, uint32 type, uint32 lang, std::string& msg, Player* receiver);
        void OnPlayerChat(Player* player, uint32 type, uint32 lang, std::string& msg, Group* group);
        void OnPlayerChat(Player* player, uint32 type, uint32 lang, std::string& msg, Guild* guild);
        void OnPlayerChat(Player* player, uint32 type, uint32 lang, std::string& msg, Channel* channel);
        void OnPlayerEmote(Player* player, uint32 emote);
        void OnPlayerTextEmote(Player* player, uint32 textEmote, uint32 emoteNum, uint64 guid);
        void OnPlayerSpellCast(Player* player, Spell* spell, bool skipCheck);
        void OnPlayerLogin(Player* player);
        void OnPlayerLogout(Player* player);
        void OnPlayerCreate(Player* player);
        void OnPlayerDelete(uint64 guid);
        void OnPlayerSave(Player* player);
        void OnPlayerBindToInstance(Player* player, Difficulty difficulty, uint32 mapid, bool permanent);
        void OnPlayerUpdateZone(Player* player, uint32 newZone, uint32 newArea);
		void OnUpdate(Player* player, uint32 diff);
        void OnGossipSelect(Player* player, uint32 menu_id, uint32 sender, uint32 action);
        void OnGossipSelectCode(Player* player, uint32 menu_id, uint32 sender, uint32 action, const char* code);

    public: /* GuildScript */

        void OnGuildAddMember(Guild* guild, Player* player, uint8& plRank);
        void OnGuildRemoveMember(Guild* guild, Player* player, bool isDisbanding, bool isKicked);
        void OnGuildMOTDChanged(Guild* guild, const std::string& newMotd);
        void OnGuildInfoChanged(Guild* guild, const std::string& newInfo);
        void OnGuildCreate(Guild* guild, Player* leader, const std::string& name);
        void OnGuildDisband(Guild* guild);
        void OnGuildMemberWitdrawMoney(Guild* guild, Player* player, uint64 &amount, bool isRepair);
        void OnGuildMemberDepositMoney(Guild* guild, Player* player, uint64 &amount);
        void OnGuildItemMove(Guild* guild, Player* player, Item* pItem, bool isSrcBank, uint8 srcContainer, uint8 srcSlotId,
            bool isDestBank, uint8 destContainer, uint8 destSlotId);
        void OnGuildEvent(Guild* guild, uint8 eventType, uint32 playerGuid1, uint32 playerGuid2, uint8 newRank);
        void OnGuildBankEvent(Guild* guild, uint8 eventType, uint8 tabId, uint32 playerGuid, uint32 itemOrMoney, uint16 itemStackCount, uint8 destTabId);

    public: /* GroupScript */

        void OnGroupAddMember(Group* group, uint64 guid);
        void OnGroupInviteMember(Group* group, uint64 guid);
        void OnGroupRemoveMember(Group* group, uint64 guid, RemoveMethod method, uint64 kicker, const char* reason);
        void OnGroupChangeLeader(Group* group, uint64 newLeaderGuid, uint64 oldLeaderGuid);
        void OnGroupDisband(Group* group);

    public: /* GameEventScript */
        bool CanGameEventStart(GameEventData const& event, bool shouldStart);
        void OnGameEventStart(GameEventData const& event);
        void OnGameEventStarted(GameEventData const& event);
        void OnGameEventStop(GameEventData const& event);
        void OnGameEventStopped(GameEventData const& event);
        uint32 GetGameEventUpdateInterval(GameEventData const& event);
        void OnGameEventUpdate(GameEventData const& event, uint32 diff);
        void FillGameEventWorldStates(GameEventData const& event, Player* player, WorldStateBuilder& builder);

    public: /* GlobalScript */
        void OnGlobalItemDelFromDB(SQLTransaction& trans, uint32 itemGuid);
        void OnGlobalMirrorImageDisplayItem(const Item *item, uint32 &display);

    public: /* UnitScript */

        void OnHeal(Unit* healer, Unit* reciever, uint32& gain);
        void OnDamage(Unit* attacker, Unit* victim, uint32& damage);
        void ModifyPeriodicDamageAurasTick(Unit* target, Unit* attacker, int32& damage);
        void ModifyMeleeDamage(Unit* target, Unit* attacker, uint32& damage);
        void ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& damage);

    public: /* Scheduled scripts */

        uint32 IncreaseScheduledScriptsCount() { return ++_scheduledScripts; }
        uint32 DecreaseScheduledScriptCount() { return --_scheduledScripts; }
        uint32 DecreaseScheduledScriptCount(size_t count) { return _scheduledScripts -= count; }
        bool IsScriptScheduled() const { return _scheduledScripts > 0; }

    public: /* SceneScript */
        void OnSceneStart(Player* player, uint32 sceneInstanceID, SceneTemplate const* sceneTemplate);
        void OnSceneTrigger(Player* player, uint32 sceneInstanceID, SceneTemplate const* sceneTemplate, std::string const& triggerName);
        void OnSceneCancel(Player* player, uint32 sceneInstanceID, SceneTemplate const* sceneTemplate);
        void OnSceneComplete(Player* player, uint32 sceneInstanceID, SceneTemplate const* sceneTemplate);

    public:
        static bool CanHavePetAI(Creature* creature);

    private:

        uint32 _scriptCount;

        //atomic op counter for active scripts amount
        ACE_Atomic_Op<ACE_Thread_Mutex, long> _scheduledScripts;
};

template <class T>
struct aura_script : SpellScriptLoader
{
    aura_script(char const* name) : SpellScriptLoader(name) { }
    AuraScript* GetAuraScript() const override { return new T(); }
};

template <class T>
struct spell_script : SpellScriptLoader
{
    spell_script(char const* name) : SpellScriptLoader(name) { }
    SpellScript* GetSpellScript() const override { return new T(); }
};

template <class T>
struct atrigger_script : public SpellAreaTriggerScript
{
    atrigger_script(char const* name) : SpellAreaTriggerScript(name) { }
    IAreaTrigger* GetInterface() const override { return new T(); }
};

template <class T>
struct creature_script: public CreatureScript
{
    creature_script(char const* name) : CreatureScript(name) { }
    CreatureAI* GetAI(Creature* creature) const override { return new T(creature); }
};

// For derives from PetAI to prevent crashes on spawn without player owner
template <class T>
struct pet_script : public CreatureScript
{
    pet_script(char const* name) : CreatureScript(name) { }
    CreatureAI* GetAI(Creature* creature) const override { return ScriptMgr::CanHavePetAI(creature) ? new T(creature) : nullptr; }
};

template <class S>
class GenericSpellScriptLoader : public SpellScriptLoader
{
    public:
        GenericSpellScriptLoader(char const* name) : SpellScriptLoader(name) { }
        SpellScript* GetSpellScript() const override { return new S(); }
};
#define RegisterSpellScript(spell_script) new GenericSpellScriptLoader<spell_script>(#spell_script)

template <class A>
class GenericAuraScriptLoader : public SpellScriptLoader
{
    public:
        GenericAuraScriptLoader(char const* name) : SpellScriptLoader(name) { }
        AuraScript* GetAuraScript() const override { return new A(); }
};
#define RegisterAuraScript(aura_script) new GenericAuraScriptLoader<aura_script>(#aura_script)

template <class S, class A>
class GenericSpellAndAuraScriptLoader : public SpellScriptLoader
{
    public:
        GenericSpellAndAuraScriptLoader(char const* name) : SpellScriptLoader(name) { }
        SpellScript* GetSpellScript() const override { return new S(); }
        AuraScript* GetAuraScript() const override { return new A(); }
};
#define RegisterSpellAndAuraScriptPair(spell_script, aura_script) new GenericSpellAndAuraScriptLoader<spell_script, aura_script>(#spell_script)

template <class AI>
class GenericCreatureScript : public CreatureScript
{
    public:
        GenericCreatureScript(char const* name) : CreatureScript(name) { }
        CreatureAI* GetAI(Creature* me) const override { return new AI(me); }
};
#define RegisterCreatureAI(ai_name) new GenericCreatureScript<ai_name>(#ai_name)

template <class AI, AI*(*AIFactory)(Creature*)>
class FactoryCreatureScript : public CreatureScript
{
    public:
        FactoryCreatureScript(char const* name) : CreatureScript(name) { }
        CreatureAI* GetAI(Creature* me) const override { return AIFactory(me); }
};
#define RegisterCreatureAIWithFactory(ai_name, factory_fn) new FactoryCreatureScript<ai_name, &factory_fn>(#ai_name)

template <class AI>
class GenericGameObjectScript : public GameObjectScript
{
    public:
        GenericGameObjectScript(char const* name) : GameObjectScript(name) { }
        GameObjectAI* GetAI(GameObject* go) const override { return new AI(go); }
};
#define RegisterGameObjectAI(ai_name) new GenericGameObjectScript<ai_name>(#ai_name)

template <class AI>
class GenericInstanceMapScript : public InstanceMapScript
{
    public:
        GenericInstanceMapScript(char const* name, uint32 mapId) : InstanceMapScript(name, mapId) { }
        InstanceScript* GetInstanceScript(InstanceMap* map) const override { return new AI(map); }
};
#define RegisterInstanceScript(ai_name, mapId) new GenericInstanceMapScript<ai_name>(#ai_name, mapId)

#endif
