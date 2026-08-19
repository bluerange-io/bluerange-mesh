////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// ** BlueRange Mesh – Community Edition (CE)
// ** Copyright (c) 2015-2021 MWAY DIGITAL GmbH, Germany
// ** Copyright (c) 2021-2026 BlueRange GmbH, Germany
// **
// ** This file is part of BlueRange Mesh Community Edition (formerly known as
// ** FruityMesh).
// **
// ** BlueRange Mesh Community Edition is free software: you can redistribute it
// ** and/or modify it under the terms of the GNU General Public License as
// ** published by the Free Software Foundation, either version 3 of the
// ** License, or (at your option) any later version.
// **
// ** BlueRange Mesh Community Edition is distributed in the hope that it will
// ** be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
// ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// ** See the GNU General Public License for more details.
// **
// ** You should have received a copy of the GNU General Public License along
// ** with this program. If not, see https://www.gnu.org/licenses/.
// **
// ** IMPORTANT:
// ** Any modification, extension, or derivative work of this file MUST also be
// ** licensed under the GNU General Public License v3 or later and the complete
// ** corresponding source code MUST be made available.
// **
// ** Commercial Use:
// ** If you wish to use this software without the obligations of the GPLv3
// ** (including source code disclosure), a commercial license for
// ** BlueRange Mesh OEM Edition is required.
// **
// ** License violations automatically terminate your rights under this license
// ** and may result in legal action under applicable law.
// ** For further information please use the contact form at:
// ** https://bluerange.io/en/contact
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
