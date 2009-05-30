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

#ifndef ALIEN_SMS_HEADER
#define ALIEN_SMS_HEADER

extern uint8_t sms_state;

#define sms_state_null          0
#define sms_state_formatcmd     1
#define sms_state_waits_a       2
#define sms_state_waitl_a       3
#define sms_state_cmdstart      4
#define sms_state_waits_b       5
#define sms_state_waitl_b       6
#define sms_state_hexstart_a    7
#define sms_state_hexstart_b    8
#define sms_state_messagehex_a  9
#define sms_state_messagehex_b  10
#define sms_state_cmdend        11
#define sms_state_waits_c       12
#define sms_state_end           13

#define sms_waits  (sms_state == sms_state_waits_a ||    \
                    sms_state == sms_state_waits_b ||    \
                    sms_state == sms_state_waits_c)

#define sms_waitl  (sms_state == sms_state_waitl_a ||    \
                    sms_state == sms_state_waitl_b)

void sms_setup();

#endif 
