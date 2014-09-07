
/*
 * IDA WinHelp Viewer
 *
 * Copyright	2010 Fabio Fornaro <fabio.fornaro@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "stdafx.h"

#include "winhelp.h"
#include "winhelp_res.h"
#include "macro.h"

#define NONAMELESSUNION
#define NONAMELESSSTRUCT

#define HELPFILE "x86eas.hlp"
#define PLUGINFILE "IDAWinHelpViewer.plw"

extern void WINHELP_InitFonts(HWND hWnd);
extern void WINHELP_SetupText(HWND hTextWnd, WINHELP_WINDOW* win, ULONG relative);
extern void CALLBACK MACRO_Finder();
extern HLPFILE_LINK* WINHELP_FindLink(WINHELP_WINDOW* win, LPARAM pos);
extern BOOL WINHELP_HandleTextMouse(WINHELP_WINDOW* win, UINT msg, LPARAM lParam);
extern void cb_KWBTree(void *p, void **next, void *cookie);
extern BOOL HLPFILE_GetKeywords(HLPFILE *hlpfile);
extern HLPFILE *first_hlpfile;

HMODULE RichedDLL = 0;
WINHELP_WNDPAGE wpage;
WINHELP_WINDOW win;

extern LRESULT CALLBACK WINHELP_RicheditWndProc(HWND hWnd, UINT msg,WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK IDAHELP_WndProc(HWND hWnd, UINT msg,WPARAM wParam, LPARAM lParam);

BOOL CALLBACK IDAHELP_DisplayKeyword(HWND hRichText, LPCSTR word);
LRESULT CALLBACK IDAHELP_InitIndexData(HLPFILE* helpFile);
BOOL CALLBACK IDAHELP_CurrentInstructionHelpDisplay();

HWND hBackButton = 0;
HWND hIndexButton = 0;
HWND hSelectButton = 0;
HWND hSummaryButton = 0;
HWND hHelpDBSelectButton = 0;
HWND hHiddenKeywordsList = 0;
HWND hIDAWindow = 0;
HWND hTextWnd = 0;
WNDPROC oldIDAWindowWndProc = 0;
WNDPROC origRicheditWndProc;
HLPFILE* hlpfile;
TForm *form;
index_data indexData;


//--------------------------------------------------------------------------
char comment[] = "IDA Pro WinHelp Viewer Plugin.";

char help[] =
"IDA Pro WinHelp Plugin module\n"
"\n"
"This module shows Winhelp Format Files.";


//--------------------------------------------------------------------------
// This is the preferred name of the plugin module in the menu system
// The preferred name may be overriden in plugins.cfg file

char wanted_name[] = "IDA Pro WinHelp Viewer";


// This is the preferred hotkey for the plugin module
// The preferred hotkey may be overriden in plugins.cfg file
// Note: IDA won't tell you if the hotkey is not correct
//       It will just disable the hotkey.

char wanted_hotkey[] = "F2";

void CALLBACK IDAHELP_CreateControls(HWND parent, HINSTANCE hInst)
{
	hBackButton = CreateWindow ("BUTTON", TEXT("Back"),
		WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
		0, 0, 100, 40, parent, NULL,
		hInst, NULL);

	SetWindowLong(hBackButton,GWL_ID, BTN_BACK);

	hIndexButton = CreateWindow ("BUTTON", TEXT("Index"),
		WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
		110, 0, 100, 40, parent, NULL,
		hInst, NULL);

	SetWindowLong(hIndexButton,GWL_ID, BTN_IDX);

	hSummaryButton = CreateWindow ("BUTTON", TEXT("Summary"),
		WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
		220, 0, 100, 40, parent, NULL,
		hInst, NULL);

	SetWindowLong(hSummaryButton,GWL_ID, BTN_SUMMARY);

	hSelectButton = CreateWindow ("BUTTON", TEXT("..."),
		WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
		360, 0, 60, 40, parent, NULL,
		hInst, NULL);

	SetWindowLong(hSelectButton,GWL_ID, BTN_SELECT);

	hHiddenKeywordsList = CreateWindow("LISTBOX", TEXT("Keywords"), WS_CHILD,0, 4, 100, 500, parent, NULL, hInst, NULL);

	SetWindowLong(hHiddenKeywordsList,GWL_ID,LST_KWRDS);

	hTextWnd = CreateWindow(RICHEDIT_CLASS, NULL,
		ES_MULTILINE | ES_READONLY | WS_CHILD | WS_HSCROLL | WS_VSCROLL | WS_VISIBLE | WS_BORDER,
		0, 50, 1330, 500, parent, NULL,  hInst, NULL);

	SendMessage(hTextWnd, EM_SETEVENTMASK, 0, SendMessage(hTextWnd, EM_GETEVENTMASK, 0, 0) | ENM_MOUSEEVENTS);

	oldIDAWindowWndProc = (WNDPROC) SetWindowLongPtr(hIDAWindow, GWLP_WNDPROC,(LONG_PTR)IDAHELP_WndProc);

	win.origRicheditWndProc = (WNDPROC) SetWindowLongPtr(hTextWnd, GWLP_WNDPROC,(LONG_PTR)WINHELP_RicheditWndProc);

	origRicheditWndProc = win.origRicheditWndProc;
}

HLPFILE* IDAHELP_LoadWinHelpFile(LPCSTR lpszPath)
{
	hlpfile =  HLPFILE_ReadHlpFile(lpszPath);
	HLPFILE_GetKeywords(hlpfile);
	IDAHELP_InitIndexData(hlpfile);

	msg("[%s] Loaded '%s' from '%s', %s\n",wanted_name,hlpfile->lpszTitle,hlpfile->lpszPath, hlpfile->lpszCopyright);

	return hlpfile;
}

void IDAHELP_InitWinHelp(HLPFILE* hlpFile, HWND hRichText)
{
	win.hHandCur = LoadCursor(NULL,IDC_HAND);
	win.hMainWnd = hIDAWindow;

	win.info = WINHELP_GetWindowInfo(hlpFile,"main");
	Globals.active_win = &win;

	WINHELP_InitFonts(hRichText);
}

void GetModuleDirectory(HMODULE hModule,char*szDirectory)
{
	char *p, *p2;

	// get current directory
	GetModuleFileName(hModule, szDirectory, MAX_PATH);

	// trim off the exe name
	p = _tcsstr(szDirectory, _T("\\"));

	if(p == NULL)
	{
		// no slash found - return root?
		qstpncpy(szDirectory, _T("\\"),sizeof(_T("\\")));

		return;
	}

	while( p != NULL)
	{
		p2 = p + 1;
		p = _tcsstr(p2, _T("\\"));
	}

	// null terminate to crop
	//p2--;
	*p2 = '\0';

	return;
}

void IDAHELP_RunIDAWinHelp(LPCSTR lpszPath)
{

	ZeroMemory(&win.back,sizeof(WINHELP_PAGESET));

	if(win.page != NULL)
		ZeroMemory(win.page,sizeof(HLPFILE_PAGE));
	
	if(win.info != NULL)
		ZeroMemory(win.info,sizeof(HLPFILE_WINDOWINFO));

	ZeroMemory(&win,sizeof(WINHELP_WINDOW));
	ZeroMemory(&wpage,sizeof(WINHELP_WNDPAGE));

	win.origRicheditWndProc = origRicheditWndProc;

	HLPFILE* hlpFile = IDAHELP_LoadWinHelpFile(lpszPath);
	IDAHELP_InitWinHelp(hlpFile, hTextWnd);

	WINHELP_OpenHelpWindow(HLPFILE_PageByHash,hlpFile,0,win.info,0);
	
	IDAHELP_CurrentInstructionHelpDisplay();
}

void IDAHELP_CloseIDAWinHelp(HLPFILE* hlpFile)
{
	HLPFILE_FreeHlpFile(hlpFile);
}

void IDAHELP_SelectExternalWinHelp()
{
	OPENFILENAME ofn;		
	char szFile[MAX_PATH];

	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hIDAWindow;
	ofn.lpstrFile = szFile;
	ofn.hInstance = Globals.hInstance;
	ofn.lpstrFile[0] = '\0';
	ofn.nMaxFile = sizeof(szFile);
	ofn.lpstrFilter = "WinHelp\0*.HLP\0";
	ofn.nFilterIndex = 1;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = NULL;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

	if (GetOpenFileName(&ofn)==TRUE) 
	{
		 WINHELP_DeleteBackSet(&win);

		if(hlpfile)
			IDAHELP_CloseIDAWinHelp(hlpfile);

		IDAHELP_RunIDAWinHelp(ofn.lpstrFile);
	}
}

void IDAHELP_OpenDefaultWinHelp()
{
	char path[MAX_PATH];
	GetModuleDirectory((HMODULE)Globals.hInstance,path);
	char* helpFileStr = (char*)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(path)+sizeof(HELPFILE));

	memcpy(helpFileStr,path,sizeof(path));
	memcpy((helpFileStr+strlen(path)),HELPFILE,sizeof(HELPFILE));

	HANDLE hf = CreateFile(helpFileStr, GENERIC_READ,
							0, (LPSECURITY_ATTRIBUTES) NULL,
							OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
							(HANDLE) NULL);

	if(hf == INVALID_HANDLE_VALUE)
		IDAHELP_SelectExternalWinHelp();
	else
	{
		CloseHandle(hf);

		IDAHELP_RunIDAWinHelp(helpFileStr);

		HeapFree(GetProcessHeap(),0,helpFileStr);
	}
}

//--------------------------------------------------------------------------
static int idaapi ui_callback(void *user_data, int notification_code, va_list va)
{
	if ( notification_code == ui_tform_visible )
	{
		TForm *form = va_arg(va, TForm *);
		if ( form == user_data )
		{
			hIDAWindow   = va_arg(va, HWND);

			IDAHELP_CreateControls(hIDAWindow, Globals.hInstance);
			
			IDAHELP_OpenDefaultWinHelp();
		}
	}
	if ( notification_code == ui_tform_invisible )
	{
		TForm *form = va_arg(va, TForm *);
		if ( form == user_data )
		{
			IDAHELP_CloseIDAWinHelp(hlpfile);

			DestroyWindow(hTextWnd);
			DestroyWindow(hIndexButton);
			DestroyWindow(hBackButton);
			DestroyWindow(hSelectButton);
			
			msg("[%s] Closed.\n",wanted_name);
		}
	}
	return 0;
}

//--------------------------------------------------------------------------
int idaapi init(void)
{
	// stay in the memory since most likely we will define our own window proc
	// and add subcontrols to the window

	InitCommonControls();

	RichedDLL = LoadLibrary("RICHED32.DLL");

	if (RichedDLL == NULL)
	{
		msg("[%s] RICHED32.DLL is not available. Skipping Plugin.\n",wanted_name);
		return PLUGIN_SKIP;
	}

	return ( callui(ui_get_hwnd).vptr != NULL ) ? PLUGIN_OK : PLUGIN_SKIP;
}

//--------------------------------------------------------------------------
void idaapi term(void)
{
	unhook_from_notification_point(HT_UI, ui_callback);
}

BOOL CALLBACK IDAHELP_CurrentInstructionHelpDisplay()
{
	ea_t ea = get_screen_ea();

	if ( !isCode(get_flags_novalue(ea)) )
	{
		msg("[%s] Not an instruction.\n",wanted_name);
		WINHELP_OpenHelpWindow(HLPFILE_PageByHash,hlpfile,0,win.info,0);
		return FALSE;

	}else{

		decode_insn(ea);
		switchto_tform(form,TRUE);

		if(!IDAHELP_DisplayKeyword(hTextWnd,cmd.get_canon_mnem()))
		{
			msg("[%s] '%s' Not Found.\n",wanted_name, cmd.get_canon_mnem());
			WINHELP_OpenHelpWindow(HLPFILE_PageByHash,hlpfile,0,win.info,0);
			return FALSE;
		}
	}

	return TRUE;
}

//--------------------------------------------------------------------------
void idaapi run(int /*arg*/)
{
	HWND hwnd = NULL;

	Globals.hInstance = (HINSTANCE)GetModuleHandle(PLUGINFILE);

	form = create_tform("IDA WinHelp", &hwnd);
	if ( hwnd != NULL )
	{
		hook_to_notification_point(HT_UI, ui_callback, form);
		open_tform(form, FORM_TAB|FORM_MENU|FORM_RESTORE);
	}
	else 
		IDAHELP_CurrentInstructionHelpDisplay();
	
}



//--------------------------------------------------------------------------
//
//      PLUGIN DESCRIPTION BLOCK
//
//--------------------------------------------------------------------------
plugin_t PLUGIN =
{
	IDP_INTERFACE_VERSION,
	0,                    // plugin flags
	init,                 // initialize

	term,                 // terminate. this pointer may be NULL.

	run,                  // invoke plugin

	comment,              // long comment about the plugin
	// it could appear in the status line
	// or as a hint

	help,                 // multiline help about the plugin

	wanted_name,          // the preferred short name of the plugin
	wanted_hotkey         // the preferred hotkey to run the plugin
};


////////////////////////////


//The sample data to be displayed

//The width of each column
int widths[] = {100, 100};
//The headers for each column
char *headers[] = {"Title", "Path"};
//The format strings for each column
char *formats[] = {"%s", "%s"};

//this function expects obj to point to a zero terminated array
//of non-zero integers.
ulong idaapi idabook_sizer(void *obj) {
	int *p = (int*)obj;
	int count = 0;
	while (*p++) count++;
	return count;
}

/*
* obj In this function obj is expected to point to an array of integers
* n indicates which line (1..n) of the display is being formatted.
*   if n is zero, the header line is being requested.
* cells is a pointer to an array of character pointers. This array
*       contains one pointer for each column in the chooser.  The output
*       for each column should not exceed the corresponding width specified
*       in the widths array.
*/
void idaapi idabook_getline_2(void *obj, ulong n, char* const *cells) {
	char *p = (char*)obj;
	if (n == 0) {
		for (int i = 0; i < 2; i++) {
			qstrncpy(cells[i], headers[i], widths[i]);
		}
	}
	else {
		for (int i = 0; i < 2; i++) {
			qsnprintf(cells[i], widths[i], formats[i], p[n - 1]);
		}
	}
}

///////////////////////////

LRESULT CALLBACK IDAHELP_WndProc(HWND hWnd, UINT mesg,WPARAM wParam, LPARAM lParam)
{

	//msg("MESSAGE 0x%x\n",mesg);

	RECT rc;
	HDC hdc;
	PAINTSTRUCT ps;

	int widths[] = {16, 16, 16};

	switch(mesg)
	{
	case WM_PAINT:

		hdc = BeginPaint(hWnd , &ps);
		GetClientRect(hWnd,&rc);
		FillRect(hdc,&rc,(HBRUSH)SelectObject(hdc ,GetStockObject(WHITE_BRUSH)));
		EndPaint(hWnd , &ps);
		return 0;

	case WM_COMMAND:
		switch(wParam)
		{
		case BTN_BACK:
			MACRO_Back();
			break;
		case BTN_IDX:
			MACRO_Finder();
			break;
		case BTN_SELECT:
			WINHELP_DeleteBackSet(&win);
			IDAHELP_SelectExternalWinHelp();
			break;
		case BTN_SUMMARY:
			WINHELP_DeleteBackSet(&win);
			WINHELP_OpenHelpWindow(HLPFILE_PageByHash,hlpfile,0,win.info,0);
			break;
		default:
			break;
		}
		break;
	default:
		return CallWindowProcA(oldIDAWindowWndProc, hWnd, mesg, wParam, lParam);
	}

	return CallWindowProcA(oldIDAWindowWndProc, hWnd, mesg, wParam, lParam);

}


LRESULT CALLBACK WINHELP_RicheditWndProc(HWND hWnd, UINT msg,WPARAM wParam, LPARAM lParam)
{
	//WINHELP_WINDOW *win = (WINHELP_WINDOW*) GetWindowLongPtr(GetParent(hWnd), 0);
	DWORD messagePos;
	POINT pt;
	HLPFILE_LINK* helpFileLink;

	if(win.back.index <= 1)
		EnableWindow(hBackButton,FALSE);
	else
		EnableWindow(hBackButton,TRUE);

	switch(msg)
	{
	case WM_LBUTTONDOWN:
		messagePos = GetMessagePos();
		pt.x = (short)LOWORD(messagePos);
		pt.y = (short)HIWORD(messagePos);
		ScreenToClient(hWnd, &pt);
		helpFileLink = WINHELP_FindLink(&win, MAKELPARAM(pt.x, pt.y));
		if (win.page && helpFileLink)
		{
			return WINHELP_HandleTextMouse(&win, msg, lParam);
		}

	case WM_SETCURSOR:
		messagePos = GetMessagePos();
		pt.x = (short)LOWORD(messagePos);
		pt.y = (short)HIWORD(messagePos);
		ScreenToClient(hWnd, &pt);
		if (win.page && WINHELP_FindLink(&win, MAKELPARAM(pt.x, pt.y)))
		{
			SetCursor(win.hHandCur);
			return 0;
		}
		/* fall through */
	default:

		return CallWindowProcA(win.origRicheditWndProc, hWnd, msg, wParam, lParam);
	}
}

BOOL CALLBACK IDAHELP_DisplayKeyword(HWND hRichText, LPCSTR word)
{
	BYTE *p;
	int count;
	struct index_data* id;
	int pstrlen = 0;
	int searchIndex = 0;

	p = (BYTE*)word;
	count = *(short*)((char *)p + strlen((char *)p) + 1);

	searchIndex = SendMessage(hHiddenKeywordsList,LB_FINDSTRING,-1,(LPARAM)word);

	if(searchIndex < 0)
		return FALSE;

	p = (BYTE*)SendMessage(hHiddenKeywordsList,LB_GETITEMDATA,searchIndex,0);

	id = &indexData;
	id->offset = *(ULONG*)((char *)p + strlen((char *)p) + 3);
	id->offset = *(long*)(id->hlpfile->kwdata + id->offset + 9);

	id->jump = TRUE;

	return WINHELP_OpenHelpWindow(HLPFILE_PageByOffset, id->hlpfile, id->offset, Globals.active_win->info, SW_NORMAL);

}

LRESULT CALLBACK IDAHELP_InitIndexData(HLPFILE* helpFile)
{
	struct index_data* id;

	id = &indexData;
	id->hlpfile = helpFile;
	HLPFILE_BPTreeEnum(id->hlpfile->kwbtree, cb_KWBTree,hHiddenKeywordsList);
	id->jump = FALSE;
	id->offset = 1;

	return 0;
}



