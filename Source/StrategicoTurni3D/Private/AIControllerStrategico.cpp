// Fill out your copyright notice in the Description page of Project Settings.

#include "AIControllerStrategico.h"  
#include "GameField.h"               
#include "Tile.h"                    
#include "Unit.h"                    
#include "Pathfinder.h"              
#include "TowerManager.h"            
#include "TurnManager.h"             


// Esecuzione del Turno


void UAIControllerStrategico::ExecuteTurn(
    ATurnManager* TurnMgr,              // per mettere su log le azioni
    AGameField*   Grid,                 // griglia di gioco
    ATowerManager* TowerMgr,            // stato corrente delle torri
    const TArray<AUnit*>& AIUnits,      // unità controllate dall'AI
    const TArray<AUnit*>& EnemyUnits)   // unità del Player
{
    if (!Grid) return;

    for (AUnit* Unit : AIUnits) // processa ogni unità AI in ordine
    {
        if (!Unit || !Unit->IsAlive()) continue; // unità non valida o morta: salta
        if (!Unit->CurrentTile)        continue; // unità senza posizione: salta

        AUnit* TargetEnemy = nullptr; // nemico bersaglio scelto per questa unità
        ATile* TargetTile  = nullptr; // tile bersaglio

        // Decide l'obiettivo ottimale per questa unità
        EAIGoal Goal = ChooseGoal(Unit, Grid, TowerMgr, EnemyUnits, TargetEnemy, TargetTile);  // Decide l'obiettivo ottimale per questa unità

        switch (Goal)
        {
        // Attacco immediato
        // Il nemico è già nel range di attacco: nessun movimento necessario
        case EAIGoal::AttackEnemy:
        {
            if (!TargetEnemy || !TargetEnemy->CurrentTile) break; // bersaglio non valido

            ATile* TileBefore = TargetEnemy->CurrentTile; // salva la tile prima di attaccare // (il nemico potrebbe non essere più lì)
                                                           
            int32 Damage = Unit->AttackUnit(TargetEnemy); // infligge il danno (gestisce anche il contrattacco)

            if (TurnMgr) TurnMgr->LogAttack(Unit, TileBefore, Damage); // registra nel log
            break;
        }

        // Avvicinamento al nemico(poi attacca se possibile)
        case EAIGoal::MoveToEnemy:
        {
            if (!TargetEnemy || !TargetEnemy->CurrentTile) break; // bersaglio non valido

            // Calcola il path A* verso la tile del nemico
            TArray<ATile*> Path = ComputeApproachPath(Unit, TargetEnemy->CurrentTile, Grid);  // Calcola il path A* verso la tile del nemico

            ATile* FromTile = Unit->CurrentTile; // posizione di partenza (per il log)

            if (Path.Num() > 1 && !Unit->bHasMoved) // path valido e unità non ancora mossa?
            {
                Unit->MoveAlongPath(Path); // esegue il movimento (aggiorna bHasMoved)

                if (TurnMgr && Unit->CurrentTile != FromTile) // si è effettivamente mossa?
                    TurnMgr->LogMove(Unit, FromTile, Unit->CurrentTile); // registra nel log
            }

            // Dopo il movimento: controlla se ora un nemico è nel range di attacco
            if (!Unit->bHasAttacked) // non ha ancora attaccato questo turno?
            {
                AUnit* Target = FindAttackableEnemy(Unit, Grid, EnemyUnits); // cerca nemico nel range
                if (Target)
                {
                    ATile* TB  = Target->CurrentTile;        // tile del bersaglio
                    int32  D   = Unit->AttackUnit(Target);    // attacca
                    if (TurnMgr) TurnMgr->LogAttack(Unit, TB, D); // registra
                }
            }
            break;
        }

        // Avvicinamento alla torre
        case EAIGoal::CaptureTower:
        {
            if (!TargetTile) break; // torre bersaglio non valida

            // Trova la tile adiacente alla torre più vicina all'unità
            // (le torri sono considerati ostacoli: non si mette sopra di esse)
            ATile* ApproachGoal = FindNearestWalkableTileAdjacentTo(Unit, TargetTile, Grid);  // tile adiacente alla torre piu vicina
            if (!ApproachGoal) break; // nessuna tile adiacente libera trovata

            TArray<ATile*> Path = ComputeApproachPath(Unit, ApproachGoal, Grid); // A* verso l'adiacente

            ATile* FromTile = Unit->CurrentTile; // posizione di partenza

            if (Path.Num() > 1 && !Unit->bHasMoved) // path valido e unità non ancora mossa?
            {
                Unit->MoveAlongPath(Path); // muove verso la zona di cattura

                if (TurnMgr && Unit->CurrentTile != FromTile) // si è mossa?
                    TurnMgr->LogMove(Unit, FromTile, Unit->CurrentTile);
            }

            // Dopo il movimento: attacca se c'è un nemico nel range
            if (!Unit->bHasAttacked)  // non ha ancora attaccato questo turno?
            {
                AUnit* Target = FindAttackableEnemy(Unit, Grid, EnemyUnits);  // cerca nemico nel range dopo il movimento
                if (Target)
                {
                    ATile* TB = Target->CurrentTile;
                    int32  D  = Unit->AttackUnit(Target);
                    if (TurnMgr) TurnMgr->LogAttack(Unit, TB, D);
                }
            }
            break;
        }

        // ─ 4. IDLE: nessuna azione utile disponibile ────────────────
        case EAIGoal::Idle:
        default:
            UE_LOG(LogTemp, Log, TEXT("[AI] %s: Idle."), *Unit->GetUnitID());
            break;
        }
    }
}


// Scelta Obiettivo

EAIGoal UAIControllerStrategico::ChooseGoal(
    AUnit*  Unit,                         // unità AI per cui scegliere l'obiettivo
    AGameField* Grid,                     // griglia (per i calcoli di distanza)
    ATowerManager* TowerMgr,             // stato delle torri
    const TArray<AUnit*>& EnemyUnits,    // unità del Player
    AUnit*& OutTargetEnemy,              // output: nemico bersaglio scelto
    ATile*& OutTargetTile) const         // output: tile torre bersaglio scelta
{
    OutTargetEnemy = nullptr; // inizializza gli output a null
    OutTargetTile  = nullptr;  // inizializza gli output a null

    // Priorità 1: attacca subito se c'è un nemico nel range ──────
    AUnit* AttackableEnemy = FindAttackableEnemy(Unit, Grid, EnemyUnits);  // priorita 1: attacca subito se nel range
    if (AttackableEnemy)
    {
        OutTargetEnemy = AttackableEnemy;
        return EAIGoal::AttackEnemy; // massima priorità: uccisione
    }

    // Priorità 2: valuta se avvicinarsi a una torre o a un nemico
    ATile* BestTower    = FindBestTowerTarget(Unit, Grid, TowerMgr); // torre più conveniente
    AUnit* NearestEnemy = FindNearestEnemy(Unit, EnemyUnits);        // nemico più vicino

    if (BestTower && NearestEnemy) // ci sono entrambe le opzioni?
    {
        int32 DistTower  = UPathfinder::ManhattanDistance(Unit->CurrentTile, BestTower);            // distanza dalla torre
        int32 DistEnemy  = UPathfinder::ManhattanDistance(Unit->CurrentTile, NearestEnemy->CurrentTile); // distanza dal nemico
        int32 AITowers   = TowerMgr ? TowerMgr->CountTowersControlledBy(1) : 0; // torri già dell'AI

        // Preferenza per la torre se è più vicina O se l'AI ne controlla già 1 // (per arrivare a 2 torri e vincere il prima possibile)
        bool bPreferTower = (DistTower <= DistEnemy) || (AITowers >= 1);  // preferisce torre se piu vicina o se ne controlla gia 1

        if (bPreferTower)
        {
            OutTargetTile = BestTower;
            return EAIGoal::CaptureTower;
        }
        else
        {
            OutTargetEnemy = NearestEnemy;
            return EAIGoal::MoveToEnemy;
        }
    }

    if (BestTower) // solo la torre disponibile (nessun nemico vivo)?
    {
        OutTargetTile = BestTower;
        return EAIGoal::CaptureTower;
    }

    if (NearestEnemy) // solo il nemico disponibile (nessuna torre conquistabile)?
    {
        OutTargetEnemy = NearestEnemy;
        return EAIGoal::MoveToEnemy;
    }

    return EAIGoal::Idle; // nessuna opzione utile, non fare nulla
}


//Ricerca bersagli


AUnit* UAIControllerStrategico::FindAttackableEnemy(
    AUnit* Attacker,                      // unità AI che attacca
    AGameField* Grid,                     // griglia
    const TArray<AUnit*>& EnemyUnits) const // unità del Player
{
    AUnit* Best   = nullptr;   // miglior bersaglio trovato
    int32  BestHP = INT32_MAX; // HP del miglior bersaglio (quello con meno HP)

    for (AUnit* Enemy : EnemyUnits)
    {
        if (!Enemy || !Enemy->IsAlive() || !Enemy->CurrentTile) continue; // nemico non valido: salta
        if (!IsInAttackRange(Attacker, Enemy)) continue; // fuori range: salta

        if (Enemy->Health < BestHP) // questo nemico ha meno HP del migliore attuale?
        {
            BestHP = Enemy->Health;
            Best   = Enemy; // aggiorna il miglior bersaglio
        }
    }

    return Best; // null se nessun nemico nel range
}

bool UAIControllerStrategico::IsInAttackRange(AUnit* Attacker, AUnit* Target) const
{
    if (!Attacker || !Target)                              return false; // parametri non validi
    if (!Attacker->CurrentTile || !Target->CurrentTile)   return false; // tile non impostate

    // Regola elevazione: non si può attaccare verso l'alto
    if (Target->CurrentTile->Elevation > Attacker->CurrentTile->Elevation) return false;  // non si attacca verso l'alto

    int32 Dist = UPathfinder::ManhattanDistance(Attacker->CurrentTile, Target->CurrentTile);

    if (Attacker->AttackType == EAttackType::Melee)
        return (Dist == 1);              // Brawler: solo cella ortogonalmente adiacente
    else
        return (Dist <= Attacker->AttackRange); // Sniper: qualsiasi distanza minore dell'AttackRange
}

AUnit* UAIControllerStrategico::FindNearestEnemy(AUnit* Unit, const TArray<AUnit*>& EnemyUnits) const
{
    if (!Unit || !Unit->CurrentTile) return nullptr; // unità non valida

    AUnit* Nearest  = nullptr;   // nemico più vicino trovato
    int32  BestDist = INT32_MAX; // distanza del più vicino

    for (AUnit* Enemy : EnemyUnits)
    {
        if (!Enemy || !Enemy->IsAlive() || !Enemy->CurrentTile) continue; // nemico non valido: salta

        int32 Dist = UPathfinder::ManhattanDistance(Unit->CurrentTile, Enemy->CurrentTile);  // calcola la distanza
        if (Dist < BestDist) // più vicino del migliore attuale?
        {
            BestDist = Dist;
            Nearest  = Enemy;
        }
    }

    return Nearest; // null se nessun nemico vivo trovato
}

ATile* UAIControllerStrategico::FindBestTowerTarget(
    AUnit* Unit, AGameField* Grid, ATowerManager* TowerMgr) const
{
    if (!Unit || !Unit->CurrentTile || !TowerMgr) return nullptr; // parametri non validi

    ATile* Best      = nullptr;   // torre bersaglio migliore
    int32  BestScore = INT32_MAX; // punteggio (minore = migliore)

    for (const FTowerControlState& State : TowerMgr->TowerStates) // analizza ogni torre
    {
        ATile* Tower = State.Tower;
        if (!Tower)                     continue; // puntatore null: salta
        if (State.ControlledBy == 1)   continue; // già controllata dall'AI: non serve conquistarla

        int32 Dist  = UPathfinder::ManhattanDistance(Unit->CurrentTile, Tower); // distanza dalla torre

        
        
        int32 Score = (State.ControlledBy == 0) ? Dist + 5 : Dist;  // Penalizzazione +5 per le torri del Player: si preferiscono le neutrali (costa meno conquistarne una neutra che strapparne una al Player)

        if (Score < BestScore) // punteggio migliore di quello attuale?
        {
            BestScore = Score;
            Best      = Tower;
        }
    }

    return Best; // null se tutte le torri sono già dell'AI
}

//Percorso di avvicinamento

TArray<ATile*> UAIControllerStrategico::ComputeApproachPath(
    AUnit* Unit, ATile* GoalTile, AGameField* Grid) const
{
    if (!Unit || !GoalTile || !Grid) return {}; // parametri non validi: path vuoto
    if (!Unit->CurrentTile)          return {}; // unità senza posizione: path vuoto
    if (Unit->CurrentTile == GoalTile) return {}; // già sulla tile goal: path vuoto

    
    return UPathfinder::FindPath(Grid, Unit->CurrentTile, GoalTile);  // A* standard: trova il percorso ottimale dalla posizione corrente alla tile obiettivo
}
//Tile calpestabile adiacente
ATile* UAIControllerStrategico::FindNearestWalkableTileAdjacentTo(
    AUnit* Unit, ATile* Target, AGameField* Grid) const
{
    if (!Unit || !Unit->CurrentTile || !Target || !Grid) return nullptr; // parametri non validi

    ATile* Best     = nullptr;   // miglior tile adiacente trovata
    int32  BestDist = INT32_MAX; // distanza della miglior tile dall'unità

    // Prima ricerca: tra le 4 tile ortogonali direttamente adiacenti alla torre
    for (ATile* Tile : Grid->GetNeighbors(Target))  // cerca tra le 4 tile adiacenti alla torre
    {
        if (!Tile || !Tile->IsWalkable()) continue; // non valida o non calpestabile: salta

        int32 Dist = UPathfinder::ManhattanDistance(Unit->CurrentTile, Tile); // distanza dall'unità
        if (Dist < BestDist) // più vicina della migliore attuale?
        {
            BestDist = Dist;
            Best     = Tile;
        }
    }

    if (Best) return Best; // trovata un'adiacente valida

    // Fallback: se tutte le tile adiacenti sono occupate/acqua, allarga la ricerca
    // Aumenta il raggio da 2 a 5 finché non trova una tile valida
    for (int32 R = 2; R <= 5 && !Best; R++)  // fallback: allarga il raggio se tutte le adiacenti sono occupate
    {
        for (ATile* Tile : Grid->GetTilesInRadius(Target, R)) // tutte le tile nel raggio R 
        {
            if (!Tile || !Tile->IsWalkable()) continue; // non valida: salta

            int32 Dist = UPathfinder::ManhattanDistance(Unit->CurrentTile, Tile);  // distanza dall unita
            if (Dist < BestDist) // più vicina?
            {
                BestDist = Dist;
                Best     = Tile; 
            }
        }
    }

    return Best; // null solo se tutta la griglia è bloccata
}
