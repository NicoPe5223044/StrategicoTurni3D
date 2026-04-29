// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Tile.generated.h"

// Stato di occupazione della tile
UENUM(BlueprintType)
enum class ETileState : uint8
{
    Empty,    // calpestabile e libera
    Occupied, // occupata da un'unità (blocca il pathfinding)
    Obstacle  // acqua o torre (non calpestabile)
};

// Stato di controllo di una torre
UENUM(BlueprintType)
enum class ETowerStatus : uint8
{
    Neutral,    // nessuna unità nella zona di cattura
    Controlled, // un solo giocatore nella zona
    Contested   // entrambi i giocatori nella zona
};

UCLASS()
class STRATEGICOTURNI3D_API ATile : public AActor
{
    GENERATED_BODY()

public:
    ATile();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class UStaticMeshComponent* TileMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile Info")
    int32 X; // colonna (con camera yaw=0: asse verticale sullo schermo)

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile Info")
    int32 Y; // riga

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile Info")
    int32 GridX;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile Info")
    int32 GridY;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile Info")
    int32 Elevation; // 0=acqua, 1=piano, 2=colline, 3=montagna, 4=picco

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Tile Info")
    ETileState TileState;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile Info")
    int32 PlayerOwner;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tower")
    bool bIsTower;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tower")
    ETowerStatus TowerStatus;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tower")
    int32 TowerOwner;

    void Init(int32 InX, int32 InY, int32 InElevation);
    void SetupTile(int32 InX, int32 InY, int32 InElevation);

    void SetAsObstacle();
    void SetTower(bool bEnable);
    void SetTileStatus(int32 Owner, ETileState NewState);

    UFUNCTION(BlueprintCallable)
    bool IsWalkable() const;

    void UpdateTowerStatus(ETowerStatus NewStatus, int32 NewOwner);
    void SetHighlight(bool bIsMovement); // true=grigio(movimento), false=rosso(attacco)
    void ClearHighlight();                // ripristina il colore di base

protected:
    virtual void BeginPlay() override;

private:
    void UpdateTileColor();
    void ApplyColor(const FLinearColor& Color);

    UPROPERTY()
    class UMaterialInstanceDynamic* DynamicMaterial;

    FLinearColor BaseColor;
    bool bIsHighlighted = false;

    void SetAsTower();   // funzione privata
};
