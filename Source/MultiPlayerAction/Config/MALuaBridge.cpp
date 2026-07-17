#include "Config/MALuaBridge.h"
#include "Misc/FileHelper.h"

// === Task 0 落定 (UnLua 2.3.6) ===
#include "lua.hpp"          // Lua C API（UnLua Public 目录，公有 include）
#include "LuaEnv.h"         // UnLua::FLuaEnv —— 自建短命 env 供配置解析

namespace
{
	// 从栈顶的 table 解析一层 <FName -> number|string> 到 OutTable。
	void ParseLeafTable(lua_State* L, FMAConfigTable& OutTable)
	{
		lua_pushnil(L);                       // 首个 key
		while (lua_next(L, -2) != 0)          // -2 = 该 leaf table
		{
			// key 在 -2, value 在 -1；只认 string key
			if (lua_type(L, -2) == LUA_TSTRING)
			{
				const FName Param(UTF8_TO_TCHAR(lua_tostring(L, -2)));
				const int VT = lua_type(L, -1);
				if (VT == LUA_TNUMBER)
				{
					OutTable.Numbers.Add(Param, lua_tonumber(L, -1));
				}
				else if (VT == LUA_TSTRING)
				{
					OutTable.Strings.Add(Param, FString(UTF8_TO_TCHAR(lua_tostring(L, -1))));
				}
				// 其它类型静默跳过（回退到 UPROPERTY 默认）
			}
			lua_pop(L, 1);                    // 弹 value，留 key 供 lua_next
		}
	}
}

bool MALuaBridge::LoadFromString(const FString& LuaSource, TMap<FName, FMAConfigTable>& Out, FString& OutError)
{
	// 自建短命 UnLua env 专供配置解析：不依赖游戏 env 是否已起、与游戏 Lua 隔离，析构自动清理。
	UnLua::FLuaEnv Env;
	lua_State* L = Env.GetMainState();
	if (!L)
	{
		OutError = TEXT("UnLua FLuaEnv has no lua_State");
		return false;
	}

	const int Top = lua_gettop(L);
	const FTCHARToUTF8 Src(*LuaSource);
	// 载入 chunk
	if (luaL_loadbuffer(L, Src.Get(), Src.Length(), "AbilityConfig") != LUA_OK)
	{
		OutError = FString::Printf(TEXT("lua load error: %s"), UTF8_TO_TCHAR(lua_tostring(L, -1)));
		lua_settop(L, Top);
		return false;
	}
	// 执行，期望 1 个返回值（配置表）
	if (lua_pcall(L, 0, 1, 0) != LUA_OK)
	{
		OutError = FString::Printf(TEXT("lua run error: %s"), UTF8_TO_TCHAR(lua_tostring(L, -1)));
		lua_settop(L, Top);
		return false;
	}
	if (lua_type(L, -1) != LUA_TTABLE)
	{
		OutError = TEXT("AbilityConfig must return a table");
		lua_settop(L, Top);
		return false;
	}

	// 顶层：<技能名(string) -> leaf table>
	TMap<FName, FMAConfigTable> Parsed;
	lua_pushnil(L);
	while (lua_next(L, -2) != 0)
	{
		if (lua_type(L, -2) == LUA_TSTRING && lua_type(L, -1) == LUA_TTABLE)
		{
			const FName ConfigKey(UTF8_TO_TCHAR(lua_tostring(L, -2)));
			FMAConfigTable& Leaf = Parsed.Add(ConfigKey);
			ParseLeafTable(L, Leaf);          // 解析 -1 的 leaf table
		}
		lua_pop(L, 1);
	}

	lua_settop(L, Top);                       // 完全恢复栈（Env 随作用域析构）
	Out = MoveTemp(Parsed);
	return true;
}

bool MALuaBridge::LoadFromFile(const FString& LuaPath, TMap<FName, FMAConfigTable>& Out, FString& OutError)
{
	FString Source;
	if (!FFileHelper::LoadFileToString(Source, *LuaPath))
	{
		OutError = FString::Printf(TEXT("cannot read lua file: %s"), *LuaPath);
		return false;
	}
	return LoadFromString(Source, Out, OutError);
}
