// Copyright 2018, Institute for Artificial Intelligence - University of Bremen
// Author: Andrei Haidu (http://haidu.eu)

#include "MCParallelGripperController.h"
#include "Kismet/GameplayStatics.h"

// Default constructor
UMCParallelGripperController::UMCParallelGripperController()
{
	// Bind update function pointer  with default
	UpdateFunctionPointer = &UMCParallelGripperController::Update_NONE;
}

// Init controller
void UMCParallelGripperController::Init(EMCGripperControlType ControlType,
	const FName& InputAxisName,
	UPhysicsConstraintComponent* LeftFingerConstraint,
	UPhysicsConstraintComponent* RightFingerConstraint,
	float InP, float InI, float InD, float InMax)
{
	if (LeftFingerConstraint == nullptr || RightFingerConstraint == nullptr)
	{
		return;
	}

	// Set member values
	LeftConstraint = LeftFingerConstraint;
	RightConstraint = RightFingerConstraint;
	LeftLimit = LeftConstraint->ConstraintInstance.GetLinearLimit();
	RightLimit = RightConstraint->ConstraintInstance.GetLinearLimit();

	// Set the user input bindings
	if(UWorld* World = LeftFingerConstraint->GetWorld())
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
		{
			if (UInputComponent* IC = PC->InputComponent)
			{
				UMCParallelGripperController::SetupInputBindings(IC, InputAxisName);
			}
		}
	}

	// Initialize control types
	switch (ControlType)
	{
	case EMCGripperControlType::Position:
		// TODO not implemented
		break;
	case EMCGripperControlType::LinearDrive:
		UMCParallelGripperController::SetupLinearDrive(InP, InD, InMax);
		break;
	case EMCGripperControlType::Acceleration:
		// TODO not implemented
		break;
	case EMCGripperControlType::Force:
		// TODO not implemented
		break;
	}
}

// Bind user input to function
void UMCParallelGripperController::SetupInputBindings(UInputComponent* InIC, const FName& InputAxisName)
{
	InIC->BindAxis(InputAxisName, this, &UMCParallelGripperController::Update);
}

// Setup the controller for linear drive (PD controller)
// force = spring * (targetPosition - position) + damping * (targetVelocity - velocity)
void UMCParallelGripperController::SetupLinearDrive(float Spring, float Damping, float ForceLimit)
{
	// TODO hardcoded for X-axis movement, and direction
	// Set movement axis
	LeftConstraint->SetLinearPositionDrive(true, false, false);
	RightConstraint->SetLinearPositionDrive(true, false, false);
	
	// Set linear driver parameters, it is a proportional derivative (PD) drive, 
	// where force = spring * (targetPosition - position) + damping * (targetVelocity - velocity)
	LeftConstraint->SetLinearDriveParams(Spring, Damping, ForceLimit);
	RightConstraint->SetLinearDriveParams(Spring, Damping, ForceLimit);
	
	// Bind update functions
	UpdateFunctionPointer = &UMCParallelGripperController::Update_LinearDriver;
}

// Update function bound to the input
void UMCParallelGripperController::Update(float Value)
{
	(this->*UpdateFunctionPointer)(Value);	
}

/* Default update function */
void UMCParallelGripperController::Update_NONE(float Value)
{
}

/* Update function for the linear driver */
void UMCParallelGripperController::Update_LinearDriver(float Value)
{
	// Value is normalized x=[0,1]
	// Mapping function is:
	// f(x) = (1-x)*MinLim + x*MaxLim; 
	// since the limits are symmetrical, MinLim = -MaxLim, the function becomes:
	// f(x) = (1-x)*-MaxLim + x*MaxLim => f(x) = (x-1)MaxLim + x*MaxLim

	// Left target becomes
	const float LeftTarget = ((Value - 1.f) * LeftLimit) + (Value * LeftLimit);
	
	// Right target is mirrored, hence a multiplication by -1 is needed
	const float RightTarget = ((1.f - Value) * RightLimit) - (Value * RightLimit);

	// Apply target command
	LeftConstraint->SetLinearPositionTarget(FVector(LeftTarget, 0.f, 0.f));
	RightConstraint->SetLinearPositionTarget(FVector(RightTarget, 0.f, 0.f));
	UE_LOG(LogTemp, Warning, TEXT(">> %s::%d Val=%f, LeftTarget=%f, RightTarget=%f"),
		TEXT(__FUNCTION__), __LINE__, Value, LeftTarget, RightTarget);

	FVector OutLinearConstraintForceLeft;
	FVector OutAngularConstraintForceLeft;
	LeftConstraint->GetConstraintForce(OutLinearConstraintForceLeft, OutAngularConstraintForceLeft);

	FVector OutLinearConstraintForceRight;
	FVector OutAngularConstraintForceRight;
	RightConstraint->GetConstraintForce(OutLinearConstraintForceRight, OutAngularConstraintForceRight);

	UE_LOG(LogTemp, Warning, TEXT(">> %s::%d Force: Left.X=%f, Right.X=%f"),
		TEXT(__FUNCTION__), __LINE__,
		OutLinearConstraintForceLeft.X, OutLinearConstraintForceRight.X);

	UE_LOG(LogTemp, Warning, TEXT(">> %s::%d *** ** ** ***"), TEXT(__FUNCTION__), __LINE__);
}