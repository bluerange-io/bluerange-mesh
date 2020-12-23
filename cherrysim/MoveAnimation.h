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
#pragma once

#include <vector>
#include <cmath>
#include <initializer_list>
#include <string>

#include "FmTypes.h"
#include "PrimitiveTypes.h"

enum class MoveAnimationType : u32
{
    LERP = 0,
    COSINE = 1,
    BOOLEAN = 2,

    INVALID = 0xFFFFFFFF,
};

class MoveAnimationKeyPoint
{
TESTER_PUBLIC:
    float x = 0;
    float y = 0;
    float z = 0;
    float duration = 0;
    MoveAnimationType type = MoveAnimationType::LERP;

    ThreeDimStruct<float> InterpolateLerp   (const ThreeDimStruct<float> &previousPosition, float percentage) const;
    ThreeDimStruct<float> InterpolateCosine (const ThreeDimStruct<float> &previousPosition, float percentage) const;
    ThreeDimStruct<float> InterpolateBoolean(const ThreeDimStruct<float> &previousPosition, float percentage) const;
    ThreeDimStruct<float> Interpolate       (const ThreeDimStruct<float> &previousPosition, float percentage) const;

public:
    MoveAnimationKeyPoint()                                              = default;
    MoveAnimationKeyPoint(const MoveAnimationKeyPoint &other)            = default;
    MoveAnimationKeyPoint(MoveAnimationKeyPoint &&other)                 = default;
    MoveAnimationKeyPoint& operator=(const MoveAnimationKeyPoint &other) = default;
    MoveAnimationKeyPoint& operator=(MoveAnimationKeyPoint &&other)      = default;

    MoveAnimationKeyPoint(float x, float y, float z, float duration, MoveAnimationType type);
    
    float GetDuration() const;
    ThreeDimStruct<float> GetEndPosition() const;

    ThreeDimStruct<float> Evaluate(const ThreeDimStruct<float> &previousPosition, float time) const;
};

class MoveAnimation
{
TESTER_PUBLIC:
    std::string name = "NULL";
    bool isStarted = false;
    u32 animationStartTimeMs = 0;
    u32 totalAnimationTimeMs = 0;
    ThreeDimStruct<float> startPosition = {};
    bool looped = false;
    std::vector<MoveAnimationKeyPoint> keyPoints = {};

    MoveAnimationType defaultAnimationType = MoveAnimationType::LERP;

public:
    MoveAnimation()                                      = default;
    MoveAnimation(const MoveAnimation &other)            = default;
    MoveAnimation(MoveAnimation &&other)                 = default;
    MoveAnimation& operator=(const MoveAnimation &other) = default;
    MoveAnimation& operator=(MoveAnimation &&other)      = default;

    MoveAnimation(bool looped, std::initializer_list<MoveAnimationKeyPoint> keyPoints);

    bool IsLooped() const;
    void SetLooped(bool looped);
    bool IsStarted() const;

    void SetDefaultType(MoveAnimationType type);

    void AddKeyPoint(const MoveAnimationKeyPoint& keyPoint);
    void AddKeyPoint(float x, float y, float z, float duration);
    void AddKeyPoint(float x, float y, float z, float duration, MoveAnimationType type);

    void SetName(const std::string& name);
    std::string GetName();

    void Start(u32 startTimeMs, const ThreeDimStruct<float> &startPosition);
    ThreeDimStruct<float> Evaluate(u32 currentTimeMs);

    size_t GetAmounOfKeyPoints() const;
};
