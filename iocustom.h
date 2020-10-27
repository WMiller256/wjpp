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

extern char errbuf[1024];

const std::string backspace({8, 8, 8, 8});

void print_percent(size_t current, size_t &previous, size_t total);
void error(const int line, const char* file);
std::string datetime();
