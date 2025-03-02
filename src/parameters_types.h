/***********************************************************************
 * Filename: parameters_types.h
 * Author: Pavel Kejik
 * Date: 2024-04-12
 * Description:
 *     Defines classes and data structures for handling parameter types
 *     within the application. The classes support serialization to JSON and
 *     ensure thread safety and data integrity via mutexes and atomic operations,
 *     and facilitate reading parameters by register address using ESP-NOW.
 *
 ***********************************************************************/
#pragma once
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include "parameter_values.h"
#include "freertos/FreeRTOS.h"
#include <atomic>
#include "Arduino.h"
#include <mutex>
#include "common.h"
#include "ArduinoJson.h"

typedef enum
{
	Par_R = 1,
	Par_W = 2,
	Par_RW = Par_R | Par_W,
	ParNv = 4,
	ParFun = 8,
	ParAccess = 0x0F,
	Par_Public = 0x10,
	Par_Installer = 0x20, /*require installer priviledge level*/
	Par_ESPNow = 0x40,	  /*used for sharing parameter defs over ESP-Now*/
	Par_U16 = 0x100,
	Par_S16 = 0x200,
	Par_U32 = 0x300,
	Par_S32 = 0x400,
	Par_STRING = 0x500,
} ParDscr_t;

//*****************************************************************************
//! \definicni struktura parametru
//*****************************************************************************
typedef struct
{
	int32_t min;
	int32_t max;
	int32_t def;
	ParDscr_t dsc;
	uint16_t adr;
	uint8_t atr;
	const char *const ptxt;
} pardef_t;

//*****************************************************************************
//! \bazova trida parametru- registru
//*****************************************************************************
class Register
{
protected:
	static Register *const ParSet[];
	static String writeString;

public:
	ParType_t ParType;
	static const uint16_t NmrParameters;
	static uint8_t ActiveLevel;
	static uint16_t ActiveRegAdr;
	static Register *GetPar(uint16_t RAdr);
	static Register *GetParByIdx(uint16_t idx);
	static size_t ActiveIdx;
	static const pardef_t ParDef[];
	const pardef_t &def;
	Register(const pardef_t &pd) : def(pd) {}
	virtual bool CheckLimits(int32_t vl) { return vl >= def.min && vl <= def.max; }
	static uint8_t ReadReg(int16_t *out, size_t adr);
	static uint8_t WriteReg(int16_t out, size_t adr);
	static Register *const ParameterSearch(const String &name);
	static bool JsonRead(const String &name, JsonObject doc);
	static bool JsonWrite(JsonPair pair);

	static void LockMtx(size_t adr);
	static void UnlockMtx(size_t adr);
	static bool IsReg(size_t adr);
	static bool IsWritable(size_t adr);
	static bool IsReadable(size_t adr);
	static void InitAll(void);
	static void Sleep(void);

	int32_t getDefault(void)
	{
		return def.def;
	}

	int32_t getMax(void)
	{
		return def.max;
	}

	int32_t getMin(void)
	{
		return def.min;
	}

	size_t getidx(void)
	{
		return (ActiveRegAdr - Register::ParDef[ActiveIdx].adr);
	}

	inline bool isNV(void) const { return def.dsc & ParNv; }

	inline bool iswritable(void) const
	{

		uint8_t reqlvl = def.dsc & ~(uint8_t)ParAccess;
		return (def.dsc & Par_W); //&& (reqlvl <= ActiveLevel);
	}
	inline bool isreadable(void) const
	{
		uint8_t reqlvl = def.dsc & ~(uint8_t)ParAccess;
		return (def.dsc & Par_R); //&& (reqlvl <= ActiveLevel);
	}

	virtual uint8_t readregval(int16_t *out) = 0;
	virtual uint8_t writeregval(int16_t out) = 0;

	virtual void GetJsonVal(JsonVariant json_val) {}
	virtual bool SetJsonVal(JsonVariant json_val) { return false; }

	virtual size_t getsize(void) { return 1; } /*pocet okupovanych registru*/
	virtual void resetval(void) = 0;

	virtual void lockmtx(void) {}
	virtual void unlockmtx(void) {}
};

//*****************************************************************************
//! \odvozena trida parametru typu S16- registru
//*****************************************************************************
class int16_reg : public Register
{
protected:
	std::atomic<int32_t> value;

public:
	int16_reg(const pardef_t &pd) : Register(pd) {}
	int32_t Get(void);
	virtual bool Set(int32_t v);
	uint8_t readregval(int16_t *out);
	uint8_t writeregval(int16_t inp);
	bool SetLimit(int32_t v);
	virtual void resetval(void);

	virtual void GetJsonVal(JsonVariant json_val)
	{
		if (def.atr & BOOL_FLAG)
		{
			json_val.set(value != 0);
		}
		else
		{
			json_val.set(value.load());
		}
	}

	virtual bool SetJsonVal(JsonVariant json_val)
	{
		bool retval = false;
		int tmp;
		if (def.atr & BOOL_FLAG)
		{
			if (json_val.is<bool>())
			{
				tmp = json_val.as<bool>() ? 1 : 0;
				retval = true;
			}
		}
		else
		{
			if (json_val.is<int>())
			{
				tmp = json_val.as<int>();
				retval = this->CheckLimits(tmp);
			}
			else if (json_val.is<String>())
			{
				String tmp_string = json_val.as<String>();
				tmp = tmp_string.toInt();
				retval = this->CheckLimits(tmp);
			}
		}

		if (retval)
		{
			this->Set(tmp);
		}
		return retval;
	}

	operator const int32_t()
	{
		return value;
	}
	int16_reg &operator=(int32_t in)
	{
		this->Set(in);
		return *this;
	}
};

//*****************************************************************************
//! \odvozena trida parametru typu U16- registru
//*****************************************************************************
class uint16_reg : public int16_reg
{
public:
	uint16_reg(const pardef_t &pd) : int16_reg(pd){};
	virtual bool Set(int32_t v)
	{
		uint16_t tmp;
		if (v > UINT16_MAX)
		{
			tmp = UINT16_MAX;
		}
		else
		{
			tmp =(uint16_t)((uint32_t)v);
		}

		bool retval = false;
		int16_t curr = value;

		if ((uint16_t)curr != tmp)
		{
			retval = true;
			value = tmp;
		}
		return retval;
	}

	virtual void GetJsonVal(JsonVariant json_val)
	{
		if (def.atr & BOOL_FLAG)
		{
			json_val.set(value != 0);
		}
		else
		{
			json_val.set((uint16_t)(value.load()));
		}
	}

	operator const uint16_t()
	{
		return (uint16_t)value;
	}
	uint16_reg &operator=(uint16_t in)
	{
		this->Set(in);
		return *this;
	}
};

//*****************************************************************************
//! \odvozena trida parametru typu U16- registru
//*****************************************************************************
class uint16_reg_nv : public uint16_reg
{

public:
	uint16_reg_nv(const pardef_t &pd) : uint16_reg(pd) {}
	virtual bool Set(int32_t v);
	virtual void resetval(void);
	operator const uint16_t()
	{
		return value;
	}
	uint16_reg_nv &operator=(uint16_t in)
	{
		Set(in);
		return *this;
	}
	// bool isValid(void) {return value.IsValid();}
};

//*****************************************************************************
//! \odvozena trida parametru typu S16- registru
//*****************************************************************************
class int16_reg_nv : public int16_reg
{
public:
	int16_reg_nv(const pardef_t &pd) : int16_reg(pd) {}
	virtual bool Set(int32_t v);
	virtual void resetval(void);
	operator const int16_t()
	{
		return value;
	}
	int16_reg_nv &operator=(int16_t in)
	{
		Set(in);
		return *this;
	}
	// bool isValid(void) {return value.IsValid();}
};

//*****************************************************************************
//! \odvozena trida parametru typu U16- registru
//*****************************************************************************
class uint16_reg_rtc : public uint16_reg
{
protected:
	int32_t &rtc_value;

public:
	uint16_reg_rtc(const pardef_t &pd, int32_t &rtc_var) : uint16_reg(pd), rtc_value(rtc_var) {}
	virtual bool Set(int32_t v);
	virtual void resetval(void);
	operator const uint16_t()
	{
		return value;
	}
	uint16_reg_rtc &operator=(uint16_t in)
	{
		this->Set(in);
		return *this;
	}
	// bool isValid(void) {return value.IsValid();}
};

//*****************************************************************************
//! \odvozena trida parametru typu S16- registru
//*****************************************************************************
class int16_reg_rtc : public int16_reg
{
protected:
	int32_t &rtc_value;

public:
	int16_reg_rtc(const pardef_t &pd, int32_t &rtc_var) : int16_reg(pd), rtc_value(rtc_var) {}
	virtual bool Set(int32_t v);
	virtual void resetval(void);
	operator const int16_t()
	{
		return value;
	}
	int16_reg_rtc &operator=(int16_t in)
	{
		this->Set(in);
		return *this;
	}
	// bool isValid(void) {return value.IsValid();}
};

//*****************************************************************************
//! \odvozena trida parametru typu S32- registru
//*****************************************************************************
class int32_reg : public Register
{
protected:
	std::atomic<int32_t> value;

public:
	int32_reg(const pardef_t &pd) : Register(pd) {}
	size_t getsize(void) { return 2; }
	int32_t Get(void);
	virtual bool Set(int32_t v);
	uint8_t readregval(int16_t *out);
	uint8_t writeregval(int16_t inp);
	bool SetLimit(int32_t v);
	virtual void resetval(void);

	virtual void GetJsonVal(JsonVariant json_val)
	{
		json_val.set(value.load());
	}

	virtual bool SetJsonVal(JsonVariant json_val)
	{
		bool retval = false;
		int tmp;
		if (json_val.is<int>())
		{
			tmp = json_val.as<int>();
			retval = this->CheckLimits(tmp);
		}
		else if (json_val.is<String>())
		{
			String tmp_string = json_val.as<String>();
			tmp = tmp_string.toInt();
			retval = this->CheckLimits(tmp);
		}

		if (retval)
		{
			this->Set(tmp);
		}
		return retval;
	}

	operator const int32_t()
	{
		return value;
	}
	int32_reg &operator=(int32_t in)
	{
		this->Set(in);
		return *this;
	}
};

//*****************************************************************************
//! \odvozena trida parametru typu S32- registru
//*****************************************************************************
class int32_reg_nv : public int32_reg
{
public:
	int32_reg_nv(const pardef_t &pd) : int32_reg(pd) {}
	virtual bool Set(int32_t v);
	virtual void resetval(void);
	operator const int32_t()
	{
		return value;
	}
	int32_reg_nv &operator=(int32_t in)
	{
		this->Set(in);
		return *this;
	}
};

//*****************************************************************************
//! \odvozena trida parametru typu S32- registru
//*****************************************************************************
class int32_reg_rtc : public int32_reg
{
protected:
	int32_t &rtc_value;

public:
	int32_reg_rtc(const pardef_t &pd, int32_t &rtc_var) : int32_reg(pd), rtc_value(rtc_var) {}
	virtual bool Set(int32_t v);
	virtual void resetval(void);
	operator const int32_t()
	{
		return value;
	}
	int32_reg_rtc &operator=(int32_t in)
	{
		this->Set(in);
		return *this;
	}
	// bool isValid(void) {return value.IsValid();}
};

//*****************************************************************************
//! \odvozena trida parametru typu U16- pro ulozeni logu
//*****************************************************************************
class log_reg : public Register
{
protected:
	ErrorState_t history[ERR_HISTORY_CNT];
	bool change;

public:
	log_reg(const pardef_t &pd) : Register(pd) {}
	size_t getsize(void) { return ERR_HISTORY_CNT; }
	uint8_t readregval(int16_t *out);
	uint8_t writeregval(int16_t inp);
	void resetval(void);
	void Set(ErrorState_t err);
};

//*****************************************************************************
//! \odvozena trida parametru - pro ulozeni mac adresy
//*****************************************************************************
class mac_reg : public Register
{
protected:
	uint8_t arr[6];
	bool change;

public:
	mac_reg(const pardef_t &pd) : Register(pd) {}
	size_t getsize(void) { return 6 / 2; }
	uint8_t readregval(int16_t *out);
	uint8_t writeregval(int16_t inp);
	void resetval(void);
	void Set(const uint8_t *mac);
	const uint8_t *Get(void)
	{
		return arr;
	}
};

//*****************************************************************************
//! \odvozena trida parametru - pro ulozeni mac adresy NV
//*****************************************************************************
class mac_reg_nv : public mac_reg
{

public:
	mac_reg_nv(const pardef_t &pd) : mac_reg(pd) {}
	void resetval(void);
	void Set(const uint8_t *mac);
};

//*****************************************************************************
//! \odvozena trida parametru typu STRING - pro ulozeni textu
//*****************************************************************************
// Parameter objekt pro ulozeni SSID textu
class string_reg : public Register
{
protected:
	std::mutex mutex;
	String val;
	bool change;

public:
	string_reg(const pardef_t &pd) : Register(pd) { val.reserve(def.max); }
	size_t getsize(void) { return (def.max + 1) / 2; }
	uint8_t readregval(int16_t *out);
	uint8_t writeregval(int16_t inp);
	void resetval(void);
	bool ischange(void);
	virtual bool Set(String &txt);
	virtual bool Set(const char *txt);
	String Get(void);

	virtual void GetJsonVal(JsonVariant json_val)
	{
		json_val.set(this->Get());
	}

	virtual bool SetTxtVal(String val)
	{
		if (val.length() < def.max)
		{
			this->Set(val);
			return true;
		}
		return false;
	}

	virtual bool SetJsonVal(JsonVariant json_val)
	{
		bool retval = false;
		if (json_val.is<String>())
		{
			String tmp = json_val.as<String>();
			if (tmp.length() < def.max)
			{
				this->Set(tmp);
				return true;
			}
			return false;
		}
		return retval;
	}
};

//*****************************************************************************
//! \odvozena trida parametru typu STRING - pro ulozeni textu
//*****************************************************************************
// Parameter objekt pro ulozeni SSID textu
class string_reg_nv : public string_reg
{
public:
	string_reg_nv(const pardef_t &pd) : string_reg(pd) {}
	void resetval(void);
	virtual bool Set(String &txt);
	virtual bool Set(const char *txt)
	{
		return string_reg::Set(txt);
	}
};

//*****************************************************************************
//! \odvozena trida parametru typu STRING - pro ulozeni IP adresy
//*****************************************************************************
class ipv4_reg : public string_reg
{
public:
	ipv4_reg(const pardef_t &pd) : string_reg(pd) {}

	void resetval(void)
	{
		IPAddress tmp((uint32_t)def.def);
		val = tmp.toString();
	}

	virtual bool Set(String &txt)
	{
		IPAddress tmp;
		bool retval = tmp.fromString(txt);
		if (retval)
		{
			String temp = tmp.toString();
			retval = string_reg::Set(temp);
		}
		return retval;
	}

	bool Set(uint32_t v)
	{
		IPAddress temp(v);
		String tmpTxt = temp.toString();
		return string_reg::Set(tmpTxt);
	}

	IPAddress GetIP()
	{
		IPAddress ip;
		{
			std::lock_guard<std::mutex> lock(mutex);
			ip.fromString(val);
		}
		return ip;
	}
};

//*****************************************************************************
//! \odvozena trida parametru typu STRING NV - pro ulozeni IP adresy
//*****************************************************************************
class ipv4_reg_nv : public string_reg_nv
{
public:
	ipv4_reg_nv(const pardef_t &pd) : string_reg_nv(pd) {}
	void resetval(void);
	bool Set(String &txt);
	bool Set(uint32_t v)
	{
		IPAddress temp(v);
		String tmpTxt = temp.toString();
		return string_reg_nv::Set(tmpTxt);
	}
	IPAddress GetIP();
};

// Modbus registr pro ovladani
class event_reg : public uint16_reg
{
public:
	event_reg(const pardef_t &pd) : uint16_reg(pd)
	{
	}

	uint8_t writeregval(int16_t inp);

	bool Set(int32_t v);
};

class calib_reg : public uint16_reg
{
public:
	calib_reg(const pardef_t &pd) : uint16_reg(pd)
	{
	}

	uint8_t writeregval(int16_t inp);

	bool Set(int32_t v);
};

class restart_cmd : public uint16_reg
{
public:
	restart_cmd(const pardef_t &pd) : uint16_reg(pd)
	{
	}

	uint8_t writeregval(int16_t inp);

	bool Set(int32_t v);
};



class time_reg : public string_reg
{
protected:
	time_t t_val;

public:
	time_reg(const pardef_t &pd) : string_reg(pd) {}
	void resetval(void)
	{
		t_val = 0;
		val = "--:--";
	}
	bool Set(String &txt)
	{
		bool retval = val.compareTo(txt);
		if (retval)
		{
			struct tm tm = GetTime();

			if (strptime(txt.c_str(), "%H:%M", &tm))
			{
				if (tm.tm_hour < 24 && tm.tm_hour >= 0 && tm.tm_min >= 0 && tm.tm_min < 60)
				{
					t_val = mktime(&tm);
					std::lock_guard<std::mutex> lock(mutex);
					val = txt;
				}
				else
				{
					retval = false;
				}
			}
			else
			{
				if (txt == "--:--")
				{
					t_val = 0;
					std::lock_guard<std::mutex> lock(mutex);
					val = txt;
					retval = true;
				}
				else
				{
					retval = false;
				}
			}
		}
		return retval;
	}
	bool Set(time_t v)
	{
		t_val = v;
		if (v == 0)
		{
			String tmp_str = "--:--";
			return string_reg::Set(tmp_str);
		}
		char timeString[8];
		struct tm tmp;
		localtime_r(&v, &tmp);
		strftime(timeString, 8, "%H:%M", &tmp);
		String tmp_str = timeString;

		return string_reg::Set(tmp_str);
	}
	time_t Get()
	{
		return t_val;
	}
};

class time_reg_nv : public time_reg
{
public:
	time_reg_nv(const pardef_t &pd) : time_reg(pd) {}
	void resetval(void);
	bool Set(String &txt);
	bool Set(time_t v);
};

// Modbus registr pro cteni pristupove urovne
class access_reg : public Register
{
public:
	access_reg(const pardef_t &pd) : Register(pd)
	{
	}
	uint8_t readregval(int16_t *out);

	void resetval(void)
	{
	}
	uint8_t writeregval(int16_t inp)
	{
		return 0;
	}
};

class chart_reg : public Register
{
protected:
	size_t maxSamples;
	size_t nmrSamples;
	size_t headIdx;
	size_t tailIdx;
	int16_t *valBuf;
	size_t newSamples;
	uint8_t maxVal;
	std::mutex mutex;

public:
	chart_reg(const pardef_t &pd) : Register(pd)
	{
		maxSamples = 0;
		nmrSamples = 0;
		headIdx = 0;
		tailIdx = 0;
		newSamples = 0;
		maxVal = 0;
	}

	void SetSize(uint16_t sz)
	{
		std::lock_guard<std::mutex> lock(mutex);
		if (maxSamples > 0)
		{
			delete valBuf;
		}
		valBuf = new int16_t[sz];
		maxSamples = sz;
		nmrSamples = 0;
		headIdx = 0;
		tailIdx = 0;
		this->resetval();
	}

	int32_t Get(void)
	{
		std::lock_guard<std::mutex> lock(mutex);

		return valBuf[tailIdx];
	}

	bool Set(int32_t val)
	{
		bool retval = false;

		if (val > INT16_MAX)
		{
			val = INT16_MAX;
		}
		else if (val < INT16_MIN)
		{
			val = INT16_MIN;
		}

		std::lock_guard<std::mutex> lock(mutex);

		valBuf[headIdx] = (int16_t)val;
		headIdx++;
		retval = true;

		if (headIdx >= maxSamples)
		{
			headIdx = 0;
			if (tailIdx == 0)
			{
				tailIdx++;
			}
		}
		if (tailIdx == headIdx)
		{
			if (tailIdx == (maxSamples - 1))
			{
				tailIdx = 0;
			}
			else
			{
				tailIdx++;
			}
		}

		if (nmrSamples < maxSamples)
		{
			nmrSamples++;
		}
		return retval;
	}
	bool SetLimit(int32_t newVal)
	{
		if (newVal > def.max)
		{
			newVal = def.max;
		}
		else if (newVal < def.min)
		{
			newVal = def.min;
		}

		bool retval = this->Set(newVal);
		if (retval)
		{
			if (newSamples < maxSamples)
			{
				newSamples++;
			}
		}
		return retval;
	}

	virtual void GetJsonVal(JsonVariant json_val)
	{
		std::lock_guard<std::mutex> lock(mutex);

		JsonArray arr = json_val.to<JsonArray>();
		size_t index = headIdx;
		for (int i = 0; i < nmrSamples; i++)
		{
			if (index == 0)
			{
				index = maxSamples - 1;
			}
			else
			{
				index--;
			}
			arr.add(valBuf[index]);
			if (nmrSamples == 1)
			{
				arr.add(valBuf[index]);
			}
		}
	}

	void resetval(void)
	{
		for (int i = 0; i < maxSamples; i++)
		{
			valBuf[i] = def.def;
		}
	}

	uint8_t readregval(int16_t *out)
	{
		*out = (int16_t)Get();
		return 1;
	}

	uint8_t writeregval(int16_t inp)
	{
		return 0;
	}
};