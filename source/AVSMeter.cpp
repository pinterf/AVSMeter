#include <windows.h>
#include <string>
#include <conio.h>
#include "avisynth.h"

#define version "1.21"

double GetTimer();
BOOL TestTimer();
std::string pad(std::string s_line);
std::string format(const char *fmt, ...);
std::string FormatHMS(unsigned int &ui_milliseconds);
BOOL FileExists(std::string sFile);
static unsigned int uiConsoleWidth = 0;

int main(int argc, char* argv[], char* envp[])
{
	printf("\nAVSMeter v%s by Groucho2004\n", version);


	CONSOLE_SCREEN_BUFFER_INFO csbi; 
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE); 
	GetConsoleScreenBufferInfo(hConsole, &csbi); 
	uiConsoleWidth = (unsigned int)csbi.dwSize.X;
	std::string sConsoleOut = "";

	HINSTANCE hDLL;
	hDLL = LoadLibrary("avisynth");
	if (!hDLL)
	{
		printf("\nCannot load avisynth.dll\n");
		return -1;
	}

	try 
	{
		IScriptEnvironment* (__stdcall *CreateEnv)(int);
		IScriptEnvironment *env;

		CreateEnv = (IScriptEnvironment *(__stdcall *)(int))GetProcAddress(hDLL, "CreateScriptEnvironment");
		if (!CreateEnv)
		{
			printf("\n%s\n", pad("Failed to load CreateScriptEnvironment()").c_str());
			return -1;
		}
	
		env = CreateEnv(AVISYNTH_INTERFACE_VERSION);
		if (!env)
		{
			printf("\n%s\n", pad("Could not create IScriptenvironment").c_str());
			return -1;
		}

		AVSValue val;
		PClip clip;
		PVideoFrame frame;
		VideoInfo	vidinfo;

		std::string sAvisynthVersion = "";
		try
		{
			AVSValue as_version;
			as_version = env->Invoke("VersionString", AVSValue(&as_version, 0));
			sAvisynthVersion = as_version.AsString();
		}
		catch (IScriptEnvironment::NotFound)
		{
		}

		printf("\r%s\n", pad(sAvisynthVersion).c_str());

		if (argc != 2)
		{
			frame = 0;
			clip = 0;
			val = 0;

			printf("\nUsage :  AVSMeter <script>\n");
			return -1;
		}

		if (!FileExists(argv[1]))
		{
			frame = 0;
			clip = 0;
			val = 0;

			printf("File not found:\n%s\n", argv[1]);
			return -1;
		}

		val = env->Invoke("Import", argv[1]);
		printf("\rChecking timer...");
		if ((GetTimer() < 0.0) || (!TestTimer()))
		{
			sConsoleOut = "";
			printf("\r%s", pad(sConsoleOut).c_str());

			printf("\nHigh resolution timer not supported\n");
			return -1;
		}

		sConsoleOut = "";
		printf("\r%s", pad(sConsoleOut).c_str());

		BOOL bIsMTVersion = TRUE;
		unsigned int uiMTMode = 0;
		try
		{
			AVSValue mt_test;
			mt_test = env->Invoke("GetMTMode", false);
			const int mt_mode = mt_test.IsInt() ? mt_test.AsInt() : 0;
			uiMTMode = mt_mode;
			if (mt_mode > 0 && mt_mode < 5)
				val = env->Invoke("Distributor", val);
		}
		catch (IScriptEnvironment::NotFound)
		{
			bIsMTVersion = FALSE;
		}

		clip = val.AsClip();
		vidinfo = clip->GetVideoInfo();
		unsigned int uiFrames = (unsigned int)vidinfo.num_frames;

		unsigned int uiMilliSeconds = (unsigned int)((((double)uiFrames * (double)vidinfo.fps_denominator * 1000.0) / (double)vidinfo.fps_numerator) + 0.5);

		if (bIsMTVersion)
			printf("Active MT Mode: %u\n", uiMTMode);

		printf("\nNumber of frames:     %11u", vidinfo.num_frames);
		printf("\nLength (h:m:s.ms):       %s", FormatHMS(uiMilliSeconds).c_str());
		printf("\nFrame width:          %11u", vidinfo.width);
		printf("\nFrame height:         %11u", vidinfo.height);
		printf("\nFramerate:                %11.3f (%u/%u)", (double)vidinfo.fps_numerator / (double)vidinfo.fps_denominator, vidinfo.fps_numerator, vidinfo.fps_denominator);

		if (!vidinfo.IsFieldBased())
			printf("\nProgressive:                  Yes");
		else
		{
			printf("\nProgressive:                   No");
			if (vidinfo.IsBFF())
				printf(" (BFF)");
			if (vidinfo.IsTFF())
				printf(" (TFF)");
		}

		printf("\nColorspace:                 ");

		if (vidinfo.IsYV12())
			printf(" YV12\n");
		else if (vidinfo.IsYUY2())
			printf(" YUY2\n");
		else if (vidinfo.IsRGB24())
			printf("RGB24\n");
		else if (vidinfo.IsRGB32())
			printf("RGB32\n");
		else
			printf("  n/a\n");

		printf("\nHit ESC to exit...\n");

		unsigned int uiCurrentFrame = 0;
		double dFPSAverage = 0.0;
		double dFPSCurrent = 0.0;
		double dFPSMax = 0.0;
		double dFPSMin = 1.0E100;
		unsigned int uiFrameWindow = 20;

		//This seems to remedy some odd caching behaviour with MT versions when "Loop()" is used in the script.
		if (uiFrames > (2 * uiFrameWindow))
		{
			frame = clip->GetFrame(0, env);
			frame = clip->GetFrame((2 * uiFrameWindow), env);
			frame = clip->GetFrame(0, env);
		}

		double dstarttime = GetTimer();
		double dcurrenttime = GetTimer();
		double dprevioustime = GetTimer();
		double dlastwindowtime = GetTimer();
		unsigned int n = 0;

		for (uiCurrentFrame = 0; uiCurrentFrame < uiFrames; uiCurrentFrame++)
		{
			frame = clip->GetFrame(uiCurrentFrame, env);
	
			n = uiCurrentFrame + 1;
			dcurrenttime = GetTimer();

			if ((n % uiFrameWindow) == 0)
			{
				dFPSCurrent = (double)uiFrameWindow / (dcurrenttime - dlastwindowtime);

				if (dFPSCurrent > dFPSMax)
					dFPSMax = dFPSCurrent;
				if (dFPSCurrent < dFPSMin)
					dFPSMin = dFPSCurrent;			

				dlastwindowtime = dcurrenttime;
			}

			if (((dcurrenttime - dprevioustime) >= 0.25) || (n == uiFrames))
			{
				dprevioustime = dcurrenttime;
				dFPSAverage = (double)n / (dcurrenttime - dstarttime);

				if (n >= (2 * uiFrameWindow))
				{
					sConsoleOut = format("Frame %u/%u, fps (min/max/avg): %.2f | %.2f | %.2f", n, uiFrames, dFPSMin, dFPSMax, dFPSAverage);
					printf("\r%s", pad(sConsoleOut).c_str());
				}
				else
				{
					sConsoleOut = format("Frame %u/%u, fps (min/max/avg): ... | ... | ...", n, uiFrames);
					printf("\r%s", pad(sConsoleOut).c_str());
				}

				if (_kbhit())
				{
					if (_getch() == 0x1B) //ESC
						break;
				}
			}
		}

		frame = 0;
		clip = 0;
		val = 0;

		unsigned int uiTotalMS = (unsigned int)(((dcurrenttime - dstarttime) * 1000.0) + 0.5);
		sConsoleOut = format("Running time (h:m:s.ms):  %s", FormatHMS(uiTotalMS).c_str());
		printf("\n%s\n", sConsoleOut.c_str());
	}
	catch(AvisynthError err) 
	{
		printf("\r%s\n", err.msg);
		return FALSE;
	}

	FreeLibrary(hDLL);

	return 0;
}


BOOL FileExists(std::string sFile)
{
	DWORD dwAttrib;
	dwAttrib = GetFileAttributes(sFile.c_str());
	if (dwAttrib == 0xFFFFFFFF)
		return FALSE;

	if (dwAttrib & FILE_ATTRIBUTE_DIRECTORY)
		return FALSE;	

	return TRUE;
}


std::string format(const char *fmt, ...)
{
	if (!fmt)	return "";

	va_list args;
	va_start(args, fmt);
	unsigned int length = 512;
	char *buffer = NULL;
	int result = -1;

	while (result == -1)
	{
		if (buffer) delete [] buffer;
		buffer = new char [length + 1];
		memset(buffer, ' ', length + 1);
		result = _vsnprintf(buffer, length, fmt, args);
		length *= 2;
	}

	std::string s(buffer);
	delete [] buffer;

	va_end(args);

	return s;
}


std::string pad(std::string s_line)
{
	std::string s_padding(((uiConsoleWidth - s_line.length()) - 1), ' ');
	return (s_line + s_padding);
}


double GetTimer()
{
	LARGE_INTEGER liPerfCounter = {0, 0};
	LARGE_INTEGER liPerfFreq = {0, 0};

	if (QueryPerformanceCounter(&liPerfCounter) == 0)
		return -1.0;
	if (QueryPerformanceFrequency(&liPerfFreq) == 0)
		return -1.0;

	return (double)liPerfCounter.QuadPart / (double)liPerfFreq.QuadPart;
}


std::string FormatHMS(unsigned int &ui_milliseconds)
{
	unsigned int hours = (ui_milliseconds / 1000) / 3600;
	unsigned int minutes = ((ui_milliseconds / 1000) / 60) % 60;
	unsigned int seconds = (ui_milliseconds / 1000) % 60;
	unsigned int milliseconds = ui_milliseconds % 1000;

	return format("%02d:%02d:%02d.%03d", hours, minutes, seconds, milliseconds);
}

BOOL TestTimer()
{
	BOOL bRet = FALSE;

	double start = 0.0;
	double dtime = 0.0;
	start = GetTimer();
	Sleep(1000);
	dtime = GetTimer() - start;
		
	if ((dtime > 0.90) && (dtime < 1.1))
		bRet = TRUE;

	return bRet;
}

