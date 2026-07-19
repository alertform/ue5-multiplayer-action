#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Config/MAConfigTypes.h"
#include "MALuaAbilityConfig.generated.h"

struct IConsoleCommand;

/** 技能数值 Lua 配置：加载/热重载时把 Lua 读进 C++ 缓存，战斗热路径只读缓存。 */
UCLASS()
class MULTIPLAYERACTION_API UMALuaAbilityConfig : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** 取 number，缺键返回 Default。 */
	float GetFloat(FName ConfigKey, FName Param, float Default) const;
	/** 取 string 为 FName，缺键返回 Default。 */
	FName GetName(FName ConfigKey, FName Param, FName Default) const;

	/** 重读配置文件；失败保留上一份好缓存。返回是否成功。 */
	bool Reload();

	/** 测试缝：直接从 Lua 源字符串灌缓存（跳过文件 IO）。 */
	bool LoadFromString(const FString& LuaSource);

	/** ProjectContentDir()/Script/AbilityConfig.lua（与 UnLua 脚本同层）。 */
	static FString DefaultConfigPath();

	DECLARE_MULTICAST_DELEGATE(FOnConfigReloaded);
	FOnConfigReloaded OnConfigReloaded;

private:
	TMap<FName, FMAConfigTable> Cache;
	IConsoleCommand* ReloadCmd = nullptr;
};
