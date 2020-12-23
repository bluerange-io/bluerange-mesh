////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2021 M-Way Solutions GmbH
// ** Contact: https://www.blureange.io/licensing
// **
// ** This file is part of the Bluerange/FruityMesh implementation
// **
// ** $BR_BEGIN_LICENSE:GPL-EXCEPT$
// ** Commercial License Usage
// ** Licensees holding valid commercial Bluerange licenses may use this file in
// ** accordance with the commercial license agreement provided with the
// ** Software or, alternatively, in accordance with the terms contained in
// ** a written agreement between them and M-Way Solutions GmbH. 
// ** For licensing terms and conditions see https://www.bluerange.io/terms-conditions. For further
// ** information use the contact form at https://www.bluerange.io/contact.
// **
// ** GNU General Public License Usage
// ** Alternatively, this file may be used under the terms of the GNU
// ** General Public License version 3 as published by the Free Software
// ** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
// ** included in the packaging of this file. Please review the following
// ** information to ensure the GNU General Public License requirements will
// ** be met: https://www.gnu.org/licenses/gpl-3.0.html.
// **
// ** $BR_END_LICENSE$
// **
// ****************************************************************************/
////////////////////////////////////////////////////////////////////////////////
#include "MoveAnimation.h"
#include "Exceptions.h"
#include "CherrySim.h"

ThreeDimStruct<float> MoveAnimationKeyPoint::InterpolateLerp(const ThreeDimStruct<float>& previousPosition, float percentage) const
{
    ThreeDimStruct<float> retVal = {};

    retVal.x = previousPosition.x * (1 - percentage) + this->x * percentage;
    retVal.y = previousPosition.y * (1 - percentage) + this->y * percentage;
    retVal.z = previousPosition.z * (1 - percentage) + this->z * percentage;

    return retVal;
}

ThreeDimStruct<float> MoveAnimationKeyPoint::InterpolateCosine(const ThreeDimStruct<float>& previousPosition, float percentage) const
{
    const float percentage2 = static_cast<float>((1 - std::cos(percentage * 3.14)) / 2);
    return InterpolateLerp(previousPosition, percentage2);
}

ThreeDimStruct<float> MoveAnimationKeyPoint::InterpolateBoolean(const ThreeDimStruct<float>& previousPosition, float percentage) const
{
    if (percentage < 0.5)
    {
        return previousPosition;
    }
    else
    {
        return GetEndPosition();
    }
}

ThreeDimStruct<float> MoveAnimationKeyPoint::Interpolate(const ThreeDimStruct<float>& previousPosition, float percentage) const
{
    switch (this->type)
    {
    case MoveAnimationType::LERP:
        return InterpolateLerp(previousPosition, percentage);
    case MoveAnimationType::COSINE:
        return InterpolateCosine(previousPosition, percentage);
    case MoveAnimationType::BOOLEAN:
        return InterpolateBoolean(previousPosition, percentage);
    default:
        SIMEXCEPTION(NotImplementedException);
        return {};
    }
}

MoveAnimationKeyPoint::MoveAnimationKeyPoint(float x, float y, float z, float duration, MoveAnimationType type)
    : x(x), y(y), z(z), duration(duration), type(type)
{
}

float MoveAnimationKeyPoint::GetDuration() const
{
    return duration;
}

ThreeDimStruct<float> MoveAnimationKeyPoint::GetEndPosition() const
{
    ThreeDimStruct<float> retVal = {};

    retVal.x = this->x;
    retVal.y = this->y;
    retVal.z = this->z;

    return retVal;
}

ThreeDimStruct<float> MoveAnimationKeyPoint::Evaluate(const ThreeDimStruct<float> &previousPosition, const float time) const
{
    if (time < 0 || time > this->duration)
    {
        SIMEXCEPTION(IllegalArgumentException);
    }

    const float percentageTime = time / this->duration;

    return Interpolate(previousPosition, percentageTime);
}

MoveAnimation::MoveAnimation(bool looped, std::initializer_list<MoveAnimationKeyPoint> keyPoints)
    : looped(looped), keyPoints(keyPoints)
{
}

bool MoveAnimation::IsLooped() const
{
    return looped;
}

void MoveAnimation::SetLooped(bool looped)
{
    this->looped = looped;
}

bool MoveAnimation::IsStarted() const
{
    return isStarted;
}

void MoveAnimation::SetDefaultType(MoveAnimationType type)
{
    this->defaultAnimationType = type;
}

void MoveAnimation::AddKeyPoint(const MoveAnimationKeyPoint & keyPoint)
{
    if (this->isStarted)
    {
        //Can't add another keypoint to an already running animation!
        SIMEXCEPTION(IllegalStateException);
    }
    keyPoints.push_back(keyPoint);
}

void MoveAnimation::AddKeyPoint(float x, float y, float z, float duration)
{
    AddKeyPoint(x, y, z, duration, defaultAnimationType);
}

void MoveAnimation::AddKeyPoint(float x, float y, float z, float duration, MoveAnimationType type)
{
    AddKeyPoint(MoveAnimationKeyPoint(x, y, z, duration, type));
}

void MoveAnimation::SetName(const std::string & name)
{
    this->name = name;
}

std::string MoveAnimation::GetName()
{
    return this->name;
}

void MoveAnimation::Start(const u32 startTimeMs, const ThreeDimStruct<float> &startPosition)
{
    if (keyPoints.size() == 0)
    {
        //Can't start animation without keypoints!
        SIMEXCEPTION(IllegalStateException);
    }
    this->isStarted = true;
    this->animationStartTimeMs = startTimeMs;

    float totalDuration = 0;
    for (const MoveAnimationKeyPoint &keyPoint : this->keyPoints)
    {
        totalDuration += keyPoint.GetDuration();
    }
    this->totalAnimationTimeMs = static_cast<u32>(totalDuration * 1000.0f);

    this->startPosition = startPosition;
}

ThreeDimStruct<float> MoveAnimation::Evaluate(u32 currentTimeMs)
{
    if (!this->isStarted)
    {
        //Call start first!
        SIMEXCEPTION(IllegalStateException);
    }

    u32 currentTimeAnimationLocalMs = currentTimeMs - animationStartTimeMs;
    bool hasLooped = false;
    if (currentTimeAnimationLocalMs >= this->totalAnimationTimeMs)
    {
        hasLooped = true;
        if (this->looped)
        {
            currentTimeAnimationLocalMs %= this->totalAnimationTimeMs;
        }
        else
        {
            currentTimeAnimationLocalMs = this->totalAnimationTimeMs;
            this->isStarted = false;
        }
    }

    u32 keyPointIndex = 0;
    u32 keyPointLocalTimeMs = 0;
    float sumOfAnimationTimes = 0;
    for (keyPointIndex = 0; keyPointIndex < keyPoints.size(); keyPointIndex++)
    {
        keyPointLocalTimeMs = currentTimeAnimationLocalMs - static_cast<u32>(sumOfAnimationTimes * 1000.0f);
        sumOfAnimationTimes += keyPoints[keyPointIndex].GetDuration();
        if (currentTimeAnimationLocalMs <= static_cast<u32>(sumOfAnimationTimes * 1000.0f))
        {
            break;
        }
    }

    ThreeDimStruct<float> previousPosition = {};
    if (keyPointIndex != 0)
    {
        previousPosition = keyPoints[keyPointIndex - 1].GetEndPosition();
    }
    else if (this->looped && hasLooped)
    {
        previousPosition = keyPoints[keyPoints.size() - 1].GetEndPosition();
    }
    else
    {
        previousPosition = startPosition;
    }

    auto retVal = keyPoints[keyPointIndex].Evaluate(previousPosition, keyPointLocalTimeMs / 1000.f);
    if (!this->isStarted)
    {
        //Reset animation
        *this = MoveAnimation();
    }
    return retVal;
}

size_t MoveAnimation::GetAmounOfKeyPoints() const
{
    return keyPoints.size();
}
