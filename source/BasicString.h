#ifndef _BASICSTRING_H
#define _BASICSTRING_H

#include <windows.h>
#include <string>

using namespace std;

class CBasicString
{
public:
	CBasicString();
	virtual ~CBasicString();
	void StrTrimLeft(string &s_string);
	void StrTrimRight(string &s_string);
	void StrTrim(string &s_string);
	void StrToUC(string &s_string);
	void StrToLC(string &s_string);
	string StrFormat(const char *fmt, ...);
	string FormatTimeString(unsigned int ui_milliseconds);
	string StrPad(string cs_line, unsigned int ui_consolewidth);
};

CBasicString::CBasicString()
{
}

CBasicString::~CBasicString()
{
}

void CBasicString::StrTrimLeft(string &s_string)
{
	s_string.erase(0, s_string.find_first_not_of(" \t\n\r"));

	return;
}


void CBasicString::StrTrimRight(string &s_string)
{
	s_string.erase(s_string.find_last_not_of(" \t\n\r") + 1);

	return;
}


void CBasicString::StrTrim(string &s_string)
{
	s_string.erase(s_string.find_last_not_of(" \t\n\r") + 1);
	s_string.erase(0, s_string.find_first_not_of(" \t\n\r"));

	return;
}


void CBasicString::StrToUC(string &s_string)
{
	for (unsigned int i = 0; i < s_string.length(); i++)
		s_string[i] = toupper(s_string[i]);

	return;
}


void CBasicString::StrToLC(string &s_string)
{
	for (unsigned int i = 0; i < s_string.length(); i++)
		s_string[i] = tolower(s_string[i]);

	return;
}


string CBasicString::StrFormat(const char *fmt, ...)
{
	if (!fmt)	return "";

	#define MAX_LEN 2048
	va_list args;
	va_start(args, fmt);
	unsigned int length = 256;
	char *buffer = NULL;
	int result = -1;

	while (result == -1)
	{
		if (buffer)
			delete [] buffer;

		buffer = new char [length];
		memset(buffer, ' ', sizeof(buffer));
		result = _vsnprintf(buffer, length, fmt, args);
		length *= 2;
		if (length > MAX_LEN)
		{
			delete [] buffer;
			va_end(args);
			return "";
		}
	}

	string str(buffer);
	delete [] buffer;
	va_end(args);

	return str;
}


string CBasicString::StrPad(string cs_line, unsigned int ui_consolewidth)
{
	int iPadLen = (int)ui_consolewidth - (int)cs_line.length();

	if ((iPadLen <= 0) || (iPadLen > 1000))
		return cs_line;

	string s_padding((iPadLen - 1), ' ');

	return (cs_line + s_padding);
}


string CBasicString::FormatTimeString(unsigned int ui_milliseconds)
{
	unsigned int hours = ui_milliseconds / 3600000;
	unsigned int minutes = (ui_milliseconds / 60000) % 60;
	unsigned int seconds = (ui_milliseconds / 1000) % 60;
	unsigned int milliseconds = ui_milliseconds % 1000;

	return StrFormat("%03d:%02d:%02d.%03d", hours, minutes, seconds, milliseconds);
}

#endif //_BASICSTRING_H

