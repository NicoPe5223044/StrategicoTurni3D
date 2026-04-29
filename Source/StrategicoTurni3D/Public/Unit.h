// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Unit.generated.h"


class ATile;

// proprietario unità
UENUM(BlueprintType)
enum class EUnitOwner : uint8
{
    Player1  UMETA(DisplayName = "Player"), // giocatore
    Player2  UMETA(DisplayName = "AI") // Ai
};

// tipo unità
UENUM(BlueprintType)
enum class EStrategicoUnitType : uint8
{
    Sniper   UMETA(DisplayName = "Sniper"), // Sniper
    Brawler  UMETA(DisplayName = "Brawler") //Brawler
};

// ─────────────────────────────────────────────────────────────
// Tipo di attacco
// ─────────────────────────────────────────────────────────────
UENUM(BlueprintType)
enum class EAttackType : uint8
{
    Ranged UMETA(DisplayName = "Ranged"), // attacco a distanza
    Melee  UMETA(DisplayName = "Melee") // attaccp ravvicinato
};

UCLASS()
class STRATEGICOTURNI3D_API AUnit : public AActor
{
    GENERATED_BODY()

public:
    AUnit();

protected:
    virtual void BeginPlay() override;

public:
    UPROPERTY(VisibleAnywhere)
    class UStaticMeshComponent* UnitMesh; // mEsh visiva dell'unità

    // Testo 3D mostrato al passaggio del mouse (es. "Sniper" / "Brawler")
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Unit")
    class UTextRenderComponent* UnitLabel; // etichetta 3D sopra l'unità

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit")
    EUnitOwner UnitOwner; // possesssore unità

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit")
    EStrategicoUnitType UnitType; // tipo unità

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    EAttackType AttackType; // tipo di attacco

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
    int32 Health; // vita

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
    int32 MaxHealth; // vita massima

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    int32 MoveRange; // range di movimento

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    int32 AttackRange; //range di attacco

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    int32 MinDamage; // danno minimo 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    int32 MaxDamage; // danno massimo

    UPROPERTY()
    ATile* CurrentTile; // tile attuale occupata

    UPROPERTY()
    ATile* SpawnTile; // tile di spawn iniziale

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Turn")
    bool bHasMoved; // flag per vedere se si è già mossa l'unità

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Turn")
    bool bHasAttacked; // per vedere se ha già attaccato

    void InitStats(); // inizializza le statistiche in base al tipo
    void SetTile(ATile* Tile, bool bSnapVisualPosition = true); // imposta la tile corrente e opzionalmente aggiorna posizione visiva
    void MoveAlongPath(const TArray<ATile*>& Path); // Movimento istantaneo e...
    void MoveAlongPathAnimated(const TArray<ATile*>& Path, float Speed = 400.f);// ...animato lungo un percorso
    int32 AttackUnit(AUnit* Target); // esegue attacco e ritorna il danno inflitto
    void ReceiveDamage(int32 Damage); // applica danno all'unità
    bool IsAlive() const; // Controlla se l'unità è viva
    void ResetTurnFlags(); // Reset dei flag del turno
    void Respawn(); // respawn

    FString GetUnitID() const; // id unità
    FString GetOwnerID() const; //id proprietario
    EUnitOwner GetUnitOwner() const { return UnitOwner; }

protected:
    // chiamate automaticamente quando il cursore "entra/esce" dall'unità
    virtual void NotifyActorBeginCursorOver() override;
    virtual void NotifyActorEndCursorOver() override;

private:
    UPROPERTY()
    class UMaterialInstanceDynamic* DynamicMaterial; // materiale dinamico per cambiare colore runtime

    void UpdateUnitColor(); // aggiorna colore in base al proprietario
    void UpdateLabel();        // aggiorna il testo della label (tipo + HP correnti)
    void UpdateWorldPosition(); // aggiorna posizione nel mondo in base alla tile
    bool ShouldReceiveCounterAttack(AUnit* Target) const;  // determina se il target può contrattaccare

    TArray<ATile*> CurrentMovementPath; //percorso corrente
    int32          CurrentPathIndex = 0; //indice percorso
    float          MovementSpeed = 400.f; //velocità di movimento

    FTimerHandle   MovementTimerHandle; // timer per movimento

    UFUNCTION()
    void AdvanceMovement(); //avanza al prossimo step di movimento
};