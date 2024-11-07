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

#ifndef PLATFORM_H
#define PLATFORM_H

#include "types.h"

#include <functional>
#include <string>

namespace Platform
{



enum LogLevel
{
    Debug,
    Info,
    Warn,
    Error,
};

void Log(LogLevel level, const char* fmt, ...);


}

#endif // PLATFORM_H
