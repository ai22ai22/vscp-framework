/* The MIT License (MIT)
 *
 * Copyright (c) 2014 - 2019, Andreas Merkle
 * http://www.blue-andi.de
 * vscp@blue-andi.de
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

/*******************************************************************************
    DESCRIPTION
*******************************************************************************/
/**
@brief  VSCP class 1 type Alarm events
@file   vscp_alarm.h
@author Andreas Merkle, http://www.blue-andi.de

@section desc Description
Alarm events that indicate that something not ordinary has occurred. Note that the priority bits
can be used as a mean to level alarm for severity.

This file is automatically generated. Don't change it manually.

*******************************************************************************/
/** @defgroup vscp_alarm Alarm events abstraction
 * Level 1 alarm events abstraction
 * @{
 * @ingroup vscp_l1_events_abstraction
 */

#ifndef __VSCP_ALARM_H__
#define __VSCP_ALARM_H__

/*******************************************************************************
    INCLUDES
*******************************************************************************/
#include <stdint.h>
#include "vscp_types.h"

/*******************************************************************************
    COMPILER SWITCHES
*******************************************************************************/

/*******************************************************************************
    CONSTANTS
*******************************************************************************/

/*******************************************************************************
    MACROS
*******************************************************************************/

/*******************************************************************************
    TYPES AND STRUCTURES
*******************************************************************************/

/*******************************************************************************
    VARIABLES
*******************************************************************************/

/*******************************************************************************
    FUNCTIONS
*******************************************************************************/

/**
 * Undefined alarm.
 *
 * @return Status
 * @retval FALSE Failed to send the event
 * @retval TRUE  Event successul sent
 *
 */
extern BOOL vscp_alarm_sendUndefinedEvent(void);

/**
 * Indicates a warning condition.
 *
 * @param[in] state State (0=off, 1=on)
 * @param[in] zone Zone for which event applies to (0-255). 255 is all zones.
 * @param[in] subZone Sub-zone for which event applies to (0-255). 255 is all sub-zones.
 * @return Status
 * @retval FALSE Failed to send the event
 * @retval TRUE  Event successul sent
 *
 */
extern BOOL vscp_alarm_sendWarningEvent(uint8_t state, uint8_t zone, uint8_t subZone);

/**
 * Indicates an alarm condition.
 *
 * @param[in] state State (0=off, 1=on)
 * @param[in] zone Zone for which event applies to (0-255). 255 is all zones.
 * @param[in] subZone Sub-zone for which event applies to (0-255). 255 is all sub-zones.
 * @return Status
 * @retval FALSE Failed to send the event
 * @retval TRUE  Event successul sent
 *
 */
extern BOOL vscp_alarm_sendAlarmOccurredEvent(uint8_t state, uint8_t zone, uint8_t subZone);

/**
 * Alarm sound should be turned on or off.
 *
 * @param[in] state State (0=off, 1=on)
 * @param[in] zone Zone for which event applies to (0-255). 255 is all zones.
 * @param[in] subZone Sub-zone for which event applies to (0-255). 255 is all sub-zones.
 * @return Status
 * @retval FALSE Failed to send the event
 * @retval TRUE  Event successul sent
 *
 */
extern BOOL vscp_alarm_sendAlarmSoundOnOffEvent(uint8_t state, uint8_t zone, uint8_t subZone);

/**
 * Alarm light should be turned on or off.
 *
 * @param[in] state State (0=off, 1=on)
 * @param[in] zone Zone for which event applies to (0-255). 255 is all zones.
 * @param[in] subZone Sub-zone for which event applies to (0-255). 255 is all sub-zones.
 * @return Status
 * @retval FALSE Failed to send the event
 * @retval TRUE  Event successul sent
 *
 */
extern BOOL vscp_alarm_sendAlarmLightOnOffEvent(uint8_t state, uint8_t zone, uint8_t subZone);

/**
 * Power has been lost or is available again.
 *
 * @param[in] state State (0=off, 1=on)
 * @param[in] zone Zone for which event applies to (0-255). 255 is all zones.
 * @param[in] subZone Sub-zone for which event applies to (0-255). 255 is all sub-zones.
 * @return Status
 * @retval FALSE Failed to send the event
 * @retval TRUE  Event successul sent
 *
 */
extern BOOL vscp_alarm_sendPowerOnOffEvent(uint8_t state, uint8_t zone, uint8_t subZone);

/**
 * Emergency stop has been hit/activated. All systems on the zone/sub-zone should go to their
 * inactive/safe state.
 *
 * @param[in] state State (0=off, 1=on)
 * @param[in] zone Zone for which event applies to (0-255). 255 is all zones.
 * @param[in] subZone Sub-zone for which event applies to (0-255). 255 is all sub-zones.
 * @return Status
 * @retval FALSE Failed to send the event
 * @retval TRUE  Event successul sent
 *
 */
extern BOOL vscp_alarm_sendEmergencyStopEvent(uint8_t state, uint8_t zone, uint8_t subZone);

/**
 * Emergency pause has been hit/activated. All systems on the zone/sub-zone should go to their
 * inactive/safe state but preserve there settings.
 *
 * @param[in] state State (0=off, 1=on)
 * @param[in] zone Zone for which event applies to (0-255). 255 is all zones.
 * @param[in] subZone Sub-zone for which event applies to (0-255). 255 is all subzones.
 * @return Status
 * @retval FALSE Failed to send the event
 * @retval TRUE  Event successul sent
 *
 */
extern BOOL vscp_alarm_sendEmergencyPauseEvent(uint8_t state, uint8_t zone, uint8_t subZone);

/**
 * Issued after an emergency stop or pause in order for nodes to reset and start operating .
 *
 * @param[in] state State (0=off, 1=on)
 * @param[in] zone Zone for which event applies to (0-255). 255 is all zones.
 * @param[in] subZone Sub-zone for which event applies to (0-255). 255 is all sub-zones.
 * @return Status
 * @retval FALSE Failed to send the event
 * @retval TRUE  Event successul sent
 *
 */
extern BOOL vscp_alarm_sendEmergencyResetEvent(uint8_t state, uint8_t zone, uint8_t subZone);

/**
 * Issued after an emergency pause in order for nodes to start operating from where they left of
 * without resetting their registers .
 *
 * @param[in] state State (0=off, 1=on)
 * @param[in] zone Zone for which event applies to (0-255). 255 is all zones.
 * @param[in] subZone Sub-zone for which event applies to (0-255). 255 is all sub-zones.
 * @return Status
 * @retval FALSE Failed to send the event
 * @retval TRUE  Event successul sent
 *
 */
extern BOOL vscp_alarm_sendEmergencyResumeEvent(uint8_t state, uint8_t zone, uint8_t subZone);

/**
 * Issued after an alarm system has been armed.
 *
 * @param[in] state State (0=off, 1=on)
 * @param[in] zone Zone for which event applies to (0-255). 255 is all zones.
 * @param[in] subZone Sub-zone for which event applies to (0-255). 255 is all sub-zones.
 * @return Status
 * @retval FALSE Failed to send the event
 * @retval TRUE  Event successul sent
 *
 */
extern BOOL vscp_alarm_sendArm(uint8_t state, uint8_t zone, uint8_t subZone);

/**
 * Issued after an alarm system has been disarmed.
 *
 * @param[in] state State (0=off, 1=on)
 * @param[in] zone Zone for which event applies to (0-255). 255 is all zones.
 * @param[in] subZone Sub-zone for which event applies to (0-255). 255 is all sub-zones.
 * @return Status
 * @retval FALSE Failed to send the event
 * @retval TRUE  Event successul sent
 *
 */
extern BOOL vscp_alarm_sendDisarm(uint8_t state, uint8_t zone, uint8_t subZone);

/**
 * Issued when a watchdog has been triggered.
 *
 * @param[in] state State (0=off, 1=on)
 * @param[in] zone Zone for which event applies to (0-255). 255 is all zones.
 * @param[in] subZone Sub-zone for which event applies to (0-255). 255 is all sub-zones.
 * @return Status
 * @retval FALSE Failed to send the event
 * @retval TRUE  Event successul sent
 *
 */
extern BOOL vscp_alarm_sendWatchdogEvent(uint8_t state, uint8_t zone, uint8_t subZone);

#endif /* __VSCP_ALARM_H__ */

/** @} */
