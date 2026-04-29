// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Pathfinder.generated.h"

class AGameField;
class ATile;

// Struttura nodo usata internamente da A*.
// Ogni nodo rappresenta una tile nel grafo.
struct FPathNode
{
    ATile* Tile = nullptr; // Tile associata al nodo
    float  GCost = 0.f; // Costo dal nodo iniziale a questo nodo
    float  HCost = 0.f; // Stima fino all'obiettivo
    int32  Parent = -1; // indice del nodo padre nel pool, -1 = nodo radice

    // Costo totale usato da A*
    float FCost() const { return GCost + HCost; }
};

UCLASS()
class STRATEGICOTURNI3D_API UPathfinder : public UObject
{
    GENERATED_BODY()

public:

    // Pathfinding

   //Calcola un percorso tra Start e Goal usando A* e tiene conto della "percorribilità" e dei costi di movimento.
    static TArray<ATile*> FindPath(
        AGameField* Grid, // Griglia
        ATile* Start, // Posizione iniziale
        ATile* Goal // Budget massimo di movimento
    );

    // Range di Movimento

    // Restituisce tutte le tiles raggiungibili dato un budget di movimento.
    static TArray<ATile*> GetReachableTiles(
        AGameField* Grid, // Griglia
        ATile* Start,  // Posizione iniziale
        int32       MoveRange // "Budget" totale di movimento dell'unità
    );

    // Range di Attacco

    // Calcola tutte le tiles attaccabili da una posizione.
    static TArray<ATile*> GetAttackableTiles(
        AGameField* Grid, // griglia
        ATile* Start, // posizione iniziale
        int32       AttackRange, //range massimo
        bool        bRanged, // True = attacco a distanza dello sniper
        int32       AttackerElev // Elevazione attaccante
    );

    //Distanza tra due tiles per range di attacco e contrattacco
     
    static int32 ManhattanDistance(ATile* A, ATile* B);

private:
    // Funzioni interne A*
    //Distanza A e B
    static float Heuristic(ATile* A, ATile* B);

    // costo movimento tra tile adiacenti
    static float MovementCost(ATile* From, ATile* To);

   //ricostruisce il percorso partendo dall'obiettivo
    static TArray<ATile*> ReconstructPath(
        const TArray<FPathNode>& Nodes, // Tutti i nodi generati
        int32                    GoalIdx // Indice del nodo goal
    );
};

