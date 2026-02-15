#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"

// Widget Slate pour le HUD Tower Defense (fonctionne en C++ pur)
class MONPREMIERMOD_API STDWaveHUD : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(STDWaveHUD) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    // Afficher l'annonce de vague
    void ShowWaveAnnouncement(int32 WaveNumber, int32 CreatureCount);
    
    // Cacher l'annonce
    void HideWaveAnnouncement();
    
    // Mettre a jour le compteur
    void UpdateMobCounter(int32 MobsRemaining);

    virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
    TSharedPtr<STextBlock> WaveAnnouncementText;
    TSharedPtr<STextBlock> MobCounterText;
    
    float AnnouncementTimer = 0.0f;
    float AnnouncementDuration = 4.0f;
    bool bShowingAnnouncement = false;
};
