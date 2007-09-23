// 
//   Copyright (C) 2007 Free Software Foundation, Inc.
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

// A file to contain all of the different strings for which we want compile time
// known string table keys.
#ifndef GNASH_NAMED_STRINGS_H
#define GNASH_NAMED_STRINGS_H

namespace gnash {

class string_table; // Forward

/// Named String Values
//
/// Long description here...
///
namespace NSV {

typedef enum {
		PROP_ADD_LISTENER = 1,
		PROP_ALIGN,
		PROP_uALPHA,
		PROP_BLOCK_INDENT,
		PROP_BOLD,
		PROP_BROADCAST_MESSAGE,
		PROP_BULLET,
		PROP_CALLEE,
		PROP_COLOR,
		PROP_CONSTRUCTOR,
		PROP_uuCONSTRUCTORuu,
		PROP_uCURRENTFRAME,
		PROP_uDROPTARGET,
		PROP_ENABLED,
		PROP_uFOCUSRECT,
		PROP_uFRAMESLOADED,
		PROP_uHEIGHT,
		PROP_uHIGHQUALITY,
		PROP_HTML_TEXT,
		PROP_INDENT,
		PROP_ITALIC,
		PROP_LEADING,
		PROP_LEFT_MARGIN,
		PROP_LENGTH,
		PROP_uLISTENERS,
		PROP_LOADED,
		PROP_uNAME,
		PROP_ON_LOAD,
		PROP_ON_RESIZE,
		PROP_ON_ROLL_OUT,
		PROP_ON_ROLL_OVER,
		PROP_ON_SELECT,
		PROP_ON_STATUS,
		PROP_uPARENT,
		PROP_uuPROTOuu,
		PROP_PROTOTYPE,
		PROP_PUSH,
		PROP_REMOVE_LISTENER,
		PROP_RIGHT_MARGIN,
		PROP_uROTATION,
		PROP_SCALE_MODE,
		PROP_SIZE,
		PROP_uSOUNDBUFTIME,
		PROP_SPLICE,
		PROP_iSTAGE,
		PROP_STATUS,
		PROP_uTARGET,
		PROP_TEXT,
		PROP_TEXT_COLOR,
		PROP_TEXT_WIDTH,
		PROP_TO_STRING,
		PROP_uTOTALFRAMES,
		PROP_UNDERLINE,
		PROP_uURL,
		PROP_VALUE_OF,
		PROP_uVISIBLE,
		PROP_uWIDTH,
		PROP_X,
		PROP_uX,
		PROP_uXMOUSE,
		PROP_uXSCALE,
		PROP_Y,
		PROP_uY,
		PROP_uYMOUSE,
		PROP_uYSCALE,
		CLASS_SYSTEM,
		CLASS_STAGE,
		CLASS_MOVIE_CLIP,
		CLASS_TEXT_FIELD,
		CLASS_MATH,
		CLASS_BOOLEAN,
		CLASS_COLOR,
		CLASS_SELECTION,
		CLASS_SOUND,
		CLASS_X_M_L_SOCKET,
		CLASS_DATE,
		CLASS_X_M_L,
		CLASS_X_M_L_NODE,
		CLASS_MOUSE,
		CLASS_OBJECT,
		CLASS_NUMBER,
		CLASS_STRING,
		CLASS_ARRAY,
		CLASS_KEY,
		CLASS_AS_BROADCASTER,
		CLASS_FUNCTION,
		CLASS_TEXT_SNAPSHOT,
		CLASS_VIDEO,
		CLASS_CAMERA,
		CLASS_MICROPHONE,
		CLASS_SHARED_OBJECT,
		CLASS_LOAD_VARS,
		CLASS_CUSTOM_ACTIONS,
		CLASS_NET_CONNECTION,
		CLASS_NET_STREAM,
		CLASS_CONTEXT_MENU,
		CLASS_MOVIE_CLIP_LOADER,
		CLASS_ERROR
	} named_strings;

/// Load the prenamed strings.
/// version controls case
void load_strings(string_table *table, int version);

} // namespace NSV
} // namespace gnash

#endif // GNASH_NAMED_STRINGS_H

