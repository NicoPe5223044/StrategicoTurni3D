// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "StrategicoPlayerController.generated.h"

class AUnit;
class ATile;
class ATurnManager;
class AGameField;

UCLASS()
class STRATEGICOTURNI3D_API AStrategicoPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    AStrategicoPlayerController();

protected:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;  // binding input
    virtual void Tick(float DeltaTime) override; // aggiornamento per frame

private:
    UPROPERTY()
    ATurnManager* TurnManager = nullptr; // recuperato dal GameMode in BeginPlay

    UPROPERTY()
    AGameField* GameField = nullptr;    // usato per pathfinding e highlight

    AUnit* SelectedUnit = nullptr;      // unità attualmente selezionata

    // tile attualmente evidenziate (grigio=movimento, rosso=attacco)
    TArray<ATile*> CurrentMovementHighlights;
    TArray<ATile*> CurrentAttackHighlights;

    //void HandleLeftMouseClick();  funzione vecchia
    bool GetTileUnderCursor(ATile*& OutTile) const; //recupera la tile sotto al cursore

    void SelectUnit(AUnit* Unit); //selettore unità
    void DeselectUnit(); //deseleziona unità

    void ShowMovementAndAttackRange(AUnit* Unit); // per mostrare tile raggiungibili ed attaccabili
    void ClearAllHighlights(); // per rimuovere highlight

    void TryMoveToTile(ATile* TargetTile); //prova a muovere l'unità selezionata...
    void TryAttackOnTile(ATile* TargetTile); //... attaccare unità selezionata

    // bindings input
    UFUNCTION()
    void OnLeftMousePressed(); //click tastso sinistro

    UFUNCTION()
    void OnEndTurnPressed(); //per finire il turno
};