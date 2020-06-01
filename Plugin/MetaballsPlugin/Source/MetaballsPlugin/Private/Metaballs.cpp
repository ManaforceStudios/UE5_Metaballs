
//=================================================
// FileName: Metaballs.cpp
// 
// Created by: Andrey Harchenko
// Project name: Metaballs FX Plugin
// Unreal Engine version: 4.10
// Created on: 2016/03/17
// Initial realisation by: Andreas Jönsson, April 2001
//
// -------------------------------------------------
// For parts referencing UE4 code, the following copyright applies:
// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
//
// Feel free to use this software in any commercial/free game.
// Selling this as a plugin/item, in whole or part, is not allowed.
// See "License.md" for full licensing details.

#include "Metaballs.h"
#include "CMarchingCubes.h"
#include "GameFramework/MovementComponent.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
//#include "Runtime/Core/Public/Async/ParallelFor.h"

constexpr int GetIndex(const int X, const int Y, const int Z, const int GridSize)
{
	/* Replaces all instances of:
	x +
	y*(m_nGridSize + 1) +
    z*(m_nGridSize + 1)*(m_nGridSize + 1)
    */
	
	return X + Y*(GridSize + 1) + Z*(GridSize + 1) * (GridSize + 1);
}

constexpr int GetIndexNoAdd(const int X, const int Y, const int Z, const int GridSize)
{
	/* Replaces all instances of:
	x +
	y*m_nGridSize +
	z*m_nGridSize*m_nGridSize
	*/
	
	return X + Y * GridSize + Z * GridSize * GridSize;
}

DECLARE_CYCLE_STAT(TEXT("MetaBall - Update"), STAT_MetaBallUpdate, STATGROUP_MetaBall);
DECLARE_CYCLE_STAT(TEXT("MetaBall - Render"), STAT_MetaBallRender, STATGROUP_MetaBall);

DECLARE_CYCLE_STAT(TEXT("MetaBall - ComputeNormal"), STAT_MetaBallComputeNormal, STATGROUP_MetaBall);
DECLARE_CYCLE_STAT(TEXT("MetaBall - AddNeighborToList"), STAT_MetaBallAddNeighborToList, STATGROUP_MetaBall);
DECLARE_CYCLE_STAT(TEXT("MetaBall - AddNeighbor"), STAT_MetaBallAddNeighbor, STATGROUP_MetaBall);
DECLARE_CYCLE_STAT(TEXT("MetaBall - ComputeEnergy"), STAT_MetaBallComputeEnergy, STATGROUP_MetaBall);
DECLARE_CYCLE_STAT(TEXT("MetaBall - ComputeGridpointEnergy"), STAT_MetaBallComputeGridpointEnergy, STATGROUP_MetaBall);
DECLARE_CYCLE_STAT(TEXT("MetaBall - ComputeGridVoxel"), STAT_MetaBallComputeGridVoxel, STATGROUP_MetaBall);
DECLARE_CYCLE_STAT(TEXT("MetaBall - SetBallTransform"), STAT_MetaBallSetBallTransform, STATGROUP_MetaBall);


// Sets default values
AMetaballs::AMetaballs(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	UCapsuleComponent* CapsuleComp = ObjectInitializer.CreateDefaultSubobject<UCapsuleComponent>(this, TEXT("RootComp"));
	CapsuleComp->InitCapsuleSize(40.0f, 40.0f);
	CapsuleComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CapsuleComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	CapsuleComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CapsuleComp->SetMobility(EComponentMobility::Movable);
//	RootComponent = CapsuleComp;

	MetaBallsBoundBox = ObjectInitializer.CreateDefaultSubobject<UBoxComponent>(this, TEXT("GridBox"));
	MetaBallsBoundBox->InitBoxExtent(FVector(100, 100, 100));
//	MetaBallsBoundBox->AttachParent = RootComponent;




	m_mesh = ObjectInitializer.CreateDefaultSubobject<UProceduralMeshComponent>(this, TEXT("MetaballsMesh"));
	m_mesh->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));

	RootComponent = m_mesh;
	MetaBallsBoundBox->SetupAttachment(RootComponent);
	CapsuleComp->SetupAttachment(RootComponent);


	m_Scale = 100.0f;
	m_NumBalls = 4;
	m_automode = true;
	m_GridStep = 32;
	m_randomseed = false;
	m_AutoLimitX = 1.0f;
	m_AutoLimitY = 1.0f;
	m_AutoLimitZ = 1.0f;
	
	m_Material = nullptr;


	m_pfGridEnergy = nullptr;
	m_pnGridPointStatus = nullptr;
	m_pnGridVoxelStatus = nullptr;



}

void AMetaballs::PostInitializeComponents()
{

	Super::PostInitializeComponents();


	m_fLevel = 100.0f;

	m_nGridSize = 0;

	m_nMaxOpenVoxels = MAX_OPEN_VOXELS;
	m_pOpenVoxels = new int[m_nMaxOpenVoxels * 3];

	m_nNumOpenVoxels = 0;
	m_pfGridEnergy = nullptr;
	m_pnGridPointStatus = nullptr;
	m_pnGridVoxelStatus = nullptr;

	m_nNumVertices = 0;
	m_nNumIndices = 0;

	InitBalls();

	CMarchingCubes::BuildTables();
	SetGridSize(m_GridStep);

//	m_SceneComponent->SetMobility(EComponentMobility::Movable);

	MetaBallsBoundBox->SetBoxExtent(FVector(m_Scale, m_Scale, m_Scale), false);
	MetaBallsBoundBox->UpdateBodySetup();
	
	m_mesh->SetMaterial(1, m_Material);
//	m_mesh->AttachParent = RootComponent;
	
//	m_mesh->SetMobility(EComponentMobility::Movable);


}


DEFINE_LOG_CATEGORY(YourLog);

#if WITH_EDITOR
void AMetaballs::PostEditChangeProperty(FPropertyChangedEvent& e)
{


	UE_LOG(YourLog, Warning, TEXT("changed respond"));

	const FName PropertyName = (e.Property != nullptr) ? e.Property->GetFName() : NAME_None;

	/// track Number of balls value
	if (PropertyName == GET_MEMBER_NAME_CHECKED(AMetaballs, m_NumBalls))
	{
		FIntProperty* prop = static_cast<FIntProperty*>(e.Property);

		const int32 value = prop->GetPropertyValue(prop->ContainerPtrToValuePtr<int32>(this));

		SetNumBalls(value);

		if (value < 0 && value > MAX_METABALLS)
		{
			prop->SetPropertyValue(prop->ContainerPtrToValuePtr<int32>(this), m_NumBalls);
		}

		UE_LOG(YourLog, Warning, TEXT("Num balls value: %d"), m_NumBalls);

	}



	/// track Scale value
	if (PropertyName == GET_MEMBER_NAME_CHECKED(AMetaballs, m_Scale))
	{

		FFloatProperty* prop = static_cast<FFloatProperty*>(e.Property);

		const float value = prop->GetPropertyValue(prop->ContainerPtrToValuePtr<float>(this));

		SetScale(value);

		if (value < MIN_SCALE)
		{
			prop->SetPropertyValue(prop->ContainerPtrToValuePtr<float>(this), m_Scale);
		}

		MetaBallsBoundBox->SetBoxExtent(FVector(m_Scale, m_Scale, m_Scale), false);
		MetaBallsBoundBox->UpdateBodySetup();

		UE_LOG(YourLog, Warning, TEXT("Scale value: %f"), m_Scale);

	}


	/// track Grid steps
	if (PropertyName == GET_MEMBER_NAME_CHECKED(AMetaballs, m_GridStep))
	{

		FIntProperty* prop = static_cast<FIntProperty*>(e.Property);

		const int32 value = prop->GetPropertyValue(prop->ContainerPtrToValuePtr<int32>(this));

		SetGridSteps(value);

		if (value < MIN_GRID_STEPS && value > MAX_GRID_STEPS)
		{
			prop->SetPropertyValue(prop->ContainerPtrToValuePtr<int32>(this), m_GridStep);
		}

		UE_LOG(YourLog, Warning, TEXT("Grid steps  value: %d"), m_GridStep);
	}


	/// track LimitX value
	if (PropertyName == GET_MEMBER_NAME_CHECKED(AMetaballs, m_AutoLimitX))
	{

		FFloatProperty* prop = static_cast<FFloatProperty*>(e.Property);

		const float value = prop->GetPropertyValue(prop->ContainerPtrToValuePtr<float>(this));

		SetAutoLimitX(value);

		if (value < MIN_LIMIT && value < MAX_LIMIT)
		{
			prop->SetPropertyValue(prop->ContainerPtrToValuePtr<float>(this), m_AutoLimitX);
		}

//		UE_LOG(YourLog, Warning, TEXT("LimitX value: %f"), m_AutoLimitX);

	}

	/// track LimitY value
	if (PropertyName == GET_MEMBER_NAME_CHECKED(AMetaballs, m_AutoLimitY))
	{

		FFloatProperty* prop = static_cast<FFloatProperty*>(e.Property);

		const float value = prop->GetPropertyValue(prop->ContainerPtrToValuePtr<float>(this));

		SetAutoLimitY(value);

		if (value < MIN_LIMIT && value < MAX_LIMIT)
		{
			prop->SetPropertyValue(prop->ContainerPtrToValuePtr<float>(this), m_AutoLimitY);
		}

				UE_LOG(YourLog, Warning, TEXT("LimitY value: %f"), m_AutoLimitY);

	}


	/// track LimitZ value
	if (PropertyName == GET_MEMBER_NAME_CHECKED(AMetaballs, m_AutoLimitZ))
	{

		FFloatProperty* prop = static_cast<FFloatProperty*>(e.Property);

		const float value = prop->GetPropertyValue(prop->ContainerPtrToValuePtr<float>(this));

		SetAutoLimitZ(value);

		if (value < MIN_LIMIT && value < MAX_LIMIT)
		{
			prop->SetPropertyValue(prop->ContainerPtrToValuePtr<float>(this), m_AutoLimitZ);
		}

		UE_LOG(YourLog, Warning, TEXT("LimitZ value: %f"), m_AutoLimitZ);

	}

	Super::PostEditChangeProperty(e);

}
#endif

// Called when the game starts or when spawned
void AMetaballs::BeginPlay()
{
	Super::BeginPlay();

}

// Called every frame
void AMetaballs::Tick( float DeltaTime )
{
	Super::Tick( DeltaTime );

	if (m_NumBalls > 0)
	{
		Update(DeltaTime);
		Render();
	}

}

void AMetaballs::Update(float dt)
{
	//SCOPE_CYCLE_COUNTER(STAT_MetaBallUpdate);

	if (!m_automode)
		return;

	for (int i = 0; i < m_NumBalls; i++)
	{
		m_Balls[i].p.X += dt*m_Balls[i].v.X;
		m_Balls[i].p.Y += dt*m_Balls[i].v.Y;
		m_Balls[i].p.Z += dt*m_Balls[i].v.Z;

		m_Balls[i].t -= dt;
		if (m_Balls[i].t < 0)
		{
			m_Balls[i].t = static_cast<float>(rand()) / RAND_MAX;

			m_Balls[i].a.X = m_AutoLimitY * (static_cast<float>(rand()) / RAND_MAX * 2 - 1);
			m_Balls[i].a.Y = m_AutoLimitX * (static_cast<float>(rand()) / RAND_MAX * 2 - 1);
			m_Balls[i].a.Z = m_AutoLimitZ * (static_cast<float>(rand()) / RAND_MAX * 2 - 1);

		}

		float x = m_Balls[i].a.X - m_Balls[i].p.X;
		float y = m_Balls[i].a.Y - m_Balls[i].p.Y;
		float z = m_Balls[i].a.Z - m_Balls[i].p.Z;
		float fDist = 1 / sqrtf(x*x + y*y + z*z);

		x *= fDist;
		y *= fDist;
		z *= fDist;

		m_Balls[i].v.X += 0.1f*x*dt;
		m_Balls[i].v.Y += 0.1f*y*dt;
		m_Balls[i].v.Z += 0.1f*z*dt;

		fDist = m_Balls[i].v.X * m_Balls[i].v.X +
			m_Balls[i].v.Y * m_Balls[i].v.Y +
			m_Balls[i].v.Z * m_Balls[i].v.Z;

		if (fDist > 0.040f)
		{
			fDist = 1 / sqrtf(fDist);
			m_Balls[i].v.X = 0.20f*m_Balls[i].v.X * fDist;
			m_Balls[i].v.Y = 0.20f*m_Balls[i].v.Y * fDist;
			m_Balls[i].v.Z = 0.20f*m_Balls[i].v.Z * fDist;
		}

		if (m_Balls[i].p.X < -m_AutoLimitY + m_fVoxelSize)
		{
			m_Balls[i].p.X = -m_AutoLimitY + m_fVoxelSize;
			m_Balls[i].v.X = 0;
		}
		if (m_Balls[i].p.X >  m_AutoLimitY - m_fVoxelSize)
		{
			m_Balls[i].p.X = m_AutoLimitY - m_fVoxelSize;
			m_Balls[i].v.X = 0;
		}

		if (m_Balls[i].p.Y < -m_AutoLimitX + m_fVoxelSize)
		{
			m_Balls[i].p.Y = -m_AutoLimitX + m_fVoxelSize;
			m_Balls[i].v.Y = 0;
		}


		if (m_Balls[i].p.Y >  m_AutoLimitX - m_fVoxelSize)
		{
			m_Balls[i].p.Y = m_AutoLimitX - m_fVoxelSize;
			m_Balls[i].v.Y = 0;
		}


		if (m_Balls[i].p.Z < -m_AutoLimitZ + m_fVoxelSize)
		{
			m_Balls[i].p.Z = -m_AutoLimitZ + m_fVoxelSize;
			m_Balls[i].v.Z = 0;
		}
		if (m_Balls[i].p.Z >  m_AutoLimitZ - m_fVoxelSize)
		{
			m_Balls[i].p.Z = m_AutoLimitZ - m_fVoxelSize;
			m_Balls[i].v.Z = 0;
		}
	}
}

void AMetaballs::Render()
{

	//SCOPE_CYCLE_COUNTER(STAT_MetaBallRender);
	
	m_vertices.Empty();
	m_Triangles.Empty();
	m_normals.Empty();
	m_UV0.Empty();
	m_tangents.Empty();

	m_mesh->ClearAllMeshSections();

	m_nNumIndices = 0;
	m_nNumVertices = 0;
	int nCase = 0;

	// Clear status grids
	//memset(m_pnGridPointStatus, 0, (m_nGridSize + 1)*(m_nGridSize + 1)*(m_nGridSize + 1));
	FMemory::Memset(m_pnGridPointStatus, 0, (m_nGridSize + 1)*(m_nGridSize + 1)*(m_nGridSize + 1));
	//memset(m_pnGridVoxelStatus, 0, m_nGridSize*m_nGridSize*m_nGridSize);
	FMemory::Memset(m_pnGridVoxelStatus, 0, m_nGridSize*m_nGridSize*m_nGridSize);

	for (int i = 0; i < m_NumBalls; i++)
	{
		int x = ConvertWorldCoordinateToGridPoint(m_Balls[i].p[0]);
		int y = ConvertWorldCoordinateToGridPoint(m_Balls[i].p[1]);
		int z = ConvertWorldCoordinateToGridPoint(m_Balls[i].p[2]);

		bool bComputed = false;

		// TODO: Check if bComputed can be used instead of constant
		while (true)
		{
			if (IsGridVoxelComputed(x, y, z))
			{
				bComputed = true;
				break;
			}

			nCase = ComputeGridVoxel(x, y, z);
			if (nCase < 255)
				break;

			z--;
		}

		if (bComputed)
			continue;

		AddNeighborsToList(nCase, x, y, z);

		while (m_nNumOpenVoxels)
		{
			m_nNumOpenVoxels--;
			x = m_pOpenVoxels[m_nNumOpenVoxels * 3];
			y = m_pOpenVoxels[m_nNumOpenVoxels * 3 + 1];
			z = m_pOpenVoxels[m_nNumOpenVoxels * 3 + 2];

			nCase = ComputeGridVoxel(x, y, z);

			AddNeighborsToList(nCase, x, y, z);
		}

	}

	m_mesh->CreateMeshSection(1, m_vertices, m_Triangles, m_normals, m_UV0, m_vertexColors, m_tangents, false);
}


void AMetaballs::ComputeNormal(const FVector Vertex)
{
	//SCOPE_CYCLE_COUNTER(STAT_MetaBallComputeNormal);
	FVector NVector = FVector::ZeroVector;

	for (int i = 0; i < m_NumBalls; i++)
	{
		FVector CalcVector = FVector(
		Vertex.X - m_Balls[i].p.Z,
		Vertex.Y - m_Balls[i].p.Y,
		Vertex.Z - m_Balls[i].p.X);

		//const float SquareDistVector = FVector::DistSquared(CalcVector, CalcVector * -1);
		const float SquareDistVector = FMath::Pow(FMath::Square(CalcVector.X) + FMath::Square(CalcVector.Y) + FMath::Square(CalcVector.Z), 2);
		
		
		NVector = NVector + 2 * m_Balls[i].m * CalcVector / (SquareDistVector);
	}

	NVector.Normalize();
	m_normals.Add(NVector);
	m_UV0.Add(FVector2D(NVector.X, NVector.Y));
}


void AMetaballs::AddNeighborsToList(int nCase, int x, int y, int z)
{
	//SCOPE_CYCLE_COUNTER(STAT_MetaBallAddNeighborToList);

	if (CMarchingCubes::m_CubeNeighbors[nCase] & (1 << 0))
		AddNeighbor(x + 1, y, z);

	if (CMarchingCubes::m_CubeNeighbors[nCase] & (1 << 1))
		AddNeighbor(x - 1, y, z);

	if (CMarchingCubes::m_CubeNeighbors[nCase] & (1 << 2))
		AddNeighbor(x, y + 1, z);

	if (CMarchingCubes::m_CubeNeighbors[nCase] & (1 << 3))
		AddNeighbor(x, y - 1, z);

	if (CMarchingCubes::m_CubeNeighbors[nCase] & (1 << 4))
		AddNeighbor(x, y, z + 1);

	if (CMarchingCubes::m_CubeNeighbors[nCase] & (1 << 5))
		AddNeighbor(x, y, z - 1);
}


void AMetaballs::AddNeighbor(int x, int y, int z)
{
	//SCOPE_CYCLE_COUNTER(STAT_MetaBallAddNeighbor);

	if (IsGridVoxelComputed(x, y, z) || IsGridVoxelInList(x, y, z))
		return;

	// Make sure the array is large enough
	if (m_nMaxOpenVoxels == m_nNumOpenVoxels)
	{
		m_nMaxOpenVoxels *= 2;
		int *pTmp = new int[m_nMaxOpenVoxels * 3];
		//memcpy(pTmp, m_pOpenVoxels, m_nNumOpenVoxels * 3 * sizeof(int));
		FMemory::Memcpy(pTmp, m_pOpenVoxels, m_nNumOpenVoxels * 3 * sizeof(int));
		delete[] m_pOpenVoxels;
		m_pOpenVoxels = pTmp;
	}
	
	m_pOpenVoxels[m_nNumOpenVoxels * 3] = x;
	m_pOpenVoxels[m_nNumOpenVoxels * 3 + 1] = y;
	m_pOpenVoxels[m_nNumOpenVoxels * 3 + 2] = z;

	SetGridVoxelInList(x, y, z);

	m_nNumOpenVoxels++;
}

float AMetaballs::ComputeEnergy(const float x, const float y, const float z) const
{
	//SCOPE_CYCLE_COUNTER(STAT_MetaBallComputeEnergy);

	float fEnergy = 0;

	for (int i = 0; i < m_NumBalls; i++)
	{
		// The formula for the energy is 
		// 
		//   e += mass/distance^2 

		float fSqDist = (m_Balls[i].p.X - x) * (m_Balls[i].p.X - x) +
			(m_Balls[i].p.Y - y) * (m_Balls[i].p.Y - y) +
			(m_Balls[i].p.Z - z) * (m_Balls[i].p.Z - z);

		if (fSqDist < 0.0001f) fSqDist = 0.0001f;

		fEnergy += m_Balls[i].m / fSqDist;
	}

	return fEnergy;
}


float AMetaballs::ComputeGridPointEnergy(const int x, const int y, const int z) const
{
	//SCOPE_CYCLE_COUNTER(STAT_MetaBallComputeGridpointEnergy);
	
	const int Index = GetIndex(x, y, z, m_nGridSize);
	
	if (IsGridPointComputed(x, y, z))
		return m_pfGridEnergy[Index];

	// The energy on the edges are always zero to make sure the isosurface is
	// always closed.
	if (x == 0 || y == 0 || z == 0 ||
		x == m_nGridSize || y == m_nGridSize || z == m_nGridSize)
	{
		m_pfGridEnergy[Index] = 0;
		SetGridPointComputed(x, y, z);
		return 0;
	}

	const float fx = ConvertGridPointToWorldCoordinate(x);
	const float fy = ConvertGridPointToWorldCoordinate(y);
	const float fz = ConvertGridPointToWorldCoordinate(z);

	m_pfGridEnergy[Index] = ComputeEnergy(fx, fy, fz);

	SetGridPointComputed(x, y, z);

	return m_pfGridEnergy[Index];
}


int AMetaballs::ComputeGridVoxel(int x, int y, int z)
{
	//SCOPE_CYCLE_COUNTER(STAT_MetaBallComputeGridVoxel);

	float b[8];

	b[0] = ComputeGridPointEnergy(x, y, z);
	b[1] = ComputeGridPointEnergy(x + 1, y, z);
	b[2] = ComputeGridPointEnergy(x + 1, y, z + 1);
	b[3] = ComputeGridPointEnergy(x, y, z + 1);
	b[4] = ComputeGridPointEnergy(x, y + 1, z);
	b[5] = ComputeGridPointEnergy(x + 1, y + 1, z);
	b[6] = ComputeGridPointEnergy(x + 1, y + 1, z + 1);
	b[7] = ComputeGridPointEnergy(x, y + 1, z + 1);

	int c = 0;
	c |= b[0] > m_fLevel ? (1 << 0) : 0;
	c |= b[1] > m_fLevel ? (1 << 1) : 0;
	c |= b[2] > m_fLevel ? (1 << 2) : 0;
	c |= b[3] > m_fLevel ? (1 << 3) : 0;
	c |= b[4] > m_fLevel ? (1 << 4) : 0;
	c |= b[5] > m_fLevel ? (1 << 5) : 0;
	c |= b[6] > m_fLevel ? (1 << 6) : 0;
	c |= b[7] > m_fLevel ? (1 << 7) : 0;

	
	const FVector PyramidVector = FVector(
		ConvertGridPointToWorldCoordinate(x),
		ConvertGridPointToWorldCoordinate(y),
		ConvertGridPointToWorldCoordinate(z));
		
	int i = 0;
	unsigned short EdgeIndices[12];
	//memset(EdgeIndices, 0xFF, 12 * sizeof(unsigned short));
	FMemory::Memset(EdgeIndices, 0xFF, 12 * sizeof(unsigned short));

	while (true)
	{
		const int nEdge = CMarchingCubes::m_CubeTriangles[c][i];
		if (nEdge == -1)
			break;

		if (EdgeIndices[nEdge] == 0xFFFF)
		{
			EdgeIndices[nEdge] = m_nNumVertices;

			// Compute the vertex by interpolating between the two points
			const int nIndex0 = CMarchingCubes::m_CubeEdges[nEdge][0];
			const int nIndex1 = CMarchingCubes::m_CubeEdges[nEdge][1];

			const float t = (m_fLevel - b[nIndex0]) / (b[nIndex1] - b[nIndex0]);

			FVector CubesVector = FVector(
			CMarchingCubes::m_CubeVertices[nIndex0][0] * (1 - t) + CMarchingCubes::m_CubeVertices[nIndex1][0] * t,
			CMarchingCubes::m_CubeVertices[nIndex0][1] * (1 - t) + CMarchingCubes::m_CubeVertices[nIndex1][1] * t,
			CMarchingCubes::m_CubeVertices[nIndex0][2] * (1 - t) + CMarchingCubes::m_CubeVertices[nIndex1][2] * t);

			FVector EdgeVector = PyramidVector + CubesVector * m_fVoxelSize;
			EdgeVector = FVector(EdgeVector.Z, EdgeVector.Y, EdgeVector.X);

			ComputeNormal(EdgeVector);

			m_vertices.Add(EdgeVector * m_Scale);

			m_nNumVertices++;
		}

		m_Triangles.Add(EdgeIndices[nEdge]);

		m_nNumIndices++;

		i++;
	}

	SetGridVoxelComputed(x, y, z);

	return c;

}

float AMetaballs::ConvertGridPointToWorldCoordinate(const int x) const
{
	return static_cast<float>(x) * m_fVoxelSize - 1.0f;
}

int AMetaballs::ConvertWorldCoordinateToGridPoint(const float x) const
{
	return static_cast<int>((x + 1.0f) / m_fVoxelSize + 0.5f);
}

void AMetaballs::SetGridSize(const int nSize)
{
	if (m_pfGridEnergy)
		delete m_pfGridEnergy;

	if (m_pnGridPointStatus)
		delete m_pnGridPointStatus;

	if (m_pnGridVoxelStatus)
		delete m_pnGridVoxelStatus;

	m_fVoxelSize = 2 / static_cast<float>(nSize);
	m_nGridSize = nSize;

	m_pfGridEnergy = new float[(nSize + 1)*(nSize + 1)*(nSize + 1)];
	m_pnGridPointStatus = new char[(nSize + 1)*(nSize + 1)*(nSize + 1)];
	m_pnGridVoxelStatus = new char[nSize*nSize*nSize];
}

inline bool AMetaballs::IsGridPointComputed(const int x, const int y, const int z) const
{
	return static_cast<bool>(m_pnGridPointStatus[GetIndex(x, y, z, m_nGridSize)] == 1);
}

inline bool AMetaballs::IsGridVoxelComputed(const int x, const int y, const int z) const
{
	 return static_cast<bool>(m_pnGridVoxelStatus[GetIndexNoAdd(x, y, z, m_nGridSize)] == 1);
}

inline bool AMetaballs::IsGridVoxelInList(const int x, const int y, const int z) const
{
	return static_cast<bool>(m_pnGridVoxelStatus[GetIndexNoAdd(x, y, z, m_nGridSize)] == 2);
}

inline void AMetaballs::SetGridPointComputed(const int x, const int y, const int z) const
{
	m_pnGridPointStatus[GetIndex(x, y, z, m_nGridSize)] = 1;
}

inline void AMetaballs::SetGridVoxelComputed(const int x, const int y, const int z) const
{
	m_pnGridVoxelStatus[GetIndexNoAdd(x, y, z, m_nGridSize)] = 1;
}

inline void AMetaballs::SetGridVoxelInList(const int x, const int y, const int z) const
{
	m_pnGridVoxelStatus[GetIndexNoAdd(x, y, z, m_nGridSize)] = 2;
}

inline FVector AMetaballs::ConvertGridPointToWorldCoordinate(const FVector& Vector) const
{
	return Vector * m_fVoxelSize - FVector::OneVector;
}

void AMetaballs::InitBalls()
{
	const FDateTime curTime;
	srand(curTime.GetTicks());

	for (int i = 0; i < MAX_METABALLS; i++)
	{
		float p0 = 0.0f;
		float p1 = 0.0f;
		float p2 = 0.0f;
		float v0 = 0.0f;
		float v1 = 0.0f;
		float v2 = 0.0f;

		if (m_randomseed)
		{
			p0 = m_AutoLimitY * (static_cast<float>(rand()) / RAND_MAX * 2 - 1);
			p1 = m_AutoLimitX * (static_cast<float>(rand()) / RAND_MAX * 2 - 1);
			p2 = m_AutoLimitZ * (static_cast<float>(rand()) / RAND_MAX * 2 - 1);

			v0 = (static_cast<float>(rand()) / RAND_MAX * 2 - 1) / 2;
			v1 = (static_cast<float>(rand()) / RAND_MAX * 2 - 1) / 2;
			v2 = (static_cast<float>(rand()) / RAND_MAX * 2 - 1) / 2;
		}

		m_Balls[i].p.X = p0;
		m_Balls[i].p.Y = p1;
		m_Balls[i].p.Z = p2;
		m_Balls[i].v.X = v0;
		m_Balls[i].v.Y = v1;
		m_Balls[i].v.Z = v2;
		m_Balls[i].a.X = m_AutoLimitY * (static_cast<float>(rand()) / RAND_MAX * 2 - 1);
		m_Balls[i].a.Y = m_AutoLimitX * (static_cast<float>(rand()) / RAND_MAX * 2 - 1);
		m_Balls[i].a.Z = m_AutoLimitZ * (static_cast<float>(rand()) / RAND_MAX * 2 - 1);
		m_Balls[i].t = static_cast<float>(rand()) / RAND_MAX;
		m_Balls[i].m = 1;
	}
}


void AMetaballs::SetBallTransform(const int32 Index, const FVector Transform)
{
	if (Index > m_NumBalls - 1)
	{
		return;
	}
	
	//SCOPE_CYCLE_COUNTER(STAT_MetaBallSetBallTransform);
	m_Balls[Index].p.Y = Transform.X;
	m_Balls[Index].p.X = Transform.Y;
	m_Balls[Index].p.Z = Transform.Z;
}

void AMetaballs::SetNumBalls(const int Value)
{
	m_NumBalls = FMath::Clamp<int32>(Value, 0, MAX_METABALLS);;
}


void AMetaballs::SetScale(float Value)
{
	float ret = Value;

	if (ret < MIN_SCALE)
	{
		ret = MIN_SCALE;
	}

	m_Scale = ret;
}


void AMetaballs::SetGridSteps(const int32 Value)
{
	m_GridStep = FMath::Clamp<int32>(Value, MIN_GRID_STEPS, MAX_GRID_STEPS);

	SetGridSize(m_GridStep);
}

void AMetaballs::SetRandomSeed(const bool bSeed)
{
	m_randomseed = bSeed;
}

void AMetaballs::SetAutoMode(const bool bMode)
{
	m_automode = bMode;
}

float AMetaballs::CheckLimit(const float Value) const
{
	return FMath::Clamp<float>(Value, MIN_LIMIT, MAX_LIMIT);
}

void AMetaballs::SetAutoLimitX(const float Limit)
{
	m_AutoLimitX = FMath::Clamp<float>(Limit, MIN_LIMIT, MAX_LIMIT);
}

void AMetaballs::SetAutoLimitY(const float Limit)
{
	m_AutoLimitY = FMath::Clamp<float>(Limit, MIN_LIMIT, MAX_LIMIT);
}

void AMetaballs::SetAutoLimitZ(const float Limit)
{
	m_AutoLimitZ = FMath::Clamp<float>(Limit, MIN_LIMIT, MAX_LIMIT);
}