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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <string>

#include "Platform.h"



namespace Platform
{



void Log(LogLevel level, const char* fmt, ...)
{
    if (fmt == nullptr)
        return;
    if (level <= LogLevel::Debug)
        return;

    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}



}
