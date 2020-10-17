/* iocustom.h
 *
 * William Miller
 * Oct 14 2020
 *
 * Custom Input/Output functions for controlling progress printing, datetime conversion and OpenCV typing
 *
 */

#pragma once

#include <colors.h>
#include <ctime>
#include <iostream>
#include <iomanip>
#include <string>

const std::string backspace({8, 8, 8, 8});

void print_percent(size_t current, size_t &previous, size_t total);
std::string datetime();
std::string type2str(int type);
