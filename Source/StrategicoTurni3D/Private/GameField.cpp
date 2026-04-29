// Fill out your copyright notice in the Description page of Project Settings.


#include "GameField.h"
#include "Tile.h"
#include "Engine/World.h"


static const TArray<FIntPoint> IDEAL_TOWER_POSITIONS = {
    FIntPoint(12, 12), // torre centrale
    FIntPoint(12,  5), // torre lato sinistro (stessa riga, colonna sinistra)
    FIntPoint(12, 19)  // torre lato destro   (stessa riga, colonna destra)
};

AGameField::AGameField()
{
    PrimaryActorTick.bCanEverTick = false;
    NoiseOffsetX = 0.f;  // inizializzati a 0, randomizzati in BeginPlay
    NoiseOffsetY = 0.f;
}

void AGameField::BeginPlay()
{
    Super::BeginPlay();

    NoiseOffsetX = FMath::RandRange(0.f, 10000.f);  // offset X: mappa diversa ad ogni partita
    NoiseOffsetY = FMath::RandRange(0.f, 10000.f);  // offset Y: mappa diversa ad ogni partita
    
}
void AGameField::ClearGrid()
{
    for (ATile* Tile : GridTiles)
    {
        if (Tile && IsValid(Tile))  // tile valida e non gia distrutta?
            Tile->Destroy();  // rimuove l'actor dalla scena
    }
    GridTiles.Empty();  // svuota l'array
}
void AGameField::GenerateMapWithTowerManager(ATowerManager* TowerMgr)
{
    // Genera griglia + torri in un'unica passata, passando il manager.
    ClearGrid();  // rimuove la griglia precedente
    GenerateGrid();  // spawna tutte le 25x25 tile
    EnsureConnectivity();  // converte in acqua le tile "isolate" (tile circondate da acqua diventano isole)
    SpawnTowers(TowerMgr);  // unica chiamata, con il manager

    UE_LOG(LogTemp, Warning, TEXT("[GameField] Mappa generata con TowerManager!"));
}

void AGameField::GenerateMap()
{
    
    ClearGrid();
    GenerateGrid();
    EnsureConnectivity();
    SpawnTowers();

    UE_LOG(LogTemp, Warning, TEXT("[GameField] Mappa generata (senza TowerManager)."));
}

//Generazione griglia

void AGameField::GenerateGrid()
{
    if (!TileClass)  // la Blueprint class è impostata in Class Defaults?
    {
        UE_LOG(LogTemp, Error, TEXT("[GameField] TileClass non impostata!"));
        return;
    }

    GridTiles.Empty();
    GridTiles.SetNumZeroed(GridWidth * GridHeight);  // pre-alloca 25*25 = 625 slots con null

    for (int32 Y = 0; Y < GridHeight; Y++)
    {
        for (int32 X = 0; X < GridWidth; X++)
        {
            int32 Elevation = GenerateElevation(X, Y);  // calcola il livello tramite Perlin Noise

            //Calcola la posizione nello spazio 3D di Unreal
            FVector SpawnLocation =
                GetActorLocation() +
                FVector(
                    X * TileSize,
                    Y * TileSize,
                    Elevation * ElevationStep
                );

            FActorSpawnParameters SpawnParams;
            SpawnParams.Owner = this; // il GameField è "proprietario" di tutte le celle

            // spawna l'oggetto visivo
            ATile* Tile = GetWorld()->SpawnActor<ATile>(
                TileClass,
                SpawnLocation,
                FRotator::ZeroRotator,
                SpawnParams
            );
            //iinizializza i dati logici della cella appena spawnata
            if (Tile)
            {
                Tile->Init(X, Y, Elevation);  // imposta coordinate ed elevazione
                GridTiles[X + Y * GridWidth] = Tile;  // inserisce con indice lineare X + Y*25
            }
        }
    }
}

int32 AGameField::GenerateElevation(int32 X, int32 Y) const
{
    float NoiseValue = FMath::PerlinNoise2D(  // campiona il Perlin Noise 2D
        // moltiplica le coordinate per la scala e somma l'offset (Seed)
        FVector2D(
            (X * NoiseScale) + NoiseOffsetX,
            (Y * NoiseScale) + NoiseOffsetY
        )
    );

    // Perlin restituisce [-1, 1], normalizziamo a [0, 1]
    float Normalized = (NoiseValue + 1.f) * 0.5f;  // mappa da [-1,1] a [0,1]

    if (Normalized < WaterThreshold)  // sotto la soglia -> acqua
    {
        return 0;   // acqua
    }

    float Remapped = (Normalized - WaterThreshold) / (1.f - WaterThreshold);  // rimappa il range sopra la soglia
    int32 Elevation = 1 + FMath::FloorToInt(Remapped * 4.f);  // divide in 4 band: 1..4

    return FMath::Clamp(Elevation, 1, 4);  // garantisce range 1..4
}

//Torri

void AGameField::SpawnTowers(ATowerManager* TowerMgr)
{
    // Tiene traccia delle tile già usate come torre in questa chiamata,
    // per evitare che il fallback scelga una tile già assegnata a un'altra torre.
    TSet<ATile*> AlreadyUsed;  // tiene traccia delle tile gia usate come torre

    int32 Spawned = 0;

    for (const FIntPoint& IdealPos : IDEAL_TOWER_POSITIONS)
    {
        if (Spawned >= NumberOfTowers) break;

        ATile* Tile = GetTile(IdealPos.X, IdealPos.Y);

        // Posizione ideale valida: non acqua, non già torre, non già usata ora
        bool bIdealOk = Tile
            && Tile->Elevation > 0
            && !Tile->bIsTower
            && !AlreadyUsed.Contains(Tile);

        if (!bIdealOk)
        {
            // Fallback: tile calpestabile più vicina non ancora usata come torre
            Tile = FindNearestWalkableTile(IdealPos.X, IdealPos.Y, true, AlreadyUsed);  // fallback: cerca la tile libera piu vicina
        }

        if (Tile)
        {
            Tile->SetTower(true);  // imposta la tile come torre
            AlreadyUsed.Add(Tile);  // segna come usata per evitare duplicati
            Spawned++;

            UE_LOG(LogTemp, Warning,
                TEXT("[GameField] Torre %d spawned in (%d, %d)"),
                Spawned, Tile->X, Tile->Y);
        }
        else
        {
            UE_LOG(LogTemp, Error,
                TEXT("[GameField] Impossibile piazzare torre %d vicino a (%d,%d)"),
                Spawned + 1, IdealPos.X, IdealPos.Y);
        }
    }
}

ATile* AGameField::FindNearestWalkableTile(
    int32             TargetX,
    int32             TargetY,
    bool              ExcludeWater,
    const TSet<ATile*>& ExcludeTiles) const
{
    ATile* Best = nullptr;
    int32  BestDist = INT32_MAX;

    for (ATile* Tile : GridTiles)
    {
        if (!Tile)                              continue;  // puntatore null: salta
        if (Tile->bIsTower)                     continue;  // gia una torre: salta
        if (ExcludeWater && Tile->Elevation == 0) continue;  // acqua esclusa
        if (!Tile->IsWalkable())                continue;  // non calpestabile: salta
        if (ExcludeTiles.Contains(Tile))        continue;   // già usata come torre

        // Distanza di Manhattan (movimento a griglia)
        int32 Dist =
            FMath::Abs(Tile->X - TargetX) +
            FMath::Abs(Tile->Y - TargetY);
        // Aggiorna il record se trova una distanza inferiore
        if (Dist < BestDist)
        {
            BestDist = Dist;
            Best = Tile;
        }
    }

    return Best;
}

//connettività mappa

void AGameField::EnsureConnectivity()
{
    // Prima tile calpestabile come punto di partenza 
    ATile* StartTile = nullptr;
    for (ATile* Tile : GridTiles)
    {
        if (Tile && Tile->IsWalkable())
        {
            StartTile = Tile;
            break;
        }
    }

    if (!StartTile)  // nessuna tile calpestabile trovata?
    {
        UE_LOG(LogTemp, Error, TEXT("[GameField] Nessuna cella calpestabile trovata!"));
        return;
    }

    //per trovare tutte le celle raggiungibili
    TSet<ATile*>   Visited;
    TArray<ATile*> Queue;

    Queue.Add(StartTile);  //parte dalla prima tile calpestabile
    Visited.Add(StartTile);  // marca come visitata

    while (Queue.Num() > 0)
    {
        ATile* Current = Queue[0];  // prende il primo elemento
        Queue.RemoveAt(0);  // rimuove dalla coda

        for (ATile* Neighbor : GetNeighbors(Current))
        {
            if (!Neighbor)                   continue;
            if (Visited.Contains(Neighbor))  continue;
            if (!Neighbor->IsWalkable())     continue;

            Visited.Add(Neighbor);
            Queue.Add(Neighbor);
        }
    }

    // Celle calpestabili non raggiunte,forza a Elevation=1
    int32 Fixed = 0;
    for (ATile* Tile : GridTiles)
    {
        if (!Tile)                   continue;
        if (!Tile->IsWalkable())     continue; // acqua/torri: non toccare
        if (Visited.Contains(Tile))  continue; // già connessa: ok

        // Cella di terra isolata, diventa acqua
        // Così non è "raggiungibile ma non connessa" ma semplicemente rimossa
        Tile->Init(Tile->X, Tile->Y, 0);
        Fixed++;
    }

    if (Fixed > 0)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[GameField] EnsureConnectivity: %d celle isolate convertite in acqua."),
            Fixed);
    }
}

// query griglia

ATile* AGameField::GetTile(int32 X, int32 Y) const
{
    if (!IsValidCoordinate(X, Y)) return nullptr;
    return GridTiles[X + Y * GridWidth];  // indice lineare: X + Y*25
}

bool AGameField::IsValidCoordinate(int32 X, int32 Y) const
{
    return X >= 0 && X < GridWidth
        && Y >= 0 && Y < GridHeight;
}

TArray<ATile*> AGameField::GetNeighbors(ATile* Tile) const
{
    TArray<ATile*> Result;
    if (!Tile) return Result;

    // Solo 4 direzioni ortogonali (movimento NON obliquo)
    static const int32 DX[] = { 1, -1,  0,  0 };  // 4 direzioni ortogonali (no diagonali)
    static const int32 DY[] = { 0,  0,  1, -1 };

    for (int32 i = 0; i < 4; i++)
    {
        ATile* Neighbor = GetTile(Tile->X + DX[i], Tile->Y + DY[i]);
        if (Neighbor)
        {
            Result.Add(Neighbor);
        }
    }

    return Result;
}

TArray<ATile*> AGameField::GetTilesInRadius(ATile* Center, int32 MaxDist) const
{
    TArray<ATile*> Result;
    if (!Center) return Result;

    // Distanza di Chebyshev: include le diagonali
    // (usata per la zona di cattura delle torri)
    for (int32 DY = -MaxDist; DY <= MaxDist; DY++)
    {
        for (int32 DX = -MaxDist; DX <= MaxDist; DX++)
        {
            if (DX == 0 && DY == 0) continue;

            if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) > MaxDist) continue;

            ATile* Tile = GetTile(Center->X + DX, Center->Y + DY);
            if (Tile)
            {
                Result.Add(Tile);
            }
        }
    }

    return Result;
}

// zona di schieramento

bool AGameField::IsPlayerDeployZone(ATile* Tile) const
{
    // Con camera yaw=0, l'asse X è verticale sullo schermo.
    // X=0..2 = righe in basso (zona Player).
    return Tile && Tile->X >= 0 && Tile->X <= 2;  // X=0..2 = righe in basso (zona Player)
}

bool AGameField::IsAIDeployZone(ATile* Tile) const
{
    // X=22..24 = righe in alto (zona AI).
    return Tile && Tile->X >= 22 && Tile->X <= 24;  // X=22..24 = righe in alto (zona AI)
}