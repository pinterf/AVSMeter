COMPILER_OPTIONS=/MT /W3 /EHa /O2 /Ob2 /GL

INTDIR=.\Intermediate32
ALL : "AVSMeter.exe"
CPP=@cl.exe
CPP_PROJ=/nologo $(COMPILER_OPTIONS) /D "WIN32" /D "_CRT_SECURE_NO_WARNINGS" /D "NDEBUG" /D WINVER=0x0501 /D _WIN32_WINNT=0x0501 /D "_CONSOLE" /Fo"$(INTDIR)\\" /c
LINK=@link.exe
LINK_FLAGS=source\cpuid\libcpuid32.lib Shell32.lib comdlg32.lib psapi.lib imagehlp.lib version.lib user32.lib advapi32.lib /nologo /subsystem:console /incremental:no /machine:x86 /out:"AVSMeter.exe" /LARGEADDRESSAWARE /LTCG
RSC=@rc.exe
RSC_PROJ=/nologo /l 0x809 /fo"$(INTDIR)\AVSMeter.res" /d "NDEBUG" 

"$(INTDIR)" :
	@if not exist "$(INTDIR)/$(NULL)" mkdir "$(INTDIR)"

LINK_OBJS= \
	"$(INTDIR)\AVSMeter.obj" \
	"$(INTDIR)\AVSMeter.res"

"AVSMeter.exe" : $(LINK_OBJS)
  $(LINK) @<<
  $(LINK_FLAGS) $(LINK_OBJS)
<<

SOURCE=.\source\AVSMeter.cpp

"$(INTDIR)\AVSMeter.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)

SOURCE=.\source\AVSMeter.rc

"$(INTDIR)\AVSMeter.res" : $(SOURCE) "$(INTDIR)"
	$(RSC) $(RSC_PROJ) $(SOURCE)

