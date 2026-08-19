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
#include <stdexcept>
#include <CherrySimUtils.h>
#include <CherrySim.h>
#include <string>
#ifdef _MSC_VER
#include <filesystem>
#endif

std::set<int> CherrySimUtils::GenerateRandomNumbers(const int min, const int max, const unsigned int count)
{
    if (!(min < max) || ((int)count > max - min)) SIMEXCEPTION(IllegalArgumentException); //Wrong parameters

    std::set<int> numbers;

    while (numbers.size() < count)
    {
        numbers.insert(PSRNGINT(min, max));
    }

    return numbers;
}

std::string CherrySimUtils::GetNormalizedPath()
{
    //Check if the working directory was given as an environment variable
    //Should be given as /path/to/cherrysim without trailing /
    if(const char* env_p = std::getenv("CHERRYSIM_WORKDIR"))
    {
        return env_p;
    }

#ifdef __GNUC__
    //Unfortunately the sanitizer goes wild for std::filesystem::path on our used GCC version, so we have to do it by hand...
    std::string path = __FILE__;
    size_t lastSlash = path.rfind("/");
    std::string pathWithoutFile = path.substr(0, lastSlash);
    return pathWithoutFile;
#else
    std::filesystem::path file = __FILE__;
    std::string pathString = file.parent_path().string();
    return pathString;
#endif
}
