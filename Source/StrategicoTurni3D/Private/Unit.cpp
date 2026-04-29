// Fill out your copyright notice in the Description page of Project Settings.

#include "Unit.h"                                   
#include "Tile.h"                                   
#include "GameField.h"                              
#include "Components/StaticMeshComponent.h"         
#include "Components/TextRenderComponent.h"         
#include "Materials/MaterialInstanceDynamic.h"      
#include "TimerManager.h"                           

// Blu acceso per il Player
static const FLinearColor PLAYER_COLOR = FLinearColor(0.0f, 0.4f, 1.0f, 1.0f);
// Arancione scuro bruciato per l'AI
static const FLinearColor AI_COLOR = FLinearColor(0.65f, 0.15f, 0.0f, 1.0f);

// Statistiche Sniper (da specifiche)
static constexpr int32 SNIPER_HEALTH  = 20;  // HP Sniper
static constexpr int32 SNIPER_MOVE    = 4;   // mobilità Sniper
static constexpr int32 SNIPER_RANGE   = 10;  // range sniper
static constexpr int32 SNIPER_DMG_MIN = 4;   // danno minimo e...
static constexpr int32 SNIPER_DMG_MAX = 8;   // ...massimo

// Statistiche Brawler (da specifiche)
static constexpr int32 BRAWLER_HEALTH  = 40; // HP Brawler
static constexpr int32 BRAWLER_MOVE    = 6;  // mobilità brawler
static constexpr int32 BRAWLER_RANGE   = 1;  // range brawler
static constexpr int32 BRAWLER_DMG_MIN = 1;  // danno minimo e...
static constexpr int32 BRAWLER_DMG_MAX = 6;  // ...massimo

// Danno del contrattacco subito dallo Sniper quando attacca vicino
static constexpr int32 COUNTER_DMG_MIN = 1;
static constexpr int32 COUNTER_DMG_MAX = 3;

AUnit::AUnit()
{
    PrimaryActorTick.bCanEverTick = false; // nessun tick

    UnitMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("UnitMesh")); // mesh principale
    RootComponent = UnitMesh; // il mesh e il root dell'actor

    UnitLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("UnitLabel")); // componente testo
    UnitLabel->SetupAttachment(RootComponent);                     // agganciato al mesh
    UnitLabel->SetRelativeLocation(FVector(0.f, 0.f, 120.f));     // sopra l'unita
    UnitLabel->SetRelativeRotation(FRotator(90.f, 0.f, 180.f));   // leggibile dalla camera top-down
    UnitLabel->SetHorizontalAlignment(EHTA_Center);               // testo centrato
    UnitLabel->SetTextRenderColor(FColor(0, 60, 200));             // blu scuro
    UnitLabel->SetWorldSize(100.f);                                // dimensione testo in unita mondo
    UnitLabel->SetVisibility(false);                               // nascosta di default

    UnitMesh->SetGenerateOverlapEvents(true); // necessario per NotifyActorBeginCursorOver

    UnitOwner  = EUnitOwner::Player1;         // default (sovrascritto da SpawnUnit)
    UnitType   = EStrategicoUnitType::Sniper; // default
    AttackType = EAttackType::Ranged;         // default

    Health = MaxHealth = MoveRange = AttackRange = MinDamage = MaxDamage = 0; // inizializzati da InitStats
    CurrentTile = SpawnTile = nullptr;  // impostate al piazzamento
    DynamicMaterial = nullptr;          // creato in BeginPlay
    bHasMoved = bHasAttacked = false;   // flag azione: resettati a ogni turno
}

void AUnit::BeginPlay()
{
    Super::BeginPlay();

    SetActorEnableCollision(true); // abilita collisione per il raycast del cursore
    UnitMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block); // canale usato da GetHitResultUnderCursor

    if (UnitMesh && !DynamicMaterial) // materiale non ancora creato?
        DynamicMaterial = UnitMesh->CreateAndSetMaterialInstanceDynamic(0); // crea sul slot 0

    UpdateUnitColor(); // applica il colore Player/AI
}

void AUnit::InitStats()
{
    switch (UnitType) // seleziona le statistiche in base al tipo
    {
    case EStrategicoUnitType::Sniper:
        MaxHealth   = SNIPER_HEALTH;   // HP massimi
        MoveRange   = SNIPER_MOVE;     // celle percorribili per turno (con costo)
        AttackRange = SNIPER_RANGE;    // range attacco
        MinDamage   = SNIPER_DMG_MIN; // danno minimo
        MaxDamage   = SNIPER_DMG_MAX; // massimo
        AttackType  = EAttackType::Ranged; // ignora ostacoli, colpisce a distanza
        break;

    case EStrategicoUnitType::Brawler:
        MaxHealth   = BRAWLER_HEALTH; // HP massimi
        MoveRange   = BRAWLER_MOVE; // celle percorribili
        AttackRange = BRAWLER_RANGE;   //range attacco
        MinDamage   = BRAWLER_DMG_MIN; // danno minimo
        MaxDamage   = BRAWLER_DMG_MAX; // massimo
        AttackType  = EAttackType::Melee; // corpo a corpo (8 celle attorno)
        break;
    }

    Health = MaxHealth;    // parte con gli HP al massimo
    UpdateUnitColor();     // applica il colore corretto
    UpdateLabel();         // imposta il testo iniziale "Sniper 20 / 20"
}

void AUnit::NotifyActorBeginCursorOver()
{
    Super::NotifyActorBeginCursorOver(); // notifica il motore
    if (UnitLabel) UnitLabel->SetVisibility(true); // mostra nome e HP
}

void AUnit::NotifyActorEndCursorOver()
{
    Super::NotifyActorEndCursorOver(); // notifica l'engine
    if (UnitLabel) UnitLabel->SetVisibility(false); // nasconde la label
}

void AUnit::UpdateLabel()
{
    if (!UnitLabel) return; // label non esiste: ignora

    FString TypeStr = (UnitType == EStrategicoUnitType::Sniper) ? TEXT("Sniper") : TEXT("Brawler");
    // Formato: "Sniper 15 / 20" (esempio di aggiornamento dopo aver sibuto danno)
    FString Text = FString::Printf(TEXT("%s\n%d / %d"), *TypeStr, Health, MaxHealth);
    UnitLabel->SetText(FText::FromString(Text)); // aggiorna il testo
}

void AUnit::UpdateUnitColor()
{
    if (!DynamicMaterial) // materiale non ancora creato?
    {
        if (UnitMesh)
            DynamicMaterial = UnitMesh->CreateAndSetMaterialInstanceDynamic(0); // prova a crearlo
        if (!DynamicMaterial) return; // ancora null: BeginPlay lo ricreerà
    }

    const FLinearColor& Color = (UnitOwner == EUnitOwner::Player1) ? PLAYER_COLOR : AI_COLOR;
    DynamicMaterial->SetVectorParameterValue(TEXT("BaseColor"), Color); // applica al materiale
}

void AUnit::SetTile(ATile* Tile, bool bSnapVisualPosition)
{
    if (!Tile) return; // tile null: non valida

    if (CurrentTile)
        CurrentTile->TileState = ETileState::Empty; // libera la tile precedente

    CurrentTile = Tile;                             // aggiorna la tile corrente
    CurrentTile->TileState = ETileState::Occupied;  // marca come occupata

    if (bSnapVisualPosition)    // aggiorna subito la posizione 3D?
        UpdateWorldPosition();  // false durante l'animazione
}

void AUnit::UpdateWorldPosition()
{
    if (!CurrentTile) return;

    FVector Location = CurrentTile->GetActorLocation(); // posizione 3D della tile
    Location.Z += 150.f; // offset verticale: unita sopra la tile
    SetActorLocation(Location);
}

void AUnit::MoveAlongPath(const TArray<ATile*>& Path)
{
    if (Path.Num() < 2) return;  // path troppo corto
    MoveAlongPathAnimated(Path); // delega sempre alla versione animata
}

void AUnit::MoveAlongPathAnimated(const TArray<ATile*>& FullPath, float Speed)
{
    if (FullPath.Num() < 2) return;

    float BudgetUsed = 0.f;         // costo di movimento accumulato
    CurrentMovementPath.Empty();    // pulisce il path precedente
    CurrentMovementPath.Add(FullPath[0]); // aggiunge la tile di partenza

    for (int32 i = 1; i < FullPath.Num(); ++i)
    {
        // Salita costa 2, piano/discesa costa 1
        float StepCost = (FullPath[i]->Elevation > FullPath[i - 1]->Elevation) ? 2.f : 1.f;
        if (BudgetUsed + StepCost > static_cast<float>(MoveRange)) break; // supera il budget

        BudgetUsed += StepCost;
        CurrentMovementPath.Add(FullPath[i]);
    }

    if (CurrentMovementPath.Num() < 2) return;

    SetTile(CurrentMovementPath.Last(), false); // aggiorna subito la logica (prima dell'animazione)
    bHasMoved = true;                           // marca come gia mossa

    CurrentPathIndex = 1;   // parte dalla seconda tile
    MovementSpeed    = Speed;

    GetWorldTimerManager().SetTimer(MovementTimerHandle, this, &AUnit::AdvanceMovement, 0.35f, false);
}

void AUnit::AdvanceMovement()
{
    if (CurrentPathIndex >= CurrentMovementPath.Num()) // raggiunta l'ultima tile?
    {
        CurrentMovementPath.Empty();
        return;
    }

    ATile*  NextTile = CurrentMovementPath[CurrentPathIndex]; // tile di destinazione del prossimo step
    FVector StartPos = GetActorLocation();                    // posizione attuale
    FVector EndPos   = NextTile->GetActorLocation() + FVector(0.f, 0.f, 80.f); // destinazione

    float Distance = FVector::Dist(StartPos, EndPos);
    float Duration = FMath::Max(Distance / MovementSpeed, 0.15f); // durata minima 0.15s

    TWeakObjectPtr<AUnit> WeakThis(this); // weak: sicuro che l'unita verrà distrutta
    FTimerManager* TimerMgr = &GetWorldTimerManager();
    TSharedPtr<FTimerHandle> LerpHandle = MakeShared<FTimerHandle>();

    // interpola la posizione ad ogni frame
    auto LerpFunc = [WeakThis, StartPos, EndPos, Duration, TimerMgr, LerpHandle, Elapsed = 0.0f]() mutable
    {
        if (!WeakThis.IsValid()) { TimerMgr->ClearTimer(*LerpHandle); return; } // unita distrutta

        Elapsed += 0.016f; // ~1 frame a 60 FPS
        float Alpha = FMath::Clamp(Elapsed / Duration, 0.f, 1.f); // 0 -> 1

        WeakThis->SetActorLocation(FMath::Lerp(StartPos, EndPos, Alpha)); // interpola

        if (Alpha >= 1.f) // raggiunta la destinazione?
        {
            TimerMgr->ClearTimer(*LerpHandle);
            WeakThis->CurrentPathIndex++;
            TimerMgr->SetTimer(WeakThis->MovementTimerHandle, WeakThis.Get(),
                &AUnit::AdvanceMovement, 0.05f, false); // avvia il prossimo step
        }
    };

    TimerMgr->SetTimer(*LerpHandle, LerpFunc, 0.016f, true); // tick a ~60 FPS, looping
}

int32 AUnit::AttackUnit(AUnit* Target)
{
    if (!Target || !Target->IsAlive()) return 0; // bersaglio non valido

    int32 Damage = FMath::RandRange(MinDamage, MaxDamage); // danno casuale
    Target->ReceiveDamage(Damage);                          // applica il danno
    bHasAttacked = true;                                    // marca come gia attaccata

    // Contrattacco: lo Sniper che attacca vicino riceve danno di ritorno
    if (UnitType == EStrategicoUnitType::Sniper && ShouldReceiveCounterAttack(Target))
    {
        int32 CounterDmg = FMath::RandRange(COUNTER_DMG_MIN, COUNTER_DMG_MAX);
        UE_LOG(LogTemp, Warning, TEXT("[Contrattacco] %s%s subisce %d"),
            *GetOwnerID(), *GetUnitID(), CounterDmg);
        ReceiveDamage(CounterDmg);
    }

    return Damage;
}
// per vedere se riceve o no danno da contrattacco
bool AUnit::ShouldReceiveCounterAttack(AUnit* Target) const
{
    if (!Target || !CurrentTile || !Target->CurrentTile) return false;

    if (Target->UnitType == EStrategicoUnitType::Sniper)
        return true; // Sniper vs Sniper: contrattacco sempre

    if (Target->UnitType == EStrategicoUnitType::Brawler)
    {
        // Solo se adiacenti (Manhattan 1)
        int32 Dist = FMath::Abs(Target->CurrentTile->X - CurrentTile->X)
                   + FMath::Abs(Target->CurrentTile->Y - CurrentTile->Y);
        return (Dist == 1);
    }

    return false;
}

void AUnit::ReceiveDamage(int32 Damage)
{
    if (Damage <= 0) return;

    Health -= Damage; // riduce gli HP

    UE_LOG(LogTemp, Warning, TEXT("[Danno] %s%s subisce %d danni (HP: %d/%d)"),
        *GetOwnerID(), *GetUnitID(), Damage, FMath::Max(0, Health), MaxHealth);

    UpdateLabel(); // aggiorna la label con gli HP correnti

    if (Health <= 0)
    {
        Health = 0;
        Respawn(); // riporta l'unita alla spawn
    }
}

bool AUnit::IsAlive() const { return Health > 0; }

void AUnit::Respawn()
{
    if (!SpawnTile) // SpawnTile non impostata?
    {
        UE_LOG(LogTemp, Error, TEXT("[Respawn] %s%s: SpawnTile nulla."), *GetOwnerID(), *GetUnitID());
        if (CurrentTile) CurrentTile->TileState = ETileState::Empty;
        Destroy();
        return;
    }

    Health = MaxHealth; // ripristina gli HP
    UpdateLabel();      // aggiorna la label

    SetTile(SpawnTile); // riposiziona alla spawn

    bHasMoved = bHasAttacked = true; // non può agire nel turno corrente
}

void AUnit::ResetTurnFlags()
{
    bHasMoved    = false; // può muoversi di nuovo
    bHasAttacked = false; // può attaccare di nuovo
}

FString AUnit::GetUnitID() const
{
    return (UnitType == EStrategicoUnitType::Sniper) ? TEXT("S") : TEXT("B");
}

FString AUnit::GetOwnerID() const
{
    return (UnitOwner == EUnitOwner::Player1) ? TEXT("HP") : TEXT("AI");
}
