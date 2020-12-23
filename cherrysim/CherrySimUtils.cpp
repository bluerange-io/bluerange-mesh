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
