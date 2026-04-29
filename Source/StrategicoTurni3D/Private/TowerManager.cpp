// Fill out your copyright notice in the Description page of Project Settings.

#include "TowerManager.h"
#include "Tile.h"  
#include "Unit.h"
#include "GameField.h"

ATowerManager::ATowerManager()
{
    PrimaryActorTick.bCanEverTick = false; // nessun tick
    Grid = nullptr; 
}

void ATowerManager::BeginPlay()
{
    Super::BeginPlay(); 
}

// Setup

void ATowerManager::RegisterTower(ATile* TowerTile)
{
    if (!TowerTile) return; // tile non valida: ignora

    FTowerControlState State;       // crea un nuovo record di stato
    State.Tower          = TowerTile; // riferimento alla tile fisica della torre
    State.ControlledBy   = -1;        // -1 = Neutral (nessun proprietario)
    State.ConsecutiveTurns = 0;       // nessun turno consecutivo accumulato

    TowerStates.Add(State); // aggiunge il record all'array (popolato da GameMode::RegisterTowersFromField)

    UE_LOG(LogTemp, Warning, TEXT("[TowerManager] Torre registrata in (%d,%d)"),
        TowerTile->X, TowerTile->Y);
}

// Valutazione torri

void ATowerManager::EvaluateTowers(const TArray<AUnit*>& PlayerUnits, const TArray<AUnit*>& AIUnits)
{
    if (!Grid) // senza la griglia non si può calcolare la zona di cattura
    {
        UE_LOG(LogTemp, Error, TEXT("[TowerManager] Grid non impostata!"));
        return;
    }

    for (FTowerControlState& State : TowerStates) // itera ogni torre registrata
    {
        if (!State.Tower) continue; // puntatore null: salta

        // Conta quante unità di ciascun giocatore sono nella zona di cattura
        int32 NPlayer = CountUnitsInCaptureZone(State.Tower, PlayerUnits);  // conta unita Player
        int32 NAI     = CountUnitsInCaptureZone(State.Tower, AIUnits);  // conta unita AI

        // Default: mantieni lo stato precedente (zona vuota = nessun cambiamento proprietario)
        // La torre rimane del proprietario finché un'unità avversaria entra nella zona di cattura
        ETowerStatus NewStatus = State.Tower->TowerStatus; // stato attuale come default
        int32        NewOwner  = State.ControlledBy;        // proprietario attuale come default

        if (NPlayer > 0 && NAI > 0)      // entrambi i giocatori nella zona?
        {
            NewStatus = ETowerStatus::Contested; // contesa: nessuno guadagna punti
            NewOwner  = -1;                      // nessun proprietario mentre è contesa
        }
        else if (NPlayer > 0 && NAI == 0) // solo il Player nella zona?
        {
            NewStatus = ETowerStatus::Controlled; // sotto controllo del Player
            NewOwner  = 0;                        // 0 = Player umano
        }
        else if (NAI > 0 && NPlayer == 0) // solo l'AI nella zona?
        {
            NewStatus = ETowerStatus::Controlled; // sotto controllo dell'AI
            NewOwner  = 1;                        // 1 = AI
        }
        

        // Aggiorna il contatore di turni consecutivi
        if (NewStatus == ETowerStatus::Controlled && NewOwner == State.ControlledBy)
        {
            State.ConsecutiveTurns++; // stesso proprietario, ancora sotto controllo -> incrementa
        }
        else if (NewStatus == ETowerStatus::Controlled && NewOwner != State.ControlledBy)
        {
            State.ConsecutiveTurns = 1; // nuovo proprietario -> resetta a 1 (questo è il primo turno)
        }
        else
        {
            State.ConsecutiveTurns = 0; // Contested o Neutral -> azzera il contatore
        }

        State.ControlledBy = NewOwner;                          // aggiorna il proprietario nel record
        State.Tower->UpdateTowerStatus(NewStatus, NewOwner);    // aggiorna il colore visivo della torre

        // Log
        FString StatusStr;
        switch (NewStatus)
        {
        case ETowerStatus::Neutral:    StatusStr = TEXT("NEUTRALE"); break;
        case ETowerStatus::Controlled: StatusStr = (NewOwner == 0) ? TEXT("PLAYER") : TEXT("AI"); break;
        case ETowerStatus::Contested:  StatusStr = TEXT("CONTESA");  break;
        }

        UE_LOG(LogTemp, Warning, TEXT("[Torre (%d,%d)] %s | Turni: %d"),
            State.Tower->X, State.Tower->Y, *StatusStr, State.ConsecutiveTurns);
    }
}

// Zona di cattura

int32 ATowerManager::CountUnitsInCaptureZone(ATile* TowerTile, const TArray<AUnit*>& Units) const
{
    if (!TowerTile || !Grid) return 0; // parametri non validi -> 0 unità

    // GetTilesInRadius usa distanza Chebyshev (include diagonali) con raggio 2
    TArray<ATile*> Zone = Grid->GetTilesInRadius(TowerTile, CAPTURE_RADIUS);  // zona Chebyshev raggio 2

    int32 Count = 0; // contatore unità nella zona

    for (AUnit* Unit : Units) // scansiona tutte le unità del giocatore
    {
        if (!Unit || !Unit->IsAlive()) continue;  // unità non valida o morta: salta
        if (!Unit->CurrentTile) continue;          // unità senza tile: salta

        if (Zone.Contains(Unit->CurrentTile)) // la tile dell'unità è nella zona di cattura?
            Count++;                          // conta questa unità
    }

    return Count; // numero totale di unità nella zona
}

// Vittoria

bool ATowerManager::CheckVictory(int32& OutWinner) const
{
    OutWinner = -1; // default: nessun vincitore

    int32 PlayerControlled  = 0; // torri attualmente controllate dal Player
    int32 PlayerConsecutive = 0; // di queste, quante hanno ConsecutiveTurns >= WIN_TURNS
    int32 AIControlled      = 0; // torri attualmente controllate dall'AI
    int32 AIConsecutive     = 0; // di queste, quante hanno ConsecutiveTurns >= WIN_TURNS

    for (const FTowerControlState& State : TowerStates) // analizza ogni torre
    {
        if (State.ControlledBy == 0) // controllata dal Player?
        {
            PlayerControlled++;
            if (State.ConsecutiveTurns >= WIN_TURNS) // da abbastanza turni consecutivi?
                PlayerConsecutive++;
        }
        else if (State.ControlledBy == 1) // controllata dall'AI?
        {
            AIControlled++;
            if (State.ConsecutiveTurns >= WIN_TURNS)  // da abbastanza turni consecutivi?
                AIConsecutive++;
        }
    }

    // Condizione vittoria: >= 2 torri su 3 controllate per >=2 turni consecutivi
    if (PlayerControlled >= 2 && PlayerConsecutive >= 2)  // condizione vittoria Player
    {
        OutWinner = 0; // Player vince
        UE_LOG(LogTemp, Warning, TEXT("[TowerManager] *** PLAYER WINS! ***"));
        return true;
    }

    if (AIControlled >= 2 && AIConsecutive >= 2)
    {
        OutWinner = 1; // AI vince
        UE_LOG(LogTemp, Warning, TEXT("[TowerManager] *** AI WINS! ***"));
        return true;
    }

    return false; // ancora nessun vincitore
}

// utiliry

int32 ATowerManager::CountTowersControlledBy(int32 PlayerID) const
{
    int32 Count = 0;
    for (const FTowerControlState& State : TowerStates) // scansiona tutte le torri
        if (State.ControlledBy == PlayerID) Count++;    // conta quelle del giocatore indicato
    return Count;
}

void ATowerManager::GetScore(int32& OutPlayer, int32& OutAI) const
{
    OutPlayer = CountTowersControlledBy(0); // torri del Player (ID=0)
    OutAI     = CountTowersControlledBy(1); // torri dell'AI (ID=1)
}
