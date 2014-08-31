// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>
#include <windef.h>
#include <winbase.h>
#include <wingdi.h>
#include <winuser.h>
#include <commdlg.h>
#include <shellapi.h>
#include <richedit.h>
#include <commctrl.h>

// C RunTime Header Files

#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

#include <ida.hpp>
#include <idp.hpp>
#include <loader.hpp>
#include <kernwin.hpp>
#include <pro.h>
#include <bytes.hpp>

#ifdef DEBUG

#define WINE_ERR(...)	msg(__VA_ARGS__);
#define WINE_FIXME(...) msg(__VA_ARGS__);
#define WINE_WARN(...)	msg(__VA_ARGS__);
#define WINE_TRACE(...) msg(__VA_ARGS__);

#else

#define WINE_ERR(...)	msg(__VA_ARGS__);
#define WINE_FIXME(...)
#define WINE_WARN(...)
#define WINE_TRACE(...)

#endif

