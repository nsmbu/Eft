#include <nw/eft/eft_AnimKeyFrame.h>
#include <nw/eft/eft_EmitterSet.h>
#include <nw/eft/eft_FrameRate.h>
#include <nw/eft/eft_EmitterSimple.h>
#include <nw/eft/eft_Renderer.h>
#include <nw/eft/eft_System.h>
#include <nw/eft/eft_UniformBlock.h>

#include <algorithm>
#include <cmath>

namespace nw { namespace eft {

void EmitterSimpleCalc::ApplyAnim(EmitterInstance* e)
{
    const SimpleEmitterData* __restrict res = e->res;
    KeyFrameAnimArray* keyAnimArray = e->emitterAnimArray;

    e->animEmitterRT .SetIdentity();
    e->animEmitterSRT.SetIdentity();

    e->animEmitterRT .SetMult(e->animEmitterRT,  res->transformRT);
    e->animEmitterSRT.SetMult(e->animEmitterSRT, res->transformSRT);

    if (keyAnimArray == NULL)
    {
        nw::math::VEC3 scale;
        nw::math::VEC3 rotat;
        nw::math::VEC3 trans;
        nw::math::VEC3Add(&scale, &res->scale, &e->scaleRnd);
        nw::math::VEC3Add(&rotat, &res->rot,   &e->rotatRnd);
        nw::math::VEC3Add(&trans, &res->trans, &e->transRnd);
        nw::math::MTX34MakeSRT(&e->animEmitterSRT, scale, rotat, trans);
        nw::math::MTX34MakeSRT(&e->animEmitterRT, nw::math::VEC3(1.0f, 1.0f, 1.0f), rotat, trans);
    }
    else
    {
        KeyFrameAnim* info = reinterpret_cast<KeyFrameAnim*>(keyAnimArray + 1);
        for (u32 i = 0; i < keyAnimArray->numAnims; ++i)
        {
            if (i != 0)
                info = reinterpret_cast<KeyFrameAnim*>((uintptr_t)info + info->offset);

            e->emitAnimValue[info->target] = CalcAnimKeyFrame(info, e->cnt);
        }

        nw::math::VEC3 scale(e->emitAnimValue[EFT_ANIM_EM_SX], e->emitAnimValue[EFT_ANIM_EM_SY], e->emitAnimValue[EFT_ANIM_EM_SZ]);
        nw::math::VEC3 rotat(e->emitAnimValue[EFT_ANIM_EM_RX], e->emitAnimValue[EFT_ANIM_EM_RY], e->emitAnimValue[EFT_ANIM_EM_RZ]);
        nw::math::VEC3 trans(e->emitAnimValue[EFT_ANIM_EM_TX], e->emitAnimValue[EFT_ANIM_EM_TY], e->emitAnimValue[EFT_ANIM_EM_TZ]);
        nw::math::MTX34MakeSRT(&e->animEmitterSRT, scale, rotat, trans);
        nw::math::MTX34MakeSRT(&e->animEmitterRT, nw::math::VEC3(1.0f, 1.0f, 1.0f), rotat, trans);
    }
}

void EmitterSimpleCalc::ApplyTransformAnim(EmitterInstance* e)
{
    EmitterSet* set = e->emitterSet;

    e->emitterRT .SetMult(set->GetRTMatrix(),  e->animEmitterRT);
    e->emitterSRT.SetMult(set->GetSRTMatrix(), e->animEmitterSRT);
}

void EmitterSimpleCalc::EmitSameDistance(const SimpleEmitterData* __restrict res, EmitterInstance* e)
{
    if (!e->emitDistPrevPosSet)
    {
        mEmitFunctions[res->volumeType](e);
    }
    else
    {
        nw::math::VEC3 prevPos(e->emitDistPrevPos.x, e->emitDistPrevPos.y, e->emitDistPrevPos.z);
        nw::math::VEC3 currPos(e->emitterRT._03, e->emitterRT._13, e->emitterRT._23);
        nw::math::VEC3 move(prevPos - currPos);

        f32 length = move.Length();

        f32 vessel = e->emitDistVessel;

        f32 virtualLength = length;
        if (virtualLength < res->emitDistMargin)
            virtualLength = 0.0f;

        const f32 minLength = res->emitDistMin * e->frameRate;
        const f32 maxLength = res->emitDistMax * e->frameRate;
        if (virtualLength == 0.0f)
            virtualLength = minLength;
        else if (virtualLength < minLength)
            virtualLength = minLength;
        else if (maxLength < virtualLength)
            virtualLength = maxLength;

        vessel += virtualLength;
        s32 count = (s32)(vessel / res->emitDistUnit); // No division-by-zero check

        for (s32 i = 0; i < count; i++)
        {
            vessel -= res->emitDistUnit;

            f32 ratio = 0.0f;
            if (virtualLength != 0.0f)
                ratio = vessel / virtualLength;

            nw::math::VEC3 pos;
            pos = currPos * (1.0f - ratio) + prevPos * ratio;

            e->emitterRT._03  = pos.x;
            e->emitterRT._13  = pos.y;
            e->emitterRT._23  = pos.z;
            e->emitterSRT._03 = pos.x;
            e->emitterSRT._13 = pos.y;
            e->emitterSRT._23 = pos.z;

            mEmitFunctions[res->volumeType](e);

            // No idea why this is done inside the loop and not after it
            e->emitterRT._03  = currPos.x;
            e->emitterRT._13  = currPos.y;
            e->emitterRT._23  = currPos.z;
            e->emitterSRT._03 = currPos.x;
            e->emitterSRT._13 = currPos.y;
            e->emitterSRT._23 = currPos.z;
        }

        e->emitDistVessel = vessel;
    }
}

void EmitterSimpleCalc::CalcEmitter(EmitterInstance* e)
{
    const SimpleEmitterData* __restrict res = e->res;
    const EmitterSet       * __restrict set = e->emitterSet;

    bool isParticleEmit;

    if (set->IsFadeRequest())
    {
        isParticleEmit = !res->isStopEmitInFade;

        e->fadeAlpha -= res->alphaAddInFade * e->frameRate;
        if (e->fadeAlpha <= 0.0f)
            return mSys->KillEmitter(e);
    }
    else
    {
        isParticleEmit = true;

        e->fadeAlpha += res->alphaAddInFade * e->frameRate;
        if (e->fadeAlpha > 1.0f)
            e->fadeAlpha = 1.0f;
    }

    ApplyAnim(e);
    ApplyTransformAnim(e);

    f32 localCnt = e->cnt - set->mStartFrame;

    if (localCnt < res->endFrame && isParticleEmit)
    {
        if (!res->emitDistEnabled)
        {
            _forEachAuthoredFrame(e->cnt, e->frameRate, [&](s32 tick, f32 frameOffset)
            {
                const f32 tickLocalCnt = static_cast<f32>(tick) - set->mStartFrame;
                if (tickLocalCnt < static_cast<f32>(res->startFrame) ||
                    tickLocalCnt >= static_cast<f32>(res->endFrame))
                {
                    return;
                }

                const f32 interval = e->emissionInterval * e->controller->mEmissionInterval;

                auto emitAt = [&]()
                {
                    PtclInstance* const previousHead = e->ptclHead;
                    const f32 updateCnt = e->cnt;
                    e->cnt = static_cast<f32>(tick);
                    ApplyAnim(e);
                    ApplyTransformAnim(e);

                    if (set->mNumParticleEmissionPoint > 0)
                    {
                        for (s32 j = 0; j < set->mNumParticleEmissionPoint; j++)
                        {
                            PtclInstance* saveHead = e->ptclHead;

                            mEmitFunctions[res->volumeType](e);

                            for (PtclInstance* ptcl = e->ptclHead; ptcl != saveHead; ptcl = ptcl->next)
                            {
                                ptcl->pos.x += set->mParticleEmissionPoints[j].x;
                                ptcl->pos.y += set->mParticleEmissionPoints[j].y;
                                ptcl->pos.z += set->mParticleEmissionPoints[j].z;
                            }
                        }
                    }
                    else
                    {
                        mEmitFunctions[res->volumeType](e);
                    }

                    e->cnt = updateCnt;
                    ApplyAnim(e);
                    ApplyTransformAnim(e);

                    for (PtclInstance* ptcl = e->ptclHead;
                         ptcl != previousHead;
                         ptcl = ptcl->next)
                    {
                        ptcl->cnt = -frameOffset;
                    }
                };

                bool emit = !e->isEmitted;
                if (static_cast<s32>(interval) != 0)
                {
                    if (static_cast<s32>(e->emitCnt) >= interval)
                    {
                        emit = true;
                        e->emitCnt = 0.0f;
                    }
                    else
                    {
                        e->emitCnt += 1.0f;
                    }
                }
                else
                {
                    emit = true;
                    e->emitCnt = 0.0f;
                }

                if (emit)
                    emitAt();

                e->emissionInterval = res->lifeStep + e->rnd.GetS32(res->lifeStepRnd);
            });
        }
        else if (localCnt >= res->startFrame)
        {
            EmitSameDistance(res, e);
        }
    }
    else
    {
        if (e->ptclHead == NULL && e->childHead == NULL)
            return mSys->KillEmitter(e);
    }

    e->preCnt = e->cnt;
    e->cnt   += e->frameRate;

    if (e->ptclHead != NULL)
    {
        e->emitDistPrevPosSet = true;
        e->emitDistPrevPos.Set(e->emitterRT._03, e->emitterRT._13, e->emitterRT._23);
    }
}

u32 EmitterSimpleCalc::CalcParticle(EmitterInstance* emitter, CpuCore core, bool skipBehavior, bool skipMakeAttribute)
{
    System* eftSystem = emitter->emitterSet->mSystem;

    if (!skipMakeAttribute)
    {
        emitter->ptclAttributeBuffer = static_cast<PtclAttributeBuffer*>(eftSystem->GetRenderer(core)->AllocFromDoubleBuffer(sizeof(PtclAttributeBuffer) * emitter->ptclNum));
        if (emitter->ptclAttributeBuffer == NULL)
            return 0;

        emitter->emitterDynamicUniformBlock = static_cast<EmitterDynamicUniformBlock*>(eftSystem->GetRenderer(core)->AllocFromDoubleBuffer(sizeof(EmitterDynamicUniformBlock)));
        if (emitter->emitterDynamicUniformBlock == NULL)
        {
            emitter->ptclAttributeBuffer = NULL;
            return 0;
        }

        nw::ut::FloatColor setColor = emitter->emitterSet->GetColor();
        setColor.r *= emitter->res->colorScale;
        setColor.g *= emitter->res->colorScale;
        setColor.b *= emitter->res->colorScale;

        emitter->emitterDynamicUniformBlock->emitterColor0.x = setColor.r * emitter->emitAnimValue[EFT_ANIM_COLOR0_R];
        emitter->emitterDynamicUniformBlock->emitterColor0.y = setColor.g * emitter->emitAnimValue[EFT_ANIM_COLOR0_G];
        emitter->emitterDynamicUniformBlock->emitterColor0.z = setColor.b * emitter->emitAnimValue[EFT_ANIM_COLOR0_B];
        emitter->emitterDynamicUniformBlock->emitterColor0.w = setColor.a * emitter->emitAnimValue[EFT_ANIM_ALPHA] * emitter->fadeAlpha;

        emitter->emitterDynamicUniformBlock->emitterColor1.x = setColor.r * emitter->emitAnimValue[EFT_ANIM_COLOR1_R];
        emitter->emitterDynamicUniformBlock->emitterColor1.y = setColor.g * emitter->emitAnimValue[EFT_ANIM_COLOR1_G];
        emitter->emitterDynamicUniformBlock->emitterColor1.z = setColor.b * emitter->emitAnimValue[EFT_ANIM_COLOR1_B];
        emitter->emitterDynamicUniformBlock->emitterColor1.w = setColor.a * emitter->emitAnimValue[EFT_ANIM_ALPHA] * emitter->fadeAlpha;

#if EFT_IS_CAFE
        GX2EndianSwap(emitter->emitterDynamicUniformBlock, sizeof(EmitterDynamicUniformBlock));
#endif // EFT_IS_CAFE
    }
    else
    {
        emitter->ptclAttributeBuffer = NULL;
        emitter->emitterDynamicUniformBlock = NULL;
    }

    emitter->entryNum = 0;

    PtclInstance* ptcl         = emitter->ptclHead;
    bool          reverseOrder = false;

    UserDataParticleCalcCallback particleCB = mSys->GetCurrentUserDataParticleCalcCallback(emitter);
    UserDataParticleMakeAttributeCallback particleMakeAttrCB = mSys->GetCurrentUserDataParticleMakeAttributeCallback(emitter);
    ParticleCalcArg arg;

    if (emitter->res->flg & EFT_EMITTER_FLAG_REVERSE_ORDER_PARTICLE)
    {
        reverseOrder = true;
        ptcl = emitter->ptclTail;
    }

    while (ptcl)
    {
        if (ptcl->res)
        {
            if (!skipBehavior)
            {
                const f32 emitterFrameRate = emitter->frameRate;
                f32 particleFrameRate = emitterFrameRate;
                if (ptcl->cnt < 0.0f)
                {
                    particleFrameRate = std::max(0.0f, emitterFrameRate + ptcl->cnt);
                    ptcl->cnt = 0.0f;
                }

                const f32 remainingLife = static_cast<f32>(ptcl->life) - ptcl->cnt;
                if (remainingLife <= 0.0f)
                {
                    RemoveParticle(emitter, ptcl, core);
                    goto next;
                }

                const f32 liveFrameRate = std::min(particleFrameRate, remainingLife);
                emitter->frameRate = liveFrameRate;
                CalcSimpleParticleBehavior(emitter, ptcl, core);
                emitter->frameRate = emitterFrameRate;

                if (particleFrameRate > liveFrameRate)
                {
                    RemoveParticle(emitter, ptcl, core);
                    goto next;
                }
            }

            if (particleCB != NULL)
            {
                arg.emitter = emitter;
                arg.ptcl = ptcl;
                arg.core = core;
                arg.skipBehavior = skipBehavior;
                particleCB(arg);
            }

            if (!skipMakeAttribute)
            {
                MakeParticleAttributeBuffer(&emitter->ptclAttributeBuffer[emitter->entryNum], ptcl, emitter->shaderArrtFlag, emitter->res->toCameraOffset);
                ptcl->attributeBuffer     = &emitter->ptclAttributeBuffer[emitter->entryNum];
                emitter->entryNum++;

                if (particleMakeAttrCB != NULL)
                {
                    arg.emitter = emitter;
                    arg.ptcl = ptcl;
                    arg.core = core;
                    arg.skipBehavior = skipBehavior;
                    particleMakeAttrCB(arg);
                }
            }
        }
next:
        ptcl = reverseOrder ? ptcl->prev : ptcl->next;
    }

    emitter->isCalculated = true;
    return emitter->entryNum;
}

} } // namespace nw::eft
