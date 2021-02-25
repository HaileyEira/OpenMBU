//-----------------------------------------------------------------------------
// Torque Shader Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "game/marble/marble.h"

#include "audio/audioDataBlock.h"
#include "game/fx/particleEmitter.h"

//----------------------------------------------------------------------------

const float gMarbleCompressDists[7] = {5.0f, 11.0f, 23.0f, 47.0f, 96.0f, 195.0f, 500.0f};

Point3F gMarbleMotionDir = Point3F(0, 0, 0);

//----------------------------------------------------------------------------

IMPLEMENT_CO_NETOBJECT_V1(Marble);

U32 Marble::smEndPadId = 0;
SimObjectPtr<StaticShape> Marble::smEndPad = NULL;

Marble::Marble()
{
    mVertBuff = NULL;
    mPrimBuff = NULL;
    mRollHandle = NULL;
    mSlipHandle = NULL;
    mMegaHandle = NULL;

    mTrailEmitter = NULL;
    mMudEmitter = NULL;
    mGrassEmitter = NULL;

    mNetFlags.set(Ghostable); // TODO: original value was 0x10100, which shouldn't be valid?

    // TODO: HiFi
    mTypeMask |= PlayerObjectType; // | GameBaseHiFiObjectType;

    mDataBlock = NULL;
    mAnimateScale = true;

    delta.pos = Point3D(0, 0, 100);
    delta.posVec = Point3D(0, 0, 0);

    dMemcpy(&delta.move, &NullMove, sizeof(delta.move));

    mLastRenderPos = Point3F(0, 0, 0);
    mLastRenderVel = Point3F(0, 0, 0);

    mLastYaw = 0.0f;
    mBounceEmitDelay = 0;
    mRenderBlastPercent = 0.0f;
    
    mMarbleTime = 0;
    mMarbleBonusTime = 0;
    mFullMarbleTime = 0;
    mUseFullMarbleTime = false;

    mMovePathSize = 0;
    mBlastEnergy = 0;

    mObjToWorld.setColumn(3, Point3F(delta.pos.x, delta.pos.y, delta.pos.z));

    MatrixF mat(true);
    mat[0] = 1.0f;
    mat[4] = 0.0f;
    mat[8] = 0.0f;
    mat[1] = 0.0f;
    mat[5] = -1.0f;
    mat[10] = -1.0f;
    mat[9] = 0.0f;
    mat[2] = 0.0f;
    mat[6] = 0.0f;
    mGravityFrame.set(mat);
    mGravityRenderFrame = mGravityFrame;

    mVelocity = Point3D(0, 0, 0);
    delta.prevMouseX = 0.0f;
    delta.prevMouseY = 0.0f;
    mOmega = Point3D(0, 0, 0);
    mPosition = Point3D(0, 0, 0);

    mControllable = true;

    mLastContact.position = Point3D(1.0e10, 1.0e10, 1.0e10);
    mLastContact.normal = Point3D(0, 0, 1);
    mLastContact.actualNormal = Point3F(0, 0, 1);
    
    mBestContact.position = Point3D(1.0e10, 1.0e10, 1.0e10);
    mBestContact.surfaceVelocity = Point3D(0, 0, 0);
    mBestContact.normal = Point3D(0, 0, 1);
    mBestContact.actualNormal = Point3F(0, 0, 1);

    mCheckPointNumber = 0;
    mPadPtr = NULL;

    mMouseY = 0.0f;
    mMouseX = 0.0f;
    mOnPad = false;

    mShadowGenerated = false;
    mStencilMaterial = NULL;

    S32 i;
    for (i = 0; i < PowerUpData::MaxPowerUps; i++)
    {
        // TODO: ticksLeft and imageSlot might be flipped here
        mPowerUpState[i].active = false;
        mPowerUpState[i].ticksLeft = 0;
        mPowerUpState[i].emitter = NULL;
        mPowerUpState[i].imageSlot = -1;
    }

    mPowerUpId = 0;
    mPowerUpTimer = 0;
    mBlastTimer = 0;
    mMode = 9;
    mModeTimer = 0;
    mOOB = false;

    mEffect.lastCamFocus = Point3F(0, 0, 0);
    mEffect.effectTime = 0.0f;

    mCameraInit = false;
    mNetSmoothPos = Point3F(0, 0, 0);

    mRadsLeftToCenter = 0.0f;
    mCenteringCamera = false;

    mSinglePrecision.mPosition = mPosition;
    mSinglePrecision.mVelocity = mVelocity;
    mSinglePrecision.mOmega = mOmega;
}

Marble::~Marble()
{
    S32 i;
    for (i = 0; i < PowerUpData::MaxPowerUps; i++)
    {
        if (!mPowerUpState[i].emitter.isNull())
            mPowerUpState[i].emitter->deleteWhenEmpty();
    }

    if (mTrailEmitter)
        mTrailEmitter->deleteWhenEmpty();

    if (mMudEmitter)
        mTrailEmitter->deleteWhenEmpty();

    if (mGrassEmitter)
        mTrailEmitter->deleteWhenEmpty();

    delete mStencilMaterial;
}

void Marble::initPersistFields()
{
    Parent::initPersistFields();

    addField("Controllable", TypeBool, Offset(mControllable, Marble));
}

//----------------------------------------------------------------------------

Marble::Contact::Contact()
{
    // Empty
}

Marble::SinglePrecision::SinglePrecision()
{
    // Empty
}

Marble::StateDelta::StateDelta()
{
    // Empty
}

Marble::EndPadEffect::EndPadEffect()
{
    // Empty
}

Marble::PowerUpState::PowerUpState()
{
    emitter = NULL;
}

Marble::PowerUpState::~PowerUpState()
{
    // Empty
}

//----------------------------------------------------------------------------

SceneObject* Marble::getPad()
{
    return mPadPtr;
}

S32 Marble::getPowerUpId()
{
    return mPowerUpId;
}

const QuatF& Marble::getGravityFrame()
{
    return mGravityFrame;
}

U32 Marble::getMaxNaturalBlastEnergy()
{
    return mDataBlock->maxNaturalBlastRecharge >> 5;
}

U32 Marble::getMaxBlastEnergy()
{
    return mDataBlock->blastRechargeTime >> 5;
}

F32 Marble::getBlastPercent()
{
    return (F32)((F64)mBlastEnergy / (F64)(mDataBlock->maxNaturalBlastRecharge >> 5));
}

F32 Marble::getBlastEnergy() const
{
    return mBlastEnergy;
}

void Marble::setBlastEnergy(F32 energy)
{
    mBlastEnergy = (U32)energy;
}

void Marble::setUseFullMarbleTime(bool useFull)
{
    mUseFullMarbleTime = useFull;
    setMaskBits(OOBMask);
}

void Marble::setMarbleTime(U32 time)
{
    mFullMarbleTime = time;
    mMarbleTime = time;
}

U32 Marble::getMarbleTime()
{
    if (mUseFullMarbleTime)
        return mFullMarbleTime;
    else
        return mMarbleTime;
}

void Marble::setMarbleBonusTime(U32 time)
{
    mMarbleBonusTime = time;
}

U32 Marble::getMarbleBonusTime()
{
    return mMarbleBonusTime;
}

U32 Marble::getFullMarbleTime()
{
    return mFullMarbleTime;
}

Marble::Contact& Marble::getLastContact()
{
    return mLastContact;
}

void Marble::setGravityFrame(const QuatF& q, bool snap)
{
    mGravityFrame = q;
    setMaskBits(GravityMask);

    if (snap)
    {
        mGravityRenderFrame = q;
        setMaskBits(GravitySnapMask);
    }
}

void Marble::onSceneRemove()
{
    Parent::onSceneRemove();
}

void Marble::setPosition(const Point3D& pos, bool doWarp)
{
    MatrixF mat = mObjToWorld;
    mat[3] = pos.x;
    mat[7] = pos.y;
    mat[11] = pos.z;
    mPosition = pos;
    mSinglePrecision.mPosition = pos;

    Parent::setTransform(mat);

    setMaskBits(MoveMask);

    if (doWarp)
    {
        mLastRenderPos = pos;
        mLastRenderVel = mVelocity;
        mRenderScale = mObjScale;
        setMaskBits(WarpMask);
    }
}

void Marble::setPosition(const Point3D& pos, const AngAxisF& angAxis, float mouseY)
{
    MatrixF mat = mObjToWorld;
    mat[3] = pos.x;
    mat[7] = pos.y;
    mat[11] = pos.z;
    mPosition = pos;
    mSinglePrecision.mPosition = pos;

    Parent::setTransform(mat);

    angAxis.setMatrix(&mat);

    mMouseX = mAtan(mat[1], mat[5]);
    mMouseY = mouseY;
    mLastRenderPos = pos;
    mLastRenderVel = mVelocity;
    mRenderScale = mObjScale;

    setMaskBits(MoveMask | WarpMask);
}

void Marble::setTransform(const MatrixF& mat)
{
    setPosition(Point3F(mat[3], mat[7], mat[11]), true);
}

Point3F& Marble::getPosition()
{
    static Point3F position;
    position = Point3F(mObjToWorld[3], mObjToWorld[7], mObjToWorld[11]);
    return position;
}

void Marble::victorySequence()
{
    setVelocity(Point3F(0.0f, 0.0f, 0.1f));
}

void Marble::setMode(U32 mode)
{
    U32 newMode = mode & (mode ^ mMode);

    mMode = mode;

    if (newMode & StartingMode)
    {
        mModeTimer = mDataBlock->startModeTime >> 5;
    }
    else if (newMode & 0x60) // TODO: Figure out Mode Enum
    {
        if (newMode & StoppingMode)
            mMode = mode | 2; // TODO: Avoid using bitwise or here
        mModeTimer = mDataBlock->startModeTime >> 5;
    }

    setMaskBits(PowerUpMask);
}

void Marble::setOOB(bool isOOB)
{
    mOOB = isOOB;
    setMaskBits(OOBMask);
}

void Marble::interpolateTick(F32 delta)
{
    Parent::interpolateTick(delta);

    if (getControllingClient() && mMode & TimerMode)
    {
        U32 marbleTime = getMarbleTime();
        S32 newMarbleTime = marbleTime > 32 ? marbleTime - 32 : 0;
        U32 marbleBonusTime = mMarbleBonusTime;
        U32 finalMarbleTime = newMarbleTime + (U64)(((F64)marbleTime - newMarbleTime) * (1.0 - delta));
        if (marbleBonusTime && !mUseFullMarbleTime)
            finalMarbleTime = marbleTime;

        Con::evaluatef("PlayGui.updateTimer(%i,%i);", finalMarbleTime, marbleBonusTime != 0);
    }
}

S32 Marble::mountPowerupImage(ShapeBaseImageData*)
{
    // TODO: Implement mountPowerupImage
    return -1;
}

void Marble::updatePowerUpParams()
{
    mPowerUpParams.init();
    mPowerUpParams.sizeScale *= mDataBlock->size;
    mPowerUpParams.bounce = mDataBlock->bounceRestitution;
    mPowerUpParams.airAccel = mDataBlock->airAcceleration;

    // TODO: Implement updatePowerUpParams

    updateMass();
}

bool Marble::getForce(Point3F& pos, Point3F* force)
{
    // TODO: Implement getForce in GameBase and StaticShape

    // TODO: Implement getForce
    return false;
}

U32 Marble::packUpdate(NetConnection* conn, U32 mask, BitStream* stream)
{
    Parent::packUpdate(conn, mask, stream);

    bool isControl = false;

    // if it's the control object and the WarpMask is not set
    if (getControllingClient() == (GameConnection*)conn && !(mask & WarpMask))
        isControl = true;

    bool gravityChange = false;
    if (isControl || !(mask & GravityMask))
        gravityChange = mask & (GravitySnapMask | WarpMask);
    else
        gravityChange = true;

    stream->writeFlag(isControl);
    stream->writeFlag(gravityChange);

    if (stream->writeFlag(mask & PowerUpMask))
    {
        stream->writeRangedU32(mPowerUpId, 0, 9);

        S32 i;
        for (i = 0; i < PowerUpData::MaxPowerUps; i++)
        {
            if (stream->writeFlag(mPowerUpState[i].active))
            {
                stream->writeRangedU32(mPowerUpState[i].ticksLeft, 0, mDataBlock->powerUps->duration[i] >> 5); // TODO: is it supposed to be >> 5?
            }
        }

        if (stream->writeFlag((mMode & ActiveModeMask) != mMode))
            stream->writeInt(mMode, 7);
        else
            stream->writeInt(mMode, 4);

        if (stream->writeFlag(mModeTimer != 0))
            stream->writeRangedU32(mModeTimer, 1, 31);

        if (stream->writeFlag(mPowerUpTimer != 0))
            stream->writeRangedU32(mPowerUpTimer >> 5, 1, 16);

        if (stream->writeFlag(mBlastTimer != 0))
            stream->writeRangedU32(mBlastTimer >> 5, 1, 16);

        if (mPowerUpState[0].active || mBlastTimer)
            stream->writeRangedU32(mBlastEnergy, 0, mDataBlock->blastRechargeTime >> 5);
    }

    if (isControl || gravityChange)
    {
        stream->writeFlag(mUseFullMarbleTime);
        stream->writeFlag(mOOB);

        if (gravityChange)
        {
            stream->write(mGravityFrame.x);
            stream->write(mGravityFrame.y);
            stream->write(mGravityFrame.z);

            // TODO: There might be more here but it could just be a decompile error

            stream->writeFlag(mGravityFrame.w < 0.0f);
            stream->writeFlag((mask & (GravitySnapMask | WarpMask)) != 0);
        }
    }

    if (!isControl)
    {
        if (getControllingClient() == (GameConnection*)conn && (mask & WarpMask))
            mask |= MoveMask;

        if (stream->writeFlag(mask != 0))
        {
            stream->writeFlag(mask & WarpMask);
            stream->writeFloat((mMouseY - -0.35f) / 1.85f, 12);
            stream->writeFloat(mMouseX / 6.283185307179586f, 12);
            stream->writeSignedFloat(mLastYaw, 12);

            Point3D vel = mVelocity;
            Point3D omega = mOmega;
            Point3F pos(mObjToWorld[3], mObjToWorld[7], mObjToWorld[11]);

            double scale;
            if (gravityChange)
                scale = 0.0002499999981373548;
            else
                scale = 0.004999999888241291;

            // TODO: Implement BitStream::writeCompressedPointRP
            //stream->writeCompressedPointRP(pos, 7, gMarbleCompressDists, g);
            stream->writeCompressedPoint(pos, scale);

            float maxRollVelocity = mDataBlock->maxRollVelocity;
            stream->writeVector(Point3F(vel.x, vel.y, vel.z), 0.0099999998f, maxRollVelocity + maxRollVelocity, 16, 16, 10);
            stream->writeVector(Point3F(omega.x, omega.y, omega.z), 0.0099999998f, 10.0f, 16, 16, 10);

            delta.move.pack(stream);
        }
    }

    return 0;
}

void Marble::unpackUpdate(NetConnection* conn, BitStream* stream)
{
    Parent::unpackUpdate(conn, stream);

    bool warp = stream->readFlag();
    bool isGravWarp = stream->readFlag();

    if (stream->readFlag())
    {
        mPowerUpId = stream->readRangedU32(0, 9);

        S32 i;
        for (i = 0; i < PowerUpData::MaxPowerUps; i++)
        {
            mPowerUpState[i].active = stream->readFlag();
            if (mPowerUpState[i].active)
            {
                mPowerUpState[i].ticksLeft = stream->readRangedU32(0, mDataBlock->powerUps->duration[i] >> 5); // TODO: is it supposed to be >> 5?
            }
        }

        if (stream->readFlag())
            setMode(stream->readInt(7));
        else
            setMode(stream->readInt(4));

        if (stream->readFlag())
            mModeTimer = stream->readRangedU32(1, 31);
        else
            mModeTimer = 0;

        if (stream->readFlag())
            mPowerUpTimer = 32 * stream->readRangedU32(1, 16);
        else
            mPowerUpTimer = 0;

        if (stream->readFlag())
            mBlastTimer = 32 * stream->readRangedU32(1, 16);
        else
            mBlastTimer = 0;

        if (mPowerUpState[0].active || mBlastTimer)
            mBlastEnergy = stream->readRangedU32(0, mDataBlock->blastRechargeTime >> 5);

        updatePowerUpParams();
    }

    if (warp || isGravWarp)
    {
        mUseFullMarbleTime = stream->readFlag();
        mOOB = stream->readFlag();
    }

    bool doWarp = false;
    if (isGravWarp)
    {
        stream->read(&mGravityFrame.x);
        stream->read(&mGravityFrame.y);
        stream->read(&mGravityFrame.z);

        float wSq = 1.0f
                  - mGravityFrame.x * mGravityFrame.x
                  - mGravityFrame.y * mGravityFrame.y
                  - mGravityFrame.z * mGravityFrame.z;

        double w = 0.0f;
        if (wSq > 0.0f)
            w = sqrtf(wSq);
        mGravityFrame.w = w;

        if (stream->readFlag())
            mGravityFrame.w *= -1.0f;

        doWarp = stream->readFlag();

        if (doWarp)
        {
            mGravityRenderFrame.x = mGravityFrame.x;
            mGravityRenderFrame.y = mGravityFrame.y;
            mGravityRenderFrame.z = mGravityFrame.z;
            mGravityRenderFrame.w = mGravityFrame.w;
        }
    }

    if (!warp && stream->readFlag())
    {
        warp = stream->readFlag();

        mMouseY = stream->readFloat(12) * 1.85f - 0.35f;
        mMouseX = stream->readFloat(12) * 6.283185307179586;
        mLastYaw = stream->readSignedFloat(12);
        mCameraInit = false;

        float scale;
        if (isGravWarp)
            scale = 0.0002499999981373548;
        else
            scale = 0.004999999888241291;

        Point3F pos;

        // TODO: Implement BitStream::readCompressedPointRP
        //stream->readCompressedPointRP(&pos, 7, gMarbleCompressDists, scale);
        stream->readCompressedPoint(&pos, scale);

        Point3F vel;
        Point3F omega;
        double maxRollVelocity = mDataBlock->maxRollVelocity;
        stream->readVector(&vel, 0.0099999998, maxRollVelocity + maxRollVelocity, 16, 16, 10);
        stream->readVector(&omega, 0.0099999998, 10.0f, 16, 16, 10);

        mSinglePrecision.mVelocity = vel;
        mSinglePrecision.mOmega = omega;

        delta.move.unpack(stream);

        mOmega = mSinglePrecision.mOmega;
        mVelocity = mSinglePrecision.mVelocity;

        float nX = mPosition.x - pos.x;
        float nY = mPosition.y - pos.y;
        float nZ = mPosition.z - pos.z;

        if (gClientProcessList.getLastDelta() <= 0.001 || warp)
        {
            delta.posVec = Point3D(0, 0, 0);
        }
        else
        {
            float invBD = 1.0f / gClientProcessList.getLastDelta();
            float posX = nX * invBD;
            float posY = nY * invBD;
            float posZ = nZ * invBD;

            delta.posVec.x = posX + delta.posVec.x;
            delta.posVec.y = posY + delta.posVec.y;
            delta.posVec.z = posY + delta.posVec.z;
        }

        mMovePathSize = 0;
        delta.pos = pos;

        setPosition(pos, doWarp);
    }
}

U32 Marble::filterMaskBits(U32 mask, NetConnection* connection)
{
    // TODO: Implement filterMaskBits
    return Parent::filterMaskBits(mask, connection);
}

void Marble::writePacketData(GameConnection* conn, BitStream* stream)
{
    // TODO: Implement writePacketData
    Parent::writePacketData(conn, stream);
}

void Marble::readPacketData(GameConnection* conn, BitStream* stream)
{
    // TODO: Implement readPacketData
    Parent::readPacketData(conn, stream);
}

void Marble::renderShadowVolumes(SceneState*)
{
    // TODO: Implement renderShadowVolumes
}

void Marble::renderShadow(F32, F32)
{
    // TODO: Implement renderShadow
}

void Marble::renderImage(SceneState* state)
{
    // TODO: Implement renderImage
    Parent::renderImage(state);
}

void Marble::bounceEmitter(F32, const Point3F&)
{
    // TODO: Implement bounceEmitter
}

MatrixF Marble::getShadowTransform() const
{
    // TODO: Missing ShapeBase::getShadowTransform. Appears to be an unused function?
    //return Parent::getShadowTransform();

    // TODO: Implement getShadowTransform
    return MatrixF();
}

void Marble::setVelocity(const Point3F& vel)
{
    // TODO: Implement setVelocity
    Parent::setVelocity(vel);
}

Point3F Marble::getVelocity() const
{
    // TODO: Implement getVelocity
    return Parent::getVelocity();
}

Point3F Marble::getShadowScale() const
{
    // TODO: Missing SceneObject::getShadowScale and ShapeBase::getShadowScale. Appears to be an unused function?

    // TODO: Implement getShadowScale
    return Point3F();
}

Point3F Marble::getGravityRenderDir()
{
    Point3F ret;
    mGravityRenderFrame.mulP(Point3F(0.0f, 0.0f, -1.0f), &ret);
    return ret;
}

void Marble::getShadowLightVectorHack(Point3F& lightVec)
{
    // TODO: Missing SceneObject::getShadowLightVectorHack in parent. BlobShadow needs to be implemented.

    // TODO: Implement getShadowLightVectorHack
}

bool Marble::onSceneAdd(SceneGraph* graph)
{
    // TODO: Implement onSceneAdd
    return Parent::onSceneAdd(graph);
}

bool Marble::onNewDataBlock(GameBaseData* dptr)
{
    if (!Parent::onNewDataBlock(dptr))
        return false;

    this->mDataBlock = dynamic_cast<MarbleData*>(dptr);
    if (this->mDataBlock)
    {
        updatePowerUpParams();

        if (this->mDataBlock->shape)
        {
            updateMass();
            return true;
        }
    }

    return false;
}

void Marble::onRemove()
{
    removeFromScene();

    if (mRollHandle)
    {
        alxStop(mRollHandle);
        mRollHandle = NULL;
    }

    if (mSlipHandle)
    {
        alxStop(mSlipHandle);
        mSlipHandle = NULL;
    }

    if (mMegaHandle)
    {
        alxStop(mMegaHandle);
        mMegaHandle = NULL;
    }

    Parent::onRemove();
}

bool Marble::updatePadState()
{
    // TODO: Implement updatePadState
    return false;
}

void Marble::doPowerUpBoost(S32)
{
    // TODO: Implement doPowerUpBoost
}

void Marble::doPowerUpPower(S32)
{
    // TODO: Implement doPowerUpPower
}

void Marble::updatePowerups()
{
    // TODO: Implement updatePowerups
}

void Marble::updateMass()
{
    Parent::updateMass();

    if (!mDataBlock)
        return;

    mMass = mPowerUpParams.massScale * mMass;
    mOneOverMass = 1.0f / mMass;

    setScale(Point3F(mPowerUpParams.sizeScale, mPowerUpParams.sizeScale, mPowerUpParams.sizeScale));

    TSShape* shape = mDataBlock->shape;

    Box3F bounds = shape->bounds;
    Point3F extents(bounds.max.x + bounds.min.x, bounds.max.y + bounds.min.y, bounds.max.z + bounds.min.z);
    Point3F center = extents * 0.5f;
    Point3F scale(bounds.max.x - bounds.min.x, bounds.max.y - bounds.min.y, bounds.max.z - bounds.min.z);
    extents = scale * 0.5f;

    extents *= mPowerUpParams.sizeScale;

    mObjBox.min = center - extents;
    mObjBox.max = extents + center;

    mRadius = getMax(getMax(mObjBox.max.x, mObjBox.max.y), mObjBox.max.z);

    resetWorldBox();
}

void Marble::trailEmitter(U32)
{
    // TODO: Implement trailEmitter
}

void Marble::updateRollSound(F32, F32)
{
    // TODO: Implement updateRollSound
}

void Marble::playBounceSound(Marble::Contact&, F64)
{
    // TODO: Implement playBounceSound
}

void Marble::setPad(SceneObject* obj)
{
    mPadPtr = obj;
    if (obj)
        mOnPad = updatePadState();
    else
        mOnPad = false;
}

void Marble::findRenderPos(F32)
{
    // TODO: Implement findRenderPos
}

void Marble::advanceTime(F32 dt)
{
    // TODO: Implement advanceTime
    Parent::advanceTime(dt);
}

void Marble::computeNetSmooth(F32 backDelta)
{
    // TODO: GameBase::computeNetSmooth is missing. Need to implement hifi for this.

    // TODO: Implement computeNetSmooth
}

void Marble::doPowerUp(S32)
{
    // TODO: Implement doPowerUp
}

void Marble::prepShadows()
{
    // TODO: Implement prepShadows
}

bool Marble::onAdd()
{
    if (!Parent::onAdd())
        return false;

    if (isGhost())
    {
        mRollHandle = alxPlay(this->mDataBlock->sound[0], &getTransform(), &Point3F(0, 0, 0));
        mSlipHandle = alxPlay(this->mDataBlock->sound[3], &getTransform(), &Point3F(0, 0, 0));
        mMegaHandle = alxPlay(this->mDataBlock->sound[1], &getTransform(), &Point3F(0, 0, 0));

        this->mVertBuff.set(GFX, 33, GFXBufferTypeStatic);
        this->mPrimBuff.set(GFX, 33, 2, GFXBufferTypeStatic);
        prepShadows();
    }

    addToScene();

    return true;
}

void Marble::processMoveTriggers(const Move *)
{
    // TODO: Implement processMoveTriggers
}

void Marble::processItemsAndTriggers(const Point3F&, const Point3F&)
{
    // TODO: Implement processItemsAndTriggers
}

void Marble::setPowerUpId(U32, bool)
{
    // TODO: Implement setPowerUpId
}

void Marble::processTick(const Move* move)
{
    Parent::processTick(move);

    clearMarbleAxis();
    if (mMode & TimerMode)
    {
        U32 bonusTime = mMarbleBonusTime;
        if (bonusTime <= 32)
        {
            mMarbleBonusTime = 0;
            mMarbleTime += 32 - bonusTime;
            if (!(mMode & (MoveMode | StoppingMode)))
                setMode(StartingMode);
        }
        else
            mMarbleBonusTime = bonusTime - 32;

        mFullMarbleTime += 32;
    }

    if (mModeTimer)
    {
        mModeTimer--;
        if (!mModeTimer)
        {
            if (mMode & StartingMode)
                mMode &= ~(RestrictXYZMode | CameraHoverMode) | (MoveMode | TimerMode);
            if (mMode & StoppingMode)
                mMode &= ~MoveMode;
            if (mMode & FinishMode)
                mMode |= CameraHoverMode;
        }
    }

    if (mBlastEnergy < mDataBlock->maxNaturalBlastRecharge >> 5)
        mBlastEnergy++;

    const Move* newMove;
    if (mControllable)
    {
        if (move)
        {
            dMemcpy(&delta.move, move, sizeof(delta.move));
            newMove = move;  
        }
        else
            newMove = &delta.move;
    }
    else
        newMove = &NullMove;

    processMoveTriggers(newMove);
    processCameraMove(newMove);

    Point3F startPos(mPosition.x, mPosition.y, mPosition.z);

    advancePhysics(newMove, TickMs);

    Point3F endPos(mPosition.x, mPosition.y, mPosition.z);

    processItemsAndTriggers(startPos, endPos);
    updatePowerups();

    if (mPadPtr)
    {
        bool oldOnPad = mOnPad;
        updatePadState();
        if (oldOnPad != mOnPad && !isGhost())
        {
            const char* funcName = "onLeavePad";
            if (!oldOnPad)
                funcName = "onEnterPad";
            Con::executef(mDataBlock, 2, funcName, scriptThis());
        }
    }

    if (isGhost())
    {
        if (getControllingClient())
        {
            if (Marble::smEndPad.isNull() || Marble::smEndPad->getId() != Marble::smEndPadId)
            {
                if (Marble::smEndPadId && Marble::smEndPadId != -1)
                    Marble::smEndPad = dynamic_cast<StaticShape*>(Sim::findObject(Marble::smEndPadId));
                
                if (Marble::smEndPad.isNull())
                    Marble::smEndPadId = 0;
            }
        }
    }

    if (mOmega.len() < 0.000001)
        mOmega.set(0, 0, 0);

    if (!isGhost() && mOOB && newMove->trigger[2])
        Con::executef(this, 1, "onOOBClick");

    notifyCollision();

    mSinglePrecision.mPosition = mPosition;
    mSinglePrecision.mVelocity = mVelocity;
    mSinglePrecision.mOmega = mOmega;

    mPosition = mSinglePrecision.mPosition;
    mVelocity = mSinglePrecision.mVelocity;
    mOmega = mSinglePrecision.mOmega;
}

//----------------------------------------------------------------------------

ConsoleMethod(Marble, setPosition, void, 4, 4, "(transform, mouseY)")
{
    Point3F posf;
    AngAxisF angAxis;
    dSscanf(argv[2], "%f %f %f %f %f %f %f", &posf.x, &posf.y, &posf.z, &angAxis.axis.x, &angAxis.axis.y, &angAxis.axis.z, &angAxis.angle);

    object->setPosition(Point3D(posf.x, posf.y, posf.z), angAxis, dAtof(argv[3]));
}

ConsoleMethod(Marble, setGravityDir, void, 3, 4, "(gravity, snap)")
{
    Point3F xvec;
    Point3F yvec;
    Point3F zvec;
    dSscanf(argv[2], "%g %g %g %g %g %g %g %g %g", &xvec.x, &xvec.y, &xvec.z, &yvec.x, &yvec.y, &yvec.z, &zvec.x, &zvec.y, &zvec.z);

    MatrixF mat1(true);
    MatrixF mat2(true);
    object->getGravityFrame().setMatrix(&mat1);

    bool snap = false;
    if (argc == 4 && dAtob(argv[3]))
    {
        snap = true;
        mat1.identity();
    }

    mat2.setColumn(0, xvec);
    mat2.setColumn(1, -yvec);
    mat2.setColumn(2, -zvec);

    Point3F sideDir(mat1[2], mat1[6], mat1[10]);
    Point3F upDir = -zvec;
    Point3F rot;
    mCross(sideDir, upDir, &rot);
    
    float len = rot.len();
    if (len <= 0.1000000014901161f)
    {
        sideDir.x = mat1[0];
        sideDir.y = mat1[4];
        sideDir.z = mat1[8];
        mat2[0] = mat1[0];
        mat2[4] = mat1[4];
        mat2[8] = mat1[8];
        upDir.x = mat2[2];
        upDir.y = mat2[6];
        upDir.z = mat2[10];

        Point3F res;
        mCross(upDir, sideDir, &res);
        mat2.setColumn(1, res);
    }
    else
    {
        if (len > 1.0f)
            len = 1.0f;
        if (len < -1.0f)
            len = -1.0f;
        m_point3F_normalize_f(rot, asinf(len));

        dMemcpy(&mat2, &mat1, sizeof(mat2));

        rotateMatrix(mat2, rot);
        mat2.setColumn(2, -zvec);
    }

    QuatF grav(mat2);
    grav.normalize();

    object->setGravityFrame(grav, snap);
}

ConsoleMethod(Marble, setMode, void, 3, 3, "(mode)")
{
    U32 modeFlags[5];
    const char* modesStrings[5];
    S32 newMode = object->getMode() & 3;

    modesStrings[0] = "Normal";
    modeFlags[0] = Marble::StartingMode;

    modesStrings[1] = "Victory";
    modeFlags[1] = Marble::StoppingMode;

    modesStrings[2] = "Lost";
    modeFlags[2] = Marble::StoppingMode;

    modesStrings[3] = "Freeze";
    modeFlags[3] = Marble::TimerMode | Marble::StoppingMode;

    modesStrings[4] = "Start";
    modeFlags[4] = Marble::MoveMode | Marble::RestrictXYZMode;

    S32 i = 0;
    while (dStricmp(modesStrings[i], argv[2]))
    {
        if (++i >= 5)
        {
            Con::errorf("Marble:: Unknown marble mode: %s", argv[2]);
            return;
        }
    }

    object->setMode(newMode | modeFlags[i]);
}

ConsoleMethod(Marble, setPad, void, 3, 3, "(pad)")
{
    U32 padId = dAtoi(argv[2]);
    if (!padId)
    {
        object->setPad(0);
        return;
    }

    SceneObject* pad;
    if (Sim::findObject(padId, pad))
        object->setPad(pad);
    else
        Con::errorf("Marble::setPad: Not a SceneObject");
}

ConsoleMethod(Marble, setMarbleTime, void, 3, 3, "(time)")
{
    object->setMarbleTime(dAtoi(argv[2]));
}

ConsoleMethod(Marble, setMarbleBonusTime, void, 3, 3, "(time)")
{
    object->setMarbleBonusTime(dAtoi(argv[2]));
}

ConsoleMethod(Marble, setUseFullMarbleTime, void, 3, 3, "(flag)")
{
    object->setUseFullMarbleTime(dAtob(argv[2]));
}

ConsoleMethod(Marble, getMarbleTime, S32, 2, 2, "()")
{
    return object->getMarbleTime();
}

ConsoleMethod(Marble, getMarbleBonusTime, S32, 2, 2, "()")
{
    return object->getMarbleBonusTime();
}

//----------------------------------------------------------------------------

IMPLEMENT_CO_DATABLOCK_V1(MarbleData);

MarbleData::MarbleData()
{
    minVelocityBounceSoft = 2.5f;
    minVelocityBounceHard = 12.0f;
    bounceMinGain = 0.2f;
    minVelocityMegaBounceSoft = 2.5f;
    maxJumpTicks = 3;
    bounceEmitter = NULL;
    minVelocityMegaBounceHard = 8.0f;
    trailEmitter = NULL;
    mudEmitter = NULL;
    bounceMegaMinGain = 0.5f;
    grassEmitter = NULL;
    maxRollVelocity = 25.0f;
    angularAcceleration = 18.0f;
    brakingAcceleration = 8.0f;
    gravity = 20.0f;
    size = 1.0f;
    megaSize = 2.0f;
    staticFriction = 1.0f;
    kineticFriction = 0.89999998f;
    bounceKineticFriction = 0.2f;
    maxDotSlide = 0.1f;
    bounceRestitution = 0.89999998f;
    airAcceleration = 5.0f;
    energyRechargeRate = 1.0f;
    jumpImpulse = 1.0f;
    maxForceRadius = 1.0f;
    cameraDistance = 2.5f;
    minBounceVel = 0.1f;
    minTrailSpeed = 10.0f;
    minBounceSpeed = 1.0f;
    memset(sound, 0, sizeof(sound));
    genericShadowLevel = 0.40000001f;
    noShadowLevel = 0.0099999998f;
    blastRechargeTime = 1;
    maxNaturalBlastRecharge = 1;
    powerUps = NULL;
    mDecalData = NULL;
    mDecalID = 0;
    startModeTime = 320;
    stopModeTime = 32;
}

void MarbleData::initPersistFields()
{
    addField("maxRollVelocity", TypeF32, Offset(maxRollVelocity, MarbleData));
    addField("angularAcceleration", TypeF32, Offset(angularAcceleration, MarbleData));
    addField("brakingAcceleration", TypeF32, Offset(brakingAcceleration, MarbleData));
    addField("staticFriction", TypeF32, Offset(staticFriction, MarbleData));
    addField("kineticFriction", TypeF32, Offset(kineticFriction, MarbleData));
    addField("bounceKineticFriction", TypeF32, Offset(bounceKineticFriction, MarbleData));
    addField("gravity", TypeF32, Offset(gravity, MarbleData));
    addField("size", TypeF32, Offset(size, MarbleData));
    addField("megaSize", TypeF32, Offset(megaSize, MarbleData));
    addField("maxDotSlide", TypeF32, Offset(maxDotSlide, MarbleData));
    addField("bounceRestitution", TypeF32, Offset(bounceRestitution, MarbleData));
    addField("airAcceleration", TypeF32, Offset(airAcceleration, MarbleData));
    addField("energyRechargeRate", TypeF32, Offset(energyRechargeRate, MarbleData));
    addField("jumpImpulse", TypeF32, Offset(jumpImpulse, MarbleData));
    addField("maxForceRadius", TypeF32, Offset(maxForceRadius, MarbleData));
    addField("cameraDistance", TypeF32, Offset(cameraDistance, MarbleData));
    addField("minBounceVel", TypeF32, Offset(minBounceVel, MarbleData));
    addField("minTrailSpeed", TypeF32, Offset(minTrailSpeed, MarbleData));
    addField("minBounceSpeed", TypeF32, Offset(minBounceSpeed, MarbleData));
    addField("bounceEmitter", TypeParticleEmitterDataPtr, Offset(bounceEmitter, MarbleData));
    addField("trailEmitter", TypeParticleEmitterDataPtr, Offset(trailEmitter, MarbleData));
    addField("mudEmitter", TypeParticleEmitterDataPtr, Offset(mudEmitter, MarbleData));
    addField("grassEmitter", TypeParticleEmitterDataPtr, Offset(grassEmitter, MarbleData));
    addField("powerUps", TypeGameBaseDataPtr, Offset(powerUps, MarbleData));
    addField("blastRechargeTime", TypeS32, Offset(blastRechargeTime, MarbleData));
    addField("maxNaturalBlastRecharge", TypeS32, Offset(maxNaturalBlastRecharge, MarbleData));
    addField("RollHardSound", TypeAudioProfilePtr, Offset(sound[0], MarbleData));
    addField("RollMegaSound", TypeAudioProfilePtr, Offset(sound[1], MarbleData));
    addField("RollIceSound", TypeAudioProfilePtr, Offset(sound[2], MarbleData));
    addField("SlipSound", TypeAudioProfilePtr, Offset(sound[3], MarbleData));
    addField("Bounce1", TypeAudioProfilePtr, Offset(sound[4], MarbleData));
    addField("Bounce2", TypeAudioProfilePtr, Offset(sound[5], MarbleData));
    addField("Bounce3", TypeAudioProfilePtr, Offset(sound[6], MarbleData));
    addField("Bounce4", TypeAudioProfilePtr, Offset(sound[7], MarbleData));
    addField("MegaBounce1", TypeAudioProfilePtr, Offset(sound[8], MarbleData));
    addField("MegaBounce2", TypeAudioProfilePtr, Offset(sound[9], MarbleData));
    addField("MegaBounce3", TypeAudioProfilePtr, Offset(sound[10], MarbleData));
    addField("MegaBounce4", TypeAudioProfilePtr, Offset(sound[11], MarbleData));
    addField("JumpSound", TypeAudioProfilePtr, Offset(sound[12], MarbleData));
    Con::addVariable("Game::endPad", TypeS32, &Marble::smEndPadId);

    Parent::initPersistFields();
}

//----------------------------------------------------------------------------

bool MarbleData::preload(bool server, char errorBuffer[256])
{
    if (!Parent::preload(server, errorBuffer))
        return false;

    if (!server)
    {
        if (!mDecalData && mDecalID != 0)
            if (!Sim::findObject(mDecalID, mDecalData))
                Con::errorf(ConsoleLogEntry::General, "MarbleData::preload Invalid packet, bad datablockId(decalData): 0x%x", mDecalID);
    }

    return true;
}

void MarbleData::packData(BitStream* stream)
{
    stream->write(maxRollVelocity);
    stream->write(angularAcceleration);
    stream->write(brakingAcceleration);
    stream->write(gravity);
    stream->write(size);
    stream->write(staticFriction);
    stream->write(kineticFriction);
    stream->write(bounceKineticFriction);
    stream->write(maxDotSlide);
    stream->write(bounceRestitution);
    stream->write(airAcceleration);
    stream->write(energyRechargeRate);
    stream->write(jumpImpulse);
    stream->write(maxForceRadius);
    stream->write(cameraDistance);
    stream->write(minBounceVel);
    stream->write(minTrailSpeed);
    stream->write(minBounceSpeed);
    stream->write(blastRechargeTime);
    stream->write(maxNaturalBlastRecharge);

    if (stream->writeFlag(bounceEmitter != NULL))
        stream->writeRangedU32(bounceEmitter->getId(), DataBlockObjectIdFirst, DataBlockObjectIdLast);
    if (stream->writeFlag(trailEmitter != NULL))
        stream->writeRangedU32(trailEmitter->getId(), DataBlockObjectIdFirst, DataBlockObjectIdLast);
    if (stream->writeFlag(mudEmitter != NULL))
        stream->writeRangedU32(mudEmitter->getId(), DataBlockObjectIdFirst, DataBlockObjectIdLast);
    if (stream->writeFlag(grassEmitter != NULL))
        stream->writeRangedU32(grassEmitter->getId(), DataBlockObjectIdFirst, DataBlockObjectIdLast);
    if (stream->writeFlag(powerUps != NULL))
        stream->writeRangedU32(powerUps->getId(), DataBlockObjectIdFirst, DataBlockObjectIdLast);


    S32 i;
    for (i = 0; i < MaxSounds; i++)
        if (stream->writeFlag(sound[i]))
            stream->writeRangedU32(packed ? SimObjectId(sound[i]) :
                sound[i]->getId(), DataBlockObjectIdFirst, DataBlockObjectIdLast);

    if (stream->writeFlag(mDecalData != NULL))
        stream->writeRangedU32(mDecalData->getId(), DataBlockObjectIdFirst, DataBlockObjectIdLast);

    Parent::packData(stream);
}

void MarbleData::unpackData(BitStream* stream)
{
    stream->read(&maxRollVelocity);
    stream->read(&angularAcceleration);
    stream->read(&brakingAcceleration);
    stream->read(&gravity);
    stream->read(&size);
    stream->read(&staticFriction);
    stream->read(&kineticFriction);
    stream->read(&bounceKineticFriction);
    stream->read(&maxDotSlide);
    stream->read(&bounceRestitution);
    stream->read(&airAcceleration);
    stream->read(&energyRechargeRate);
    stream->read(&jumpImpulse);
    stream->read(&maxForceRadius);
    stream->read(&cameraDistance);
    stream->read(&minBounceVel);
    stream->read(&minTrailSpeed);
    stream->read(&minBounceSpeed);
    stream->read(&blastRechargeTime);
    stream->read(&maxNaturalBlastRecharge);

    if (stream->readFlag())
        Sim::findObject(stream->readRangedU32(DataBlockObjectIdFirst, DataBlockObjectIdLast), bounceEmitter);
    if (stream->readFlag())
        Sim::findObject(stream->readRangedU32(DataBlockObjectIdFirst, DataBlockObjectIdLast), trailEmitter);
    if (stream->readFlag())
        Sim::findObject(stream->readRangedU32(DataBlockObjectIdFirst, DataBlockObjectIdLast), mudEmitter);
    if (stream->readFlag())
        Sim::findObject(stream->readRangedU32(DataBlockObjectIdFirst, DataBlockObjectIdLast), grassEmitter);
    if (stream->readFlag())
        Sim::findObject(stream->readRangedU32(DataBlockObjectIdFirst, DataBlockObjectIdLast), powerUps);

    S32 i;
    for (i = 0; i < MaxSounds; i++) {
        sound[i] = NULL;
        if (stream->readFlag())
            sound[i] = (AudioProfile*)stream->readRangedU32(DataBlockObjectIdFirst,
                DataBlockObjectIdLast);
    }

    if (stream->readFlag())
        mDecalID = stream->readRangedU32(DataBlockObjectIdFirst, DataBlockObjectIdLast);

    Parent::unpackData(stream);
}

//----------------------------------------------------------------------------
