/* 
 *  Copyright (c) 2010,
 *  Gavriloaie Eugen-Andrei (shiretu@gmail.com)
 *
 *  This file is part of crtmpserver.
 *  crtmpserver is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  crtmpserver is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with crtmpserver.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "new/defines.h"
#include "new/utils/misc/variant.h"
#include "new/utils/misc/variantmap.h"
#include "new/utils/logging/logging.h"
#include "new/utils/tinyxml/tinyxml.h"
#include "new/utils/misc/file.h"
#include "new/utils/misc/crypto.h"

#ifdef LOG_VARIANT_MEMORY_MANAGEMENT
int Variant::_constructorCount = 0;
int Variant::_dynamicAllocationCount = 0;
#endif

Variant::Variant() {
	CONSTRUCTOR;
	_type = V_NULL;
	memset(&_value, 0, sizeof (_value));
}

Variant::Variant(const Variant &val) {
	CONSTRUCTOR;
	InternalCopy(val);
}

Variant::Variant(const bool &val) {
	CONSTRUCTOR;
	_type = V_BOOL;
	memset(&_value, 0, sizeof (_value));
	_value.b = val;
}

Variant::Variant(const int8_t &val) {
	CONSTRUCTOR;
	_type = V_INT8;
	memset(&_value, 0, sizeof (_value));
	_value.i8 = val;
}

Variant::Variant(const int16_t &val) {
	CONSTRUCTOR;
	_type = V_INT16;
	memset(&_value, 0, sizeof (_value));
	_value.i16 = val;
}

Variant::Variant(const int32_t &val) {
	CONSTRUCTOR;
	_type = V_INT32;
	memset(&_value, 0, sizeof (_value));
	_value.i32 = val;
}

Variant::Variant(const int64_t &val) {
	CONSTRUCTOR;
	_type = V_INT64;
	memset(&_value, 0, sizeof (_value));
	_value.i64 = val;
}

Variant::Variant(const uint8_t &val) {
	CONSTRUCTOR;
	_type = V_UINT8;
	memset(&_value, 0, sizeof (_value));
	_value.ui8 = val;
}

Variant::Variant(const uint16_t &val) {
	CONSTRUCTOR;
	_type = V_UINT16;
	memset(&_value, 0, sizeof (_value));
	_value.ui16 = val;
}

Variant::Variant(const uint32_t &val) {
	CONSTRUCTOR;
	_type = V_UINT32;
	memset(&_value, 0, sizeof (_value));
	_value.ui32 = val;
}

Variant::Variant(const uint64_t &val) {
	CONSTRUCTOR;
	_type = V_UINT64;
	memset(&_value, 0, sizeof (_value));
	_value.ui64 = val;
}

Variant::Variant(const double &val) {
	CONSTRUCTOR;
	_type = V_DOUBLE;
	memset(&_value, 0, sizeof (_value));
	_value.d = val;
}

Variant::Variant(const Timestamp &val) {
	CONSTRUCTOR;
	_type = V_TIMESTAMP;
	memset(&_value, 0, sizeof (_value));
	DYNAMIC_ALLOC("_value.t");
	_value.t = new Timestamp;
	*_value.t = val;
	NormalizeTs();
}

Variant::Variant(const uint16_t year, const uint8_t month, const uint8_t day) {
	CONSTRUCTOR;
	_type = V_DATE;
	memset(&_value, 0, sizeof (_value));
	DYNAMIC_ALLOC("_value.t");
	_value.t = new Timestamp;
	memset(_value.t, 0, sizeof (Timestamp));
	_value.t->tm_year = year - 1900;
	_value.t->tm_mon = month - 1;
	_value.t->tm_mday = day;
	_value.t->tm_hour = 0;
	_value.t->tm_min = 0;
	_value.t->tm_sec = 0;
	NormalizeTs();
}

Variant::Variant(const uint8_t hour, const uint8_t min, const uint8_t sec, const uint16_t m) {
	CONSTRUCTOR;
	_type = V_TIME;
	memset(&_value, 0, sizeof (_value));
	DYNAMIC_ALLOC("_value.t");
	_value.t = new Timestamp;
	memset(_value.t, 0, sizeof (Timestamp));
	_value.t->tm_year = 70;
	_value.t->tm_mon = 0;
	_value.t->tm_mday = 1;
	_value.t->tm_hour = hour;
	_value.t->tm_min = min;
	_value.t->tm_sec = sec;
	NormalizeTs();
}

Variant::Variant(const uint16_t year, const uint8_t month, const uint8_t day,
		const uint8_t hour, const uint8_t min, const uint8_t sec, const uint16_t m) {
	CONSTRUCTOR;
	_type = V_TIMESTAMP;
	memset(&_value, 0, sizeof (_value));
	DYNAMIC_ALLOC("_value.t");
	_value.t = new Timestamp;
	memset(_value.t, 0, sizeof (Timestamp));
	_value.t->tm_year = year - 1900;
	_value.t->tm_mon = month - 1;
	_value.t->tm_mday = day;
	_value.t->tm_hour = hour;
	_value.t->tm_min = min;
	_value.t->tm_sec = sec;
	NormalizeTs();
}

Variant::Variant(const char *pVal) {
	CONSTRUCTOR;
	_type = V_STRING;
	memset(&_value, 0, sizeof (_value));
	DYNAMIC_ALLOC("_value.s");
	_value.s = new string(pVal);
}

Variant::Variant(const string &val) {
	CONSTRUCTOR;
	_type = V_STRING;
	memset(&_value, 0, sizeof (_value));
	DYNAMIC_ALLOC("_value.s");
	_value.s = new string(val);
}

Variant::Variant(const string &key, const Variant &value) {
	CONSTRUCTOR;
	_type = V_MAP;
	memset(&_value, 0, sizeof (_value));
	DYNAMIC_ALLOC("_value.m");
	_value.m = new VariantMap();
	_value.m->children[key] = value;
}

Variant::Variant(const string &typeName, const string &key, const Variant &value) {
	CONSTRUCTOR;
	_type = V_TYPED_MAP;
	memset(&_value, 0, sizeof (_value));
	DYNAMIC_ALLOC("_value.m");
	_value.m = new VariantMap();
	_value.m->typeName = typeName;
	if (key != "")
		_value.m->children[key] = value;
}

Variant::~Variant() {
	DESTRUCTOR;
	Reset();
}

void Variant::Reset(bool isUndefined) {
	switch (_type) {
		case V_DATE:
		case V_TIME:
		case V_TIMESTAMP:
		{
			DYNAMIC_FREE("_value.t");
			delete _value.t;
			break;
		}
		case V_BYTEARRAY:
		case V_STRING:
		{
			DYNAMIC_FREE("_value.s");
			delete _value.s;
			break;
		}
		case V_MAP:
		case V_TYPED_MAP:
		{
			DYNAMIC_FREE("_value.m");
			delete _value.m;
			break;
		}
		default:
		{
			break;
		}
	}
	if (isUndefined)
		_type = V_UNDEFINED;
	else
		_type = V_NULL;
	memset(&_value, 0, sizeof (_value));
}

string Variant::ToString(string name, uint32_t indent) {
	string result = "";
	string strIndent = string(indent * 4, ' ');
	switch (_type) {
		case V_NULL:
		{
			result += format("%s<NULL name=\"%s\"></NULL>",
					STR(strIndent), STR(name));
			break;
		}
		case V_UNDEFINED:
		{
			result += format("%s<UNDEFINED name=\"%s\"></UNDEFINED>",
					STR(strIndent), STR(name));
			break;
		}
		case V_BOOL:
		{
			result += format("%s<BOOL name=\"%s\">%s</BOOL>",
					STR(strIndent), STR(name), _value.b ? "true" : "false");
			break;
		}
		case V_INT8:
		{
			result += format("%s<INT8 name=\"%s\">%d</INT8>",
					STR(strIndent), STR(name), _value.i8);
			break;
		}
		case V_INT16:
		{
			result += format("%s<INT16 name=\"%s\">%d</INT16>",
					STR(strIndent), STR(name), _value.i16);
			break;
		}
		case V_INT32:
		{
			result += format("%s<INT32 name=\"%s\">%d</INT32>",
					STR(strIndent), STR(name), _value.i32);
			break;
		}
		case V_INT64:
		{
			result += format("%s<INT64 name=\"%s\">%lld</INT64>",
					STR(strIndent), STR(name), _value.i64);
			break;
		}
		case V_UINT8:
		{
			result += format("%s<UINT8 name=\"%s\">%u</UINT8>",
					STR(strIndent), STR(name), _value.ui8);
			break;
		}
		case V_UINT16:
		{
			result += format("%s<UINT16 name=\"%s\">%u</UINT16>",
					STR(strIndent), STR(name), _value.ui16);
			break;
		}
		case V_UINT32:
		{
			result += format("%s<UINT32 name=\"%s\">%u</UINT32>",
					STR(strIndent), STR(name), _value.ui32);
			break;
		}
		case V_UINT64:
		{
			result += format("%s<UINT64 name=\"%s\">%llu</UINT64>",
					STR(strIndent), STR(name), _value.ui64);
			break;
		}
		case V_DOUBLE:
		{
			result += format("%s<DOUBLE name=\"%s\">%.03f</DOUBLE>",
					STR(strIndent), STR(name), _value.d);
			break;
		}
		case V_TIMESTAMP:
		{
			result += format("%s<TIMESTAMP name=\"%s\">%s</TIMESTAMP>",
					STR(strIndent), STR(name), STR(*this));
			break;
		}
		case V_DATE:
		{
			result += format("%s<DATE name=\"%s\">%s</DATE>",
					STR(strIndent), STR(name), STR(*this));
			break;
		}
		case V_TIME:
		{
			result += format("%s<TIME name=\"%s\">%s</TIME>",
					STR(strIndent), STR(name), STR(*this));
			break;
		}
		case V_STRING:
		{
			result += format("%s<STR name=\"%s\">%s</STR>",
					STR(strIndent), STR(name), STR(*_value.s));
			break;
		}
		case V_BYTEARRAY:
		{
			result += format("%s<BYTEARRAY name=\"%s\">%llu bytes</BYTEARRAY>",
					STR(strIndent), STR(name), _value.s->length());
			break;
		}
		case V_TYPED_MAP:
		{
			result += format("%s<TYPED_MAP name=\"%s\" typename=\"%s\" isArray=\"%s\">\n",
					STR(strIndent), STR(name), STR(_value.m->typeName),
					_value.m->isArray ? "true" : "false");

			FOR_MAP(_value.m->children, string, Variant, i) {
				result += MAP_VAL(i).ToString(MAP_KEY(i), indent + 1) + "\n";
			}
			result += strIndent + "</TYPED_MAP>";
			break;
		}
		case V_MAP:
		{
			result += format("%s<MAP name=\"%s\" isArray=\"%s\">\n",
					STR(strIndent), STR(name),
					_value.m->isArray ? "true" : "false");

			FOR_MAP(_value.m->children, string, Variant, i) {
				result += MAP_VAL(i).ToString(MAP_KEY(i), indent + 1) + "\n";
			}
			result += strIndent + "</MAP>";
			break;
		}
		default:
		{
			FATAL("Invalid type: %d", _type);
			assert(false);
		}
	}
	return result;
}

Variant& Variant::operator=(const Variant &val) {
	Reset();
	InternalCopy(val);
	return *this;
}

Variant& Variant::operator=(const bool &val) {
	Reset();
	_type = V_BOOL;
	_value.b = val;
	return *this;
}

Variant& Variant::operator=(const int8_t &val) {
	Reset();
	_type = V_INT8;
	_value.i8 = val;
	return *this;
}

Variant& Variant::operator=(const int16_t &val) {
	Reset();
	_type = V_INT16;
	_value.i16 = val;
	return *this;
}

Variant& Variant::operator=(const int32_t &val) {
	Reset();
	_type = V_INT32;
	_value.i32 = val;
	return *this;
}

Variant& Variant::operator=(const int64_t &val) {
	Reset();
	_type = V_INT64;
	_value.i64 = val;
	return *this;
}

Variant& Variant::operator=(const uint8_t &val) {
	Reset();
	_type = V_UINT8;
	_value.ui8 = val;
	return *this;
}

Variant& Variant::operator=(const uint16_t &val) {
	Reset();
	_type = V_UINT16;
	_value.ui16 = val;
	return *this;
}

Variant& Variant::operator=(const uint32_t &val) {
	Reset();
	_type = V_UINT32;
	_value.ui32 = val;
	return *this;
}

Variant& Variant::operator=(const uint64_t &val) {
	Reset();
	_type = V_UINT64;
	_value.ui64 = val;
	return *this;
}

Variant& Variant::operator=(const double &val) {
	Reset();
	_type = V_DOUBLE;
	_value.d = val;
	return *this;
}

Variant& Variant::operator=(const Timestamp &val) {
	Reset();
	_type = V_TIMESTAMP;
	DYNAMIC_ALLOC("_value.t");
	_value.t = new Timestamp;
	*_value.t = val;
	NormalizeTs();
	return *this;
}

Variant& Variant::operator=(const char *pVal) {
	Reset();
	_type = V_STRING;
	DYNAMIC_ALLOC("_value.s");
	_value.s = new string(pVal);
	return *this;
}

Variant& Variant::operator=(const string &val) {
	Reset();
	_type = V_STRING;
	DYNAMIC_ALLOC("_value.s");
	_value.s = new string(val);
	return *this;
}

Variant::operator VariantType() {
	return _type;
}

Variant::operator bool() {
	switch (_type) {
		case V_NULL:
		case V_UNDEFINED:
		{
			return false;
		}
		case V_BOOL:
		{
			return _value.b;
		}
		case V_INT8:
		case V_INT16:
		case V_INT32:
		case V_INT64:
		case V_UINT8:
		case V_UINT16:
		case V_UINT32:
		case V_UINT64:
		case V_DOUBLE:
		{
			bool result = false;
			result |= (_value.i8 != 0);
			result |= (_value.i16 != 0);
			result |= (_value.i32 != 0);
			result |= (_value.i64 != 0);
			result |= (_value.ui8 != 0);
			result |= (_value.ui16 != 0);
			result |= (_value.ui32 != 0);
			result |= (_value.ui64 != 0);
			return result;
		}
		case V_TIMESTAMP:
		case V_DATE:
		case V_TIME:
		case V_STRING:
		case V_TYPED_MAP:
		case V_MAP:
		default:
		{
			ASSERT("Cast to bool failed: %s", STR(ToString()));
			return false;
		}
	}
}

#define OPERATOR_DEF(ctype) \
Variant::operator ctype() {\
    switch (_type) {\
        case V_NULL:\
        case V_UNDEFINED:\
        {\
            return 0;\
        }\
        case V_BOOL:\
        {\
            return (ctype) _value.b;\
        }\
        case V_INT8:\
        {\
            return (ctype) _value.i8;\
        }\
        case V_INT16:\
        {\
            return (ctype) _value.i16;\
        }\
        case V_INT32:\
        {\
            return (ctype) _value.i32;\
        }\
        case V_INT64:\
        {\
            return (ctype) _value.i64;\
        }\
        case V_UINT8:\
        {\
            return (ctype) _value.ui8;\
        }\
        case V_UINT16:\
        {\
            return (ctype) _value.ui16;\
        }\
        case V_UINT32:\
        {\
            return (ctype) _value.ui32;\
        }\
        case V_UINT64:\
        {\
            return (ctype) _value.ui64;\
        }\
        case V_DOUBLE:\
        {\
            return (ctype) _value.d;\
        }\
        case V_TIMESTAMP:\
        case V_DATE:\
        case V_TIME:\
        case V_STRING:\
        case V_TYPED_MAP:\
        case V_MAP:\
        default:\
        {\
            ASSERT("Cast failed: %s", STR(ToString()));\
            return 0;\
        }\
    }\
}

OPERATOR_DEF(int8_t);
OPERATOR_DEF(int16_t);
OPERATOR_DEF(int32_t);
OPERATOR_DEF(int64_t);
OPERATOR_DEF(uint8_t);
OPERATOR_DEF(uint16_t);
OPERATOR_DEF(uint32_t);
OPERATOR_DEF(uint64_t);
OPERATOR_DEF(double);

Variant::operator Timestamp() {
	if (_type == V_DATE ||
			_type == V_TIME ||
			_type == V_TIMESTAMP) {
		return *_value.t;
	} else {
		ASSERT("Cast to struct tm failed: %s", STR(ToString()));
		Timestamp temp = {0};
		return temp;
	}
}

Variant::operator string() {
	switch (_type) {
		case V_BOOL:
		{
			return _value.b ? "true" : "false";
		}
		case V_INT8:
		case V_INT16:
		case V_INT32:
		{
			return format("%d", this->operator int32_t());
		}
		case V_INT64:
		{
			return format("%lld", this->operator int64_t());
		}
		case V_UINT8:
		case V_UINT16:
		case V_UINT32:
		{
			return format("%u", this->operator uint32_t());
		}
		case V_UINT64:
		{
			return format("%llu", this->operator uint64_t());
		}
		case V_DOUBLE:
		{
			return format("%.3f", this->operator double());
		}
		case V_TIMESTAMP:
		{
			char tempBuff[24] = {0};
			return string(tempBuff, strftime(tempBuff, 24, "%Y-%m-%dT%H:%M:%S.000", _value.t));
		}
		case V_DATE:
		{
			char tempBuff[24] = {0};
			return string(tempBuff, strftime(tempBuff, 24, "%Y-%m-%d", _value.t));
		}
		case V_TIME:
		{
			char tempBuff[24] = {0};
			return string(tempBuff, strftime(tempBuff, 24, "%H:%M:%S.000", _value.t));
		}
		case V_BYTEARRAY:
		case V_STRING:
		{
			return *_value.s;
		}
		case V_NULL:
		case V_UNDEFINED:
		case V_TYPED_MAP:
		case V_MAP:
		default:
		{
			ASSERT("Cast to string failed: %s", STR(ToString()));
			return "";
		}
	}
	return "";
}

Variant& Variant::operator[](const string &key) {
	if ((_type != V_TYPED_MAP) &&
			(_type != V_MAP) &&
			(_type != V_NULL) &&
			(_type != V_UNDEFINED)) {
		ASSERT("Subscript operator applied on a incorrect Variant type: %s",
				STR(ToString()));
	}
	if ((_type == V_NULL) || (_type == V_UNDEFINED)) {
		_type = V_MAP;
		DYNAMIC_ALLOC("_value.m");
		_value.m = new VariantMap;
	}
	if (!MAP_HAS1(_value.m->children, key)) {
		_value.m->children[key] = Variant();
	}
	return _value.m->children[key];
}

Variant& Variant::operator[](const char *key) {
	return operator[](string(key));
}

Variant& Variant::operator[](const double &key) {
	stringstream ss;
	ss << "__index__value__" << key;
	return operator[](ss.str());
}

Variant& Variant::operator[](const uint32_t &key) {
	stringstream ss;
	ss << "__index__value__" << key;
	return operator[](ss.str());
}

Variant& Variant::operator[](Variant &key) {
	stringstream ss;
	switch (key._type) {
		case V_BOOL:
		case V_INT8:
		case V_INT16:
		case V_INT32:
		case V_INT64:
		case V_UINT8:
		case V_UINT16:
		case V_UINT32:
		case V_UINT64:
		case V_DOUBLE:
		case V_TIMESTAMP:
		case V_DATE:
		case V_TIME:
		{
			ss << "__index__value__" << STR(key);
			break;
		}
		case V_STRING:
		{
			ss << *key._value.s;
			break;
		}
		case V_NULL:
		case V_UNDEFINED:
		case V_TYPED_MAP:
		case V_MAP:
		default:
		{
			ASSERT("Variant has invalid type to be used as an index: %s", STR(key.ToString()));
			break;
		}
	}
	return operator[](ss.str());
}

Variant &Variant::GetValue(string key, bool caseSensitive) {
	if (caseSensitive) {
		return (*this)[key];
	} else {

		FOR_MAP(*this, string, Variant, i) {
			if (lowercase(MAP_KEY(i)) == lowercase(key))
				return MAP_VAL(i);
		}

		return (*this)[key];
	}
}

bool Variant::operator==(Variant variant) {
	return ToString() == variant.ToString();
}

bool Variant::operator!=(Variant variant) {
	return !operator==(variant);
}

bool Variant::operator==(VariantType type) {
	if (type == _V_NUMERIC)
		return _type == V_INT8 ||
			_type == V_INT8 ||
			_type == V_INT16 ||
			_type == V_INT32 ||
			_type == V_INT64 ||
			_type == V_UINT8 ||
			_type == V_UINT16 ||
			_type == V_UINT32 ||
			_type == V_UINT64 ||
			_type == V_DOUBLE;
	else
		return _type == type;
}

bool Variant::operator!=(VariantType type) {
	return !operator ==(type);
}

string Variant::GetTypeName() {
	if (_type != V_TYPED_MAP) {
		ASSERT("GetMapName failed: %s", STR(ToString()));
		return "";
	}
	return _value.m->typeName;
}

void Variant::SetTypeName(string name) {
	if ((_type != V_TYPED_MAP) && (_type != V_MAP) &&
			(_type != V_UNDEFINED) && (_type != V_NULL)) {
		ASSERT("SetMapName failed: %s", STR(ToString()));
		return;
	}
	if (_type == V_UNDEFINED || _type == V_NULL) {
		DYNAMIC_ALLOC("_value.m");
		_value.m = new VariantMap;
	}
	_type = V_TYPED_MAP;
	_value.m->typeName = name;
}

bool Variant::HasKey(const string &key, bool caseSensitive) {
	if (_type != V_TYPED_MAP && _type != V_MAP) {
		ASSERT("HasKey failed: %s", STR(ToString()));
		return false;
	}
	if (caseSensitive) {
		return MAP_HAS1(_value.m->children, key);
	} else {

		FOR_MAP(*this, string, Variant, i) {
			if (lowercase(MAP_KEY(i)) == lowercase(key))
				return true;
		}
		return false;
	}
}

void Variant::RemoveKey(const string &key) {
	if (_type != V_TYPED_MAP && _type != V_MAP) {
		ASSERT("RemoveKey failed: %s", STR(ToString()));
		return;
	}
	_value.m->children.erase(key);
}

void Variant::RemoveAt(const uint32_t index) {
	if (_type != V_TYPED_MAP && _type != V_MAP) {
		ASSERT("RemoveKey failed: %s", STR(ToString()));
		return;
	}
	_value.m->children.erase(format("__index__value__%u", index));
}

void Variant::RemoveAllKeys() {
	if (_type != V_TYPED_MAP && _type != V_MAP) {
		ASSERT("RemoveAllKeys failed: %s", STR(ToString()));
		return;
	}
	_value.m->children.clear();
}

uint32_t Variant::MapSize() {
	if (_type == V_NULL || _type == V_UNDEFINED)
		return 0;
	if (_type != V_TYPED_MAP && _type != V_MAP) {
		ASSERT("MapSize failed: %s", STR(ToString()));
		return 0;
	}
	return (uint32_t) _value.m->children.size();
}

uint32_t Variant::MapDenseSize() {
	if (_type == V_NULL || _type == V_UNDEFINED)
		return 0;
	if (_type != V_TYPED_MAP && _type != V_MAP) {
		ASSERT("MapSize failed: %s", STR(ToString()));
		return 0;
	}

	uint32_t denseCount = 0;
	for (denseCount = 0; denseCount < MapSize(); denseCount++) {
		if (!MAP_HAS1(_value.m->children, format("__index__value__%u", denseCount)))
			break;
	}

	return denseCount;
}

void Variant::PushToArray(Variant value) {
	if (_type != V_NULL && _type != V_MAP)
		ASSERT("This is not an array and it can't be converted to array");
	IsArray(true);
	(*this)[(uint32_t)this->MapDenseSize()] = value;
}

map<string, Variant>::iterator Variant::begin() {
	if (_type != V_TYPED_MAP && _type != V_MAP) {
		ASSERT("This is not a map-like variant: %s", STR(ToString()));
		map<string, Variant> temp;
		return temp.begin();
	}
	return _value.m->children.begin();
}

map<string, Variant>::iterator Variant::end() {
	if (_type != V_TYPED_MAP && _type != V_MAP) {
		ASSERT("This is not a map-like variant: %s", STR(ToString()));
		map<string, Variant> temp;
		return temp.end();
	}
	return _value.m->children.end();
}

bool Variant::IsTimestamp(VariantType &type) {
	Variant &temp = *this;

	if ((VariantType) temp != V_MAP)
		return false;

	bool hasDate = temp.HasKey("year") && temp.HasKey("month") && temp.HasKey("day");
	bool hasLongTime = temp.HasKey("hour") && temp.HasKey("min") && temp.HasKey("sec");
	bool hasShortTime = false;
	if (!hasLongTime)
		hasShortTime = temp.HasKey("hour") && temp.HasKey("min");
	bool hasIsdst = temp.HasKey("isdst");
	bool hasType = temp.HasKey("type");

	if (hasDate) {
		hasDate = hasDate && (temp["year"] == _V_NUMERIC);
		hasDate = hasDate && (temp["month"] == _V_NUMERIC);
		hasDate = hasDate && (temp["day"] == _V_NUMERIC);
	}

	if (hasLongTime) {
		hasLongTime = hasLongTime && (temp["hour"] == _V_NUMERIC);
		hasLongTime = hasLongTime && (temp["min"] == _V_NUMERIC);
		hasLongTime = hasLongTime && (temp["sec"] == _V_NUMERIC);
	} else if (hasShortTime) {
		hasShortTime = hasShortTime && (temp["hour"] == _V_NUMERIC);
		hasShortTime = hasShortTime && (temp["min"] == _V_NUMERIC);
	}
	bool hasTime = hasLongTime || hasShortTime;

	if (hasIsdst)
		hasIsdst = hasIsdst && (temp["isdst"] == V_BOOL);

	if ((!hasDate) && (!hasTime))
		return false;

	uint32_t size = 0;
	if (hasDate)
		size += 3;
	if (hasLongTime)
		size += 3;
	else if (hasShortTime)
		size += 2;
	if (hasType)
		size += 1;
	if (hasIsdst)
		size += 1;


	if (hasType) {
		if (temp["type"] == "date") {
			hasDate = true;
			hasTime = false;
		}
		if (temp["type"] == "time") {
			hasDate = false;
			hasTime = true;
		}
		if (temp["type"] == "timestamp") {
			hasDate = true;
			hasTime = true;
		}
	}

	if (hasDate && hasTime)
		type = V_TIMESTAMP;
	else if (hasDate)
		type = V_DATE;
	else
		type = V_TIME;

	return temp.MapSize() == size;
}

bool Variant::IsNumeric() {
	return _type == V_DOUBLE ||
			_type == V_INT16 ||
			_type == V_INT32 ||
			_type == V_INT64 ||
			_type == V_INT8 ||
			_type == V_UINT16 ||
			_type == V_UINT32 ||
			_type == V_UINT64 ||
			_type == V_UINT8;
}

bool Variant::IsArray() {
	if (_type == V_MAP)
		return _value.m->isArray;
	return false;
}

void Variant::IsArray(bool isArray) {
	if (_type == V_NULL) {
		_type = V_MAP;
		DYNAMIC_ALLOC("_value.m");
		_value.m = new VariantMap;
	}
	if (_type == V_MAP)
		_value.m->isArray = isArray;
}

bool Variant::IsByteArray() {
	return _type == V_BYTEARRAY;
}

void Variant::IsByteArray(bool isByteArray) {
	if (isByteArray) {
		if (_type == V_STRING) {
			_type = V_BYTEARRAY;
		}
	} else {
		if (_type == V_BYTEARRAY) {
			_type = V_STRING;
		}
	}
}

bool Variant::ConvertToTimestamp() {
	VariantType detectedType = V_NULL;
	if (!IsTimestamp(detectedType))
		return false;

	Timestamp temp = {0};

	if (detectedType == V_DATE || detectedType == V_TIMESTAMP) {
		temp.tm_year = (int) ((int32_t) (*this)["year"] - 1900);
		temp.tm_mon = (int) ((int32_t) (*this)["month"]);
		temp.tm_mday = (int) ((int32_t) (*this)["day"]);
	} else {
		temp.tm_year = 70;
		temp.tm_mon = 0;
		temp.tm_mday = 1;
	}

	if (detectedType == V_TIME || detectedType == V_TIMESTAMP) {
		temp.tm_hour = (int) ((int32_t) (*this)["hour"]);
		temp.tm_min = (int) ((int32_t) (*this)["min"]);
		temp.tm_sec = (int) (HasKey("sec") ?
				(int32_t) (*this)["sec"] : 0);
		temp.tm_isdst = HasKey("isdst") ? (bool) ((*this)["isdst"]) : false;
	}

	if (mktime(&temp) < 0) {
		FATAL("mktime failed");
		return false;
	}

	Reset();
	DYNAMIC_ALLOC("_value.t");
	_value.t = new Timestamp;
	*_value.t = temp;

	_type = detectedType;

	return true;
}

void Variant::Compact() {
	switch (_type) {
		case V_DOUBLE:
		{
			if ((((double) (*this)) < INT32_MIN)
					|| (((double) (*this)) > UINT32_MAX))
				break;
			Variant &variant = *this;
			double doubleVal = (double) variant;
			if ((int64_t) doubleVal != doubleVal)
				break;
			variant = (int64_t) doubleVal;
			variant.Compact();
			break;
		}
		case V_INT64:
		{
			Variant &variant = *this;
			int64_t val = (int64_t) variant;
			if ((val < INT32_MIN) || (val > UINT32_MAX))
				break;
			if (val < 0)
				variant = (int32_t) variant;
			else
				variant = (uint32_t) variant;
			Compact();
			break;
		}
		case V_INT32:
		{
			Variant &variant = *this;
			int32_t val = (int32_t) variant;
			if ((val < INT16_MIN) || (val > (int32_t)(UINT16_MAX)))
				break;
			if (val < 0)
				variant = (int16_t) variant;
			else
				variant = (uint16_t) variant;
			Compact();
			break;
		}
		case V_INT16:
		{
			Variant &variant = *this;
			int16_t val = (int16_t) variant;
			if ((val < INT8_MIN) || (val > (int16_t)(UINT8_MAX)))
				break;
			if (val < 0)
				variant = (int8_t) variant;
			else
				variant = (uint8_t) variant;
			Compact();
			break;
		}
		case V_UINT64:
		{
			Variant &variant = *this;
			if ((uint64_t) variant <= INT64_MAX) {
				variant = (int64_t) variant;
				Compact();
			}
			break;
		}
		case V_UINT32:
		{
			Variant &variant = *this;
			if ((uint32_t) variant <= INT32_MAX) {
				variant = (int32_t) variant;
				Compact();
			}
			break;
		}
		case V_UINT16:
		{
			Variant &variant = *this;
			if ((uint16_t) variant <= INT16_MAX) {
				variant = (int16_t) variant;
				Compact();
			}
			break;
		}
		case V_UINT8:
		{
			Variant &variant = *this;
			if ((uint8_t) variant <= INT8_MAX) {
				variant = (int8_t) variant;
				Compact();
			}
			break;
		}
		case V_MAP:
		case V_TYPED_MAP:
		{

			FOR_MAP(*this, string, Variant, i) {
				MAP_VAL(i).Compact();
			}
			break;
		}
		default:
		{
			break;
		}
	}
}

bool Variant::DeserializeFromBin(uint8_t *pBuffer, uint32_t bufferLength,
		Variant &variant) {
	uint32_t cursor = 0;
	return DeserializeFromBin(pBuffer, bufferLength, variant, cursor);
}

bool Variant::DeserializeFromBin(string &data, Variant &variant) {
	return DeserializeFromBin((uint8_t *) data.c_str(), data.size(), variant);
}

bool Variant::SerializeToBin(string &result) {
	result += string(1, (char) _type);
	switch (_type) {
		case V_NULL:
		case V_UNDEFINED:
		{
			return true;
		}
		case V_BOOL:
		{
			result += string(1, (char) _value.b);
			return true;
		}
		case V_INT8:
		{
			result += string(1, (char) _value.i8);
			return true;
		}
		case V_INT16:
		{
			int16_t val = htons(_value.i16); //----MARKED-SHORT----
			result += string((char *) & val, sizeof (int16_t));
			return true;
		}
		case V_INT32:
		{
			int32_t val = htonl(_value.i32); //----MARKED-LONG---
			result += string((char *) & val, sizeof (int32_t));
			return true;
		}
		case V_INT64:
		{
			int64_t val = htonll(_value.i64); //----MARKED-LONG---
			result += string((char *) & val, sizeof (int64_t));
			return true;
		}
		case V_UINT8:
		{
			result += string((char *) & _value.ui8, sizeof (uint8_t));
			return true;
		}
		case V_UINT16:
		{
			uint16_t val = htons(_value.ui16); //----MARKED-SHORT----
			result += string((char *) & val, sizeof (uint16_t));
			return true;
		}
		case V_UINT32:
		{
			uint32_t val = htonl(_value.ui32); //----MARKED-LONG---
			result += string((char *) & val, sizeof (uint32_t));
			return true;
		}
		case V_UINT64:
		{
			uint64_t val = htonll(_value.ui64); //----MARKED-LONG---
			result += string((char *) & val, sizeof (uint64_t));
			return true;
		}
		case V_DOUBLE:
		{
			uint64_t val = htonll(*((uint64_t *) & _value.d)); //----MARKED-LONG---
			result += string((char *) & val, sizeof (uint64_t));
			return true;
		}
		case V_TIMESTAMP:
		case V_DATE:
		case V_TIME:
		{
			uint64_t temp = htonll((uint64_t) timegm(_value.t)); //----MARKED-LONG---
			result += string((char *) & temp, sizeof (uint64_t));
			return true;
		}
		case V_BYTEARRAY:
		case V_STRING:
		{
			uint32_t length = htonl((uint32_t) _value.s->size()); //----MARKED-LONG---
			result += string((char *) & length, sizeof (uint32_t));
			result += *(_value.s);
			return true;
		}
		case V_MAP:
		case V_TYPED_MAP:
		{
			bool isArray = IsArray();
			result += string(1, (char) isArray);

			uint32_t length = 0;
			if (_type == V_TYPED_MAP) {
				length = htonl((uint32_t) _value.m->typeName.size()); //----MARKED-LONG---
				result += string((char *) & length, sizeof (uint32_t));
				result += _value.m->typeName;
			}

			length = htonl(MapSize()); //----MARKED-LONG---
			result += string((char *) & length, sizeof (uint32_t));

			FOR_MAP(*this, string, Variant, i) {
				length = htonl((uint32_t) MAP_KEY(i).size()); //----MARKED-LONG---
				result += string((char *) & length, sizeof (uint32_t));
				result += MAP_KEY(i);
				string temp = "";
				if (!MAP_VAL(i).SerializeToBin(temp)) {
					FATAL("Unable to serialize variant");
					result = "";
					return false;
				} else {
					result += temp;
				}
			}
			return true;
		}
		default:
		{
			result = "";
			FATAL("Invalid variant type: %d", _type);
			return false;
		}
	}
	return true;
}

bool Variant::DeserializeFromXml(const uint8_t *pBuffer, uint32_t bufferLength,
		Variant &variant) {
	variant.Reset();

	TiXmlDocument document;
	document.Parse((char *) pBuffer);
	if (document.Error()) {
		FATAL("Invalid XML file: Error id: %d; Error desc: %s; Row: %d; Col: %d",
				document.ErrorId(),
				document.ErrorDesc(),
				document.ErrorRow(),
				document.ErrorCol());
		return false;
	}

	if (!DeserializeFromXml(document.RootElement(), variant)) {
		variant.Reset();
		return false;
	}

	return true;
}

bool Variant::DeserializeFromXml(string data, Variant &result) {
	return DeserializeFromXml((const uint8_t *) data.c_str(), data.size(), result);
}

bool Variant::SerializeToXml(string &result, bool prettyPrint) {
	result = "";
	string name = "";
	TiXmlElement *pElement = SerializeToXmlElement(name);
	if (pElement == NULL) {
		FATAL("Unable to serialize variant to xml element");
		return false;
	}

	TiXmlDocument document;
	TiXmlDeclaration * pXmlDeclaration = new TiXmlDeclaration("1.0", "", "");
	document.LinkEndChild(pXmlDeclaration);
	document.LinkEndChild(pElement);

	if (prettyPrint) {
		TiXmlPrinter printer;
		document.Accept(&printer);
		result = printer.Str();
	} else {
		stringstream ss;
		ss << document;
		result = ss.str();
	}

	return true;
}

bool Variant::DeserializeFromBinFile(string path, Variant &variant) {
	//1. Open the file
	File file;
	if (!file.Initialize(path)) {
		FATAL("Unable to open file %s", STR(path));
		return false;
	}

	if (file.Size() >= 0x100000000LL) {
		FATAL("File too big");
		return false;
	}

	//2. Allocate memory
	uint8_t *pBuffer = new uint8_t[(uint32_t) file.Size()];

	//3. Read the content
	if (!file.ReadBuffer((uint8_t *) pBuffer, file.Size())) {
		FATAL("Unable to read the file");
		return false;
	}

	//4. Prepare the string
	string raw = string((char *) pBuffer, (uint32_t) file.Size());

	//8. Dispose the buffer
	delete[] pBuffer;

	//9. read the variant from the buffer
	variant.Reset();
	return DeserializeFromBin(raw, variant);
}

bool Variant::SerializeToBinFile(string fileName) {
	string rawContent = "";

	if (!SerializeToBin(rawContent)) {
		FATAL("Unable to serialize to XML");
		return false;
	}

	File file;
	if (!file.Initialize(fileName, true, false)) {
		FATAL("Unable to open file %s", STR(fileName));
		return false;
	}

	if (!file.WriteString(rawContent)) {
		FATAL("Unable to write content");
		return false;
	}

	return true;
}

bool Variant::DeserializeFromXmlFile(string path, Variant &variant) {
	//1. Open the file
	File file;
	if (!file.Initialize(path)) {
		FATAL("Unable to open file %s", STR(path));
		return false;
	}

	//2. Check his size
	if (file.Size() == 0) {
		variant.Reset();
		return true;
	}

	if (file.Size() > 1024 * 1024 * 4) {
		FATAL("File too large");
		return false;
	}

	//3. Allocate memory
	uint8_t *pBuffer = new uint8_t[(uint32_t) file.Size()];

	//4. Read the content
	if (!file.ReadBuffer((uint8_t *) pBuffer, file.Size())) {
		FATAL("Unable to read the file");
		return false;
	}

	//5. read the variant from the buffer
	variant.Reset();
	bool result = DeserializeFromXml(pBuffer, (uint32_t) file.Size(), variant);

	//8. Dispose the buffer
	delete[] pBuffer;

	return result;
}

bool Variant::SerializeToXmlFile(string fileName) {
	string rawContent = "";
	if (!SerializeToXml(rawContent, true)) {
		FATAL("Unable to serialize to XML");
		return false;
	}

	File file;
	if (!file.Initialize(fileName, true, false)) {
		FATAL("Unable to open file %s", STR(fileName));
		return false;
	}

	if (!file.WriteString(rawContent)) {
		FATAL("Unable to write content");
		return false;
	}

	return true;
}

TiXmlElement *Variant::SerializeToXmlElement(string &name) {
	TiXmlElement *pResult = NULL;
	switch (_type) {
		case V_NULL:
		{
			pResult = new TiXmlElement("NULL");
			break;
		}
		case V_UNDEFINED:
		{
			pResult = new TiXmlElement("UNDEFINED");
			break;
		}
		case V_BOOL:
		{
			pResult = new TiXmlElement("BOOL");
			pResult->LinkEndChild(new TiXmlText(_value.b ? "true" : "false"));
			break;
		}
		case V_INT8:
		{
			pResult = new TiXmlElement("INT8");
			pResult->LinkEndChild(new TiXmlText(format("%d", _value.i8)));
			break;
		}
		case V_INT16:
		{
			pResult = new TiXmlElement("INT16");
			pResult->LinkEndChild(new TiXmlText(format("%d", _value.i16)));
			break;
		}
		case V_INT32:
		{
			pResult = new TiXmlElement("INT32");
			pResult->LinkEndChild(new TiXmlText(format("%d", _value.i32)));
			break;
		}
		case V_INT64:
		{
			pResult = new TiXmlElement("INT64");
			pResult->LinkEndChild(new TiXmlText(format("%lld", _value.i64)));
			break;
		}
		case V_UINT8:
		{
			pResult = new TiXmlElement("UINT8");
			pResult->LinkEndChild(new TiXmlText(format("%u", _value.ui8)));
			break;
		}
		case V_UINT16:
		{
			pResult = new TiXmlElement("UINT16");
			pResult->LinkEndChild(new TiXmlText(format("%u", _value.ui16)));
			break;
		}
		case V_UINT32:
		{
			pResult = new TiXmlElement("UINT32");
			pResult->LinkEndChild(new TiXmlText(format("%u", _value.ui32)));
			break;
		}
		case V_UINT64:
		{
			pResult = new TiXmlElement("UINT64");
			pResult->LinkEndChild(new TiXmlText(format("%llu", _value.ui64)));
			break;
		}
		case V_DOUBLE:
		{
			pResult = new TiXmlElement("DOUBLE");
			pResult->LinkEndChild(new TiXmlText(format("%.03f", _value.d)));
			break;
		}
		case V_TIMESTAMP:
		{
			pResult = new TiXmlElement("TIMESTAMP");
			pResult->LinkEndChild(new TiXmlText(STR(*this)));
			break;
		}
		case V_DATE:
		{
			pResult = new TiXmlElement("DATE");
			pResult->LinkEndChild(new TiXmlText(STR(*this)));
			break;
		}
		case V_TIME:
		{
			pResult = new TiXmlElement("TIME");
			pResult->LinkEndChild(new TiXmlText(STR(*this)));
			break;
		}
		case V_STRING:
		{
			pResult = new TiXmlElement("STR");
			pResult->LinkEndChild(new TiXmlText(STR(*_value.s)));
			break;
		}
		case V_BYTEARRAY:
		{
			pResult = new TiXmlElement("BYTEARRAY");
			pResult->LinkEndChild(new TiXmlText(STR(b64(*_value.s))));
			break;
		}
		case V_TYPED_MAP:
		case V_MAP:
		{
			if (_type == V_MAP) {
				pResult = new TiXmlElement("MAP");
			} else {
				pResult = new TiXmlElement("TYPED_MAP");
				pResult->SetAttribute("typeName", _value.m->typeName);
			}
			pResult->SetAttribute("isArray", _value.m->isArray ? "true" : "false");

			FOR_MAP(_value.m->children, string, Variant, i) {
				string key = MAP_KEY(i);
				TiXmlElement *pElement = MAP_VAL(i).SerializeToXmlElement(key);
				if (pElement == NULL) {
					delete pResult;
					pResult = NULL;
					break;
				}
				pResult->LinkEndChild(pElement);
			}

			break;
		}
		default:
		{
			ASSERT("Invalid type: %d", _type);
			return NULL;
		}
	}
	if (pResult != NULL) {
		pResult->SetAttribute("name", name);
	}
	return pResult;
}

#define VARIANT_CHECK_BOUNDS(s) \
do {\
	if(s>bufferSize-cursor) \
	{ \
		FATAL("Not enough data. Wanted: %u; Got: %u",s,bufferSize-cursor);\
		return false; \
	} \
}\
while(0)

#define PTR (pBuffer+cursor)

bool Variant::DeserializeFromBin(uint8_t *pBuffer, uint32_t bufferSize,
		Variant &variant, uint32_t &cursor) {
	VARIANT_CHECK_BOUNDS(1);
	VariantType type = (VariantType) PTR[0];
	cursor += 1;

	switch (type) {
		case V_NULL:
		{
			variant.Reset();
			return true;
		}
		case V_UNDEFINED:
		{
			variant.Reset(true);
			return true;
		}
		case V_BOOL:
		{
			VARIANT_CHECK_BOUNDS(1);
			variant = (bool)(PTR[0] != 0);
			cursor += 1;
			return true;
		}
		case V_INT8:
		{
			VARIANT_CHECK_BOUNDS(1);
			variant = *((int8_t *) PTR);
			cursor += 1;
			return true;
		}
		case V_INT16:
		{
			VARIANT_CHECK_BOUNDS(2);
			uint16_t val = ntohsp(PTR); //----MARKED-SHORT----
			cursor += 2;
			variant = *((int16_t *) & val);
			return true;
			break;
		}
		case V_INT32:
		{
			VARIANT_CHECK_BOUNDS(4);
			uint32_t val = ntohlp(PTR); //----MARKED-LONG---
			cursor += 4;
			variant = *((int32_t *) & val);
			return true;
		}
		case V_INT64:
		{
			VARIANT_CHECK_BOUNDS(8);
			uint64_t val = ntohlp(PTR); //----MARKED-LONG---
			cursor += 8;
			variant = *((int64_t *) & val);
			return true;
		}
		case V_UINT8:
		{
			VARIANT_CHECK_BOUNDS(1);
			variant = *((uint8_t *) PTR);
			cursor += 1;
			return true;
		}
		case V_UINT16:
		{
			VARIANT_CHECK_BOUNDS(2);
			variant = ntohsp(PTR); //----MARKED-SHORT----
			cursor += 2;
			return true;
		}
		case V_UINT32:
		{
			VARIANT_CHECK_BOUNDS(4);
			variant = ntohlp(PTR); //----MARKED-LONG---
			cursor += 4;
			return true;
		}
		case V_UINT64:
		{
			VARIANT_CHECK_BOUNDS(8);
			variant = (uint64_t) ntohllp(PTR); //----MARKED-LONG---
			cursor += 8;
			return true;
		}
		case V_DOUBLE:
		{
			VARIANT_CHECK_BOUNDS(8);
			uint64_t val = ntohllp(PTR); //----MARKED-LONG---
			cursor += 8;
			variant = *((double *) & val);
			return true;
		}
		case V_TIMESTAMP:
		case V_DATE:
		case V_TIME:
		{
			VARIANT_CHECK_BOUNDS(8);
			time_t val = ntohllp(PTR); //----MARKED-LONG---
			cursor += 8;
			variant = *((Timestamp *) gmtime(&val));
			variant._type = type;
			return true;
		}
		case V_BYTEARRAY:
		case V_STRING:
		{
			VARIANT_CHECK_BOUNDS(4);
			uint32_t length = ntohlp(PTR); //----MARKED-LONG---
			cursor += 4;
			VARIANT_CHECK_BOUNDS(length);
			if (length > 1024 * 128) {
				FATAL("string too large");
				return false;
			}
			variant = string((char *) PTR, length);
			cursor += length;
			variant.IsByteArray(type == V_BYTEARRAY);
			return true;
		}
		case V_MAP:
		case V_TYPED_MAP:
		{
			VARIANT_CHECK_BOUNDS(1);
			bool isArray = (PTR[0] != 0);
			cursor += 1;
			variant.IsArray(isArray);

			uint32_t length = 0;
			if (type == V_TYPED_MAP) {
				VARIANT_CHECK_BOUNDS(4);
				length = ntohlp(PTR); //----MARKED-LONG---
				cursor += 4;
				VARIANT_CHECK_BOUNDS(length);
				if (length > 1024 * 128) {
					FATAL("string too large");
					return false;
				}
				string name = string((char *) PTR, length);
				cursor += length;
				variant.SetTypeName(name);
			}

			VARIANT_CHECK_BOUNDS(4);
			length = ntohlp(PTR); //----MARKED-LONG---
			if (length > 1024) {
				FATAL("Length too large");
				return false;
			}
			cursor += 4;

			for (uint32_t i = 0; i < length; i++) {
				string key;
				uint32_t keyLength;

				VARIANT_CHECK_BOUNDS(4);
				keyLength = ntohlp(PTR); //----MARKED-LONG---
				cursor += 4;
				VARIANT_CHECK_BOUNDS(keyLength);
				if (keyLength > 1024 * 128) {
					FATAL("string too large");
					return false;
				}
				key = string((char *) PTR, keyLength);
				cursor += keyLength;

				if (!DeserializeFromBin(pBuffer, bufferSize, variant[key], cursor)) {
					FATAL("Unable to deserialize variant");
					return false;
				}
			}
			return true;
		}
		default:
		{
			FATAL("Invalid variant type: %d", type);
			return false;
		}
	}
}

bool Variant::DeserializeFromXml(TiXmlElement *pNode, Variant &variant) {
	string nodeName = lowercase(pNode->ValueStr());

	union {
		int64_t i64;
		uint64_t ui64;
		double d;
		Timestamp t;
	} val;

	const char *pText = pNode->GetText();
	string text = pText == NULL ? "" : pText;

	if (nodeName == "bool") {
		variant = (bool)(lowercase(text) == "true");
		return true;
	} else if (nodeName == "null") {
		variant.Reset();
		return true;
	} else if (nodeName == "undefined") {
		variant.Reset(true);
		return true;
	} else if (nodeName == "int8") {
		if (sscanf(STR(text), "%lld", (long long int *) & val.i64) != 1) {
			FATAL("Invalid number");
			return false;
		}
		variant = (int8_t) val.i64;
		return true;
	} else if (nodeName == "int16") {
		if (sscanf(STR(text), "%lld", (long long int *) & val.i64) != 1) {
			FATAL("Invalid number");
			return false;
		}
		variant = (int16_t) val.i64;
		return true;
	} else if (nodeName == "int32") {
		if (sscanf(STR(text), "%lld", (long long int *) & val.i64) != 1) {
			FATAL("Invalid number");
			return false;
		}
		variant = (int32_t) val.i64;
		return true;
	} else if (nodeName == "int64") {
		if (sscanf(STR(text), "%lld", (long long int *) & val.i64) != 1) {
			FATAL("Invalid number");
			return false;
		}
		variant = (int64_t) val.i64;
		return true;
	} else if (nodeName == "uint8") {
		if (sscanf(STR(text), "%llu", (long long unsigned int *) & val.ui64) != 1) {
			FATAL("Invalid number");
			return false;
		}
		variant = (uint8_t) val.ui64;
		return true;
	} else if (nodeName == "uint16") {
		if (sscanf(STR(text), "%llu", (long long unsigned int *) & val.ui64) != 1) {
			FATAL("Invalid number");
			return false;
		}
		variant = (uint16_t) val.ui64;
		return true;
	} else if (nodeName == "uint32") {
		if (sscanf(STR(text), "%llu", (long long unsigned int *) & val.ui64) != 1) {
			FATAL("Invalid number");
			return false;
		}
		variant = (uint32_t) val.ui64;
		return true;
	} else if (nodeName == "uint64") {
		if (sscanf(STR(text), "%llu", (long long unsigned int *) & val.ui64) != 1) {
			FATAL("Invalid number");
			return false;
		}
		variant = (uint64_t) val.ui64;
		return true;
	} else if (nodeName == "double") {
		if (sscanf(STR(text), "%lf", &val.d) != 1) {
			FATAL("Invalid number");
			return false;
		}
		variant = (double) val.d;
		return true;
	} else if (nodeName == "timestamp") {
		if (strptime(STR(text), "%Y-%m-%dT%H:%M:%S.000", &val.t) == NULL) {
			FATAL("Invalid timestamp (date, time or timestamp)");
			return false;
		}
		variant = (Timestamp) val.t;
		variant._type = V_TIMESTAMP;
		return true;
	} else if (nodeName == "date") {
		if (strptime(STR(text), "%Y-%m-%d", &val.t) == NULL) {
			FATAL("Invalid timestamp (date, time or timestamp)");
			return false;
		}
		variant = (Timestamp) val.t;
		variant._type = V_DATE;
		return true;
	} else if (nodeName == "time") {
		if (strptime(STR(text), "%H:%M:%S.000", &val.t) == NULL) {
			FATAL("Invalid timestamp (date, time or timestamp)");
			return false;
		}
		variant = (Timestamp) val.t;
		variant._type = V_TIME;
		return true;
	} else if (nodeName == "str") {
		variant = text;
		return true;
	} else if (nodeName == "bytearray") {
		variant = unb64(text);
		variant.IsByteArray(true);
		return true;
	} else if (nodeName == "map" || nodeName == "typed_map") {
		TiXmlAttribute *pAttribute = pNode->FirstAttribute();

		//isArray and typename
		variant.IsArray(false);
		for (TiXmlAttribute *pI = pAttribute; pI != NULL; pI = pI->Next()) {
			if (lowercase(pI->NameTStr()) == "isarray") {
				variant.IsArray(lowercase(pI->ValueStr()) == "true");
			} else if (lowercase(pI->NameTStr()) == "typename") {
				variant.SetTypeName(pI->Value());
			}
		}

		//childs
		for (TiXmlElement *pE = pNode->FirstChildElement(); pE != NULL; pE = pE->NextSiblingElement()) {
			//search for the name
			string key = "";
			for (TiXmlAttribute *pA = pE->FirstAttribute(); pA != NULL; pA = pA->Next()) {
				if (lowercase(pA->NameTStr()) == "name") {
					key = pA->ValueStr();
					break;
				}
			}

			if (!DeserializeFromXml(pE, variant[key])) {
				FATAL("Unable to deserialize element: %s", STR(key));
				return false;
			}
		}

		return true;
	} else {
		FATAL("Invalid node name: %s", STR(nodeName));

		return false;
	}
}

void Variant::InternalCopy(const Variant &val) {
	_type = val._type;
	memset(&_value, 0, sizeof (_value));
	switch (val._type) {
		case V_DATE:
		case V_TIME:
		case V_TIMESTAMP:
		{
			DYNAMIC_ALLOC("_value.t");
			_value.t = new Timestamp(*val._value.t);
			break;
		}
		case V_BYTEARRAY:
		case V_STRING:
		{
			DYNAMIC_ALLOC("_value.s");
			_value.s = new string(*val._value.s);
			break;
		}
		case V_MAP:
		case V_TYPED_MAP:
		{
			DYNAMIC_ALLOC("_value.m");
			_value.m = new VariantMap(*val._value.m);
			break;
		}
		default:
		{
			memcpy(&_value, &val._value, sizeof (_value));
			break;
		}
	}
}

void Variant::NormalizeTs() {
	time_t val = timegm(_value.t);
	if (val < 0) {
		val = 0;
	}
	gmtime_r(&val, _value.t);
	//    FINEST("tm_sec: %d;\n"
	//            "tm_min:%d;\n"
	//            "tm_hour:%d;\n"
	//            "tm_mday:%d;\n"
	//            "tm_mon:%d;\n"
	//            "tm_year:%d;\n"
	//            "tm_wday:%d;\n"
	//            "tm_yday:%d;\n"
	//            "tm_isdst:%d;\n"
	//            "tm_gmtoff:%d;\n"
	//            "*tm_zone:%s;\n",
	//            _value.t->tm_sec,
	//            _value.t->tm_min,
	//            _value.t->tm_hour,
	//            _value.t->tm_mday,
	//            _value.t->tm_mon,
	//            _value.t->tm_year,
	//            _value.t->tm_wday,
	//            _value.t->tm_yday,
	//            _value.t->tm_isdst,
	//            _value.t->tm_gmtoff,
	//            _value.t->tm_zone);
}

