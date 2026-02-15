#include "TDWaveHUD.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/SOverlay.h"
#include "Fonts/SlateFontInfo.h"
#include "Engine/Font.h"

void STDWaveHUD::Construct(const FArguments& InArgs)
{
    // Activer le tick pour le timer
    SetCanTick(true);
    
    // IMPORTANT: Ne pas capturer les clics de souris (HitTestInvisible)
    SetVisibility(EVisibility::HitTestInvisible);
    
    // Police Roboto du jeu (ou fallback sur la police par defaut)
    FSlateFontInfo BigFont = FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 52);
    FSlateFontInfo SmallFont = FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 28);
    
    // Couleurs avec meilleur contraste
    FLinearColor WaveColor = FLinearColor(1.0f, 0.2f, 0.2f, 1.0f);  // Rouge vif
    FLinearColor CounterColor = FLinearColor(1.0f, 0.9f, 0.3f, 1.0f);  // Jaune/Or
    
    ChildSlot
    [
        SNew(SConstraintCanvas)
        
        // === ANNONCE DE VAGUE (centre de l'ecran) ===
        + SConstraintCanvas::Slot()
        .Anchors(FAnchors(0.5f, 0.25f, 0.5f, 0.25f))
        .Alignment(FVector2D(0.5f, 0.5f))
        .AutoSize(true)
        [
            SAssignNew(WaveAnnouncementText, STextBlock)
            .Text(FText::GetEmpty())
            .Font(BigFont)
            .ColorAndOpacity(WaveColor)
            .Justification(ETextJustify::Center)
            .ShadowOffset(FVector2D(2.0f, 2.0f))
            .ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f))
            .Visibility(EVisibility::Collapsed)
        ]
        
        // === COMPTEUR DE MOBS (bas centre, au-dessus de la hotbar) ===
        + SConstraintCanvas::Slot()
        .Anchors(FAnchors(0.5f, 1.0f, 0.5f, 1.0f))  // Centre-bas
        .Alignment(FVector2D(0.5f, 1.0f))
        .Offset(FMargin(0.0f, 0.0f, 0.0f, 120.0f))  // 120px au-dessus du bas
        .AutoSize(true)
        [
            SAssignNew(MobCounterText, STextBlock)
            .Text(FText::FromString(TEXT("Mobs: 0")))
            .Font(SmallFont)
            .ColorAndOpacity(CounterColor)
            .ShadowOffset(FVector2D(1.5f, 1.5f))
            .ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f))
            .Visibility(EVisibility::Collapsed)
        ]
    ];
    
    UE_LOG(LogTemp, Warning, TEXT("STDWaveHUD: Widget Slate construit avec Roboto!"));
}

void STDWaveHUD::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
    // Timer pour cacher l'annonce
    if (bShowingAnnouncement)
    {
        AnnouncementTimer -= InDeltaTime;
        if (AnnouncementTimer <= 0.0f)
        {
            HideWaveAnnouncement();
        }
    }
}

void STDWaveHUD::ShowWaveAnnouncement(int32 WaveNumber, int32 CreatureCount)
{
    if (WaveAnnouncementText.IsValid())
    {
        FString Message = FString::Printf(TEXT("=== VAGUE %d ===\n%d CREATURES!"), WaveNumber, CreatureCount);
        WaveAnnouncementText->SetText(FText::FromString(Message));
        WaveAnnouncementText->SetVisibility(EVisibility::Visible);
        
        bShowingAnnouncement = true;
        AnnouncementTimer = AnnouncementDuration;
        
        UE_LOG(LogTemp, Warning, TEXT("STDWaveHUD: Annonce vague %d affichee"), WaveNumber);
    }
}

void STDWaveHUD::HideWaveAnnouncement()
{
    if (WaveAnnouncementText.IsValid())
    {
        WaveAnnouncementText->SetVisibility(EVisibility::Collapsed);
    }
    bShowingAnnouncement = false;
}

void STDWaveHUD::UpdateMobCounter(int32 MobsRemaining)
{
    if (MobCounterText.IsValid())
    {
        if (MobsRemaining > 0)
        {
            FString CounterText = FString::Printf(TEXT("Mobs: %d"), MobsRemaining);
            MobCounterText->SetText(FText::FromString(CounterText));
            MobCounterText->SetVisibility(EVisibility::HitTestInvisible);
        }
        else
        {
            MobCounterText->SetVisibility(EVisibility::Collapsed);
        }
    }
}
