// Fill out your copyright notice in the Description page of Project Settings.

#include "Pathfinder.h"
#include "GameField.h"
#include "Tile.h"

// pathfinding A*

TArray<ATile*> UPathfinder::FindPath(
    AGameField* Grid,
    ATile* Start,
    ATile* Goal)
{
    if (!Grid || !Start || !Goal)  // parametri obbligatori mancanti
        return {};

    // Goal non calpestabile, nessun path possibile
    
    if (!Goal->IsWalkable() && Goal != Start)
    {
        // Si permette comunque di cercare il percorso verso torri/nemici
        
    }

    // Struttura dati
    // Usiamo un TArray<FPathNode> come pool: nessun new/delete.
    // Gli indici nel pool rimpiazzano i puntatori Parent.

    TArray<FPathNode> NodePool;  // pool di nodi
    NodePool.Reserve(Grid->GridWidth * Grid->GridHeight);  // pre-allocazione

    // OpenSet e ClosedSet contengono indici nel NodePool
    TArray<int32> OpenSet;  // indici dei nodi ancora da espandere
    TSet<ATile*>  ClosedTiles;  // set di tiles già espanse

    // Nodo iniziale
    {
        FPathNode StartNode;
        StartNode.Tile = Start;  // tile iniziale
        StartNode.GCost = 0.f;  // costo zero per il nodo di partenza
        StartNode.HCost = Heuristic(Start, Goal);  // stima verso il Goal
        StartNode.Parent = -1;  // -1 = nodo radice (nessun padre)
        NodePool.Add(StartNode);
        OpenSet.Add(0);
    }

    while (OpenSet.Num() > 0)
    {
        // Trova il nodo con FCost minore nell'OpenSet
        int32 BestIdx = OpenSet[0];  // assume il primo come migliore
        int32 BestPos = 0;  // posizione nell OpenSet

        for (int32 i = 1; i < OpenSet.Num(); i++)
        {
            const FPathNode& A = NodePool[OpenSet[i]];
            const FPathNode& B = NodePool[BestIdx];

            // In caso di parità su FCost, preferisce HCost minore
            if (A.FCost() < B.FCost() ||  // A* con tiebreaking su HCost
                (A.FCost() == B.FCost() && A.HCost < B.HCost))
            {
                BestIdx = OpenSet[i];
                BestPos = i;
            }
        }

        OpenSet.RemoveAt(BestPos);  // rimuove il nodo migliore

        ATile* CurrentTile = NodePool[BestIdx].Tile;  // tile correntemente espansa

        // Goal raggiunto
        if (CurrentTile == Goal)  // raggiunto il Goal
        {
            return ReconstructPath(NodePool, BestIdx);  // ricostruisce il path dagli indici Parent
        }

        ClosedTiles.Add(CurrentTile);  // marca come espansa

        // Espande vicini
        for (ATile* NeighborTile : Grid->GetNeighbors(CurrentTile))
        {
            if (!NeighborTile) continue;

            // Salta celle non calpestabili (acqua, torri, unità) e se il Goal stesso possa essere la tile di un nemico
            // (Occupied ma non Obstacle); lo gestisce permettendo l'entrata.
            bool bIsGoal = (NeighborTile == Goal);  // il vicino è l'obiettivo?
            if (!bIsGoal && !NeighborTile->IsWalkable()) continue;  // salta tile non calpestabili (eccezione: obiettivo può essere Occupied)

            // Salta celle già espanse
            if (ClosedTiles.Contains(NeighborTile)) continue;  // già espansa: salta

            float TentativeG =  // costo tentativo per raggiungere il vicino
                NodePool[BestIdx].GCost +
                MovementCost(CurrentTile, NeighborTile);

            // Cerca se NeighborTile ha già un nodo nell'OpenSet
            int32 ExistingIdx = -1;  // controlla se esiste gia un nodo per questo vicino
            for (int32 OIdx : OpenSet)
            {
                if (NodePool[OIdx].Tile == NeighborTile)
                {
                    ExistingIdx = OIdx;
                    break;
                }
            }

            if (ExistingIdx == -1)
            {
                // Nuovo nodo
                FPathNode NewNode;
                NewNode.Tile = NeighborTile;
                NewNode.GCost = TentativeG;
                NewNode.HCost = Heuristic(NeighborTile, Goal);
                NewNode.Parent = BestIdx;

                int32 NewIdx = NodePool.Add(NewNode);
                OpenSet.Add(NewIdx);
            }
            else if (TentativeG < NodePool[ExistingIdx].GCost)
            {
                // Path migliore trovato per questo nodo
                NodePool[ExistingIdx].GCost = TentativeG;
                NodePool[ExistingIdx].Parent = BestIdx;  // nuovo padre (percorso più corto)
            }
        }
    }

    // Nessun percorso trovato
    return {};
}

//Range di movimento

TArray<ATile*> UPathfinder::GetReachableTiles(
    AGameField* Grid,
    ATile* Start,
    int32       MoveRange)
{
    TArray<ATile*> Result;
    if (!Grid || !Start) return Result;

    // Dijkstra:
    // TMap<tile, costo_minimo_fin'ora>
    TMap<ATile*, float> CostSoFar;  // costo minimo fin'ora per ogni tile
    TArray<ATile*>      Frontier;  // coda di tile da espandere

    CostSoFar.Add(Start, 0.f);  // costo 0 per la tile di partenza
    Frontier.Add(Start);  // inizia dal punto di partenza

    while (Frontier.Num() > 0)
    {
        // Prende la tile con costo minore (mini-priority queue)
        int32  MinIdx = 0;  // indice della tile con costo minore
        float  MinCost = CostSoFar[Frontier[0]];  // costo minimo attuale

        for (int32 i = 1; i < Frontier.Num(); i++)
        {
            float C = CostSoFar[Frontier[i]];
            if (C < MinCost)
            {
                MinCost = C;
                MinIdx = i;
            }
        }

        ATile* Current = Frontier[MinIdx];  // tile con costo minore
        Frontier.RemoveAt(MinIdx);  // rimuove dalla frontiera

        for (ATile* Neighbor : Grid->GetNeighbors(Current))
        {
            if (!Neighbor || !Neighbor->IsWalkable()) continue;  // non valida o non calpestabile: salta

            float NewCost = CostSoFar[Current] + MovementCost(Current, Neighbor);  // costo cumulativo

            if (NewCost > static_cast<float>(MoveRange)) continue;  // supera il budget: non raggiungibile

            if (!CostSoFar.Contains(Neighbor) || NewCost < CostSoFar[Neighbor])  // costo migliore?
            {
                CostSoFar.Add(Neighbor, NewCost);
                // Aggiunge alla frontiera solo se non già presente,
                // per evitare di processare la stessa tile più volte.
                if (!Frontier.Contains(Neighbor))  // evita duplicati nella frontiera
                    Frontier.Add(Neighbor);
            }
        }
    }

    // Raccogle tutte le tiles raggiungibili (esclusa la Starting)
    for (auto& Pair : CostSoFar)
    {
        if (Pair.Key != Start)
        {
            Result.Add(Pair.Key);
        }
    }

    return Result;
}

//Range di attacco

TArray<ATile*> UPathfinder::GetAttackableTiles(
    AGameField* Grid,
    ATile* Start,
    int32       AttackRange,
    bool        bRanged,
    int32       AttackerElev)
{
    TArray<ATile*> Result;
    if (!Grid || !Start) return Result;

    if (bRanged)
    {
        // Attacco a distanza:
        // Distanza Manhattan minore o uguale di AttackRange
        // Ignora ostacoli (attraversa tutto)
        // Può colpire solo tile allo stesso livello o inferiore
        for (int32 DY = -AttackRange; DY <= AttackRange; DY++)
        {
            for (int32 DX = -AttackRange; DX <= AttackRange; DX++)
            {
                if (DX == 0 && DY == 0) continue;  // salta la tile dell attaccante

                int32 Dist = FMath::Abs(DX) + FMath::Abs(DY);  // distanza Manhattan
                if (Dist > AttackRange) continue;  // fuori dal range circolare: salta

                ATile* Tile = Grid->GetTile(Start->X + DX, Start->Y + DY);
                if (!Tile) continue;

                // Può colpire solo unità allo stesso livello o inferiore
                if (Tile->Elevation > AttackerElev) continue;  // terreno piu alto: non attaccabile

                Result.Add(Tile);
            }
        }
    }
    else
    {
        // Attacco corpo a corpo (Brawler):
        // Tutte le celle nel raggio 1 di Chebyshev (4 ortogonali + 4 diagonali)
        // Può colpire solo tile allo stesso livello o inferiore
        for (int32 DY = -1; DY <= 1; DY++)
        {
            for (int32 DX = -1; DX <= 1; DX++)
            {
                if (DX == 0 && DY == 0) continue; // salta la tile dell'attaccante

                ATile* Tile = Grid->GetTile(Start->X + DX, Start->Y + DY);
                if (!Tile) continue;                               // fuori griglia
                if (Tile->Elevation > AttackerElev) continue;     // terreno più alto: non attaccabile

                Result.Add(Tile);
            }
        }
    }

    return Result;
}

//Distanza

int32 UPathfinder::ManhattanDistance(ATile* A, ATile* B)
{
    if (!A || !B) return INT32_MAX;  // parametri non validi: distanza massima

    return FMath::Abs(A->X - B->X) + FMath::Abs(A->Y - B->Y);  // distanza Manhattan: |DX| + |DY|
}

//Utility

float UPathfinder::Heuristic(ATile* A, ATile* B)
{
    // Distanza Manhattan: ammissibile per griglia ortogonale
    return static_cast<float>(  // distanza Manhattan
        FMath::Abs(A->X - B->X) +
        FMath::Abs(A->Y - B->Y)
        );
}

float UPathfinder::MovementCost(ATile* From, ATile* To)
{
    //   piano (stesso livello): 1
    //   discesa (livello più basso): 1
    //   salita  (livello più alto): 2
    int32 ElevDiff = To->Elevation - From->Elevation;  // differenza di elevazione

    return (ElevDiff > 0) ? 2.f : 1.f;  // salita = costo 2, piano/discesa = costo 1
}

TArray<ATile*> UPathfinder::ReconstructPath(
    const TArray<FPathNode>& Nodes,
    int32                    GoalIdx)
{
    TArray<ATile*> Path;

    int32 Current = GoalIdx;

    while (Current != -1)
    {
        Path.Insert(Nodes[Current].Tile, 0);  // inserisce in testa per ottenere Start->Goal
        Current = Nodes[Current].Parent;  // risale la catena di indici Parent
    }

    return Path;
}