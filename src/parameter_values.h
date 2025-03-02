/***********************************************************************
 * Filename: parameter_values.h
 * Author: Pavel Kejik
 * Date: 2024-04-12
 * Description:
 *     Defines constants, enumerations, and flags for parameter values used
 *     throughout the application.
 *
 ***********************************************************************/

#pragma once

#define MAIN_REVISION 18

#define FLAGS_NONE 0
#define BOOL_FLAG 0x01
#define COMM_PERIOD_FLAG 0x02
#define CHART_FLAG 0x04
#define FW_VERSION_FLAG 0x08

#define COMMAND_FLAG 0x10
#define STATE_FLAG 0x20


#define ERR_HISTORY_CNT 16

typedef enum
{
	PAR_U16 = 1,
	PAR_S16 = 2,
	PAR_U32 = 3,
	PAR_S32 = 4,
	PAR_STRING = 5,
} ParType_t;

typedef enum
{
	Manualni = 0,
	Casem = 1
} AutoOvladani_t;

typedef enum
{
	NormalniMod = 0,
	Parovani = 1,
	Vybrano = 2,
	Sparovano = 3,
} DeviceState_t;

typedef enum
{
	NeniChyba = 0,
	ChybaKamery = 1,
	MAX_ERROR = 100
} ErrorState_t;

typedef enum
{
	v_empty = 0,
	v_error = 1,
	v_warning = 2,
	v_info = 3,
} Verbosity_t;

typedef enum
{
	vypnuto = 0,
	povoleno = 1,
} AutoControl_t;

typedef enum
{
	automaticky = 0,
	vzdy = 1,
	nikdy = 2,
} FlashControl_t;

// Odpovida FeederEvents_t

typedef enum
{
	rst_Unknown = 0,
	rst_Software = 1,
	rst_Watchdog = 2,
	rst_Brownout = 3,
	rst_Poweron = 4,
	rst_External = 5,
	rst_Deepsleep = 6,

} ResetReason_t;
