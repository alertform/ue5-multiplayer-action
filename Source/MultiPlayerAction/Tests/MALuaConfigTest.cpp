#include "Misc/AutomationTest.h"
#include "Config/MALuaBridge.h"
#include "Config/MALuaAbilityConfig.h"
#include "Engine/GameInstance.h"

#if WITH_DEV_AUTOMATION_TESTS

// 需要 UnLua 模块（自建短命 FLuaEnv）→ EditorContext（在编辑器内跑，插件已加载）
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMALuaBridgeParseTest,
	"MultiPlayerAction.LuaConfig.BridgeParse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMALuaBridgeParseTest::RunTest(const FString& Parameters)
{
	const FString Src = TEXT(
		"return {\n"
		"  MeleeAttack = { TraceRadius = 120, MontagePlayRate = 2.0 },\n"
		"  Fireball    = { ExplosionRadius = 600, MuzzleSocketName = \"hand_l\" },\n"
		"}\n");

	TMap<FName, FMAConfigTable> Out;
	FString Err;
	const bool bOk = MALuaBridge::LoadFromString(Src, Out, Err);
	TestTrue(FString::Printf(TEXT("parse ok (%s)"), *Err), bOk);
	if (!bOk) { return false; }

	TestEqual(TEXT("melee trace"),  MAConfigLookup::GetFloat(Out, TEXT("MeleeAttack"), TEXT("TraceRadius"), 0.f), 120.f);
	TestEqual(TEXT("melee rate"),   MAConfigLookup::GetFloat(Out, TEXT("MeleeAttack"), TEXT("MontagePlayRate"), 0.f), 2.f);
	TestEqual(TEXT("fireball aoe"), MAConfigLookup::GetFloat(Out, TEXT("Fireball"), TEXT("ExplosionRadius"), 0.f), 600.f);
	TestEqual(TEXT("fireball socket"),
		MAConfigLookup::GetName(Out, TEXT("Fireball"), TEXT("MuzzleSocketName"), FName(TEXT("x"))),
		FName(TEXT("hand_l")));

	// 语法错 → 失败且不动 Out
	TMap<FName, FMAConfigTable> Keep = Out;
	FString Err2;
	const bool bBad = MALuaBridge::LoadFromString(TEXT("return { this is not lua"), Keep, Err2);
	TestFalse(TEXT("bad lua fails"), bBad);
	TestEqual(TEXT("Out untouched on failure"),
		MAConfigLookup::GetFloat(Keep, TEXT("MeleeAttack"), TEXT("TraceRadius"), 0.f), 120.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMALuaSubsystemTest,
	"MultiPlayerAction.LuaConfig.Subsystem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMALuaSubsystemTest::RunTest(const FString& Parameters)
{
	// UGameInstanceSubsystem 的 ClassWithin = UGameInstance：裸 NewObject 会触发 invalid-Outer ensure
	UGameInstance* GI = NewObject<UGameInstance>();
	UMALuaAbilityConfig* Cfg = NewObject<UMALuaAbilityConfig>(GI);

	TestTrue(TEXT("load v1"), Cfg->LoadFromString(
		TEXT("return { Fireball = { ExplosionRadius = 300 } }")));
	TestEqual(TEXT("v1 value"), Cfg->GetFloat(TEXT("Fireball"), TEXT("ExplosionRadius"), 0.f), 300.f);
	TestEqual(TEXT("v1 fallback"), Cfg->GetFloat(TEXT("Fireball"), TEXT("Nope"), 42.f), 42.f);

	// reload 换值语义
	TestTrue(TEXT("load v2"), Cfg->LoadFromString(
		TEXT("return { Fireball = { ExplosionRadius = 600 } }")));
	TestEqual(TEXT("v2 value"), Cfg->GetFloat(TEXT("Fireball"), TEXT("ExplosionRadius"), 0.f), 600.f);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
