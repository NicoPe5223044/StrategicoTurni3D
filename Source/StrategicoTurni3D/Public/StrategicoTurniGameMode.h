// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "StrategicoTurniGameMode.generated.h"

class AGameField;
class ATurnManager;
class ATowerManager;

UCLASS()
class STRATEGICOTURNI3D_API AStrategicoTurniGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AStrategicoTurniGameMode();

protected:
    virtual void BeginPlay() override;

public:
    UPROPERTY(EditAnywhere, Category = "Setup")
    TSubclassOf<AGameField> GameFieldClass; // Classe per la gestione della griglia 25x25

    UPROPERTY(EditAnywhere, Category = "Setup")
    TSubclassOf<ATurnManager> TurnManagerClass; // Classe per la logica dei turni e il log mosse

    UPROPERTY(EditAnywhere, Category = "Setup")
    TSubclassOf<ATowerManager> TowerManagerClass; // Classe per il controllo obiettivi (Torri)

    //questi tre vengono popolati durante lo SpawnActors
    UPROPERTY(BlueprintReadOnly, Category = "Game")
    AGameField* GameField;

    UPROPERTY(BlueprintReadOnly, Category = "Game")
    ATurnManager* TurnManager;

    UPROPERTY(BlueprintReadOnly, Category = "Game")
    ATowerManager* TowerManager;

    // Widget mostrato all'avvio per configurare e generare la mappa
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    TSubclassOf<class UUserWidget> MapConfigWidgetClass;

    // Widget che mostra il risultato del lancio della moneta
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    TSubclassOf<class UUserWidget> CoinFlipWidgetClass;

    // Widget che guida il Player nel piazzamento delle unità
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    TSubclassOf<class UUserWidget> PlacementWidgetClass;

    // Chiamata dal widget MapConfig dopo GenerateMapWithTowerManager()
    UFUNCTION(BlueprintCallable, Category = "Game")
    void RegisterTowersFromField(); // Registra le torri generate nella mappa all'interno del TowerManager per monitorarne il controllo

    UFUNCTION(BlueprintCallable, Category = "UI")
    void ShowCoinFlipWidget(); // Crea e aggiunge il widget per il lancio della moneta

    UFUNCTION(BlueprintCallable, Category = "UI")
    void ShowPlacementWidget(); // Crea e aggiunge il widget per la fase di schieramento unità

private:
    void SpawnAllActors(); // gestisce lo spawn in game
    void SetupTopDownCamera(); // Configura la telecamera in Top-Down 
};


