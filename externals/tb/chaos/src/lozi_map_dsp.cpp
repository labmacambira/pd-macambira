// 
//  
//  chaos~
//  Copyright (C) 2004  Tim Blechmann
//  
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//  
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//  
//  You should have received a copy of the GNU General Public License
//  along with this program; see the file COPYING.  If not, write to
//  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
//  Boston, MA 02111-1307, USA.

#include "lozi_map.hpp"
#include "chaos_dsp.hpp"

class lozi_map_dsp:
	public chaos_dsp<lozi_map>
{
	CHAOS_DSP_INIT(lozi_map, LOZI_MAP_ATTRIBUTES);

	LOZI_MAP_CALLBACKS;
};



FLEXT_LIB_DSP_V("lozi~", lozi_map_dsp);
