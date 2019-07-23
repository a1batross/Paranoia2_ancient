# Microsoft Developer Studio Project File - Name="cl_dll" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=cl_dll - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "cl_dll.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "cl_dll.mak" CFG="cl_dll - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "cl_dll - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "cl_dll - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""$/GoldSrc/cl_dll", HGEBAAAA"
# PROP Scc_LocalPath "."
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "cl_dll - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\temp\client\!release"
# PROP Intermediate_Dir "..\temp\client\!release"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /W3 /GX /Ox /Ot /Ow /Og /Oi /Op /I "..\utils\vgui\include" /I "..\engine" /I "..\common" /I "..\pm_shared" /I "..\dlls" /I ".\render" /I ".\\" /I "..\game_shared" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "CLIENT_DLL" /D "CLIENT_WEAPONS" /FD /c
# SUBTRACT CPP /Fr /YX
# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 msvcrt.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib winmm.lib ../utils/vgui/lib/win32_vc6/vgui.lib wsock32.lib /nologo /subsystem:windows /dll /pdb:none /machine:I386 /nodefaultlib:"libc.lib" /out:"..\temp\client\!release\client.dll"
# SUBTRACT LINK32 /map /debug
# Begin Custom Build
TargetDir=\Paranoia2\src_main\temp\client\!release
InputPath=\Paranoia2\src_main\temp\client\!release\client.dll
SOURCE="$(InputPath)"

"D:\Paranoia2\base\bin\client.dll" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetDir)\client.dll "D:\Paranoia2\base\bin\client.dll"

# End Custom Build

!ELSEIF  "$(CFG)" == "cl_dll - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "cl_dll___Win32_Debug"
# PROP BASE Intermediate_Dir "cl_dll___Win32_Debug"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\temp\client\!debug"
# PROP Intermediate_Dir "..\temp\client\!debug"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /Zi /O2 /I "..\utils\vgui\include" /I "..\engine" /I "..\common" /I "..\pm_shared" /I "..\dlls" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "CLIENT_DLL" /D "CLIENT_WEAPONS" /Fr /YX /FD /c
# ADD CPP /nologo /MTd /W3 /Gi /GX /ZI /Od /I "..\utils\vgui\include" /I "..\engine" /I "..\common" /I "..\pm_shared" /I "..\dlls" /I ".\render" /I ".\\" /I "..\game_shared" /D "WIN32" /D "DEBUG" /D "_DEBUG" /D "_WINDOWS" /D "CLIENT_DLL" /D "CLIENT_WEAPONS" /FAs /FR /FD /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /D "DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "_DEBUG" /d "DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib winmm.lib ../utils/vgui/lib/win32_vc6/vgui.lib wsock32.lib opengl32.lib cg.lib cgGL.lib /nologo /subsystem:windows /dll /map /debug /machine:I386 /out:".\Release\client.dll"
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib winmm.lib ../utils/vgui/lib/win32_vc6/vgui.lib wsock32.lib /nologo /subsystem:windows /dll /incremental:yes /debug /machine:I386 /out:"..\temp\client\!debug\client.dll"
# SUBTRACT LINK32 /map
# Begin Custom Build
TargetDir=\Paranoia2\src_main\temp\client\!debug
InputPath=\Paranoia2\src_main\temp\client\!debug\client.dll
SOURCE="$(InputPath)"

"D:\Paranoia2\base\bin\client.dll" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetDir)\client.dll "D:\Paranoia2\base\bin\client.dll"

# End Custom Build

!ENDIF 

# Begin Target

# Name "cl_dll - Win32 Release"
# Name "cl_dll - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat;for;f90"
# Begin Group "hl"

# PROP Default_Filter "*.CPP"
# Begin Source File

SOURCE=..\game_shared\common.cpp
# End Source File
# Begin Source File

SOURCE=.\render\gl_framebuffer.cpp
# End Source File
# Begin Source File

SOURCE=.\render\gl_frustum.cpp
# End Source File
# Begin Source File

SOURCE=.\render\gl_shader.cpp
# End Source File
# Begin Source File

SOURCE=.\render\gl_world.cpp
# End Source File
# Begin Source File

SOURCE=.\hl\hl_events.cpp
# End Source File
# Begin Source File

SOURCE=.\hl\hl_objects.cpp
# End Source File
# Begin Source File

SOURCE=.\hl\hl_weapons.cpp
# End Source File
# Begin Source File

SOURCE=..\game_shared\mathlib.cpp
# End Source File
# Begin Source File

SOURCE=..\game_shared\matrix.cpp
# End Source File
# Begin Source File

SOURCE=..\pm_shared\pm_shared.cpp
# End Source File
# Begin Source File

SOURCE=..\game_shared\stringlib.cpp
# End Source File
# Begin Source File

SOURCE=..\game_shared\vgui_checkbutton2.cpp
# End Source File
# Begin Source File

SOURCE=..\game_shared\vgui_grid.cpp
# End Source File
# Begin Source File

SOURCE=..\game_shared\vgui_helpers.cpp
# End Source File
# Begin Source File

SOURCE=..\game_shared\vgui_listbox.cpp
# End Source File
# Begin Source File

SOURCE=..\game_shared\vgui_loadtga.cpp
# End Source File
# Begin Source File

SOURCE=..\game_shared\vgui_scrollbar2.cpp
# End Source File
# Begin Source File

SOURCE=..\game_shared\vgui_slider2.cpp
# End Source File
# Begin Source File

SOURCE=..\game_shared\voice_banmgr.cpp
# End Source File
# Begin Source File

SOURCE=..\game_shared\voice_status.cpp
# End Source File
# Begin Source File

SOURCE=..\game_shared\voice_vgui_tweakdlg.cpp
# End Source File
# End Group
# Begin Group "render"

# PROP Default_Filter "*.CPP"
# Begin Source File

SOURCE=.\render\gl_aurora.cpp
# End Source File
# Begin Source File

SOURCE=.\render\gl_backend.cpp
# End Source File
# Begin Source File

SOURCE=.\render\gl_bloom.cpp
# End Source File
# Begin Source File

SOURCE=.\render\gl_cull.cpp
# End Source File
# Begin Source File

SOURCE=.\render\gl_debug.cpp
# End Source File
# Begin Source File

SOURCE=.\render\gl_decals.cpp
# End Source File
# Begin Source File

SOURCE=.\render\gl_export.cpp
# End Source File
# Begin Source File

SOURCE=.\render\gl_light_dynamic.cpp
# End Source File
# Begin Source File

SOURCE=.\render\gl_lightmap.cpp
# End Source File
# Begin Source File

SOURCE=.\render\gl_mirror.cpp
# End Source File
# Begin Source File

SOURCE=.\render\gl_movie.cpp
# End Source File
# Begin Source File

SOURCE=.\render\gl_postprocess.cpp
# End Source File
# Begin Source File

SOURCE=.\render\gl_rbeams.cpp
# End Source File
# Begin Source File

SOURCE=.\render\gl_rmain.cpp
# End Source File
# Begin Source File

SOURCE=.\render\gl_rmisc.cpp
# End Source File
# Begin Source File

SOURCE=.\render\gl_rpart.cpp
# End Source File
# Begin Source File

SOURCE=.\render\gl_rsurf.cpp
# End Source File
# Begin Source File

SOURCE=.\render\gl_shadows.cpp
# End Source File
# Begin Source File

SOURCE=.\render\gl_sky.cpp
# End Source File
# Begin Source File

SOURCE=.\render\gl_sprite.cpp
# End Source File
# Begin Source File

SOURCE=.\render\gl_studio.cpp
# End Source File
# Begin Source File

SOURCE=.\render\gl_texloader.cpp
# End Source File
# Begin Source File

SOURCE=.\render\glows.cpp
# End Source File
# Begin Source File

SOURCE=.\render\glsl_shader.cpp
# End Source File
# Begin Source File

SOURCE=.\render\grass.cpp
# End Source File
# Begin Source File

SOURCE=.\render\rain.cpp
# End Source File
# Begin Source File

SOURCE=.\render\tri.cpp
# End Source File
# Begin Source File

SOURCE=.\render\triapiobjects.cpp
# End Source File
# Begin Source File

SOURCE=.\render\view.cpp
# End Source File
# End Group
# Begin Source File

SOURCE=.\ammo.cpp
# End Source File
# Begin Source File

SOURCE=.\ammo_secondary.cpp
# End Source File
# Begin Source File

SOURCE=.\ammohistory.cpp
# End Source File
# Begin Source File

SOURCE=.\battery.cpp
# End Source File
# Begin Source File

SOURCE=.\cdll_int.cpp
# End Source File
# Begin Source File

SOURCE=.\death.cpp
# End Source File
# Begin Source File

SOURCE=.\demo.cpp
# End Source File
# Begin Source File

SOURCE=.\entity.cpp
# End Source File
# Begin Source File

SOURCE=.\ev_common.cpp
# End Source File
# Begin Source File

SOURCE=.\ev_hldm.cpp
# End Source File
# Begin Source File

SOURCE=.\events.cpp
# End Source File
# Begin Source File

SOURCE=.\flashlight.cpp
# End Source File
# Begin Source File

SOURCE=.\geiger.cpp
# End Source File
# Begin Source File

SOURCE=.\health.cpp
# End Source File
# Begin Source File

SOURCE=.\hud.cpp
# End Source File
# Begin Source File

SOURCE=.\hud_msg.cpp
# End Source File
# Begin Source File

SOURCE=.\hud_redraw.cpp
# End Source File
# Begin Source File

SOURCE=.\hud_servers.cpp
# End Source File
# Begin Source File

SOURCE=.\hud_spectator.cpp
# End Source File
# Begin Source File

SOURCE=.\hud_update.cpp
# End Source File
# Begin Source File

SOURCE=.\in_camera.cpp
# End Source File
# Begin Source File

SOURCE=.\input.cpp
# End Source File
# Begin Source File

SOURCE=.\inputw32.cpp
# End Source File
# Begin Source File

SOURCE=.\lensflare.cpp
# End Source File
# Begin Source File

SOURCE=.\menu.cpp
# End Source File
# Begin Source File

SOURCE=.\message.cpp
# End Source File
# Begin Source File

SOURCE=.\parsemsg.cpp
# End Source File
# Begin Source File

SOURCE=.\saytext.cpp
# End Source File
# Begin Source File

SOURCE=.\status_icons.cpp
# End Source File
# Begin Source File

SOURCE=.\statusbar.cpp
# End Source File
# Begin Source File

SOURCE=.\text_message.cpp
# End Source File
# Begin Source File

SOURCE=.\train.cpp
# End Source File
# Begin Source File

SOURCE=.\util.cpp
# End Source File
# Begin Source File

SOURCE=.\vgui_ClassMenu.cpp
# End Source File
# Begin Source File

SOURCE=.\vgui_ConsolePanel.cpp
# End Source File
# Begin Source File

SOURCE=.\vgui_ControlConfigPanel.cpp
# End Source File
# Begin Source File

SOURCE=.\vgui_CustomObjects.cpp
# End Source File
# Begin Source File

SOURCE=.\vgui_gamma.cpp
# End Source File
# Begin Source File

SOURCE=.\vgui_hud.cpp
# End Source File
# Begin Source File

SOURCE=.\vgui_int.cpp
# End Source File
# Begin Source File

SOURCE=.\vgui_MOTDWindow.cpp
# End Source File
# Begin Source File

SOURCE=.\vgui_paranoiatext.cpp
# End Source File
# Begin Source File

SOURCE=.\vgui_radio.cpp
# End Source File
# Begin Source File

SOURCE=.\vgui_SchemeManager.cpp
# End Source File
# Begin Source File

SOURCE=.\vgui_ScorePanel.cpp
# End Source File
# Begin Source File

SOURCE=.\vgui_ServerBrowser.cpp
# End Source File
# Begin Source File

SOURCE=.\vgui_SpectatorPanel.cpp
# End Source File
# Begin Source File

SOURCE=.\vgui_subtitles.cpp
# End Source File
# Begin Source File

SOURCE=.\vgui_tabpanel.cpp
# End Source File
# Begin Source File

SOURCE=.\vgui_TeamFortressViewport.cpp
# End Source File
# Begin Source File

SOURCE=.\vgui_teammenu.cpp
# End Source File
# Begin Source File

SOURCE=.\vgui_tips.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl;fi;fd"
# Begin Source File

SOURCE=.\ammo.h
# End Source File
# Begin Source File

SOURCE=.\ammohistory.h
# End Source File
# Begin Source File

SOURCE=..\game_shared\bitvec.h
# End Source File
# Begin Source File

SOURCE=.\camera.h
# End Source File
# Begin Source File

SOURCE=..\game_shared\cdll_dll.h
# End Source File
# Begin Source File

SOURCE=.\cl_dll.h
# End Source File
# Begin Source File

SOURCE=.\cl_util.h
# End Source File
# Begin Source File

SOURCE=.\com_weapons.h
# End Source File
# Begin Source File

SOURCE=.\custom_alloc.h
# End Source File
# Begin Source File

SOURCE=.\demo.h
# End Source File
# Begin Source File

SOURCE=.\enginecallback.h
# End Source File
# Begin Source File

SOURCE=.\ev_hldm.h
# End Source File
# Begin Source File

SOURCE=.\eventscripts.h
# End Source File
# Begin Source File

SOURCE=.\fmod_errors.h
# End Source File
# Begin Source File

SOURCE=.\getfont.h
# End Source File
# Begin Source File

SOURCE=.\render\gl_aurora.h
# End Source File
# Begin Source File

SOURCE=.\render\gl_decals.h
# End Source File
# Begin Source File

SOURCE=.\render\gl_export.h
# End Source File
# Begin Source File

SOURCE=.\render\gl_local.h
# End Source File
# Begin Source File

SOURCE=.\render\gl_rpart.h
# End Source File
# Begin Source File

SOURCE=.\render\gl_shader.h
# End Source File
# Begin Source File

SOURCE=.\render\gl_sprite.h
# End Source File
# Begin Source File

SOURCE=.\render\gl_studio.h
# End Source File
# Begin Source File

SOURCE=.\render\glsl_shader.h
# End Source File
# Begin Source File

SOURCE=.\health.h
# End Source File
# Begin Source File

SOURCE=.\hud.h
# End Source File
# Begin Source File

SOURCE=.\hud_iface.h
# End Source File
# Begin Source File

SOURCE=.\hud_servers.h
# End Source File
# Begin Source File

SOURCE=.\hud_servers_priv.h
# End Source File
# Begin Source File

SOURCE=.\hud_spectator.h
# End Source File
# Begin Source File

SOURCE=.\in_defs.h
# End Source File
# Begin Source File

SOURCE=.\kbutton.h
# End Source File
# Begin Source File

SOURCE=..\game_shared\matrix.h
# End Source File
# Begin Source File

SOURCE=.\overview.h
# End Source File
# Begin Source File

SOURCE=.\parsemsg.h
# End Source File
# Begin Source File

SOURCE=..\pm_shared\pm_debug.h
# End Source File
# Begin Source File

SOURCE=.\render\rain.h
# End Source File
# Begin Source File

SOURCE=..\game_shared\stringlib.h
# End Source File
# Begin Source File

SOURCE=.\render\texture.h
# End Source File
# Begin Source File

SOURCE=.\tf_defs.h
# End Source File
# Begin Source File

SOURCE=..\game_shared\tri_stripper.h
# End Source File
# Begin Source File

SOURCE=.\render\triapiobjects.h
# End Source File
# Begin Source File

SOURCE=..\game_shared\utlarray.h
# End Source File
# Begin Source File

SOURCE=..\game_shared\utlblockmemory.h
# End Source File
# Begin Source File

SOURCE=..\game_shared\utllinkedlist.h
# End Source File
# Begin Source File

SOURCE=..\game_shared\utlmemory.h
# End Source File
# Begin Source File

SOURCE=..\game_shared\vector.h
# End Source File
# Begin Source File

SOURCE=..\game_shared\vgui_checkbutton2.h
# End Source File
# Begin Source File

SOURCE=.\vgui_ConsolePanel.h
# End Source File
# Begin Source File

SOURCE=.\vgui_ControlConfigPanel.h
# End Source File
# Begin Source File

SOURCE=..\game_shared\vgui_defaultinputsignal.h
# End Source File
# Begin Source File

SOURCE=.\vgui_gamma.h
# End Source File
# Begin Source File

SOURCE=..\game_shared\vgui_grid.h
# End Source File
# Begin Source File

SOURCE=..\game_shared\vgui_helpers.h
# End Source File
# Begin Source File

SOURCE=.\vgui_hud.h
# End Source File
# Begin Source File

SOURCE=.\vgui_int.h
# End Source File
# Begin Source File

SOURCE=..\game_shared\vgui_listbox.h
# End Source File
# Begin Source File

SOURCE=..\game_shared\vgui_loadtga.h
# End Source File
# Begin Source File

SOURCE=.\vgui_paranoiatext.h
# End Source File
# Begin Source File

SOURCE=.\vgui_pickup.h
# End Source File
# Begin Source File

SOURCE=.\vgui_radio.h
# End Source File
# Begin Source File

SOURCE=.\vgui_SchemeManager.h
# End Source File
# Begin Source File

SOURCE=.\vgui_ScorePanel.h
# End Source File
# Begin Source File

SOURCE=.\vgui_screenmsg.h
# End Source File
# Begin Source File

SOURCE=..\game_shared\vgui_scrollbar2.h
# End Source File
# Begin Source File

SOURCE=.\vgui_ServerBrowser.h
# End Source File
# Begin Source File

SOURCE=.\vgui_shadowtext.h
# End Source File
# Begin Source File

SOURCE=..\game_shared\vgui_slider2.h
# End Source File
# Begin Source File

SOURCE=.\vgui_SpectatorPanel.h
# End Source File
# Begin Source File

SOURCE=.\vgui_subtitles.h
# End Source File
# Begin Source File

SOURCE=.\vgui_tabpanel.h
# End Source File
# Begin Source File

SOURCE=.\vgui_TeamFortressViewport.h
# End Source File
# Begin Source File

SOURCE=.\vgui_tips.h
# End Source File
# Begin Source File

SOURCE=..\game_shared\voice_banmgr.h
# End Source File
# Begin Source File

SOURCE=..\game_shared\voice_common.h
# End Source File
# Begin Source File

SOURCE=..\game_shared\voice_gamemgr.h
# End Source File
# Begin Source File

SOURCE=..\game_shared\voice_status.h
# End Source File
# Begin Source File

SOURCE=..\game_shared\voice_vgui_tweakdlg.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;cnt;rtf;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
