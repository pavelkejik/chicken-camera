/****************************************************************************
 * File: parameters.h
 * Author: Pavel Kejik
 * Date: 2024-04-12
 * Description:
 *     This header file serves as a central point for declaring and referencing
 *     system parameters used across the application. 
 * 
 ****************************************************************************/

#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "parameters_types.h"
#include "parameter_values.h"

#define INVALID_REGADR 0xFFFF
#define NONDEF_REG_VAL 0


#define DefPar_Ext(_name_,_regadr_,_def_,_min_, _max_,_type_,_dir_)

//#include "parameters_types.h"
#undef PAR_DEF_INCLUDES
/*Externalni reference RAM parametru*/
#undef  U32_
#undef  S32_
#undef  S16_
#undef  U16_
#undef  STRING_
#define  U32_
#define  S32_ int32_reg
#define  S16_ int16_reg
#define  U16_ uint16_reg
#define  STRING_ string_reg
#undef  DefPar_Ram
#undef  DefPar_Fun
#undef  DefPar_Nv
#undef  DefPar_RTC
#define DefPar_Ram(_name_,_regadr_,_def_,_min_, _max_,_type_,_dir_,_lvl_,_atr_) extern _type_ _name_;
#define DefPar_Nv(_name_,_regadr_,_def_,_min_, _max_,_type_,_dir_,_lvl_,_atr_)
#define DefPar_Fun(_name_,_regadr_,_def_,_min_, _max_,_type_,_dir_,_lvl_,_atr_,_fun_)
#define DefPar_RTC(_name_,_regadr_,_def_,_min_, _max_,_type_,_dir_,_lvl_,_atr_)
#include "parameters_table.h"

/*Externalni reference NV parametru*/
#undef  U32_
#undef  S32_
#undef  S16_
#undef  U16_
#undef  STRING_
#define  U32_
#define  S32_ int32_reg_nv
#define  S16_ int16_reg_nv
#define  U16_ uint16_reg_nv
#define STRING_ string_reg_nv
#undef  DefPar_Ram
#undef  DefPar_Fun
#undef  DefPar_Nv
#undef  DefPar_RTC
#define DefPar_Ram(_name_,_regadr_,_def_,_min_, _max_,_type_,_dir_,_lvl_,_atr_)
#define DefPar_Nv(_name_,_regadr_,_def_,_min_, _max_,_type_,_dir_,_lvl_,_atr_)  extern _type_ _name_;
#define DefPar_Fun(_name_,_regadr_,_def_,_min_, _max_,_type_,_dir_,_lvl_,_atr_,_fun_)
#define DefPar_RTC(_name_,_regadr_,_def_,_min_, _max_,_type_,_dir_,_lvl_,_atr_)
#include "parameters_table.h"

/*Externalni reference FUN parametru*/
#undef  U32_
#undef  S32_
#undef  S16_
#undef  U16_
#undef  STRING_
#define  U32_
#define  S32_
#define  S16_
#define  U16_
#define STRING_
#undef  DefPar_Ram
#undef  DefPar_Fun
#undef  DefPar_Nv
#undef  DefPar_RTC
#define DefPar_Ram(_name_,_regadr_,_def_,_min_, _max_,_type_,_dir_,_lvl_,_atr_)
#define DefPar_Nv(_name_,_regadr_,_def_,_min_, _max_,_type_,_dir_,_lvl_,_atr_)
#define DefPar_Fun(_name_,_regadr_,_def_,_min_, _max_,_type_,_dir_,_lvl_,_atr_,_fun_) extern _fun_ _name_;
#define DefPar_RTC(_name_,_regadr_,_def_,_min_, _max_,_type_,_dir_,_lvl_,_atr_)
#include "parameters_table.h"

/*Externalni reference RTC parametru*/
#undef  U32_
#undef  S32_
#undef  S16_
#undef  U16_
#undef  STRING_
#define  U32_
#define  S32_ int32_reg_rtc
#define  S16_ int16_reg_rtc
#define  U16_ uint16_reg_rtc
#define STRING_ string_reg
#undef  DefPar_Ram
#undef  DefPar_Fun
#undef  DefPar_Nv
#undef  DefPar_RTC
#define DefPar_Ram(_name_,_regadr_,_def_,_min_, _max_,_type_,_dir_,_lvl_,_atr_)
#define DefPar_Nv(_name_,_regadr_,_def_,_min_, _max_,_type_,_dir_,_lvl_,_atr_)
#define DefPar_Fun(_name_,_regadr_,_def_,_min_, _max_,_type_,_dir_,_lvl_,_atr_,_fun_)
#define DefPar_RTC(_name_,_regadr_,_def_,_min_, _max_,_type_,_dir_,_lvl_,_atr_)  extern _type_ _name_;
#include "parameters_table.h"
