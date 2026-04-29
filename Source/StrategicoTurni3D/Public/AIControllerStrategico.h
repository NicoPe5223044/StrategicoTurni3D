// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "AIControllerStrategico.generated.h"

// ─────────────────────────────────────────────────────────────
// Obiettivo scelto dall'AI per una singola unità
// ─────────────────────────────────────────────────────────────
// Obiettivo dell'AI per un singolo turno di un'unità
UENUM()
enum class EAIGoal : uint8
{
    AttackEnemy,    // L'unità attacca un nemico se possibile
    MoveToEnemy,    // L'unità si muove verso un nemico
    CaptureTower,   // L'unità punta a catturare una torre
    Idle            // nessuna azione utile disponibile
};

UCLASS()
class STRATEGICOTURNI3D_API UAIControllerStrategico : public UObject
{
    GENERATED_BODY()

public:
    void ExecuteTurn(
        class ATurnManager* TurnMgr,  // Riferimento a TurnManager
        class AGameField* Grid,  // a GameField
        class ATowerManager* TowerMgr,  // a TowerManager

        const TArray<class AUnit*>& AIUnits,  // unità del giocatore
        const TArray<class AUnit*>& EnemyUnits  //dell'avversario
    );

private:
    // decidere un obiettivo da assegnare alla unità nel turno
    EAIGoal ChooseGoal(
        class AUnit* Unit,   // Unità AI
        class AGameField* Grid,  // Accesso alla griglia
        class ATowerManager* TowerMgr,  // Accesso allo stato delle torri
        const TArray<class AUnit*>& EnemyUnits, // Lista nemici per valutazioni
        class AUnit*& OutTargetEnemy, // Output: nemico selezionato
        class ATile*& OutTargetTile  // Output: tile obiettivo (che può essere una torre o posizione nemica)
    ) const;

    // Cerca un nemico attaccabile
    class AUnit* FindAttackableEnemy(
        class AUnit* Attacker, // Unità che vuole attaccare
        class AGameField* Grid, // Per calcolare range/posizione
        const TArray<class AUnit*>& EnemyUnits // Lista dei possibili target
    ) const;
    // Controlla se il target è nel range di attacco
    bool IsInAttackRange(class AUnit* Attacker, class AUnit* Target) const;

    // Trova il nemico più vicino
    class AUnit* FindNearestEnemy(
        class AUnit* Unit, // Unità AI
        const TArray<class AUnit*>& EnemyUnits  // Lista nemici
    ) const;

    // Trova la torre più conveniente da catturare
    class ATile* FindBestTowerTarget(
        class AUnit* Unit, // Unità AI
        class AGameField* Grid, // Griglia per posizione torri
        class ATowerManager* TowerMgr // Stato della torre 
    ) const;

    // Calcola un path verso una tile obiettivo
    TArray<class ATile*> ComputeApproachPath(
        class AUnit* Unit, // Unità che si muove
        class ATile* GoalTile, // Destinazione
        class AGameField* Grid 
    ) const;

    // Trova una tile adiacente camminabile vicino al target
    class ATile* FindNearestWalkableTileAdjacentTo(
        class AUnit* Unit, // Unità che deve raggiungere il target
        class ATile* Target, // Target (nemico o torre)
        class AGameField* Grid // Per controllare la "percorribilità" della tile
    ) const;
};