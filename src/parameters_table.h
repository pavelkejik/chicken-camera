/****************************************************************************
 * Filename: parameters_table.h
 * Author: Pavel Kejik
 * Date: 2024-04-12
 * Description:
 *     This header file contains the definitions for all parameters and
 *     registers used throughout the application. It defines a structured way
 *     to declare system parameters that are essential for operational control,
 *     configuration, and status monitoring of various components.
 *
 *     The parameters are defined using the following macros that encapsulate
 *     their properties and behavioral characteristics:
 *     - DefPar_Ram: Define a volatile parameter stored in RAM.
 *     - DefPar_Nv: Define a non-volatile parameter stored in persistent memory.
 *     - DefPar_Fun: Define a custom parameter type derived from the Register class.
 *
 *     In this table use following macros:
 *     - DefPar_Ram(_alias_,_regadr_,_def_,_min_, _max_,_type_,_dir_,_lvl_,_atr_)
 *     - DefPar_Nv (_alias_,_regadr_,_def_,_min_, _max_,_type_,_dir_,_lvl_,_atr_)
 *     - DefPar_Fun(_alias_,_regadr_,_def_,_min_, _max_,_type_,_dir_,_lvl_,_atr_,_fun_)
 * 
 * 
 *     Parameters are defined with attributes:
 *     - _alias_: Name of the parameter instance derived from the Register class.
 *     - _regadr_: Modbus register address. Parameters can occupy mutliple registers.
 *     - _def_: Default value, overwritten by stored value if valid for non-volatile types.
 *     - _min_, _max_: Define the permissible range of values.
 *     - _type_: Data type of the parameter, e.g., S16_, U16_, S32_, U32_, STRING_.
 *     - _dir_: Access direction (Par_R for read-only, Par_RW for read/write, Par_W for write-only).
 *     - _lvl_: Access level required to modify the parameter:
 *         - Par_Public: Accessible without privileges.
 *         - Par_Installer: Requires installer-level privileges.
 *         - Par_ESPNow: Accessible over ESP-Now, suitable for sharing definitions/values.
 *     - _atr_: Additional user-defined attributes or flags.
 *     - _fun_: Specific parameter type derived from the Register class (used with DefPar_Fun).
 ****************************************************************************/


#ifdef PAR_DEF_INCLUDES
#include "log.h"
#undef PAR_DEF_INCLUDES

#else /*PAR_DEF_INCLUDES*/

/*
-----------------------------------------------------------------------------------------------------------
  @ Stav kamery

-----------------------------------------------------------------------------------------------------------
*/
DefPar_Ram( StavZarizeni,  1,     Parovani,    NormalniMod ,     Sparovano, U16_,   Par_R  ,    Par_Public,    FLAGS_NONE )
DefPar_Ram( PoriditSnimek,  2,     vypnuto,    vypnuto ,     povoleno, U16_,   Par_RW  ,    Par_Public | Par_ESPNow,    BOOL_FLAG )
DefPar_RTC( PocetVajec, 10,  0,     5,    300, U16_,   Par_R,    Par_Public | Par_ESPNow,    CHART_FLAG )
// DefPar_RTC( NapetiBaterie_mV, 2,  0,    5,   300, U16_,   Par_R  ,    Par_Public | Par_ESPNow,    CHART_FLAG)

/*
-----------------------------------------------------------------------------------------------------------
  @ Konfigurace

-----------------------------------------------------------------------------------------------------------
*/

DefPar_Nv( KonfiguraceSnimani,   3,  automaticky, automaticky,nikdy, U16_,   Par_RW  ,   Par_Public | Par_ESPNow, STATE_FLAG)
DefPar_Nv( PouzitBlesk,   4,  automaticky, automaticky,nikdy, U16_,   Par_RW  ,   Par_Public | Par_ESPNow, STATE_FLAG)
DefPar_Nv( PosunVychodu, 5,  0,    -180,    180, S16_,   Par_RW  ,    Par_Public | Par_ESPNow,    FLAGS_NONE )
DefPar_Nv( PosunZapadu, 6,  0,    -180,    180, S16_,   Par_RW  ,    Par_Public | Par_ESPNow,    FLAGS_NONE )

/*
-----------------------------------------------------------------------------------------------------------
  @ ESP-NOW pripojeni

-----------------------------------------------------------------------------------------------------------
*/
DefPar_Nv( PeriodaKomunikace_S, 7,  10,    2,    3600, U16_,   Par_RW  ,    Par_Public | Par_ESPNow,    COMM_PERIOD_FLAG)
DefPar_Fun( MasterMacAdresa, 200,  255,    0,    0, U16_,   Par_RW  ,    Par_Installer,    FLAGS_NONE, mac_reg_nv)
DefPar_RTC( WiFiKanal, 203,  1,     1,    13, U16_,   Par_R,    Par_Public,    FLAGS_NONE )

/*
-----------------------------------------------------------------------------------------------------------
  @ Datum a cas

-----------------------------------------------------------------------------------------------------------
*/
DefPar_Nv( PopisCasu, 300,  0,    0,    46, STRING_,   Par_RW  ,    Par_Public,    FLAGS_NONE )
DefPar_Ram( AktualniCas, 323,  0,    0,    20, STRING_,   Par_R  ,    Par_Public,    FLAGS_NONE )
DefPar_RTC( CasVychodu, 333,  0,    0,   0 , S32_,   Par_R  ,    Par_Public,    FLAGS_NONE)
DefPar_RTC( CasZapadu, 335,  0,    0,   0 , S32_,   Par_R  ,    Par_Public,    FLAGS_NONE)


// -----------------------------------------------------------------------------------------------------------
//   @ System info
// -----------------------------------------------------------------------------------------------------------
// */

DefPar_Ram(VerzeFW, 8,MAIN_REVISION, MAIN_REVISION,	UINT16_MAX,U16_ ,Par_R, Par_Public | Par_ESPNow,FW_VERSION_FLAG)
DefPar_Ram(RestartCmd, 9,vypnuto, vypnuto,	povoleno,U16_ ,Par_RW, Par_Installer | Par_ESPNow,BOOL_FLAG)

DefPar_Ram(CompDate, 1001,0, 0,	30,STRING_ ,Par_R, Par_Public,FLAGS_NONE)
DefPar_Ram(ResetReason, 1016,rst_Poweron, rst_Unknown,	rst_Deepsleep,U16_ ,Par_R, Par_Public,FLAGS_NONE)

#endif /*PAR_DEF_INCLUDES*/
