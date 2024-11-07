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

#ifndef WIFI_H
#define WIFI_H

#include "types.h"

namespace Wifi
{

enum
{
    IRQ_TXFrame = 6,
    IRQ_TXMisc,
};

bool Init();
void DeInit();
void Reset();

void SendCommand(u8 cmd, u32 arg);
void ReadBlock(u8* data, u32 len);
void WriteBlock(u8* data, u32 len);

}

#endif // WIFI_H
