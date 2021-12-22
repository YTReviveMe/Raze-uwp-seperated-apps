//-------------------------------------------------------------------------
/*
Copyright (C) 2010-2019 EDuke32 developers and contributors
Copyright (C) 2019 Nuke.YKT

This file is part of NBlood.

NBlood is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License version 2
as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
//-------------------------------------------------------------------------
#include "ns.h"	// Must come before everything else!

#include <random>

#include "build.h"

#include "blood.h"
#include "misc.h"
#include "d_net.h"

BEGIN_BLD_NS

unsigned int GetWaveValue(unsigned int nPhase, int nType)
{
    switch (nType)
    {
    case 0:
        return 0x8000-(Cos(FixedToInt(nPhase<<10))>>15);
    case 1:
        return nPhase;
    case 2:
        return 0x10000-(Cos(FixedToInt(nPhase<<9))>>14);
    case 3:
        return Sin(FixedToInt(nPhase<<9))>>14;
    }
    return nPhase;
}

bool SetSpriteState(DBloodActor* actor, int nState)
{
    auto pSprite = &actor->s();
    auto pXSprite = &actor->x();
    if ((pXSprite->busy & 0xffff) == 0 && pXSprite->state == nState)
        return 0;
    pXSprite->busy  = IntToFixed(nState);
    pXSprite->state = nState;
    evKillActor(actor);
    if ((pSprite->flags & kHitagRespawn) != 0 && pSprite->inittype >= kDudeBase && pSprite->inittype < kDudeMax)
    {
        pXSprite->respawnPending = 3;
        evPostActor(actor, gGameOptions.nMonsterRespawnTime, kCallbackRespawn);
        return 1;
    }
    if (pXSprite->restState != nState && pXSprite->waitTime > 0)
        evPostActor(actor, (pXSprite->waitTime * 120) / 10, pXSprite->restState ? kCmdOn : kCmdOff);
    if (pXSprite->txID)
    {
        if (pXSprite->command != kCmdLink && pXSprite->triggerOn && pXSprite->state)
            evSendActor(actor, pXSprite->txID, (COMMAND_ID)pXSprite->command);
        if (pXSprite->command != kCmdLink && pXSprite->triggerOff && !pXSprite->state)
            evSendActor(actor, pXSprite->txID, (COMMAND_ID)pXSprite->command);
    }
    return 1;
}

bool SetWallState(walltype* pWall, int nState)
{
    auto pXWall = &pWall->xw();
    if ((pXWall->busy&0xffff) == 0 && pXWall->state == nState)
        return 0;
    pXWall->busy  = IntToFixed(nState);
    pXWall->state = nState;
    evKillWall(pWall);
    if (pXWall->restState != nState && pXWall->waitTime > 0)
        evPostWall(pWall, (pXWall->waitTime*120) / 10, pXWall->restState ? kCmdOn : kCmdOff);
    if (pXWall->txID)
    {
        if (pXWall->command != kCmdLink && pXWall->triggerOn && pXWall->state)
            evSendWall(pWall, pXWall->txID, (COMMAND_ID)pXWall->command);
        if (pXWall->command != kCmdLink && pXWall->triggerOff && !pXWall->state)
            evSendWall(pWall, pXWall->txID, (COMMAND_ID)pXWall->command);
    }
    return 1;
}

bool SetSectorState(sectortype *pSector, int nState)
{
    assert(pSector->hasX());
    auto pXSector = &pSector->xs();
    if ((pXSector->busy&0xffff) == 0 && pXSector->state == nState)
        return 0;
    pXSector->busy = IntToFixed(nState);
    pXSector->state = nState;
    evKillSector(pSector);
    if (nState == 1)
    {
        if (pXSector->command != kCmdLink && pXSector->triggerOn && pXSector->txID)
            evSendSector(pSector,pXSector->txID, (COMMAND_ID)pXSector->command);
        if (pXSector->stopOn)
        {
            pXSector->stopOn = 0;
            pXSector->stopOff = 0;
        }
        else if (pXSector->reTriggerA)
            evPostSector(pSector, (pXSector->waitTimeA * 120) / 10, kCmdOff);
    }
    else
    {
        if (pXSector->command != kCmdLink && pXSector->triggerOff && pXSector->txID)
            evSendSector(pSector,pXSector->txID, (COMMAND_ID)pXSector->command);
        if (pXSector->stopOff)
        {
            pXSector->stopOn = 0;
            pXSector->stopOff = 0;
        }
        else if (pXSector->reTriggerB)
            evPostSector(pSector, (pXSector->waitTimeB * 120) / 10, kCmdOn);
    }
    return 1;
}

TArray<BUSY> gBusy;

void AddBusy(sectortype* pSector, BUSYID a2, int nDelta)
{
    assert(nDelta != 0);
    for (auto& b : gBusy)
    {
        if (b.sect == pSector && b.type == a2)
        {
            b.delta = nDelta;
            return;
        }
    }
    BUSY b = { pSector, nDelta, nDelta > 0 ? 0 : 65536, a2 };
    gBusy.Push(b);
}

void ReverseBusy(sectortype* pSector, BUSYID a2)
{
    for (auto& b : gBusy)
    {
        if (b.sect == pSector && b.type == a2)
        {
            b.delta = -b.delta;
            break;
        }
    }
}

unsigned int GetSourceBusy(EVENT& a1)
{
    if (a1.isSector())
    {
        auto sect = a1.getSector();
        return sect->hasX()? sect->xs().busy : 0;
    }
    else if (a1.isWall())
    {
        auto wal = a1.getWall();
        return wal->hasX()? wal->xw().busy : 0;
    }
    else if (a1.isActor())
    {
        auto pActor = a1.getActor();
        return pActor && pActor->hasX() ? pActor->xspr.busy : false;
    }
    return 0;
}

void LifeLeechOperate(DBloodActor* actor, EVENT event)
{
    auto pSprite = &actor->s();
    auto pXSprite = &actor->x();
    switch (event.cmd) {
    case kCmdSpritePush:
    {
        int nPlayer = pXSprite->data4;
        if (nPlayer >= 0 && nPlayer < kMaxPlayers && playeringame[nPlayer])
        {
            PLAYER *pPlayer = &gPlayer[nPlayer];
            if (pPlayer->pXSprite->health > 0)
            {
                evKillActor(actor);
                pPlayer->ammoCount[8] = ClipHigh(pPlayer->ammoCount[8]+pXSprite->data3, gAmmoInfo[8].max);
                pPlayer->hasWeapon[9] = 1;
                if (pPlayer->curWeapon != kWeapLifeLeech)
                {
                    if (!VanillaMode() && checkFired6or7(pPlayer)) // if tnt/spray is actively used, do not switch weapon
                        break;
                    pPlayer->weaponState = 0;
                    pPlayer->nextWeapon = 9;
                }
            }
        }
        break;
    }
    case kCmdSpriteProximity:
    {
        auto target = actor->GetTarget();
        if (target)
        {
            if (!pXSprite->stateTimer)
            {
                spritetype *pTarget = &target->s();
                if (pTarget->statnum == kStatDude && !(pTarget->flags&32) && target->hasX())
                {
                    int top, bottom;
                    GetSpriteExtents(pSprite, &top, &bottom);
                    int nType = pTarget->type-kDudeBase;
                    DUDEINFO *pDudeInfo = getDudeInfo(nType+kDudeBase);
                    int z1 = (top-pSprite->pos.Z)-256;
                    int x = pTarget->pos.X;
                    int y = pTarget->pos.Y;
                    int z = pTarget->pos.Z;
                    int nDist = approxDist(x - pSprite->pos.X, y - pSprite->pos.Y);
                    if (nDist != 0 && cansee(pSprite->pos.X, pSprite->pos.Y, top, pSprite->sector(), x, y, z, pTarget->sector()))
                    {
                        int t = DivScale(nDist, 0x1aaaaa, 12);
                        x += (target->xvel*t)>>12;
                        y += (target->yvel*t)>>12;
                        int angBak = pSprite->ang;
                        pSprite->ang = getangle(x-pSprite->pos.X, y-pSprite->pos.Y);
                        int dx = bcos(pSprite->ang);
                        int dy = bsin(pSprite->ang);
                        int tz = pTarget->pos.Z - (pTarget->yrepeat * pDudeInfo->aimHeight) * 4;
                        int dz = DivScale(tz - top - 256, nDist, 10);
                        int nMissileType = kMissileLifeLeechAltNormal + (pXSprite->data3 ? 1 : 0);
                        int t2;
                        if (!pXSprite->data3)
                            t2 = 120 / 10;
                        else
                            t2 = (3*120) / 10;
                        auto missile = actFireMissile(actor, 0, z1, dx, dy, dz, nMissileType);
                        if (missile)
                        {
                            missile->SetOwner(actor);
                            pXSprite->stateTimer = 1;
                            evPostActor(actor, t2, kCallbackLeechStateTimer);
                            pXSprite->data3 = ClipLow(pXSprite->data3-1, 0);
                            if (!VanillaMode()) // disable collisions so lifeleech doesn't do that weird bobbing
                                missile->spr.cstat &= ~CSTAT_SPRITE_BLOCK_ALL;
                        }
                        pSprite->ang = angBak;
                    }
                }
            }
        }
        return;
    }
    }
    actPostSprite(actor, kStatFree);
}

void ActivateGenerator(DBloodActor*);

void OperateSprite(DBloodActor* actor, EVENT event)
{
    auto pSprite = &actor->s();
    auto pXSprite = &actor->x();

    #ifdef NOONE_EXTENSIONS
    if (gModernMap && modernTypeOperateSprite(actor, event))
        return;
    #endif

    switch (event.cmd) {
        case kCmdLock:
            pXSprite->locked = 1;
            return;
        case kCmdUnlock:
            pXSprite->locked = 0;
            return;
        case kCmdToggleLock:
            pXSprite->locked = pXSprite->locked ^ 1;
            return;
    }

    if (pSprite->statnum == kStatDude && pSprite->type >= kDudeBase && pSprite->type < kDudeMax) {
        
        switch (event.cmd) {
            case kCmdOff:
                SetSpriteState(actor, 0);
                break;
            case kCmdSpriteProximity:
                if (pXSprite->state) break;
                [[fallthrough]];
            case kCmdOn:
            case kCmdSpritePush:
            case kCmdSpriteTouch:
                if (!pXSprite->state) SetSpriteState(actor, 1);
                aiActivateDude(actor);
                break;
        }

        return;
    }


    switch (pSprite->type) {
    case kTrapMachinegun:
        if (pXSprite->health <= 0) break; 
        switch (event.cmd) {
            case kCmdOff:
                if (!SetSpriteState(actor, 0)) break;
                seqSpawn(40, actor, -1);
                break;
            case kCmdOn:
                if (!SetSpriteState(actor, 1)) break;
                seqSpawn(38, actor, nMGunOpenClient);
                if (pXSprite->data1 > 0)
                    pXSprite->data2 = pXSprite->data1;
                break;
        }
        break;
    case kThingFallingRock:
        if (SetSpriteState(actor, 1))
            pSprite->flags |= 7;
        break;
    case kThingWallCrack:
        if (SetSpriteState(actor, 0))
            actPostSprite(actor, kStatFree);
        break;
    case kThingCrateFace:
        if (SetSpriteState(actor, 0))
            actPostSprite(actor, kStatFree);
        break;
    case kTrapZapSwitchable:
        switch (event.cmd) {
            case kCmdOff:
                pXSprite->state = 0;
                pSprite->cstat |= CSTAT_SPRITE_INVISIBLE;
                pSprite->cstat &= ~CSTAT_SPRITE_BLOCK;
                break;
            case kCmdOn:
                pXSprite->state = 1;
                pSprite->cstat &= ~CSTAT_SPRITE_INVISIBLE;
                pSprite->cstat |= CSTAT_SPRITE_BLOCK;
                break;
            case kCmdToggle:
                pXSprite->state ^= 1;
                pSprite->cstat ^= CSTAT_SPRITE_INVISIBLE;
                pSprite->cstat ^= CSTAT_SPRITE_BLOCK;
                break;
        }
        break;
    case kTrapFlame:
        switch (event.cmd) {
            case kCmdOff:
                if (!SetSpriteState(actor, 0)) break;
                seqSpawn(40, actor, -1);
                sfxKill3DSound(actor, 0, -1);
                break;
            case kCmdOn:
                if (!SetSpriteState(actor, 1)) break;
                seqSpawn(38, actor, -1);
                sfxPlay3DSound(actor, 441, 0, 0);
                break;
        }
        break;
    case kSwitchPadlock:
        switch (event.cmd) {
            case kCmdOff:
                SetSpriteState(actor, 0);
                break;
            case kCmdOn:
                if (!SetSpriteState(actor, 1)) break;
                seqSpawn(37, actor, -1);
                break;
            default:
                SetSpriteState(actor, pXSprite->state ^ 1);
                if (pXSprite->state) seqSpawn(37, actor, -1);
                break;
        }
        break;
    case kSwitchToggle:
        switch (event.cmd) {
            case kCmdOff:
                if (!SetSpriteState(actor, 0)) break;
                sfxPlay3DSound(actor, pXSprite->data2, 0, 0);
                break;
            case kCmdOn:
                if (!SetSpriteState(actor, 1)) break;
                sfxPlay3DSound(actor, pXSprite->data1, 0, 0);
                break;
            default:
                if (!SetSpriteState(actor, pXSprite->state ^ 1)) break;
                if (pXSprite->state) sfxPlay3DSound(actor, pXSprite->data1, 0, 0);
                else sfxPlay3DSound(actor, pXSprite->data2, 0, 0);
                break;
        }
        break;
    case kSwitchOneWay:
        switch (event.cmd) {
            case kCmdOff:
                if (!SetSpriteState(actor, 0)) break;
                sfxPlay3DSound(actor, pXSprite->data2, 0, 0);
                break;
            case kCmdOn:
                if (!SetSpriteState(actor, 1)) break;
                sfxPlay3DSound(actor, pXSprite->data1, 0, 0);
                break;
            default:
                if (!SetSpriteState(actor, pXSprite->restState ^ 1)) break;
                if (pXSprite->state) sfxPlay3DSound(actor, pXSprite->data1, 0, 0);
                else sfxPlay3DSound(actor, pXSprite->data2, 0, 0);
                break;
        }
        break;
    case kSwitchCombo:
        switch (event.cmd) {
            case kCmdOff:
                pXSprite->data1--;
                if (pXSprite->data1 < 0)
                    pXSprite->data1 += pXSprite->data3;
                break;
            default:
                pXSprite->data1++;
                if (pXSprite->data1 >= pXSprite->data3)
                    pXSprite->data1 -= pXSprite->data3;
                break;
        }
        
        sfxPlay3DSound(actor, pXSprite->data4, -1, 0);
        
        if (pXSprite->command == kCmdLink && pXSprite->txID > 0)
            evSendActor(actor, pXSprite->txID, kCmdLink);

        if (pXSprite->data1 == pXSprite->data2) 
            SetSpriteState(actor, 1);
        else 
            SetSpriteState(actor, 0);

        break;
    case kMarkerDudeSpawn:
        if (gGameOptions.nMonsterSettings && pXSprite->data1 >= kDudeBase && pXSprite->data1 < kDudeMax) 
        {
            auto spawned = actSpawnDude(actor, pXSprite->data1, -1, 0);
            if (spawned) {
                XSPRITE *pXSpawn = &spawned->x();
                gKillMgr.AddNewKill(1);
                switch (pXSprite->data1) {
                    case kDudeBurningInnocent:
                    case kDudeBurningCultist:
                    case kDudeBurningZombieButcher:
                    case kDudeBurningTinyCaleb:
                    case kDudeBurningBeast: {
                        pXSpawn->health = getDudeInfo(pXSprite->data1)->startHealth << 4;
                        pXSpawn->burnTime = 10;
                        spawned->SetTarget(nullptr);
                        aiActivateDude(spawned);
                        break;
                    default:
                        break;
                    }
                }
            }
        }
        break;
    case kMarkerEarthQuake:
        pXSprite->triggerOn = 0;
        pXSprite->isTriggered = 1;
        SetSpriteState(actor, 1);
        for (int p = connecthead; p >= 0; p = connectpoint2[p]) {
            spritetype *pPlayerSprite = gPlayer[p].pSprite;
            int dx = (pSprite->pos.X - pPlayerSprite->pos.X)>>4;
            int dy = (pSprite->pos.Y - pPlayerSprite->pos.Y)>>4;
            int dz = (pSprite->pos.Z - pPlayerSprite->pos.Z)>>8;
            int nDist = dx*dx+dy*dy+dz*dz+0x40000;
            gPlayer[p].quakeEffect = DivScale(pXSprite->data1, nDist, 16);
        }
        break;
    case kThingTNTBarrel:
        if (pSprite->flags & kHitagRespawn) return;
        [[fallthrough]];
    case kThingArmedTNTStick:
    case kThingArmedTNTBundle:
    case kThingArmedSpray:
        actExplodeSprite(actor);
        break;
    case kTrapExploder:
        switch (event.cmd) {
            case kCmdOn:
                SetSpriteState(actor, 1);
                break;
            default:
                pSprite->cstat &= ~CSTAT_SPRITE_INVISIBLE;
                actExplodeSprite(actor);
                break;
        }
        break;
    case kThingArmedRemoteBomb:
        if (pSprite->statnum != kStatRespawn) {
            if (event.cmd != kCmdOn) actExplodeSprite(actor);
            else {
                sfxPlay3DSound(actor, 454, 0, 0);
                evPostActor(actor, 18, kCmdOff);
            }
        }
        break;    
    case kThingArmedProxBomb:
        if (pSprite->statnum != kStatRespawn) {
            switch (event.cmd) {
                case kCmdSpriteProximity:
                    if (pXSprite->state) break;
                    sfxPlay3DSound(actor, 452, 0, 0);
                    evPostActor(actor, 30, kCmdOff);
                    pXSprite->state = 1;
                    [[fallthrough]];
                case kCmdOn:
                    sfxPlay3DSound(actor, 451, 0, 0);
                    pXSprite->Proximity = 1;
                    break;
                default:
                    actExplodeSprite(actor);
                    break;
            }
        }
        break;
    case kThingDroppedLifeLeech:
        LifeLeechOperate(actor, event);
        break;
    case kGenTrigger:
    case kGenDripWater:
    case kGenDripBlood:
    case kGenMissileFireball:
    case kGenMissileEctoSkull:
    case kGenDart:
    case kGenBubble:
    case kGenBubbleMulti:
    case kGenSound:
        switch (event.cmd) {
            case kCmdOff:
                SetSpriteState(actor, 0);
                break;
            case kCmdRepeat:
                if (pSprite->type != kGenTrigger) ActivateGenerator(actor);
                if (pXSprite->txID) evSendActor(actor, pXSprite->txID, (COMMAND_ID)pXSprite->command);
                if (pXSprite->busyTime > 0) {
                    int nRand = Random2(pXSprite->data1);
                    evPostActor(actor, 120*(nRand+pXSprite->busyTime) / 10, kCmdRepeat);
                }
                break;
            default:
                if (!pXSprite->state) {
                    SetSpriteState(actor, 1);
                    evPostActor(actor, 0, kCmdRepeat);
                }
                break;
        }
        break;
    case kSoundPlayer:
        if (gGameOptions.nGameType == 0)
        {
            if (gMe->pXSprite->health <= 0)
                break;
            gMe->restTime = 0;
        }
        sndStartSample(pXSprite->data1, -1, 1, 0, CHANF_FORCE);
        break;
    case kThingObjectGib:
    case kThingObjectExplode:
    case kThingBloodBits:
    case kThingBloodChunks:
    case kThingZombieHead:
        switch (event.cmd) {
            case kCmdOff:
                if (!SetSpriteState(actor, 0)) break;
                actActivateGibObject(actor);
                break;
            case kCmdOn:
                if (!SetSpriteState(actor, 1)) break;
                actActivateGibObject(actor);
                break;
            default:
                if (!SetSpriteState(actor, pXSprite->state ^ 1)) break;
                actActivateGibObject(actor);
                break;
        }
        break;
    default:
        switch (event.cmd) {
            case kCmdOff:
                SetSpriteState(actor, 0);
                break;
            case kCmdOn:
                SetSpriteState(actor, 1);
                break;
            default:
                SetSpriteState(actor, pXSprite->state ^ 1);
                break;
        }
        break;
    }
}

void SetupGibWallState(walltype *pWall, XWALL *pXWall)
{
    walltype *pWall2 = NULL;
    if (pWall->twoSided())
        pWall2 = pWall->nextWall();
    if (pXWall->state)
    {
        pWall->cstat &= ~(CSTAT_WALL_BLOCK | CSTAT_WALL_BLOCK_HITSCAN);
        if (pWall2)
        {
            pWall2->cstat &= ~(CSTAT_WALL_BLOCK | CSTAT_WALL_BLOCK_HITSCAN);
            pWall->cstat &= ~CSTAT_WALL_MASKED;
            pWall2->cstat &= ~CSTAT_WALL_MASKED;
        }
        return;
    }
    bool bVector = pXWall->triggerVector != 0;
    pWall->cstat |= CSTAT_WALL_BLOCK;
    if (bVector)
        pWall->cstat |= CSTAT_WALL_BLOCK_HITSCAN;
    if (pWall2)
    {
        pWall2->cstat |= CSTAT_WALL_BLOCK;
        if (bVector)
            pWall2->cstat |= CSTAT_WALL_BLOCK_HITSCAN;
        pWall->cstat |= CSTAT_WALL_MASKED;
        pWall2->cstat |= CSTAT_WALL_MASKED;
    }
}

void OperateWall(walltype* pWall, EVENT event) {
    auto pXWall = &pWall->xw();
  
    switch (event.cmd) {
        case kCmdLock:
            pXWall->locked = 1;
            return;
        case kCmdUnlock:
            pXWall->locked = 0;
            return;
        case kCmdToggleLock:
            pXWall->locked ^= 1;
            return;
    }
    
    #ifdef NOONE_EXTENSIONS
    if (gModernMap && modernTypeOperateWall(pWall, event))
        return;
    #endif

    switch (pWall->type) {
        case kWallGib:
            bool bStatus;
            switch (event.cmd) {
                case kCmdOn:
                case kCmdWallImpact:
                    bStatus = SetWallState(pWall, 1);
                    break;
                case kCmdOff:
                    bStatus = SetWallState(pWall, 0);
                    break;
                default:
                    bStatus = SetWallState(pWall, pXWall->state ^ 1);
                    break;
            }

            if (bStatus) {
                SetupGibWallState(pWall, pXWall);
                if (pXWall->state) {
                    CGibVelocity vel(100, 100, 250);
                    int nType = ClipRange(pXWall->data, 0, 31);
                    if (nType > 0)
                        GibWall(pWall, (GIBTYPE)nType, &vel);
                }
            }
            return;
        default:
            switch (event.cmd) {
                case kCmdOff:
                    SetWallState(pWall, 0);
                    break;
                case kCmdOn:
                    SetWallState(pWall, 1);
                    break;
                default:
                    SetWallState(pWall, pXWall->state ^ 1);
                    break;
            }
            return;
    }


}

void SectorStartSound(sectortype* pSector, int nState)
{
    BloodSectIterator it(pSector);
    while (auto actor = it.Next())
    {
        spritetype *pSprite = &actor->s();
        if (pSprite->statnum == kStatDecoration && pSprite->type == kSoundSector && actor->hasX())
        {
            XSPRITE *pXSprite = &actor->x();
            if (nState)
            {
                if (pXSprite->data3)
                    sfxPlay3DSound(actor, pXSprite->data3, 0, 0);
            }
            else
            {
                if (pXSprite->data1)
                    sfxPlay3DSound(actor, pXSprite->data1, 0, 0);
            }
        }
    }
}

void SectorEndSound(sectortype* pSector, int nState)
{
    BloodSectIterator it(pSector);
    while (auto actor = it.Next())
    {
        spritetype* pSprite = &actor->s();
        if (pSprite->statnum == kStatDecoration && pSprite->type == kSoundSector && actor->hasX())
        {
            XSPRITE *pXSprite = &actor->x();
            if (nState)
            {
                if (pXSprite->data2)
                    sfxPlay3DSound(actor, pXSprite->data2, 0, 0);
            }
            else
            {
                if (pXSprite->data4)
                    sfxPlay3DSound(actor, pXSprite->data4, 0, 0);
            }
        }
    }
}

void PathSound(sectortype* pSector, int nSound)
{
    BloodSectIterator it(pSector);
    while (auto actor = it.Next())
    {
        spritetype* pSprite = &actor->s();
        if (pSprite->statnum == kStatDecoration && pSprite->type == kSoundSector)
            sfxPlay3DSound(actor, nSound, 0, 0);
    }
}

void DragPoint(walltype* pWall, int x, int y)
{
    viewInterpolateWall(pWall);
    pWall->move(x, y);

    int vsi = wall.Size();
    auto prevWall = pWall;
    do
    {
        if (prevWall->twoSided())
        {
            prevWall = prevWall->nextWall()->point2Wall();
            viewInterpolateWall(prevWall);
            prevWall->move(x, y);
        }
        else
        {
            prevWall = pWall;
            do
            {
                auto lw = prevWall->lastWall();
                if (lw && lw->twoSided())
                {
                    prevWall = lw->nextWall();
                    viewInterpolateWall(prevWall);
                    prevWall->move(x, y);
                }
                else
                    break;
                vsi--;
            } while (prevWall != pWall && vsi > 0);
            break;
        }
        vsi--;
    } while (prevWall != pWall && vsi > 0);
}

void TranslateSector(sectortype* pSector, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9, int a10, int a11, char a12)
{
    int x, y;
    XSECTOR *pXSector = &pSector->xs();
    int v20 = interpolatedvalue(a6, a9, a2);
    int vc = interpolatedvalue(a6, a9, a3);
    int v28 = vc - v20;
    int v24 = interpolatedvalue(a7, a10, a2);
    int v8 = interpolatedvalue(a7, a10, a3);
    int v2c = v8 - v24;
    int v44 = interpolatedvalue(a8, a11, a2);
    int vbp = interpolatedvalue(a8, a11, a3);
    int v14 = vbp - v44;
    if (a12)
    {
        for (auto& wal : wallsofsector(pSector))
        {
            x = wal.baseWall.X;
            y = wal.baseWall.Y;
            if (vbp)
                RotatePoint((int*)&x, (int*)&y, vbp, a4, a5);
            DragPoint(&wal, x+vc-a4, y+v8-a5);
        }
    }
    else
    {
        for (auto& wal : wallsofsector(pSector))
        {
            auto p2Wall = wal.point2Wall();
            x = wal.baseWall.X;
            y = wal.baseWall.Y;
            if (wal.cstat & CSTAT_WALL_MOVE_FORWARD)
            {
                if (vbp)
                    RotatePoint((int*)&x, (int*)&y, vbp, a4, a5);
                DragPoint(&wal, x+vc-a4, y+v8-a5);
                if ((p2Wall->cstat & CSTAT_WALL_MOVE_MASK) == 0)
                {
                    x = p2Wall->baseWall.X;
                    y = p2Wall->baseWall.Y;
                    if (vbp)
                        RotatePoint((int*)&x, (int*)&y, vbp, a4, a5);
                    DragPoint(p2Wall, x+vc-a4, y+v8-a5);
                }
                continue;
            }
            if (wal.cstat & CSTAT_WALL_MOVE_BACKWARD)
            {
                if (vbp)
                    RotatePoint((int*)&x, (int*)&y, -vbp, a4, a5);
                DragPoint(&wal, x-(vc-a4), y-(v8-a5));
                if ((p2Wall->cstat & CSTAT_WALL_MOVE_MASK) == 0)
                {
                    x = p2Wall->baseWall.X;
                    y = p2Wall->baseWall.Y;
                    if (vbp)
                        RotatePoint((int*)&x, (int*)&y, -vbp, a4, a5);
                    DragPoint(p2Wall, x-(vc-a4), y-(v8-a5));
                }
                continue;
            }
        }
    }
    BloodSectIterator it(pSector);
    while (auto actor = it.Next())
    {
        spritetype *pSprite = &actor->s();
        // allow to move markers by sector movements in game if flags 1 is added in editor.
        switch (pSprite->statnum) {
            case kStatMarker:
            case kStatPathMarker:
                #ifdef NOONE_EXTENSIONS
                    if (!gModernMap || !(pSprite->flags & 0x1)) continue;
                #else
                    continue;
                #endif
                break;
        }

        x = actor->basePoint.X;
        y = actor->basePoint.Y;
        if (pSprite->cstat & CSTAT_SPRITE_MOVE_FORWARD)
        {
            if (vbp)
                RotatePoint((int*)&x, (int*)&y, vbp, a4, a5);
            viewBackupSpriteLoc(actor);
            pSprite->ang = (pSprite->ang+v14)&2047;
            pSprite->pos.X = x+vc-a4;
            pSprite->pos.Y = y+v8-a5;
        }
        else if (pSprite->cstat & CSTAT_SPRITE_MOVE_REVERSE)
        {
            if (vbp)
                RotatePoint((int*)& x, (int*)& y, -vbp, a4, a4);
            viewBackupSpriteLoc(actor);
            pSprite->ang = (pSprite->ang-v14)&2047;
            pSprite->pos.X = x-(vc-a4);
            pSprite->pos.Y = y-(v8-a5);
        }
        else if (pXSector->Drag)
        {
            int top, bottom;
            GetSpriteExtents(pSprite, &top, &bottom);
            int floorZ = getflorzofslopeptr(pSector, pSprite->pos.X, pSprite->pos.Y);
            if (!(pSprite->cstat & CSTAT_SPRITE_ALIGNMENT_MASK) && floorZ <= bottom)
            {
                if (v14)
                    RotatePoint((int*)&pSprite->pos.X, (int*)&pSprite->pos.Y, v14, v20, v24);
                viewBackupSpriteLoc(actor);
                pSprite->ang = (pSprite->ang+v14)&2047;
                pSprite->pos.X += v28;
                pSprite->pos.Y += v2c;
            }
        }
    }
}

void ZTranslateSector(sectortype* pSector, XSECTOR *pXSector, int a3, int a4)
{
    viewInterpolateSector(pSector);
    int dz = pXSector->onFloorZ-pXSector->offFloorZ;
    if (dz != 0)
    {
        int oldZ = pSector->floorz;
        pSector->baseFloor = pSector->floorz = pXSector->offFloorZ + MulScale(dz, GetWaveValue(a3, a4), 16);
        pSector->velFloor += (pSector->floorz-oldZ)<<8;

        BloodSectIterator it(pSector);
        while (auto actor = it.Next())
        {
            spritetype* pSprite = &actor->s();
            if (pSprite->statnum == kStatMarker || pSprite->statnum == kStatPathMarker)
                continue;
            int top, bottom;
            GetSpriteExtents(pSprite, &top, &bottom);
            if (pSprite->cstat & CSTAT_SPRITE_MOVE_FORWARD)
            {
                viewBackupSpriteLoc(actor);
                pSprite->pos.Z += pSector->floorz-oldZ;
            }
            else if (pSprite->flags&2)
                pSprite->flags |= 4;
            else if (oldZ <= bottom && !(pSprite->cstat & CSTAT_SPRITE_ALIGNMENT_MASK))
            {
                viewBackupSpriteLoc(actor);
                pSprite->pos.Z += pSector->floorz-oldZ;
            }
        }
    }
    dz = pXSector->onCeilZ-pXSector->offCeilZ;
    if (dz != 0)
    {
        int oldZ = pSector->ceilingz;
        pSector->baseCeil = pSector->ceilingz = pXSector->offCeilZ + MulScale(dz, GetWaveValue(a3, a4), 16);
        pSector->velCeil += (pSector->ceilingz-oldZ)<<8;

        BloodSectIterator it(pSector);
        while (auto actor = it.Next())
        {
            spritetype* pSprite = &actor->s();
            if (pSprite->statnum == kStatMarker || pSprite->statnum == kStatPathMarker)
                continue;
            if (pSprite->cstat & CSTAT_SPRITE_MOVE_REVERSE)
            {
                viewBackupSpriteLoc(actor);
                pSprite->pos.Z += pSector->ceilingz-oldZ;
            }
        }
    }
}

DBloodActor* GetHighestSprite(sectortype* pSector, int nStatus, int *z)
{
    *z = pSector->floorz;
    DBloodActor* found = nullptr;

    BloodSectIterator it(pSector);
    while (auto actor = it.Next())
    {
        spritetype* pSprite = &actor->s();
        if (pSprite->statnum == nStatus || nStatus == kStatFree)
        {
            int top, bottom;
            GetSpriteExtents(pSprite, &top, &bottom);
            if (top-pSprite->pos.Z > *z)
            {
                *z = top-pSprite->pos.Z;
                found = actor;
            }
        }
    }
    return found;
}

DBloodActor* GetCrushedSpriteExtents(sectortype* pSector, int *pzTop, int *pzBot)
{
    assert(pzTop != NULL && pzBot != NULL);
    assert(pSector);
    DBloodActor* found = nullptr;
    int foundz = pSector->ceilingz;

    BloodSectIterator it(pSector);
    while (auto actor = it.Next())
    {
        spritetype* pSprite = &actor->s();
        if (pSprite->statnum == kStatDude || pSprite->statnum == kStatThing)
        {
            int top, bottom;
            GetActorExtents(actor, &top, &bottom);
            if (foundz > top)
            {
                foundz = top;
                *pzTop = top;
                *pzBot = bottom;
                found = actor;
            }
        }
    }
    return found;
}

int VCrushBusy(sectortype *pSector, unsigned int a2)
{
    assert(pSector && pSector->hasX());
    XSECTOR *pXSector = &pSector->xs();
    int nWave;
    if (pXSector->busy < a2)
        nWave = pXSector->busyWaveA;
    else
        nWave = pXSector->busyWaveB;
    int dz1 = pXSector->onCeilZ - pXSector->offCeilZ;
    int vc = pXSector->offCeilZ;
    if (dz1 != 0)
        vc += MulScale(dz1, GetWaveValue(a2, nWave), 16);
    int dz2 = pXSector->onFloorZ - pXSector->offFloorZ;
    int v10 = pXSector->offFloorZ;
    if (dz2 != 0)
        v10 += MulScale(dz2, GetWaveValue(a2, nWave), 16);
    int v18;
    if (GetHighestSprite(pSector, 6, &v18) && vc >= v18)
        return 1;
    viewInterpolateSector(pSector);
    if (dz1 != 0)
        pSector->ceilingz = vc;
    if (dz2 != 0)
        pSector->floorz = v10;
    pXSector->busy = a2;
    if (pXSector->command == kCmdLink && pXSector->txID)
        evSendSector(pSector,pXSector->txID, kCmdLink);
    if ((a2&0xffff) == 0)
    {
        SetSectorState(pSector, FixedToInt(a2));
        SectorEndSound(pSector, FixedToInt(a2));
        return 3;
    }
    return 0;
}

int VSpriteBusy(sectortype* pSector, unsigned int a2)
{
    assert(pSector && pSector->hasX());
    XSECTOR* pXSector = &pSector->xs();
    int nWave;
    if (pXSector->busy < a2)
        nWave = pXSector->busyWaveA;
    else
        nWave = pXSector->busyWaveB;
    int dz1 = pXSector->onFloorZ - pXSector->offFloorZ;
    if (dz1 != 0)
    {
        BloodSectIterator it(pSector);
        while (auto actor = it.Next())
        {
            spritetype *pSprite = &actor->s();
            if (pSprite->cstat & CSTAT_SPRITE_MOVE_FORWARD)
            {
                viewBackupSpriteLoc(actor);
                pSprite->pos.Z = actor->basePoint.Z+MulScale(dz1, GetWaveValue(a2, nWave), 16);
            }
        }
    }
    int dz2 = pXSector->onCeilZ - pXSector->offCeilZ;
    if (dz2 != 0)
    {
        BloodSectIterator it(pSector);
        while (auto actor = it.Next())
        {
            spritetype* pSprite = &actor->s();
            if (pSprite->cstat & CSTAT_SPRITE_MOVE_REVERSE)
            {
                viewBackupSpriteLoc(actor);
                pSprite->pos.Z = actor->basePoint.Z + MulScale(dz2, GetWaveValue(a2, nWave), 16);
            }
        }
    }
    pXSector->busy = a2;
    if (pXSector->command == kCmdLink && pXSector->txID)
        evSendSector(pSector,pXSector->txID, kCmdLink);
    if ((a2&0xffff) == 0)
    {
        SetSectorState(pSector, FixedToInt(a2));
        SectorEndSound(pSector, FixedToInt(a2));
        return 3;
    }
    return 0;
}

int VDoorBusy(sectortype* pSector, unsigned int a2)
{
    assert(pSector && pSector->hasX());
    XSECTOR *pXSector = &pSector->xs();
    int vbp;
    if (pXSector->state)
        vbp = 65536/ClipLow((120*pXSector->busyTimeA)/10, 1);
    else
        vbp = -65536/ClipLow((120*pXSector->busyTimeB)/10, 1);
    int top, bottom;
    auto actor = GetCrushedSpriteExtents(pSector, &top, &bottom);
    if (actor && a2 > pXSector->busy)
    {
        assert(actor->hasX());
        XSPRITE *pXSprite = &actor->x();
        if (pXSector->onCeilZ > pXSector->offCeilZ || pXSector->onFloorZ < pXSector->offFloorZ)
        {
            if (pXSector->interruptable)
            {
                if (pXSector->Crush)
                {
                    if (pXSprite->health <= 0)
                        return 2;
                    int nDamage;
                    if (pXSector->data == 0)
                        nDamage = 500;
                    else
                        nDamage = pXSector->data;
                    actDamageSprite(actor, actor, kDamageFall, nDamage<<4);
                }
                a2 = ClipRange(a2-(vbp/2)*4, 0, 65536);
            }
            else if (pXSector->Crush && pXSprite->health > 0)
            {
                int nDamage;
                if (pXSector->data == 0)
                    nDamage = 500;
                else
                    nDamage = pXSector->data;
                actDamageSprite(actor, actor, kDamageFall, nDamage<<4);
                a2 = ClipRange(a2-(vbp/2)*4, 0, 65536);
            }
        }
    }
    else if (actor && a2 < pXSector->busy)
    {
        assert(actor->hasX());
        XSPRITE* pXSprite = &actor->x();
        if (pXSector->offCeilZ > pXSector->onCeilZ || pXSector->offFloorZ < pXSector->onFloorZ)
        {
            if (pXSector->interruptable)
            {
                if (pXSector->Crush)
                {
                    if (pXSprite->health <= 0)
                        return 2;
                    int nDamage;
                    if (pXSector->data == 0)
                        nDamage = 500;
                    else
                        nDamage = pXSector->data;
                    actDamageSprite(actor, actor, kDamageFall, nDamage<<4);
                }
                a2 = ClipRange(a2+(vbp/2)*4, 0, 65536);
            }
            else if (pXSector->Crush && pXSprite->health > 0)
            {
                int nDamage;
                if (pXSector->data == 0)
                    nDamage = 500;
                else
                    nDamage = pXSector->data;
                actDamageSprite(actor, actor, kDamageFall, nDamage<<4);
                a2 = ClipRange(a2+(vbp/2)*4, 0, 65536);
            }
        }
    }
    int nWave;
    if (pXSector->busy < a2)
        nWave = pXSector->busyWaveA;
    else
        nWave = pXSector->busyWaveB;
    ZTranslateSector(pSector, pXSector, a2, nWave);
    pXSector->busy = a2;
    if (pXSector->command == kCmdLink && pXSector->txID)
        evSendSector(pSector,pXSector->txID, kCmdLink);
    if ((a2&0xffff) == 0)
    {
        SetSectorState(pSector, FixedToInt(a2));
        SectorEndSound(pSector, FixedToInt(a2));
        return 3;
    }
    return 0;
}

int HDoorBusy(sectortype* pSector, unsigned int a2)
{
    assert(pSector && pSector->hasX());
    XSECTOR* pXSector = &pSector->xs();
    int nWave;
    if (pXSector->busy < a2)
        nWave = pXSector->busyWaveA;
    else
        nWave = pXSector->busyWaveB;
    if (!pXSector->marker0 || !pXSector->marker1) return 0;
    spritetype *pSprite1 = &pXSector->marker0->s();
    spritetype *pSprite2 = &pXSector->marker1->s();
    TranslateSector(pSector, GetWaveValue(pXSector->busy, nWave), GetWaveValue(a2, nWave), pSprite1->pos.X, pSprite1->pos.Y, pSprite1->pos.X, pSprite1->pos.Y, pSprite1->ang, pSprite2->pos.X, pSprite2->pos.Y, pSprite2->ang, pSector->type == kSectorSlide);
    ZTranslateSector(pSector, pXSector, a2, nWave);
    pXSector->busy = a2;
    if (pXSector->command == kCmdLink && pXSector->txID)
        evSendSector(pSector, pXSector->txID, kCmdLink);
    if ((a2&0xffff) == 0)
    {
        SetSectorState(pSector, FixedToInt(a2));
        SectorEndSound(pSector, FixedToInt(a2));
        return 3;
    }
    return 0;
}

int RDoorBusy(sectortype* pSector, unsigned int a2)
{
    assert(pSector && pSector->hasX());
    XSECTOR* pXSector = &pSector->xs();
    int nWave;
    if (pXSector->busy < a2)
        nWave = pXSector->busyWaveA;
    else
        nWave = pXSector->busyWaveB;
    if (!pXSector->marker0) return 0;
    spritetype* pSprite1 = &pXSector->marker0->s();
    TranslateSector(pSector, GetWaveValue(pXSector->busy, nWave), GetWaveValue(a2, nWave), pSprite1->pos.X, pSprite1->pos.Y, pSprite1->pos.X, pSprite1->pos.Y, 0, pSprite1->pos.X, pSprite1->pos.Y, pSprite1->ang, pSector->type == kSectorRotate);
    ZTranslateSector(pSector, pXSector, a2, nWave);
    pXSector->busy = a2;
    if (pXSector->command == kCmdLink && pXSector->txID)
        evSendSector(pSector, pXSector->txID, kCmdLink);
    if ((a2&0xffff) == 0)
    {
        SetSectorState(pSector, FixedToInt(a2));
        SectorEndSound(pSector, FixedToInt(a2));
        return 3;
    }
    return 0;
}

int StepRotateBusy(sectortype* pSector, unsigned int a2)
{
    assert(pSector && pSector->hasX());
    XSECTOR* pXSector = &pSector->xs();
    if (!pXSector->marker0) return 0;
    spritetype* pSprite1 = &pXSector->marker0->s();
    int vbp;
    if (pXSector->busy < a2)
    {
        vbp = pXSector->data+pSprite1->ang;
        int nWave = pXSector->busyWaveA;
        TranslateSector(pSector, GetWaveValue(pXSector->busy, nWave), GetWaveValue(a2, nWave), pSprite1->pos.X, pSprite1->pos.Y, pSprite1->pos.X, pSprite1->pos.Y, pXSector->data, pSprite1->pos.X, pSprite1->pos.Y, vbp, 1);
    }
    else
    {
        vbp = pXSector->data-pSprite1->ang;
        int nWave = pXSector->busyWaveB;
        TranslateSector(pSector, GetWaveValue(pXSector->busy, nWave), GetWaveValue(a2, nWave), pSprite1->pos.X, pSprite1->pos.Y, pSprite1->pos.X, pSprite1->pos.Y, vbp, pSprite1->pos.X, pSprite1->pos.Y, pXSector->data, 1);
    }
    pXSector->busy = a2;
    if (pXSector->command == kCmdLink && pXSector->txID)
        evSendSector(pSector, pXSector->txID, kCmdLink);
    if ((a2&0xffff) == 0)
    {
        SetSectorState(pSector, FixedToInt(a2));
        SectorEndSound(pSector, FixedToInt(a2));
        pXSector->data = vbp&2047;
        return 3;
    }
    return 0;
}

int GenSectorBusy(sectortype* pSector, unsigned int a2)
{
    assert(pSector && pSector->hasX());
    XSECTOR* pXSector = &pSector->xs();
    pXSector->busy = a2;
    if (pXSector->command == kCmdLink && pXSector->txID)
        evSendSector(pSector, pXSector->txID, kCmdLink);
    if ((a2&0xffff) == 0)
    {
        SetSectorState(pSector, FixedToInt(a2));
        SectorEndSound(pSector, FixedToInt(a2));
        return 3;
    }
    return 0;
}

int PathBusy(sectortype* pSector, unsigned int a2)
{
    assert(pSector && pSector->hasX());
    XSECTOR* pXSector = &pSector->xs();

    auto basepath = pXSector->basePath;
    auto marker0 = pXSector->marker0;
    auto marker1 = pXSector->marker1;
    if (!basepath || !marker0 || !marker1) return 0;

    XSPRITE *pXSprite1 = &pXSector->marker0->x();
    XSPRITE *pXSprite2 = &pXSector->marker1->x();

    int nWave = pXSprite1->wave;
    TranslateSector(pSector, GetWaveValue(pXSector->busy, nWave), GetWaveValue(a2, nWave), basepath->spr.pos.X, basepath->spr.pos.Y, marker0->spr.pos.X, marker0->spr.pos.Y, marker0->spr.ang, marker1->spr.pos.X, marker1->spr.pos.Y, marker1->spr.ang, 1);
    ZTranslateSector(pSector, pXSector, a2, nWave);
    pXSector->busy = a2;
    if ((a2&0xffff) == 0)
    {
        evPostSector(pSector, (120*pXSprite2->waitTime)/10, kCmdOn);
        pXSector->state = 0;
        pXSector->busy = 0;
        if (pXSprite1->data4)
            PathSound(pSector, pXSprite1->data4);
        pXSector->marker0 = marker1;
        pXSector->data = pXSprite2->data1;
        return 3;
    }
    return 0;
}

void OperateDoor(sectortype* pSector, EVENT event, BUSYID busyWave) 
{
    auto pXSector = &pSector->xs();
    switch (event.cmd) {
        case kCmdOff:
            if (!pXSector->busy) break;
            AddBusy(pSector, busyWave, -65536/ClipLow((pXSector->busyTimeB*120)/10, 1));
            SectorStartSound(pSector, 1);
            break;
        case kCmdOn:
            if (pXSector->busy == 0x10000) break;
            AddBusy(pSector, busyWave, 65536/ClipLow((pXSector->busyTimeA*120)/10, 1));
            SectorStartSound(pSector, 0);
            break;
        default:
            if (pXSector->busy & 0xffff)  {
                if (pXSector->interruptable) {
                    ReverseBusy(pSector, busyWave);
                    pXSector->state = !pXSector->state;
                }
            } else {
                char t = !pXSector->state; int nDelta;
            
                if (t) nDelta = 65536/ClipLow((pXSector->busyTimeA*120)/10, 1);
                else nDelta = -65536/ClipLow((pXSector->busyTimeB*120)/10, 1);
            
                AddBusy(pSector, busyWave, nDelta);
                SectorStartSound(pSector, pXSector->state);
            }
            break;
    }
}

bool SectorContainsDudes(sectortype * pSector)
{
    BloodSectIterator it(pSector);
    while (auto actor = it.Next())
    {
        spritetype* pSprite = &actor->s();
        if (pSprite->statnum == kStatDude)
            return 1;
    }
    return 0;
}

void TeleFrag(DBloodActor* killer, sectortype* pSector)
{
    BloodSectIterator it(pSector);
    while (auto victim = it.Next())
    {
        if (victim->spr.statnum == kStatDude)
            actDamageSprite(killer, victim, kDamageExplode, 4000);
        else if (victim->spr.statnum == kStatThing)
            actDamageSprite(killer, victim, kDamageExplode, 4000);
    }
}

void OperateTeleport(sectortype* pSector)
{
    assert(pSector);
    auto pXSector = &pSector->xs();
    auto destactor = pXSector->marker0;
    assert(destactor != nullptr);
    spritetype *pDest = &destactor->s();
    assert(pDest->statnum == kStatMarker);
    assert(pDest->type == kMarkerWarpDest);
    assert(pDest->insector());
    BloodSectIterator it(pSector);
    while (auto actor = it.Next())
    {
        spritetype *pSprite = &actor->s();
        if (pSprite->statnum == kStatDude)
        {
            PLAYER *pPlayer;
            char bPlayer = IsPlayerSprite(pSprite);
            if (bPlayer)
                pPlayer = &gPlayer[pSprite->type-kDudePlayer1];
            else
                pPlayer = NULL;
            if (bPlayer || !SectorContainsDudes(pDest->sector()))
            {
                if (!(gGameOptions.uNetGameFlags & 2))
                {
                    TeleFrag(pXSector->actordata, pDest->sector());
                }
                pSprite->pos.X = pDest->pos.X;
                pSprite->pos.Y = pDest->pos.Y;
                pSprite->pos.Z += pDest->sector()->floorz - pSector->floorz;
                pSprite->ang = pDest->ang;
                ChangeActorSect(actor, pDest->sector());
                sfxPlay3DSound(destactor, 201, -1, 0);
                actor->xvel = actor->yvel = actor->zvel = 0;
                actor->interpolated = false;
                viewBackupSpriteLoc(actor);
                if (pPlayer)
                {
                    playerResetInertia(pPlayer);
                    pPlayer->zViewVel = pPlayer->zWeaponVel = 0;
                    pPlayer->angle.settarget(pSprite->ang, true);
                }
            }
        }
    }
}

void OperatePath(sectortype* pSector, EVENT event)
{
    DBloodActor* actor;
    spritetype *pSprite = NULL;
    XSPRITE *pXSprite;
    assert(pSector);
    auto pXSector = &pSector->xs();
    if (!pXSector->marker0) return;
    spritetype* pSprite1 = &pXSector->marker0->s();
    XSPRITE *pXSprite2 = &pXSector->marker0->x();
    int nId = pXSprite2->data2;
    
    BloodStatIterator it(kStatPathMarker);
    while ((actor = it.Next()))
    {
        pSprite = &actor->s();
        if (pSprite->type == kMarkerPath)
        {
            pXSprite = &actor->x();
            if (pXSprite->data1 == nId)
                break;
        }
    }

    // trigger marker after it gets reached
    #ifdef NOONE_EXTENSIONS
        if (gModernMap && pXSprite2->state != 1)
            trTriggerSprite(pXSector->marker0, kCmdOn);
    #endif

    if (actor == nullptr) {
        viewSetSystemMessage("Unable to find path marker with id #%d for path sector", nId);
        pXSector->state = 0;
        pXSector->busy = 0;
        return;
    }
        
    pXSector->marker1 = actor;
    pXSector->offFloorZ = pSprite1->pos.Z;
    pXSector->onFloorZ = pSprite->pos.Z;
    switch (event.cmd) {
        case kCmdOn:
            pXSector->state = 0;
            pXSector->busy = 0;
            AddBusy(pSector, BUSYID_7, 65536/ClipLow((120*pXSprite2->busyTime)/10,1));
            if (pXSprite2->data3) PathSound(pSector, pXSprite2->data3);
            break;
    }
}

void OperateSector(sectortype* pSector, EVENT event)
{
    if (!pSector->hasX()) return;
    auto pXSector = &pSector->xs();
    #ifdef NOONE_EXTENSIONS
    if (gModernMap && modernTypeOperateSector(pSector, event))
        return;
    #endif

    switch (event.cmd) {
        case kCmdLock:
            pXSector->locked = 1;
            break;
        case kCmdUnlock:
            pXSector->locked = 0;
            break;
        case kCmdToggleLock:
            pXSector->locked ^= 1;
            break;
        case kCmdStopOff:
            pXSector->stopOn = 0;
            pXSector->stopOff = 1;
            break;
        case kCmdStopOn:
            pXSector->stopOn = 1;
            pXSector->stopOff = 0;
            break;
        case kCmdStopNext:
            pXSector->stopOn = 1;
            pXSector->stopOff = 1;
            break;
        default:
        #ifdef NOONE_EXTENSIONS
        if (gModernMap && pXSector->unused1) break;
        #endif
            switch (pSector->type) {
                case kSectorZMotionSprite:
                    OperateDoor(pSector,event, BUSYID_1);
                    break;
                case kSectorZMotion:
                    OperateDoor(pSector,event, BUSYID_2);
                    break;
                case kSectorSlideMarked:
                case kSectorSlide:
                    OperateDoor(pSector,event, BUSYID_3);
                    break;
                case kSectorRotateMarked:
                case kSectorRotate:
                    OperateDoor(pSector,event, BUSYID_4);
                    break;
                case kSectorRotateStep:
                    switch (event.cmd) {
                        case kCmdOn:
                            pXSector->state = 0;
                            pXSector->busy = 0;
                            AddBusy(pSector, BUSYID_5, 65536/ClipLow((120*pXSector->busyTimeA)/10, 1));
                            SectorStartSound(pSector, 0);
                            break;
                        case kCmdOff:
                            pXSector->state = 1;
                            pXSector->busy = 65536;
                            AddBusy(pSector, BUSYID_5, -65536/ClipLow((120*pXSector->busyTimeB)/10, 1));
                            SectorStartSound(pSector, 1);
                            break;
                    }
                    break;
                case kSectorTeleport:
                    OperateTeleport(pSector);
                    break;
                case kSectorPath:
                    OperatePath(pSector, event);
                    break;
                default:
                    if (!pXSector->busyTimeA && !pXSector->busyTimeB) {
                        
                        switch (event.cmd) {
                            case kCmdOff:
                                SetSectorState(pSector, 0);
                                break;
                            case kCmdOn:
                                SetSectorState(pSector, 1);
                                break;
                            default:
                                SetSectorState(pSector, pXSector->state ^ 1);
                                break;
                        }

                    } else {
                        
                        OperateDoor(pSector,event, BUSYID_6);

                    }

                    break;
            }
            break;
    }
}

void InitPath(sectortype* pSector, XSECTOR *pXSector)
{
    DBloodActor* actor = nullptr;
    spritetype *pSprite = nullptr;
    XSPRITE *pXSprite;
    assert(pSector);
    int nId = pXSector->data;

    BloodStatIterator it(kStatPathMarker);
    while ((actor = it.Next()))
    {
        pSprite = &actor->s();
        if (pSprite->type == kMarkerPath && actor->hasX())
        {
            pXSprite = &actor->x();
            if (pXSprite->data1 == nId)
                break;
        }
    }
    
    if (pSprite == nullptr) {
        viewSetSystemMessage("Unable to find path marker with id #%d for path sector", nId);
        return;
        
    }

    pXSector->basePath = pXSector->marker0 = actor;
    if (pXSector->state)
        evPostSector(pSector, 0, kCmdOn);
}

void LinkSector(sectortype* pSector, EVENT event)
{
    auto pXSector = &pSector->xs();
    int nBusy = GetSourceBusy(event);
    switch (pSector->type) {
        case kSectorZMotionSprite:
            VSpriteBusy(pSector, nBusy);
            break;
        case kSectorZMotion:
            VDoorBusy(pSector, nBusy);
            break;
        case kSectorSlideMarked:
        case kSectorSlide:
            HDoorBusy(pSector, nBusy);
            break;
        case kSectorRotateMarked:
        case kSectorRotate:
            // force synchronised input here for now.
            setForcedSyncInput();
            RDoorBusy(pSector, nBusy);
            break;
        default:
            pXSector->busy = nBusy;
            if ((pXSector->busy&0xffff) == 0)
                SetSectorState(pSector, FixedToInt(nBusy));
            break;
    }
}

void LinkSprite(DBloodActor* actor, EVENT event) 
{
    spritetype *pSprite = &actor->s();
    auto pXSprite = &actor->x();
    int nBusy = GetSourceBusy(event);

    switch (pSprite->type)  {
        case kSwitchCombo:
        {
            if (event.isActor())
            {
                auto actor2 = event.getActor();

                pXSprite->data1 = actor2 && actor2->hasX()? actor2->xspr.data1 : 0;
                if (pXSprite->data1 == pXSprite->data2)
                    SetSpriteState(actor, 1);
                else
                    SetSpriteState(actor, 0);
            }
        }
        break;
        default:
        {
            pXSprite->busy = nBusy;
            if ((pXSprite->busy & 0xffff) == 0)
                SetSpriteState(actor, FixedToInt(nBusy));
        }
        break;
    }
}

void LinkWall(walltype* pWall, EVENT& event)
{
    int nBusy = GetSourceBusy(event);
    pWall->xw().busy = nBusy;
    if ((pWall->xw().busy & 0xffff) == 0)
        SetWallState(pWall, FixedToInt(nBusy));
}

void trTriggerSector(sectortype* pSector, int command) 
{
    auto pXSector = &pSector->xs();
    if (!pXSector->locked && !pXSector->isTriggered) {
        
        if (pXSector->triggerOnce) 
            pXSector->isTriggered = 1;
        
        if (pXSector->decoupled && pXSector->txID > 0)
            evSendSector(pSector, pXSector->txID, (COMMAND_ID)pXSector->command);
        
        else {
            EVENT event;
            event.cmd = command;
            OperateSector(pSector, event);
        }

    }
}

void trTriggerWall(walltype* pWall, int command) 
{
    if (!pWall->hasX()) return;
    auto pXWall = &pWall->xw();
    if (!pXWall->locked && !pXWall->isTriggered) {
        
        if (pXWall->triggerOnce)
            pXWall->isTriggered = 1;
        
        if (pXWall->decoupled && pXWall->txID > 0)
            evSendWall(pWall, pXWall->txID, (COMMAND_ID)pXWall->command);

        else {
            EVENT event;
            event.cmd = command;
            OperateWall(pWall, event);
        }

    }
}

void trTriggerSprite(DBloodActor* actor, int command) 
{
    auto pXSprite = &actor->x();

    if (!pXSprite->locked && !pXSprite->isTriggered) {
        
        if (pXSprite->triggerOnce)
            pXSprite->isTriggered = 1;

        if (pXSprite->Decoupled && pXSprite->txID > 0)
           evSendActor(actor, pXSprite->txID, (COMMAND_ID)pXSprite->command);
        
        else {
            EVENT event;
            event.cmd = command;
            OperateSprite(actor, event);
        }

    }
}

void trMessageSector(sectortype* pSector, EVENT event) 
{
    if (!pSector->hasX()) return;
    XSECTOR *pXSector = &pSector->xs();
    if (!pXSector->locked || event.cmd == kCmdUnlock || event.cmd == kCmdToggleLock) 
    {
        switch (event.cmd) 
        {
            case kCmdLink:
                LinkSector(pSector, event);
                break;
            #ifdef NOONE_EXTENSIONS
            case kCmdModernUse:
                modernTypeTrigger(OBJ_SECTOR, pSector, nullptr, nullptr, event);
                break;
            #endif
            default:
                OperateSector(pSector, event);
                break;
        }
    }
}

void trMessageWall(walltype* pWall, EVENT& event) 
{
    assert(pWall->hasX());
    
    XWALL *pXWall = &pWall->xw();
    if (!pXWall->locked || event.cmd == kCmdUnlock || event.cmd == kCmdToggleLock) 
    {
        switch (event.cmd) {
            case kCmdLink:
                LinkWall(pWall, event);
                break;
            #ifdef NOONE_EXTENSIONS
            case kCmdModernUse:
                modernTypeTrigger(OBJ_WALL, nullptr, pWall, nullptr, event);
                break;
            #endif
            default:
                OperateWall(pWall, event);
                break;
        }
    }
}

void trMessageSprite(DBloodActor* actor, EVENT event) 
{
    auto pSprite = &actor->s();
    auto pXSprite = &actor->x();
    if (pSprite->statnum != kStatFree) {

        if (!pXSprite->locked || event.cmd == kCmdUnlock || event.cmd == kCmdToggleLock) 
        {
            switch (event.cmd) 
            {
                case kCmdLink:
                    LinkSprite(actor, event);
                    break;
                #ifdef NOONE_EXTENSIONS
                case kCmdModernUse:
                    modernTypeTrigger(OBJ_SPRITE, 0, 0, actor, event);
                    break;
                #endif
                default:
                    OperateSprite(actor, event);
                    break;
            }
        }
    }
}



void ProcessMotion(void)
{
    for(auto& sect: sector)
    {
        sectortype* pSector = &sect;

        if (!pSector->hasX()) continue;
        XSECTOR* pXSector = &pSector->xs();
        if (pXSector->bobSpeed != 0)
        {
            if (pXSector->bobAlways)
                pXSector->bobTheta += pXSector->bobSpeed;
            else if (pXSector->busy == 0)
                continue;
            else
                pXSector->bobTheta += MulScale(pXSector->bobSpeed, pXSector->busy, 16);
            int vdi = MulScale(Sin(pXSector->bobTheta), pXSector->bobZRange<<8, 30);

            BloodSectIterator it(pSector);
            while (auto actor = it.Next())
            {
                auto pSprite = &actor->s();
                if (pSprite->cstat & CSTAT_SPRITE_MOVE_MASK)
                {
                    viewBackupSpriteLoc(actor);
                    pSprite->pos.Z += vdi;
                }
            }
            if (pXSector->bobFloor)
            {
                int floorZ = pSector->floorz;
                viewInterpolateSector(pSector);
                pSector->floorz = pSector->baseFloor + vdi;

                BloodSectIterator it(pSector);
                while (auto actor = it.Next())
                {
                    auto pSprite = &actor->s();
                    if (pSprite->flags&2)
                        pSprite->flags |= 4;
                    else
                    {
                        int top, bottom;
                        GetSpriteExtents(pSprite, &top, &bottom);
                        if (bottom >= floorZ && (pSprite->cstat & CSTAT_SPRITE_ALIGNMENT_MASK) == 0)
                        {
                            viewBackupSpriteLoc(actor);
                            pSprite->pos.Z += vdi;
                        }
                    }
                }
            }
            if (pXSector->bobCeiling)
            {
                int ceilZ = pSector->ceilingz;
                viewInterpolateSector(pSector);
                pSector->ceilingz = pSector->baseCeil + vdi;

                BloodSectIterator it(pSector);
                while (auto actor = it.Next())
                {
                    auto pSprite = &actor->s();
                    int top, bottom;
                    GetSpriteExtents(pSprite, &top, &bottom);
                    if (top <= ceilZ && (pSprite->cstat & CSTAT_SPRITE_ALIGNMENT_MASK) == 0)
                    {
                        viewBackupSpriteLoc(actor);
                        pSprite->pos.Z += vdi;
                    }
                }
            }
        }
    }
}

void AlignSlopes(void)
{
    for(auto& sect: sector)
    {
        if (sect.slopewallofs)
        {
            walltype *pWall = sect.firstWall() + sect.slopewallofs;
            walltype *pWall2 = pWall->point2Wall();
            if (pWall->twoSided())
            {
                auto pNextSector = pWall->nextSector();

                int x = (pWall->pos.X+pWall2->pos.X)/2;
                int y = (pWall->pos.Y+pWall2->pos.Y)/2;
                viewInterpolateSector(&sect);
                alignflorslope(&sect, x, y, getflorzofslopeptr(pNextSector, x, y));
                alignceilslope(&sect, x, y, getceilzofslopeptr(pNextSector, x, y));
            }
        }
    }
}

int(*gBusyProc[])(sectortype*, unsigned int) =
{
    VCrushBusy,
    VSpriteBusy,
    VDoorBusy,
    HDoorBusy,
    RDoorBusy,
    StepRotateBusy,
    GenSectorBusy,
    PathBusy
};

void trProcessBusy(void)
{
    for (auto& sect: sector)
    {
        sect.velCeil = sect.velFloor = 0;
    }
    for (auto& b : backwards(gBusy))
    {
        int nStatus;
        int oldBusy = b.busy;
        b.busy = ClipRange(oldBusy+b.delta*4, 0, 65536);
        #ifdef NOONE_EXTENSIONS
            if (!gModernMap || !b.sect->xs().unused1) nStatus = gBusyProc[b.type](b.sect, b.busy);
            else nStatus = 3; // allow to pause/continue motion for sectors any time by sending special command
        #else
            nStatus = gBusyProc[b.type](b.at0, b.at8);
        #endif
        switch (nStatus) {
            case 1:
                b.busy = oldBusy;
                break;
            case 2:
                b.busy = oldBusy;
                b.delta = -b.delta;
                break;
            case 3:
                b = gBusy.Last();
                gBusy.Pop();
                break;
        }
    }
    ProcessMotion();
    AlignSlopes();
}

void InitGenerator(DBloodActor*);

void trInit(TArray<DBloodActor*>& actors)
{
    gBusy.Clear();
    for(auto& wal : wall)
    {
        wal.baseWall.X = wal.pos.X;
        wal.baseWall.Y = wal.pos.Y;
    }
    for(auto actor : actors)
    {
        if (!actor->exists()) continue;
        auto spr = &actor->s();
        spr->inittype = spr->type;
        actor->basePoint = spr->pos;
    }
    for(auto& wal : wall)
    {
        if (wal.hasX())
        {
            XWALL *pXWall = &wal.xw();
            if (pXWall->state)
                pXWall->busy = 65536;
        }
    }

    for(auto& sect: sector)
    {
        sectortype *pSector = &sect;
        pSector->baseFloor = pSector->floorz;
        pSector->baseCeil = pSector->ceilingz;
        if (pSector->hasX())
        {
            XSECTOR *pXSector = &pSector->xs();
            if (pXSector->state)
                pXSector->busy = 65536;
            switch (pSector->type)
            {
            case kSectorCounter:
                #ifdef NOONE_EXTENSIONS 
                if (gModernMap)
                    pXSector->triggerOff = false;
                else
                #endif
                pXSector->triggerOnce = 1;
                evPostSector(pSector, 0, kCallbackCounterCheck);
                break;
            case kSectorZMotion:
            case kSectorZMotionSprite:
                ZTranslateSector(pSector, pXSector, pXSector->busy, 1);
                break;
            case kSectorSlideMarked:
            case kSectorSlide:
            {
                spritetype* pSprite1 = &pXSector->marker0->s();
                spritetype* pSprite2 = &pXSector->marker1->s();
                TranslateSector(pSector, 0, -65536, pSprite1->pos.X, pSprite1->pos.Y, pSprite1->pos.X, pSprite1->pos.Y, pSprite1->ang, pSprite2->pos.X, pSprite2->pos.Y, pSprite2->ang, pSector->type == kSectorSlide);
                for(auto& wal : wallsofsector(pSector))
                {
                    wal.baseWall.X = wal.pos.X;
                    wal.baseWall.Y = wal.pos.Y;
                }
                BloodSectIterator it(pSector);
                while (auto actor = it.Next())
                {
                    actor->basePoint = actor->spr.pos;
                }
                TranslateSector(pSector, 0, pXSector->busy, pSprite1->pos.X, pSprite1->pos.Y, pSprite1->pos.X, pSprite1->pos.Y, pSprite1->ang, pSprite2->pos.X, pSprite2->pos.Y, pSprite2->ang, pSector->type == kSectorSlide);
                ZTranslateSector(pSector, pXSector, pXSector->busy, 1);
                break;
            }
            case kSectorRotateMarked:
            case kSectorRotate:
            {
                spritetype* pSprite1 = &pXSector->marker0->s();
                TranslateSector(pSector, 0, -65536, pSprite1->pos.X, pSprite1->pos.Y, pSprite1->pos.X, pSprite1->pos.Y, 0, pSprite1->pos.X, pSprite1->pos.Y, pSprite1->ang, pSector->type == kSectorRotate);
                for (auto& wal : wallsofsector(pSector))
                {
                    wal.baseWall.X = wal.pos.X;
                    wal.baseWall.Y = wal.pos.Y;
                }
                BloodSectIterator it(pSector);
                while (auto actor = it.Next())
                {
                    actor->basePoint = actor->spr.pos;
                }
                TranslateSector(pSector, 0, pXSector->busy, pSprite1->pos.X, pSprite1->pos.Y, pSprite1->pos.X, pSprite1->pos.Y, 0, pSprite1->pos.X, pSprite1->pos.Y, pSprite1->ang, pSector->type == kSectorRotate);
                ZTranslateSector(pSector, pXSector, pXSector->busy, 1);
                break;
            }
            case kSectorPath:
                InitPath(pSector, pXSector);
                break;
            default:
                break;
            }
        }
    }

    for (auto actor : actors)
    {
        auto pSprite = &actor->s();
        if (pSprite->statnum < kStatFree && actor->hasX())
        {
            auto pXSprite = &actor->x();
            if (pXSprite->state)
                pXSprite->busy = 65536;
            switch (pSprite->type) {
            case kSwitchPadlock:
                pXSprite->triggerOnce = 1;
                break;
            #ifdef NOONE_EXTENSIONS
            case kModernRandom:
            case kModernRandom2:

                if (!gModernMap || pXSprite->state == pXSprite->restState) break;
                evPostActor(actor, (120 * pXSprite->busyTime) / 10, kCmdRepeat);
                if (pXSprite->waitTime > 0)
                    evPostActor(actor, (pXSprite->waitTime * 120) / 10, pXSprite->restState ? kCmdOn : kCmdOff);
                break;
            case kModernSeqSpawner:
            case kModernObjDataAccumulator:
            case kModernDudeTargetChanger:
            case kModernEffectSpawner:
            case kModernWindGenerator:
                if (pXSprite->state == pXSprite->restState) break;
                evPostActor(actor, 0, kCmdRepeat);
                if (pXSprite->waitTime > 0)
                    evPostActor(actor, (pXSprite->waitTime * 120) / 10, pXSprite->restState ? kCmdOn : kCmdOff);
                break;
            #endif
            case kGenTrigger:
            case kGenDripWater:
            case kGenDripBlood:
            case kGenMissileFireball:
            case kGenDart:
            case kGenBubble:
            case kGenBubbleMulti:
            case kGenMissileEctoSkull:
            case kGenSound:
                InitGenerator(actor);
                break;
            case kThingArmedProxBomb:
                pXSprite->Proximity = 1;
                break;
            case kThingFallingRock:
                if (pXSprite->state) pSprite->flags |= 7;
                else pSprite->flags &= ~7;
                break;
            }
            if (pXSprite->Vector) pSprite->cstat |= CSTAT_SPRITE_BLOCK_HITSCAN;
            if (pXSprite->Push) pSprite->cstat |= CSTAT_SPRITE_BLOOD_BIT1;
        }
    }
    
    evSendGame(kChannelLevelStart, kCmdOn);
    switch (gGameOptions.nGameType) {
        case 1:
            evSendGame(kChannelLevelStartCoop, kCmdOn);
            break;
        case 2:
            evSendGame(kChannelLevelStartMatch, kCmdOn);
            break;
        case 3:
            evSendGame(kChannelLevelStartMatch, kCmdOn);
            evSendGame(kChannelLevelStartTeamsOnly, kCmdOn);
            break;
    }
}

void trTextOver(int nId)
{
    const char *pzMessage = currentLevel->GetMessage(nId);
    if (pzMessage)
        viewSetMessage(pzMessage, VanillaMode() ? 0 : 8, MESSAGE_PRIORITY_INI); // 8: gold
}

void InitGenerator(DBloodActor* actor)
{
    spritetype *pSprite = &actor->s();
    assert(actor->hasX());
    XSPRITE *pXSprite = &actor->x();
    switch (pSprite->type) {
        case kGenTrigger:
            pSprite->cstat &= ~CSTAT_SPRITE_BLOCK;
            pSprite->cstat |= CSTAT_SPRITE_INVISIBLE;
            break;
    }
    if (pXSprite->state != pXSprite->restState && pXSprite->busyTime > 0)
        evPostActor(actor, (120*(pXSprite->busyTime+Random2(pXSprite->data1)))/10, kCmdRepeat);
}

void ActivateGenerator(DBloodActor* actor)
{
    spritetype *pSprite = &actor->s();
    assert(actor->hasX());
    XSPRITE *pXSprite = &actor->x();
    switch (pSprite->type) {
        case kGenDripWater:
        case kGenDripBlood: {
            int top, bottom;
            GetActorExtents(actor, &top, &bottom);
            actSpawnThing(pSprite->sector(), pSprite->pos.X, pSprite->pos.Y, bottom, (pSprite->type == kGenDripWater) ? kThingDripWater : kThingDripBlood);
            break;
        }
        case kGenSound:
            sfxPlay3DSound(actor, pXSprite->data2, -1, 0);
            break;
        case kGenMissileFireball:
            switch (pXSprite->data2) {
                case 0:
                    FireballTrapSeqCallback(3, actor);
                    break;
                case 1:
                    seqSpawn(35, actor, nFireballTrapClient);
                    break;
                case 2:
                    seqSpawn(36, actor, nFireballTrapClient);
                    break;
            }
            break;
        case kGenMissileEctoSkull:
            break;
        case kGenBubble:
        case kGenBubbleMulti: {
            int top, bottom;
            GetActorExtents(actor, &top, &bottom);
            gFX.fxSpawnActor((pSprite->type == kGenBubble) ? FX_23 : FX_26, pSprite->sector(), pSprite->pos.X, pSprite->pos.Y, top, 0);
            break;
        }
    }
}

void FireballTrapSeqCallback(int, DBloodActor* actor)
{
    spritetype* pSprite = &actor->s();
    if (pSprite->cstat & CSTAT_SPRITE_ALIGNMENT_FLOOR)
        actFireMissile(actor, 0, 0, 0, 0, (pSprite->cstat & CSTAT_SPRITE_YFLIP) ? 0x4000 : -0x4000, kMissileFireball);
    else
        actFireMissile(actor, 0, 0, bcos(pSprite->ang), bsin(pSprite->ang), 0, kMissileFireball);
}


void MGunFireSeqCallback(int, DBloodActor* actor)
{
    XSPRITE* pXSprite = &actor->x();
    spritetype* pSprite = &actor->s();
    if (pXSprite->data2 > 0 || pXSprite->data1 == 0)
    {
        if (pXSprite->data2 > 0)
        {
            pXSprite->data2--;
            if (pXSprite->data2 == 0)
                evPostActor(actor, 1, kCmdOff);
        }
        int dx = bcos(pSprite->ang)+Random2(1000);
        int dy = bsin(pSprite->ang)+Random2(1000);
        int dz = Random2(1000);
        actFireVector(actor, 0, 0, dx, dy, dz, kVectorBullet);
        sfxPlay3DSound(actor, 359, -1, 0);
    }
}

void MGunOpenSeqCallback(int, DBloodActor* actor)
{
    seqSpawn(39, actor, nMGunFireClient);
}


//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

FSerializer& Serialize(FSerializer& arc, const char* keyname, BUSY& w, BUSY* def)
{
	if (arc.BeginObject(keyname))
	{
		arc("index", w.sect)
			("type", w.type)
			("delta", w.delta)
			("busy", w.busy)
			.EndObject();
	}
	return arc;
}

void SerializeTriggers(FSerializer& arc)
{
	if (arc.BeginObject("triggers"))
	{
		arc("busy", gBusy)
			.EndObject();
	}
}

END_BLD_NS
