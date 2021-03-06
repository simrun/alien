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
#include <stdint.h>
#include "temperature.h"
#include "messages.h"
#include "timer1.h"
#include "timer3.h"

/* We will have temperature sensors on GPIO6 and GPIO7 (PD6 and PD7) - 
 * While I appreciate that you can have more sensors on one 1wire, we're
 * not exactly short for GPIOs and this means that we don't have to mess
 * around with ROM and SELECT commands to talk to each sensor individually */
/* External Temperature will be PD6, Internal Temperature will be PD7 */

#define skiprom_cmd        0xCC
#define convtemp_cmd       0x44
#define readscratch_cmd    0xBE

#define temperature_ext_read       0x01
#define temperature_int_read       0x02

#define temperature_flags_ext_ok          0x01
#define temperature_flags_int_ok          0x02

uint8_t temperature_ext_crc, temperature_int_crc;
uint8_t temperature_flags,   temperature_state;
uint8_t temperature_internal_msb, temperature_internal_lsb;
uint8_t temperature_external_msb, temperature_external_lsb;

#define TEMP_EXT_RELEASE  DDRA  &= ~(1 <<  DDA7)   /* Set to input  */
#define TEMP_EXT_PULLLOW  DDRA  |=  (1 <<  DDA7)   /* Set to output */
#define TEMP_EXT_READ    (PINA  &   (1 << PINA7))  /* Read bit 7    */

#define TEMP_INT_RELEASE  DDRA  &= ~(1 <<  DDA6)
#define TEMP_INT_PULLLOW  DDRA  |=  (1 <<  DDA6)
#define TEMP_INT_READ    (PINA  &   (1 << PINA6))

#define TEMP_EXT_OK   (temperature_flags & temperature_flags_ext_ok)
#define TEMP_INT_OK   (temperature_flags & temperature_flags_int_ok)

/* No point having this as a function */
#define TEMP_CHECK_CARRYON                                                   \
          if(!(temperature_flags & (temperature_flags_ext_ok |               \
                                    temperature_flags_int_ok)))              \
          {                                                                  \
            temperature_state = temperature_state_want_to_get;               \
            return;                                                          \
          }

/* Enable timer 0, but don't configure for CTC or interrupts.
 * At FCPU/64 we can use this to perform long waits without leaving
 * the interrupt. */
#define TEMP_BUSYWAIT_SETUP_64    TCCR0  = ((1 << CS00) | (1 <<CS01))
#define TEMP_BUSYWAIT_SETUP_1     TCCR0  =  (1 << CS00)
#define TEMP_BUSYWAIT_SETUP_STOP  TCCR0  = 0;

#define TEMP_BUSYWAIT(n)      for (TCNT0 = 0; TCNT0 < (n);)
#define TEMP_BUSYWAIT_1_us(n)    TEMP_BUSYWAIT((n) * 16)
#define TEMP_BUSYWAIT_64_us(n)   TEMP_BUSYWAIT((n) / 4)

/* Because the timing must be accurate for RW we don't leave the interrupt */

void temperature_request()
{
  /* Reset some stuff */
  temperature_flags    = temperature_flags_ext_ok | temperature_flags_int_ok;
  temperature_ext_crc  = 0;
  temperature_int_crc  = 0;
  temperature_external_msb = 0;
  temperature_external_lsb = 0;
  temperature_internal_msb = 0;
  temperature_internal_lsb = 0;

  /* RESET */
  temperature_reset();

  /* Check if it's worth carrying on... */
  TEMP_CHECK_CARRYON

  /* SKIPROM cmd */
  temperature_writebyte(skiprom_cmd);

  /* CONV_T cmd */
  temperature_writebyte(convtemp_cmd);

  /* Return to timer1.c; timer3.c will ask it to bring control back here 
   * when a second has passed. Also, timer1/3.c guarantees that temperature 
   * will not have been started while timer3 is in use by sms.c */
  temperature_state = temperature_state_requested;
  timer3_clear();
  timer3_start();
}

void temperature_retrieve()
{
  uint8_t i, d;

  /* CONV_T test */
  d = temperature_readbit();

  if (!(d & temperature_ext_read))
  {
    /* If it hasn't completed it by now... gah! */
    temperature_flags &= ~(temperature_flags_ext_ok);
  }
  if (!(d & temperature_int_read))
  {
    temperature_flags &= ~(temperature_flags_int_ok);
  }

  TEMP_CHECK_CARRYON

  /* RESET */
  temperature_reset();

  TEMP_CHECK_CARRYON

  /* SKIPROM cmd */
  temperature_writebyte(skiprom_cmd);

  /* READSCRATCH cmd */
  temperature_writebyte(readscratch_cmd);

  /* READSCRATCH readbytes */
  /* Bytes 0 and 1 are temperature data in little endian (that's good),
   * then the rest can be ignored (but automatically shifted into the CRC
   * by readbyte). */
  temperature_readbyte(&temperature_external_lsb, &temperature_internal_lsb);
  temperature_readbyte(&temperature_external_msb, &temperature_internal_msb);

  /* Read the remaining 7 bytes, CRC and discard */
  for (i = 0; i < 7; i++)
  {
    temperature_readbyte(NULL, NULL);
  }

  /* SIGN_CHECK */
  /* For some reason, all the bits in the most significan byte of the temp.
   * should be the same. We use them to signal other things, like how good the 
   * temperature is, so check that they actually are the all 0 or 1 */
  if (TEMP_EXT_OK && temperature_external_msb != 0x00 &&
                     temperature_external_msb != 0xFF)
  {
    temperature_flags &= ~(temperature_flags_ext_ok);
  }
  if (TEMP_INT_OK && temperature_internal_msb != 0x00 &&
                     temperature_internal_msb != 0xFF)
  {
    temperature_flags &= ~(temperature_flags_int_ok);
  }

  TEMP_CHECK_CARRYON

  /* CRC_CHECK */
  /* The CRC is such that if you shift the last byte, the CRC byte, into the
   * CRC register, then it should equal zero. */
  if (temperature_ext_crc != 0)
  {
    temperature_flags &= ~(temperature_flags_ext_ok);
  }
  if (temperature_int_crc != 0)
  {
    temperature_flags &= ~(temperature_flags_int_ok);
  }

  TEMP_CHECK_CARRYON

  /* BIT_CLEAR */
  /* Clear a few bits so that we can use them to signal age or invalidness. */
  temperature_external_msb &= ~(temperature_msb_bit_err   |
                                temperature_msb_bit_valid |
                                temperature_msb_bits_toggle);
  temperature_internal_msb &= ~(temperature_msb_bit_err   |
                                temperature_msb_bit_valid |
                                temperature_msb_bits_toggle);

  /* BIT_SET */
  /* Always set this bit to signal that it is actually a real temperature.
   * Since the latest_data will be initialised to zero, then using this
   * statusled.c can tell if temperature.c has ever written a value */
  temperature_external_msb |= temperature_msb_bit_valid;
  temperature_internal_msb |= temperature_msb_bit_valid;

  /* So that it can be detected in the radio and the log when the value is
   * globally and not on a per-message basis (with SMS we don't care). */
  temperature_external_msb |= (latest_data.system_temp.external_msb + 
                               temperature_toggle_add) & 
                              temperature_msb_bits_toggle;
  temperature_internal_msb |= (latest_data.system_temp.internal_msb + 
                               temperature_toggle_add) & 
                              temperature_msb_bits_toggle;

  /* TEMP_SAVE */
  if (TEMP_EXT_OK)
  {
    latest_data.system_temp.external_msb = temperature_external_msb;
    latest_data.system_temp.external_lsb = temperature_external_lsb;
  }
  else
  {
    latest_data.system_temp.external_msb |= temperature_msb_bit_err;
  }

  if (TEMP_INT_OK)
  {
    latest_data.system_temp.internal_msb = temperature_internal_msb;
    latest_data.system_temp.internal_lsb = temperature_internal_lsb;
  }
  else
  {
    latest_data.system_temp.internal_msb |= temperature_msb_bit_err;
  }

  /* Have we completed successfully? If not, set state to want_to_get and 
   * timer1.c will call us again quicker */
  if (TEMP_EXT_OK && TEMP_INT_OK)
  {
    temperature_state = temperature_state_null;   /* Finished. */
  }
  else
  {
    temperature_state = temperature_state_want_to_get;
  }
}

void temperature_reset()
{
  /* RESET_PULSE part1 */
  if (TEMP_EXT_OK) TEMP_EXT_PULLLOW;
  if (TEMP_INT_OK) TEMP_INT_PULLLOW;

  TEMP_BUSYWAIT_SETUP_64;
  TEMP_BUSYWAIT_64_us(560);

  /* RESET_PULSE part2 */
  if (TEMP_EXT_OK) TEMP_EXT_RELEASE;
  if (TEMP_INT_OK) TEMP_INT_RELEASE;

  TEMP_BUSYWAIT_64_us(100);

  /* PRESENCE_PULSE test */
  if (TEMP_EXT_OK && TEMP_EXT_READ)
  {
    /* If it's high, then the sensor isn't there, or isn't working. */
    temperature_flags &= ~(temperature_flags_ext_ok);
  } 
  if (TEMP_INT_OK && TEMP_INT_READ)
  {
    temperature_flags &= ~(temperature_flags_int_ok);
  }

  TEMP_BUSYWAIT_64_us(240);

  TEMP_BUSYWAIT_SETUP_STOP;
}

void temperature_writebyte(uint8_t db)
{
  uint8_t i, b;

  TEMP_BUSYWAIT_SETUP_64;

  /* Shift left until we rollover to 0 */
  for (i = 0x01; i != 0x00; i = i << 1)
  {
    /* Select the required bit */
    b = db & i;

    /* If we are writing low then pull it down for the full 64us, 
     * release and wait a tiny bit for it to be surely high.
     * If we are writing high then pull it down for 4us then release,
     * and wait out the rest of the 72us (72 - 4 = 68) */

    if (TEMP_EXT_OK)  TEMP_EXT_PULLLOW;
    if (TEMP_INT_OK)  TEMP_INT_PULLLOW;

    if (b)  TEMP_BUSYWAIT_64_us(4);            /* Wait 4us    */
    else    TEMP_BUSYWAIT_64_us(64);           /* Wait 64us   */

    if (TEMP_EXT_OK)  TEMP_EXT_RELEASE;
    if (TEMP_INT_OK)  TEMP_INT_RELEASE;

    if (b)  TEMP_BUSYWAIT_64_us(68);    /* Wait out the rest of the slot */
    else    TEMP_BUSYWAIT_64_us(8);     /* Wait 8us for it to come high  */
  }

  TEMP_BUSYWAIT_SETUP_STOP;
}

void temperature_readbyte(uint8_t *ext_target, uint8_t *int_target)
{
  uint8_t i, d;

  for (i = 0x01; i != 0x00; i = i << 1)
  {
    d = temperature_readbit();

    if (TEMP_EXT_OK) 
    {
      temperature_crcpush(d & temperature_ext_read, &temperature_ext_crc);
 
      if ((d & temperature_ext_read) && ext_target != NULL)
      {
        *ext_target |= i;
      }
    }

    if (TEMP_INT_OK)
    {
      temperature_crcpush(d & temperature_int_read, &temperature_int_crc);

      if ((d & temperature_int_read) && int_target != NULL)
      {
        *int_target |= i;
      }
    }
  }
}

uint8_t temperature_readbit()
{
  uint8_t d;

  d = 0;   /* Initialise */

  TEMP_BUSYWAIT_SETUP_1;

  if (TEMP_EXT_OK)  TEMP_EXT_PULLLOW;
  if (TEMP_INT_OK)  TEMP_INT_PULLLOW;

  TEMP_BUSYWAIT(24)                        /* Wait 1.5us */

  if (TEMP_EXT_OK)  TEMP_EXT_RELEASE;
  if (TEMP_INT_OK)  TEMP_INT_RELEASE;

  TEMP_BUSYWAIT_1_us(12)                   /* Wait 12us  */

  /* Read */
  if (TEMP_EXT_OK && TEMP_EXT_READ)     d |= temperature_ext_read;
  if (TEMP_INT_OK && TEMP_INT_READ)     d |= temperature_int_read;

  /* Wait for the slot to end: Wait another 52us */

  TEMP_BUSYWAIT_SETUP_64;
  TEMP_BUSYWAIT_64_us(52);

  TEMP_BUSYWAIT_SETUP_STOP;

  return d;
}

void temperature_crcpush(uint8_t bit, uint8_t *crc)
{
  /* XOR gates push into bits 7, 3 and 2, so move the bit in 'bit' to
   * be in those. Take the bit and XOR it with the least significant
   * CRC bit. */

  bit = 0;
  if (bit || (*crc & 0x01))        /* One or the other */
  {
    if (!(bit && (*crc & 0x01)))   /* But not both     */
    {
      bit = 0x8C;                  /*  0b10001100  */
    }
  }

  /* Now shift the crc right */
  *crc = *crc >> 1;

  /* Now xor the bit with the crc to write the result of those gates */
  *crc ^= bit;
}

/* By default the port register bits will below and leaving it means that 
 * when we put outputmode (PULLLOW) then DQ gets grounded; When we set
 * input mode (RELEASE) no extra internal pullups are turned on and
 * DQ floats high */

