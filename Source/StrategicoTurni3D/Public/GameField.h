// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameField.generated.h"

class ATile;
class ATowerManager;

UCLASS(Blueprintable)
class STRATEGICOTURNI3D_API AGameField : public AActor
{
    GENERATED_BODY()

public:
    AGameField();

protected:
    virtual void BeginPlay() override;

public:
    // Parametri griglia
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
    int32 GridWidth = 25; // Numero colonne griglia

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
    int32 GridHeight = 25; // Numero righe griglia

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
    float TileSize = 200.f; // Dimensione di ogni tile nel mondo

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
    float ElevationStep = 50.f; // Altezza per ogni livello di elevazione

    // Parametri Noise
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise")
    float NoiseScale = 0.1f; // frequenza del Noise (più basso = terreno più uniforme)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise")
    float WaterThreshold = 0.2f; // soglia sotto cui la tile diventa acqua

    //  Classe Tile
    UPROPERTY(EditAnywhere, Category = "Grid")
    TSubclassOf<ATile> TileClass; // Classe delle Tile da spawnare

    // Tiles
    UPROPERTY()
    TArray<ATile*> GridTiles; // Array contenente tutte le tiles della griglia


    // Torri
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Towers")
    int32 NumberOfTowers = 3; // Numero di torri da spawnare

    // ***Api Pubblica***

    void GenerateGrid(); // Genera la griglia
    void SpawnTowers(ATowerManager* TowerMgr = nullptr); // Spawna le torri

    ATile* GetTile(int32 X, int32 Y) const; // Ritorna la tile alle coordinate (X,Y)
    bool   IsValidCoordinate(int32 X, int32 Y) const; // Controlla se le coordinate sono dentro la griglia

    TArray<ATile*> GetNeighbors(ATile* Tile) const; // Ritorna le tile adiacenti
    TArray<ATile*> GetTilesInRadius(ATile* Center, int32 MaxDist) const; // // Ritorna tutte le tile entro il raggio

    void EnsureConnectivity(); // Per garantire che la mappa sia completamente attraversabile

    bool IsPlayerDeployZone(ATile* Tile) const; // Controlla se la tile è zona di spawn player
    bool IsAIDeployZone(ATile* Tile) const; // e quella dell'AI

    UFUNCTION(BlueprintCallable, Category = "Map")
    void GenerateMap(); // Funzione Blueprint per generare la mappa


    UFUNCTION(BlueprintCallable, Category = "Map")
    void GenerateMapWithTowerManager(ATowerManager* TowerMgr); // Genera mappa + torri con manager

    UFUNCTION(BlueprintCallable, Category = "Map")
    void ClearGrid(); // Cancella tutte le tiles esistenti

private:
    int32 GenerateElevation(int32 X, int32 Y) const;  // Calcola l'elevazione della tile usando Perlin Noise

    ATile* FindNearestWalkableTile(
        int32 TargetX,
        int32 TargetY,
        bool ExcludeWater = true,
        const TSet<ATile*>& ExcludeTiles = TSet<ATile*>()) const;
    // Trova la tile camminabile più vicina a una posizione target
    // esclude l'acqua

    float NoiseOffsetX; // Offset casuale per il rumore (asse X)
    float NoiseOffsetY; // (asse Y)
};