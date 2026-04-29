// Fill out your copyright notice in the Description page of Project Settings.


#include "StrategicoPlayerController.h"
#include "StrategicoTurniGameMode.h"
#include "TurnManager.h"
#include "GameField.h"
#include "Tile.h"
#include "Unit.h"
#include "Pathfinder.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "InputCoreTypes.h"  

AStrategicoPlayerController::AStrategicoPlayerController()
{
    PrimaryActorTick.bCanEverTick = true;  // Tick abilitato
}

void AStrategicoPlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (auto* GM = Cast<AStrategicoTurniGameMode>(UGameplayStatics::GetGameMode(this)))
    {
        TurnManager = GM->TurnManager;  // salva il riferimento al TurnManager
        GameField = GM->GameField; // e Gamefield
    }
}

void AStrategicoPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    // Click sinistro
    InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this,  // click sinistro: piazzamento o selezione unita
        &AStrategicoPlayerController::OnLeftMousePressed);

    // Fine turno
    InputComponent->BindKey(EKeys::Enter, IE_Pressed, this,  // fine turno: binding diretto su Enter
        &AStrategicoPlayerController::OnEndTurnPressed);

    // Binding alternativo per il Fine Turno
    InputComponent->BindKey(EKeys::Tab, IE_Pressed, this,  // fine turno: binding alternativo su Tab
        &AStrategicoPlayerController::OnEndTurnPressed);
}

void AStrategicoPlayerController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!TurnManager || !TurnManager->IsPlayerTurn()) return;  // solo durante il turno Player
}

void AStrategicoPlayerController::OnLeftMousePressed()
{
    if (!TurnManager) return;

    // Fase di piazzamento
    if (TurnManager->IsPlacementPhase())
    {
        // Tocca al Player solo se PlacementTurn == 0
        if (TurnManager->PlacementTurn != 0) return;  // non è il turno del Player: ignora il click

        TArray<EStrategicoUnitType>& Queue = TurnManager->PlayerUnitsToPlace;  // coda unita da piazzare
        if (Queue.Num() == 0) return;  // il Player ha gia piazzato tutte le sue unita

        ATile* ClickedTile = nullptr;
        if (!GetTileUnderCursor(ClickedTile)) return;

        EStrategicoUnitType NextType = Queue[0];  // tipo dell unita da piazzare (prima in coda)
        TSubclassOf<AUnit> UnitClass = (NextType == EStrategicoUnitType::Sniper)
            ? TurnManager->SniperClass
            : TurnManager->BrawlerClass;

        TurnManager->PlacePlayerUnit(ClickedTile, NextType, UnitClass);  // tenta il piazzamento
        return;
    }

    // Turno player
    if (!TurnManager->IsPlayerTurn()) return;  // non è il turno Player: ignora

    ATile* ClickedTile = nullptr;
    if (!GetTileUnderCursor(ClickedTile)) return;

    // Se c'è già un'unità selezionata
    if (SelectedUnit)
    {
        // Click su tile raggiungibile -> muovi
        if (CurrentMovementHighlights.Contains(ClickedTile))  // click su tile di movimento?
        {
            TryMoveToTile(ClickedTile);  // tenta il movimento
            DeselectUnit();  // deseleziona dopo l azione
            return;
        }

        // Click su nemico attaccabile -> attacca
        if (CurrentAttackHighlights.Contains(ClickedTile))  // click su tile di attacco (nemica)?
        {
            TryAttackOnTile(ClickedTile);  // tenta l'attacco
            DeselectUnit();
            return;
        }

        // Click altrove -> deseleziona
        DeselectUnit();
    }

    // Nessuna unità selezionata -> prova a selezionare la propria
    if (ClickedTile && ClickedTile->TileState == ETileState::Occupied)
    {
        for (AUnit* Unit : TurnManager->PlayerUnits)
        {
            if (Unit && Unit->CurrentTile == ClickedTile && Unit->IsAlive())
            {
                SelectUnit(Unit);
                return;
            }
        }
    }
}
//per ottenere la tile sotto il cursore
bool AStrategicoPlayerController::GetTileUnderCursor(ATile*& OutTile) const
{
    OutTile = nullptr;
    FHitResult Hit;

    if (GetHitResultUnderCursor(ECC_Visibility, false, Hit))
    {
        AActor* HitActor = Hit.GetActor();

        if (AUnit* HitUnit = Cast<AUnit>(HitActor))  // ha colpito un unita?
        {
            if (HitUnit->CurrentTile)
            {
                OutTile = HitUnit->CurrentTile;  // usa la tile dell'unita come bersaglio
                return true;
            }
        }

        // altrimenti è una tile normale
        OutTile = Cast<ATile>(HitActor);  // ha colpito direttamente una tile
        return OutTile != nullptr;
    }
    return false;
}

void AStrategicoPlayerController::SelectUnit(AUnit* Unit)
{
    DeselectUnit();
    if (!Unit) return;

    SelectedUnit = Unit;

    // Evidenzia range
    ShowMovementAndAttackRange(Unit);  // calcola e mostra i range sulla griglia

    // Evidenzia la tile dell'unità selezionata
    if (Unit->CurrentTile)
        Unit->CurrentTile->SetHighlight(true); // verde = selezione
}

void AStrategicoPlayerController::DeselectUnit()
{
    ClearAllHighlights();
    if (SelectedUnit && SelectedUnit->CurrentTile)
        SelectedUnit->CurrentTile->ClearHighlight();

    SelectedUnit = nullptr;
}

void AStrategicoPlayerController::ShowMovementAndAttackRange(AUnit* Unit)
{
    ClearAllHighlights();

    if (!Unit || !Unit->CurrentTile) return;

    // Movimento: solo caselle raggiungibili (grigio)
    CurrentMovementHighlights = UPathfinder::GetReachableTiles(  // flood-fill con budget MoveRange
        GameField, Unit->CurrentTile, Unit->MoveRange);

    for (ATile* Tile : CurrentMovementHighlights)
    {
        if (Tile)
            Tile->SetHighlight(true);   // true = grigio
    }

    // Attacco: solo sulle unità nemiche (rosso)
    CurrentAttackHighlights = UPathfinder::GetAttackableTiles(  // tutte le tile nel range di attacco
        GameField,
        Unit->CurrentTile,
        Unit->AttackRange,
        (Unit->AttackType == EAttackType::Ranged),
        Unit->CurrentTile->Elevation);

    // Evidenzia in rosso solo le tile che contengono un nemico
    for (AUnit* Enemy : TurnManager->AIUnits)
    {
        if (Enemy && Enemy->IsAlive() && Enemy->CurrentTile)
        {
            if (CurrentAttackHighlights.Contains(Enemy->CurrentTile))
            {
                Enemy->CurrentTile->SetHighlight(false);   // false = rosso
            }
        }
    }

    // Evidenzia anche la tile dell'unità selezionata (grigio)
    if (Unit->CurrentTile)
        Unit->CurrentTile->SetHighlight(true);  // true = grigio = movimento
}
//pulisce gli highlight
void AStrategicoPlayerController::ClearAllHighlights()
{
    for (ATile* Tile : CurrentMovementHighlights)
        if (Tile) Tile->ClearHighlight();
    for (ATile* Tile : CurrentAttackHighlights)
        if (Tile) Tile->ClearHighlight();

    CurrentMovementHighlights.Empty();
    CurrentAttackHighlights.Empty();
}

void AStrategicoPlayerController::TryMoveToTile(ATile* TargetTile)
{
    if (!SelectedUnit || !TargetTile || !TurnManager) return;

    // Cattura la tile di partenza prima che SetTile() aggiorni CurrentTile
    ATile* FromTile = SelectedUnit->CurrentTile;  // cattura la posizione attuale prima di muovere

    TArray<ATile*> Path = UPathfinder::FindPath(GameField, FromTile, TargetTile);  // A* verso la destinazione

    // Instrada tutto attraverso TurnManager: gestisce bHasMoved + log
    TurnManager->TryMoveUnit(SelectedUnit, Path);  // delega al TurnManager: gestisce log e bHasMoved
}

void AStrategicoPlayerController::TryAttackOnTile(ATile* TargetTile)
{
    if (!SelectedUnit || !TargetTile) return;

    // Cerca unità nemica sulla tile
    for (AUnit* Enemy : TurnManager->AIUnits)
    {
        if (Enemy && Enemy->CurrentTile == TargetTile && Enemy->IsAlive())
        {
            TurnManager->TryAttackUnit(SelectedUnit, Enemy);  // delega al TurnManager: gestisce log e bHasAttacked
            return;
        }
    }
}

void AStrategicoPlayerController::OnEndTurnPressed()
{
    UE_LOG(LogTemp, Warning, TEXT("=== OnEndTurnPressed chiamato! ==="));  

    if (!TurnManager)
    {
        UE_LOG(LogTemp, Error, TEXT("TurnManager è nullptr!"));
        return;
    }

    if (TurnManager->IsPlayerTurn())
    {
        UE_LOG(LogTemp, Warning, TEXT("Fine turno Player → EndPlayerTurn()"));
        DeselectUnit();
        TurnManager->EndPlayerTurn();  // passa al turno AI (anche senza aver usato tutte le azioni)
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Non è il turno del Player — ignorato."));
    }
}



