// Copyright 2018, Institute for Artificial Intelligence - University of Bremen
// Author: Andrei Haidu (http://haidu.eu)

#include "MCFixationGrasp.h"
#include "Animation/SkeletalMeshActor.h"
#include "Engine/StaticMeshActor.h"
#include "Kismet/GameplayStatics.h"

// Default constructor
UMCFixationGrasp::UMCFixationGrasp()
{	
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// Default sphere radius
	InitSphereRadius(3.0f);

	// Default physics properties
	SetGenerateOverlapEvents(true);

	// Default values
	InputActionName = "LeftFixate";
	bWeldBodies = true;
	WeightLimit = 15.0f;
	VolumeLimit = 30000.0f; // 1000cm^3 = 1 Liter
}

// Called when the game starts
void UMCFixationGrasp::BeginPlay()
{
	Super::BeginPlay();

	// Bind user input
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
	{
		if (UInputComponent* IC = PC->InputComponent)
		{
			UMCFixationGrasp::SetupInputBindings(IC);
		}
	}

	// Bind overlap functions
	OnComponentBeginOverlap.AddDynamic(this, &UMCFixationGrasp::OnOverlapBegin);
	OnComponentEndOverlap.AddDynamic(this, &UMCFixationGrasp::OnOverlapEnd);
}

// Bind user inputs
void UMCFixationGrasp::SetupInputBindings(UInputComponent* InIC)
{
	InIC->BindAction(InputActionName, IE_Pressed, this, &UMCFixationGrasp::Grasp);
	InIC->BindAction(InputActionName, IE_Released, this, &UMCFixationGrasp::Release);
}

// Try to fixate overlapping object to parent
void UMCFixationGrasp::Grasp()
{
	while (!GraspedObject && ObjectsInSphereArea.Num() > 0)
	{
		// Get an object from the sphere area and try to grasp it
		AStaticMeshActor* SMA = ObjectsInSphereArea.Pop();

		// Check if the object can be grasped (not to heavy/large)
		if (UMCFixationGrasp::CanObjectBeGrasped(SMA))
		{
			// Try to fixate the object to parent
			if (UMCFixationGrasp::Fixate(SMA))
			{
				// Broadcast starting of grasp event
				OnGraspBegin.Broadcast(GetOwner()->GetUniqueID(), GraspedObject->GetUniqueID(), GetWorld()->GetTimeSeconds());

				// Clear objects in sphere
				ObjectsInSphereArea.Empty();

				break;
			}
		}
	}
}

// Free fixated object from parent
void UMCFixationGrasp::Release()
{
	if (GraspedObject)
	{
		// Cache the current grasped object velocity (it will be re-applied to the released object)
		const FVector CachedVelocity = GraspedObject->GetVelocity();

		// Detach the static mesh component
		UStaticMeshComponent* SMC = GraspedObject->GetStaticMeshComponent();
		{
			SMC->DetachFromComponent(FDetachmentTransformRules(EDetachmentRule::KeepWorld, true));

			// Enable physics with and apply current hand velocity, clear pointer to object
			SMC->SetSimulatePhysics(true);
			SMC->SetPhysicsLinearVelocity(CachedVelocity);

			// Enable and update overlaps
			SetGenerateOverlapEvents(true);
			UpdateOverlaps();

			// Broadcast ending of grasp event
			OnGraspEnd.Broadcast(GetOwner()->GetUniqueID(), GraspedObject->GetUniqueID(), GetWorld()->GetTimeSeconds());
			
			// Clear fixate object reference
			GraspedObject = nullptr;
		}
	}
}

// Check if the object can be grasped (not too heavy/large)
bool UMCFixationGrasp::CanObjectBeGrasped(AStaticMeshActor* InObject)
{
	// Check if the object is movable
	if (!InObject->IsRootComponentMovable())
	{
		return false;
	}

	// Check if actor has a valid static mesh component
	if (UStaticMeshComponent* SMC = InObject->GetStaticMeshComponent())
	{
		// Check if component has physics on
		if (!SMC->IsSimulatingPhysics())
		{
			return false;
		}

		// Check that object is not too heavy/large
		if (SMC->GetMass() < WeightLimit && 
			InObject->GetComponentsBoundingBox().GetVolume() < VolumeLimit)
		{
			return true;
		}
	}

	return false;
}

// Fixate the given object to parent
bool UMCFixationGrasp::Fixate(AStaticMeshActor* InObject)
{
	if (UStaticMeshComponent* SMC = InObject->GetStaticMeshComponent())
	{
		// Check if  the object can be attach to the parent
		if (SMC->AttachToComponent(GetOwner()->GetRootComponent(), 
			FAttachmentTransformRules(EAttachmentRule::KeepWorld, bWeldBodies)))
		{
			// Disable physics
			SMC->SetSimulatePhysics(false);

			// Set the pointer to the grasped object
			GraspedObject = InObject;

			// Disable overlap checks during fixation grasp
			SetGenerateOverlapEvents(false);

			return true;
		}
	}
	return false;
}

// Called on overlap begin events
void UMCFixationGrasp::OnOverlapBegin(UPrimitiveComponent* OverlappedComp,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (AStaticMeshActor* OtherAsSMA = Cast<AStaticMeshActor>(OtherActor))
	{
		ObjectsInSphereArea.Emplace(OtherAsSMA);
	}
}

// Called on overlap end events
void UMCFixationGrasp::OnOverlapEnd(UPrimitiveComponent* OverlappedComp,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	// Remove actor from array (if present)
	if (AStaticMeshActor* OtherAsSMA = Cast<AStaticMeshActor>(OtherActor))
	{
		ObjectsInSphereArea.Remove(OtherAsSMA);
	}
}