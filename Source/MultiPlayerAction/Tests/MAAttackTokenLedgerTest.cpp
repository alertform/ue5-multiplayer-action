#include "Misc/AutomationTest.h"
#include "AI/Combat/MAAttackTokenLedger.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMAAttackTokenLedgerTest,
	"MultiPlayerAction.AICombat.TokenLedger",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMAAttackTokenLedgerTest::RunTest(const FString& Parameters)
{
	FMAAttackTokenLedger L;
	L.SetCapacity(1);

	// Capacity 1: first claim grants, second holder denied.
	TestTrue(TEXT("grant first"), L.Request(1, 0.f));
	TestFalse(TEXT("deny second"), L.Request(2, 0.f));
	TestEqual(TEXT("one holder"), L.NumHolders(), 1);
	TestTrue(TEXT("holds 1"), L.Holds(1));
	TestFalse(TEXT("not holds 2"), L.Holds(2));

	// Idempotent re-request refreshes the grant time (keeps an active attacker alive past TTL).
	TestTrue(TEXT("re-request ok"), L.Request(1, 1.f));
	TestEqual(TEXT("still one holder"), L.NumHolders(), 1);
	L.ReapExpired(3.5f, 3.f); // 3.5 - 1.0 = 2.5 < 3 -> survives BECAUSE refreshed
	TestTrue(TEXT("refreshed survives reap"), L.Holds(1));

	// Release frees the slot for the next claimant; double release is a no-op false.
	TestTrue(TEXT("release held"), L.Release(1));
	TestFalse(TEXT("double release"), L.Release(1));
	TestTrue(TEXT("slot freed"), L.Request(2, 4.f));

	// TTL reaps only stale grants.
	L.SetCapacity(2);
	TestTrue(TEXT("grant 3"), L.Request(3, 4.f));
	TestTrue(TEXT("refresh 2"), L.Request(2, 10.f));
	L.ReapExpired(13.5f, 3.6f); // holder 2 age 3.5 < 3.6 kept; holder 3 age 9.5 > 3.6 dropped
	TestTrue(TEXT("fresh kept"), L.Holds(2));
	TestFalse(TEXT("stale reaped"), L.Holds(3));

	// Capacity shrink: keeps current holders, denies new until below capacity.
	TestTrue(TEXT("grant 5"), L.Request(5, 14.f)); // holders: 2, 5
	L.SetCapacity(1);
	TestEqual(TEXT("holders kept on shrink"), L.NumHolders(), 2);
	TestFalse(TEXT("deny at shrunk capacity"), L.Request(6, 14.f));
	TestTrue(TEXT("release 5"), L.Release(5));
	TestFalse(TEXT("still full at cap 1"), L.Request(6, 14.f)); // 1 holder == capacity
	TestTrue(TEXT("release 2"), L.Release(2));
	TestTrue(TEXT("grant after drain"), L.Request(6, 15.f));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
