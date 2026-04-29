// Fill out your copyright notice in the Description page of Project Settings.


#include "TurnManager.h"
#include "Tile.h"
#include "Unit.h"
#include "GameField.h"
#include "TowerManager.h"
#include "Pathfinder.h"
#include "AIControllerStrategico.h"
#include "StrategicoTurniGameMode.h"
#include "Engine/World.h"
#include "StrategicoTurniGameMode.h"

// composizione iniziale: 1 Sniper + 1 Brawler per giocatore
static const TArray<EStrategicoUnitType> STARTING_UNITS = {
    EStrategicoUnitType::Sniper,
    EStrategicoUnitType::Brawler
};


ATurnManager::ATurnManager()
{
    PrimaryActorTick.bCanEverTick = false; // nessun tick

    CurrentPhase = EGamePhase::CoinFlip; // fase iniziale prima del coin flip
    RoundNumber = 1; // i round partono da 1
    WinnerID = -1; // -1 = nessun vincitore ancora
    PlacementTurn = 0; // 0 = tocca al Player, 1 = tocca all AI

    Grid = nullptr; // assegnato da GameMode::SpawnAllActors()
    TowerManager = nullptr;
    SniperClass = nullptr; // impostato in BP_TurnManager Class Defaults
    BrawlerClass = nullptr; // impostato in BP_TurnManager Class Defaults
}

void ATurnManager::BeginPlay()
{
    Super::BeginPlay(); // inizializzazione base di AActor

}

// Avvio partita

void ATurnManager::StartGame()
{
    if (!Grid)
    {
        UE_LOG(LogTemp, Error, TEXT("[TurnManager] Grid non impostata!"));
        return;
    }

    MoveLog.Empty(); // svuota il log di eventuali partite precedenti
    RoundNumber = 1;
    WinnerID = -1;

    // Prima il coin flip (imposta CoinResult)
    PerformCoinFlip(); // imposta CoinResult (Player o AI)

    // Mostra il widget CoinFlip adeso che CoinResult è impostato
    if (GameModeRef)
        GameModeRef->ShowCoinFlipWidget(); // mostra il widget DOPO che CoinResult è impostato

    StartPlacementPhase(); // avvia la fase di piazzamento
}

void ATurnManager::PerformCoinFlip()
{
    CoinResult = FMath::RandBool() ? ECoinResult::Player : ECoinResult::AI; // 50% di probabilita per ciascuno

    FString Winner = (CoinResult == ECoinResult::Player) ? TEXT("PLAYER") : TEXT("AI");
    LogRaw(FString::Printf(TEXT("=== LANCIO MONETA: %s inizia ==="), *Winner));
    UE_LOG(LogTemp, Warning,
        TEXT("[TurnManager] Lancio moneta: %s inizia il piazzamento."), *Winner);
}

// Piazzamento

void ATurnManager::StartPlacementPhase()
{
    CurrentPhase = EGamePhase::Placement; // cambia la fase corrente

    PlayerUnitsToPlace = STARTING_UNITS; // coda Player: [Sniper, Brawler]
    AIUnitsToPlace = STARTING_UNITS;  // coda AI:     [Sniper, Brawler]

    PlacementTurn = (CoinResult == ECoinResult::Player) ? 0 : 1; // chi ha vinto il coin flip piazza per primo

    LogRaw(TEXT("=== FASE DI PIAZZAMENTO ==="));

    // Mostra il widget di piazzamento
    if (GameModeRef)
        GameModeRef->ShowPlacementWidget();

    // Se l'AI vince il coin flip -> piazza UNA sola unità, poi tocca al Player
    if (PlacementTurn == 1)
    {
        PlaceAIUnitsAuto();   // piazza una sola unità
        PlacementTurn = 0;    // ora tocca al Player
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[TurnManager] Piazzamento: tocca a %s"),
        (PlacementTurn == 0) ? TEXT("PLAYER") : TEXT("AI"));
}

void ATurnManager::PlaceAIUnitsAuto()
{
    // Piazza una sola unità per turno: alternanza 1-1
    if (AIUnitsToPlace.Num() == 0) return; // coda vuota: nulla da piazzare

    if (!Grid || !SniperClass || !BrawlerClass)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[TurnManager] SniperClass o BrawlerClass non impostate!"));
        return;
    }

    EStrategicoUnitType UnitType = AIUnitsToPlace[0];  // solo la prima

    TSubclassOf<AUnit> ChosenClass =
        (UnitType == EStrategicoUnitType::Sniper) ? SniperClass : BrawlerClass;

    // Raccoglie tutte le tile libere nella zona AI
    TArray<ATile*> FreeTiles; // raccoglie tutte le tile libere nella zona AI
    for (int32 X = 22; X <= 24; X++)
        for (int32 Y = 0; Y < Grid->GridHeight; Y++)
        {
            ATile* Tile = Grid->GetTile(X, Y);
            if (Tile && Tile->IsWalkable())
                FreeTiles.Add(Tile);
        }

    
    ATile* ChosenTile = nullptr;
    if (FreeTiles.Num() > 0)
        ChosenTile = FreeTiles[FMath::RandRange(0, FreeTiles.Num() - 1)]; // Sceglie una tile casuale tra quelle disponibili

    if (ChosenTile)
    {
        AUnit* Unit = SpawnUnit(ChosenTile, UnitType, EUnitOwner::Player2, ChosenClass);
        if (Unit)
        {
            FString TypeStr = (UnitType == EStrategicoUnitType::Sniper)
                ? TEXT("Sniper") : TEXT("Brawler");
            LogRaw(FString::Printf(TEXT("AI piazza %s in %s%d"),
                *TypeStr,
                *FString::Chr('A' + ChosenTile->X),
                ChosenTile->Y));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error,
            TEXT("[TurnManager] PlaceAIUnitsAuto: nessuna tile libera nella zona AI!"));
    }

    AIUnitsToPlace.RemoveAt(0);  // rimuove SOLO la prima
}

bool ATurnManager::PlacePlayerUnit(
    ATile* Tile,
    EStrategicoUnitType          UnitType,
    TSubclassOf<AUnit> UnitClass)
{
    if (CurrentPhase != EGamePhase::Placement) return false; // valido solo durante il piazzamento
    if (PlacementTurn != 0)                    return false; // solo se e il turno del Player
    if (!Tile || !UnitClass)                   return false; // Puntatori validi richiesti

    if (!Grid->IsPlayerDeployZone(Tile)) // verifica che la cella scelta appartenga alle prime 3 righe inferiori ( zona player)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TurnManager] Tile fuori zona Player."));
        return false;
    }
    
    int32 TypeIdx = PlayerUnitsToPlace.Find(UnitType); // cerca il tipo nella coda Player
    if (TypeIdx == INDEX_NONE) return false;

    if (!Tile->IsWalkable()) return false; // assicura che la cella non sia acqua o torre

    AUnit* Unit = SpawnUnit(Tile, UnitType, EUnitOwner::Player1, UnitClass); // esegue lo spawn effettivo nel mondo di gioco
    if (!Unit) return false;

    PlayerUnitsToPlace.RemoveAt(TypeIdx); // rimuove il tipo appena piazzato

    FString TypeStr = (UnitType == EStrategicoUnitType::Sniper) ? TEXT("Sniper") : TEXT("Brawler");
    LogRaw(FString::Printf(TEXT("HP piazza %s in %s%d"),
        *TypeStr,
        *FString::Chr('A' + Tile->X),
        Tile->Y));

    AdvancePlacement(); // aggiorna PlacementTurn e fa piazzare all'AI se necessario
    return true;
}

void ATurnManager::AdvancePlacement()
{
    bool bPlayerDone = PlayerUnitsToPlace.Num() == 0; // il Player ha finito tutte le unita?
    bool bAIDone = AIUnitsToPlace.Num() == 0;  // l'AI ha finito tutte le unita?

    if (bPlayerDone && bAIDone)
    {
        LogRaw(TEXT("=== PARTITA INIZIATA ==="));
        if (CoinResult == ECoinResult::Player)
            StartPlayerTurn(); // ricomincia il ciclo con il turno Player
        else
            StartAITurn(); // passa direttamente al turno AI
        return;
    }

    // Alterna: ora tocca all'altro
    PlacementTurn = (PlacementTurn == 0) ? 1 : 0; // alterna il turno di piazzamento

    if (PlacementTurn == 1 && AIUnitsToPlace.Num() > 0)
    {
        // AI piazza una sola unità e restituisce subito il turno al Player
        PlaceAIUnitsAuto();
        PlacementTurn = 0;

        // Controlla se entrambi hanno finito dopo questo piazzamento
        if (PlayerUnitsToPlace.Num() == 0 && AIUnitsToPlace.Num() == 0)
        {
            LogRaw(TEXT("=== PARTITA INIZIATA ==="));
            if (CoinResult == ECoinResult::Player)
                StartPlayerTurn();
            else
                StartAITurn();
        }
        return;
    }

    // Se l'AI ha già finito ma è "il suo turno", ridai al Player
    if (PlacementTurn == 1 && AIUnitsToPlace.Num() == 0)
    {
        PlacementTurn = 0;
    }
}

// Turno player

void ATurnManager::StartPlayerTurn()
{
    CurrentPhase = EGamePhase::PlayerTurn; // aggiorna la fase corrente
    CleanUnitArrays(); // rimuove dalla memoria i riferimenti alle unità distrutte nei turni precedenti

    
    ClearAllHighlights(); // cancella gli highlight rimasti dal turno AI

    for (AUnit* Unit : PlayerUnits)
    {
        if (Unit) Unit->ResetTurnFlags(); // resetta bHasMoved=false e bHasAttacked=false
    }

    LogRaw(FString::Printf(TEXT("=== TURNO %d - PLAYER ==="), RoundNumber));
    UE_LOG(LogTemp, Warning,
        TEXT("[TurnManager] === TURNO %d - PLAYER ==="), RoundNumber);
}

void ATurnManager::EndPlayerTurn()
{
    if (CurrentPhase != EGamePhase::PlayerTurn) return; // ignorato se non è il turno Player

    StartAITurn();
}

// turno AI

void ATurnManager::StartAITurn()
{
    CurrentPhase = EGamePhase::AITurn; // aggiorna la fase corrente
    CleanUnitArrays();

    for (AUnit* Unit : AIUnits)
    {
        if (Unit) Unit->ResetTurnFlags();
    }

    LogRaw(FString::Printf(TEXT("=== TURNO %d - AI ==="), RoundNumber));
    UE_LOG(LogTemp, Warning,
        TEXT("[TurnManager] === TURNO %d - AI ==="), RoundNumber);

    // Esegui il turno AI
    UAIControllerStrategico* AICtrl = NewObject<UAIControllerStrategico>(this); // crea il controller AI temporaneo
    if (AICtrl)
    {
        AICtrl->ExecuteTurn(this, Grid, TowerManager, AIUnits, PlayerUnits); // istanzia il controller AI per calcolare ed eseguire la strategia di questo turno
    }

    // Evidenzia range di movimento e attacco delle unità AI
    HighlightAIActions(); // mostra visivamente i range delle unita AI

    EvaluateEndOfTurn(); // valuta torri e controlla vittoria (solo fine round)

    if (CurrentPhase != EGamePhase::GameOver)
    {
        RoundNumber++; // avanza il contatore di round
        StartPlayerTurn();   // StartPlayerTurn() chiama ClearAllHighlights()
    }
}

// Azioni unità

bool ATurnManager::TryMoveUnit(AUnit* Unit, const TArray<ATile*>& Path)
{
    // spawn fallito
    if (CurrentPhase != EGamePhase::PlayerTurn) return false;
    if (!Unit || !Unit->IsAlive())              return false;
    if (Unit->UnitOwner != EUnitOwner::Player1) return false;
    if (Unit->bHasMoved)                        return false;
    if (Path.Num() < 2)                         return false; 

    ATile* From = Unit->CurrentTile; // cattura la posizione prima di muovere
    Unit->MoveAlongPathAnimated(Path); // aggiorna CurrentTile e bHasMoved subito (prima dell'animazione)
    ATile* To = Unit->CurrentTile;

    if (To != From) // l'unita si e effettivamente spostata?
        LogMove(Unit, From, To);

    CheckAndAutoEndPlayerTurn(); // controlla se tutte le unita hanno finito
    return true;
}

bool ATurnManager::TryAttackUnit(AUnit* Attacker, AUnit* Target)
{
    if (CurrentPhase != EGamePhase::PlayerTurn) return false;
    if (!Attacker || !Target)                   return false;
    if (!Attacker->IsAlive() || !Target->IsAlive()) return false;
    if (Attacker->UnitOwner != EUnitOwner::Player1) return false;
    if (Attacker->bHasAttacked)                 return false; // ha gia attaccato questo turno

    // Melee (Brawler) usa distanza Chebyshev: max(|DX|,|DY|)
    // Ranged (Sniper) usa distanza Manhattan: |DX|+|DY|
    int32 Dist; // distanza calcolata in base al tipo di attacco
    if (Attacker->AttackType == EAttackType::Melee)
    {
        int32 DX = FMath::Abs(Attacker->CurrentTile->X - Target->CurrentTile->X); // delta colonna
        int32 DY = FMath::Abs(Attacker->CurrentTile->Y - Target->CurrentTile->Y); // delta riga
        Dist = FMath::Max(DX, DY); // Chebyshev
    }
    else
    {
        Dist = UPathfinder::ManhattanDistance(Attacker->CurrentTile, Target->CurrentTile); // Manhattan: (Sniper)
    }

    if (Dist > Attacker->AttackRange) return false; // fuori range: attacco non valido

    if (Target->CurrentTile->Elevation > Attacker->CurrentTile->Elevation) // non si attacca verso l'alto
        return false;

    ATile* TargetTile = Target->CurrentTile; // salva la tile prima dell attacco (il target potrebbe respawnare)
    int32  Damage = Attacker->AttackUnit(Target);
    LogAttack(Attacker, TargetTile, Damage); // registra l'esito

    CheckAndAutoEndPlayerTurn();
    return true;
}

void ATurnManager::CheckAndAutoEndPlayerTurn()
{
    if (CurrentPhase != EGamePhase::PlayerTurn) return;

    int32 AliveCount = 0;
    int32 DoneCount = 0;
    // Analizza lo stato di tutte le unità del giocatore
    for (AUnit* Unit : PlayerUnits)
    {
        if (!Unit || !Unit->IsAlive()) continue;
        AliveCount++;
        if (Unit->bHasMoved && Unit->bHasAttacked) DoneCount++; // l'unità ha concluso quando ha esaurito sia il movimento che l'attacco
    }
    // Se tutte le unità viventi hanno agito, termina automaticamente il turno
    if (AliveCount > 0 && DoneCount == AliveCount)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[TurnManager] Tutte le unità Player hanno agito — fine turno automatica."));
        EndPlayerTurn();
    }
}

//Valutazione di fine turno

void ATurnManager::EvaluateEndOfTurn()
{
    if (!TowerManager) return;
    // Delega al TowerManager l'analisi delle posizioni e l'aggiornamento degli stati (Neutrale/Controllo/Contesa)
    TowerManager->EvaluateTowers(PlayerUnits, AIUnits); // aggiorna stato e ConsecutiveTurns di ogni torre

    int32 Winner = -1;
    if (TowerManager->CheckVictory(Winner)) // c'è un vincitore?
    {
        SetGameOver(Winner);
    }
}

void ATurnManager::SetGameOver(int32 Winner)
{
    CurrentPhase = EGamePhase::GameOver; // blocca qualsiasi ulteriore azione
    WinnerID = Winner;  // memorizza il vincitore (0=Player, 1=AI)

    FString WinnerStr = (Winner == 0) ? TEXT("PLAYER") : TEXT("AI");
    LogRaw(FString::Printf(TEXT("=== FINE PARTITA: %s VINCE ==="), *WinnerStr));
    UE_LOG(LogTemp, Warning,
        TEXT("[TurnManager] === FINE PARTITA: %s VINCE ==="), *WinnerStr);
}

// Highlight

void ATurnManager::ClearAllHighlights()
{
    if (!Grid) return;

    for (ATile* Tile : Grid->GridTiles) // scansiona tutta la griglia
    {
        if (Tile)
        {
            Tile->ClearHighlight();  // riporta ogni tile al suo colore di base
        }
    }
}

void ATurnManager::HighlightAIActions()
{
    if (!Grid) return;

    // Prima pulisce eventuali highlight residui del turno precedente
    ClearAllHighlights();

    for (AUnit* Unit : AIUnits)
    {
        if (!Unit || !Unit->IsAlive() || !Unit->CurrentTile) continue;

        // Mostriamo le tile raggiungibili dalla posizione finale
        // (dopo che l'unità si è già mossa in questo turno)
        TArray<ATile*> Reachable = UPathfinder::GetReachableTiles(
            Grid,
            Unit->CurrentTile,
            Unit->MoveRange
        );

        for (ATile* Tile : Reachable)
        {
            if (Tile) Tile->SetHighlight(true);
        }

        // Range di attacco (rosso)
        bool bRanged = (Unit->AttackType == EAttackType::Ranged);

        TArray<ATile*> Attackable = UPathfinder::GetAttackableTiles(
            Grid,
            Unit->CurrentTile,
            Unit->AttackRange,
            bRanged,
            Unit->CurrentTile->Elevation
        );

        for (ATile* Tile : Attackable)
        {
            if (Tile) Tile->SetHighlight(false);
        }
    }
}

// Spawn unità

AUnit* ATurnManager::SpawnUnit(
    ATile* Tile,
    EStrategicoUnitType Type,
    EUnitOwner InOwner,
    TSubclassOf<AUnit> UnitClass)
{
    if (!Tile || !UnitClass) return nullptr;

    FVector Location = Tile->GetActorLocation();
    Location.Z += 80.f;

    FActorSpawnParameters Params;
    Params.Owner = this;

    AUnit* Unit = GetWorld()->SpawnActor<AUnit>(
        UnitClass, Location, FRotator::ZeroRotator, Params);
    if (!Unit) return nullptr;

    Unit->UnitType = Type;  // imposta Sniper o Brawler
    Unit->UnitOwner = InOwner; // imposta Player1 o Player2(AI)
    Unit->InitStats(); // calcola HP, range, danno in base al tipo

    // === FIX VISIBILITÀ UNITÀ (reso più visibile) ===
    if (Unit->UnitMesh)
    {
        Unit->UnitMesh->SetRelativeScale3D(FVector(1.5f, 1.5f, 1.5f)); // ingrandisce per visibilita
        Unit->UnitMesh->SetRelativeLocation(FVector(0.f, 0.f, 25.f));
    }

    Unit->SpawnTile = Tile; // memorizza per il rispawn futuro
    Unit->SetTile(Tile); // marca la tile come Occupied e aggiorna la posizione

    RegisterUnit(Unit); // inserisce nell array PlayerUnits o AIUnits
    return Unit;
}

// Registrazione unità

void ATurnManager::RegisterUnit(AUnit* Unit)
{
    if (!Unit) return;

    if (Unit->GetUnitOwner() == EUnitOwner::Player1)
    {
        if (!PlayerUnits.Contains(Unit))
            PlayerUnits.Add(Unit);
    }
    else
    {
        if (!AIUnits.Contains(Unit))
            AIUnits.Add(Unit);
    }
}

// log mosse

void ATurnManager::LogMove(AUnit* Unit, ATile* From, ATile* To)
{
    if (!Unit || !From || !To) return;

    FString FromStr = FString::Printf(TEXT("%s%d"), // converte X in lettera (A=0, B=1...)
        *FString::Chr('A' + From->X), From->Y);
    FString ToStr = FString::Printf(TEXT("%s%d"),
        *FString::Chr('A' + To->X), To->Y);

    LogRaw(FString::Printf(TEXT("%s: %s %s -> %s"), *Unit->GetOwnerID(), *Unit->GetUnitID(), *FromStr, *ToStr));
}

void ATurnManager::LogAttack(AUnit* Attacker, ATile* TargetTile, int32 Damage)
{
    if (!Attacker || !TargetTile) return;

    FString TileStr = FString::Printf(TEXT("%s%d"),
        *FString::Chr('A' + TargetTile->X), TargetTile->Y);

    LogRaw(FString::Printf(TEXT("%s: %s %s %d"), *Attacker->GetOwnerID(), *Attacker->GetUnitID(), *TileStr, Damage));
}

void ATurnManager::LogRaw(const FString& Text)
{
    FMoveLogEntry Entry;
    Entry.Text = Text;
    MoveLog.Add(Entry);

    UE_LOG(LogTemp, Log, TEXT("[MoveLog] %s"), *Text);
}

// query

bool ATurnManager::IsPlayerTurn() const
{
    return CurrentPhase == EGamePhase::PlayerTurn;
}

bool ATurnManager::IsPlacementPhase() const
{
    return CurrentPhase == EGamePhase::Placement;
}

bool ATurnManager::IsGameOver() const
{
    return CurrentPhase == EGamePhase::GameOver;
}

// pulizia

void ATurnManager::CleanUnitArrays()
{
    PlayerUnits.RemoveAll([](AUnit* U) { return U == nullptr; }); // rimuove voci null con lambda
    AIUnits.RemoveAll([](AUnit* U) { return U == nullptr; }); // rimuove voci null con lambda
}