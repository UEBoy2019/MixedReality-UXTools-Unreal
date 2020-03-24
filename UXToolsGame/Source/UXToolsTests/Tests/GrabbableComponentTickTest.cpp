#include "GrabbableComponentTickTest.h"

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Engine.h"
#include "EngineUtils.h"

#include "UxtTestUtils.h"
#include "Input/UxtTouchPointer.h"

static UGrabbableTickTestComponent* CreateTestComponent(UWorld* World, const FVector& Location)
{
	AActor* actor = World->SpawnActor<AActor>();

	USceneComponent* root = NewObject<USceneComponent>(actor);
	actor->SetRootComponent(root);
	root->SetWorldLocation(Location);
	root->RegisterComponent();

	UGrabbableTickTestComponent* testTarget = NewObject<UGrabbableTickTestComponent>(actor);
	testTarget->RegisterComponent();

	FString meshFilename = TEXT("/Engine/BasicShapes/Cube.Cube");
	float meshScale = 0.3f;
	if (!meshFilename.IsEmpty())
	{
		UStaticMeshComponent* mesh = NewObject<UStaticMeshComponent>(actor);
		mesh->SetupAttachment(actor->GetRootComponent());
		mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		mesh->SetCollisionProfileName(TEXT("OverlapAll"));
		mesh->SetGenerateOverlapEvents(true);

		UStaticMesh* meshAsset = LoadObject<UStaticMesh>(actor, *meshFilename);
		mesh->SetStaticMesh(meshAsset);
		mesh->SetRelativeScale3D(FVector::OneVector * meshScale);

		mesh->RegisterComponent();
	}

	return testTarget;
}


class FTestGrabbableComponentTickingCommand : public IAutomationLatentCommand
{
public:
	FTestGrabbableComponentTickingCommand(FAutomationTestBase* Test, UUxtTouchPointer* Pointer, UGrabbableTickTestComponent* Target, bool bEnableGrasp, bool bExpectTicking)
		: Test(Test)
		, Pointer(Pointer)
		, Target(Target)
		, bEnableGrasp(bEnableGrasp)
		, bExpectTicking(bExpectTicking)
		, UpdateCount(0)
	{}

	virtual bool Update() override
	{
		// Two step update:
		// 
		// 1. First update the pointer grasp state.
		//    Return false, so the pointer has one frame to update overlaps and raise events.
		// 2. Test the expected tick behaviour of the target component.

		switch (UpdateCount)
		{
			case 0:
				Pointer->SetGrasped(bEnableGrasp);
				break;

			case 1:
				bool bWasTicked = Target->GetNumTicks() > 0;
				Test->TestEqual(TEXT("Grabbable component ticked"), bExpectTicking, bWasTicked);
				Target->Reset();
				break;
		}

		++UpdateCount;
		return (UpdateCount >= 1);
	}

private:

	FAutomationTestBase* Test;
	UUxtTouchPointer* Pointer;
	UGrabbableTickTestComponent* Target;
	bool bEnableGrasp;
	bool bExpectTicking;

	int UpdateCount;
};


IMPLEMENT_COMPLEX_AUTOMATION_TEST(FGrabbableComponentTickTest, "UXTools.GrabbableComponentTick",
	EAutomationTestFlags::EditorContext |
	EAutomationTestFlags::ClientContext |
	EAutomationTestFlags::ProductFilter)

static const FString TestCase_ComponentTickDisabled = TEXT("ComponentTickDisabled");
static const FString TestCase_TickAlways = TEXT("TickAlways");
static const FString TestCase_TickOnlyWhileGrabbed = TEXT("TickOnlyWhileGrabbed");

void FGrabbableComponentTickTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray <FString>& OutTestCommands) const
{
	OutBeautifiedNames.Add(TestCase_ComponentTickDisabled);
	OutBeautifiedNames.Add(TestCase_TickAlways);
	OutBeautifiedNames.Add(TestCase_TickOnlyWhileGrabbed);

	OutTestCommands.Add(TestCase_ComponentTickDisabled);
	OutTestCommands.Add(TestCase_TickAlways);
	OutTestCommands.Add(TestCase_TickOnlyWhileGrabbed);
}

bool FGrabbableComponentTickTest::RunTest(const FString& Parameters)
{
	// Load the empty test map to run the test in.
	AutomationOpenMap(TEXT("/Game/UXToolsGame/Tests/Maps/TestEmpty"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForMapToLoadCommand());
	UWorld *World = UxtTestUtils::GetTestWorld();

	FVector Center(150, 0, 0);
	UUxtTouchPointer* Pointer = UxtTestUtils::CreateTouchPointer(World, Center + FVector(-15, 0, 0));
	Pointer->SetTouchRadius(30);
	UGrabbableTickTestComponent* Target = CreateTestComponent(World, Center);

	bool bExpectUngraspedTicks = false;
	bool bExpectGraspedTicks = false;
	if (Parameters == TestCase_ComponentTickDisabled)
	{
		Target->SetTickOnlyWhileGrabbed(false);
		Target->SetComponentTickEnabled(false);
		bExpectUngraspedTicks = false;
		bExpectGraspedTicks = false;
	}
	if (Parameters == TestCase_TickAlways)
	{
		Target->SetTickOnlyWhileGrabbed(false);
		Target->SetComponentTickEnabled(true);
		bExpectUngraspedTicks = true;
		bExpectGraspedTicks = true;
	}
	if (Parameters == TestCase_TickOnlyWhileGrabbed)
	{
		Target->SetTickOnlyWhileGrabbed(true);
		bExpectUngraspedTicks = false;
		bExpectGraspedTicks = true;
	}

	// Register all new components.
	World->UpdateWorldComponents(false, false);

	// Have to update overlaps explicitly because the pointer doesn't move.
	Pointer->GetOwner()->UpdateOverlaps();

	// Test without grasping
	ADD_LATENT_AUTOMATION_COMMAND(FTestGrabbableComponentTickingCommand(this, Pointer, Target, false, bExpectUngraspedTicks));

	// Test with grasping
	ADD_LATENT_AUTOMATION_COMMAND(FTestGrabbableComponentTickingCommand(this, Pointer, Target, true, bExpectGraspedTicks));

	ADD_LATENT_AUTOMATION_COMMAND(FExitGameCommand());

	return true;
}