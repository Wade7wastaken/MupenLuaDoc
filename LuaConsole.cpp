﻿/*
	
*/


#include "LuaConsole.h"

#include <vector>
#include <algorithm>
#include <string>
#include <map>
#include <list>
#include <filesystem>
#include "../../winproject/resource.h"
#include "win/DebugInfo.hpp"
#include "win/main_win.h"
#include "win/Config.h"
#include "include/lua.hpp"
#include "../memory/memory.h"
#include "../r4300/r4300.h"
#include "../r4300/recomp.h"
#include "../main/plugin.h"
#include "../main/disasm.h"
#include "../main/vcr.h"
#include "../main/savestates.h"
#include "../main/win/Config.h"
#include "../main/win/configdialog.h"
#include "../main/win/wrapper/ReassociatingFileDialog.h"
#include <vcr.h>
#include <gdiplus.h>
#include "../main/vcr_compress.h"


#ifdef LUA_MODULEIMPL
//nice msvc pragma smh
#pragma comment(lib, "lua54.lib")
#pragma comment (lib,"Gdiplus.lib")
#endif

extern unsigned long op;
extern void (*interp_ops[64])(void);
extern int m_currentVI;
extern long m_currentSample;
extern int fast_memory;
extern int shouldSave;
void SYNC();
void NOTCOMPILED();
void InitTimer();
inline void TraceLoggingBufFlush();
extern void(__cdecl* CaptureScreen) (char* Directory);// for lua screenshot

unsigned long lastInputLua[4];
unsigned long rewriteInputLua[4];
bool rewriteInputFlagLua[4];
bool enableTraceLog;
bool traceLogMode;
bool enablePCBreak;
bool maximumSpeedMode;
bool anyLuaRunning = false;
bool gdiPlusInitialized = false;
ULONG_PTR gdiPlusToken;

// LoadScreen variables
HDC hwindowDC, hsrcDC;
SWindowInfo windowSize{};
HBITMAP hbitmap;
bool LoadScreenInitialized = false;

// Deletes all the variables used in LoadScreen (avoid memory leaks)
void LoadScreenDelete() {
	ReleaseDC(mainHWND, hwindowDC);
	DeleteDC(hsrcDC);

	LoadScreenInitialized = false;
}

// Initializes everything needed for LoadScreen
void LoadScreenInit() {
	if (LoadScreenInitialized) LoadScreenDelete();

	hwindowDC = GetDC(mainHWND);// Create a handle to the main window Device Context
	hsrcDC = CreateCompatibleDC(hwindowDC);// Create a DC to copy the screen to

	CalculateWindowDimensions(mainHWND, windowSize);
	windowSize.height -= 1; // ¯\_(ツ)_/¯
	printf("LoadScreen Size: %d x %d\n", windowSize.width, windowSize.height);

	// create an hbitmap
	hbitmap = CreateCompatibleBitmap(hwindowDC, windowSize.width, windowSize.height);

	LoadScreenInitialized = true;
}

#ifdef LUA_MODULEIMPL

#define DEBUG_GETLASTERROR 0//if(GetLastError()){ShowInfo("Line:%d GetLastError:%d",__LINE__,GetLastError());SetLastError(0);}

ReassociatingFileDialog fdOpenLuaScript; // internal picker
ReassociatingFileDialog fdOpenLua; // api picker
ReassociatingFileDialog fdLuaTraceLog;


namespace LuaEngine {
std::vector<HWND> luaWindows;
RECT InitalWindowRect[3] = {0};
HANDLE TraceLogFile;



class Lua;

std::vector<Gdiplus::Bitmap*> imagePool;

struct AddrBreakFunc{
	lua_State *lua;
	int idx;
};
typedef std::vector<AddrBreakFunc> AddrBreakFuncVec;
struct AddrBreak {
	AddrBreakFuncVec func;
};
struct SyncBreak {
	AddrBreakFuncVec func;
	unsigned op;
	precomp_instr pc;
};
struct MemoryHashInfo {
	unsigned count;
	void(*func[4])();
};
void (**readFuncHashMap[])() = {
		readmemb, readmemh, readmem, readmemd
};
void (**writeFuncHashMap[])() = {
		writememb, writememh, writemem, writememd
};
typedef std::map<ULONG, AddrBreak> AddrBreakMap;
typedef std::map<ULONG, SyncBreak> SyncBreakMap;
SyncBreakMap syncBreakMap;
AddrBreakMap readBreakMap;
AddrBreakMap writeBreakMap;
MemoryHashInfo *readHashMap[0x10000];
MemoryHashInfo *writeHashMap[0x10000];
void *pcBreakMap_[0x800000/4];
unsigned pcBreakCount = 0;
#define pcBreakMap ((AddrBreakFuncVec**)pcBreakMap_)
ULONGLONG break_value;	//read/write���p
bool break_value_flag;
unsigned inputCount = 0;
size_t current_break_value_size = 1;
std::map<HWND, Lua*> luaWindowMap;
int getn(lua_State*);

HDC lua_dc;
int lua_dc_width, lua_dc_height;

//improved debug print from stackoverflow, now shows function info
#ifdef _DEBUG
static void stackDump(lua_State* L) {
	int i = lua_gettop(L);
	printf(" ----------------  Stack Dump ----------------\n");
	while (i) {
		int t = lua_type(L, i);
		switch (t) {
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
			printf("%d: %s %p, type: %s", i, "function at", lua_topointer(L,-1), ar.what);
			break;
		default: printf("%d: %s", i, lua_typename(L, t)); break;
		}
		printf("\n");
		i--;
	}
	printf("--------------- Stack Dump Finished ---------------\n");
}
#endif
void ASSERT(bool e, const char *s = "assert"){
	if(!e) {
		DebugBreak();
	}
}
struct EmulationLock{
	EmulationLock()
	{
		ShowInfo("Emulation Lock");
		pauseEmu(FALSE);
	}
	~EmulationLock()
	{
		resumeEmu(FALSE);
		ShowInfo("Emulation Unlock");
	}
};

void ConsoleWrite(HWND, const char*);
void SetWindowLua(HWND wnd, Lua *lua);
void SetButtonState(HWND wnd, bool state);
void SetLuaClass(lua_State *L, void *lua);
int AtPanic(lua_State *L);
SyncBreakMap::iterator RemoveSyncBreak(SyncBreakMap::iterator it);
template<bool rw>
AddrBreakMap::iterator RemoveMemoryBreak(AddrBreakMap::iterator it);
AddrBreakFuncVec::iterator RemovePCBreak(AddrBreakFuncVec &f, AddrBreakFuncVec::iterator it);
extern const luaL_Reg globalFuncs[];
extern const luaL_Reg emuFuncs[];
extern const luaL_Reg guiFuncs[];
extern const luaL_Reg wguiFuncs[];
extern const luaL_Reg memoryFuncs[];
extern const luaL_Reg inputFuncs[];
extern const luaL_Reg joypadFuncs[];
extern const luaL_Reg movieFuncs[];
extern const luaL_Reg savestateFuncs[];
extern const luaL_Reg ioHelperFuncs[];
extern const luaL_Reg aviFuncs[];
extern const char * const REG_ATSTOP;
int AtStop(lua_State *L); 

class Lua {
private:
	bool stopping = false;

public:
	Lua(HWND wnd):
		L(NULL),
		ownWnd(wnd){
		SetWindowLua(wnd, this);
		ShowInfo("Lua construct");
	}
	~Lua(){
		if(isrunning()) {
			stop();
		}
		SetWindowLua(ownWnd, NULL);
		ShowInfo("Lua destruct");
	}
	void run(char *path){
		stopping = false;
		std::string pathStr(path);
		if (&path == NULL || pathStr.substr(pathStr.find_last_of(".") + 1).compare("lua"))
			return;

		ASSERT(!isrunning());
		brush = (HBRUSH)GetStockObject(WHITE_BRUSH);
		pen = (HPEN)GetStockObject(BLACK_PEN);
		font = (HFONT)GetStockObject(SYSTEM_FONT);
		col = bkcol = 0;
		bkmode = TRANSPARENT;
		hMutex = CreateMutex(0, 0, 0);
		LoadScreenInit();
		newLuaState();
		runFile(path);
		if (isrunning())
		{
			SetButtonState(ownWnd, true);
			AddToRecentScripts(path);
			strcpy(Config.lua_script_path, path);
		}
		ShowInfo("Lua run");
	}
	void stop() {
		if(!isrunning())
			return;
		
		stopping = true;
		registerFuncEach(AtStop, REG_ATSTOP);
		deleteGDIObject(brush, WHITE_BRUSH);
		deleteGDIObject(pen, BLACK_PEN);
		deleteGDIObject(font, SYSTEM_FONT);
		deleteLuaState();
		for (auto x : imagePool)
		{
			delete x;
		}
		imagePool.clear();
		LoadScreenDelete();
		SetButtonState(ownWnd, false);
		ShowInfo("Lua stop");
	}
	bool isrunning() {
		return L != NULL && !stopping;
	}
	void setBrush(HBRUSH h) {
		setGDIObject((HGDIOBJ*)&brush, h);
	}
	void selectBrush() {
		selectGDIObject(brush);
	}
	void setPen(HPEN h) {
		setGDIObject((HGDIOBJ*)&pen, h);
	}
	void selectPen() {
		selectGDIObject(pen);
	}
	void setFont(HFONT h) {
		setGDIObject((HGDIOBJ*)&font, h);
	}
	void selectFont() {
		selectGDIObject(font);
	}
	void setTextColor(COLORREF c) {
		col = c;
	}
	void selectTextColor() {
		::SetTextColor(lua_dc, col);
	}
	void setBackgroundColor(COLORREF c, int mode = OPAQUE) {
		bkcol = c;
		bkmode = mode;
	}
	void selectBackgroundColor() {
		::SetBkMode(lua_dc, bkmode);
		::SetBkColor(lua_dc, bkcol);
	}
	//calls all functions that lua script has defined as callbacks, reads them from registry
	//returns true at fail
	bool registerFuncEach(int(*f)(lua_State*), const char *key) {
		lua_getfield(L, LUA_REGISTRYINDEX, key);
		//shouldn't ever happen but could cause kernel panic
		if lua_isnil(L, -1) { lua_pop(L, 1); return false; }
		int n = luaL_len(L, -1);
		for(LUA_INTEGER i = 0; i < n; i ++) {
			lua_pushinteger(L, 1+i);
			lua_gettable(L, -2);
#ifdef _DEBUG
			//printf("Load state...\n");
			//stackDump(L);
#endif
			if(f(L)){
				error();
				return true;
			}
		}
		lua_pop(L, 1);
		return false;
	}
	bool errorPCall(int a, int r){
		if(lua_pcall(L, a, r, 0)) {
			error();
			return true;
		}
		return false;
	}
	
	HWND ownWnd;
	HANDLE hMutex;
private:
	void newLuaState(){
		ASSERT(L==0);
		L = luaL_newstate();
		lua_atpanic(L, AtPanic);
		SetLuaClass(L, this);
		registerFunctions();
	}
	void deleteLuaState(){
		if(L == NULL)*(int*)0=0;
		correctData();
		lua_close(L);
		L = NULL;
	}

	void registerAsPackage(lua_State *L, const char* name,const luaL_Reg reg[])
	{
		luaL_newlib(L, reg);
		lua_setglobal(L, name);
	}

	void registerFunctions(){
		luaL_openlibs(L);
		//�Ȃ�luaL_register(L, NULL, globalFuncs)����Ɨ�����
		const luaL_Reg *p = globalFuncs;
		do{
			lua_register(L, p->name, p->func);
		}while((++p)->func);
		registerAsPackage(L, "emu", emuFuncs);
		registerAsPackage(L, "memory", memoryFuncs);
		registerAsPackage(L, "gui", guiFuncs);
		registerAsPackage(L, "wgui", wguiFuncs);
		registerAsPackage(L, "input", inputFuncs);
		registerAsPackage(L, "joypad", joypadFuncs);
		registerAsPackage(L, "movie", movieFuncs);
		registerAsPackage(L, "savestate", savestateFuncs);
		registerAsPackage(L, "iohelper", ioHelperFuncs);
		registerAsPackage(L, "avi", aviFuncs);

		//this makes old scripts backward compatible, new syntax for table length is '#'
		lua_getglobal(L, "table");
		lua_pushcfunction(L, getn);
		lua_setfield(L, -2, "getn");
		lua_pop(L, 1);
	}
	void runFile(char *path) {
		//int GetErrorMessage(lua_State *L);
		int result;
		//lua_pushcfunction(L, GetErrorMessage);
		result = luaL_dofile(L, path);
		if(result) {
			error();
			return;
		}
		return;
	}

	void error() {
		const char *str = lua_tostring(L, -1);
		ConsoleWrite(ownWnd, str);
		ConsoleWrite(ownWnd, "\r\n");
		ShowInfo("Lua error: %s", str);
		stop();
	}
	void setGDIObject(HGDIOBJ *save, HGDIOBJ newobj) {
		if(*save)
			::DeleteObject(*save);
		DEBUG_GETLASTERROR;
		*save = newobj;
	}
	void selectGDIObject(HGDIOBJ p) {
		SelectObject(lua_dc, p);
		DEBUG_GETLASTERROR;
	}
	void deleteGDIObject(HGDIOBJ p, int stockobj) {
		SelectObject(lua_dc, GetStockObject(stockobj));
		DeleteObject(p);
	}
	template<typename T>
	void correctBreakMap(T &breakMap,
		typename T::iterator(*removeFunc)(typename T::iterator)) {
		for(typename T::iterator it =
			breakMap.begin(); it != breakMap.end();) {
			AddrBreakFuncVec &f = it->second.func;
			for(AddrBreakFuncVec::iterator itt = f.begin();
				itt != f.end(); ) {
				if(itt->lua == L) {
					itt = f.erase(itt);
				}else {
					itt ++;
				}
			}
			if(f.empty()) {
				it = removeFunc(it);
			}else {
				it ++;
			}
		}

	}
	void correctPCBreak(){
		for(AddrBreakFuncVec **p = pcBreakMap;
			p < &pcBreakMap[0x800000/4]; p ++) {
			if(*p) {
				AddrBreakFuncVec &f = **p;
				for(AddrBreakFuncVec::iterator it = f.begin();
					it != f.end(); ) {
					if(it->lua == L) {
						it = RemovePCBreak(f, it);
					}else {
						it ++;
					}
				}
				if(f.empty()) {
					delete *p;
					*p = NULL;
				}
			}
		}
	}
	void correctData(){
		correctBreakMap<SyncBreakMap>(syncBreakMap, RemoveSyncBreak);
		correctBreakMap<AddrBreakMap>(readBreakMap, RemoveMemoryBreak<false>);
		correctBreakMap<AddrBreakMap>(writeBreakMap, RemoveMemoryBreak<true>);
		correctPCBreak();
	}
	lua_State *L;
	//DCobjects
	HBRUSH brush;
	HPEN pen;
	HFONT font;
	COLORREF col, bkcol;
	int bkmode;
};
int AtPanic(lua_State *L) {
	printf("Lua panic: %s\n", lua_tostring(L, -1));
	MessageBox(mainHWND, lua_tostring(L, -1), "Lua Panic", 0);
	return 0;
}
//�E�B���h�E�̃X���b�h���炾�Ƃ���Ȃ̂�
//�G�~�����[�V�����̃X���b�h����̂�Lua��M��Ƃ������Ƃ�
//�t�ɃG�~�����[�V�����X���b�h����E�B���h�E�X���b�h�𓮂������Ƃ͂���
//ConsoleWrite�Ƃ�
void SetWindowLua(HWND,Lua*);
Lua *GetWindowLua(HWND);
void destroy_lua_dc();
void registerFuncEach(int(*f)(lua_State*), const char *key);
extern const char * const REG_WINDOWMESSAGE;
int AtWindowMessage(lua_State *L);
void CreateLuaWindow(void(*callback)());
//#define EnterCriticalSection(a) EnterCriticalSection((ShowInfo("Enter thread:%d line:%d",GetCurrentThreadId(),__LINE__),a))
//#define LeaveCriticalSection(a) LeaveCriticalSection((ShowInfo("Leave thread:%d line:%d",GetCurrentThreadId(),__LINE__),a))


void checkGDIPlusInitialized() {
	// will be inlined by compiler
	if (!gdiPlusInitialized) {
		printf("lua initialize gdiplus\n");
		Gdiplus::GdiplusStartupInput gdiplusStartupInput;
		Gdiplus::GdiplusStartup(&gdiPlusToken, &gdiplusStartupInput, NULL);
		gdiPlusInitialized = true;
	}
}

class LuaMessage {
public:
	struct Msg;
private:
	Msg *get() {
		EnterCriticalSection(&cs);
		if(msgs.empty()) {
			LeaveCriticalSection(&cs);
			return NULL;
		}
		Msg *msg = msgs.front();
		msgs.pop_front();
		LeaveCriticalSection(&cs);
		return msg;
	}
	CRITICAL_SECTION cs;
	std::list<Msg*> msgs;
public:
	LuaMessage(){
		InitializeCriticalSection(&cs);
	}
	~LuaMessage(){
		DeleteCriticalSection(&cs);
	}
	enum MSGTYPE {
		NewLua,
		DestroyLua,
		RunPath,
		StopCurrent,
		CloseAll,
		ReloadFirst,
		WindowMessage,
	};
	struct Msg {
		MSGTYPE type;
		union {
			struct {
				HWND wnd;
				void(*callback)();
			} newLua;
			struct {
				HWND wnd;
			} destroyLua;
			struct {
				HWND wnd;
				char path[MAX_PATH];
			} runPath;
			struct {
				HWND wnd;
			} stopCurrent;
			struct {
				HWND wnd;
				UINT msg;
				WPARAM wParam;
				LPARAM lParam;
			} windowMessage;
		};
	};
	void post(Msg *msg){
		EnterCriticalSection(&cs);
		msgs.push_back(msg);
		LeaveCriticalSection(&cs);
	}
//#define post(msg) post((ShowInfo("type: %d, line:%d",msg->type,__LINE__), msg))
	void getMessages() {
//		ShowInfo("getMessages");
		Msg *msg;
		while(msg = get()) {
//		ShowInfo("msg: type: %d", msg->type);
			current_msg = msg;
			switch(msg->type) {
			case NewLua:{
				HWND wnd = msg->newLua.wnd;
				Lua *lua = new Lua(wnd);
				SetWindowLua(wnd, lua);
				luaWindows.push_back(wnd);
				if(msg->newLua.callback)
					msg->newLua.callback();
				break;
			}
			case DestroyLua:{
				HWND wnd = msg->destroyLua.wnd;
				Lua *lua = GetWindowLua(wnd);
				luaWindows.erase(std::find(
					luaWindows.begin(), luaWindows.end(), wnd));
				if (lua) { lua->stop(); }
				if(luaWindows.empty()) {
					destroy_lua_dc();
					anyLuaRunning = false;
				}
				delete lua;
				break;
			}
			case RunPath:{
				HWND wnd;
				if (msg->runPath.wnd == NULL) {
					if (luaWindows.size() == 0) {
						break; // silently skip the message if it can't be run
					}
					wnd = luaWindows.back();
					SetWindowText(GetDlgItem(wnd, IDC_TEXTBOX_LUASCRIPTPATH), msg->runPath.path);
				}
				else {
					wnd = msg->runPath.wnd;
				}
				Lua *lua = GetWindowLua(wnd);
				if(lua) {
					lua->stop();
					lua->run(msg->runPath.path);
				}
				break;
			}
			case StopCurrent:
				GetWindowLua(msg->stopCurrent.wnd)->stop();
				break;
			case CloseAll:{
				std::vector<HWND>::iterator it;
				std::vector<HWND> copy(luaWindows);
				for(it = copy.begin(); it != copy.end(); it ++) {
					PostMessage(*it, WM_CLOSE, 0, 0);
				}
				anyLuaRunning = false;
				break;
			}
			case ReloadFirst:{
				if(luaWindows.empty()) {
					PostMessage(mainHWND, WM_COMMAND,
						ID_MENU_LUASCRIPT_NEW, (LPARAM)LuaReload);
				}else {
					HWND wnd = luaWindows[0];
					PostMessage(wnd,
						WM_COMMAND, IDC_BUTTON_LUASTATE|(0<<16),
						(LPARAM)GetDlgItem(wnd, IDC_BUTTON_LUASTATE));
				}
				break;
			}
			case WindowMessage:{
				registerFuncEach(AtWindowMessage, REG_WINDOWMESSAGE);
				break;
			}
			}
			delete msg;
		}
		current_msg = NULL;
	}
	Msg *current_msg;
};
LuaMessage luaMessage;

Lua *GetWindowLua(HWND wnd) {
	return luaWindowMap.find(wnd)->second;
}
void SetWindowLua(HWND wnd, Lua *lua) {
	if(lua)
		luaWindowMap[wnd] = lua;
	else
		luaWindowMap.erase(wnd);
}
void SizingControl(HWND wnd, RECT *p, int x, int y, int w, int h) {
	SetWindowPos(wnd, NULL, p->left+x, p->top+y,
		p->right-p->left+w, p->bottom-p->top+h, SWP_NOZORDER);
}
void SizingControls(HWND wnd, WORD width, WORD height) {
	if (Config.is_lua_simple_dialog_enabled)return;
	int xa = width - (InitalWindowRect[0].right - InitalWindowRect[0].left),
		ya = height - (InitalWindowRect[0].bottom - InitalWindowRect[0].top);
	SizingControl(GetDlgItem(wnd, IDC_TEXTBOX_LUASCRIPTPATH),
		&InitalWindowRect[1], 0, 0, xa, 0);
	SizingControl(GetDlgItem(wnd, IDC_TEXTBOX_LUACONSOLE),
		&InitalWindowRect[2], 0, 0, xa, ya);
}
void GetInitalWindowRect(HWND wnd) {
	GetClientRect(wnd, &InitalWindowRect[0]);
	GetWindowRect(GetDlgItem(wnd, IDC_TEXTBOX_LUASCRIPTPATH),
		&InitalWindowRect[1]);
	GetWindowRect(GetDlgItem(wnd, IDC_TEXTBOX_LUACONSOLE),
		&InitalWindowRect[2]);
	MapWindowPoints(NULL, wnd, (LPPOINT)&InitalWindowRect[1], 2*2);
}

BOOL WmCommand(HWND wnd, WORD id, WORD code, HWND control);
INT_PTR CALLBACK DialogProc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
	case WM_INITDIALOG:{
		checkGDIPlusInitialized();
		LuaMessage::Msg *msg = new LuaMessage::Msg();
		msg->type = LuaMessage::NewLua;
		msg->newLua.wnd = wnd;
		msg->newLua.callback = (void(*)())lParam;
		luaMessage.post(msg);
		if(InitalWindowRect[0].right == 0) {	//�蔲���ȁA�ŏ��ł��邱�Ƃ̔���
			GetInitalWindowRect(wnd);
		}
		SetWindowText(GetDlgItem(wnd, IDC_TEXTBOX_LUASCRIPTPATH),
			Config.lua_script_path);
		return TRUE;
	}
	case WM_CLOSE:

		if (Config.is_lua_exit_confirm_enabled
			&& (MessageBox(0, "Are you sure you want to close this dialog and terminate this lua script instance?", "Confirm closing", MB_TASKMODAL | MB_TOPMOST | MB_YESNO | MB_ICONQUESTION) == IDNO))
			return TRUE;

		DestroyWindow(wnd);
		return TRUE;
	case WM_DESTROY:{
		LuaMessage::Msg *msg = new LuaMessage::Msg();
		msg->type = LuaMessage::DestroyLua;
		msg->destroyLua.wnd = wnd;
		luaMessage.post(msg);
		return TRUE;
	}
	case WM_COMMAND:
		return WmCommand(wnd, LOWORD(wParam), HIWORD(wParam), (HWND)lParam);
	case WM_SIZE:
		SizingControls(wnd, LOWORD(lParam), HIWORD(lParam));
		if (wParam == SIZE_MINIMIZED) SetFocus(mainHWND);
		break;
	}
	return FALSE;
}
std::string OpenLuaFileDialog() {
	
	int storePaused = emu_paused;
	pauseEmu(1);

	// The default directory we open the file dialog window in is
	// the parent directory of the last script that the user ran
	char scriptParentDir[MAX_PATH] = "";
	std::filesystem::path scriptPath = Config.lua_script_path;
	strncpy(scriptParentDir, scriptPath.parent_path().string().c_str(), MAX_PATH); // monstrosity

	if (!fdOpenLuaScript.ShowFileDialog(scriptParentDir, L"*.lua", TRUE, FALSE, mainHWND)) {
		if (!storePaused) resumeEmu(1);
		return "";
	}

	if (!storePaused) resumeEmu(1);

	return std::string(scriptParentDir); // umm fuck you
}
void SetButtonState(HWND wnd, bool state) {
	if(!IsWindow(wnd)) return;
	HWND stateButton = GetDlgItem(wnd, IDC_BUTTON_LUASTATE),
		stopButton = GetDlgItem(wnd, IDC_BUTTON_LUASTOP);
	if(state) {
		SetWindowText(stateButton, "Restart");
		SetWindowLongPtr(stopButton, GWL_STYLE,
			GetWindowLongPtr(stopButton, GWL_STYLE) & ~WS_DISABLED);
	}else {
		SetWindowText(stateButton, "Run");
		SetWindowLongPtr(stopButton, GWL_STYLE,
			GetWindowLongPtr(stopButton, GWL_STYLE) | WS_DISABLED);
	}
	//redraw
	InvalidateRect(stateButton, NULL, FALSE);
	InvalidateRect(stopButton, NULL, FALSE);
}


BOOL WmCommand(HWND wnd, WORD id, WORD code, HWND control){
	switch (id) {
	case IDC_BUTTON_LUASTATE: {
		LuaMessage::Msg* msg = new LuaMessage::Msg();
		msg->type = LuaMessage::RunPath;
		msg->runPath.wnd = wnd;

		GetWindowText(GetDlgItem(wnd, IDC_TEXTBOX_LUASCRIPTPATH),
		msg->runPath.path, MAX_PATH);
		//strcpy(Config.lua_script_path, msg->runPath.path);

		if (Config.is_lua_simple_dialog_enabled)
			SetWindowText(wnd, msg->runPath.path);

		anyLuaRunning = true;
		luaMessage.post(msg);
		shouldSave = true;
		return TRUE;
	}
	case IDC_BUTTON_LUASTOP: {
		LuaMessage::Msg* msg = new LuaMessage::Msg();
		msg->type = LuaMessage::StopCurrent;
		msg->stopCurrent.wnd = wnd;
		luaMessage.post(msg);
		// save config?
		return TRUE;
	}
	case IDC_BUTTON_LUABROWSE: {
		std::string newPath = OpenLuaFileDialog();
		if (!newPath.empty())
			SetWindowText(GetDlgItem(wnd, IDC_TEXTBOX_LUASCRIPTPATH),
				newPath.c_str());
		shouldSave = true;
		return TRUE;
	}
	case IDC_BUTTON_LUAEDIT: {
		CHAR buf[MAX_PATH];
		GetWindowText(GetDlgItem(wnd, IDC_TEXTBOX_LUASCRIPTPATH),
			buf, MAX_PATH);
		if (buf == NULL || strlen(buf) == 0)/* || strlen(buf)>MAX_PATH*/
			return FALSE; // previously , clicking edit with empty path will open current directory in explorer, which is very bad

		ShellExecute(0, 0, buf, 0, 0, SW_SHOW);
		return TRUE;
	}
	case IDC_BUTTON_LUACLEAR:
		if (GetAsyncKeyState(VK_MENU)) {
			// clear path
			SetWindowText(GetDlgItem(wnd, IDC_TEXTBOX_LUASCRIPTPATH), "");
			return TRUE;
		}

		SetWindowText(GetDlgItem(wnd, IDC_TEXTBOX_LUACONSOLE), "");
		return TRUE;
	}
	return FALSE;
}

void CreateLuaWindow(void(*callback)()) {

	if (LuaCriticalSettingChangePending)return;

	if(!lua_dc) {
		InitializeLuaDC(mainHWND);
	}

	int LuaWndId = Config.is_lua_simple_dialog_enabled ? IDD_LUAWINDOW_SIMPLIFIED : IDD_LUAWINDOW;

	HWND wnd = CreateDialogParam(app_hInstance,
		MAKEINTRESOURCE(LuaWndId), mainHWND, DialogProc,
		(LPARAM)callback);

	ShowWindow(wnd, SW_SHOW);	//�^�u�X�g�b�v�����Ȃ��̂Ɠ����������Ǝv��
}
void ConsoleWrite(HWND wnd, const char *str) {
	HWND console = GetDlgItem(wnd, IDC_TEXTBOX_LUACONSOLE);

	int length = GetWindowTextLength(console);
	if(length >= 0x7000) {
		SendMessage(console, EM_SETSEL, 0, length/2);
		SendMessage(console, EM_REPLACESEL, false, (LPARAM)"");
		length = GetWindowTextLength(console);
	}
	SendMessage(console, EM_SETSEL, length, length);
	SendMessage(console, EM_REPLACESEL, false, (LPARAM)str);
}
/*
const COLORREF LUADC_BG_COLOR = 0x000000;
const COLORREF LUADC_BG_COLOR_A = 0x010101;
*/
LRESULT CALLBACK LuaGUIWndProc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
	case WM_CREATE:
	case WM_DESTROY:
		return 0;
	}
	return DefWindowProc(wnd, msg, wParam, lParam);

}
void InitializeLuaDC_(HWND mainWnd){
	if (lua_dc) {
		destroy_lua_dc();
	}

	RECT window_rect;
	GetClientRect(mainWnd, &window_rect);
	lua_dc_width = window_rect.right;
	lua_dc_height = window_rect.bottom;

	if (Config.is_lua_double_buffered) {
		HDC main_dc = GetDC(mainWnd);
		lua_dc = CreateCompatibleDC(main_dc);
		HBITMAP bmp = CreateCompatibleBitmap(main_dc, window_rect.right, window_rect.bottom);
		SelectObject(lua_dc, bmp);
		ReleaseDC(mainWnd, main_dc);
	}
	else {
		lua_dc = GetDC(mainWnd);
	}

}

void copy_lua_dc_to_window(){
	if (Config.is_lua_double_buffered) {
		HDC main_dc = GetDC(mainHWND);
		BitBlt(main_dc, 0, 0, lua_dc_width, lua_dc_height, lua_dc, 0, 0, SRCCOPY);
		ReleaseDC(mainHWND, main_dc);
		DEBUG_GETLASTERROR;
	}
}
void copy_window_to_lua_dc(){
	if (Config.is_lua_double_buffered) {
		HDC main_dc = GetDC(mainHWND);
		BitBlt(lua_dc, 0, 0, lua_dc_width, lua_dc_height, main_dc, 0, 0, SRCCOPY);
		ReleaseDC(mainHWND, main_dc);
		DEBUG_GETLASTERROR;
	}
}
void destroy_lua_dc() {
	ReleaseDC(mainHWND, lua_dc);
	lua_dc = NULL;
}

void RecompileNextAll();
void RecompileNow(ULONG);
void TraceLogStart(const char *name, BOOL append = FALSE){
	if(TraceLogFile = CreateFile(name, GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
		append ? OPEN_ALWAYS : CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL)) {
		if(append) {
			SetFilePointer(TraceLogFile, 0, NULL, FILE_END);
		}
		enableTraceLog = true;
		if(interpcore==0) {
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
void TraceLogStop(){
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

//������ւ񂩂�EmuLua���Ċ�����

const char * const REG_LUACLASS = "C";
const char * const REG_ATUPDATESCREEN = "S";
const char * const REG_ATVI = "V";
const char * const REG_ATINPUT = "I";
const char * const REG_ATSTOP = "T";
const char * const REG_SYNCBREAK = "B";
const char * const REG_PCBREAK = "P";
const char * const REG_READBREAK = "R";
const char * const REG_WRITEBREAK = "W";
const char * const REG_WINDOWMESSAGE = "M";
const char * const REG_ATINTERVAL = "N";
const char* const REG_ATPLAYMOVIE = "PM";
const char* const REG_ATSTOPMOVIE = "SM";
const char* const REG_ATLOADSTATE = "LS";
const char* const REG_ATSAVESTATE = "SS";
const char* const REG_ATRESET = "RE";

Lua *GetLuaClass(lua_State *L) {
	lua_getfield(L, LUA_REGISTRYINDEX, REG_LUACLASS);
	Lua *lua = (Lua*)lua_topointer(L, -1);
	lua_pop(L, 1);
	return lua;
}
void SetLuaClass(lua_State *L, void *lua) {
	lua_pushlightuserdata(L, lua);
	lua_setfield(L, LUA_REGISTRYINDEX, REG_LUACLASS);
	//lua_pop(L, 1); //iteresting, it worked before
}
int GetErrorMessage(lua_State *L) {
	return 1;
}

int getn(lua_State* L)
{
	lua_pushinteger(L, luaL_len(L, -1));
	return 1;
}

//lua�̕⏕�֐��Ƃ�
DWORD CheckIntegerU(lua_State *L, int i = -1) {
	return (DWORD)luaL_checknumber(L, i);
}
void PushIntU(lua_State *L, unsigned int x) {
	lua_pushinteger(L, x);
}
void PushDword(lua_State *L, ULONGLONG x) {
	lua_newtable(L);
	lua_pushinteger(L, 1);
	PushIntU(L, x>>32);
	lua_settable(L, -3);
	lua_pushinteger(L, 2);
	PushIntU(L, x&0xFFFFFFFF);
	lua_settable(L, -3);
}
ULONGLONG CheckDword(lua_State *L, int i) {
	ULONGLONG n;
	lua_pushinteger(L, 1);
	lua_gettable(L, i);
	n = (ULONGLONG)CheckIntegerU(L) << 32;
	lua_pop(L, 1);
	lua_pushinteger(L, 2);
	lua_gettable(L, i);
	n |= CheckIntegerU(L);
	return n;
}
int RegisterFunction(lua_State *L, const char *key) {
	lua_getfield(L, LUA_REGISTRYINDEX, key);
	if(lua_isnil(L, -1)) {
		lua_pop(L, 1);
		lua_newtable(L);
		lua_setfield(L, LUA_REGISTRYINDEX, key);
		lua_getfield(L, LUA_REGISTRYINDEX, key);
	}
	int i = luaL_len(L, -1)+1;
	lua_pushinteger(L, i);
	lua_pushvalue(L, -3);	//
	lua_settable(L, -3);
	lua_pop(L, 1);
	return i;
}
void UnregisterFunction(lua_State *L, const char *key) {
	lua_getfield(L, LUA_REGISTRYINDEX, key);
	if(lua_isnil(L, -1)){
		lua_pop(L, 1);
		lua_newtable(L);	//�Ƃ肠����
	}
	int n = luaL_len(L, -1);
	for(LUA_INTEGER i = 0; i < n; i ++) {
		lua_pushinteger(L, 1+i);
		lua_gettable(L, -2);
		if(lua_rawequal(L, -1, -3)) {
			lua_pop(L, 1);
			lua_getglobal(L, "table");
			lua_getfield(L, -1, "remove");
			lua_pushvalue(L, -3);
			lua_pushinteger(L, 1+i);
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
void registerFuncEach(int(*f)(lua_State*), const char *key) {
	for(std::vector<HWND>::iterator it = luaWindows.begin();
		it != luaWindows.end(); it ++) {
		Lua *lua = GetWindowLua(*it);
		if(lua && lua->isrunning()) {
#ifdef WIN32
			//ensure thread safety (load and savestate callbacks for example)
			DWORD waitResult = WaitForSingleObject(lua->hMutex,INFINITE);
			switch (waitResult)
			{
			case WAIT_OBJECT_0:
#endif
				if (lua->registerFuncEach(f, key)) {
					printf("!ERRROR, RETRY\n");
					//if error happened, try again (but what if it fails again and again?)
					registerFuncEach(f, key);
					return;
				}
#ifdef WIN32
				ReleaseMutex(lua->hMutex);
#endif
			}
		}
	}
}


//������ւ񂩂�֐�
int ToStringExs(lua_State *L);
int Print(lua_State *L) {
	lua_pushcfunction(L, ToStringExs);
	lua_insert(L, 1);
	lua_call(L, lua_gettop(L)-1, 1);
	const char *str = lua_tostring(L, 1);
	HWND wnd = GetLuaClass(L)->ownWnd;
	ConsoleWrite(wnd, str);
	ConsoleWrite(wnd, "\r\n");
	return 0;
}
int StopScript(lua_State* L) {
	HWND wnd = GetLuaClass(L)->ownWnd;
	GetWindowLua(wnd)->stop();
	return 0;
}
int ToStringEx(lua_State *L) {	
	switch(lua_type(L, -1)) {
	case LUA_TNIL:
	case LUA_TBOOLEAN:
	case LUA_TFUNCTION:
	case LUA_TUSERDATA:
	case LUA_TTHREAD:
	case LUA_TLIGHTUSERDATA:
	case LUA_TNUMBER:
		lua_getglobal(L, "tostring");
		lua_pushvalue(L, -2);
		lua_call(L, 1, 1);
		lua_insert(L, lua_gettop(L)-1);
		lua_pop(L, 1);
		break;
	case LUA_TSTRING:
		lua_getglobal(L, "string");
		lua_getfield(L, -1, "format");
		lua_pushstring(L, "%q");
		lua_pushvalue(L, -4);
		lua_call(L, 2, 1);
		lua_insert(L, lua_gettop(L)-2);
		lua_pop(L, 2);
		break;
	case LUA_TTABLE:{
		lua_pushvalue(L, -1);
		lua_rawget(L, 1);
		if(lua_toboolean(L, -1)) {
			lua_pop(L, 2);
			lua_pushstring(L, "{...}");
			return 1;
		}
		lua_pop(L, 1);
		lua_pushvalue(L, -1);
		lua_pushboolean(L, TRUE);
		lua_rawset(L, 1);
		int isArray = 0;
		std::string s("{");
		lua_pushnil(L);
		if(lua_next(L, -2)){
		while(1) {
			lua_pushvalue(L, -2);
			if(lua_type(L, -1) == LUA_TNUMBER &&
				isArray + 1 == lua_tonumber(L, -1)){
				lua_pop(L, 1);
				isArray ++;
			}else { isArray = -1;
			if(lua_type(L, -1) == LUA_TSTRING) {
				s.append(lua_tostring(L, -1));
				lua_pop(L, 1);
			}else {
				ToStringEx(L);
				s.append("[").append(lua_tostring(L, -1)).append("]");
				lua_pop(L, 1);
			}}
			ToStringEx(L);
			if(isArray == -1) {
				s.append("=");
			}
			s.append(lua_tostring(L, -1));
			lua_pop(L, 1);
			if(!lua_next(L, -2))break;
			s.append(", ");
		}
		}
		lua_pop(L, 1);
		s.append("}");
		lua_pushstring(L, s.c_str());
		break;
		}
	}
	return 1;
}
int ToStringExInit(lua_State *L) {
	lua_newtable(L);
	lua_insert(L, 1);
	ToStringEx(L);
	return 1;
}
int ToStringExs(lua_State *L) {
	int len = lua_gettop(L);
	std::string str("");
	for(int i = 0; i < len; i ++) {
		lua_pushvalue(L, 1+i);
		if(lua_type(L, -1) != LUA_TSTRING) {
			ToStringExInit(L);
		}
		str.append(lua_tostring(L, -1));
		lua_pop(L, 1);
		if(i != len-1) {str.append("\t");}
	}
	lua_pushstring(L, str.c_str());
	return 1;
}
int PrintX(lua_State *L) {
	int len = lua_gettop(L);
	std::string str("");
	for(int i = 0; i < len; i ++) {
		lua_pushvalue(L, 1+i);
		if(lua_type(L, -1) == LUA_TNUMBER) {
			int n = CheckIntegerU(L, -1);
			lua_pop(L, 1);
			lua_getglobal(L, "string");
			lua_getfield(L, -1, "format");	//string,string.format
			lua_pushstring(L, "%X");	//s,f,X
			lua_pushinteger(L, n);	//s,f,X,n
			lua_call(L, 2, 1);	//s,r
			lua_insert(L, lua_gettop(L)-1);	//
			lua_pop(L, 1);
		}else if(lua_type(L, -1) == LUA_TSTRING){
			//do nothing
		}else {
			ToStringExInit(L);
		}
		str.append(lua_tostring(L, -1));
		lua_pop(L, 1);
		if(i != len-1) {str.append("\t");}
	}
	ConsoleWrite(GetLuaClass(L)->ownWnd, str.append("\r\n").c_str());
	return 1;
}

int MoveToSingle(lua_State *L) {
	ULONG n = CheckIntegerU(L, 1);
	lua_pushnumber(L, *(FLOAT*)&n);
	return 1;
}
int MoveToDouble(lua_State *L) {
	ULONGLONG n = CheckDword(L, 1);
	lua_pushnumber(L, *(DOUBLE*)&n);
	return 1;
}
int MoveFromSingle(lua_State *L) {
	FLOAT n = luaL_checknumber(L, 1);
	PushIntU(L, *(ULONG*)&n);
	return 1;
}
int MoveFromDouble(lua_State *L) {
	DOUBLE n = luaL_checknumber(L, 1);
	PushDword(L, *(ULONGLONG*)&n);
	return 1;
}
int ConvertDwordToNumber(lua_State *L) {
	lua_pushnumber(L, CheckDword(L, 1));
	return 1;
}

//memory
unsigned char * const rdramb = (unsigned char*)rdram;
const unsigned long AddrMask = 0x7FFFFF;
template<typename T>
ULONG ToAddr(ULONG addr){
	return sizeof(T)==4?
		addr:sizeof(T)==2?
		addr^S16:sizeof(T)==1?
		addr^S8:throw"ToAddr: sizeof(T)";
}
template<typename T>
T LoadRDRAMSafe(unsigned long addr){
	return *((T*)(rdramb + ((ToAddr<T>(addr) & AddrMask))));
}
template<>
ULONGLONG LoadRDRAMSafe(unsigned long addr){
	return ((ULONGLONG)(*(ULONG*)(rdramb + (addr & AddrMask))) << 32) |
     ((*(ULONG*)(rdramb + (addr & AddrMask) + 4)));
}
template<>
LONGLONG LoadRDRAMSafe(unsigned long addr){
	return ((LONGLONG)(*(ULONG*)(rdramb + (addr & AddrMask))) << 32) |
     ((*(ULONG*)(rdramb + (addr & AddrMask) + 4)));
}
template<typename T>
void StoreRDRAMSafe(unsigned long addr, T value){
	*((T*)(rdramb + ((ToAddr<T>(addr) & AddrMask)))) = value;
}
template<>
void StoreRDRAMSafe(unsigned long addr, ULONGLONG value){
   *((unsigned long *)(rdramb + (addr & AddrMask))) = value >> 32;
   *((unsigned long *)(rdramb + (addr & AddrMask) + 4 )) = value & 0xFFFFFFFF;
}
template<>
void StoreRDRAMSafe(unsigned long addr, LONGLONG value){
   *((unsigned long *)(rdramb + (addr & AddrMask))) = value >> 32;
   *((unsigned long *)(rdramb + (addr & AddrMask) + 4 )) = value & 0xFFFFFFFF;
}

int LoadByteUnsigned(lua_State *L) {
	UCHAR value = LoadRDRAMSafe<UCHAR>(CheckIntegerU(L, 1));
	PushIntU(L, value);
	return 1;
}
int LoadByteSigned(lua_State *L) {
	CHAR value = LoadRDRAMSafe<CHAR>(CheckIntegerU(L, 1));
	lua_pushinteger(L, value);
	return 1;
}
int LoadHalfUnsigned(lua_State *L) {
	USHORT value = LoadRDRAMSafe<USHORT>(CheckIntegerU(L, 1));
	PushIntU(L, value);
	return 1;
}
int LoadHalfSigned(lua_State *L) {
	SHORT value = LoadRDRAMSafe<SHORT>(CheckIntegerU(L, 1));
	lua_pushinteger(L, value);
	return 1;
}
int LoadWordUnsigned(lua_State *L) {
	ULONG value = LoadRDRAMSafe<ULONG>(CheckIntegerU(L, 1));
	PushIntU(L, value);
	return 1;
}
int LoadWordSigned(lua_State *L) {
	LONG value = LoadRDRAMSafe<LONG>(CheckIntegerU(L, 1));
	lua_pushinteger(L, value);
	return 1;
}
//64bit�l�͂Ƃ肠����hi,lo�̃e�[�u����
//signed���Ăǂ������ӂ��Ɋi�[�����炢���񂾂�(���͗���unsigned)
int LoadDwordUnsigned(lua_State *L) {
	ULONGLONG value = LoadRDRAMSafe<ULONGLONG>(CheckIntegerU(L, 1));
	PushDword(L, value);
	return 1;
}
int LoadDwordSigned(lua_State *L) {
	LONGLONG value = LoadRDRAMSafe<LONGLONG>(CheckIntegerU(L, 1));
	PushDword(L, value);
	return 1;
}
int LoadFloat(lua_State *L) {
	ULONG value = LoadRDRAMSafe<ULONG>(CheckIntegerU(L, 1));
	lua_pushnumber(L, *(FLOAT*)&value);
	return 1;
}
int LoadDouble(lua_State *L) {
	ULONGLONG value = LoadRDRAMSafe<ULONGLONG>(CheckIntegerU(L, 1));
	lua_pushnumber(L, *(DOUBLE*)value);
	return 1;
}
int StoreByte(lua_State *L) {
	StoreRDRAMSafe<UCHAR>(CheckIntegerU(L, 1), CheckIntegerU(L, 2));
	return 0;
}
int StoreHalf(lua_State *L) {
	StoreRDRAMSafe<USHORT>(CheckIntegerU(L, 1), CheckIntegerU(L, 2));
	return 0;
}
int StoreWord(lua_State *L) {
	StoreRDRAMSafe<ULONG>(CheckIntegerU(L, 1), CheckIntegerU(L, 2));
	return 0;
}
int StoreDword(lua_State *L) {
	StoreRDRAMSafe<ULONGLONG>(CheckIntegerU(L, 1), CheckDword(L, 2));
	return 0;
}
int StoreFloat(lua_State *L) {
	FLOAT f = lua_tonumber(L, -1);
	StoreRDRAMSafe<ULONG>(CheckIntegerU(L, 1), *(ULONG*)&f);
	return 0;
}
int StoreDouble(lua_State *L) {
	DOUBLE f = lua_tonumber(L, -1);
	StoreRDRAMSafe<ULONGLONG>(CheckIntegerU(L, 1), *(ULONGLONG*)&f);
	return 0;
}
int LoadSizeUnsigned(lua_State *L) {
	ULONG addr = CheckIntegerU(L, 1);
	int size = luaL_checkinteger(L, 2);
	switch(size) {
	case 1: PushIntU(L, LoadRDRAMSafe<UCHAR>(addr)); break;
	case 2: PushIntU(L, LoadRDRAMSafe<USHORT>(addr)); break;
	case 4: PushIntU(L, LoadRDRAMSafe<ULONG>(addr)); break;
	case 8: PushDword(L, LoadRDRAMSafe<ULONGLONG>(addr)); break;
	default: luaL_error(L, "size: 1,2,4,8");
	}
	return 1;
}
template<typename T>
int LoadTs(lua_State *L) {
	ULONG addr = CheckIntegerU(L, 1);
	ULONG size = CheckIntegerU(L, 2);
	addr|=0x80000000;
	if(addr+size*sizeof(T)>0x80800000){
		luaL_error(L, "range: 0x80000000 <= n + l < 0x80800000");
	}
	lua_newtable(L);
	T *p = (T*)((unsigned char*)rdram+addr);
	for(ULONG i = 0; i < size; i ++) {
		lua_pushinteger(L, i+1);
		PushIntU(L, *(p++));
		lua_settable(L, -3);
	}
	return 1;
}
int StoreSize(lua_State *L) {
	ULONG addr = CheckIntegerU(L, 1);
	int size = luaL_checkinteger(L, 2);
	switch(size) {
	case 1: StoreRDRAMSafe<UCHAR>(addr, CheckIntegerU(L, 3)); break;
	case 2: StoreRDRAMSafe<USHORT>(addr, CheckIntegerU(L, 3)); break;
	case 4: StoreRDRAMSafe<ULONG>(addr, CheckIntegerU(L, 3)); break;
	case 8: StoreRDRAMSafe<ULONGLONG>(addr, CheckDword(L, 3)); break;
	default: luaL_error(L, "size: 1,2,4,8");
	}
	return 0;
}
// 000000 | 0000 0000 0000 000 | stype(5) = 10101 |001111
const ULONG BREAKPOINTSYNC_MAGIC_STYPE = 0x15;
const ULONG BREAKPOINTSYNC_MAGIC = 0x0000000F|
	(BREAKPOINTSYNC_MAGIC_STYPE<<6);
void PureInterpreterCoreCheck(lua_State *L){
	if(!(dynacore == 0 && interpcore == 1)) {
		luaL_error(L, "this function works only in pure interpreter core"
			"(Menu->Option->Settings->General->CPU Core->Pure Interpreter)");
	}
}
void InterpreterCoreCheck(lua_State *L, const char *s = "") {
	if(dynacore) {
		luaL_error(L, "this function%s works only in (pure) interpreter core"
			"(Menu->Option->Settings->General->CPU Core->Interpreter or Pure Interpreter)",
			s);
	}
}
void Recompile(ULONG);
SyncBreakMap::iterator RemoveSyncBreak(SyncBreakMap::iterator it) {
	StoreRDRAMSafe<ULONG>(it->first, it->second.op);
	if(interpcore==0) {
		Recompile(it->first);
	}
	syncBreakMap.erase(it++);
	return it;
}
void RecompileNow(ULONG addr);
int SetSyncBreak(lua_State *L) {
	InterpreterCoreCheck(L);
	ULONG addr = CheckIntegerU(L, 1) | 0x80000000;
	luaL_checktype(L, 2, LUA_TFUNCTION);
	if(!lua_toboolean(L, 3)) {
		lua_pushvalue(L, 2);
		AddrBreakFunc s{};
		SyncBreakMap::iterator it = syncBreakMap.find(addr);
		if(it == syncBreakMap.end()) {
			SyncBreak b;
			std::pair<SyncBreakMap::iterator, bool> p;
			ULONG op = LoadRDRAMSafe<ULONG>(addr);
			b.op = op;
			if(interpcore==0){
				precomp_instr* s_PC = PC;
				RecompileNow(addr);
				jump_to(addr);
				b.pc = *PC;
				PC->ops = NOTCOMPILED;
				StoreRDRAMSafe<ULONG>(addr, BREAKPOINTSYNC_MAGIC);
				PC = s_PC;
			}else {
				StoreRDRAMSafe<ULONG>(addr, BREAKPOINTSYNC_MAGIC);
			}
			p = syncBreakMap.insert(std::pair<ULONG,SyncBreak>(addr, b));
			it = p.first;
		}
		s.lua = L;
		s.idx = RegisterFunction(L, REG_SYNCBREAK);
		it->second.func.push_back(s);
	}else {
		lua_pushvalue(L, 2);
		SyncBreakMap::iterator it = syncBreakMap.find(addr);
		if(it != syncBreakMap.end()) {
			AddrBreakFuncVec &f = it->second.func;
			AddrBreakFuncVec::iterator itt = f.begin();
			for(; itt != f.end(); itt ++) {
				if(itt->lua == L) {
					lua_getglobal(L, "table");
					lua_getfield(L, -1, "remove");
					lua_getfield(L, LUA_REGISTRYINDEX, REG_SYNCBREAK);
					lua_pushinteger(L, itt->idx);
					lua_call(L, 2, 0);
					lua_pop(L, 1);
					f.erase(itt);
					if(f.size() == 0) {
						RemoveSyncBreak(it);
					}
					return 0;
				}
			}
		}
		luaL_error(L, "SetSyncBreak: not found registry function");
	}
	return 0;
}
template<bool rw>
AddrBreakMap::iterator RemoveMemoryBreak(AddrBreakMap::iterator it) {
	USHORT hash = it->first>>16;
	MemoryHashInfo **hashMap = rw ? writeHashMap : readHashMap;
	MemoryHashInfo *p = hashMap[hash];
	ASSERT(p != NULL);
	if(--p->count == 0) {
		for(int i = 0; i < 4; i ++) {
			(rw ? writeFuncHashMap : readFuncHashMap)[i][hash] =
				p->func[i];
		}
		delete p;
		hashMap[hash] = NULL;
	}
	(rw ? writeBreakMap : readBreakMap).erase(it++);
	return it;
}
extern void(*AtMemoryRead[])();
extern void(*AtMemoryWrite[])();
template <bool rw>
void SetMemoryBreak(lua_State *L) {
//	PureInterpreterCoreCheck(L);
	ULONG addr = CheckIntegerU(L, 1) | 0x80000000;
	luaL_checktype(L, 2, LUA_TFUNCTION);

	AddrBreakMap &breakMap = rw ? writeBreakMap : readBreakMap;
	MemoryHashInfo **hashMap = rw ? writeHashMap : readHashMap;
	const char * const &reg = rw ? REG_WRITEBREAK : REG_READBREAK;
	if(!lua_toboolean(L, 3)) {
		lua_pushvalue(L, 2);
		{
			AddrBreakFunc s{};
			AddrBreakMap::iterator it = breakMap.find(addr);
			if(it == breakMap.end()) {
				AddrBreak b;
				it = breakMap.insert(std::pair<ULONG,AddrBreak>(addr, b)).first;
			}
			s.lua = L;
			s.idx = RegisterFunction(L, reg);
			it->second.func.push_back(s);
		}
		{
			void(***funcHashMap)() = rw ? writeFuncHashMap : readFuncHashMap;
			USHORT hash = addr >> 16;
			MemoryHashInfo *p = hashMap[hash];
			void(**atMemoryBreak)() = rw ? AtMemoryWrite : AtMemoryRead;
			if(p == NULL) {
				MemoryHashInfo *info = new MemoryHashInfo;
				info->count = 0;
				for(int i = 0; i < 4; i ++) {
					info->func[i] = funcHashMap[i][hash];
					funcHashMap[i][hash] = atMemoryBreak[i];
				}
				p = hashMap[hash] = info;
			}
			p->count ++;
		}
		if(dynacore) {
			if(fast_memory) {
				fast_memory = 0;	//�ǂ����ŕ����������ق����������B�ǂ��ŁH
				RecompileNextAll();
			}
		}
	}else {
		lua_pushvalue(L, 2);
		AddrBreakMap::iterator it = breakMap.find(addr);
		if(it != breakMap.end()) {
			AddrBreakFuncVec &f = it->second.func;
			AddrBreakFuncVec::iterator itt = f.begin();
			for(; itt != f.end(); itt ++) {
				if(itt->lua == L) {
					lua_getglobal(L, "table");
					lua_getfield(L, -1, "remove");
					lua_getfield(L, LUA_REGISTRYINDEX, reg);
					lua_pushinteger(L, itt->idx);
					lua_call(L, 2, 0);
					lua_pop(L, 1);
					f.erase(itt);
					if(f.size() == 0) {
						RemoveMemoryBreak<rw>(it);
					}
					return;
				}
			}
		}
		luaL_error(L, "SetMemoryBreak: not found registry function");
	}
}
//dynacore�̏ꍇ��recompile����܂Ō��ʂ����f����Ȃ�
int SetReadBreak(lua_State *L){
	SetMemoryBreak<false>(L);
	return 0;
}
int SetWriteBreak(lua_State *L) {
	SetMemoryBreak<true>(L);
	return 0;
}
AddrBreakFuncVec::iterator RemovePCBreak(AddrBreakFuncVec &f, AddrBreakFuncVec::iterator it){
	it = f.erase(it);
	pcBreakCount --;
	if(pcBreakCount == 0) {
		enablePCBreak = false;
	}
	return it;
}
int SetPCBreak(lua_State *L) {
	InterpreterCoreCheck(L);
	ULONG addr = CheckIntegerU(L, 1) | 0x80000000;
	luaL_checktype(L, 2, LUA_TFUNCTION);
	if(!(0x80000000 <= addr && addr < 0x80800000 && addr%4 == 0)) {
		luaL_error(L, "SetPCBreak: 0x80000000 <= addr && addr < 0x80800000 && addr%4 == 0");
	}
	ULONG a = (addr - 0x80000000) >> 2;
	if(!lua_toboolean(L, 3)) {
		if(!pcBreakMap[a]) {
			pcBreakMap[a] = new AddrBreakFuncVec();
		}
		AddrBreakFunc s{};
		s.lua = L;
		s.idx = RegisterFunction(L, REG_PCBREAK);
		pcBreakMap[a]->push_back(s);
		enablePCBreak = true;
		pcBreakCount ++;
	}else {
		lua_pushvalue(L, 2);
		if(pcBreakMap[a]) {
			AddrBreakFuncVec &f = *pcBreakMap[a];
			AddrBreakFuncVec::iterator itt = f.begin();
			for(; itt != f.end(); itt ++) {
				if(itt->lua == L) {
					lua_getglobal(L, "table");
					lua_getfield(L, -1, "remove");
					lua_getfield(L, LUA_REGISTRYINDEX, REG_PCBREAK);
					lua_pushinteger(L, itt->idx);
					lua_call(L, 2, 0);
					lua_pop(L, 1);
					RemovePCBreak(f, itt);
					if(f.size() == 0) {
						delete pcBreakMap[a];
						pcBreakMap[a] = NULL;
					}
					return 0;
				}
			}
		}
		luaL_error(L, "SetPCBreak: not found registry function");
	}
	return 0;
}
const char * const RegName[] = {
	//CPU
	"r0","at","v0","v1","a0","a1","a2","a3",
	"t0","t1","t2","t3","t4","t5","t6","t7",
	"s0","s1","s2","s3","s4","s5","s6","s7",
	"t8","t9","k0","k1","gp","sp","s8","ra",
	//COP0
	"index","random","entrylo0","entrylo1",
	"context","pagemask","wired","reserved7",
	"badvaddr","count","entryhi","compare",
	"status","cause","epc","previd",
	"config","lladdr","watchlo","watchhi",
	"xcontext","reserved21","reserved22","reserved23",
	"reserved24","reserved25","perr","cacheerr",
	"taglo","taghi","errorepc","reserved31",
	"hi", "lo",	//division and multiple
	"fcr0", "fcr31",	//cop1 control register
	"pc",
	//not register
	"break_value",	//������break�̎��A
	//����̒l��ύX�ł���(readbreak/writebreak)�A�������ݐ�p�A1��̂�
	"writebreak_value",	//writebreak�̂݁A����������Ƃ��Ă���l�𓾂鎞�͂�����
	//readbreak�œǂ݂�����Ƃ��Ă���l�𓾂�ɂ�readsize(addr, size)��(addr,size�͊֐��̈���)

	//COP1: "f"+N
};
int SelectRegister(lua_State *L, void **r, int *arg) {
	//InterpreterCoreCheck(L);
	/*
		dynacore���ƃ��W�X�^���蓖�Ăɂ���ĈႤ���ʂ�Ԃ����Ƃ͂��邪�A
		��ɃC���^�v���^�Ɠ������ʂ�Ԃ����W�X�^����邵
		�܂��A�ł��Ȃ����o�����ق��������̂ŁB
		dynacore�ł͒��ӂƂ������Ƃ�
	*/
	int t = lua_type(L, 1);
	int size = 32;
	int n = -1;
	const int cop1index = sizeof(RegName)/sizeof(*RegName);
	if(lua_gettop(L) == *arg+2) {
		size = lua_tointeger(L, 2);
		*arg += 2;
	}else {
		*arg += 1;
	}
	if(size < 0 || size > 64) {
		luaL_error(L, "size 0 - 64");
	}
	if(t == LUA_TSTRING) {
		const char *s = lua_tostring(L, 1);
		if(s[0] == 'f'){
			sscanf(s+1, "%u", &n);
			if(n >= 32) n = -1;
			else n += 66;
		}else {
			for(int i = 0; i < sizeof(RegName)/sizeof(RegName[0]); i ++) {
				if(lstrcmpi(s, RegName[i]) == 0) {
					n = i;
					break;
				}
			}
		}
	}else if(t == LUA_TNUMBER) {
		n = lua_tointeger(L, 1);
	}
	if(n < 0 || n > cop1index+32) {
		luaL_error(L, "unknown register");
	}else if(n < 32) {
		*r = &reg[n];
	}else if(n < 64) {
		*r = &reg_cop0[n-32];
	}else if(n < cop1index){
		switch(n){
		case 64:
			*r = &hi; break;
		case 65:
			*r = &lo; break;
		case 66:
			*r = &FCR0;
			if(size > 32) size = 32;
			break;
		case 67:
			*r = &FCR31;
			if(size > 32) size = 32;
			break;
		case 68:
			InterpreterCoreCheck(L, "(get PC)");
			//MemoryBreak�ł�PC++������ɏ�������݂���������A1���[�h�����
			if(interpcore == 0) {
				*r = &PC->addr;
			}else {
				*r = &interp_addr;
			}
			break;
		case 69:
			if(!break_value_flag) {
				//2��ڂƂ�break�̊O�Ƃ�
				luaL_error(L, "break_value");
			}
			break_value_flag = false;
			*r = &break_value;
			break;
		case 70:
			//�ǂݍ��݂̂�
			switch(current_break_value_size) {
			case 1: *r = &g_byte; break;
			case 2: *r = &hword; break;
			case 4: *r = &word; break;
			case 8: *r = &dword; break;
			default: ASSERT(0);
			}
			if(size > current_break_value_size * 8) {
				size = current_break_value_size * 8;
			}
			break;
		}
	}else {
		//Status�ɂ�����炸�������ʂɂȂ�����������H(cop0�悭�킩���ĂȂ��̂�����ǂ�)
		if(size == 32) {
			*r = reg_cop1_simple[n-cop1index];
			size = -32;
		}else if(size == 64) {
			*r = reg_cop1_double[n-cop1index];
			size = -64;
		}else {
			luaL_error(L, "get cop1 register size must be 32 or 64");
		}
	}
	return size;
}

int GetRegister(lua_State *L) {
	void *r;
	int size, arg = 0;
	size = SelectRegister(L, &r, &arg);
	if(size == -32) {
		lua_pushnumber(L, *(float*)r);
	}else if(size == -64) {
		lua_pushnumber(L, *(double*)r);
	}else if(size > 32){
		ULONGLONG n = *(ULONGLONG*)r;
		if(size != 64)
			n &= (1ULL<<size)-1;
		PushDword(L, n);
	}else {
		PushIntU(L, *(ULONGLONG*)r & ((1ULL<<size)-1));
	}
	return 1;
}
int SetRegister(lua_State *L) {
	void *r;
	int size, arg = 1;
	size = SelectRegister(L, &r, &arg);
	if(size == -32) {
		*(float*)r = lua_tonumber(L, arg);
	}else if(size == -64) {
		*(double*)r = lua_tonumber(L, arg);
	}else if(size > 32){
		ULONGLONG n = CheckDword(L, arg);
		ULONGLONG mask;
		if(size == 64)
			mask = ~0;
		else
			mask = (1ULL<<size)-1;
		*(ULONGLONG*)r &=~mask;
		*(ULONGLONG*)r |= n&mask;
		
	}else {
		ULONG mask = (1ULL<<size)-1;
		*(ULONG*)r &=~mask;
		*(ULONG*)r |= CheckIntegerU(L, arg)&mask;
	}
	return 0;
}
int TraceLog(lua_State *L) {
	if(lua_toboolean(L, 1)) {
		const char *path = lua_tostring(L, 1);
		if(!path)path = "tracelog.txt";
		TraceLogStart(path, lua_toboolean(L, 2));
	}else {
		TraceLogStop();
	}
	return 0;
}
int TraceLogMode(lua_State *L) {
	traceLogMode = !!lua_toboolean(L, 1);
	return 0;
}
int GetCore(lua_State *L) {
	lua_pushinteger(L, dynacore ? 0 : 1 + interpcore);
	return 1;
}

unsigned long PAddr(unsigned long addr){
	if (addr >= 0x80000000 && addr < 0xC0000000) {
		return addr;
	}else {
		return virtual_to_physical_address(addr, 2);
	}
}
void RecompileNow(ULONG addr) {
	//NOTCOMPILED���B�����ɃR���p�C�����ʂ�ops�Ȃǂ��~�������Ɏg��
	if ((addr>>16) == 0xa400)
		recompile_block((long*)SP_DMEM, blocks[0xa4000000>>12], addr);
	else {
		unsigned long paddr = PAddr(addr);
		if(paddr) {
			if ((paddr & 0x1FFFFFFF) >= 0x10000000) {
				recompile_block((long*)rom+((((paddr-(addr-blocks[addr>>12]->start)) & 0x1FFFFFFF) - 0x10000000)>>2),
					blocks[addr>>12], addr);
			} else {
				recompile_block((long*)(rdram+(((paddr-(addr-blocks[addr>>12]->start)) & 0x1FFFFFFF)>>2)),
					blocks[addr>>12], addr);
			}
	  }
	}
}
void Recompile(ULONG addr) {
	//jump_to���
	//���ʂɃ��R���p�C���������Ƃ��͂���ł���
	ULONG block = addr >> 12;
	ULONG paddr = PAddr(addr);
	if(!blocks[block]) {
		blocks[block] = (precomp_block*) malloc(sizeof(precomp_block));
		actual = blocks[block];
		blocks[block]->code = NULL;
		blocks[block]->block = NULL;
		blocks[block]->jumps_table = NULL;
	}
	blocks[block]->start = addr & ~0xFFF;
	blocks[block]->end = (addr & ~0xFFF) + 0x1000;
	init_block((long*)(rdram+(((paddr-(addr-blocks[block]->start)) & 0x1FFFFFFF)>>2)),
		   blocks[block]);
}
void RecompileNext(ULONG addr) {
	//jump_to�̎�(�u���b�N��܂������W�����v)�Ƀ`�F�b�N�����H
	//������u���b�N�𒼂��ɏC�����������ȊO�͂���ł���
	invalid_code[addr>>12]=1;
}
void RecompileNextAll() {
	memset(invalid_code, 1, 0x100000);
}
int RecompileNowLua(lua_State *L) {
	Recompile(CheckIntegerU(L, 1));
	return 0;
}
int RecompileLua(lua_State *L) {
	Recompile(CheckIntegerU(L, 1));
	return 0;
}
int RecompileNextLua(lua_State *L) {
	Recompile(CheckIntegerU(L, 1));
	return 0;
}
int RecompileNextAllLua(lua_State *L) {
	RecompileNextAll();
	return 0;
}
template<typename T>void PushT(lua_State *L, T value) {
	PushIntU(L, value);
}
template<>void PushT<ULONGLONG>(lua_State *L, ULONGLONG value) {
	PushDword(L, value);
}
template<typename T, void(**readmem_func)()>
int ReadMemT(lua_State *L) {
	ULONGLONG *rdword_s = rdword, tmp, address_s = address;
	address = CheckIntegerU(L, 1);
	rdword = &tmp;
	readmem_func[address>>16]();
	PushT<T>(L, tmp);
	rdword = rdword_s;
	address = address_s;
	return 1;
}
template<typename T>T CheckT(lua_State *L, int i) {
	return CheckIntegerU(L, i);
}
template<>ULONGLONG CheckT<ULONGLONG>(lua_State *L, int i) {
	return CheckDword(L, i);
}
template<typename T, void(**writemem_func)(), T &g_T>
int WriteMemT(lua_State *L) {
	ULONGLONG *rdword_s = rdword, address_s = address;
	T g_T_s = g_T;
	address = CheckIntegerU(L, 1);
	g_T = CheckT<T>(L, 2);
	writemem_func[address>>16]();
	address = address_s;
	g_T = g_T_s;
	return 0;
}
int GetSystemMetricsLua(lua_State* L)
{
	Lua* lua = GetLuaClass(L);

	long param = luaL_checknumber(L, 1);
	int ret = GetSystemMetrics(param); // we have to store intermediate value because of calling convention? i dont know
	lua_pushinteger(L, ret);
	
	return 1;
}
int IsMainWindowInForeground(lua_State* L) 
{
	Lua* lua = GetLuaClass(L);
	lua_pushboolean(L, GetForegroundWindow() == mainHWND || GetActiveWindow() == mainHWND);
	return 1;
}
//Gui
//�v���O�C���ɕ�����Ă邩�玩�R�ɏo���Ȃ��H
//�Ƃ������E�B���h�E�ɒ��ڏ����Ă���
//�Ƃ肠������������E�B���h�E�ɒ�������
typedef struct COLORNAME {
	const char *name;
	COLORREF value;
} COLORNAME;

const int hexTable[256] = {
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,1,2,3,4,5,6,7,8,9,0,0,0,0,0,0,
		0,10,11,12,13,14,15,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,10,11,12,13,14,15,0,0,0,0,0,0,0,0,
};
const COLORNAME colors[] = {
	{"white", 0xFFFFFFFF},
	{"black", 0xFF000000},
	{"clear", 0x00000000},
	{"gray", 0xFF808080},
	{"red", 0xFF0000FF},
	{"orange", 0xFF0080FF},
	{"yellow", 0xFF00FFFF},
	{"chartreuse", 0xFF00FF80},
	{"green", 0xFF00FF00},
	{"teal", 0xFF80FF00},
	{"cyan", 0xFFFFFF00},
	{"blue", 0xFFFF0000},
	{"purple", 0xFFFF0080},
	{NULL}
};

COLORREF StrToColorA(const char *s, bool alpha = false, COLORREF def = 0) {
	if(s[0] == '#') {
		int l = lstrlen(s);
		if(l == 4) {
			return (hexTable[s[1]]*0x10+hexTable[s[1]])|
						 ((hexTable[s[2]]*0x10+hexTable[s[2]])<<8)|
 						 ((hexTable[s[3]]*0x10+hexTable[s[3]])<<16)|
						 (alpha ? 0xFF000000 : 0);
		}else if(alpha && l == 5) {
			return (hexTable[s[1]]*0x10+hexTable[s[1]])|
						 ((hexTable[s[2]]*0x10+hexTable[s[2]])<<8)|
 						 ((hexTable[s[3]]*0x10+hexTable[s[3]])<<16)|
						 ((hexTable[s[4]]*0x10+hexTable[s[4]])<<24);
		}else if(l == 7){
			return (hexTable[s[1]]*0x10+hexTable[s[2]])|
						 ((hexTable[s[3]]*0x10+hexTable[s[4]])<<8)|
 						 ((hexTable[s[5]]*0x10+hexTable[s[6]])<<16)|
						 (alpha ? 0xFF000000 : 0);
		}else if(alpha && l == 9){
			return (hexTable[s[1]]*0x10+hexTable[s[2]])|
						 ((hexTable[s[3]]*0x10+hexTable[s[4]])<<8)|
 						 ((hexTable[s[5]]*0x10+hexTable[s[6]])<<16)|
						 ((hexTable[s[7]]*0x10+hexTable[s[8]])<<24);
		}
	}else {
		const COLORNAME *p = colors;
		do{
			if(lstrcmpi(s, p->name) == 0) {
				return (alpha ? 0xFFFFFFFF : 0xFFFFFF) & p->value;
			}
		}while((++p)->name);
	}
	return (alpha ? 0xFF000000 : 0x00000000) | def;
}
COLORREF StrToColor(const char *s, bool alpha = false, COLORREF def = 0) {
	COLORREF c = StrToColorA(s, alpha, def);
/*
	if((c&0xFFFFFF) == LUADC_BG_COLOR) {
		return LUADC_BG_COLOR_A|(alpha?0xFF000000:0);
	}else {
		return c;
	}
*/
	return c;
}

//wgui
int SetBrush(lua_State* L) {
	Lua* lua = GetLuaClass(L);
	const char* s = lua_tostring(L, 1);
	if (lstrcmpi(s, "null") == 0)
		lua->setBrush((HBRUSH)GetStockObject(NULL_BRUSH));
	else
		lua->setBrush(::CreateSolidBrush(StrToColor(s)));
	return 0;
}
int SetPen(lua_State* L) {
	Lua* lua = GetLuaClass(L);
	const char* s = lua_tostring(L, 1);
	if (lstrcmpi(s, "null") == 0)
		lua->setPen((HPEN)GetStockObject(NULL_PEN));
	else
		// optional pen width defaults to 1
		lua->setPen(::CreatePen(PS_SOLID, luaL_optnumber(L, 2, 1), StrToColor(s)));
	return 0;
}
int SetTextColor(lua_State *L) {
	GetLuaClass(L)->setTextColor(StrToColor(lua_tostring(L, 1)));
	return 0;
}
int SetBackgroundColor(lua_State* L) {
	Lua* lua = GetLuaClass(L);
	const char* s = lua_tostring(L, 1);
	if (lstrcmpi(s, "null") == 0)
		lua->setBackgroundColor(0, TRANSPARENT);
	else
		lua->setBackgroundColor(StrToColor(s));
	return 0;
}
int SetFont(lua_State* L) {
	LOGFONT font = {0};
	
	// set the size of the font
	font.lfHeight = -MulDiv(luaL_checknumber(L, 1), GetDeviceCaps(lua_dc, LOGPIXELSY), 72);
	lstrcpyn(font.lfFaceName, luaL_optstring(L, 2, "MS Gothic"), LF_FACESIZE);
	font.lfCharSet = DEFAULT_CHARSET;
	const char* style = luaL_optstring(L, 3, "");
	for (const char* p = style; *p; p++) {
		switch (*p) {
			case 'b': font.lfWeight = FW_BOLD; break;
			case 'i': font.lfItalic = TRUE; break;
			case 'u': font.lfUnderline = TRUE; break;
			case 's': font.lfStrikeOut = TRUE; break;
			case 'a': font.lfQuality = ANTIALIASED_QUALITY; break;
		}
	}
	GetLuaClass(L)->setFont(CreateFontIndirect(&font));
	return 0;
}
int LuaTextOut(lua_State *L) {
	Lua* lua = GetLuaClass(L);
	lua->selectTextColor();
	lua->selectBackgroundColor();
	lua->selectFont();

	int x = luaL_checknumber(L, 1);
	int y = luaL_checknumber(L, 2);
	const char* text = lua_tostring(L, 3);

	::TextOut(lua_dc, x, y, text, lstrlen(text));
	return 0;
}
bool GetRectLua(lua_State *L, int idx, RECT *rect) {
	switch(lua_type(L, idx)) {
	case LUA_TTABLE:
		lua_getfield(L, idx, "l");
		rect->left = lua_tointeger(L, -1);
		lua_pop(L, 1);
		lua_getfield(L, idx, "t");
		rect->top = lua_tointeger(L, -1);
		lua_pop(L, 1);
		lua_getfield(L, idx, "r");
		if(lua_isnil(L, -1)) {
			lua_pop(L, 1);
			lua_getfield(L, idx, "w");
			if(lua_isnil(L, -1)) {
				return false;
			}
			rect->right = rect->left + lua_tointeger(L, -1);
			lua_pop(L, 1);
			lua_getfield(L, idx, "h");
			rect->bottom = rect->top + lua_tointeger(L, -1);
			lua_pop(L, 1);
		}else {
			rect->right = lua_tointeger(L, -1);
			lua_pop(L, 1);
			lua_getfield(L, idx, "b");
			rect->bottom = lua_tointeger(L, -1);
			lua_pop(L, 1);
		}
		return true;
	default:
		return false;
	}
}

int GetTextExtent(lua_State* L) {
	const char* string = luaL_checkstring(L, 1);
	SIZE size = { 0 };
	GetTextExtentPoint32(lua_dc, string, strlen(string), &size);
	lua_newtable(L);
	lua_pushinteger(L, size.cx);
	lua_setfield(L, -2, "width");
	lua_pushinteger(L, size.cy);
	lua_setfield(L, -2, "height");
	return 1;
}

int LuaDrawText(lua_State *L) {
	Lua *lua = GetLuaClass(L);
	lua->selectTextColor();
	lua->selectBackgroundColor();
	lua->selectFont();
	RECT rect = {0};
	UINT format = DT_NOPREFIX | DT_WORDBREAK;
	if(!GetRectLua(L, 2, &rect)) {
		format |= DT_NOCLIP;
	}
	if(!lua_isnil(L, 3)) {
		const char *p = lua_tostring(L, 3);
		for(; p && *p; p ++) {
			switch(*p) {
			case 'l':format |= DT_LEFT; break;
			case 'r':format |= DT_RIGHT; break;
			case 't':format |= DT_TOP; break;
			case 'b':format |= DT_BOTTOM; break;
			case 'c':format |= DT_CENTER; break;
			case 'v':format |= DT_VCENTER | DT_SINGLELINE; break;
			case 'e':format |= DT_WORD_ELLIPSIS; break;
			case 's':format &=~DT_WORDBREAK; break;
			}
		}
	}
	::DrawText(lua_dc, lua_tostring(L, 1), -1, &rect, format);
	return 0;
}
int DrawRect(lua_State* L) {
	Lua* lua = GetLuaClass(L);

	int left = luaL_checknumber(L, 1);
	int top = luaL_checknumber(L, 2);
	int right = luaL_checknumber(L, 3);
	int bottom = luaL_checknumber(L, 4);
	int cornerW = luaL_optnumber(L, 5, 0);
	int cornerH = luaL_optnumber(L, 6, 0);

	lua->selectBrush();
	lua->selectPen();
	::RoundRect(lua_dc, left, top, right, bottom, cornerW, cornerH);
	return 0;
}

int LuaLoadImage(lua_State* L) {
	const char* path = luaL_checkstring(L,1);
	int output_size = MultiByteToWideChar(CP_ACP, 0, path, -1, NULL, 0);
	wchar_t* pathw = (wchar_t*)malloc(output_size * sizeof(wchar_t));
	int size = MultiByteToWideChar(CP_ACP, 0, path, -1, pathw, output_size);

	printf("LoadImage: %ws\n", pathw);
	Gdiplus::Bitmap *img = new Gdiplus::Bitmap(pathw);
	free(pathw);

	if (img->GetLastStatus()) {
		luaL_error(L, "Couldn't find image '%s'", path);
		return 0;
	}
	imagePool.push_back(img);
	lua_pushinteger(L, imagePool.size()); //return the identifier (index+1)
	return 1;
}

int DeleteImage(lua_State* L) { // Clears one or all images from imagePool
	unsigned int clearIndex = luaL_checkinteger(L, 1);
	if (clearIndex == 0) { // If clearIndex is 0, clear all images
		printf("Deleting all images\n");
		for (auto x : imagePool) {
			delete x;
		}
		imagePool.clear();
	}
	else { // If clear index is not 0, clear 1 image
		if (clearIndex <= imagePool.size()) {
			printf("Deleting image index %d (%d in lua)\n", clearIndex - 1, clearIndex);
			delete imagePool[clearIndex - 1];
			imagePool.erase(imagePool.begin() + clearIndex - 1);
		}
		else { // Error if the image doesn't exist
			luaL_error(L, "Argument #1: Invalid image index");
			return 0;
		}
	}
	return 0;
}

int DrawImage(lua_State* L) {
	unsigned int imgIndex = luaL_checkinteger(L, 1) - 1; // because lua

	// Error if the image doesn't exist
	if (imgIndex > imagePool.size() - 1) {
		luaL_error(L, "Argument #1: Invalid image index");
		return 0;
	}
	
	// Gets the number of arguments
	unsigned int args = lua_gettop(L);

	Gdiplus::Graphics gfx(lua_dc);
	Gdiplus::Bitmap* img = imagePool[imgIndex];

	// Original DrawImage
	if (args == 3) {
		int x = luaL_checknumber(L, 2);
		int y = luaL_checknumber(L, 3);

		gfx.DrawImage(img, x, y);// Gdiplus::Image *image, int x, int y
		return 0;
	}
	else if (args == 4) {
		int x = luaL_checknumber(L, 2);
		int y = luaL_checknumber(L, 3);
		float scale = luaL_checknumber(L, 4);
		if (scale == 0) return 0;

		// Create a Rect at x and y and scale the image's width and height
		Gdiplus::Rect dest(x, y, img->GetWidth() * scale, img->GetHeight() * scale);

		gfx.DrawImage(img, dest);// Gdiplus::Image *image, const Gdiplus::Rect &rect
		return 0;
	}
	// In original DrawImage
	else if (args == 5) {
		int x = luaL_checknumber(L, 2);
		int y = luaL_checknumber(L, 3);
		int w = luaL_checknumber(L, 4);
		int h = luaL_checknumber(L, 5);
		if (w == 0 or h == 0) return 0;

		Gdiplus::Rect dest(x, y, w, h);

		gfx.DrawImage(img, dest);// Gdiplus::Image *image, const Gdiplus::Rect &rect
		return 0;
	}
	else if (args == 10) {
		int x = luaL_checknumber(L, 2);
		int y = luaL_checknumber(L, 3);
		int w = luaL_checknumber(L, 4);
		int h = luaL_checknumber(L, 5);
		int srcx = luaL_checknumber(L, 6);
		int srcy = luaL_checknumber(L, 7);
		int srcw = luaL_checknumber(L, 8);
		int srch = luaL_checknumber(L, 9);
		float rotate = luaL_checknumber(L, 10);
		if (w == 0 or h == 0 or srcw == 0 or srch == 0) return 0;
		bool shouldrotate = ((int)rotate % 360) != 0; // Only rotate if the angle isn't a multiple of 360 Modulo only works with int

		Gdiplus::Rect dest(x, y, w, h);

		// Rotate
		if (shouldrotate) {
			Gdiplus::PointF center(x + (w / 2), y + (h / 2));// The center of dest
			Gdiplus::Matrix matrix;
			matrix.RotateAt(rotate, center);// rotate "rotate" number of degrees around "center"
			gfx.SetTransform(&matrix);
		}

		gfx.DrawImage(img, dest, srcx, srcy, srcw, srch, Gdiplus::UnitPixel);// Gdiplus::Image *image, const Gdiplus::Rect &destRect, int srcx, int srcy, int srcheight, Gdiplus::srcUnit

		if (shouldrotate) gfx.ResetTransform();
		return 0;
	}
	luaL_error(L, "Incorrect number of arguments");
	return 0;
}

int Screenshot(lua_State* L) {
	CaptureScreen((char*)luaL_checkstring(L, 1));
	return 0;
}

int LoadScreen(lua_State* L) {
	if (!LoadScreenInitialized) {
		luaL_error(L, "LoadScreen not initialized! Something has gone wrong.");
		return 0;
	}
	// set the selected object of hsrcDC to hbwindow
	SelectObject(hsrcDC, hbitmap);
	// copy from the window device context to the bitmap device context
	BitBlt(hsrcDC, 0, 0, windowSize.width, windowSize.height, hwindowDC, 0, 0, SRCCOPY);

	Gdiplus::Bitmap* out = new Gdiplus::Bitmap(hbitmap, nullptr);

	imagePool.push_back(out);

	lua_pushinteger(L, imagePool.size());

	return 1;
}

int LoadScreenReset(lua_State* L) {
	LoadScreenInit();
	return 0;
}

int GetImageInfo(lua_State* L) {
	unsigned int imgIndex = luaL_checkinteger(L, 1) - 1;

	if (imgIndex > imagePool.size() - 1) {
		luaL_error(L, "Argument #1: Invalid image index");
		return 0;
	}

	Gdiplus::Bitmap* img = imagePool[imgIndex];

	lua_newtable(L);
	lua_pushinteger(L, img->GetWidth());
	lua_setfield(L, -2, "width");
	lua_pushinteger(L, img->GetHeight());
	lua_setfield(L, -2, "height");

	return 1;
}

//1st arg is table of points
//2nd arg is color #xxxxxxxx
int FillPolygonAlpha(lua_State* L)
{
	//Get lua instance stored in script class
	Lua* lua = GetLuaClass(L);
	
	//stack should look like
	//--------
	//2: color string
	//--------
	//1: table of points
	//--------
	//assert that first argument is table
	luaL_checktype(L, 1, LUA_TTABLE);

	int n = luaL_len(L, 1); //length of the table, doesnt modify stack
	if (n > 255) { //hard cap, the vector can handle more but dont try
		lua_pushfstring(L, "wgui.polygon: too many points (%d > %d)",
			n, 255);
		return lua_error(L);
	}

	std::vector<Gdiplus::PointF> pts(n); //list of points that make the poly

	//do n times
	for (int i = 0; i < n; i++) {
		//push current index +1 because lua
		lua_pushinteger(L, i + 1);
		//get index i+1 from table at the bottom of the stack (index 1 is bottom, 2 is next etc, -1 is top)
		//pops the index and places the element inside table on top, which again is a table [x,y]
		lua_gettable(L, 1);
		//make sure its a table
		luaL_checktype(L, -1, LUA_TTABLE);
		//push '1'
		lua_pushinteger(L, 1);
		//get index 1 from table that is second from top, because '1' is on top right now
		//then remove '1' and put table contents, its the X coord
		lua_gettable(L, -2);
		//read it
		pts[i].X = lua_tointeger(L, -1);
		//remove X coord
		lua_pop(L, 1);
		//push '2'
		lua_pushinteger(L, 2);
		//same thing
		lua_gettable(L, -2);
		pts[i].Y = lua_tointeger(L, -1);
		lua_pop(L, 2);
		//now stack again has only table at the bottom and color string on top, repeat
	}

	const char* col = luaL_checkstring(L, 2); //get string at index 2

	Gdiplus::Graphics gfx(lua_dc);
	Gdiplus::SolidBrush brush(Gdiplus::Color(StrToColorA(col, true)));
	gfx.FillPolygon(&brush, pts.data(), n);
	
	return 0;
}


int FillEllipseAlpha(lua_State* L) {
	Lua* lua = GetLuaClass(L);

	int x = luaL_checknumber(L, 1);
	int y = luaL_checknumber(L, 2);
	int w = luaL_checknumber(L, 3);
	int h = luaL_checknumber(L, 4);
	const char* col = luaL_checkstring(L, 5); //color string

	Gdiplus::Graphics gfx(lua_dc);
	Gdiplus::SolidBrush brush(Gdiplus::Color(StrToColorA(col, true)));

	gfx.FillEllipse(&brush, x, y, w, h);

	return 0;
}
int FillRectAlpha(lua_State* L) 
{
	Lua* lua = GetLuaClass(L);

	int x = luaL_checknumber(L, 1);
	int y = luaL_checknumber(L, 2);
	int w = luaL_checknumber(L, 3);
	int h = luaL_checknumber(L, 4);
	const char* col = luaL_checkstring(L, 5); //color string

	Gdiplus::Graphics gfx(lua_dc);
	Gdiplus::SolidBrush brush(Gdiplus::Color(StrToColorA(col, true)));

	gfx.FillRectangle(&brush, x, y, w, h);

	return 0;
}
int FillRect(lua_State* L) {
	/*
	(Info)
	Drawing for 1 frame only with double buffering enabled is impossible due to the way double buffering works.
	This applies to all wgui functions that only run once, but this shouldn't be a problem for scripts like Input Direction which
	repaint at every input.

	*/
	Lua* lua = GetLuaClass(L);
	RECT rect{};
	COLORREF color = RGB(
		luaL_checknumber(L, 5),
		luaL_checknumber(L, 6),
		luaL_checknumber(L, 7)
	);
	COLORREF colorold = SetBkColor(lua_dc, color);
	rect.left = luaL_checknumber(L, 1);
	rect.top = luaL_checknumber(L, 2);
	rect.right = luaL_checknumber(L, 3);
	rect.bottom = luaL_checknumber(L, 4);
	ExtTextOut(lua_dc, 0, 0, ETO_OPAQUE, &rect, "", 0, 0);
	SetBkColor(lua_dc, colorold);
	return 0;
}
int DrawEllipse(lua_State *L) {
	Lua *lua = GetLuaClass(L);
	lua->selectBrush();
	lua->selectPen();

	int left = luaL_checknumber(L, 1);
	int top = luaL_checknumber(L, 2);
	int right = luaL_checknumber(L, 3);
	int bottom = luaL_checknumber(L, 4);

	::Ellipse(lua_dc, left, top, right, bottom);
	return 0;
}
int DrawPolygon(lua_State *L) {
	POINT p[0x100];
	luaL_checktype(L, 1, LUA_TTABLE);
	int n = luaL_len(L, 1);
	if(n >= sizeof(p)/sizeof(p[0])){
		lua_pushfstring(L, "wgui.polygon: too many points (%d < %d)",
			sizeof(p)/sizeof(p[0]), n);
		return lua_error(L);
	}
	for(int i = 0; i < n; i ++) {
		lua_pushinteger(L, i+1);
		lua_gettable(L, 1);
		luaL_checktype(L, -1, LUA_TTABLE);
		lua_pushinteger(L, 1);
		lua_gettable(L, -2);
		p[i].x = lua_tointeger(L, -1);
		lua_pop(L, 1);
		lua_pushinteger(L, 2);
		lua_gettable(L, -2);
		p[i].y = lua_tointeger(L, -1);
		lua_pop(L, 2);
	}
	Lua *lua = GetLuaClass(L);
	lua->selectBrush();
	lua->selectPen();
	::Polygon(lua_dc, p, n);
	return 0;
}
int DrawLine(lua_State *L) {
	Lua *lua = GetLuaClass(L);
	lua->selectPen();
	::MoveToEx(lua_dc, luaL_checknumber(L, 1), luaL_checknumber(L, 2),
		NULL);
	::LineTo(lua_dc, luaL_checknumber(L, 3), luaL_checknumber(L, 4));
	return 0;
}
int GetGUIInfo(lua_State *L) {
	InitializeLuaDC(mainHWND);
	lua_newtable(L);
	lua_pushinteger(L, lua_dc_width);
	lua_setfield(L, -2, "width");
	lua_pushinteger(L, lua_dc_height);
	lua_setfield(L, -2, "height");
	return 1;
}
int ResizeWindow(lua_State *L) {
	RECT clientRect, wndRect;
	GetWindowRect(mainHWND, &wndRect);
	GetClientRect(mainHWND, &clientRect);
	wndRect.bottom -= wndRect.top;
	wndRect.right -= wndRect.left;
	int w = luaL_checkinteger(L, 1),
		h = luaL_checkinteger(L, 2);
	SetWindowPos(mainHWND, 0, 0, 0,
		w + (wndRect.right - clientRect.right),
		h + (wndRect.bottom - clientRect.bottom),
		SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOMOVE);
	InitializeLuaDC(mainHWND);
	return 0;
}
//emu
int ConsoleWriteLua(lua_State *L) {
	ConsoleWrite(GetLuaClass(L)->ownWnd, lua_tostring(L, 1));
	return 0;
}
int DebugviewWrite(lua_State *L) {
	ShowInfo((char*)lua_tostring(L, 1));
	return 0;
}
int StatusbarWrite(lua_State *L) {
	SendMessage(hStatus, (WM_USER+1), 0, (LPARAM)lua_tostring(L,1)); 
	return 0;
}
int RegisterUpdateScreen(lua_State *L) {
	if(lua_toboolean(L, 2)) {
		lua_pop(L, 1);
		UnregisterFunction(L, REG_ATUPDATESCREEN);
	}else {
		if(lua_gettop(L) == 2)
			lua_pop(L, 1);
		RegisterFunction(L, REG_ATUPDATESCREEN);
	}
	return 0;
}
int AtUpdateScreen(lua_State *L) {
	return lua_pcall(L, 0, 0, 0);
}
int RegisterVI(lua_State *L) {
	if(lua_toboolean(L, 2)) {
		lua_pop(L, 1);
		UnregisterFunction(L, REG_ATVI);
	}else {
		if(lua_gettop(L) == 2)
			lua_pop(L, 1);
		RegisterFunction(L, REG_ATVI);
	}
	return 0;
}
int AtVI(lua_State *L) {
	return lua_pcall(L, 0, 0, 0);
}
int RegisterInput(lua_State *L) {
	if(lua_toboolean(L, 2)) {
		lua_pop(L, 1);
		UnregisterFunction(L, REG_ATINPUT);
	}else {
		if(lua_gettop(L) == 2)
			lua_pop(L, 1);
		RegisterFunction(L, REG_ATINPUT);
	}
	return 0;
}
static int current_input_n;
int AtInput(lua_State *L) {
	lua_pushinteger(L, current_input_n);
	return lua_pcall(L, 1, 0, 0);
}
int RegisterStop(lua_State *L) {
	if(lua_toboolean(L, 2)) {
		lua_pop(L, 1);
		UnregisterFunction(L, REG_ATSTOP);
	}else {
		if(lua_gettop(L) == 2)
			lua_pop(L, 1);
		RegisterFunction(L, REG_ATSTOP);
	}
	return 0;
}
int AtStop(lua_State *L) {
	return lua_pcall(L, 0, 0, 0);
}
int RegisterWindowMessage(lua_State *L) {
	if(lua_toboolean(L, 2)) {
		lua_pop(L, 1);
		UnregisterFunction(L, REG_WINDOWMESSAGE);
	}else {
		if(lua_gettop(L) == 2)
			lua_pop(L, 1);
		RegisterFunction(L, REG_WINDOWMESSAGE);
	}
	return 0;
}
int AtWindowMessage(lua_State *L) {
	PushIntU(L, (unsigned)luaMessage.current_msg->windowMessage.wnd);
	PushIntU(L, luaMessage.current_msg->windowMessage.msg);
	PushIntU(L, luaMessage.current_msg->windowMessage.wParam);
	PushIntU(L, luaMessage.current_msg->windowMessage.lParam);
	return lua_pcall(L, 4, 0, 0);
}
int RegisterInterval(lua_State *L) {
	if(lua_toboolean(L, 2)) {
		lua_pop(L, 1);
		UnregisterFunction(L, REG_ATINTERVAL);
	}else {
		if(lua_gettop(L) == 2)
			lua_pop(L, 1);
		RegisterFunction(L, REG_ATINTERVAL);
	}
	return 0;
}

int RegisterPlayMovie(lua_State* L) {
	if (lua_toboolean(L, 2)) {
		lua_pop(L, 1);
		UnregisterFunction(L, REG_ATPLAYMOVIE);
	}
	else {
		if (lua_gettop(L) == 2)
			lua_pop(L, 1);
		RegisterFunction(L, REG_ATPLAYMOVIE);
	}
	return 0;
}

int RegisterStopMovie(lua_State* L) {
	if (lua_toboolean(L, 2)) {
		lua_pop(L, 1);
		UnregisterFunction(L, REG_ATSTOPMOVIE);
	}
	else {
		if (lua_gettop(L) == 2)
			lua_pop(L, 1);
		RegisterFunction(L, REG_ATSTOPMOVIE);
	}
	return 0;
}

int RegisterLoadState(lua_State* L) {
	if (lua_toboolean(L, 2)) {
		lua_pop(L, 1);
		UnregisterFunction(L, REG_ATLOADSTATE);
	}
	else {
		if (lua_gettop(L) == 2)
			lua_pop(L, 1);
		RegisterFunction(L, REG_ATLOADSTATE);
	}
	return 0;
}

int RegisterSaveState(lua_State* L) {
	if (lua_toboolean(L, 2)) {
		lua_pop(L, 1);
		UnregisterFunction(L, REG_ATSAVESTATE);
	}
	else {
		if (lua_gettop(L) == 2)
			lua_pop(L, 1);
		RegisterFunction(L, REG_ATSAVESTATE);
	}
	return 0;
}

int RegisterReset(lua_State* L) {
	if (lua_toboolean(L, 2)) {
		lua_pop(L, 1);
		UnregisterFunction(L, REG_ATRESET);
	}
	else {
		if (lua_gettop(L) == 2)
			lua_pop(L, 1);
		RegisterFunction(L, REG_ATRESET);
	}
	return 0;
}

//generic function used for all of the At... callbacks, calls function from stack top.
int CallTop(lua_State* L) {
	return lua_pcall(L, 0, 0, 0);
}

int GetVICount(lua_State *L) {
	lua_pushinteger(L, m_currentVI);
	return 1;
}
int GetSampleCount(lua_State *L) {
	lua_pushinteger(L, m_currentSample);
	return 1;
}
int GetInputCount(lua_State *L) {
	lua_pushinteger(L, inputCount);
	return 1;
}


int GetMupenVersion(lua_State* L) {
	int type = luaL_checknumber(L, 1);
	const char* version;
	// 0 = name + version number
	// 1 = version number
	version = MUPEN_VERSION;
	if (type > 0)
		version = { &MUPEN_VERSION[strlen("Mupen 64 ")] };

	
	lua_pushstring(L, version);
	return 1;
}
int GetVCRReadOnly(lua_State* L) {
	lua_pushboolean(L, VCR_getReadOnly());
	return 1;
}
int SetGFX(lua_State* L) {
	// Ignore or update gfx
	int state = luaL_checknumber(L, 1);
	//forceIgnoreRSP = !state; // ???? buggy

	// DO NOT CALL THIS LUA FUNCTION INSIDE A LOOP!!! 
	// (unpausing will not work and it gets stuck with paused gfx lol)

	forceIgnoreRSP = state == 0;
	/*if (state == 0) {
		forceIgnoreRSP = true;
	}
	else {
		forceIgnoreRSP = false;
		//UpdateWindow(mainHWND); // vcr updatescreen causes access violation when called in loop. 
								// this doesnt but still doesnt work consistently
	}*/
	return 0;
}
int GetAddress(lua_State *L) {
	struct NameAndVariable {
		const char *name;
		void *pointer;
	};
#define A(n) {#n, &n}
#define B(n) {#n, n}
	const NameAndVariable list[] = {
		A(rdram),
		A(rdram_register),
		A(MI_register),
		A(pi_register),
		A(sp_register),
		A(rsp_register),
		A(si_register),
		A(vi_register),
		A(ri_register),
		A(ai_register),
		A(dpc_register),
		A(dps_register),
		B(SP_DMEM),
		B(PIF_RAM),
		{NULL, NULL}
	};
#undef A
#undef B
	const char *s = lua_tostring(L, 1);
	for(const NameAndVariable *p = list; p->name; p ++) {
		if(lstrcmpi(p->name, s) == 0) {
			PushIntU(L, (unsigned)p->pointer);
			return 1;
		}
	}
	return 0;
}
int EmuPause(lua_State *L) {
	if(!lua_toboolean(L, 1)) {
		pauseEmu(FALSE);
	}else {
		resumeEmu(TRUE);
	}
	return 0;
}
int GetEmuPause(lua_State *L) {
	lua_pushboolean(L, emu_paused);
	return 1;
}
int GetSpeed(lua_State *L) {
	lua_pushinteger(L, Config.fps_modifier); 
	return 1;
}
int SetSpeed(lua_State *L) {
	Config.fps_modifier = luaL_checkinteger(L, 1);
	InitTimer();
	return 0;
}
int SetSpeedMode(lua_State *L) {
	const char *s = lua_tostring(L, 1);
	if(lstrcmpi(s, "normal")==0){
		maximumSpeedMode = false;
	}else if(lstrcmpi(s, "maximum")==0){
		maximumSpeedMode = true;
	}
	return 0;
}

// Movie
int PlayMovie(lua_State* L) {
	const char* fname = lua_tostring(L, 1);
	VCR_setReadOnly(true);
	VCR_startPlayback(fname, "", "");
	return 0;
}
int StopMovie(lua_State* L) {
	VCR_stopPlayback();
	return 0;
}
int GetMovieFilename(lua_State* L) {
	if (VCR_isStarting() || VCR_isPlaying()) {
		lua_pushstring(L, VCR_getMovieFilename());
	} else {
		luaL_error(L, "No movie is currently playing");
		lua_pushstring(L, "");
	}
	return 1;
}

//savestate
//�蔲��
int SaveFileSavestate(lua_State *L) {
  savestates_select_filename(lua_tostring(L, 1));
  savestates_job = SAVESTATE;
	return 0;
}
int LoadFileSavestate(lua_State *L) {
  savestates_select_filename(lua_tostring(L, 1));
  savestates_job = LOADSTATE;                
	return 0;
}

// IO
int LuaFileDialog(lua_State* L) {
	EmulationLock lock;
	char filename[MAX_PATH]; 
	const char* filter = luaL_checkstring(L, 1);
	wchar_t filterW[MAX_PATH];
	int type = luaL_checkinteger(L, 2);
	if (!filter[0] || strlen(filter)>MAX_PATH) filter = "*.*"; // fucking catastrophe
	mbstowcs(filterW, filter, MAX_PATH);

	fdOpenLua.ShowFileDialog(filename, filterW, type ? FALSE : TRUE, mainHWND);

	lua_pushstring(L, filename);
	return 1;
}



BOOL validType(const char* type) {
	printf("Type: %s\n", type);
	const char* validTypes[15] = { "r","rb","w","wb","a","ab","r+","rb+","r+b","w+","wb+","w+b","a+","ab+","a+b" };
	for (int i = 0; i <= 15; i++)
	{
		if (strcmp(validTypes[i], type))
			return TRUE;
	}
	/*return strcmp(type, "r") ||
		   strcmp(type,"rw") ||
		   strcmp(type,"rb");*/
	return FALSE;
}
BOOL fileexists(const char* path) {
	struct stat buffer;
	return (stat(path, &buffer) == 0);
}

// AVI
int StartCapture(lua_State* L) {
	const char* fname = lua_tostring(L, 1);
	if (!VCR_isCapturing())
		VCR_startCapture("", fname, false);
	else
		luaL_error(L, "Tried to start AVI capture when one was already in progress");
	return 0;
}

int StopCapture(lua_State* L) {
	if (VCR_isCapturing())
		VCR_stopCapture();
	else
		luaL_error(L, "Tried to end AVI capture when none was in progress");
	return 0;
}

//callback�Ȃ�
bool BreakpointSync(SyncBreakMap::iterator it, ULONG addr){
	AddrBreakFuncVec &f = it->second.func;
	for(AddrBreakFuncVec::iterator itt = f.begin();
		itt != f.end(); itt ++) {
		lua_State *L = itt->lua;
		lua_getfield(L, LUA_REGISTRYINDEX, REG_SYNCBREAK);
		lua_pushinteger(L, itt->idx);
		lua_gettable(L, -2);
		PushIntU(L, addr);
		if(GetLuaClass(L)->errorPCall(1, 0)) {
			return true;
		}
		lua_pop(L, 1);
	}
	return false;
}
void BreakpointSyncPure(){
	if(op != BREAKPOINTSYNC_MAGIC) {
		interp_addr ++;
		return;
	}
	SyncBreakMap::iterator it = syncBreakMap.find(interp_addr);
	if(it != syncBreakMap.end()) {
		if(BreakpointSync(it, interp_addr)) {
			//�C�e���[�^�������ɂȂ����肵�Ă�
			it = syncBreakMap.find(interp_addr);
			if(it==syncBreakMap.end()){
				BreakpointSyncPure();
				return;
			}
		}
		op = it->second.op;
		prefetch_opcode(op);
		if(op != BREAKPOINTSYNC_MAGIC)	//�����ċA�΍�
			interp_ops[((op >> 26) & 0x3F)]();
	}
}
void BreakpointSyncInterp(){
	if(PC->f.stype != BREAKPOINTSYNC_MAGIC_STYPE) {
		PC ++;
		return;
	}
	unsigned long addr = PC->addr;
	SyncBreakMap::iterator it = syncBreakMap.find(addr);
	if(it != syncBreakMap.end()) {
		if(BreakpointSync(it, addr)) {
			//�C�e���[�^�������ɂȂ����肵�Ă�
			it = syncBreakMap.find(addr);
			if(it==syncBreakMap.end()){
				BreakpointSyncInterp();
				return;
			}
		}
		precomp_instr *o_PC = PC;
		*PC = it->second.pc;
		PC->ops();
		StoreRDRAMSafe<ULONG>(addr, BREAKPOINTSYNC_MAGIC);
		o_PC->ops = SYNC;
		o_PC->f.stype = BREAKPOINTSYNC_MAGIC_STYPE;
	}
}
void PCBreak(void *p_, unsigned long addr) {
	AddrBreakFuncVec *p = (AddrBreakFuncVec*)p_;
	AddrBreakFuncVec &f = *p;
	for(AddrBreakFuncVec::iterator itt = f.begin();
		itt != f.end(); itt ++) {
		lua_State *L = itt->lua;
		lua_getfield(L, LUA_REGISTRYINDEX, REG_PCBREAK);
		lua_pushinteger(L, itt->idx);
		lua_gettable(L, -2);
		PushIntU(L, addr);
		if(GetLuaClass(L)->errorPCall(1, 0)) {
			PCBreak(p, addr);
			return;
		}
		lua_pop(L, 1);
	}
}
extern void LuaDCUpdate(int redraw);
template<typename T>struct TypeIndex{enum{v = -1};};
template<>struct TypeIndex<UCHAR>{enum{v = 0};};
template<>struct TypeIndex<USHORT>{enum{v = 1};};
template<>struct TypeIndex<ULONG>{enum{v = 2};};
template<>struct TypeIndex<ULONGLONG>{enum{v = 3};};
template<typename T, bool rw>
void AtMemoryRW() {
	MemoryHashInfo *(&hashMap)[0x10000] = rw ? writeHashMap : readHashMap;
	AddrBreakMap &breakMap = rw ? writeBreakMap : readBreakMap;
	AddrBreakMap::iterator it = breakMap.find(address);
	if(it == breakMap.end()) {
		hashMap[address>>16]->func[TypeIndex<T>::v]();
		return;
	}else {
		break_value_flag = true;
		current_break_value_size = sizeof(T);
		AddrBreakFuncVec &f = it->second.func;
		for(AddrBreakFuncVec::iterator itt = f.begin();
			itt != f.end(); itt ++) {
			lua_State *L = itt->lua;
			lua_getfield(L, LUA_REGISTRYINDEX,
				rw ? REG_WRITEBREAK : REG_READBREAK);
			lua_pushinteger(L, itt->idx);
			lua_gettable(L, -2);
			PushIntU(L, address);
			PushIntU(L, sizeof(T));
			if(GetLuaClass(L)->errorPCall(2, 0)){
				if(hashMap[address>>16] != NULL)
					AtMemoryRW<T, rw>();
				return;
			}
			lua_pop(L, 1);
		}
		if(rw && !break_value_flag){
			if(sizeof(T)==1){
				g_byte = (T)break_value;
			}else if(sizeof(T)==2){
				hword = (T)break_value;
			}else if(sizeof(T)==4){
				word = (T)break_value;
			}else if(sizeof(T)==8){
				dword = (T)break_value;
			}
		}
		hashMap[address>>16]->func[TypeIndex<T>::v]();
		if(!rw && !break_value_flag) {
			*rdword = break_value;
		}
		break_value_flag = false;
		return;
	}
}
template<typename T>
void AtMemoryReadT() {
	AtMemoryRW<T, false>();
}

template<typename T>
void AtMemoryWriteT() {
	AtMemoryRW<T, true>();
}

void(*AtMemoryRead[])() = {
	AtMemoryReadT<UCHAR>, AtMemoryReadT<USHORT>,
	AtMemoryReadT<ULONG>, AtMemoryReadT<ULONGLONG>,
};
void(*AtMemoryWrite[])() = {
	AtMemoryWriteT<UCHAR>, AtMemoryWriteT<USHORT>,
	AtMemoryWriteT<ULONG>, AtMemoryWriteT<ULONGLONG>,
};

//input

const char* KeyName[256] =
{
	NULL, "leftclick", "rightclick", NULL,
	"middleclick", NULL, NULL, NULL,
	"backspace", "tab", NULL, NULL,
	NULL, "enter", NULL, NULL,
	"shift", "control", "alt", "pause", // 0x10
	"capslock", NULL, NULL, NULL,
	NULL, NULL, NULL, "escape",
	NULL, NULL, NULL, NULL,
	"space", "pageup", "pagedown", "end", // 0x20
	"home", "left", "up", "right",
	"down", NULL, NULL, NULL,
	NULL, "insert", "delete", NULL,
	"0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
	NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	"A", "B", "C", "D", "E", "F", "G", "H", "I", "J",
	"K", "L", "M", "N", "O", "P", "Q", "R", "S", "T",
	"U", "V", "W", "X", "Y", "Z",
	NULL, NULL, NULL, NULL, NULL,
	"numpad0", "numpad1", "numpad2", "numpad3", "numpad4", "numpad5", "numpad6", "numpad7", "numpad8", "numpad9",
	"numpad*", "numpad+",
	NULL,
	"numpad-","numpad.","numpad/",
	"F1","F2","F3","F4","F5","F6","F7","F8","F9","F10","F11","F12",
	"F13","F14","F15","F16","F17","F18","F19","F20","F21","F22","F23","F24",
	NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
	"numlock", "scrolllock",
	NULL, // 0x92
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, // 0xB9
	"semicolon", "plus", "comma", "minus",
	"period", "slash", "tilde",
	NULL, // 0xC1
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, // 0xDA
	"leftbracket", "backslash", "rightbracket", "quote",
};

int GetKeys(lua_State *L) {
	lua_newtable(L);
	for(int i = 1; i < 255; i++) {
		const char* name = KeyName[i];
		if(name) {
			int active;
			if(i == VK_CAPITAL || i == VK_NUMLOCK || i == VK_SCROLL)
				active = GetKeyState(i) & 0x01;
			else
				active = GetAsyncKeyState(i) & 0x8000;
			if(active) {
				lua_pushboolean(L, true);
				lua_setfield(L, -2, name);
			}
		}
	}

	POINT mouse;
	GetCursorPos(&mouse);
	ScreenToClient(mainHWND, &mouse);
	lua_pushinteger(L, mouse.x);
	lua_setfield(L, -2, "xmouse");
	lua_pushinteger(L, mouse.y);
	lua_setfield(L, -2, "ymouse");
	return 1;
}
/*
	local oinp
	emu.atvi(function()
		local inp = input.get()
		local dinp = input.diff(inp, oinp)
		...
		oinp = inp
	end)
*/
int GetKeyDifference(lua_State *L) {
	if(lua_isnil(L, 1)) {
		lua_newtable(L);
		lua_insert(L, 1);
	}
	luaL_checktype(L, 2, LUA_TTABLE);
	lua_newtable(L);
	lua_pushnil(L);
	while(lua_next(L, 1)) {
		lua_pushvalue(L, -2);
		lua_gettable(L, 2);
		if(lua_isnil(L, -1)) {
			lua_pushvalue(L, -3);
			lua_pushboolean(L, 1);
			lua_settable(L, 3);
		}
		lua_pop(L, 2);
	}
	return 1;
}
INT_PTR CALLBACK InputPromptProc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	static lua_State *L;
	switch(msg) {
	case WM_INITDIALOG:{
		L = (lua_State*)lParam;
		std::string str(luaL_optstring(L, 2, ""));
		SetWindowText(wnd,
			luaL_optstring(L, 1, "input:"));
		std::string::size_type p = 0;
		while((p = str.find('\n', p)) != std::string::npos){
			str.replace(p, 1, "\r\n");
			p += 2;
		}
		SetWindowText(GetDlgItem(wnd, IDC_TEXTBOX_LUAPROMPT),
			str.c_str());
		SetFocus(GetDlgItem(wnd, IDC_TEXTBOX_LUAPROMPT));
		break;
		}
	case WM_COMMAND:
		switch(LOWORD(wParam)){
		case IDOK:{
			HWND inp = GetDlgItem(wnd, IDC_TEXTBOX_LUAPROMPT);
			int size = GetWindowTextLength(inp)+1;
			char *buf = new char[size];
			GetWindowText(inp, buf, size);
			std::string str(buf);
			delete buf;
			std::string::size_type p = 0;
			while((p = str.find("\r\n", p)) != std::string::npos){
				str.replace(p, 2, "\n");
				p += 1;
			}
			lua_pushstring(L, str.c_str());
			EndDialog(wnd, 0);
			break;
		}
		case IDCANCEL:
			lua_pushnil(L);
			EndDialog(wnd, 1);
			break;
		}
		break;
	}
	return FALSE;
}
int InputPrompt(lua_State *L) {
	DialogBoxParam(app_hInstance,
		MAKEINTRESOURCE(IDD_LUAINPUTPROMPT), mainHWND,
		InputPromptProc, (LPARAM)L);
	return 1;
}

//joypad
int GetJoypad(lua_State *L) {
	int i = luaL_optinteger(L, 1, 1)-1;
	if(i < 0 || i >= 4) {
		luaL_error(L, "port: 1-4");
	}
	BUTTONS b = *(BUTTONS*)&lastInputLua[i];
	lua_newtable(L);
#define A(a,s) lua_pushboolean(L,b.a);lua_setfield(L, -2, s)
	A(R_DPAD,"right");
	A(L_DPAD,"left");
	A(D_DPAD,"down");
	A(U_DPAD,"up");
	A(START_BUTTON,"start");
	A(Z_TRIG,"Z");
	A(B_BUTTON,"B");
	A(A_BUTTON,"A");
	A(R_CBUTTON,"Cright");
	A(L_CBUTTON,"Cleft");
	A(D_CBUTTON,"Cdown");
	A(U_CBUTTON,"Cup");
	A(R_TRIG,"R");
	A(L_TRIG,"L");
//	A(Reserved1,"reserved1");
//	A(Reserved2,"reserved2");
	lua_pushinteger(L, b.X_AXIS);
	lua_setfield(L, -2, "Y");			//X��Y���t�A�㉺��t(�オ��)
	lua_pushinteger(L, b.Y_AXIS);	//X��Y�͒������A�㉺�͒����Ȃ�(-128�Ƃ����͂����獬���̌�)
	lua_setfield(L, -2, "X");
#undef A
	return 1;
}

int SetJoypad(lua_State *L) {
	int a_2 = 2;
	int i;
	if(lua_type(L, 1) == LUA_TTABLE) {
		a_2 = 1;
		i = 0;
	}else {
		i = luaL_optinteger(L, 1, 1)-1;
	}
	if(i < 0 || i >= 4) {
		luaL_error(L, "control: 1-4");
	}
	BUTTONS *b = (BUTTONS*)&rewriteInputLua[i];
	lua_pushvalue(L, a_2);
#define A(a,s) lua_getfield(L, -1, s);b->a=lua_toboolean(L,-1);lua_pop(L,1);
	A(R_DPAD,"right");
	A(L_DPAD,"left");
	A(D_DPAD,"down");
	A(U_DPAD,"up");
	A(START_BUTTON,"start");
	A(Z_TRIG,"Z");
	A(B_BUTTON,"B");
	A(A_BUTTON,"A");
	A(R_CBUTTON,"Cright");
	A(L_CBUTTON,"Cleft");
	A(D_CBUTTON,"Cdown");
	A(U_CBUTTON,"Cup");
	A(R_TRIG,"R");
	A(L_TRIG,"L");
//	A(Reserved1,"reserved1");
//	A(Reserved2,"reserved2");
	lua_getfield(L, -1, "Y");
	b->X_AXIS = lua_tointeger(L,-1);lua_pop(L,1);
	lua_getfield(L, -1, "X");
	b->Y_AXIS = lua_tointeger(L,-1);lua_pop(L,1);
	rewriteInputFlagLua[i] = true;
#undef A
	return 1;
}

const luaL_Reg globalFuncs[] = {
	{"print", Print},
	{"printx", PrintX},
	{"tostringex", ToStringExs},
	{"stop", StopScript},
	//floating number
	{"MTC1", MoveToSingle},
	{"DMTC1", MoveToDouble},
	{"MFC1", MoveFromSingle},
	{"DMFC1", MoveFromDouble},
	{"CVT_D_L", ConvertDwordToNumber},
	{NULL, NULL}
};
//�G���Ȋ֐�
const luaL_Reg emuFuncs[] = {
	{"console", ConsoleWriteLua},
	{"debugview", DebugviewWrite},
	{"statusbar", StatusbarWrite},

	{"atvi", RegisterVI},
	{"atupdatescreen", RegisterUpdateScreen},
	{"atinput", RegisterInput},
	{"atstop", RegisterStop},
	{"atwindowmessage", RegisterWindowMessage},
	{"atinterval", RegisterInterval},
	{"atplaymovie", RegisterPlayMovie},
	{"atstopmovie", RegisterStopMovie},
	{"atloadstate", RegisterLoadState},
	{"atsavestate", RegisterSaveState},
	{"atreset", RegisterReset},

	{"framecount", GetVICount},
	{"samplecount", GetSampleCount},
	{"inputcount", GetInputCount},

	{"getversion", GetMupenVersion},

	{"pause", EmuPause},
	{"getpause", GetEmuPause},
	{"getspeed", GetSpeed},
	{"speed", SetSpeed},
	{"speedmode", SetSpeedMode},
	{"setgfx", SetGFX},
	
	{"getaddress", GetAddress},
	
	{"isreadonly", GetVCRReadOnly},

	{"getsystemmetrics", GetSystemMetricsLua}, // irrelevant to core but i dont give a 
	{"ismainwindowinforeground", IsMainWindowInForeground},

	{"screenshot", Screenshot},

	{NULL, NULL}
};
const luaL_Reg memoryFuncs[] = {
	//�D���Ȗ��O
	{"LBU", LoadByteUnsigned},
	{"LB", LoadByteSigned},
	{"LHU", LoadHalfUnsigned},
	{"LH", LoadHalfSigned},
	{"LWU", LoadWordUnsigned},
	{"LW", LoadWordSigned},
	{"LDU", LoadDwordUnsigned},
	{"LD", LoadDwordSigned},
	{"LWC1", LoadFloat},
	{"LDC1", LoadDouble},
	{"loadsize", LoadSizeUnsigned},
	{"loadbytes", LoadTs<UCHAR>},
	{"loadhalfs", LoadTs<USHORT>},
	{"loadwords", LoadTs<ULONG>},

	{"SB", StoreByte},
	{"SH", StoreHalf},
	{"SW", StoreWord},
	{"SD", StoreDword},
	{"SWC1", StoreFloat},
	{"SDC1", StoreDouble},
	{"storesize", StoreSize},

	{"syncbreak", SetSyncBreak},	//SyncBreak��PCBreak�ɔ�ׂāA
	{"pcbreak", SetPCBreak},			//��o�^�̂Ƃ���̂ł̏��������Ȃ��Ǝv��
	{"readbreak", SetReadBreak},
	{"writebreak", SetWriteBreak},
	{"reg", GetRegister},
	{"getreg", GetRegister},
	{"setreg", SetRegister},
	{"trace", TraceLog},
	{"tracemode", TraceLogMode},
	{"getcore", GetCore},
	{"recompilenow", RecompileNowLua},
	{"recompile", RecompileLua},
	{"recompilenext", RecompileNextLua},
	{"recompilenextall", RecompileNextAllLua},

	//IO����A�N�Z�X
	{"readmemb", ReadMemT<UCHAR,readmemb>},
	{"readmemh", ReadMemT<USHORT,readmemh>},
	{"readmem", ReadMemT<ULONG,readmem>},
	{"readmemd", ReadMemT<ULONGLONG,readmemd>},
	{"writememb", ReadMemT<UCHAR,writememb>},
	{"writememh", ReadMemT<USHORT,writememh>},
	{"writemem", ReadMemT<ULONG,writemem>},
	{"writememd", ReadMemT<ULONGLONG,writememd>},

	//��ʓI�Ȗ��O(word=2byte)
	{"readbytesigned", LoadByteSigned},
	{"readbyte", LoadByteUnsigned},
	{"readwordsigned", LoadHalfSigned},
	{"readword", LoadHalfUnsigned},
	{"readdwordsigned", LoadWordSigned},
	{"readdword", LoadWordUnsigned},
	{"readqwordsigned", LoadDwordSigned},
	{"readqword", LoadDwordUnsigned},
	{"readfloat", LoadFloat},
	{"readdouble", LoadDouble},
	{"readsize", LoadSizeUnsigned},
	{"readbyterange", LoadTs<UCHAR>},

	{"writebyte", StoreByte},
	{"writeword", StoreHalf},
	{"writedword", StoreWord},
	{"writelong", StoreWord},
	{"writeqword", StoreDword},
	{"writefloat", StoreFloat},
	{"writedouble", StoreDouble},
	{"writesize", StoreSize},

	{"registerread", SetReadBreak},
	{"registerwrite", SetWriteBreak},
	{"registerexec", SetSyncBreak},
	{"getregister", GetRegister},
	{"setregister",SetRegister},

	{NULL, NULL}
};

const luaL_Reg guiFuncs[] = {
	{"register", RegisterUpdateScreen},
	{NULL, NULL}
};

//winAPI GDI�֐���
const luaL_Reg wguiFuncs[] = {
	{"setbrush", SetBrush},
	{"setpen", SetPen},
	{"setcolor", SetTextColor},
	{"setbk", SetBackgroundColor},
	{"setfont", SetFont},
	{"text", LuaTextOut},
	{"drawtext", LuaDrawText},
	{"gettextextent", GetTextExtent},
	{"rect", DrawRect},
	{"fillrect", FillRect},
	/*<GDIPlus>*/
	// GDIPlus functions marked with "a" suffix
	{"fillrecta", FillRectAlpha},
	{"fillellipsea", FillEllipseAlpha},
	{"fillpolygona", FillPolygonAlpha},
	{"loadimage", LuaLoadImage},
	{"deleteimage", DeleteImage},
	{"drawimage", DrawImage},
	{"loadscreen", LoadScreen},
	{"loadscreenreset", LoadScreenReset},
	{"getimageinfo", GetImageInfo},
	/*</GDIPlus*/
	{"ellipse", DrawEllipse},
	{"polygon", DrawPolygon},
	{"line", DrawLine},
	{"info", GetGUIInfo},
	{"resize", ResizeWindow},
	{NULL, NULL}
};

const luaL_Reg inputFuncs[] = {
	{"get", GetKeys},
	{"diff", GetKeyDifference},
	{"prompt", InputPrompt},
	{NULL, NULL}
};
const luaL_Reg joypadFuncs[] = {
	{"get", GetJoypad},
	{"set", SetJoypad},
	{"register",RegisterInput},
	{"count", GetInputCount},
	{NULL, NULL}
};

const luaL_Reg movieFuncs[] = {
	{"playmovie", PlayMovie},
	{"stopmovie", StopMovie},
	{"getmoviefilename", GetMovieFilename},
	{NULL, NULL}
};

const luaL_Reg savestateFuncs[] = {
	{"savefile", SaveFileSavestate},
	{"loadfile", LoadFileSavestate},
	{NULL, NULL}
};
const luaL_Reg ioHelperFuncs[] = {
		{"filediag", LuaFileDialog},
		{NULL, NULL}
};
const luaL_Reg aviFuncs[] = {
	{"startcapture", StartCapture},
	{"stopcapture", StopCapture},
	{NULL, NULL}
};

}	//namespace

#if 1

void NewLuaScript(void(*callback)()) {
	LuaEngine::CreateLuaWindow(callback);
}

void CloseAllLuaScript(void) {
	if (LuaEngine::luaWindowMap.empty()||LuaEngine::luaWindows.empty()) { 
		MUPEN64RR_DEBUGINFO("No scripts running");
		anyLuaRunning = false;
		return;
	}
	MUPEN64RR_DEBUGINFO("Close all scripts");
	LuaEngine::LuaMessage::Msg *msg = new LuaEngine::LuaMessage::Msg();
	msg->type = LuaEngine::LuaMessage::CloseAll;
	LuaEngine::luaMessage.post(msg);
}
bool IsLuaConsoleMessage(MSG* msg)
{
	/*
	if(std::find(
		(std::vector<HWND>::const_iterator)LuaEngine::luaWindows.begin(),
		(std::vector<HWND>::const_iterator)LuaEngine::luaWindows.end(),
		msg->hwnd) != LuaEngine::luaWindows.end()) {
		IsDialogMessage(msg->hwnd, msg);
		return true;
	}
	*/
	return false;
}
void AtUpdateScreenLuaCallback() {
	GetLuaMessage();
	LuaEngine::registerFuncEach(LuaEngine::AtUpdateScreen, LuaEngine::REG_ATUPDATESCREEN);
}
void AtVILuaCallback() {
	GetLuaMessage();
	LuaEngine::registerFuncEach(LuaEngine::AtVI, LuaEngine::REG_ATVI);
}
void AtInputLuaCallback(int n) {
	GetLuaMessage();
	LuaEngine::current_input_n = n;
	LuaEngine::registerFuncEach(LuaEngine::AtInput, LuaEngine::REG_ATINPUT);
	LuaEngine::inputCount ++;
}
void AtIntervalLuaCallback() {
	LuaEngine::registerFuncEach(LuaEngine::CallTop, LuaEngine::REG_ATINTERVAL);
}

void AtPlayMovieLuaCallback() {
	LuaEngine::registerFuncEach(LuaEngine::CallTop, LuaEngine::REG_ATPLAYMOVIE);
}

void AtStopMovieLuaCallback() {
	LuaEngine::registerFuncEach(LuaEngine::CallTop, LuaEngine::REG_ATSTOPMOVIE);
}

void AtLoadStateLuaCallback() {
	LuaEngine::registerFuncEach(LuaEngine::CallTop, LuaEngine::REG_ATLOADSTATE);
}

void AtSaveStateLuaCallback() {
	LuaEngine::registerFuncEach(LuaEngine::CallTop, LuaEngine::REG_ATSAVESTATE);
}

//called after reset, when emulation ready
void AtResetCallback() {
	LuaEngine::registerFuncEach(LuaEngine::CallTop, LuaEngine::REG_ATRESET);
}

void GetLuaMessage(){
	LuaEngine::luaMessage.getMessages();
}
void LuaBreakpointSyncPure(){
	LuaEngine::BreakpointSyncPure();
}
void LuaBreakpointSyncInterp() {
	LuaEngine::BreakpointSyncInterp();
}


void LuaReload() {
	LuaEngine::LuaMessage::Msg *msg = new LuaEngine::LuaMessage::Msg();
	msg->type = LuaEngine::LuaMessage::ReloadFirst;
	LuaEngine::luaMessage.post(msg);
}

// allow loading multiple lua scripts at a time
static char ExternallyLoadedPaths[MAX_LUA_OPEN_AND_RUN_INSTANCES][MAX_PATH];
static int elpLoadIndex = 0; // increment after runPath message has been sent
static int elpSaveIndex = 0; // increment after new console created

// Send a lua message to run a path on the last lua window.
// This works off the assumption that no new windows were made between
// the time the desired window was created, and the time this callback
// is run (practically safe, theoretically not good)
static void RunExternallyLoadedPath() {
	LuaEngine::LuaMessage::Msg *msg = new LuaEngine::LuaMessage::Msg();
	msg->type = LuaEngine::LuaMessage::RunPath;
	strcpy(msg->runPath.path, ExternallyLoadedPaths[elpLoadIndex]);
	elpLoadIndex = (elpLoadIndex + 1) % MAX_LUA_OPEN_AND_RUN_INSTANCES;
	msg->runPath.wnd = NULL; 
	LuaEngine::luaMessage.post(msg);
}

// Stores the path to be used after a new lua window is created
void LuaOpenAndRun(const char *path) {
	strcpy(ExternallyLoadedPaths[elpSaveIndex], path);
	elpSaveIndex = (elpSaveIndex + 1) % MAX_LUA_OPEN_AND_RUN_INSTANCES;
	PostMessage(mainHWND, WM_COMMAND, ID_MENU_LUASCRIPT_NEW, (LPARAM)RunExternallyLoadedPath);
}

void InitializeLuaDC(HWND mainWnd) {
	LuaEngine::InitializeLuaDC_(mainWnd);
}

//Draws lua, somewhere, either straight to window or to buffer, then buffer to dc
//Next and DrawLuaDC are only used with double buffering
//otherwise calls vi callback and updatescreen callback
void lua_new_vi(int redraw){

	
	int can_repaint = (LuaEngine::lua_dc && redraw) ? 1 : 0;

	// we copy window front buffer to the lua dc, 
	if(can_repaint) {
		LuaEngine::copy_window_to_lua_dc();
	}

	// then let user draw crap over it,
	AtVILuaCallback();

	// and finally we push it back directly to the window

	// FIXME:
	// (somewhat unrealistic, as it requires a spec change)
	// don't give video plugin window handle, instead let it perform
	// hardware-accelerated drawing on a bitmap with some additional info (w, h, pixel format) provided by the emulator
	// this way, the video plugin can't overwrite our window contents whenever it wants, thereby causing flicker
	if(can_repaint) {
		AtUpdateScreenLuaCallback();
		LuaEngine::copy_lua_dc_to_window();
	}
}

//�Ƃ肠����lua�ɓ���Ƃ�
char traceLoggingBuf[0x10000];
char *traceLoggingPointer = traceLoggingBuf;
inline void TraceLoggingBufFlush() {
	DWORD writeen;
	WriteFile(LuaEngine::TraceLogFile,
		traceLoggingBuf, traceLoggingPointer-traceLoggingBuf, &writeen, NULL);
	traceLoggingPointer = traceLoggingBuf;
}
inline void TraceLoggingWriteBuf() {
	const char * const buflength = traceLoggingBuf + sizeof(traceLoggingBuf) - 512;
	if(traceLoggingPointer >= buflength) {
		TraceLoggingBufFlush();
	}
}

void instrStr2(r4300word pc, r4300word w, char* p1) {
	char*& p = p1;
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

	switch (decode.format) {
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
	p1[strlen(p1)] = '\0';
#undef HEX8
#undef REGCPU
#undef REGFPU
#undef REGCPU2
#undef REGFPU2
}

void instrStr1(unsigned long pc, unsigned long w, char* p1) {
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
	if (w == 0x00000000) {
		*(p++) = 'n';
		*(p++) = 'o';
		*(p++) = 'p';
	}
	else {
		for (const char* q = GetOpecodeString(&decode); *q; q++) {
			*(p++) = *q;
		}
		*(p++) = ' ';
		p = GetOperandString(p, &decode, pc);
	}
	for (int i = p - ps + 3; i < 24; i += 4) {
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

	if (delay_slot) {
		*(p++) = '#';
	}
	switch (decode.format) {
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

void TraceLogging(r4300word pc, r4300word w) {
	char *&p = traceLoggingPointer;
	INSTDECODE decode;
	const char * const x = "0123456789abcdef";
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
	const char *ps = p;
	if(w == 0x00000000) {
		*(p++) = 'n';
		*(p++) = 'o';
		*(p++) = 'p';
	}else {
		for(const char *q = GetOpecodeString(&decode); *q; q ++) {
			*(p++) = *q;
		}
		*(p++) = ' ';
		p = GetOperandString(p, &decode, pc);
	}
	for(int i = p-ps+3; i < 24; i += 4) {
		*(p++) = '\t';
	}
	*(p++) = ';';
	INSTOPERAND &o = decode.operand;
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

	if(delay_slot) {
		*(p++) = '#';
	}
	switch(decode.format) {
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
		REGCPU2(o.i.rs,o.i.rt);
		break;
	case INSTF_ADDRW:
		REGCPU(o.i.rt);
		if(o.i.rt!=0){C;}
	case INSTF_ADDRR:
		*(p++) = '@';
		*(p++) = '=';
		HEX8(reg[o.i.rs]+(r4300halfsigned)o.i.immediate);
		break;
	case INSTF_LFW:
		REGFPU(o.lf.ft);
		C;
	case INSTF_LFR:
		*(p++) = '@';
		*(p++) = '=';
		HEX8(reg[o.lf.base]+(r4300halfsigned)o.lf.offset);
		break;
	case INSTF_R1:
		REGCPU(o.r.rd);
		break;
	case INSTF_R2:
		REGCPU2(o.i.rs,o.i.rt);
		break;
	case INSTF_R3:
		REGCPU2(o.i.rs,o.i.rt);
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
		REGFPU2(o.cf.fs,o.cf.ft);
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

void TraceLoggingBin(r4300word pc, r4300word w){
	char *&p = traceLoggingPointer;
	INSTDECODE decode;
	//little endian
#define HEX8(n) 	*(r4300word*)p = n; p += 4

	DecodeInstruction(w, &decode);
	HEX8(pc);
	HEX8(w);
	INSTOPERAND &o = decode.operand;
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

	switch(decode.format) {
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
		REGCPU2(o.i.rs,o.i.rt);
		break;
	case INSTF_ADDRW:
		HEX8(reg[o.i.rs]+(r4300halfsigned)o.i.immediate);
		REGCPU(o.i.rt);
		break;
	case INSTF_ADDRR:
		HEX8(reg[o.i.rs]+(r4300halfsigned)o.i.immediate);
		NONE;
		break;
	case INSTF_LFW:
		HEX8(reg[o.lf.base]+(r4300halfsigned)o.lf.offset);
		REGFPU(o.lf.ft);
		break;
	case INSTF_LFR:
		HEX8(reg[o.lf.base]+(r4300halfsigned)o.lf.offset);
		NONE;
		break;
	case INSTF_R1:
		REGCPU(o.r.rd);
		NONE;
		break;
	case INSTF_R2:
		REGCPU2(o.i.rs,o.i.rt);
		break;
	case INSTF_R3:
		REGCPU2(o.i.rs,o.i.rt);
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
		REGFPU2(o.cf.fs,o.cf.ft);
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

void LuaTraceLoggingPure() {
	if(!traceLogMode) {
		TraceLogging(interp_addr, op);
	}else {
		TraceLoggingBin(interp_addr, op);
	}
}

void LuaTraceLoggingInterpOps(){
#ifdef LUA_TRACEINTERP
	if(enableTraceLog) {
		if(!traceLogMode) {
			TraceLogging(PC->addr, PC->src);
		}else {
			TraceLoggingBin(PC->addr, PC->src);
		}
	}
	PC->s_ops();
#endif
}


void LuaTraceLogState() {
	if (!enableTraceLog) return;
	LuaEngine::EmulationLock lock;
	char filename[MAX_PATH] = "trace.log";
	if (fdLuaTraceLog.ShowFileDialog(filename, L"*.log", FALSE, FALSE, mainHWND)) {
		LuaEngine::TraceLogStart(filename);
	}
	else {
		LuaEngine::TraceLogStop();
	}
}


void LuaWindowMessage(HWND wnd, UINT msg, WPARAM w, LPARAM l) {
	LuaEngine::LuaMessage::Msg *m = new LuaEngine::LuaMessage::Msg();
	m->type = LuaEngine::LuaMessage::WindowMessage;
	m->windowMessage.wnd = mainHWND;
	m->windowMessage.msg = msg;
	m->windowMessage.wParam = w;
	m->windowMessage.lParam = l;
	LuaEngine::luaMessage.post(m);
}



#endif

#else
// bad: empty dummy functions
void AtUpdateScreenLuaCallback(){};
void AtVILuaCallback()			{};
void AtInputLuaCallback(int n)	{};
void AtIntervalLuaCallback()	{};
void AtPlayMovieLuaCallback()	{};
void AtStopMovieLuaCallback()	{};
void AtLoadStateLuaCallback()	{};
void AtSaveStateLuaCallback()	{};
void AtResetCallback()			{};
void GetLuaMessage()			{};
void LuaBreakpointSyncPure()	{};
void LuaBreakpointSyncInterp()	{};
void LuaOpenAndRun(char const* n) {};
void CloseAllLuaScript(void) {};
void instrStr1(r4300word pc, r4300word w, char* buffer){};
void instrStr2(r4300word pc, r4300word w, char* buffer){};
#endif
