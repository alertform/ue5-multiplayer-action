#include "Config/MALuaAbilityConfig.h"
#include "Config/MALuaBridge.h"
#include "Misc/Paths.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogMALuaConfig, Log, All);

FString UMALuaAbilityConfig::DefaultConfigPath()
{
	return FPaths::ProjectContentDir() / TEXT("Lua") / TEXT("AbilityConfig.lua");
}

void UMALuaAbilityConfig::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Reload(); // 首载；失败则留空缓存（全部走 UPROPERTY 回退），控制台命令可再来

	ReloadCmd = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("MA.Lua.ReloadAbilityConfig"),
		TEXT("Reload Content/Lua/AbilityConfig.lua into the ability config cache."),
		FConsoleCommandDelegate::CreateWeakLambda(this, [this]() { Reload(); }),
		ECVF_Default);
}

void UMALuaAbilityConfig::Deinitialize()
{
	if (ReloadCmd)
	{
		IConsoleManager::Get().UnregisterConsoleObject(ReloadCmd);
		ReloadCmd = nullptr;
	}
	Super::Deinitialize();
}

float UMALuaAbilityConfig::GetFloat(FName ConfigKey, FName Param, float Default) const
{
	return MAConfigLookup::GetFloat(Cache, ConfigKey, Param, Default);
}

FName UMALuaAbilityConfig::GetName(FName ConfigKey, FName Param, FName Default) const
{
	return MAConfigLookup::GetName(Cache, ConfigKey, Param, Default);
}

bool UMALuaAbilityConfig::Reload()
{
	TMap<FName, FMAConfigTable> Fresh;
	FString Err;
	if (!MALuaBridge::LoadFromFile(DefaultConfigPath(), Fresh, Err))
	{
		UE_LOG(LogMALuaConfig, Error, TEXT("AbilityConfig reload failed (keeping previous): %s"), *Err);
		return false;
	}
	Cache = MoveTemp(Fresh);
	UE_LOG(LogMALuaConfig, Display, TEXT("AbilityConfig loaded: %d abilities"), Cache.Num());
	OnConfigReloaded.Broadcast();
	return true;
}

bool UMALuaAbilityConfig::LoadFromString(const FString& LuaSource)
{
	TMap<FName, FMAConfigTable> Fresh;
	FString Err;
	if (!MALuaBridge::LoadFromString(LuaSource, Fresh, Err))
	{
		UE_LOG(LogMALuaConfig, Error, TEXT("AbilityConfig load-from-string failed: %s"), *Err);
		return false;
	}
	Cache = MoveTemp(Fresh);
	OnConfigReloaded.Broadcast();
	return true;
}
