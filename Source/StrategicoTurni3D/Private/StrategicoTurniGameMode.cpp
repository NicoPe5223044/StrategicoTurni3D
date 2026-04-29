// Fill out your copyright notice in the Description page of Project Settings.

#include "StrategicoTurniGameMode.h"      
#include "GameField.h"                     
#include "TurnManager.h"                   
#include "TowerManager.h"                  
#include "Tile.h"                          
#include "Camera/CameraActor.h"            
#include "Kismet/GameplayStatics.h"        
#include "Blueprint/UserWidget.h"          
#include "Camera/CameraComponent.h"        

AStrategicoTurniGameMode::AStrategicoTurniGameMode()
{
    PrimaryActorTick.bCanEverTick = false; // nessun tick necessario
}

void AStrategicoTurniGameMode::BeginPlay()
{
    Super::BeginPlay(); 

    SpawnAllActors();    // crea GameField, TurnManager, TowerManager e collega i riferimenti
    SetupTopDownCamera(); // posiziona la camera ortografica centrata sulla griglia

    // Mostra il widget di configurazione mappa (primo schermo visibile al giocatore)
    if (MapConfigWidgetClass) // la classe Blueprint è impostata nei Class Defaults?
    {
        UUserWidget* Widget = CreateWidget<UUserWidget>(GetWorld(), MapConfigWidgetClass);
        if (Widget)
        {
            Widget->AddToViewport(999); // ZOrder altissimo: sopra tutto il resto
            UE_LOG(LogTemp, Warning, TEXT("[GameMode] Widget MapConfig mostrato."));
        }
    }
}

void AStrategicoTurniGameMode::SpawnAllActors()
{
    // Tutti e tre i parametri devono essere impostati in BP_GameMode
    if (!GameFieldClass || !TurnManagerClass || !TowerManagerClass) return;  // tutti e tre devono essere impostati in Class Defaults

    GameField    = GetWorld()->SpawnActor<AGameField>(GameFieldClass);   // griglia di gioco
    TurnManager  = GetWorld()->SpawnActor<ATurnManager>(TurnManagerClass); // gestore turni
    TowerManager = GetWorld()->SpawnActor<ATowerManager>(TowerManagerClass); // gestore torri

    if (!GameField || !TurnManager || !TowerManager) return; // spawn fallito: esci

    // Collega i riferimenti incrociati tra gli attori
    TurnManager->Grid         = GameField;   // il TurnManager ha bisogno della griglia per lo spawn
    TurnManager->TowerManager = TowerManager; // il TurnManager delega la valutazione torri
    TurnManager->GameModeRef  = this;         // permette al TurnManager di mostrare i widget
    TowerManager->Grid        = GameField;    // il TowerManager ha bisogno della griglia per GetTilesInRadius

    UE_LOG(LogTemp, Warning, TEXT("[GameMode] Attori spawned. In attesa del widget MapConfig."));
}

void AStrategicoTurniGameMode::RegisterTowersFromField()
{
    // Precondizioni: TowerManager e GameField devono esistere
    if (!TowerManager || !GameField) return;

    TowerManager->TowerStates.Empty(); // svuota le torri precedenti

    for (ATile* Tile : GameField->GridTiles) // scansiona tutte le tile della griglia
    {
        if (Tile && Tile->bIsTower)           // questa tile è una torre?
            TowerManager->RegisterTower(Tile); // registra nel TowerManager
    }

    UE_LOG(LogTemp, Warning, TEXT("[GameMode] Torri registrate: %d"),
        TowerManager->TowerStates.Num()); //numero torri (3)
}

void AStrategicoTurniGameMode::ShowCoinFlipWidget()
{
    if (!CoinFlipWidgetClass) // classe Blueprint non impostata in Class Defaults?
    {
        UE_LOG(LogTemp, Error, TEXT("[GameMode] CoinFlipWidgetClass non impostata!"));
        return;
    }

    UUserWidget* Widget = CreateWidget<UUserWidget>(GetWorld(), CoinFlipWidgetClass);
    if (Widget)
    {
        Widget->AddToViewport(500); // ZOrder 500: sopra il HUD (0-100) ma sotto il MapConfig (999)
        UE_LOG(LogTemp, Warning, TEXT("[GameMode] WBP_CoinFlip mostrato."));
    }
   
}

//mostrare placement widget
void AStrategicoTurniGameMode::ShowPlacementWidget()
{
    if (!PlacementWidgetClass) // classe Blueprint non impostata?
    {
        UE_LOG(LogTemp, Error, TEXT("[GameMode] PlacementWidgetClass non impostata!"));
        return;
    }

    UUserWidget* Widget = CreateWidget<UUserWidget>(GetWorld(), PlacementWidgetClass);
    if (Widget)
    {
        Widget->AddToViewport(100); // ZOrder 100: visibile durante il gioco, sopra il HUD base
        UE_LOG(LogTemp, Warning, TEXT("[GameMode] WBP_Placement mostrato."));
    }
}

//setup della camera di gioco
void AStrategicoTurniGameMode::SetupTopDownCamera()
{
    
    FVector Center = FVector(12 * 200.f, 12 * 200.f, 3000.f); // X=2400, Y=2400, Z=3000 (altezza camera)
    FRotator Rot   = FRotator(-90.f, 0.f, 0.f);               // guarda verso il basso

    ACameraActor* Camera = GetWorld()->SpawnActor<ACameraActor>(
        ACameraActor::StaticClass(), Center, Rot); // spawna la camera nella scena

    if (!Camera) return; // spawn fallito

    // Imposta proiezione
    Camera->GetCameraComponent()->ProjectionMode = ECameraProjectionMode::Orthographic;  // proiezione: niente prospettiva

    // larghezza della finestra visiva in unità mondo
    Camera->GetCameraComponent()->OrthoWidth = 9000.f;

    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0); // recupera il PlayerController (indice 0)
    if (PC)
    {
        PC->SetViewTargetWithBlend(Camera, 0.f); // imposta immediatamente la camera
        PC->bShowMouseCursor = true;             // mostra il cursore del mouse
    }
}
