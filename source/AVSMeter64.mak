COMPILER_OPTIONS=/MT /W3 /EHa /O1 /Ob2 /GL

INTDIR=.\Intermediate
ALL : "AVSMeter.exe"
CPP=@cl.exe
CPP_PROJ=/nologo $(COMPILER_OPTIONS) /D "AVS64" /D "WIN32" /D "_CRT_SECURE_NO_WARNINGS" /D "NDEBUG" /D WINVER=0x0501 /D _WIN32_WINNT=0x0501 /D "_CONSOLE" /Fo"$(INTDIR)\\" /c
LINK=@link.exe
LINK_FLAGS=source\cpuid\libcpuid64.lib psapi.lib imagehlp.lib version.lib user32.lib advapi32.lib /nologo /subsystem:console /incremental:no /machine:x64 /out:"AVSMeter64.exe" /LTCG
RSC=@rc.exe
RSC_PROJ=/nologo /l 0x809 /fo"$(INTDIR)\AVSMeter.res" /d "NDEBUG" 

"$(INTDIR)" :
	@if not exist "$(INTDIR)/$(NULL)" mkdir "$(INTDIR)"

LINK_OBJS= \
	"$(INTDIR)\AvisynthInfo.obj" \
	"$(INTDIR)\GPUInfo.obj" \
	"$(INTDIR)\ProcessInfo.obj" \
	"$(INTDIR)\Timer.obj" \
	"$(INTDIR)\Utility.obj" \
	"$(INTDIR)\SysInfo.obj" \
	"$(INTDIR)\AVSMeter.obj" \
	"$(INTDIR)\AVSMeter.res"

"AVSMeter.exe" : $(LINK_OBJS)
  $(LINK) @<<
  $(LINK_FLAGS) $(LINK_OBJS)
<<

SOURCE=.\source\AvisynthInfo.cpp

"$(INTDIR)\AvisynthInfo.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)

SOURCE=.\source\GPUInfo.cpp

"$(INTDIR)\GPUInfo.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)

SOURCE=.\source\ProcessInfo.cpp

"$(INTDIR)\ProcessInfo.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)

SOURCE=.\source\Timer.cpp

"$(INTDIR)\Timer.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)

SOURCE=.\source\Utility.cpp

"$(INTDIR)\Utility.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)

SOURCE=.\source\SysInfo.cpp

"$(INTDIR)\SysInfo.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)

SOURCE=.\source\AVSMeter.cpp

"$(INTDIR)\AVSMeter.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)

SOURCE=.\source\AVSMeter.rc

"$(INTDIR)\AVSMeter.res" : $(SOURCE) "$(INTDIR)"
	$(RSC) $(RSC_PROJ) $(SOURCE)

