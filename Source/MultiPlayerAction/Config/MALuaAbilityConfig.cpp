#include "Config/MALuaAbilityConfig.h"
#include "Config/MALuaBridge.h"
#include "Misc/Paths.h"
#include "Containers/Ticker.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogMALuaConfig, Log, All);

FString UMALuaAbilityConfig::DefaultConfigPath()
{
	// 与 UnLua 同层的 Content/Script —— 所有 lua 一个目录，打包 staging 也共用同一条目。
	return FPaths::ProjectContentDir() / TEXT("Script") / TEXT("AbilityConfig.lua");
}

void UMALuaAbilityConfig::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Reload(); // 首载；失败则留空缓存（全部走 UPROPERTY 回退），控制台命令可再来

	ReloadCmd = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("MA.Lua.ReloadAbilityConfig"),
		TEXT("Reload Content/Script/AbilityConfig.lua into the ability config cache."),
		FConsoleCommandDelegate::CreateWeakLambda(this, [this]() { Reload(); }),
		ECVF_Default);

#if !UE_BUILD_SHIPPING
	// 存盘即生效：秒级轮询时间戳（一次 stat 调用，成本可忽略），变了自动重载。
	// 用 FTSTicker 而非 Tick —— GameInstanceSubsystem 本身不带 Tick。
	LastConfigTimestamp = IFileManager::Get().GetTimeStamp(*DefaultConfigPath());
	FileWatchTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,
		[this](float) -> bool
		{
			const FDateTime Stamp = IFileManager::Get().GetTimeStamp(*DefaultConfigPath());
			if (Stamp != LastConfigTimestamp)
			{
				LastConfigTimestamp = Stamp;
				const bool bOk = Reload();
				UE_LOG(LogMALuaConfig, Display, TEXT("AbilityConfig.lua 已保存 → 自动热重载%s"),
					bOk ? TEXT("成功") : TEXT("失败（语法错误？缓存保留上一份）"));
			}
			return true; // 持续轮询
		}), 1.f);
#endif
}

void UMALuaAbilityConfig::Deinitialize()
{
#if !UE_BUILD_SHIPPING
	if (FileWatchTicker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(FileWatchTicker);
		FileWatchTicker.Reset();
	}
#endif
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
