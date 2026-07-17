#pragma once

#include "CoreMinimal.h"
#include "Config/MAConfigTypes.h"

/** 唯一依赖 UnLua 的层：把"返回 table-of-tables 的 Lua 代码"在一个短命 UnLua env 里跑出来，
 *  解析成 C++ 缓存。失败返回 false 并填 OutError，Out 保持不变。 */
namespace MALuaBridge
{
	bool LoadFromString(const FString& LuaSource, TMap<FName, FMAConfigTable>& Out, FString& OutError);
	bool LoadFromFile(const FString& LuaPath, TMap<FName, FMAConfigTable>& Out, FString& OutError);
}
