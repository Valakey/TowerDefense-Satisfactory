#include "TDLaserFence.h"
#include "TDEnemy.h"
#include "TDEnemyFlying.h"
#include "TDEnemyRam.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"

// Registre statique de tous les pylones actifs
TArray<ATDLaserFence*> ATDLaserFence::AllPylons;

// Breches: positions des pylones detruits
TArray<FVector> ATDLaserFence::BreachPoints;

ATDLaserFence::ATDLaserFence(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PrimaryActorTick.bCanEverTick = true;
    mFactoryTickFunction.bCanEverTick = true;

    // Mesh pylone
    PylonMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PylonMesh"));
    PylonMesh->SetupAttachment(RootComponent);
    PylonMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
    PylonMesh->SetCastShadow(false);
    PylonMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    PylonMesh->SetCollisionResponseToAllChannels(ECR_Block);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> PylonMeshObj(
        TEXT("/MonPremierMod/Meshes/Fence/Fence.Fence"));
    if (PylonMeshObj.Succeeded()) PylonMesh->SetStaticMesh(PylonMeshObj.Object);

    // Colonne collision invisible (bouche le trou au-dessus du pylone pour les flying)
    PylonCollisionColumn = CreateDefaultSubobject<UBoxComponent>(TEXT("PylonCollisionColumn"));
    PylonCollisionColumn->SetupAttachment(RootComponent);
    PylonCollisionColumn->SetBoxExtent(FVector(60.0f, 60.0f, 5000.0f));  // 120x120u base, 10000u haut
    PylonCollisionColumn->SetRelativeLocation(FVector(0.0f, 0.0f, 5000.0f));  // Centre a mi-hauteur
    PylonCollisionColumn->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    PylonCollisionColumn->SetCollisionResponseToAllChannels(ECR_Ignore);
    PylonCollisionColumn->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
    PylonCollisionColumn->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
    PylonCollisionColumn->SetVisibility(false);

    // Electricite
    PowerConnection = CreateDefaultSubobject<UFGPowerConnectionComponent>(TEXT("PowerConnection"));
    PowerConnection->SetupAttachment(PylonMesh);
    PowerConnection->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
    PowerInfo = CreateDefaultSubobject<UFGPowerInfoComponent>(TEXT("PowerInfo"));

    // Material laser beam (orange/jaune)
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> LaserMatObj(
        TEXT("/MonPremierMod/Materials/M_LaserBeam.M_LaserBeam"));
    if (LaserMatObj.Succeeded()) BarrierMaterial = LaserMatObj.Object;

    // Cube mesh pour les barrieres visuelles
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshObj(
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMeshObj.Succeeded()) CubeMesh = CubeMeshObj.Object;

}

void ATDLaserFence::BeginPlay()
{
    Super::BeginPlay();

    // S'enregistrer dans le registre statique
    AllPylons.AddUnique(this);

    // Retirer les breach points proches (joueur a replace un pylone = breche reparee)
    FVector MyLoc = GetActorLocation();
    for (int32 i = BreachPoints.Num() - 1; i >= 0; i--)
    {
        if (FVector::Dist(BreachPoints[i], MyLoc) < ConnectionRange)
        {
            UE_LOG(LogTemp, Warning, TEXT("TDLaserFence: Breche reparee a %s"), *BreachPoints[i].ToString());
            BreachPoints.RemoveAt(i);
        }
    }

    if (PowerInfo)
    {
        PowerInfo->SetTargetConsumption(PowerConsumption);
    }

    // Le joueur est bloque par le PylonMesh visible (BlockAll).
    // Mais la PylonCollisionColumn invisible (120x120u) est plus large que le mesh -> joueur doit l'ignorer.
    // Les barrieres holographiques sont ignorees dans CreateBarrierTo().
    ACharacter* Player = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
    if (Player)
    {
        UCapsuleComponent* Capsule = Player->GetCapsuleComponent();
        if (Capsule && PylonCollisionColumn)
        {
            Capsule->IgnoreComponentWhenMoving(PylonCollisionColumn, true);
        }
    }

    // Timer game-thread pour scan + creation barrieres (toutes les 2s, delai initial 1s)
    GetWorldTimerManager().SetTimer(GameThreadTimerHandle, this, &ATDLaserFence::GameThreadUpdate, 2.0f, true, 1.0f);
}

void ATDLaserFence::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Arreter le timer
    GetWorldTimerManager().ClearTimer(GameThreadTimerHandle);

    RemoveAllBarriers();

    // Notifier les autres pylones de supprimer leurs connexions vers nous
    for (ATDLaserFence* Other : AllPylons)
    {
        if (Other && Other != this && IsValid(Other))
        {
            Other->RemoveBarrierTo(this);
        }
    }

    // Se retirer du registre statique
    AllPylons.Remove(this);

    Super::EndPlay(EndPlayReason);
}

void ATDLaserFence::Factory_Tick(float dt)
{
    Super::Factory_Tick(dt);

    // SEULE operation safe sur worker thread: lire le statut power
    CheckPowerStatus();
}

void ATDLaserFence::GameThreadUpdate()
{
    // Cette fonction tourne sur le GAME THREAD (via FTimerHandle)
    // Safe pour: creer composants, modifier visibilite, etc.
    ScanForNearbyPylons();
    UpdateBarrierState();
}

void ATDLaserFence::CheckPowerStatus()
{
    bHasPower = false;

    // Courant direct via cable electrique
    if (PowerInfo && PowerInfo->HasPower())
    {
        bHasPower = true;
    }
    else if (PowerConnection && PowerConnection->HasPower())
    {
        bHasPower = true;
    }

    // Propagation: verifier si un pylone connecte a du courant direct
    if (!bHasPower)
    {
        bHasPower = HasPowerInNetwork();
    }
}

bool ATDLaserFence::HasPowerInNetwork() const
{
    // BFS via barrieres + AllPylons (pas de TActorIterator, thread-safe)
    TSet<const ATDLaserFence*> Visited;
    TArray<const ATDLaserFence*> Queue;
    Queue.Add(this);
    Visited.Add(this);

    while (Queue.Num() > 0)
    {
        const ATDLaserFence* Current = Queue.Pop();

        // Verifier courant DIRECT (pas reseau, eviter recursion infinie)
        if (Current != this)
        {
            if ((Current->PowerInfo && Current->PowerInfo->HasPower()) ||
                (Current->PowerConnection && Current->PowerConnection->HasPower()))
            {
                return true;
            }
        }

        // Ajouter les voisins connectes par barriere (nos barrieres)
        for (const FBarrierConnection& B : Current->Barriers)
        {
            if (B.OtherPylon.IsValid() && !Visited.Contains(B.OtherPylon.Get()))
            {
                Visited.Add(B.OtherPylon.Get());
                Queue.Add(B.OtherPylon.Get());
            }
        }

        // Aussi verifier les pylones qui nous ont connecte (connexion unidirectionnelle)
        for (ATDLaserFence* Other : AllPylons)
        {
            if (!Other || !IsValid(Other) || Visited.Contains(Other)) continue;
            for (const FBarrierConnection& OB : Other->Barriers)
            {
                if (OB.OtherPylon.Get() == Current)
                {
                    Visited.Add(Other);
                    Queue.Add(Other);
                    break;
                }
            }
        }
    }

    return false;
}

void ATDLaserFence::ScanForNearbyPylons()
{
    for (ATDLaserFence* Other : AllPylons)
    {
        if (!Other || Other == this || !IsValid(Other)) continue;

        float Dist = FVector::Dist(GetActorLocation(), Other->GetActorLocation());
        if (Dist > ConnectionRange) continue;

        // Deja connecte (dans un sens ou l'autre)?
        if (IsAlreadyConnectedTo(Other)) continue;

        // Seulement le pylone avec le plus petit ID cree la barriere (evite doublons)
        if (GetUniqueID() < Other->GetUniqueID())
        {
            CreateBarrierTo(Other);
        }
    }

    // Nettoyer les connexions invalides (pylone detruit)
    for (int32 i = Barriers.Num() - 1; i >= 0; i--)
    {
        if (!Barriers[i].OtherPylon.IsValid())
        {
            // Detruire tous les laser beams
            for (UStaticMeshComponent* Beam : Barriers[i].LaserBeams)
            {
                if (Beam) Beam->DestroyComponent();
            }
            if (Barriers[i].BarrierCollision) Barriers[i].BarrierCollision->DestroyComponent();
            Barriers.RemoveAt(i);
        }
    }
}

void ATDLaserFence::CreateBarrierTo(ATDLaserFence* OtherPylon)
{
    if (!OtherPylon || !CubeMesh || !BarrierMaterial) return;

    FVector MyLoc = GetActorLocation();
    FVector OtherLoc = OtherPylon->GetActorLocation();
    float MidZ = (MyLoc.Z + OtherLoc.Z) * 0.5f;

    // Direction 2D uniquement (ignore Z) pour eviter mur penche/mal place
    FVector Dir2D = FVector(OtherLoc.X - MyLoc.X, OtherLoc.Y - MyLoc.Y, 0.0f).GetSafeNormal();
    FRotator BarrierRot = Dir2D.Rotation();

    FVector MidPoint2D = FVector((MyLoc.X + OtherLoc.X) * 0.5f, (MyLoc.Y + OtherLoc.Y) * 0.5f, MidZ);
    float Distance = FVector::Dist2D(MyLoc, OtherLoc);

    // Offset bas de la barriere: -80u sous le midpoint
    float BaseZ = MidZ - 80.0f;

    // === VISUEL: 4 barres laser suivant la pente entre pylones ===
    // Hauteurs relatives au pylone (pas au BaseZ)
    TArray<float> BeamHeights = {0.0f, 75.0f, 150.0f, 225.0f};  // 4 barres espacees, demarrage au sol
    TArray<UStaticMeshComponent*> LaserBeamMeshes;

    for (float RelativeHeight : BeamHeights)
    {
        // Position des points d'attache sur chaque pylone
        FVector PointA = MyLoc + FVector(0.0f, 0.0f, RelativeHeight);
        FVector PointB = OtherLoc + FVector(0.0f, 0.0f, RelativeHeight);
        
        // Milieu entre les 2 points d'attache
        FVector BeamMidpoint = (PointA + PointB) * 0.5f;
        
        // Direction du faisceau (avec pitch si pente)
        FVector BeamDir = (PointB - PointA).GetSafeNormal();
        FRotator BeamRotation = BeamDir.Rotation();
        
        // Distance 3D entre les 2 points d'attache
        float BeamLength = FVector::Dist(PointA, PointB);
        float ScaleX = BeamLength / 100.0f;  // Longueur du laser
        float ScaleY = 0.06f;  // Diametre du faisceau (6u)
        float ScaleZ = 0.06f;  // Diametre du faisceau

        UStaticMeshComponent* LaserBeam = NewObject<UStaticMeshComponent>(this);
        LaserBeam->SetStaticMesh(CubeMesh);
        LaserBeam->SetMaterial(0, BarrierMaterial);
        LaserBeam->SetWorldLocation(BeamMidpoint);
        LaserBeam->SetWorldRotation(BeamRotation);
        LaserBeam->SetWorldScale3D(FVector(ScaleX, ScaleY, ScaleZ));
        LaserBeam->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        LaserBeam->SetCastShadow(false);
        LaserBeam->RegisterComponent();
        LaserBeam->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
        
        LaserBeamMeshes.Add(LaserBeam);
    }

    // === COLLISION: Box invisible qui bloque les ennemis (sol -> ciel, bloque flying) ===
    // Partir du sol (ou plus bas) jusqu'au ciel pour aucun gap en dessous
    float GroundZ = FMath::Min(MyLoc.Z, OtherLoc.Z) - 500.0f;  // 5m sous le pylone le plus bas
    float TopZ = FMath::Max(MyLoc.Z, OtherLoc.Z) + BarrierHeight;  // 100m au-dessus du plus haut
    float CollisionCenterZ = (GroundZ + TopZ) * 0.5f;
    float CollisionHeightHalf = (TopZ - GroundZ) * 0.5f;
    
    UBoxComponent* Box = NewObject<UBoxComponent>(this);
    Box->SetWorldLocation(FVector(MidPoint2D.X, MidPoint2D.Y, CollisionCenterZ));
    Box->SetWorldRotation(BarrierRot);
    Box->SetBoxExtent(FVector(Distance * 0.5f + 50.0f, 60.0f, CollisionHeightHalf));
    Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Box->SetCollisionResponseToAllChannels(ECR_Ignore);
    Box->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
    Box->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);  // LOS ennemis detecte la barriere
    // ECC_Visibility ignore -> les tourelles tirent a travers
    Box->SetGenerateOverlapEvents(true);
    Box->RegisterComponent();
    Box->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);

    // Joueur traverse les barrieres holographiques (pas les pylones)
    ACharacter* Player = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
    if (Player)
    {
        UCapsuleComponent* Capsule = Player->GetCapsuleComponent();
        if (Capsule)
        {
            Capsule->IgnoreComponentWhenMoving(Box, true);
        }
    }

    // Enregistrer la connexion
    FBarrierConnection Connection;
    Connection.OtherPylon = OtherPylon;
    Connection.LaserBeams = LaserBeamMeshes;
    Connection.BarrierCollision = Box;
    Barriers.Add(Connection);

    UE_LOG(LogTemp, Warning, TEXT("TDLaserFence: Barriere creee %s <-> %s (dist=%.0f)"),
        *GetName(), *OtherPylon->GetName(), Distance);
}

void ATDLaserFence::RemoveBarrierTo(ATDLaserFence* OtherPylon)
{
    for (int32 i = Barriers.Num() - 1; i >= 0; i--)
    {
        if (Barriers[i].OtherPylon.Get() == OtherPylon)
        {
            // Detruire tous les laser beams
            for (UStaticMeshComponent* Beam : Barriers[i].LaserBeams)
            {
                if (Beam) Beam->DestroyComponent();
            }
            if (Barriers[i].BarrierCollision) Barriers[i].BarrierCollision->DestroyComponent();
            Barriers.RemoveAt(i);
        }
    }
}

void ATDLaserFence::RemoveAllBarriers()
{
    for (FBarrierConnection& B : Barriers)
    {
        // Detruire tous les laser beams
        for (UStaticMeshComponent* Beam : B.LaserBeams)
        {
            if (Beam) Beam->DestroyComponent();
        }
        if (B.BarrierCollision) B.BarrierCollision->DestroyComponent();
    }
    Barriers.Empty();
}

void ATDLaserFence::UpdateBarrierState()
{
    bool bActive = bHasPower;

    for (FBarrierConnection& B : Barriers)
    {
        // L'autre pylone doit aussi avoir du courant dans son reseau
        bool bOtherHasPower = B.OtherPylon.IsValid() && B.OtherPylon->bHasPower;
        bool bBarrierActive = bActive && bOtherHasPower;

        // Afficher/masquer tous les laser beams
        for (UStaticMeshComponent* Beam : B.LaserBeams)
        {
            if (Beam)
            {
                Beam->SetVisibility(bBarrierActive);
            }
        }
        if (B.BarrierCollision)
        {
            B.BarrierCollision->SetCollisionEnabled(
                bBarrierActive ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
        }
    }
}

void ATDLaserFence::OnBarrierBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    // Callback de secours - normalement le Block sur ECC_Pawn suffit
    // Le joueur est deja ignore via IgnoreActorWhenMoving
    if (!OtherActor || !bHasPower) return;

    ATDEnemy* Enemy = Cast<ATDEnemy>(OtherActor);
    ATDEnemyFlying* FlyingEnemy = Cast<ATDEnemyFlying>(OtherActor);
    ATDEnemyRam* RamEnemy = Cast<ATDEnemyRam>(OtherActor);

    if (!Enemy && !FlyingEnemy && !RamEnemy) return;

    // Repousser l'ennemi hors de la barriere
    FVector EnemyLoc = OtherActor->GetActorLocation();
    FVector BarrierCenter = OverlappedComp->GetComponentLocation();
    FVector PushDir = (EnemyLoc - BarrierCenter).GetSafeNormal();
    PushDir.Z = 0.0f;
    if (PushDir.IsNearlyZero()) PushDir = FVector(1.0f, 0.0f, 0.0f);

    OtherActor->SetActorLocation(EnemyLoc + PushDir * 60.0f);
}

void ATDLaserFence::TakeDamageCustom(float DamageAmount)
{
    Health -= DamageAmount;
    
    if (Health <= 0.0f)
    {
        Die();
    }
}

void ATDLaserFence::Die()
{
    UE_LOG(LogTemp, Warning, TEXT("TDLaserFence %s detruit! Breche creee a %s"), *GetName(), *GetActorLocation().ToString());
    
    // Enregistrer la breche pour que les ennemis puissent la trouver
    BreachPoints.Add(GetActorLocation());
    
    RemoveAllBarriers();
    Destroy();
}

FVector ATDLaserFence::FindNearestBreach(const FVector& FromLocation, float MaxDistance)
{
    FVector Best = FVector::ZeroVector;
    float BestDist = MaxDistance;
    for (const FVector& Breach : BreachPoints)
    {
        float D = FVector::Dist(FromLocation, Breach);
        if (D < BestDist)
        {
            BestDist = D;
            Best = Breach;
        }
    }
    return Best;
}

bool ATDLaserFence::IsAlreadyConnectedTo(ATDLaserFence* Other) const
{
    // Verifier nos barrieres
    for (const FBarrierConnection& B : Barriers)
    {
        if (B.OtherPylon.Get() == Other) return true;
    }
    // Verifier les barrieres de l'autre (connexion creee de son cote)
    if (Other)
    {
        for (const FBarrierConnection& B : Other->Barriers)
        {
            if (B.OtherPylon.Get() == this) return true;
        }
    }
    return false;
}
