/*

*/


#include "LuaConsole.h"
#include <cstdlib>
#include <vector>
#include <algorithm>
#include <string>
#include <map>
#include <list>
#include <filesystem>
#include "../../winproject/resource.h"
#include "win/main_win.h"
#include "win/Config.hpp"
#include "include/lua.hpp"
#include "../memory/memory.h"
#include "../r4300/r4300.h"
#include "../r4300/recomp.h"
#include "../main/plugin.hpp"
#include "../main/disasm.h"
#include "../main/vcr.h"
#include "../main/savestates.h"
#include "../main/win/configdialog.h"
#include "../main/helpers/win_helpers.h"
#include "../main/win/wrapper/PersistentPathDialog.h"
#include "../main/vcr_compress.h"
#include <vcr.h>
#include <gdiplus.h>
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <wincodec.h>
#include <functional>
#include <queue>
#include <md5.h>
#include <assert.h>

#include "Recent.h"
#include "helpers/string_helpers.h"
#include "win/features/Statusbar.hpp"

#include <Windows.h>

#include "modules/avi.h"
#include "modules/d2d.h"
#include "modules/emu.h"
#include "modules/global.h"
#include "modules/input.h"
#include "modules/iohelper.h"
#include "modules/joypad.h"
#include "modules/memory.h"
#include "modules/movie.h"
#include "modules/savestate.h"
#include "modules/wgui.h"
#include "win/timers.h"
#pragma comment(lib, "lua54.lib")

extern unsigned long op;
extern void (*interp_ops[64])(void);

extern int fast_memory;
inline void TraceLoggingBufFlush();

std::vector<std::string> recent_closed_lua;

BUTTONS last_controller_data[4];
BUTTONS new_controller_data[4];
bool overwrite_controller_data[4];

bool enableTraceLog;
bool traceLogMode;
ULONG_PTR gdi_plus_token;

std::map<HWND, LuaEnvironment*> hwnd_lua_map;
t_window_procedure_params window_proc_params = {0};


#define DEBUG_GETLASTERROR 0
	HANDLE TraceLogFile;

	uint64_t inputCount = 0;

	int getn(lua_State*);


	//improved debug print from stackoverflow, now shows function info
#ifdef _DEBUG
	static void stackDump(lua_State* L)
	{
		int i = lua_gettop(L);
		printf(" ----------------  Stack Dump ----------------\n");
		while (i)
		{
			int t = lua_type(L, i);
			switch (t)
			{
			case LUA_TSTRING:
				printf("%d:`%s'", i, lua_tostring(L, i));
				break;
			case LUA_TBOOLEAN:
				printf("%d: %s", i, lua_toboolean(L, i) ? "true" : "false");
				break;
			case LUA_TNUMBER:
				printf("%d: %g", i, lua_tonumber(L, i));
				break;
			case LUA_TFUNCTION:
				lua_Debug ar;
				lua_getstack(L, 0, &ar);
				lua_pushvalue(L, -1);
				lua_getinfo(L, ">S", &ar);
				printf("%d: %s %p, type: %s", i, "function at",
				       lua_topointer(L, -1), ar.what);
				break;
			default: printf("%d: %s", i, lua_typename(L, t));
				break;
			}
			printf("\n");
			i--;
		}
		printf("--------------- Stack Dump Finished ---------------\n");
	}
#endif
	void ASSERT(bool e, const char* s = "assert")
	{
		if (!e)
		{
			DebugBreak();
			printf("Lua assert failed: %s\n", s);
		}
	}



	int AtPanic(lua_State* L);
	extern const luaL_Reg globalFuncs[];
	extern const luaL_Reg emuFuncs[];
	extern const luaL_Reg wguiFuncs[];
	extern const luaL_Reg d2dFuncs[];
	extern const luaL_Reg memoryFuncs[];
	extern const luaL_Reg inputFuncs[];
	extern const luaL_Reg joypadFuncs[];
	extern const luaL_Reg movieFuncs[];
	extern const luaL_Reg savestateFuncs[];
	extern const luaL_Reg ioHelperFuncs[];
	extern const luaL_Reg aviFuncs[];
	extern const char* const REG_ATSTOP;
	int AtStop(lua_State* L);



	int AtUpdateScreen(lua_State* L);

	int AtPanic(lua_State* L)
	{
		printf("Lua panic: %s\n", lua_tostring(L, -1));
		MessageBox(mainHWND, lua_tostring(L, -1), "Lua Panic", 0);
		return 0;
	}

	extern const char* const REG_WINDOWMESSAGE;
	int AtWindowMessage(lua_State* L);

	void invoke_callbacks_with_key_on_all_instances(
		std::function<int(lua_State*)> function, const char* key)
	{
		for (auto pair : hwnd_lua_map)
		{
			pair.second->invoke_callbacks_with_key(function, key);
		}
	}

	BOOL WmCommand(HWND wnd, WORD id, WORD code, HWND control);

	INT_PTR CALLBACK DialogProc(HWND wnd, UINT msg, WPARAM wParam,
	                            LPARAM lParam)
	{
		switch (msg)
		{
		case WM_INITDIALOG:
			{
				SetWindowText(GetDlgItem(wnd, IDC_TEXTBOX_LUASCRIPTPATH),
				              Config.lua_script_path.c_str());
				return TRUE;
			}
		case WM_CLOSE:
			DestroyWindow(wnd);
			return TRUE;
		case WM_DESTROY:
			{
				if (hwnd_lua_map.contains(wnd)) {
					LuaEnvironment::destroy(hwnd_lua_map[wnd]);
				}
				return TRUE;
			}
		case WM_COMMAND:
			return WmCommand(wnd, LOWORD(wParam), HIWORD(wParam), (HWND)lParam);
		case WM_SIZE:
			{
				RECT window_rect = {0};
				GetClientRect(wnd, &window_rect);

				HWND console_hwnd = GetDlgItem(wnd, IDC_TEXTBOX_LUACONSOLE);
				RECT console_rect = get_window_rect_client_space(wnd, console_hwnd);
				SetWindowPos(console_hwnd, nullptr, 0, 0, window_rect.right - console_rect.left * 2, window_rect.bottom - console_rect.top, SWP_NOMOVE);

				HWND path_hwnd = GetDlgItem(wnd, IDC_TEXTBOX_LUASCRIPTPATH);
				RECT path_rect = get_window_rect_client_space(wnd, path_hwnd);
				SetWindowPos(path_hwnd, nullptr, 0, 0, window_rect.right - console_rect.left * 2, path_rect.bottom - path_rect.top, SWP_NOMOVE);
				if (wParam == SIZE_MINIMIZED) SetFocus(mainHWND);
				break;
			}
		}
		return FALSE;
	}

	void SetButtonState(HWND wnd, bool state)
	{
		if (!IsWindow(wnd)) return;
		const HWND state_button = GetDlgItem(wnd, IDC_BUTTON_LUASTATE);
		const HWND stop_button = GetDlgItem(wnd, IDC_BUTTON_LUASTOP);
		if (state)
		{
			SetWindowText(state_button, "Restart");
			EnableWindow(stop_button, TRUE);
		} else
		{
			SetWindowText(state_button, "Run");
			EnableWindow(stop_button, FALSE);
		}
	}

	BOOL WmCommand(HWND wnd, WORD id, WORD code, HWND control)
	{
		switch (id)
		{
		case IDC_BUTTON_LUASTATE:
			{
				char path[MAX_PATH] = {0};
				GetWindowText(GetDlgItem(wnd, IDC_TEXTBOX_LUASCRIPTPATH),
				              path, MAX_PATH);

				// if already running, delete and erase it (we dont want to overwrite the environment without properly disposing it)
				if (hwnd_lua_map.contains(wnd)) {
					LuaEnvironment::destroy(hwnd_lua_map[wnd]);
				}

				// now spool up a new one
				auto status = LuaEnvironment::create(path, wnd);
				lua_recent_scripts_add(path);
				if (status.first == nullptr) {
					// failed, we give user some info and thats it
					ConsoleWrite(wnd, status.second.c_str());
				} else {
					// it worked, we can set up associations and sync ui state
					hwnd_lua_map[wnd] = status.first;
					SetButtonState(wnd, true);
				}

				return TRUE;
			}
		case IDC_BUTTON_LUASTOP:
			{
				if (hwnd_lua_map.contains(wnd)) {
					LuaEnvironment::destroy(hwnd_lua_map[wnd]);
					SetButtonState(wnd, false);
				}
				return TRUE;
			}
		case IDC_BUTTON_LUABROWSE:
			{
				auto path = show_persistent_open_dialog("o_lua", wnd, L"*.lua");

				if (path.empty())
				{
					break;
				}

				SetWindowText(GetDlgItem(wnd, IDC_TEXTBOX_LUASCRIPTPATH), wstring_to_string(path).c_str());
				return TRUE;
			}
		case IDC_BUTTON_LUAEDIT:
			{
				CHAR buf[MAX_PATH];
				GetWindowText(GetDlgItem(wnd, IDC_TEXTBOX_LUASCRIPTPATH),
				              buf, MAX_PATH);
				if (buf == NULL || buf[0] == '\0')
					/* || strlen(buf)>MAX_PATH*/
					return FALSE;
				// previously , clicking edit with empty path will open current directory in explorer, which is very bad

				ShellExecute(0, 0, buf, 0, 0, SW_SHOW);
				return TRUE;
			}
		case IDC_BUTTON_LUACLEAR:
			if (GetAsyncKeyState(VK_MENU))
			{
				// clear path
				SetWindowText(GetDlgItem(wnd, IDC_TEXTBOX_LUASCRIPTPATH), "");
				return TRUE;
			}

			SetWindowText(GetDlgItem(wnd, IDC_TEXTBOX_LUACONSOLE), "");
			return TRUE;
		}
		return FALSE;
	}

	HWND lua_create()
	{
		HWND hwnd = CreateDialogParam(app_instance,
		                             MAKEINTRESOURCE(IDD_LUAWINDOW), mainHWND,
		                             DialogProc,
		                             NULL);
		ShowWindow(hwnd, SW_SHOW);
		return hwnd;
	}

void lua_init()
{
	Gdiplus::GdiplusStartupInput startup_input;
	GdiplusStartup(&gdi_plus_token, &startup_input, NULL);
}

void lua_exit()
{
	Gdiplus::GdiplusShutdown(gdi_plus_token);
}

void lua_create_and_run(const char* path)
	{
		printf("Creating lua window...\n");
		auto hwnd = lua_create();

		printf("Setting path...\n");
		// set the textbox content to match the path
		SetWindowText(GetDlgItem(hwnd, IDC_TEXTBOX_LUASCRIPTPATH), path);

		printf("Simulating run button click...\n");
		// click run button
		SendMessage(hwnd, WM_COMMAND,
						MAKEWPARAM(IDC_BUTTON_LUASTATE, BN_CLICKED),
						(LPARAM)GetDlgItem(hwnd, IDC_BUTTON_LUASTATE));
	}


	void ConsoleWrite(HWND wnd, const char* str)
	{
		HWND console = GetDlgItem(wnd, IDC_TEXTBOX_LUACONSOLE);

		int length = GetWindowTextLength(console);
		if (length >= 0x7000)
		{
			SendMessage(console, EM_SETSEL, 0, length / 2);
			SendMessage(console, EM_REPLACESEL, false, (LPARAM)"");
			length = GetWindowTextLength(console);
		}
		SendMessage(console, EM_SETSEL, length, length);
		SendMessage(console, EM_REPLACESEL, false, (LPARAM)str);
	}

	LRESULT CALLBACK LuaGUIWndProc(HWND wnd, UINT msg, WPARAM wParam,
	                               LPARAM lParam)
	{
		switch (msg)
		{
		case WM_CREATE:
		case WM_DESTROY:
			return 0;
		}
		return DefWindowProc(wnd, msg, wParam, lParam);
	}


	void RecompileNextAll();
	void RecompileNow(ULONG);

	void TraceLogStart(const char* name, BOOL append = FALSE)
	{
		if (TraceLogFile = CreateFile(name, GENERIC_WRITE,
		                              FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
		                              append ? OPEN_ALWAYS : CREATE_ALWAYS,
		                              FILE_ATTRIBUTE_NORMAL |
		                              FILE_FLAG_SEQUENTIAL_SCAN, NULL))
		{
			if (append)
			{
				SetFilePointer(TraceLogFile, 0, NULL, FILE_END);
			}
			enableTraceLog = true;
			if (interpcore == 0)
			{
				RecompileNextAll();
				RecompileNow(PC->addr);
			}
			MENUITEMINFO mii = {};
			mii.cbSize = sizeof(MENUITEMINFO);
			mii.fMask = MIIM_STRING;
			mii.dwTypeData = (LPTSTR)"Stop &Trace Logger";
			SetMenuItemInfo(GetMenu(mainHWND), ID_TRACELOG,
			                FALSE, &mii);
		}
	}

	void TraceLogStop()
	{
		enableTraceLog = false;
		CloseHandle(TraceLogFile);
		TraceLogFile = NULL;
		MENUITEMINFO mii = {};
		mii.cbSize = sizeof(MENUITEMINFO);
		mii.fMask = MIIM_STRING;
		mii.dwTypeData = (LPTSTR)"Start &Trace Logger";
		SetMenuItemInfo(GetMenu(mainHWND), ID_TRACELOG,
		                FALSE, &mii);
		TraceLoggingBufFlush();
	}


	LuaEnvironment* GetLuaClass(lua_State* L)
	{
		lua_getfield(L, LUA_REGISTRYINDEX, REG_LUACLASS);
		auto lua = (LuaEnvironment*)lua_topointer(L, -1);
		lua_pop(L, 1);
		return lua;
	}

	void SetLuaClass(lua_State* L, void* lua)
	{
		lua_pushlightuserdata(L, lua);
		lua_setfield(L, LUA_REGISTRYINDEX, REG_LUACLASS);
		//lua_pop(L, 1); //iteresting, it worked before
	}

	int GetErrorMessage(lua_State* L)
	{
		return 1;
	}

	int getn(lua_State* L)
	{
		lua_pushinteger(L, luaL_len(L, -1));
		return 1;
	}

	int RegisterFunction(lua_State* L, const char* key)
	{
		lua_getfield(L, LUA_REGISTRYINDEX, key);
		if (lua_isnil(L, -1))
		{
			lua_pop(L, 1);
			lua_newtable(L);
			lua_setfield(L, LUA_REGISTRYINDEX, key);
			lua_getfield(L, LUA_REGISTRYINDEX, key);
		}
		int i = luaL_len(L, -1) + 1;
		lua_pushinteger(L, i);
		lua_pushvalue(L, -3); //
		lua_settable(L, -3);
		lua_pop(L, 1);
		return i;
	}

	void UnregisterFunction(lua_State* L, const char* key)
	{
		lua_getfield(L, LUA_REGISTRYINDEX, key);
		if (lua_isnil(L, -1))
		{
			lua_pop(L, 1);
			lua_newtable(L); //�Ƃ肠����
		}
		int n = luaL_len(L, -1);
		for (LUA_INTEGER i = 0; i < n; i++)
		{
			lua_pushinteger(L, 1 + i);
			lua_gettable(L, -2);
			if (lua_rawequal(L, -1, -3))
			{
				lua_pop(L, 1);
				lua_getglobal(L, "table");
				lua_getfield(L, -1, "remove");
				lua_pushvalue(L, -3);
				lua_pushinteger(L, 1 + i);
				lua_call(L, 2, 0);
				lua_pop(L, 2);
				return;
			}
			lua_pop(L, 1);
		}
		lua_pop(L, 1);
		lua_pushfstring(L, "unregisterFunction(%s): not found function",
		                key);
		lua_error(L);
	}


	//������ւ񂩂�֐�



	// 000000 | 0000 0000 0000 000 | stype(5) = 10101 |001111
	const ULONG BREAKPOINTSYNC_MAGIC_STYPE = 0x15;
	const ULONG BREAKPOINTSYNC_MAGIC = 0x0000000F |
		(BREAKPOINTSYNC_MAGIC_STYPE << 6);

	void Recompile(ULONG);

	unsigned long PAddr(unsigned long addr)
	{
		if (addr >= 0x80000000 && addr < 0xC0000000)
		{
			return addr;
		} else
		{
			return virtual_to_physical_address(addr, 2);
		}
	}

	void RecompileNow(ULONG addr)
	{
		//NOTCOMPILED���B�����ɃR���p�C�����ʂ�ops�Ȃǂ��~�������Ɏg��
		if ((addr >> 16) == 0xa400)
			recompile_block((long*)SP_DMEM, blocks[0xa4000000 >> 12], addr);
		else
		{
			unsigned long paddr = PAddr(addr);
			if (paddr)
			{
				if ((paddr & 0x1FFFFFFF) >= 0x10000000)
				{
					recompile_block(
						(long*)rom + ((((paddr - (addr - blocks[addr >> 12]->
							start)) & 0x1FFFFFFF) - 0x10000000) >> 2),
						blocks[addr >> 12], addr);
				} else
				{
					recompile_block(
						(long*)(rdram + (((paddr - (addr - blocks[addr >> 12]->
							start)) & 0x1FFFFFFF) >> 2)),
						blocks[addr >> 12], addr);
				}
			}
		}
	}

	void Recompile(ULONG addr)
	{
		//jump_to���
		//���ʂɃ��R���p�C���������Ƃ��͂���ł���
		ULONG block = addr >> 12;
		ULONG paddr = PAddr(addr);
		if (!blocks[block])
		{
			blocks[block] = (precomp_block*)malloc(sizeof(precomp_block));
			actual = blocks[block];
			blocks[block]->code = NULL;
			blocks[block]->block = NULL;
			blocks[block]->jumps_table = NULL;
		}
		blocks[block]->start = addr & ~0xFFF;
		blocks[block]->end = (addr & ~0xFFF) + 0x1000;
		init_block(
			(long*)(rdram + (((paddr - (addr - blocks[block]->start)) &
				0x1FFFFFFF) >> 2)),
			blocks[block]);
	}

	void RecompileNext(ULONG addr)
	{
		//jump_to�̎�(�u���b�N��܂������W�����v)�Ƀ`�F�b�N�����H
		//������u���b�N�𒼂��ɏC�����������ȊO�͂���ł���
		invalid_code[addr >> 12] = 1;
	}

	void RecompileNextAll()
	{
		memset(invalid_code, 1, 0x100000);
	}






	int AtUpdateScreen(lua_State* L)
	{
		return lua_pcall(L, 0, 0, 0);
	}

	int AtVI(lua_State* L)
	{
		return lua_pcall(L, 0, 0, 0);
	}

	static int current_input_n;

	int AtInput(lua_State* L)
	{
		lua_pushinteger(L, current_input_n);
		return lua_pcall(L, 1, 0, 0);
	}

	int AtStop(lua_State* L)
	{
		return lua_pcall(L, 0, 0, 0);
	}

	int AtWindowMessage(lua_State* L)
	{
		lua_pushinteger(L, (unsigned)window_proc_params.wnd);
		lua_pushinteger(L, window_proc_params.msg);
		lua_pushinteger(L, window_proc_params.w_param);
		lua_pushinteger(L, window_proc_params.l_param);

		return lua_pcall(L, 4, 0, 0);
	}



	//generic function used for all of the At... callbacks, calls function from stack top.
	int CallTop(lua_State* L)
	{
		return lua_pcall(L, 0, 0, 0);
	}

	const luaL_Reg globalFuncs[] = {
		{"print", LuaCore::Global::Print},
		{"printx", LuaCore::Global::PrintX},
		{"tostringex", LuaCore::Global::ToStringExs},
		{"stop", LuaCore::Global::StopScript},
		{NULL, NULL}
	};
	//�G���Ȋ֐�
	const luaL_Reg emuFuncs[] = {
		{"console", LuaCore::Emu::ConsoleWriteLua},
		{"statusbar", LuaCore::Emu::StatusbarWrite},

		{"atvi", LuaCore::Emu::RegisterVI},
		{"atupdatescreen", LuaCore::Emu::RegisterUpdateScreen},
		{"atinput", LuaCore::Emu::RegisterInput},
		{"atstop", LuaCore::Emu::RegisterStop},
		{"atwindowmessage", LuaCore::Emu::RegisterWindowMessage},
		{"atinterval", LuaCore::Emu::RegisterInterval},
		{"atplaymovie", LuaCore::Emu::RegisterPlayMovie},
		{"atstopmovie", LuaCore::Emu::RegisterStopMovie},
		{"atloadstate", LuaCore::Emu::RegisterLoadState},
		{"atsavestate", LuaCore::Emu::RegisterSaveState},
		{"atreset", LuaCore::Emu::RegisterReset},

		{"framecount", LuaCore::Emu::GetVICount},
		{"samplecount", LuaCore::Emu::GetSampleCount},
		{"inputcount", LuaCore::Emu::GetInputCount},

		// DEPRECATE: This is completely useless
		{"getversion", LuaCore::Emu::GetMupenVersion},

		{"pause", LuaCore::Emu::EmuPause},
		{"getpause", LuaCore::Emu::GetEmuPause},
		{"getspeed", LuaCore::Emu::GetSpeed},
		{"speed", LuaCore::Emu::SetSpeed},
		{"speedmode", LuaCore::Emu::SetSpeedMode},
		// DEPRECATE: This is completely useless
		{"setgfx", LuaCore::Emu::SetGFX},

		{"getaddress", LuaCore::Emu::GetAddress},

		{"screenshot", LuaCore::Emu::Screenshot},

#pragma region WinAPI
		{"getsystemmetrics", LuaCore::Emu::GetSystemMetricsLua},
		{"ismainwindowinforeground", LuaCore::Emu::IsMainWindowInForeground},
		{"play_sound", LuaCore::Emu::LuaPlaySound},
#pragma endregion

		{NULL, NULL}
	};
	const luaL_Reg memoryFuncs[] = {
		// memory conversion functions
		{"inttofloat", LuaCore::Memory::LuaIntToFloat},
		{"inttodouble", LuaCore::Memory::LuaIntToDouble},
		{"floattoint", LuaCore::Memory::LuaFloatToInt},
		{"doubletoint", LuaCore::Memory::LuaDoubleToInt},
		{"qwordtonumber", LuaCore::Memory::LuaQWordToNumber},

		// word = 2 bytes
		// reading functions
		{"readbytesigned", LuaCore::Memory::LuaReadByteSigned},
		{"readbyte", LuaCore::Memory::LuaReadByteUnsigned},
		{"readwordsigned", LuaCore::Memory::LuaReadWordSigned},
		{"readword", LuaCore::Memory::LuaReadWordUnsigned},
		{"readdwordsigned", LuaCore::Memory::LuaReadDWordSigned},
		{"readdword", LuaCore::Memory::LuaReadDWorldUnsigned},
		{"readqwordsigned", LuaCore::Memory::LuaReadQWordSigned},
		{"readqword", LuaCore::Memory::LuaReadQWordUnsigned},
		{"readfloat", LuaCore::Memory::LuaReadFloat},
		{"readdouble", LuaCore::Memory::LuaReadDouble},
		{"readsize", LuaCore::Memory::LuaReadSize},

		// writing functions
		// all of these are assumed to be unsigned
		{"writebyte", LuaCore::Memory::LuaWriteByteUnsigned},
		{"writeword", LuaCore::Memory::LuaWriteWordUnsigned},
		{"writedword", LuaCore::Memory::LuaWriteDWordUnsigned},
		{"writeqword", LuaCore::Memory::LuaWriteQWordUnsigned},
		{"writefloat", LuaCore::Memory::LuaWriteFloatUnsigned},
		{"writedouble", LuaCore::Memory::LuaWriteDoubleUnsigned},

		{"writesize", LuaCore::Memory::LuaWriteSize},

		{NULL, NULL}
	};

	//winAPI GDI�֐���
	const luaL_Reg wguiFuncs[] = {
		{"setbrush", LuaCore::Wgui::set_brush},
		{"setpen", LuaCore::Wgui::set_pen},
		{"setcolor", LuaCore::Wgui::set_text_color},
		{"setbk", LuaCore::Wgui::SetBackgroundColor},
		{"setfont", LuaCore::Wgui::SetFont},
		{"text", LuaCore::Wgui::LuaTextOut},
		{"drawtext", LuaCore::Wgui::LuaDrawText},
		{"drawtextalt", LuaCore::Wgui::LuaDrawTextAlt},
		{"gettextextent", LuaCore::Wgui::GetTextExtent},
		{"rect", LuaCore::Wgui::DrawRect},
		{"fillrect", LuaCore::Wgui::FillRect},
		/*<GDIPlus>*/
		// GDIPlus functions marked with "a" suffix
		{"fillrecta", LuaCore::Wgui::FillRectAlpha},
		{"fillellipsea", LuaCore::Wgui::FillEllipseAlpha},
		{"fillpolygona", LuaCore::Wgui::FillPolygonAlpha},
		{"loadimage", LuaCore::Wgui::LuaLoadImage},
		{"deleteimage", LuaCore::Wgui::DeleteImage},
		{"drawimage", LuaCore::Wgui::DrawImage},
		{"loadscreen", LuaCore::Wgui::LoadScreen},
		{"loadscreenreset", LuaCore::Wgui::LoadScreenReset},
		{"getimageinfo", LuaCore::Wgui::GetImageInfo},
		/*</GDIPlus*/
		{"ellipse", LuaCore::Wgui::DrawEllipse},
		{"polygon", LuaCore::Wgui::DrawPolygon},
		{"line", LuaCore::Wgui::DrawLine},
		{"info", LuaCore::Wgui::GetGUIInfo},
		{"resize", LuaCore::Wgui::ResizeWindow},
		{"setclip", LuaCore::Wgui::SetClip},
		{"resetclip", LuaCore::Wgui::ResetClip},
		{NULL, NULL}
	};

	const luaL_Reg d2dFuncs[] = {
		{"create_brush", LuaCore::D2D::create_brush},
		{"free_brush", LuaCore::D2D::free_brush},

		{"fill_rectangle", LuaCore::D2D::fill_rectangle},
		{"draw_rectangle", LuaCore::D2D::draw_rectangle},
		{"fill_ellipse", LuaCore::D2D::fill_ellipse},
		{"draw_ellipse", LuaCore::D2D::draw_ellipse},
		{"draw_line", LuaCore::D2D::draw_line},
		{"draw_text", LuaCore::D2D::draw_text},
		{"get_text_size", LuaCore::D2D::measure_text},
		{"push_clip", LuaCore::D2D::push_clip},
		{"pop_clip", LuaCore::D2D::pop_clip},
		{"fill_rounded_rectangle", LuaCore::D2D::fill_rounded_rectangle},
		{"draw_rounded_rectangle", LuaCore::D2D::draw_rounded_rectangle},
		{"load_image", LuaCore::D2D::load_image},
		{"free_image", LuaCore::D2D::free_image},
		{"draw_image", LuaCore::D2D::draw_image},
		{"get_image_info", LuaCore::D2D::get_image_info},
		{"set_text_antialias_mode", LuaCore::D2D::set_text_antialias_mode},
		{"set_antialias_mode", LuaCore::D2D::set_antialias_mode},

		{"draw_to_image", LuaCore::D2D::draw_to_image},
		{NULL, NULL}
	};

	const luaL_Reg inputFuncs[] = {
		{"get", LuaCore::Input::get_keys},
		{"diff", LuaCore::Input::GetKeyDifference},
		{"prompt", LuaCore::Input::InputPrompt},
		{"get_key_name_text", LuaCore::Input::LuaGetKeyNameText},
		{"map_virtual_key_ex", LuaCore::Input::LuaMapVirtualKeyEx},
		{NULL, NULL}
	};
	const luaL_Reg joypadFuncs[] = {
		{"get", LuaCore::Joypad::lua_get_joypad},
		{"set", LuaCore::Joypad::lua_set_joypad},
		// OBSOLETE: Cross-module reach
		{"count", LuaCore::Emu::GetInputCount},
		{NULL, NULL}
	};

	const luaL_Reg movieFuncs[] = {
		{"playmovie", LuaCore::Movie::PlayMovie},
		{"stopmovie", LuaCore::Movie::StopMovie},
		{"getmoviefilename", LuaCore::Movie::GetMovieFilename},
		{"isreadonly", LuaCore::Movie::GetVCRReadOnly},
		{NULL, NULL}
	};

	const luaL_Reg savestateFuncs[] = {
		{"savefile", LuaCore::Savestate::SaveFileSavestate},
		{"loadfile", LuaCore::Savestate::LoadFileSavestate},
		{NULL, NULL}
	};
	const luaL_Reg ioHelperFuncs[] = {
		{"filediag", LuaCore::IOHelper::LuaFileDialog},
		{NULL, NULL}
	};
	const luaL_Reg aviFuncs[] = {
		{"startcapture", LuaCore::Avi::StartCapture},
		{"stopcapture", LuaCore::Avi::StopCapture},
		{NULL, NULL}
	};

void AtUpdateScreenLuaCallback()
{
	HDC main_dc = GetDC(mainHWND);

	for (auto& pair : hwnd_lua_map) {
		/// Let the environment draw to its DC
		pair.second->draw();

		/// Blit its DC to the main window with alpha mask
        TransparentBlt(main_dc, 0, 0, pair.second->dc_width, pair.second->dc_height, pair.second->dc, 0, 0,
        	pair.second->dc_width, pair.second->dc_height, bitmap_color_mask);

		RECT rect = { 0, 0, pair.second->dc_width, pair.second->dc_height};
		HBRUSH brush = CreateSolidBrush(bitmap_color_mask);
		FillRect(pair.second->dc, &rect, brush);
		DeleteObject(brush);

	}


	ReleaseDC(mainHWND, main_dc);

}

void AtVILuaCallback()
{
	invoke_callbacks_with_key_on_all_instances(
		AtVI, REG_ATVI);
}

void AtInputLuaCallback(int n)
{
	current_input_n = n;
	invoke_callbacks_with_key_on_all_instances(
		AtInput, REG_ATINPUT);
	inputCount++;
}

void AtIntervalLuaCallback()
{
	invoke_callbacks_with_key_on_all_instances(
		CallTop, REG_ATINTERVAL);
}

void AtPlayMovieLuaCallback()
{
	invoke_callbacks_with_key_on_all_instances(
		CallTop, REG_ATPLAYMOVIE);
}

void AtStopMovieLuaCallback()
{
	invoke_callbacks_with_key_on_all_instances(
		CallTop, REG_ATSTOPMOVIE);
}

void AtLoadStateLuaCallback()
{
	invoke_callbacks_with_key_on_all_instances(
		CallTop, REG_ATLOADSTATE);
}

void AtSaveStateLuaCallback()
{
	invoke_callbacks_with_key_on_all_instances(
		CallTop, REG_ATSAVESTATE);
}

//called after reset, when emulation ready
void AtResetLuaCallback()
{
	invoke_callbacks_with_key_on_all_instances(
		CallTop, REG_ATRESET);
}

//�Ƃ肠����lua�ɓ���Ƃ�
char traceLoggingBuf[0x10000];
char* traceLoggingPointer = traceLoggingBuf;

inline void TraceLoggingBufFlush()
{
	DWORD writeen;
	WriteFile(TraceLogFile,
	          traceLoggingBuf, traceLoggingPointer - traceLoggingBuf, &writeen,
	          NULL);
	traceLoggingPointer = traceLoggingBuf;
}

inline void TraceLoggingWriteBuf()
{
	const char* const buflength = traceLoggingBuf + sizeof(traceLoggingBuf) -
		512;
	if (traceLoggingPointer >= buflength)
	{
		TraceLoggingBufFlush();
	}
}

void instrStr1(unsigned long pc, unsigned long w, char* p1)
{
	char*& p = p1;
	INSTDECODE decode;
	const char* const x = "0123456789abcdef";
#define HEX8(n) 	p[0] = x[(r4300word)(n)>>28&0xF];\
	p[1] = x[(r4300word)(n)>>24&0xF];\
	p[2] = x[(r4300word)(n)>>20&0xF];\
	p[3] = x[(r4300word)(n)>>16&0xF];\
	p[4] = x[(r4300word)(n)>>12&0xF];\
	p[5] = x[(r4300word)(n)>>8&0xF];\
	p[6] = x[(r4300word)(n)>>4&0xF];\
	p[7] = x[(r4300word)(n)&0xF];\
	p+=8;

	DecodeInstruction(w, &decode);
	HEX8(pc);
	*(p++) = ':';
	*(p++) = ' ';
	HEX8(w);
	*(p++) = ' ';
	const char* ps = p;
	if (w == 0x00000000)
	{
		*(p++) = 'n';
		*(p++) = 'o';
		*(p++) = 'p';
	} else
	{
		for (const char* q = GetOpecodeString(&decode); *q; q++)
		{
			*(p++) = *q;
		}
		*(p++) = ' ';
		p = GetOperandString(p, &decode, pc);
	}
	for (int i = p - ps + 3; i < 24; i += 4)
	{
		*(p++) = '\t';
	}
	*(p++) = ';';
	INSTOPERAND& o = decode.operand;
#define REGCPU(n) if((n)!=0){\
			for(const char *l = CPURegisterName[n]; *l; l++){\
				*(p++) = *l;\
			}\
			*(p++) = '=';\
			HEX8(reg[n]);\
	}
#define REGCPU2(n,m) \
		REGCPU(n);\
		if((n)!=(m)&&(m)!=0){C;REGCPU(m);}
	//10�i��
#define REGFPU(n) *(p++)='f';\
			*(p++)=x[(n)/10];\
			*(p++)=x[(n)%10];\
			*(p++) = '=';\
			p+=sprintf(p,"%f",*reg_cop1_simple[n])
#define REGFPU2(n,m) REGFPU(n);\
		if((n)!=(m)){C;REGFPU(m);}
#define C *(p++) = ','

	if (delay_slot)
	{
		*(p++) = '#';
	}
	switch (decode.format)
	{
	case INSTF_NONE:
		break;
	case INSTF_J:
	case INSTF_0BRANCH:
		break;
	case INSTF_LUI:
		break;
	case INSTF_1BRANCH:
	case INSTF_JR:
	case INSTF_ISIGN:
	case INSTF_IUNSIGN:
		REGCPU(o.i.rs);
		break;
	case INSTF_2BRANCH:
		REGCPU2(o.i.rs, o.i.rt);
		break;
	case INSTF_ADDRW:
		REGCPU(o.i.rt);
		if (o.i.rt != 0) { C; }
	case INSTF_ADDRR:
		*(p++) = '@';
		*(p++) = '=';
		HEX8(reg[o.i.rs] + (r4300halfsigned)o.i.immediate);
		break;
	case INSTF_LFW:
		REGFPU(o.lf.ft);
		C;
	case INSTF_LFR:
		*(p++) = '@';
		*(p++) = '=';
		HEX8(reg[o.lf.base] + (r4300halfsigned)o.lf.offset);
		break;
	case INSTF_R1:
		REGCPU(o.r.rd);
		break;
	case INSTF_R2:
		REGCPU2(o.i.rs, o.i.rt);
		break;
	case INSTF_R3:
		REGCPU2(o.i.rs, o.i.rt);
		break;
	case INSTF_MTC0:
	case INSTF_MTC1:
	case INSTF_SA:
		REGCPU(o.r.rt);
		break;
	case INSTF_R2F:
		REGFPU(o.cf.fs);
		break;
	case INSTF_R3F:
	case INSTF_C:
		REGFPU2(o.cf.fs, o.cf.ft);
		break;
	case INSTF_MFC0:
		break;
	case INSTF_MFC1:
		REGFPU(((FPUREG)o.r.rs));
		break;
	}
	p1[strlen(p1)] = '\0';
#undef HEX8
#undef REGCPU
#undef REGFPU
#undef REGCPU2
#undef REGFPU2
#undef C
}

EmulationLock::EmulationLock()
{
	printf("Emulation Lock\n");
	pauseEmu(FALSE);
}

EmulationLock::~EmulationLock()
{
	resumeEmu(FALSE);
	printf("Emulation Unlock\n");
}

void TraceLogging(r4300word pc, r4300word w)
{
	char*& p = traceLoggingPointer;
	INSTDECODE decode;
	const char* const x = "0123456789abcdef";
#define HEX8(n) 	p[0] = x[(r4300word)(n)>>28&0xF];\
	p[1] = x[(r4300word)(n)>>24&0xF];\
	p[2] = x[(r4300word)(n)>>20&0xF];\
	p[3] = x[(r4300word)(n)>>16&0xF];\
	p[4] = x[(r4300word)(n)>>12&0xF];\
	p[5] = x[(r4300word)(n)>>8&0xF];\
	p[6] = x[(r4300word)(n)>>4&0xF];\
	p[7] = x[(r4300word)(n)&0xF];\
	p+=8;

	DecodeInstruction(w, &decode);
	HEX8(pc);
	*(p++) = ':';
	*(p++) = ' ';
	HEX8(w);
	*(p++) = ' ';
	const char* ps = p;
	if (w == 0x00000000)
	{
		*(p++) = 'n';
		*(p++) = 'o';
		*(p++) = 'p';
	} else
	{
		for (const char* q = GetOpecodeString(&decode); *q; q++)
		{
			*(p++) = *q;
		}
		*(p++) = ' ';
		p = GetOperandString(p, &decode, pc);
	}
	for (int i = p - ps + 3; i < 24; i += 4)
	{
		*(p++) = '\t';
	}
	*(p++) = ';';
	INSTOPERAND& o = decode.operand;
#define REGCPU(n) if((n)!=0){\
			for(const char *l = CPURegisterName[n]; *l; l++){\
				*(p++) = *l;\
			}\
			*(p++) = '=';\
			HEX8(reg[n]);\
	}
#define REGCPU2(n,m) \
		REGCPU(n);\
		if((n)!=(m)&&(m)!=0){C;REGCPU(m);}
	//10�i��
#define REGFPU(n) *(p++)='f';\
			*(p++)=x[(n)/10];\
			*(p++)=x[(n)%10];\
			*(p++) = '=';\
			p+=sprintf(p,"%f",*reg_cop1_simple[n])
#define REGFPU2(n,m) REGFPU(n);\
		if((n)!=(m)){C;REGFPU(m);}
#define C *(p++) = ','

	if (delay_slot)
	{
		*(p++) = '#';
	}
	switch (decode.format)
	{
	case INSTF_NONE:
		break;
	case INSTF_J:
	case INSTF_0BRANCH:
		break;
	case INSTF_LUI:
		break;
	case INSTF_1BRANCH:
	case INSTF_JR:
	case INSTF_ISIGN:
	case INSTF_IUNSIGN:
		REGCPU(o.i.rs);
		break;
	case INSTF_2BRANCH:
		REGCPU2(o.i.rs, o.i.rt);
		break;
	case INSTF_ADDRW:
		REGCPU(o.i.rt);
		if (o.i.rt != 0) { C; }
	case INSTF_ADDRR:
		*(p++) = '@';
		*(p++) = '=';
		HEX8(reg[o.i.rs] + (r4300halfsigned)o.i.immediate);
		break;
	case INSTF_LFW:
		REGFPU(o.lf.ft);
		C;
	case INSTF_LFR:
		*(p++) = '@';
		*(p++) = '=';
		HEX8(reg[o.lf.base] + (r4300halfsigned)o.lf.offset);
		break;
	case INSTF_R1:
		REGCPU(o.r.rd);
		break;
	case INSTF_R2:
		REGCPU2(o.i.rs, o.i.rt);
		break;
	case INSTF_R3:
		REGCPU2(o.i.rs, o.i.rt);
		break;
	case INSTF_MTC0:
	case INSTF_MTC1:
	case INSTF_SA:
		REGCPU(o.r.rt);
		break;
	case INSTF_R2F:
		REGFPU(o.cf.fs);
		break;
	case INSTF_R3F:
	case INSTF_C:
		REGFPU2(o.cf.fs, o.cf.ft);
		break;
	case INSTF_MFC0:
		break;
	case INSTF_MFC1:
		REGFPU(((FPUREG)o.r.rs));
		break;
	}
	*(p++) = '\n';

	TraceLoggingWriteBuf();
#undef HEX8
#undef REGCPU
#undef REGFPU
#undef REGCPU2
#undef REGFPU2
#undef C
}

void TraceLoggingBin(r4300word pc, r4300word w)
{
	char*& p = traceLoggingPointer;
	INSTDECODE decode;
	//little endian
#define HEX8(n) 	*(r4300word*)p = n; p += 4

	DecodeInstruction(w, &decode);
	HEX8(pc);
	HEX8(w);
	INSTOPERAND& o = decode.operand;
	//n�͌ォ��f�R�[�h����΂킩��
#define REGCPU(n) \
	HEX8(reg[n])
#define REGCPU2(n,m) \
		REGCPU(n);\
		REGCPU(m);
	//10�i��
#define REGFPU(n) \
	HEX8(*(r4300word*)reg_cop1_simple[n])
#define REGFPU2(n,m) REGFPU(n);REGFPU(m)
#define NONE *(r4300word*)p=0;p+=4
#define NONE2 NONE;NONE

	switch (decode.format)
	{
	case INSTF_NONE:
		NONE2;
		break;
	case INSTF_J:
	case INSTF_0BRANCH:
		NONE2;
		break;
	case INSTF_LUI:
		NONE2;
		break;
	case INSTF_1BRANCH:
	case INSTF_JR:
	case INSTF_ISIGN:
	case INSTF_IUNSIGN:
		REGCPU(o.i.rs);
		NONE;
		break;
	case INSTF_2BRANCH:
		REGCPU2(o.i.rs, o.i.rt);
		break;
	case INSTF_ADDRW:
		HEX8(reg[o.i.rs] + (r4300halfsigned)o.i.immediate);
		REGCPU(o.i.rt);
		break;
	case INSTF_ADDRR:
		HEX8(reg[o.i.rs] + (r4300halfsigned)o.i.immediate);
		NONE;
		break;
	case INSTF_LFW:
		HEX8(reg[o.lf.base] + (r4300halfsigned)o.lf.offset);
		REGFPU(o.lf.ft);
		break;
	case INSTF_LFR:
		HEX8(reg[o.lf.base] + (r4300halfsigned)o.lf.offset);
		NONE;
		break;
	case INSTF_R1:
		REGCPU(o.r.rd);
		NONE;
		break;
	case INSTF_R2:
		REGCPU2(o.i.rs, o.i.rt);
		break;
	case INSTF_R3:
		REGCPU2(o.i.rs, o.i.rt);
		break;
	case INSTF_MTC0:
	case INSTF_MTC1:
	case INSTF_SA:
		REGCPU(o.r.rt);
		NONE;
		break;
	case INSTF_R2F:
		REGFPU(o.cf.fs);
		NONE;
		break;
	case INSTF_R3F:
	case INSTF_C:
		REGFPU2(o.cf.fs, o.cf.ft);
		break;
	case INSTF_MFC0:
		NONE2;
		break;
	case INSTF_MFC1:
		REGFPU(((FPUREG)o.r.rs));
		NONE;
		break;
	}
	TraceLoggingWriteBuf();
#undef HEX8
#undef REGCPU
#undef REGFPU
#undef REGCPU2
#undef REGFPU2
}

void LuaTraceLoggingPure()
{
	if (!traceLogMode)
	{
		TraceLogging(interp_addr, op);
	} else
	{
		TraceLoggingBin(interp_addr, op);
	}
}

void LuaTraceLoggingInterpOps()
{
	if (enableTraceLog)
	{
		if (!traceLogMode)
		{
			TraceLogging(PC->addr, PC->src);
		} else
		{
			TraceLoggingBin(PC->addr, PC->src);
		}
	}
	PC->s_ops();
}


void LuaTraceLogState()
{
	if (!enableTraceLog) return;
	EmulationLock lock;

	auto path = show_persistent_save_dialog("o_tracelog", mainHWND, L"*.log");

	if (path.size() == 0)
	{
		return;
	}

	TraceLogStart(wstring_to_string(path).c_str());
}

void close_all_scripts() {
	assert(IsGUIThread(false));

	// we mutate the map's nodes while iterating, so we have to make a copy
	auto copy = std::map(hwnd_lua_map);
	for (auto pair : copy) {
		SendMessage(pair.first, WM_CLOSE, 0, 0);
	}
	assert(hwnd_lua_map.empty());
}

void stop_all_scripts()
{
	assert(IsGUIThread(false));
	// we mutate the map's nodes while iterating, so we have to make a copy
	auto copy = std::map(hwnd_lua_map);
	for (auto pair : copy) {
		SendMessage(pair.first, WM_COMMAND,
					MAKEWPARAM(IDC_BUTTON_LUASTOP, BN_CLICKED),
					(LPARAM)GetDlgItem(pair.first, IDC_BUTTON_LUASTOP));

	}
	assert(hwnd_lua_map.empty());
}


void LuaWindowMessage(HWND wnd, UINT msg, WPARAM w, LPARAM l)
{
	window_proc_params = {
		.wnd = wnd,
		.msg = msg,
		.w_param = w,
		.l_param = l
	};
	invoke_callbacks_with_key_on_all_instances(AtWindowMessage, REG_WINDOWMESSAGE);
}

void LuaEnvironment::create_renderer()
{
	if (dc != nullptr)
	{
		return;
	}
	printf("Creating multi-target renderer for Lua...\n");

	RECT window_rect;
	GetClientRect(mainHWND, &window_rect);
	dc_width = window_rect.right;
	dc_height = window_rect.bottom;

	// we create a bitmap with the main window's size and point our dc to it
	HDC main_dc = GetDC(mainHWND);
	dc = CreateCompatibleDC(main_dc);
	HBITMAP bmp = CreateCompatibleBitmap(
		main_dc, window_rect.right, window_rect.bottom);
	SelectObject(dc, bmp);
	ReleaseDC(mainHWND, main_dc);

	D2D1_RENDER_TARGET_PROPERTIES props =
		D2D1::RenderTargetProperties(
			D2D1_RENDER_TARGET_TYPE_DEFAULT,
			D2D1::PixelFormat(
				DXGI_FORMAT_B8G8R8A8_UNORM,
				D2D1_ALPHA_MODE_PREMULTIPLIED));

	D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
					  &d2d_factory);
	d2d_factory->CreateDCRenderTarget(&props, &d2d_render_target);
	DWriteCreateFactory(
		DWRITE_FACTORY_TYPE_SHARED,
		__uuidof(dw_factory),
		reinterpret_cast<IUnknown**>(&dw_factory)
	);

	RECT dc_rect = {0, 0, dc_width, dc_height};
	d2d_render_target->BindDC(dc, &dc_rect);
	d2d_render_target_stack.push(d2d_render_target);
}

void LuaEnvironment::destroy_renderer()
{
	if (!dc)
	{
		return;
	}

	printf("Destroying Lua renderer...\n");

	d2d_factory->Release();
	d2d_render_target->Release();

	for (auto const& [_, val] : d2d_bitmap_render_target) {
		val->Release();
	}
	d2d_bitmap_render_target.clear();

	for (auto const& [_, val] : dw_text_layouts) {
		val->Release();
	}
	dw_text_layouts.clear();

	while (!d2d_render_target_stack.empty()) {
		d2d_render_target_stack.pop();
	}

	for (auto x : image_pool) {
		delete x;
	}
	image_pool.clear();

	ReleaseDC(mainHWND, dc);
	dc = NULL;
	d2d_factory = NULL;
	d2d_render_target = NULL;
}

void LuaEnvironment::draw() {
	d2d_render_target->BeginDraw();
	d2d_render_target->SetTransform(D2D1::Matrix3x2F::Identity());

	this->invoke_callbacks_with_key(AtUpdateScreen, REG_ATUPDATESCREEN);

	d2d_render_target->EndDraw();
}

void LuaEnvironment::destroy(LuaEnvironment* lua_environment) {
	hwnd_lua_map.erase(lua_environment->hwnd);
	delete lua_environment;
}

std::pair<LuaEnvironment*, std::string> LuaEnvironment::create(std::filesystem::path path, HWND wnd)
{
	auto lua_environment = new LuaEnvironment();

	lua_environment->hwnd = wnd;
	lua_environment->path = path;

	lua_environment->brush = static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
	lua_environment->pen = static_cast<HPEN>(GetStockObject(BLACK_PEN));
	lua_environment->font = static_cast<HFONT>(GetStockObject(SYSTEM_FONT));
	lua_environment->col = lua_environment->bkcol = 0;
	lua_environment->bkmode = TRANSPARENT;
	lua_environment->L = luaL_newstate();
	lua_atpanic(lua_environment->L, AtPanic);
	SetLuaClass(lua_environment->L, lua_environment);
	lua_environment->register_functions();
	lua_environment->create_renderer();

	bool error = luaL_dofile(lua_environment->L, lua_environment->path.string().c_str());

	std::string error_str;
	if (error)
	{
		error_str = lua_tostring(lua_environment->L, -1);
		delete lua_environment;
		lua_environment = nullptr;
	}

	return std::make_pair(lua_environment, error_str);
}

LuaEnvironment::~LuaEnvironment() {
	invoke_callbacks_with_key(AtStop, REG_ATSTOP);
	SelectObject(dc, nullptr);
	DeleteObject(brush);
	DeleteObject(pen);
	DeleteObject(font);
	lua_close(L);
	L = NULL;
	SetButtonState(hwnd, false);
	this->destroy_renderer();
	printf("Lua destroyed\n");
}

//calls all functions that lua script has defined as callbacks, reads them from registry
//returns true at fail
bool LuaEnvironment::invoke_callbacks_with_key(std::function<int(lua_State*)> function,
							   const char* key) {
	assert(IsGUIThread(false));
	lua_getfield(L, LUA_REGISTRYINDEX, key);
	//shouldn't ever happen but could cause kernel panic
	if lua_isnil(L, -1) {
		lua_pop(L, 1);
		return false;
	}
	int n = luaL_len(L, -1);
	for (LUA_INTEGER i = 0; i < n; i++) {
		lua_pushinteger(L, 1 + i);
		lua_gettable(L, -2);
		if (function(L)) {\
			const char* str = lua_tostring(L, -1);
			ConsoleWrite(hwnd, str);
			ConsoleWrite(hwnd, "\r\n");
			printf("Lua error: %s\n", str);
			return true;
		}
	}
	lua_pop(L, 1);
	return false;
}

void LuaEnvironment::LoadScreenDelete()
{
	ReleaseDC(mainHWND, hwindowDC);
	DeleteDC(hsrcDC);

	LoadScreenInitialized = false;
}

void LuaEnvironment::LoadScreenInit()
{
	if (LoadScreenInitialized) LoadScreenDelete();

	hwindowDC = GetDC(mainHWND);
	// Create a handle to the main window Device Context
	hsrcDC = CreateCompatibleDC(hwindowDC); // Create a DC to copy the screen to

	get_window_info(mainHWND, windowSize);
	windowSize.height -= 1; // ¯\_(ツ)_/¯
	printf("LoadScreen Size: %d x %d\n", windowSize.width, windowSize.height);

	// create an hbitmap
	hbitmap = CreateCompatibleBitmap(hwindowDC, windowSize.width,
									 windowSize.height);

	LoadScreenInitialized = true;
}

void register_as_package(lua_State* lua_state, const char* name, const luaL_Reg regs[]) {
	if (name == nullptr)
	{
		const luaL_Reg* p = regs;
		do {
			lua_register(lua_state, p->name, p->func);
		} while ((++p)->func);
		return;
	}
	luaL_newlib(lua_state, regs);
	lua_setglobal(lua_state, name);
}

void LuaEnvironment::register_functions() {
	luaL_openlibs(L);

	register_as_package(L, nullptr, globalFuncs);
	register_as_package(L, "emu", emuFuncs);
	register_as_package(L, "memory", memoryFuncs);
	register_as_package(L, "wgui", wguiFuncs);
	register_as_package(L, "d2d", d2dFuncs);
	register_as_package(L, "input", inputFuncs);
	register_as_package(L, "joypad", joypadFuncs);
	register_as_package(L, "movie", movieFuncs);
	register_as_package(L, "savestate", savestateFuncs);
	register_as_package(L, "iohelper", ioHelperFuncs);
	register_as_package(L, "avi", aviFuncs);

	// COMPAT: table.getn deprecated, replaced by # prefix
	luaL_dostring(L, "table.getn = function(t) return #t end");

	// COMPAT: emu.debugview deprecated, forwarded to print
	luaL_dostring(L, "emu.debugview = print");

	// COMPAT: emu.isreadonly deprecated, forwarded to movie.isreadonly
	luaL_dostring(L, "emu.isreadonly = movie.isreadonly");

	// os.execute poses security risks
	luaL_dostring(L, "os.execute = function() print('os.execute is disabled') end");
}

