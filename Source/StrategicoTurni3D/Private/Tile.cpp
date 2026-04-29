// Fill out your copyright notice in the Description page of Project Settings.

#include "Tile.h"
#include "Components/StaticMeshComponent.h"    
#include "Materials/MaterialInstanceDynamic.h"  
ATile::ATile()
{
    PrimaryActorTick.bCanEverTick = false; // nesun tick

    TileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TileMesh")); // crea il componente mesh
    RootComponent = TileMesh; // il mesh è il root -> posizione actor = posizione mesh

    X = Y = GridX = GridY = 0; // coordinate nella griglia
    Elevation  = 1;             // elevazione di default: livello piano (non acqua)
    TileState  = ETileState::Empty; // non occupata all'inizio
    PlayerOwner = -1;           // -1 = nessun proprietario

    bIsTower    = false;             // non è una torre di default
    TowerStatus = ETowerStatus::Neutral; // neutrale all'inizio
    TowerOwner  = -1;                // -1 = nessun proprietario torre

    DynamicMaterial = nullptr;           // creato in BeginPlay()
    BaseColor       = FLinearColor::Green; // colore provvisorio (sovrascritto da UpdateTileColor)
    bIsHighlighted  = false;             // nessun highlight attivo
}

void ATile::BeginPlay()
{
    Super::BeginPlay(); 

    if (TileMesh) // mesh valido?
        DynamicMaterial = TileMesh->CreateAndSetMaterialInstanceDynamic(0); // crea il materiale dinamico (slot 0)

    UpdateTileColor(); // applica il colore corretto ora che il materiale esiste
}

// tile setup

void ATile::Init(int32 InX, int32 InY, int32 InElevation)
{
    X = GridX = InX; // imposta colonna e alias GridX sincronizzato
    Y = GridY = InY; // imposta riga e alias GridY sincronizzato

    Elevation = FMath::Clamp(InElevation, 0, 4); //clamp valori validi da 0 a 4

    // Elevazione 0 = acqua -> diventa ostacolo automaticamente (non calpestabile)
    TileState = (Elevation == 0) ? ETileState::Obstacle : ETileState::Empty;  // acqua = ostacolo automatico

    
    UpdateTileColor();  // applica il colore corretto ora che il materiale esiste
}

void ATile::SetupTile(int32 InX, int32 InY, int32 InElevation)
{
    Init(InX, InY, InElevation); // alias per compatibilità con codice Blueprint
}

// Stato tile

void ATile::SetAsObstacle()
{
    TileState = ETileState::Obstacle; // marca la tile come non calpestabile
}

void ATile::SetTower(bool bEnable)
{
    if (bEnable)
    {
        SetAsTower(); // setta la torre 
    }
    else
    {
        bIsTower    = false;              // rimuove il flag torre
        TowerStatus = ETowerStatus::Neutral; // resetta lo stato di controllo
        TowerOwner  = -1;                // nessun proprietario

        // Ripristina lo stato in base all'elevazione (acqua = ostacolo, terra = empty)
        TileState = (Elevation == 0) ? ETileState::Obstacle : ETileState::Empty;

        UpdateTileColor(); // aggiorna il colore al valore di base dell'elevazione
    }
}

void ATile::SetTileStatus(int32 InOwner, ETileState NewState)
{
    PlayerOwner = InOwner;  // aggiorna il proprietario
    TileState   = NewState; // aggiorna lo stato (Empty/Occupied/Obstacle)
}

bool ATile::IsWalkable() const
{
    if (Elevation == 0)                    return false; // acqua: mai calpestabile
    if (bIsTower)                          return false; // torre: ostacolo fisico (conquista per prossimità)
    if (TileState == ETileState::Obstacle) return false; // ostacolo generico
    if (TileState == ETileState::Occupied) return false; // un'unità è già qui -> blocca il pathfinding
    return true; // libera e su terreno solido
}

// torre

void ATile::SetAsTower()
{
    bIsTower    = true;                   // attiva il flag torre
    TowerStatus = ETowerStatus::Neutral;  // inizia neutra
    TowerOwner  = -1;                     // nessun proprietario

    TileState = ETileState::Obstacle;     // la torre è un ostacolo fisico (non si può salire sopra)

    BaseColor = FLinearColor(0.9f, 0.9f, 0.9f, 1.0f); // colore della torre
    ApplyColor(BaseColor); // applica subito il colore 
}

void ATile::UpdateTowerStatus(ETowerStatus NewStatus, int32 NewOwner)
{
    if (!bIsTower) return; // non è una torre: ignora

    TowerStatus = NewStatus; // aggiorna lo stato di controllo
    TowerOwner  = NewOwner;  // aggiorna il proprietario (-1, 0 o 1)

    switch (TowerStatus)
    {
    case ETowerStatus::Neutral:
        BaseColor = FLinearColor(0.9f, 0.9f, 0.9f, 1.0f); //colore torre neutrale
        break;

    case ETowerStatus::Controlled:
        BaseColor = (TowerOwner == 0)
            ? FLinearColor(0.0f, 0.4f, 1.0f, 1.0f)   // blu  = Player
            : FLinearColor(1.0f, 0.2f, 0.2f, 1.0f);  // rosso = AI
        break;

    case ETowerStatus::Contested:
        BaseColor = FLinearColor(0.8f, 0.0f, 0.8f, 1.0f); // viola = entrambi nella zona
        break;
    }

    if (!bIsHighlighted) // non sovrascrivere se c'è un highlight attivo
        ApplyColor(BaseColor); // aggiorna il colore visivo
}

//highlight

void ATile::SetHighlight(bool bIsMovement)
{
    bIsHighlighted = true; // marca come evidenziata 

    // bIsMovement determina il colore:
    // true  -> grigio medio = range di movimento (cliccabili per muoversi)
    // false -> rosso = range di attacco (cliccabili per attaccare)
    FLinearColor HighlightColor = bIsMovement
        ? FLinearColor(0.7f, 0.7f, 0.7f, 1.0f)  // grigio 
        : FLinearColor(1.0f, 0.0f, 0.0f, 1.0f); // rosso 

    ApplyColor(HighlightColor); // applica il colore di highlight al materiale
}

void ATile::ClearHighlight()
{
    bIsHighlighted = false;  // disattiva il flag highlight
    ApplyColor(BaseColor);   // ripristina il colore di base (elevazione o torre)
}

// Colore

void ATile::UpdateTileColor()
{
    if (bIsTower) return; // le torri gestiscono il proprio colore tramite UpdateTowerStatus

    switch (Elevation) // assegna il colore in base al livello di elevazione
    {
    case 0: BaseColor = FLinearColor(0.0f, 0.3f, 0.9f,  1.0f); break; // blu scuro  = acqua (non calpestabile)
    case 1: BaseColor = FLinearColor(0.1f, 0.7f, 0.1f,  1.0f); break; // verde      = pianura (livello base)
    case 2: BaseColor = FLinearColor(0.9f, 0.9f, 0.0f,  1.0f); break; // giallo     = colline basse
    case 3: BaseColor = FLinearColor(1.0f, 0.5f, 0.0f,  1.0f); break; // arancio    = montagne
    case 4: BaseColor = FLinearColor(0.9f, 0.05f, 0.05f,1.0f); break; // rosso scuro = picchi (costo movimento 2)
    default: BaseColor = FLinearColor::White; break;                   // fallback 
    }

    if (!bIsHighlighted) // se non c'è un highlight attivo...
        ApplyColor(BaseColor); // ...applica il colore di base
}

void ATile::ApplyColor(const FLinearColor& Color)
{
    if (DynamicMaterial) // il materiale esiste (dopo BeginPlay)?
        DynamicMaterial->SetVectorParameterValue(TEXT("BaseColor"), Color); // imposta il parametro "BaseColor"
}
