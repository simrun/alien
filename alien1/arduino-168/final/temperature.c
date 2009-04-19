/*
    Copyright (C) 2008  Daniel Richman

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    For a full copy of the GNU General Public License, 
    see <http://www.gnu.org/licenses/>.
*/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <stdint.h>

#include "temperature.h"
#include "messages.h"
#include "hexdump.h"
#include "gps.h"
#include "radio.h"

void temperature_retrieve_reading()
{
  /* TODO temperature_retrieve_reading() Retrieve the temperature */
}

void temperature_request_reading()
{
  /* TODO temperature_request_conversion() Start taking a reading*/
}

void temperature_init()
{
  /* TODO Implement temperature_init() Initialise IO */
}
