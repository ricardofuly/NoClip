// Copyright Fuly Games. All Rights Reserved.

#include "Enemy/NoclipNavGrid.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "WorldCollision.h"
#include "DrawDebugHelpers.h"
#include "Algo/Reverse.h"
#include "NOCLIP.h"

namespace
{
	constexpr int32 MaxExpansions = 60000;
	const FIntPoint Neighbors[8] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1} };
	const int32 NeighborCost[8] = { 10, 10, 10, 10, 14, 14, 14, 14 };

	int32 Octile(const FIntPoint& A, const FIntPoint& B)
	{
		const int32 Dx = FMath::Abs(A.X - B.X);
		const int32 Dy = FMath::Abs(A.Y - B.Y);
		return 10 * FMath::Max(Dx, Dy) + 4 * FMath::Min(Dx, Dy);
	}
}

void FNoclipNavGrid::Reset()
{
	Width = Height = 0;
	Clear.Reset();
	GScore.Reset();
	Parent.Reset();
	Visited.Reset();
}

void FNoclipNavGrid::Build(UWorld* World, const FBox2D& Bounds, float InFloorZ, float InCellSize, const TArray<FBox2D>& Forbidden)
{
	Reset();
	if (!World || !Bounds.bIsValid)
	{
		return;
	}
	const double StartTime = FPlatformTime::Seconds();

	CellSize = FMath::Max(InCellSize, 20.f);
	FloorZ = InFloorZ;
	Origin = Bounds.Min;
	const FVector2D Size = Bounds.GetSize();
	Width = FMath::Clamp(FMath::CeilToInt(Size.X / CellSize), 1, 1024);
	Height = FMath::Clamp(FMath::CeilToInt(Size.Y / CellSize), 1, 1024);
	const int32 Num = Width * Height;

	// 1) Bloqueio: caixa do tamanho da celula, do joelho a cabeca (o chao e o teto nao contam).
	TArray<bool> Blocked;
	Blocked.SetNumZeroed(Num);
	const FCollisionObjectQueryParams Objects(ECC_WorldStatic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NoclipNavGrid), false);
	const float BoxBottom = 40.f, BoxTop = 170.f;
	const FCollisionShape Box = FCollisionShape::MakeBox(FVector(CellSize * 0.5f - 1.f, CellSize * 0.5f - 1.f, (BoxTop - BoxBottom) * 0.5f));
	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			const FVector C = CellCenter(X, Y) + FVector(0.f, 0.f, (BoxBottom + BoxTop) * 0.5f);
			bool bBlock = World->OverlapAnyTestByObjectType(C, FQuat::Identity, Objects, Box, Params);
			if (!bBlock)
			{
				const FVector2D P(C.X, C.Y);
				for (const FBox2D& F : Forbidden)
				{
					if (F.bIsValid && F.IsInside(P))
					{
						bBlock = true;
						break;
					}
				}
			}
			Blocked[Index(X, Y)] = bBlock;
		}
	}

	// 2) Folga: distancia (xadrez) ate a celula bloqueada ou a borda mais proxima.
	Clear.SetNumUninitialized(Num);
	TArray<int32> Queue;
	Queue.Reserve(Num);
	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			const int32 I = Index(X, Y);
			const bool bEdge = X == 0 || Y == 0 || X == Width - 1 || Y == Height - 1;
			if (Blocked[I])
			{
				Clear[I] = 0;
				Queue.Add(I);
			}
			else
			{
				Clear[I] = bEdge ? 1 : 255;
				if (bEdge)
				{
					Queue.Add(I);
				}
			}
		}
	}
	for (int32 Head = 0; Head < Queue.Num(); ++Head)
	{
		const int32 I = Queue[Head];
		const int32 X = I % Width, Y = I / Width;
		const uint8 Next = static_cast<uint8>(FMath::Min<int32>(Clear[I] + 1, 254));
		for (const FIntPoint& N : Neighbors)
		{
			const int32 NX = X + N.X, NY = Y + N.Y;
			if (!InGrid(NX, NY))
			{
				continue;
			}
			const int32 NI = Index(NX, NY);
			if (Clear[NI] > Next)
			{
				Clear[NI] = Next;
				Queue.Add(NI);
			}
		}
	}

	GScore.SetNumUninitialized(Num);
	Parent.SetNumUninitialized(Num);
	Visited.SetNumZeroed(Num);
	SearchStamp = 0;

	UE_LOG(LogNoclip, Log, TEXT("[Entidades] Grade de navegacao %dx%d (%.0f cm), %d celulas livres, %.1f ms"),
		Width, Height, CellSize, NumWalkable(0.f), (FPlatformTime::Seconds() - StartTime) * 1000.0);
}

int32 FNoclipNavGrid::NumWalkable(float Clearance) const
{
	const uint8 Need = RequiredClearance(Clearance);
	int32 Count = 0;
	for (const uint8 C : Clear)
	{
		Count += C >= Need ? 1 : 0;
	}
	return Count;
}

FIntPoint FNoclipNavGrid::ToCell(const FVector2D& P) const
{
	return FIntPoint(FMath::FloorToInt((P.X - Origin.X) / CellSize), FMath::FloorToInt((P.Y - Origin.Y) / CellSize));
}

FVector FNoclipNavGrid::CellCenter(int32 X, int32 Y) const
{
	return FVector(Origin.X + (X + 0.5f) * CellSize, Origin.Y + (Y + 0.5f) * CellSize, FloorZ);
}

uint8 FNoclipNavGrid::RequiredClearance(float Clearance) const
{
	// Celula com folga N: a parede mais proxima esta a ~(N - 0.5) celulas do centro.
	return static_cast<uint8>(FMath::Clamp(FMath::CeilToInt(Clearance / CellSize + 0.5f), 1, 250));
}

bool FNoclipNavGrid::IsWalkable(const FVector& Location, float Clearance) const
{
	if (!IsValid())
	{
		return false;
	}
	const FIntPoint C = ToCell(FVector2D(Location));
	return CellOk(C.X, C.Y, RequiredClearance(Clearance));
}

bool FNoclipNavGrid::FindNearestCell(const FIntPoint& Start, uint8 Need, int32 MaxRadius, FIntPoint& Out) const
{
	if (CellOk(Start.X, Start.Y, Need))
	{
		Out = Start;
		return true;
	}
	for (int32 R = 1; R <= MaxRadius; ++R)
	{
		int32 BestD = MAX_int32;
		for (int32 Dy = -R; Dy <= R; ++Dy)
		{
			for (int32 Dx = -R; Dx <= R; ++Dx)
			{
				if (FMath::Max(FMath::Abs(Dx), FMath::Abs(Dy)) != R)
				{
					continue;
				}
				const int32 X = Start.X + Dx, Y = Start.Y + Dy;
				if (CellOk(X, Y, Need))
				{
					const int32 D = Dx * Dx + Dy * Dy;
					if (D < BestD)
					{
						BestD = D;
						Out = FIntPoint(X, Y);
					}
				}
			}
		}
		if (BestD != MAX_int32)
		{
			return true;
		}
	}
	return false;
}

bool FNoclipNavGrid::ProjectToWalkable(const FVector& Location, float Clearance, FVector& OutLocation) const
{
	if (!IsValid())
	{
		return false;
	}
	FIntPoint Cell;
	if (!FindNearestCell(ToCell(FVector2D(Location)), RequiredClearance(Clearance), FMath::CeilToInt(500.f / CellSize), Cell))
	{
		return false;
	}
	const FIntPoint Start = ToCell(FVector2D(Location));
	OutLocation = Cell == Start ? FVector(Location.X, Location.Y, FloorZ) : CellCenter(Cell.X, Cell.Y);
	return true;
}

bool FNoclipNavGrid::LineOk(const FIntPoint& A, const FIntPoint& B, uint8 Need) const
{
	// Amostra a cada meia celula (supercobertura simples).
	const FVector2D PA(A.X + 0.5f, A.Y + 0.5f), PB(B.X + 0.5f, B.Y + 0.5f);
	const float Len = FVector2D::Distance(PA, PB);
	const int32 Steps = FMath::Max(1, FMath::CeilToInt(Len * 2.f));
	for (int32 i = 0; i <= Steps; ++i)
	{
		const FVector2D P = FMath::Lerp(PA, PB, static_cast<float>(i) / Steps);
		if (!CellOk(FMath::FloorToInt(P.X), FMath::FloorToInt(P.Y), Need))
		{
			return false;
		}
	}
	return true;
}

bool FNoclipNavGrid::HasStraightPath(const FVector& From, const FVector& To, float Clearance) const
{
	if (!IsValid())
	{
		return false;
	}
	return LineOk(ToCell(FVector2D(From)), ToCell(FVector2D(To)), RequiredClearance(Clearance));
}

bool FNoclipNavGrid::FindPath(const FVector& From, const FVector& To, float Clearance, TArray<FVector>& OutPoints) const
{
	OutPoints.Reset();
	if (!IsValid())
	{
		return false;
	}
	const uint8 Need = RequiredClearance(Clearance);
	FIntPoint Start, Goal;
	const int32 SnapRadius = FMath::CeilToInt(400.f / CellSize);
	if (!FindNearestCell(ToCell(FVector2D(From)), Need, SnapRadius, Start) || !FindNearestCell(ToCell(FVector2D(To)), Need, SnapRadius * 2, Goal))
	{
		return false;
	}
	const FVector GoalWorld = Goal == ToCell(FVector2D(To)) ? FVector(To.X, To.Y, FloorZ) : CellCenter(Goal.X, Goal.Y);
	if (Start == Goal || LineOk(Start, Goal, Need))
	{
		OutPoints.Add(GoalWorld);
		return true;
	}

	if (++SearchStamp == 0)
	{
		FMemory::Memzero(Visited.GetData(), Visited.Num() * sizeof(uint32));
		SearchStamp = 1;
	}

	struct FOpen
	{
		int32 F;
		int32 Cell;
	};
	TArray<FOpen> Open;
	Open.Reserve(1024);
	auto Less = [](const FOpen& A, const FOpen& B) { return A.F < B.F; };

	const int32 StartI = Index(Start.X, Start.Y);
	const int32 GoalI = Index(Goal.X, Goal.Y);
	Visited[StartI] = SearchStamp;
	GScore[StartI] = 0;
	Parent[StartI] = INDEX_NONE;
	Open.HeapPush({ Octile(Start, Goal), StartI }, Less);

	bool bFound = false;
	int32 Expansions = 0;
	while (Open.Num() > 0 && Expansions < MaxExpansions)
	{
		FOpen Cur;
		Open.HeapPop(Cur, Less, EAllowShrinking::No);
		if (Cur.Cell == GoalI)
		{
			bFound = true;
			break;
		}
		++Expansions;
		const int32 CX = Cur.Cell % Width, CY = Cur.Cell / Width;
		const int32 CurG = GScore[Cur.Cell];
		if (Cur.F - Octile(FIntPoint(CX, CY), Goal) > CurG)
		{
			continue; // entrada velha
		}
		for (int32 n = 0; n < 8; ++n)
		{
			const int32 NX = CX + Neighbors[n].X, NY = CY + Neighbors[n].Y;
			if (!CellOk(NX, NY, Need))
			{
				continue;
			}
			// Diagonal nao corta quina.
			if (n >= 4 && (!CellOk(CX + Neighbors[n].X, CY, Need) || !CellOk(CX, CY + Neighbors[n].Y, Need)))
			{
				continue;
			}
			const int32 NI = Index(NX, NY);
			// Prefere o meio do corredor: celula colada na parede custa um pouco mais.
			const int32 WallPenalty = Clear[NI] <= Need ? 6 : 0;
			const int32 G = CurG + NeighborCost[n] + WallPenalty;
			if (Visited[NI] == SearchStamp && GScore[NI] <= G)
			{
				continue;
			}
			Visited[NI] = SearchStamp;
			GScore[NI] = G;
			Parent[NI] = Cur.Cell;
			Open.HeapPush({ G + Octile(FIntPoint(NX, NY), Goal), NI }, Less);
		}
	}
	if (!bFound)
	{
		return false;
	}

	// Celulas do fim para o comeco.
	TArray<FIntPoint> Cells;
	for (int32 I = GoalI; I != INDEX_NONE; I = Parent[I])
	{
		Cells.Add(FIntPoint(I % Width, I / Width));
		if (I == StartI)
		{
			break;
		}
	}
	Algo::Reverse(Cells);

	// Puxa o fio: do ponto atual, pula para a celula mais distante ainda em linha reta.
	int32 Anchor = 0;
	while (Anchor < Cells.Num() - 1)
	{
		int32 Best = Anchor + 1;
		for (int32 j = Cells.Num() - 1; j > Anchor + 1; --j)
		{
			if (LineOk(Cells[Anchor], Cells[j], Need))
			{
				Best = j;
				break;
			}
		}
		Anchor = Best;
		OutPoints.Add(Anchor == Cells.Num() - 1 ? GoalWorld : CellCenter(Cells[Anchor].X, Cells[Anchor].Y));
	}
	return OutPoints.Num() > 0;
}

bool FNoclipNavGrid::RandomWalkable(FRandomStream& Stream, float Clearance, const FVector& Around, float MinDistance, float MaxDistance, FVector& OutLocation) const
{
	if (!IsValid())
	{
		return false;
	}
	const uint8 Need = RequiredClearance(Clearance);
	for (int32 Attempt = 0; Attempt < 48; ++Attempt)
	{
		FIntPoint C;
		if (MaxDistance > 0.f)
		{
			const float Angle = Stream.FRandRange(0.f, 2.f * PI);
			const float Dist = Stream.FRandRange(MinDistance, MaxDistance);
			C = ToCell(FVector2D(Around) + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Dist);
		}
		else
		{
			C = FIntPoint(Stream.RandRange(0, Width - 1), Stream.RandRange(0, Height - 1));
		}
		if (CellOk(C.X, C.Y, Need))
		{
			const FVector P = CellCenter(C.X, C.Y);
			if (MaxDistance <= 0.f && MinDistance > 0.f && FVector::Dist2D(P, Around) < MinDistance)
			{
				continue;
			}
			OutLocation = P;
			return true;
		}
	}
	return false;
}

void FNoclipNavGrid::DebugDraw(UWorld* World, const FVector& Around, float Radius, float Clearance) const
{
	if (!World || !IsValid())
	{
		return;
	}
	const uint8 Need = RequiredClearance(Clearance);
	const FIntPoint Min = ToCell(FVector2D(Around) - FVector2D(Radius, Radius));
	const FIntPoint Max = ToCell(FVector2D(Around) + FVector2D(Radius, Radius));
	for (int32 Y = FMath::Max(Min.Y, 0); Y <= FMath::Min(Max.Y, Height - 1); ++Y)
	{
		for (int32 X = FMath::Max(Min.X, 0); X <= FMath::Min(Max.X, Width - 1); ++X)
		{
			if (Clear[Index(X, Y)] < Need)
			{
				DrawDebugPoint(World, CellCenter(X, Y) + FVector(0, 0, 3), 6.f, Clear[Index(X, Y)] == 0 ? FColor::Red : FColor::Orange, false, 0.25f);
			}
		}
	}
}
