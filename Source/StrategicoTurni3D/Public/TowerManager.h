// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Tile.h"
#include "TowerManager.generated.h"

class ATile;
class AUnit;
class AGameField;


// Stato di controllo di una singola torre, valutato alla fine di ogni turno


USTRUCT(BlueprintType)
struct FTowerControlState
{
    GENERATED_BODY()

    UPROPERTY()
    ATile* Tower = nullptr; // riferimento alla tile fisica della torre

    // Chi controlla la torre al termine dell'ultimo turno valutato: -1 = Neutral, 0 = Player, 1 = AI
    
    UPROPERTY()
    int32 ControlledBy = -1; // -1=Neutral, 0=Player, 1=AI

    
    UPROPERTY()
    int32 ConsecutiveTurns = 0; // turni consecutivi sotto lo stesso controllo
};

UCLASS()
class STRATEGICOTURNI3D_API ATowerManager : public AActor
{
    GENERATED_BODY()

public:

    ATowerManager();

protected:

    virtual void BeginPlay() override;

public:


    // Griglia di gioco
    UPROPERTY()
    AGameField* Grid;

   
    UPROPERTY()
    TArray<FTowerControlState> TowerStates; // Stato di controllo per ciascuna torre


    //registra torre nel sistema
    void RegisterTower(ATile* TowerTile);

    // Valuta lo stato delle torri
    void EvaluateTowers(
        const TArray<AUnit*>& PlayerUnits,
        const TArray<AUnit*>& AIUnits
    );

    
    bool CheckVictory(int32& OutWinner) const; // controlla condizione di vittoria

    
    int32 CountTowersControlledBy(int32 PlayerID) const; // conta numero di torri controllate da player ed Ai

    
    void GetScore(int32& OutPlayer, int32& OutAI) const; // rutorna il punteggio attuale

private:

    // Raggio della zona di cattura
    static constexpr int32 CAPTURE_RADIUS = 2; 

    
    static constexpr int32 WIN_TURNS = 2; // turni consecutivi necessari per vincere (da specifiche)

    // conta le unità nella zona di cattura
    int32 CountUnitsInCaptureZone(
        ATile* TowerTile,
        const TArray<AUnit*>& Units
    ) const;
};