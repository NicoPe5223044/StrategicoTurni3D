// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Unit.h"
#include "TurnManager.generated.h"

class AUnit;
class AGameField;
class ATowerManager;
class AStrategicoTurniGameMode;

// Sequenza delle fasi di gioco
UENUM(BlueprintType)
enum class EGamePhase : uint8
{
    CoinFlip,    // Lancio moneta
    Placement,   // Piazzamento unità
    PlayerTurn,  // Turno del giocatore
    AITurn,      // Turno dell'AI
    GameOver     // Termine Partita
};

// Risultato lancio della moneta
UENUM(BlueprintType)
enum class ECoinResult : uint8
{
    Player,  // Inizia il giocatore
    AI  //inizia l'AI
};

// Entry del log mosse
USTRUCT(BlueprintType)
struct FMoveLogEntry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString Text; // Testo della singola entry di log
};

UCLASS()
class STRATEGICOTURNI3D_API ATurnManager : public AActor
{
    GENERATED_BODY()

public:
    ATurnManager();

protected:
    virtual void BeginPlay() override;

public:
    // ***Puntatori agli attori principali***
    UPROPERTY()
    AGameField* Grid;          // griglia di gioco

    UPROPERTY()
    ATowerManager* TowerManager;  // TowerManager

    // Usato per mostrare CoinFlip e Placement Widget
    UPROPERTY()
    AStrategicoTurniGameMode* GameModeRef = nullptr;

    // ***Stato partita***
    UPROPERTY(BlueprintReadOnly, Category = "Game")
    EGamePhase CurrentPhase; // Stato attuale del gioco

    UPROPERTY(BlueprintReadOnly, Category = "Game")
    ECoinResult CoinResult;    // chi ha vinto il lancio moneta

    UPROPERTY(BlueprintReadOnly, Category = "Game")
    int32 RoundNumber;         // numero del turno/round attuale

    UPROPERTY(BlueprintReadOnly, Category = "Game")
    int32 WinnerID;            // 0=Player, 1=AI, -1=non ancora confermato

    // ***Unità***
    UPROPERTY(BlueprintReadOnly, Category = "Units")
    TArray<AUnit*> PlayerUnits; // Array unità del giocatore

    UPROPERTY(BlueprintReadOnly, Category = "Units")
    TArray<AUnit*> AIUnits; // Array unità AI

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Units|Blueprint")
    TSubclassOf<AUnit> SniperClass; // Classe blueprint dello Sniper

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Units|Blueprint")
    TSubclassOf<AUnit> BrawlerClass;  // Classe blueprint del Brawler

    // ***Piazzamento***
    UPROPERTY(BlueprintReadOnly, Category = "Placement")
    TArray<EStrategicoUnitType> PlayerUnitsToPlace; // Coda unità da piazzare per il player

    UPROPERTY(BlueprintReadOnly, Category = "Placement")
    TArray<EStrategicoUnitType> AIUnitsToPlace; // Coda unità AI

    UPROPERTY(BlueprintReadOnly, Category = "Placement")
    int32 PlacementTurn; // 0 = player piazza, 1 = AI piazza (in modo da alternare)

    // ***Log mosse***
    UPROPERTY(BlueprintReadOnly, Category = "Log")
    TArray<FMoveLogEntry> MoveLog;

    // ***API PUBBLICA ***

    UFUNCTION(BlueprintCallable, Category = "Game")
    void StartGame(); //inizio partita

    UFUNCTION(BlueprintCallable, Category = "Game")
    bool PlacePlayerUnit(ATile* Tile, EStrategicoUnitType UnitType, TSubclassOf<AUnit> UnitClass); // piazzamento unità player

    UFUNCTION(BlueprintCallable, Category = "Game")
    void EndPlayerTurn(); // fine turno player

    UFUNCTION(BlueprintCallable, Category = "Game")
    void RegisterUnit(AUnit* Unit); // tegistra unità

    UFUNCTION(BlueprintCallable, Category = "Game")
    bool TryMoveUnit(AUnit* Unit, const TArray<ATile*>& Path); // tenta movimento lungo un percorso

    UFUNCTION(BlueprintCallable, Category = "Game")
    bool TryAttackUnit(AUnit* Attacker, AUnit* Target); // tenta attacco

    
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Game")
    bool IsPlayerTurn() const; // controlla lo stato del turno

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Game")
    bool IsPlacementPhase() const; // controlla fase di piazzamento

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Game")
    bool IsGameOver() const;  // controllo fine partita

    
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Placement")
    TArray<EStrategicoUnitType> GetCurrentPlacementQueueCopy() const
    {
        return (PlacementTurn == 0) ? PlayerUnitsToPlace : AIUnitsToPlace; // Se turno player, prende la sua coda per piazzare, altrimenti AI
    }

    UFUNCTION(BlueprintCallable, Category = "Log")
    void LogMove(AUnit* Unit, ATile* From, ATile* To);  // log movimento

    UFUNCTION(BlueprintCallable, Category = "Log")
    void LogAttack(AUnit* Attacker, ATile* TargetTile, int32 Damage); // log attacco

    UFUNCTION(BlueprintCallable, Category = "Log")
    void LogRaw(const FString& Text);

    UFUNCTION(BlueprintCallable, Category = "Game")
    void ClearAllHighlights(); // rimossione highlight visivi

    UFUNCTION(BlueprintCallable, Category = "Game")
    void HighlightAIActions(); // evidenzia azioni

    // Controlla se tutte le unità Player hanno agito → fine turno automatica
    void CheckAndAutoEndPlayerTurn();

private:
    void PerformCoinFlip();  // Determina chi inizia
    void StartPlacementPhase(); // Inizializza fase piazzamento
    void PlaceAIUnitsAuto(); // Piazzamento automatico AI
    void StartPlayerTurn();  // Setup turno player
    void StartAITurn(); // Setup turno AI
    void EvaluateEndOfTurn(); // Controlla condizioni di fine turno (torri, vittoria, ecc.)
    void SetGameOver(int32 Winner);  // Imposta stato finale
    void AdvancePlacement(); // Avanza nella coda di piazzamento


    AUnit* SpawnUnit(ATile* Tile, EStrategicoUnitType Type, EUnitOwner Owner, TSubclassOf<AUnit> UnitClass);

    void CleanUnitArrays(); // Rimuove unità morte dagli array
};