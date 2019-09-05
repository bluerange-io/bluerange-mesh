////////////////////////////////////////////////////////////////////////////////
// /****************************************************************************
// **
// ** Copyright (C) 2015-2019 M-Way Solutions GmbH
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

/*
 * This file contains the mesh configuration, which is a singleton. Some of the
 * values can be changed at runtime to alter the meshing behaviour.
 */

#ifndef BOARDCONFIG_H
#define BOARDCONFIG_H

#ifdef __cplusplus
	#include <types.h>

	#ifndef Boardconfig
	#define Boardconfig (&(Boardconf::getInstance().configuration))
	#endif
#endif //__cplusplus


#ifdef __cplusplus
	class Boardconf
	{
		public:
			static Boardconf& getInstance();

			void Initialize();
			void ResetToDefaultConfiguration();

		DECLARE_CONFIG_AND_PACKED_STRUCT(BoardConfiguration);
	};
#endif //__cplusplus

//Can be used to make the boardconfig available to C
#ifdef __cplusplus
extern void* fmBoardConfigPtr;
#else
extern struct BoardConfigurationC* fmBoardConfigPtr;
#endif

#endif //BOARDCONFIG_H
