#pragma once

#include "CoreMinimal.h"

/** 一个技能从 Lua 解析出的 tunables：number 与 string 两类，按参数名索引。 */
struct FMAConfigTable
{
	TMap<FName, double>  Numbers;
	TMap<FName, FString> Strings;
};

/** 对已解析缓存的纯查表逻辑。不碰 Lua、不碰 UObject —— 可脱环境单测。 */
namespace MAConfigLookup
{
	inline float GetFloat(const TMap<FName, FMAConfigTable>& Cache, FName ConfigKey, FName Param, float Default)
	{
		if (const FMAConfigTable* Table = Cache.Find(ConfigKey))
		{
			if (const double* V = Table->Numbers.Find(Param))
			{
				return static_cast<float>(*V);
			}
		}
		return Default;
	}

	inline FName GetName(const TMap<FName, FMAConfigTable>& Cache, FName ConfigKey, FName Param, FName Default)
	{
		if (const FMAConfigTable* Table = Cache.Find(ConfigKey))
		{
			if (const FString* V = Table->Strings.Find(Param))
			{
				return FName(**V);
			}
		}
		return Default;
	}
}
