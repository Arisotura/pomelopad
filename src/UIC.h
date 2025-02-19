/*
    Copyright 2024 Arisotura

    This file is part of pomelopad.

    pomelopad is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    pomelopad is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with pomelopad. If not, see http://www.gnu.org/licenses/.
*/

#ifndef UIC_H
#define UIC_H

#include "types.h"

namespace UIC
{

bool Init();
void DeInit();
void Reset();

void SetKeyMask(u32 mask);
void SetTouchCoords(bool touching, int x, int y);
void SetVolume(u8 vol);

void Select();
void Release();
u8 Read();
void Write(u8 val);

}

#endif // UIC_H
