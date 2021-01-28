﻿/*
*	Copyright(c) 2020 lutianming email：641471957@qq.com
*
*	Sheeps may be copied only under the terms of the GNU Affero General Public License v3.0
*/

// pch.h: 这是预编译标头文件。
// 下方列出的文件仅编译一次，提高了将来生成的生成性能。
// 这还将影响 IntelliSense 性能，包括代码完成和许多代码浏览功能。
// 但是，如果此处列出的文件中的任何一个在生成之间有更新，它们全部都将被重新编译。
// 请勿在此处添加要频繁更新的文件，这将使得性能优势无效。

#ifndef PCH_H
#define PCH_H

#if !defined(__WINDOWS__) && (defined(WIN32) || defined(WIN64) || defined(_MSC_VER) || defined(_WIN32))
#define __WINDOWS__
#endif

#ifdef __WINDOWS__
#define WIN32_LEAN_AND_MEAN             // 从 Windows 头文件中排除极少使用的内容
#include <windows.h>

extern HMODULE Sheeps_Module;
#else
#define MAX_PATH 260
#endif // __WINDOWS__
#include "cJSON.h"
#include "common.h"
#include "log.h"
#include "mycrypto.h"
#include "sqlite3.h"
#include "Config.h"

extern char EXE_Path[];
extern char ConfigFile[];
extern char ProjectPath[];
extern char RecordPath[];
extern char LogPath[];
#endif //PCH_H
