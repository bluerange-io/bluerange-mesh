////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2020 M-Way Solutions GmbH
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

#if !defined(_MSC_BUILD) && !defined(_WIN32)
static char *strrev(char *str)
{
      char *p1, *p2;

      if (! str || ! *str)
            return str;
      for (p1 = str, p2 = str + strlen(str) - 1; p2 > p1; ++p1, --p2)
      {
            *p1 ^= *p2;
            *p2 ^= *p1;
            *p1 ^= *p2;
      }
      return str;
}
#endif // !defined(_MSC_BUILD) && !defined(_WIN32) 

const char* ALPHABET = "BCDFGHJKLMNPQRSTVWXYZ123456789";
void CherrySimUtils::generateBeaconSerialForIndex(u32 index, char* buffer) {
	for (u32 i = 0; i < NODE_SERIAL_NUMBER_LENGTH; i++) {
		int rest = (int)(index % strlen(ALPHABET));
		buffer[i] = ALPHABET[rest];
		index /= strlen(ALPHABET);
	}
	buffer[NODE_SERIAL_NUMBER_LENGTH] = '\0';

	strrev(buffer);
}

std::set<int> CherrySimUtils::generateRandomNumbers(const int min, const int max, const unsigned int count)
{
	if (!(min < max) || ((int)count > max - min)) SIMEXCEPTION(IllegalArgumentException); //Wrong parameters

	std::set<int> numbers;

	while (numbers.size() < count)
	{
		numbers.insert(PSRNGINT(min, max));
	}

	return numbers;
}

std::string CherrySimUtils::getNormalizedPath()
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
