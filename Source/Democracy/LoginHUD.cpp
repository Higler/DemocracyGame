#include "LoginHUD.h"

#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Engine/PointLight.h"
#include "DifficultyProfile.h"
#include "DemocracyGameState.h"
#include "DemocracySaveGameRuntime.h"
#include "Framework/Application/SlateApplication.h"
#include "OfficePlayerPawn.h"
#include "OfficeLevelBuilder.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/Guid.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/PlatformFilemanager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "SlateOptMacros.h"
#include "TimerManager.h"
#include "Blueprint/UserWidget.h"
#include "Components/PointLightComponent.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"

namespace
{

    class SRtsMapInputSurface : public SCompoundWidget
    {
    public:
        SLATE_BEGIN_ARGS(SRtsMapInputSurface) {}
            SLATE_EVENT(FPointerEventHandler, OnMapMouseWheel)
            SLATE_EVENT(FPointerEventHandler, OnMapMouseButtonDown)
            SLATE_EVENT(FPointerEventHandler, OnMapMouseButtonUp)
            SLATE_EVENT(FPointerEventHandler, OnMapMouseMove)
            SLATE_DEFAULT_SLOT(FArguments, Content)
        SLATE_END_ARGS()

        void Construct(const FArguments& InArgs)
        {
            OnMapMouseWheel = InArgs._OnMapMouseWheel;
            OnMapMouseButtonDown = InArgs._OnMapMouseButtonDown;
            OnMapMouseButtonUp = InArgs._OnMapMouseButtonUp;
            OnMapMouseMove = InArgs._OnMapMouseMove;
            ChildSlot
            [
                InArgs._Content.Widget
            ];
        }

        virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
        {
            return OnMapMouseWheel.IsBound() ? OnMapMouseWheel.Execute(MyGeometry, MouseEvent) : FReply::Unhandled();
        }

        virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
        {
            return OnMapMouseButtonDown.IsBound() ? OnMapMouseButtonDown.Execute(MyGeometry, MouseEvent) : FReply::Unhandled();
        }

        virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
        {
            return OnMapMouseButtonUp.IsBound() ? OnMapMouseButtonUp.Execute(MyGeometry, MouseEvent) : FReply::Unhandled();
        }

        virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
        {
            return OnMapMouseMove.IsBound() ? OnMapMouseMove.Execute(MyGeometry, MouseEvent) : FReply::Unhandled();
        }

    private:
        FPointerEventHandler OnMapMouseWheel;
        FPointerEventHandler OnMapMouseButtonDown;
        FPointerEventHandler OnMapMouseButtonUp;
        FPointerEventHandler OnMapMouseMove;
    };

    struct FPlaceholderServer
    {
        FString Name;
        FString Region;
        FString Ruleset;
        int32 PingMs;
    };

    const TArray<FPlaceholderServer>& GetPlaceholderServers()
    {
        static const TArray<FPlaceholderServer> Servers = {
            { TEXT("Civic-East-01"), TEXT("US East"), TEXT("Standard"), 28 },
            { TEXT("Civic-Central-02"), TEXT("US Central"), TEXT("Standard"), 47 },
            { TEXT("Civic-West-01"), TEXT("US West"), TEXT("Standard"), 71 },
            { TEXT("Coalition-EU-01"), TEXT("Europe"), TEXT("Standard"), 118 },
            { TEXT("Assembly-Asia-01"), TEXT("Asia Pacific"), TEXT("Standard"), 184 }
        };

        return Servers;
    }

    FText BodyText(const FString& Text)
    {
        return FText::FromString(Text);
    }

    FString PercentText(float Value)
    {
        return FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Value * 100.0f));
    }

    FString JsonEscape(FString Text)
    {
        Text.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
        Text.ReplaceInline(TEXT("\""), TEXT("\\\""));
        Text.ReplaceInline(TEXT("\r"), TEXT("\\r"));
        Text.ReplaceInline(TEXT("\n"), TEXT("\\n"));
        return Text;
    }


    struct FDuliaCountryMapPoint
    {
        int32 CountryIndex = 0;
        FVector2D Centroid = FVector2D::ZeroVector;
        FVector2D BoundsMin = FVector2D::ZeroVector;
        FVector2D BoundsMax = FVector2D::ZeroVector;
        TArray<FVector2D> Footprint;
    };


    struct FDuliaCountryIdMask
    {
        bool bLoadAttempted = false;
        bool bLoaded = false;
        int32 Width = 0;
        int32 Height = 0;
        TArray<int32> CountryIds;
    };

    FDuliaCountryIdMask& GetMutableDuliaCountryIdMask()
    {
        static FDuliaCountryIdMask Mask;
        if (Mask.bLoadAttempted)
        {
            return Mask;
        }

        Mask.bLoadAttempted = true;
        const FString MaskPath = FPaths::ProjectContentDir() / TEXT("World/Dulia/Generated/Dulia_Country_Id_Map.png");
        TArray<uint8> CompressedData;
        if (!FFileHelper::LoadFileToArray(CompressedData, *MaskPath))
        {
            UE_LOG(LogTemp, Warning, TEXT("RTS map selection could not load Dulia country ID mask: %s"), *MaskPath);
            return Mask;
        }

        IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
        const TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
        if (!ImageWrapper.IsValid() || !ImageWrapper->SetCompressed(CompressedData.GetData(), CompressedData.Num()))
        {
            UE_LOG(LogTemp, Warning, TEXT("RTS map selection could not decode Dulia country ID mask: %s"), *MaskPath);
            return Mask;
        }

        TArray64<uint8> RawPixels;
        if (!ImageWrapper->GetRaw(ERGBFormat::RGBA, 8, RawPixels))
        {
            UE_LOG(LogTemp, Warning, TEXT("RTS map selection could not read Dulia country ID mask pixels: %s"), *MaskPath);
            return Mask;
        }

        Mask.Width = ImageWrapper->GetWidth();
        Mask.Height = ImageWrapper->GetHeight();
        const int64 ExpectedBytes = static_cast<int64>(Mask.Width) * static_cast<int64>(Mask.Height) * 4;
        if (Mask.Width <= 0 || Mask.Height <= 0 || RawPixels.Num() < ExpectedBytes)
        {
            UE_LOG(LogTemp, Warning, TEXT("RTS map selection got invalid Dulia ID mask dimensions %dx%d from %s"), Mask.Width, Mask.Height, *MaskPath);
            return Mask;
        }

        Mask.CountryIds.SetNumZeroed(Mask.Width * Mask.Height);
        for (int32 PixelIndex = 0; PixelIndex < Mask.CountryIds.Num(); ++PixelIndex)
        {
            const int64 RawIndex = static_cast<int64>(PixelIndex) * 4;
            const int32 Red = RawPixels[RawIndex];
            const int32 Green = RawPixels[RawIndex + 1];
            Mask.CountryIds[PixelIndex] = Red + Green * 256;
        }

        Mask.bLoaded = true;
        UE_LOG(LogTemp, Log, TEXT("RTS map selection loaded exact Dulia country ID mask: %dx%d, %d pixels."), Mask.Width, Mask.Height, Mask.CountryIds.Num());
        return Mask;
    }

    int32 SampleDuliaCountryIdAtMapPoint(const FVector2D& MapPoint)
    {
        FDuliaCountryIdMask& Mask = GetMutableDuliaCountryIdMask();
        if (!Mask.bLoaded || Mask.Width <= 0 || Mask.Height <= 0)
        {
            return 0;
        }

        const int32 X = FMath::FloorToInt(MapPoint.X);
        const int32 Y = FMath::FloorToInt(MapPoint.Y);
        if (X < 0 || Y < 0 || X >= Mask.Width || Y >= Mask.Height)
        {
            return 0;
        }

        return Mask.CountryIds[Y * Mask.Width + X];
    }
    bool EnsureDuliaSetupCountryMapImage(int32 SelectedCountryIndex, FString& OutImagePath)
    {
        FDuliaCountryIdMask& Mask = GetMutableDuliaCountryIdMask();
        if (!Mask.bLoaded || Mask.Width <= 0 || Mask.Height <= 0 || Mask.CountryIds.Num() <= 0)
        {
            return false;
        }

        OutImagePath = FPaths::ProjectDir() / TEXT("Generated") / FString::Printf(TEXT("Dulia_Setup_Country_%03d.png"), FMath::Max(0, SelectedCountryIndex));
        if (FPaths::FileExists(OutImagePath))
        {
            return true;
        }

        IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutImagePath), true);
        TArray<uint8> RawPixels;
        RawPixels.SetNumZeroed(Mask.Width * Mask.Height * 4);

        auto WritePixel = [&RawPixels](int32 PixelIndex, uint8 R, uint8 G, uint8 B, uint8 A)
        {
            const int32 RawIndex = PixelIndex * 4;
            RawPixels[RawIndex] = R;
            RawPixels[RawIndex + 1] = G;
            RawPixels[RawIndex + 2] = B;
            RawPixels[RawIndex + 3] = A;
        };

        for (int32 Y = 0; Y < Mask.Height; ++Y)
        {
            for (int32 X = 0; X < Mask.Width; ++X)
            {
                const int32 PixelIndex = Y * Mask.Width + X;
                const int32 CountryId = Mask.CountryIds[PixelIndex];
                if (CountryId <= 0)
                {
                    WritePixel(PixelIndex, 70, 156, 176, 255);
                    continue;
                }

                const bool bSelected = SelectedCountryIndex > 0 && CountryId == SelectedCountryIndex;
                uint8 R = bSelected ? 0 : 58;
                uint8 G = bSelected ? 235 : 125;
                uint8 B = bSelected ? 72 : 205;

                const int32 LeftId = X > 0 ? Mask.CountryIds[PixelIndex - 1] : CountryId;
                const int32 RightId = X + 1 < Mask.Width ? Mask.CountryIds[PixelIndex + 1] : CountryId;
                const int32 UpId = Y > 0 ? Mask.CountryIds[PixelIndex - Mask.Width] : CountryId;
                const int32 DownId = Y + 1 < Mask.Height ? Mask.CountryIds[PixelIndex + Mask.Width] : CountryId;
                const bool bBorder = LeftId != CountryId || RightId != CountryId || UpId != CountryId || DownId != CountryId;
                if (bBorder)
                {
                    R = bSelected ? 215 : 210;
                    G = bSelected ? 255 : 214;
                    B = bSelected ? 220 : 222;
                }

                WritePixel(PixelIndex, R, G, B, 255);
            }
        }

        IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
        const TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
        if (!ImageWrapper.IsValid() || !ImageWrapper->SetRaw(RawPixels.GetData(), RawPixels.Num(), Mask.Width, Mask.Height, ERGBFormat::RGBA, 8))
        {
            return false;
        }

        const TArray64<uint8>& CompressedData64 = ImageWrapper->GetCompressed(100);
        TArray<uint8> CompressedData;
        CompressedData.Append(CompressedData64.GetData(), CompressedData64.Num());
        return FFileHelper::SaveArrayToFile(CompressedData, *OutImagePath);
    }
    bool IsRuntimeMapDemocracyType(const FString& GovernmentType)
    {
        return GovernmentType.Contains(TEXT("Democracy"), ESearchCase::IgnoreCase) ||
            GovernmentType.Contains(TEXT("Democratic"), ESearchCase::IgnoreCase) ||
            GovernmentType.Contains(TEXT("Republic"), ESearchCase::IgnoreCase) ||
            GovernmentType.Contains(TEXT("Player"), ESearchCase::IgnoreCase);
    }

    bool IsRuntimeMapDictatorshipType(const FString& GovernmentType)
    {
        return GovernmentType.Contains(TEXT("Dictatorship"), ESearchCase::IgnoreCase) ||
            GovernmentType.Contains(TEXT("Authoritarian"), ESearchCase::IgnoreCase) ||
            GovernmentType.Contains(TEXT("Autocracy"), ESearchCase::IgnoreCase) ||
            GovernmentType.Contains(TEXT("Regime"), ESearchCase::IgnoreCase) ||
            GovernmentType.Contains(TEXT("Hostile"), ESearchCase::IgnoreCase);
    }

    int32 ResolveRuntimePlayerMapCountryIndex(const FDemocracySimulationState& State)
    {
        for (const FDemocracyCountryOwnershipState& Country : State.RtsWorld.Ownership.Countries)
        {
            if (Country.MapCountryIndex > 0 && (Country.bPlayerCountry || Country.CountryName.Equals(State.PlayerCountry.CountryName, ESearchCase::IgnoreCase)))
            {
                return Country.MapCountryIndex;
            }
        }
        for (const FDemocracyContinentState& Continent : State.WorldMap.Continents)
        {
            for (const FDemocracyGeneratedCountryState& Country : Continent.Countries)
            {
                if (Country.MapCountryIndex > 0 && (Country.CountryName.Equals(State.PlayerCountry.CountryName, ESearchCase::IgnoreCase) || Country.PoliticalType.Contains(TEXT("Player"), ESearchCase::IgnoreCase)))
                {
                    return Country.MapCountryIndex;
                }
            }
        }
        return 0;
    }

    bool EnsureDuliaRuntimeGovernmentMapImage(const FDemocracySimulationState& State, FString& OutImagePath)
    {
        FDuliaCountryIdMask& Mask = GetMutableDuliaCountryIdMask();
        if (!Mask.bLoaded || Mask.Width <= 0 || Mask.Height <= 0 || Mask.CountryIds.Num() <= 0)
        {
            return false;
        }

        const int32 PlayerMapCountryIndex = ResolveRuntimePlayerMapCountryIndex(State);
        if (PlayerMapCountryIndex <= 0)
        {
            return false;
        }

        OutImagePath = FPaths::ProjectDir() / TEXT("Generated") / FString::Printf(TEXT("Dulia_Rts_Government_Player_%03d.png"), PlayerMapCountryIndex);
        if (FPaths::FileExists(OutImagePath))
        {
            return true;
        }

        TMap<int32, FColor> CountryColors;
        auto SetCountryColor = [&CountryColors, PlayerMapCountryIndex](int32 MapCountryIndex, const FString& GovernmentType, bool bPlayerCountry)
        {
            if (MapCountryIndex <= 0 || CountryColors.Contains(MapCountryIndex))
            {
                return;
            }

            if (bPlayerCountry || MapCountryIndex == PlayerMapCountryIndex)
            {
                CountryColors.Add(MapCountryIndex, FColor(0, 218, 82, 255));
            }
            else if (IsRuntimeMapDemocracyType(GovernmentType))
            {
                CountryColors.Add(MapCountryIndex, FColor(58, 125, 205, 255));
            }
            else if (IsRuntimeMapDictatorshipType(GovernmentType))
            {
                CountryColors.Add(MapCountryIndex, FColor(214, 52, 55, 255));
            }
            else
            {
                CountryColors.Add(MapCountryIndex, FColor(132, 116, 170, 255));
            }
        };

        for (const FDemocracyCountryOwnershipState& Country : State.RtsWorld.Ownership.Countries)
        {
            SetCountryColor(Country.MapCountryIndex, Country.GovernmentType, Country.bPlayerCountry || Country.CountryName.Equals(State.PlayerCountry.CountryName, ESearchCase::IgnoreCase));
        }
        for (const FDemocracyContinentState& Continent : State.WorldMap.Continents)
        {
            for (const FDemocracyGeneratedCountryState& Country : Continent.Countries)
            {
                SetCountryColor(Country.MapCountryIndex, Country.PoliticalType, Country.MapCountryIndex == PlayerMapCountryIndex || Country.CountryName.Equals(State.PlayerCountry.CountryName, ESearchCase::IgnoreCase));
            }
        }

        IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutImagePath), true);
        TArray<uint8> RawPixels;
        RawPixels.SetNumZeroed(Mask.Width * Mask.Height * 4);

        auto WritePixel = [&RawPixels](int32 PixelIndex, uint8 R, uint8 G, uint8 B, uint8 A)
        {
            const int32 RawIndex = PixelIndex * 4;
            RawPixels[RawIndex] = R;
            RawPixels[RawIndex + 1] = G;
            RawPixels[RawIndex + 2] = B;
            RawPixels[RawIndex + 3] = A;
        };

        for (int32 Y = 0; Y < Mask.Height; ++Y)
        {
            for (int32 X = 0; X < Mask.Width; ++X)
            {
                const int32 PixelIndex = Y * Mask.Width + X;
                const int32 CountryId = Mask.CountryIds[PixelIndex];
                if (CountryId <= 0)
                {
                    WritePixel(PixelIndex, 70, 156, 176, 255);
                    continue;
                }

                FColor Color = CountryColors.Contains(CountryId) ? CountryColors[CountryId] : FColor(132, 116, 170, 255);
                const int32 LeftId = X > 0 ? Mask.CountryIds[PixelIndex - 1] : CountryId;
                const int32 RightId = X + 1 < Mask.Width ? Mask.CountryIds[PixelIndex + 1] : CountryId;
                const int32 UpId = Y > 0 ? Mask.CountryIds[PixelIndex - Mask.Width] : CountryId;
                const int32 DownId = Y + 1 < Mask.Height ? Mask.CountryIds[PixelIndex + Mask.Width] : CountryId;
                const bool bBorder = LeftId != CountryId || RightId != CountryId || UpId != CountryId || DownId != CountryId;
                if (bBorder)
                {
                    Color = FColor(
                        FMath::Clamp(static_cast<int32>(Color.R) + 110, 0, 255),
                        FMath::Clamp(static_cast<int32>(Color.G) + 96, 0, 255),
                        FMath::Clamp(static_cast<int32>(Color.B) + 90, 0, 255),
                        255);
                }

                WritePixel(PixelIndex, Color.R, Color.G, Color.B, 255);
            }
        }

        IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
        const TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
        if (!ImageWrapper.IsValid() || !ImageWrapper->SetRaw(RawPixels.GetData(), RawPixels.Num(), Mask.Width, Mask.Height, ERGBFormat::RGBA, 8))
        {
            return false;
        }

        const TArray64<uint8>& CompressedData64 = ImageWrapper->GetCompressed(100);
        TArray<uint8> CompressedData;
        CompressedData.Append(CompressedData64.GetData(), CompressedData64.Num());
        return FFileHelper::SaveArrayToFile(CompressedData, *OutImagePath);
    }
    bool DuliaPointInPolygon(const FVector2D& Point, const TArray<FVector2D>& Polygon)
    {
        if (Polygon.Num() < 3)
        {
            return false;
        }

        bool bInside = false;
        for (int32 Index = 0, PreviousIndex = Polygon.Num() - 1; Index < Polygon.Num(); PreviousIndex = Index++)
        {
            const FVector2D& A = Polygon[Index];
            const FVector2D& B = Polygon[PreviousIndex];
            const bool bCrossesY = (A.Y > Point.Y) != (B.Y > Point.Y);
            if (bCrossesY)
            {
                const float Denom = B.Y - A.Y;
                if (FMath::IsNearlyZero(Denom))
                {
                    continue;
                }

                const float CrossX = (B.X - A.X) * (Point.Y - A.Y) / Denom + A.X;
                if (Point.X < CrossX)
                {
                    bInside = !bInside;
                }
            }
        }
        return bInside;
    }

    TArray<FVector2D> BuildDuliaFootprintFromBounds(const FVector2D& BoundsMin, const FVector2D& BoundsMax, const FVector2D& Centroid, int32 CountryIndex)
    {
        const float Width = FMath::Max(12.0f, BoundsMax.X - BoundsMin.X);
        const float Height = FMath::Max(12.0f, BoundsMax.Y - BoundsMin.Y);
        const float Expand = FMath::Clamp(FMath::Min(Width, Height) * 0.22f, 4.0f, 22.0f);
        const FVector2D Min(BoundsMin.X - Expand, BoundsMin.Y - Expand);
        const FVector2D Max(BoundsMax.X + Expand, BoundsMax.Y + Expand);
        const float Skew = (CountryIndex % 5 - 2) * FMath::Min(Expand * 0.45f, 6.0f);
        TArray<FVector2D> Points;
        Points.Add(FVector2D((Min.X + Max.X) * 0.5f + Skew, Min.Y));
        Points.Add(FVector2D(Max.X, Min.Y + Height * 0.25f));
        Points.Add(FVector2D(Max.X - Expand * 0.15f, (Min.Y + Max.Y) * 0.5f));
        Points.Add(FVector2D(Max.X, Max.Y - Height * 0.25f));
        Points.Add(FVector2D((Min.X + Max.X) * 0.5f - Skew, Max.Y));
        Points.Add(FVector2D(Min.X, Max.Y - Height * 0.25f));
        Points.Add(FVector2D(Min.X + Expand * 0.15f, (Min.Y + Max.Y) * 0.5f));
        Points.Add(FVector2D(Min.X, Min.Y + Height * 0.25f));
        return Points;
    }

    const TArray<FDuliaCountryMapPoint>& GetDuliaCountryMapPoints()
    {
        static bool bLoaded = false;
        static TArray<FDuliaCountryMapPoint> Points;
        if (bLoaded)
        {
            return Points;
        }

        bLoaded = true;
        const FString ManifestPath = FPaths::ProjectContentDir() / TEXT("World/Dulia/Generated/Dulia_Country_Manifest.json");
        FString ManifestJson;
        if (!FFileHelper::LoadFileToString(ManifestJson, *ManifestPath))
        {
            UE_LOG(LogTemp, Warning, TEXT("RTS map selection could not load Dulia manifest: %s"), *ManifestPath);
            return Points;
        }

        TSharedPtr<FJsonObject> RootObject;
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ManifestJson);
        if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
        {
            UE_LOG(LogTemp, Warning, TEXT("RTS map selection could not parse Dulia manifest: %s"), *ManifestPath);
            return Points;
        }

        const TArray<TSharedPtr<FJsonValue>>* Countries = nullptr;
        if (!RootObject->TryGetArrayField(TEXT("countries"), Countries) || Countries == nullptr)
        {
            return Points;
        }

        for (const TSharedPtr<FJsonValue>& CountryValue : *Countries)
        {
            const TSharedPtr<FJsonObject> CountryObject = CountryValue.IsValid() ? CountryValue->AsObject() : nullptr;
            if (!CountryObject.IsValid())
            {
                continue;
            }

            const TSharedPtr<FJsonObject>* CentroidObject = nullptr;
            if (!CountryObject->TryGetObjectField(TEXT("centroid"), CentroidObject) || CentroidObject == nullptr || !CentroidObject->IsValid())
            {
                continue;
            }

            FDuliaCountryMapPoint Point;
            Point.CountryIndex = FMath::RoundToInt(CountryObject->GetNumberField(TEXT("id")));
            Point.Centroid.X = (*CentroidObject)->GetNumberField(TEXT("x"));
            Point.Centroid.Y = (*CentroidObject)->GetNumberField(TEXT("y"));

            const TSharedPtr<FJsonObject>* BoundsObject = nullptr;
            if (CountryObject->TryGetObjectField(TEXT("bounds"), BoundsObject) && BoundsObject != nullptr && BoundsObject->IsValid())
            {
                Point.BoundsMin.X = (*BoundsObject)->GetNumberField(TEXT("minX"));
                Point.BoundsMin.Y = (*BoundsObject)->GetNumberField(TEXT("minY"));
                Point.BoundsMax.X = (*BoundsObject)->GetNumberField(TEXT("maxX"));
                Point.BoundsMax.Y = (*BoundsObject)->GetNumberField(TEXT("maxY"));
            }
            else
            {
                Point.BoundsMin = Point.Centroid - FVector2D(18.0f, 18.0f);
                Point.BoundsMax = Point.Centroid + FVector2D(18.0f, 18.0f);
            }
            Point.Footprint = BuildDuliaFootprintFromBounds(Point.BoundsMin, Point.BoundsMax, Point.Centroid, Point.CountryIndex);
            if (Point.CountryIndex > 0)
            {
                Points.Add(Point);
            }
        }

        return Points;
    }
    TArray<FVector2D> BuildDuliaProvincePolygon(const FVector2D& Center, float RadiusX, float RadiusY, int32 Seed)
    {
        TArray<FVector2D> Polygon;
        for (int32 Index = 0; Index < 6; ++Index)
        {
            const float Angle = (2.0f * PI * Index / 6.0f) + (Seed % 4) * 0.08f;
            const float Shape = 0.86f + 0.12f * ((Seed + Index) % 3);
            Polygon.Add(FVector2D(
                FMath::Clamp(Center.X + FMath::Cos(Angle) * RadiusX * Shape, 0.0f, 2242.0f),
                FMath::Clamp(Center.Y + FMath::Sin(Angle) * RadiusY * Shape, 0.0f, 1104.0f)));
        }
        return Polygon;
    }

    void AssignRuntimeProvinceGeometry(FDemocracyProvinceOwnershipState& Province, const FDemocracyCountryOwnershipState& Country, int32 ProvinceCount)
    {
        const FDuliaCountryMapPoint* CountryPoint = nullptr;
        for (const FDuliaCountryMapPoint& Point : GetDuliaCountryMapPoints())
        {
            if (Point.CountryIndex == Country.MapCountryIndex)
            {
                CountryPoint = &Point;
                break;
            }
        }
        if (!CountryPoint)
        {
            return;
        }

        const float Width = FMath::Max(18.0f, CountryPoint->BoundsMax.X - CountryPoint->BoundsMin.X);
        const float Height = FMath::Max(18.0f, CountryPoint->BoundsMax.Y - CountryPoint->BoundsMin.Y);
        const int32 RingIndex = Province.ProvinceIndex - 1;
        const float Angle = ProvinceCount <= 1 ? 0.0f : (2.0f * PI * RingIndex / ProvinceCount) + (Country.MapCountryIndex % 7) * 0.07f;
        const float Distance = Province.ProvinceIndex == 1 ? 0.0f : FMath::Clamp(FMath::Min(Width, Height) * 0.28f, 12.0f, 48.0f);
        const FVector2D Center(
            FMath::Clamp(CountryPoint->Centroid.X + FMath::Cos(Angle) * Distance, CountryPoint->BoundsMin.X, CountryPoint->BoundsMax.X),
            FMath::Clamp(CountryPoint->Centroid.Y + FMath::Sin(Angle) * Distance, CountryPoint->BoundsMin.Y, CountryPoint->BoundsMax.Y));
        Province.MapCenterX = Center.X;
        Province.MapCenterY = Center.Y;
        Province.MapRadiusX = FMath::Clamp(Width / FMath::Max(2.4f, ProvinceCount * 0.72f), 10.0f, 46.0f);
        Province.MapRadiusY = FMath::Clamp(Height / FMath::Max(2.4f, ProvinceCount * 0.72f), 10.0f, 42.0f);
        Province.GeometrySource = TEXT("dulia-generated-province-polygon-v1");
        Province.MapPolygon = BuildDuliaProvincePolygon(Center, Province.MapRadiusX, Province.MapRadiusY, Country.MapCountryIndex + Province.ProvinceIndex * 17);
    }

    bool IsPointInsideProvinceGeometry(const FDemocracyProvinceOwnershipState& Province, const FVector2D& MapPoint)
    {
        if (Province.MapPolygon.Num() >= 3 && DuliaPointInPolygon(MapPoint, Province.MapPolygon))
        {
            return true;
        }
        if (Province.MapRadiusX > 0.0f && Province.MapRadiusY > 0.0f)
        {
            const float NormalizedX = (MapPoint.X - Province.MapCenterX) / Province.MapRadiusX;
            const float NormalizedY = (MapPoint.Y - Province.MapCenterY) / Province.MapRadiusY;
            return NormalizedX * NormalizedX + NormalizedY * NormalizedY <= 1.0f;
        }
        return false;
    }

    bool EnsureRuntimeProvinceGeometry(FDemocracyMapOwnershipState& Ownership)
    {
        bool bChanged = false;
        for (const FDemocracyCountryOwnershipState& Country : Ownership.Countries)
        {
            const int32 ProvinceCount = FMath::Max(1, Country.ProvinceIds.Num());
            for (FDemocracyProvinceOwnershipState& Province : Ownership.Provinces)
            {
                if (!Country.ProvinceIds.Contains(Province.ProvinceId))
                {
                    continue;
                }
                if (Province.MapPolygon.Num() < 3 || Province.MapCenterX <= 0.0f || Province.MapCenterY <= 0.0f)
                {
                    AssignRuntimeProvinceGeometry(Province, Country, ProvinceCount);
                    bChanged = true;
                }
            }
        }
        return bChanged;
    }

    FString SelectDuliaProvinceIdAtMapPoint(const FDemocracyMapOwnershipState& Ownership, const FDemocracyCountryOwnershipState& Country, const FVector2D& MapPoint, bool& bExactProvinceHit)
    {
        bExactProvinceHit = false;
        if (Country.ProvinceIds.Num() == 0)
        {
            return TEXT("");
        }

        float BestDistanceSq = TNumericLimits<float>::Max();
        FString BestProvinceId = Country.ProvinceIds[0];
        for (const FString& ProvinceId : Country.ProvinceIds)
        {
            const FDemocracyProvinceOwnershipState* Province = nullptr;
            for (const FDemocracyProvinceOwnershipState& CandidateProvince : Ownership.Provinces)
            {
                if (CandidateProvince.ProvinceId.Equals(ProvinceId, ESearchCase::IgnoreCase))
                {
                    Province = &CandidateProvince;
                    break;
                }
            }
            if (!Province)
            {
                continue;
            }
            if (IsPointInsideProvinceGeometry(*Province, MapPoint))
            {
                bExactProvinceHit = true;
                return Province->ProvinceId;
            }
            const float DistanceSq = FVector2D::DistSquared(MapPoint, FVector2D(Province->MapCenterX, Province->MapCenterY));
            if (DistanceSq < BestDistanceSq)
            {
                BestDistanceSq = DistanceSq;
                BestProvinceId = Province->ProvinceId;
            }
        }
        return BestProvinceId;
    }
    FString SaveTimestamp()
    {
        return FDateTime::UtcNow().ToIso8601();
    }
}


    struct FPolicyTickModifiers
    {
        int32 ApprovalDelta = 0;
        int32 StabilityDelta = 0;
        int32 UnrestDelta = 0;
        int32 TreasuryDelta = 0;
        int32 EconomicDelta = 0;
        int32 DiplomacyDelta = 0;
        int32 MilitaryDelta = 0;
        int32 InfrastructureDelta = 0;
        int32 EnvironmentDelta = 0;
        int32 FoodDelta = 0;
        int32 GasOilDelta = 0;
        int32 WoodDelta = 0;
        int32 MetalsDelta = 0;
        int32 WaterDelta = 0;
        int32 InvasionRiskDelta = 0;
    };

    void AddPolicyEffect(FPolicyTickModifiers& Modifiers, TArray<FString>& Effects, const FString& EffectText,
        int32 Approval, int32 Stability, int32 Unrest, int32 Treasury, int32 Economy, int32 Diplomacy,
        int32 Military, int32 Infrastructure, int32 Environment, int32 Food, int32 GasOil, int32 Wood, int32 Metals, int32 Water, int32 InvasionRisk)
    {
        Effects.Add(EffectText);
        Modifiers.ApprovalDelta += Approval;
        Modifiers.StabilityDelta += Stability;
        Modifiers.UnrestDelta += Unrest;
        Modifiers.TreasuryDelta += Treasury;
        Modifiers.EconomicDelta += Economy;
        Modifiers.DiplomacyDelta += Diplomacy;
        Modifiers.MilitaryDelta += Military;
        Modifiers.InfrastructureDelta += Infrastructure;
        Modifiers.EnvironmentDelta += Environment;
        Modifiers.FoodDelta += Food;
        Modifiers.GasOilDelta += GasOil;
        Modifiers.WoodDelta += Wood;
        Modifiers.MetalsDelta += Metals;
        Modifiers.WaterDelta += Water;
        Modifiers.InvasionRiskDelta += InvasionRisk;
    }

    FPolicyTickModifiers BuildPolicyModifiers(const FDemocracyPolicyState& Policies, TArray<FString>* OutEffects = nullptr)
    {
        FPolicyTickModifiers Modifiers;
        TArray<FString> Effects;

        if (Policies.EconomicPolicy.Equals(TEXT("Stimulus Spending"), ESearchCase::IgnoreCase))
        {
            AddPolicyEffect(Modifiers, Effects, TEXT("Stimulus Spending: approval/economy/infrastructure rise, treasury burns faster."), 2, 1, -1, -12, 3, 0, 0, 1, 0, 2, 0, 0, 0, 1, 0);
        }
        else if (Policies.EconomicPolicy.Equals(TEXT("Austerity Program"), ESearchCase::IgnoreCase))
        {
            AddPolicyEffect(Modifiers, Effects, TEXT("Austerity Program: treasury improves, approval and stability suffer."), -3, -1, 2, 16, -2, 0, 0, -1, 0, -1, 0, 0, 0, 0, 0);
        }
        else if (Policies.EconomicPolicy.Equals(TEXT("Industrial Subsidies"), ESearchCase::IgnoreCase))
        {
            AddPolicyEffect(Modifiers, Effects, TEXT("Industrial Subsidies: economy and materials improve, treasury and environment decline."), 0, 0, 0, -8, 3, 0, 0, 1, -2, 0, 1, 2, 3, 0, 0);
        }
        else
        {
            AddPolicyEffect(Modifiers, Effects, TEXT("Balanced Budget: modest treasury gain with low public impact."), 0, 0, 0, 6, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        }

        if (Policies.EnvironmentalPolicy.Equals(TEXT("Conservation Mandate"), ESearchCase::IgnoreCase))
        {
            AddPolicyEffect(Modifiers, Effects, TEXT("Conservation Mandate: environmental health and stability improve, extraction slows."), 1, 1, -1, -4, 0, 1, 0, 0, 3, 1, -1, -2, -2, 2, 0);
        }
        else if (Policies.EnvironmentalPolicy.Equals(TEXT("Extraction Expansion"), ESearchCase::IgnoreCase))
        {
            AddPolicyEffect(Modifiers, Effects, TEXT("Extraction Expansion: resources and treasury improve, environment and unrest worsen."), -1, -1, 2, 8, 2, -1, 0, 0, -4, 0, 3, 4, 4, -1, 0);
        }
        else
        {
            AddPolicyEffect(Modifiers, Effects, TEXT("Managed Development: balanced resource output and environmental pressure."), 0, 0, 0, 2, 1, 0, 0, 0, -1, 1, 1, 1, 1, 0, 0);
        }

        if (Policies.MilitaryPolicy.Equals(TEXT("National Mobilization"), ESearchCase::IgnoreCase))
        {
            AddPolicyEffect(Modifiers, Effects, TEXT("National Mobilization: readiness rises, but treasury, approval, and unrest worsen."), -2, 0, 2, -10, -1, -1, 5, 0, 0, -1, -2, 0, -2, 0, -1);
        }
        else if (Policies.MilitaryPolicy.Equals(TEXT("Demilitarization"), ESearchCase::IgnoreCase))
        {
            AddPolicyEffect(Modifiers, Effects, TEXT("Demilitarization: approval, treasury, and diplomacy rise, military risk increases."), 2, 0, -1, 8, 0, 3, -4, 0, 0, 0, 1, 0, 1, 0, 3);
        }
        else
        {
            AddPolicyEffect(Modifiers, Effects, TEXT("Defensive Readiness: maintains military readiness without provoking rivals."), 0, 1, 0, -3, 0, 1, 2, 0, 0, 0, -1, 0, -1, 0, -1);
        }

        if (Policies.DiplomacyPolicy.Equals(TEXT("Alliance Outreach"), ESearchCase::IgnoreCase))
        {
            AddPolicyEffect(Modifiers, Effects, TEXT("Alliance Outreach: diplomacy and invasion safety improve, treasury cost rises."), 1, 1, -1, -6, 1, 4, 0, 0, 0, 0, 0, 0, 0, 0, -3);
        }
        else if (Policies.DiplomacyPolicy.Equals(TEXT("Hardline Sovereignty"), ESearchCase::IgnoreCase))
        {
            AddPolicyEffect(Modifiers, Effects, TEXT("Hardline Sovereignty: readiness and domestic approval rise, diplomacy and invasion risk worsen."), 1, 0, 1, 0, 0, -4, 2, 0, 0, 0, -1, 0, -1, 0, 3);
        }
        else
        {
            AddPolicyEffect(Modifiers, Effects, TEXT("Neutral Engagement: stable diplomacy with no strong alliance push."), 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        }

        if (Policies.CivilPolicy.Equals(TEXT("Civil Liberties"), ESearchCase::IgnoreCase))
        {
            AddPolicyEffect(Modifiers, Effects, TEXT("Civil Liberties: approval and stability improve while unrest cools slowly."), 3, 1, -2, -3, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        }
        else if (Policies.CivilPolicy.Equals(TEXT("Emergency Powers"), ESearchCase::IgnoreCase))
        {
            AddPolicyEffect(Modifiers, Effects, TEXT("Emergency Powers: unrest drops immediately, but approval, diplomacy, and legitimacy suffer."), -4, 2, -4, -4, -1, -3, 1, 0, 0, 0, 0, 0, 0, 0, 1);
        }
        else
        {
            AddPolicyEffect(Modifiers, Effects, TEXT("Public Stability: small stability support with low civil backlash."), 0, 1, -1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        }

        if (OutEffects)
        {
            *OutEffects = Effects;
        }
        return Modifiers;
    }

    FString JoinPolicyEffects(const FDemocracyPolicyState& Policies)
    {
        TArray<FString> Effects;
        BuildPolicyModifiers(Policies, &Effects);
        return FString::Join(Effects, TEXT("\n"));
    }


    FDemocracyResourceChainEntry MakeResourceChainEntry(const FString& Name, int32 Reserve, int32 ReserveTarget, int32 Production, int32 Consumption, int32 Imports, int32 Exports, int32 StrategicValue, const FString& Role, const TArray<FString>& Drivers)
    {
        FDemocracyResourceChainEntry Entry;
        Entry.ResourceName = Name;
        Entry.Production = FMath::Max(0, Production);
        Entry.Consumption = FMath::Max(0, Consumption);
        Entry.Imports = FMath::Max(0, Imports);
        Entry.Exports = FMath::Max(0, Exports);
        Entry.Reserve = FMath::Max(0, Reserve);
        Entry.ReserveTarget = FMath::Max(1, ReserveTarget);
        Entry.Shortage = FMath::Max(0, Entry.ReserveTarget - Entry.Reserve);
        Entry.Surplus = FMath::Max(0, Entry.Reserve - Entry.ReserveTarget);
        Entry.StrategicValue = FMath::Clamp(StrategicValue, 0, 100);
        Entry.Role = Role;
        Entry.Drivers = Drivers;
        Entry.Status = Entry.Shortage > 0
            ? FString::Printf(TEXT("Shortage %d below reserve target %d."), Entry.Shortage, Entry.ReserveTarget)
            : FString::Printf(TEXT("Stable reserve %d with surplus %d over target."), Entry.Reserve, Entry.Surplus);
        return Entry;
    }

    int32 GetResourceChainShortage(const FDemocracyResourceProductionChainState& ResourceChains, const FString& ResourceName)
    {
        for (const FDemocracyResourceChainEntry& Entry : ResourceChains.Chains)
        {
            if (Entry.ResourceName.Equals(ResourceName, ESearchCase::IgnoreCase))
            {
                return Entry.Shortage;
            }
        }
        return 0;
    }

    FString BuildResourceChainSummaryText(const FDemocracyResourceProductionChainState& ResourceChains)
    {
        TArray<FString> Lines;
        Lines.Add(FString::Printf(TEXT("Shortage pressure %d | trade balance %+d | updated turn %d"), ResourceChains.TotalShortagePressure, ResourceChains.TradeBalance, ResourceChains.LastUpdatedTurn));
        Lines.Add(ResourceChains.Summary);
        for (const FDemocracyResourceChainEntry& Entry : ResourceChains.Chains)
        {
            Lines.Add(FString::Printf(TEXT("%s: reserve %d/%d | production %d | consumption %d | imports %d | exports %d | shortage %d | %s"),
                *Entry.ResourceName,
                Entry.Reserve,
                Entry.ReserveTarget,
                Entry.Production,
                Entry.Consumption,
                Entry.Imports,
                Entry.Exports,
                Entry.Shortage,
                *Entry.Role));
        }
        return FString::Join(Lines, TEXT("\n"));
    }

    void RecalculateResourceProductionChains(FDemocracySimulationState& State, const FPolicyTickModifiers& PolicyModifiers, int32 FoodUse, int32 WaterUse, int32 GasUse, int32 WoodUse, int32 MetalsUse)
    {
        FDemocracyCountryState& Country = State.PlayerCountry;
        FDemocracyResourceInventory& Resources = Country.Resources;
        FDemocracyEconomyBudgetState& Budget = State.EconomyBudget;
        const int32 DifficultyScore = FMath::Clamp(Country.CountrySizeScore, 1, 4);
        const int32 ProductionBonus = FMath::Clamp(Budget.ProductionEfficiency / 16, 0, 7);
        const int32 ServicesBonus = FMath::Clamp(Budget.PublicServices / 30, 0, 4);
        const int32 ClimateFoodBonus = Country.Climate.Equals(TEXT("Southern Tropical"), ESearchCase::IgnoreCase) ? 3 : (Country.Climate.Equals(TEXT("Northern Cold"), ESearchCase::IgnoreCase) ? -1 : 1);
        const int32 ClimateWaterBonus = Country.Climate.Equals(TEXT("Northern Cold"), ESearchCase::IgnoreCase) ? 2 : (Country.Climate.Equals(TEXT("Southern Tropical"), ESearchCase::IgnoreCase) ? 1 : 0);
        const int32 TradeCapacity = FMath::Clamp(Country.DiplomaticStanding / 18 + Country.Treasury / 450, 0, 10);
        const int32 ExportCapacity = FMath::Clamp(Country.DiplomaticStanding / 28 + Budget.ProductionEfficiency / 35, 0, 6);

        const int32 FoodTarget = 150 + DifficultyScore * 18;
        const int32 WaterTarget = 130 + DifficultyScore * 15;
        const int32 FuelTarget = 80 + DifficultyScore * 14;
        const int32 WoodTarget = 70 + DifficultyScore * 10;
        const int32 MetalsTarget = 75 + DifficultyScore * 12;

        const int32 FoodProduction = FMath::Max(0, Country.Infrastructure / 12 + Country.EnvironmentalHealth / 25 + ProductionBonus + ClimateFoodBonus + PolicyModifiers.FoodDelta);
        const int32 WaterProduction = FMath::Max(0, Country.Infrastructure / 18 + ServicesBonus + ClimateWaterBonus + PolicyModifiers.WaterDelta);
        const int32 FuelProduction = FMath::Max(0, Budget.ProductionEfficiency / 18 + PolicyModifiers.GasOilDelta + (Country.Policies.EnvironmentalPolicy.Equals(TEXT("Extraction Expansion"), ESearchCase::IgnoreCase) ? 3 : 0));
        const int32 WoodProduction = FMath::Max(0, Country.EnvironmentalHealth / 20 + Budget.InfrastructureSpending / 20 + PolicyModifiers.WoodDelta);
        const int32 MetalsProduction = FMath::Max(0, Budget.ProductionEfficiency / 20 + Country.Infrastructure / 30 + PolicyModifiers.MetalsDelta + (Country.Policies.EconomicPolicy.Equals(TEXT("Industrial Subsidies"), ESearchCase::IgnoreCase) ? 2 : 0));

        const int32 FuelConsumption = GasUse + Budget.DefenseSpending / 18 + Budget.InfrastructureSpending / 35;
        const int32 WoodConsumption = WoodUse + Budget.InfrastructureSpending / 16;
        const int32 MetalsConsumption = MetalsUse + Budget.DefenseSpending / 16 + Budget.InfrastructureSpending / 22;

        const int32 FoodImports = Resources.Food < FoodTarget ? FMath::Clamp(TradeCapacity, 0, 8) : 0;
        const int32 WaterImports = Resources.Water < WaterTarget ? FMath::Clamp(TradeCapacity / 2, 0, 5) : 0;
        const int32 FuelImports = Resources.GasOil < FuelTarget ? FMath::Clamp(TradeCapacity, 0, 9) : 0;
        const int32 WoodImports = Resources.Wood < WoodTarget ? FMath::Clamp(TradeCapacity / 2, 0, 5) : 0;
        const int32 MetalsImports = Resources.Metals < MetalsTarget ? FMath::Clamp(TradeCapacity / 2 + 1, 0, 6) : 0;

        const int32 FoodExports = Resources.Food > FoodTarget + 90 ? FMath::Clamp(ExportCapacity, 0, 5) : 0;
        const int32 WaterExports = Resources.Water > WaterTarget + 80 ? FMath::Clamp(ExportCapacity / 2, 0, 3) : 0;
        const int32 FuelExports = Resources.GasOil > FuelTarget + 70 ? FMath::Clamp(ExportCapacity, 0, 5) : 0;
        const int32 WoodExports = Resources.Wood > WoodTarget + 60 ? FMath::Clamp(ExportCapacity / 2, 0, 4) : 0;
        const int32 MetalsExports = Resources.Metals > MetalsTarget + 60 ? FMath::Clamp(ExportCapacity / 2, 0, 4) : 0;

        Resources.Food = FMath::Max(0, Resources.Food + FoodProduction + FoodImports - FoodUse - FoodExports);
        Resources.Water = FMath::Max(0, Resources.Water + WaterProduction + WaterImports - WaterUse - WaterExports);
        Resources.GasOil = FMath::Max(0, Resources.GasOil + FuelProduction + FuelImports - FuelConsumption - FuelExports);
        Resources.Wood = FMath::Max(0, Resources.Wood + WoodProduction + WoodImports - WoodConsumption - WoodExports);
        Resources.Metals = FMath::Max(0, Resources.Metals + MetalsProduction + MetalsImports - MetalsConsumption - MetalsExports);

        State.ResourceChains.Chains.Reset();
        State.ResourceChains.Chains.Add(MakeResourceChainEntry(TEXT("Food"), Resources.Food, FoodTarget, FoodProduction, FoodUse, FoodImports, FoodExports, 90, TEXT("Population food access; shortages raise unrest fastest."), { TEXT("agriculture"), TEXT("climate"), TEXT("infrastructure"), TEXT("food imports") }));
        State.ResourceChains.Chains.Add(MakeResourceChainEntry(TEXT("Water"), Resources.Water, WaterTarget, WaterProduction, WaterUse, WaterImports, WaterExports, 95, TEXT("Public health and regional stability; shortages damage demographics."), { TEXT("climate"), TEXT("public services"), TEXT("water access") }));
        State.ResourceChains.Chains.Add(MakeResourceChainEntry(TEXT("Fuel"), Resources.GasOil, FuelTarget, FuelProduction, FuelConsumption, FuelImports, FuelExports, 100, TEXT("Logistics, industry, military readiness, and inflation pressure."), { TEXT("extraction"), TEXT("trade"), TEXT("defense demand") }));
        State.ResourceChains.Chains.Add(MakeResourceChainEntry(TEXT("Wood"), Resources.Wood, WoodTarget, WoodProduction, WoodConsumption, WoodImports, WoodExports, 60, TEXT("Construction, infrastructure repairs, and disaster recovery."), { TEXT("forestry"), TEXT("infrastructure spending"), TEXT("environment") }));
        State.ResourceChains.Chains.Add(MakeResourceChainEntry(TEXT("Metals"), Resources.Metals, MetalsTarget, MetalsProduction, MetalsConsumption, MetalsImports, MetalsExports, 85, TEXT("Industry, infrastructure projects, and military production."), { TEXT("mining"), TEXT("industrial policy"), TEXT("defense spending") }));

        int32 WeightedShortage = 0;
        for (const FDemocracyResourceChainEntry& Entry : State.ResourceChains.Chains)
        {
            WeightedShortage += (Entry.Shortage * Entry.StrategicValue) / FMath::Max(1, Entry.ReserveTarget);
        }
        State.ResourceChains.TotalShortagePressure = FMath::Clamp(WeightedShortage, 0, 100);

        const int32 ImportCost = FoodImports * 5 + WaterImports * 4 + FuelImports * 8 + WoodImports * 4 + MetalsImports * 7;
        const int32 ExportIncome = FoodExports * 4 + WaterExports * 2 + FuelExports * 7 + WoodExports * 3 + MetalsExports * 6;
        State.ResourceChains.TradeBalance = ExportIncome - ImportCost;
        Country.Treasury = FMath::Max(0, Country.Treasury + State.ResourceChains.TradeBalance);
        State.ResourceChains.LastUpdatedTurn = State.Turn;
        State.ResourceChains.Summary = FString::Printf(TEXT("Production chains updated. Imports cost %d, exports earned %d, net trade %+d."), ImportCost, ExportIncome, State.ResourceChains.TradeBalance);
    }
    
    void ApplyResourceShortageEffects(FDemocracySimulationState& State)
    {
        FDemocracyCountryState& Country = State.PlayerCountry;
        FDemocracyEconomyBudgetState& Budget = State.EconomyBudget;
        const int32 FoodShortage = GetResourceChainShortage(State.ResourceChains, TEXT("Food"));
        const int32 FuelShortage = GetResourceChainShortage(State.ResourceChains, TEXT("Fuel"));
        const int32 WoodShortage = GetResourceChainShortage(State.ResourceChains, TEXT("Wood"));
        const int32 MetalsShortage = GetResourceChainShortage(State.ResourceChains, TEXT("Metals"));
        const int32 TotalMaterialShortage = WoodShortage + MetalsShortage;

        const int32 FoodUnrest = FMath::Clamp(FoodShortage / 28, 0, 6);
        const int32 FuelReadinessLoss = FMath::Clamp(FuelShortage / 22, 0, 7);
        const int32 FuelProductionPenalty = FMath::Clamp(FuelShortage / 28, 0, 6);
        const int32 WoodInfrastructurePenalty = FMath::Clamp(WoodShortage / 35, 0, 4);
        const int32 MetalsReadinessLoss = FMath::Clamp(MetalsShortage / 30, 0, 5);
        const int32 MaterialProductionPenalty = FMath::Clamp(TotalMaterialShortage / 45, 0, 6);
        const int32 ShortageInflation = FMath::Clamp((FoodShortage + FuelShortage + TotalMaterialShortage / 2) / 75, 0, 5);

        if (FoodUnrest <= 0 && FuelReadinessLoss <= 0 && FuelProductionPenalty <= 0 && WoodInfrastructurePenalty <= 0 && MetalsReadinessLoss <= 0 && MaterialProductionPenalty <= 0 && ShortageInflation <= 0)
        {
            return;
        }

        Country.PublicApproval = FMath::Clamp(Country.PublicApproval - FoodUnrest - FMath::Clamp(FuelShortage / 45, 0, 3), 0, 100);
        Country.Unrest = FMath::Clamp(Country.Unrest + FoodUnrest + FMath::Clamp(State.ResourceChains.TotalShortagePressure / 30, 0, 3), 0, 100);
        Country.Stability = FMath::Clamp(Country.Stability - FMath::Clamp(FoodShortage / 45 + TotalMaterialShortage / 90, 0, 5), 0, 100);
        Country.MilitaryReadiness = FMath::Clamp(Country.MilitaryReadiness - FuelReadinessLoss - MetalsReadinessLoss, 0, 100);
        Country.Infrastructure = FMath::Clamp(Country.Infrastructure - WoodInfrastructurePenalty, 0, 100);
        Country.EconomicHealth = FMath::Clamp(Country.EconomicHealth - FuelProductionPenalty - MaterialProductionPenalty, 0, 100);
        Budget.ProductionEfficiency = FMath::Clamp(Budget.ProductionEfficiency - FuelProductionPenalty - MaterialProductionPenalty, 0, 100);
        Budget.Inflation = FMath::Clamp(Budget.Inflation + ShortageInflation, 0, 45);
        State.FailureRisk.CurrentAssassinationRisk = FMath::Clamp(State.FailureRisk.CurrentAssassinationRisk + FoodUnrest / 2 + FMath::Clamp(State.ResourceChains.TotalShortagePressure / 45, 0, 3), 0, State.FailureRisk.AssassinationRiskTrigger);
        State.InvasionRisk.CurrentInvasionRisk = FMath::Clamp(State.InvasionRisk.CurrentInvasionRisk + FMath::Clamp((FuelShortage + MetalsShortage) / 55, 0, 5), 0, State.InvasionRisk.InvasionRiskTrigger);

        TArray<FString> Effects;
        auto AddShortageEffectLine = [&Effects](const FString& Label, int32 Delta)
        {
            if (Delta != 0)
            {
                Effects.Add(FString::Printf(TEXT("%s %+d"), *Label, Delta));
            }
        };
        AddShortageEffectLine(TEXT("approval"), -FoodUnrest - FMath::Clamp(FuelShortage / 45, 0, 3));
        AddShortageEffectLine(TEXT("unrest"), FoodUnrest + FMath::Clamp(State.ResourceChains.TotalShortagePressure / 30, 0, 3));
        AddShortageEffectLine(TEXT("military readiness"), -FuelReadinessLoss - MetalsReadinessLoss);
        AddShortageEffectLine(TEXT("production"), -FuelProductionPenalty - MaterialProductionPenalty);
        AddShortageEffectLine(TEXT("infrastructure"), -WoodInfrastructurePenalty);
        AddShortageEffectLine(TEXT("inflation"), ShortageInflation);
        if (Effects.Num() > 0)
        {
            State.ResourceChains.Summary += FString::Printf(TEXT(" Shortage effects applied: %s."), *FString::Join(Effects, TEXT(", ")));
        }
    }
    FDemocracyDepartmentState MakeDepartment(const FString& DepartmentName, const FString& MinisterTitle, const FString& Domain, int32 BudgetShare, int32 Staffing, int32 Effectiveness, int32 PublicTrust, int32 Priority, const FString& CurrentAction, const FString& PolicyInterface, const FString& AdvisorySummary, const TArray<FString>& ActionEffects)
    {
        FDemocracyDepartmentState Department;
        Department.DepartmentName = DepartmentName;
        Department.MinisterTitle = MinisterTitle;
        Department.Domain = Domain;
        Department.BudgetShare = FMath::Clamp(BudgetShare, 0, 100);
        Department.Staffing = FMath::Clamp(Staffing, 0, 100);
        Department.Effectiveness = FMath::Clamp(Effectiveness, 0, 100);
        Department.PublicTrust = FMath::Clamp(PublicTrust, 0, 100);
        Department.Priority = FMath::Clamp(Priority, 0, 100);
        Department.CurrentAction = CurrentAction;
        Department.PolicyInterface = PolicyInterface;
        Department.AdvisorySummary = AdvisorySummary;
        Department.ActionEffects = ActionEffects;
        return Department;
    }

    void InitializeDefaultDepartments(FDemocracySimulationState& State)
    {
        if (State.Departments.Departments.Num() > 0)
        {
            return;
        }

        const FDemocracyCountryState& Country = State.PlayerCountry;
        State.Departments.LastUpdatedTurn = State.Turn;
        State.Departments.Coordination = FMath::Clamp(58 - Country.CountrySizeScore * 3, 35, 70);
        State.Departments.Summary = TEXT("Departments initialized from loaded save. Use the Departments Desk to assign actions.");
        State.Departments.Departments = {
            MakeDepartment(TEXT("Defense"), TEXT("Minister of Defense"), TEXT("Military readiness, invasion risk, border security, and mobilization."), State.EconomyBudget.DefenseSpending, 55, Country.MilitaryReadiness, 50, 55, TEXT("Maintain Readiness"), TEXT("Military policy"), TEXT("Keep readiness above takeover warning thresholds."), { TEXT("Improves readiness when funded."), TEXT("High tempo can increase unrest and fuel demand.") }),
            MakeDepartment(TEXT("Treasury"), TEXT("Treasury Secretary"), TEXT("Taxes, spending, debt, inflation, reserves, and emergency funding."), State.EconomyBudget.TaxRate, 60, Country.EconomicHealth, 52, 55, TEXT("Stabilize Budget"), TEXT("Tax policy and spending posture"), TEXT("Keep deficit and inflation under control."), { TEXT("Improves treasury and budget clarity."), TEXT("Austerity can reduce public trust.") }),
            MakeDepartment(TEXT("Agriculture"), TEXT("Minister of Agriculture"), TEXT("Food production, rural regions, farm inputs, and shortage relief."), 12, 52, Country.Resources.Food / 4, 50, 50, TEXT("Boost Food Supply"), TEXT("Environmental and resource policy"), TEXT("Food shortages drive unrest quickly."), { TEXT("Improves food production and rural approval."), TEXT("Requires water and infrastructure support.") }),
            MakeDepartment(TEXT("Energy"), TEXT("Minister of Energy"), TEXT("Fuel reserves, power grid, imports, exports, and industrial energy demand."), 12, 50, Country.Resources.GasOil / 3, 48, 50, TEXT("Secure Fuel Supply"), TEXT("Extraction and trade policy"), TEXT("Fuel shortages weaken readiness and raise inflation."), { TEXT("Improves fuel availability and production efficiency."), TEXT("Extraction can hurt environmental health.") }),
            MakeDepartment(TEXT("Health"), TEXT("Minister of Health"), TEXT("Public health, crisis response, water safety, and public services."), State.EconomyBudget.PublicServicesSpending / 2, 55, State.EconomyBudget.PublicServices, 55, 50, TEXT("Maintain Public Health"), TEXT("Public services and welfare policy"), TEXT("Health capacity improves stability during shortages."), { TEXT("Improves public trust and lowers needs pressure."), TEXT("Requires steady spending.") }),
            MakeDepartment(TEXT("Education"), TEXT("Minister of Education"), TEXT("Schools, workforce training, technology growth, and youth approval."), 10, 50, 50 + Country.Technology * 5, 54, 45, TEXT("Workforce Training"), TEXT("Long-term technology and public services"), TEXT("Education raises production over time."), { TEXT("Improves technology and jobs over time."), TEXT("Short-term crisis impact is limited.") }),
            MakeDepartment(TEXT("Infrastructure"), TEXT("Minister of Infrastructure"), TEXT("Roads, logistics, utilities, construction, repairs, and disaster resilience."), State.EconomyBudget.InfrastructureSpending, 54, Country.Infrastructure, 50, 55, TEXT("Repair Critical Systems"), TEXT("Infrastructure spending and resource chains"), TEXT("Infrastructure supports every resource chain."), { TEXT("Improves production efficiency and regional stability."), TEXT("Consumes wood, metals, and treasury.") })
        };
    }

    FDemocracyDepartmentState* FindDepartment(FDemocracyDepartmentSystemState& Departments, const FString& DepartmentName)
    {
        for (FDemocracyDepartmentState& Department : Departments.Departments)
        {
            if (Department.DepartmentName.Equals(DepartmentName, ESearchCase::IgnoreCase))
            {
                return &Department;
            }
        }
        return nullptr;
    }

    const FDemocracyDepartmentState* FindDepartmentConst(const FDemocracyDepartmentSystemState& Departments, const FString& DepartmentName)
    {
        for (const FDemocracyDepartmentState& Department : Departments.Departments)
        {
            if (Department.DepartmentName.Equals(DepartmentName, ESearchCase::IgnoreCase))
            {
                return &Department;
            }
        }
        return nullptr;
    }

    int32 GetDepartmentEffectiveness(const FDemocracyDepartmentSystemState& Departments, const FString& DepartmentName)
    {
        const FDemocracyDepartmentState* Department = FindDepartmentConst(Departments, DepartmentName);
        return Department ? Department->Effectiveness : 50;
    }

    void RecalculateDepartments(FDemocracySimulationState& State)
    {
        InitializeDefaultDepartments(State);
        FDemocracyCountryState& Country = State.PlayerCountry;
        FDemocracyDepartmentSystemState& Departments = State.Departments;
        int32 TotalEffectiveness = 0;
        int32 CriticalCount = 0;

        for (FDemocracyDepartmentState& Department : Departments.Departments)
        {
            int32 Stress = 0;
            int32 Funding = Department.BudgetShare;
            if (Department.DepartmentName.Equals(TEXT("Defense"), ESearchCase::IgnoreCase))
            {
                Funding = State.EconomyBudget.DefenseSpending;
                Stress = FMath::Max(0, 65 - Country.MilitaryReadiness) + State.InvasionRisk.CurrentInvasionRisk / 4;
                Department.AdvisorySummary = TEXT("Controls mobilization, readiness, and takeover response options.");
            }
            else if (Department.DepartmentName.Equals(TEXT("Treasury"), ESearchCase::IgnoreCase))
            {
                Funding = State.EconomyBudget.TaxRate;
                Stress = FMath::Max(0, State.EconomyBudget.Deficit) / 12 + State.EconomyBudget.Inflation;
                Department.AdvisorySummary = TEXT("Controls budget, debt, tax, and emergency funding actions.");
            }
            else if (Department.DepartmentName.Equals(TEXT("Agriculture"), ESearchCase::IgnoreCase))
            {
                Stress = GetResourceChainShortage(State.ResourceChains, TEXT("Food")) / 4;
                Department.AdvisorySummary = TEXT("Controls food supply actions and rural production support.");
            }
            else if (Department.DepartmentName.Equals(TEXT("Energy"), ESearchCase::IgnoreCase))
            {
                Stress = GetResourceChainShortage(State.ResourceChains, TEXT("Fuel")) / 4 + FMath::Max(0, State.EconomyBudget.Inflation - 8);
                Department.AdvisorySummary = TEXT("Controls fuel security, power reliability, and extraction/trade response.");
            }
            else if (Department.DepartmentName.Equals(TEXT("Health"), ESearchCase::IgnoreCase))
            {
                Funding = State.EconomyBudget.PublicServicesSpending / 2;
                Stress = State.Demographics.NationalNeedsPressure / 5 + GetResourceChainShortage(State.ResourceChains, TEXT("Water")) / 5;
                Department.AdvisorySummary = TEXT("Controls public health, shortage response, and welfare support.");
            }
            else if (Department.DepartmentName.Equals(TEXT("Education"), ESearchCase::IgnoreCase))
            {
                Stress = FMath::Max(0, 55 - Country.Technology * 12) + FMath::Max(0, 55 - Country.EconomicHealth) / 3;
                Department.AdvisorySummary = TEXT("Controls long-term technology, workforce, and youth approval actions.");
            }
            else if (Department.DepartmentName.Equals(TEXT("Infrastructure"), ESearchCase::IgnoreCase))
            {
                Funding = State.EconomyBudget.InfrastructureSpending;
                Stress = FMath::Max(0, 70 - Country.Infrastructure) + GetResourceChainShortage(State.ResourceChains, TEXT("Wood")) / 6 + GetResourceChainShortage(State.ResourceChains, TEXT("Metals")) / 6;
                Department.AdvisorySummary = TEXT("Controls repairs, logistics, utilities, and construction capacity.");
            }
            else if (Department.DepartmentName.Equals(TEXT("Foreign Affairs"), ESearchCase::IgnoreCase))
            {
                Stress = FMath::Max(0, 58 - Country.DiplomaticStanding) + State.InvasionRisk.CurrentInvasionRisk / 5;
                Department.AdvisorySummary = TEXT("Controls alliance outreach, crisis channels, trade access, and foreign meeting preparation.");
            }
            else if (Department.DepartmentName.Equals(TEXT("Justice"), ESearchCase::IgnoreCase))
            {
                Stress = Country.Unrest / 3 + State.FailureRisk.CurrentAssassinationRisk / 5;
                Department.AdvisorySummary = TEXT("Controls legitimacy, public order, investigations, and anti-corruption response.");
            }
            else if (Department.DepartmentName.Equals(TEXT("Commerce"), ESearchCase::IgnoreCase))
            {
                Stress = FMath::Max(0, 58 - Country.EconomicHealth) + State.EconomyBudget.Inflation / 2;
                Department.AdvisorySummary = TEXT("Controls jobs, industry confidence, imports, exports, and market-shock response.");
            }
            else if (Department.DepartmentName.Equals(TEXT("Environment"), ESearchCase::IgnoreCase))
            {
                Stress = FMath::Max(0, 62 - Country.EnvironmentalHealth) + GetResourceChainShortage(State.ResourceChains, TEXT("Water")) / 8;
                Department.AdvisorySummary = TEXT("Controls conservation, disaster resilience, water pressure, and extraction tradeoffs.");
            }

            const int32 PrioritySupport = Department.Priority / 8;
            const int32 FundingSupport = Funding / 4;
            Department.BudgetShare = FMath::Clamp(Funding, 0, 100);
            Department.Effectiveness = FMath::Clamp((Department.Effectiveness + Department.Staffing + Department.PublicTrust + PrioritySupport + FundingSupport - Stress / 2) / 3, 0, 100);
            Department.PublicTrust = FMath::Clamp(Department.PublicTrust + (Department.Effectiveness > 62 ? 1 : 0) - (Stress > 25 ? 1 : 0) - (Department.CurrentAction.Contains(TEXT("Emergency")) ? 1 : 0), 0, 100);
            Department.ActionEffects.Reset();
            Department.ActionEffects.Add(FString::Printf(TEXT("Effectiveness %d, trust %d, priority %d, stress %d."), Department.Effectiveness, Department.PublicTrust, Department.Priority, Stress));
            Department.ActionEffects.Add(FString::Printf(TEXT("Current action: %s."), *Department.CurrentAction));
            TotalEffectiveness += Department.Effectiveness;
            CriticalCount += Department.Effectiveness < 35 ? 1 : 0;
        }

        const int32 AverageEffectiveness = Departments.Departments.Num() > 0 ? TotalEffectiveness / Departments.Departments.Num() : 50;
        Departments.Coordination = FMath::Clamp((Departments.Coordination + AverageEffectiveness + Country.Stability / 2 - CriticalCount * 5) / 2, 0, 100);
        Departments.LastUpdatedTurn = State.Turn;
        Departments.Summary = FString::Printf(TEXT("%d ministries active | average effectiveness %d | coordination %d | critical ministries %d."), Departments.Departments.Num(), AverageEffectiveness, Departments.Coordination, CriticalCount);
    }

    FString BuildDepartmentSummaryText(const FDemocracyDepartmentSystemState& Departments)
    {
        TArray<FString> Lines;
        Lines.Add(FString::Printf(TEXT("Coordination %d | updated turn %d"), Departments.Coordination, Departments.LastUpdatedTurn));
        Lines.Add(Departments.Summary);
        for (const FDemocracyDepartmentState& Department : Departments.Departments)
        {
            Lines.Add(FString::Printf(TEXT("%s: action %s | effectiveness %d | trust %d | priority %d | interface: %s"),
                *Department.DepartmentName,
                *Department.CurrentAction,
                Department.Effectiveness,
                Department.PublicTrust,
                Department.Priority,
                *Department.PolicyInterface));
        }
        return FString::Join(Lines, TEXT("\n"));
    }
    FString BuildPressOfficeSummaryText(const FDemocracyPressOfficeState& PressOffice)
    {
        TArray<FString> Lines;
        Lines.Add(FString::Printf(TEXT("Credibility %d | announcements %d | empty streak %d | false streak %d | updated turn %d"), PressOffice.Credibility, PressOffice.TotalAnnouncements, PressOffice.ConsecutiveEmptyAnnouncements, PressOffice.ConsecutiveFalseAnnouncements, PressOffice.LastUpdatedTurn));
        Lines.Add(PressOffice.LastAnnouncementSummary);
        const int32 StartIndex = FMath::Max(0, PressOffice.Records.Num() - 5);
        for (int32 Index = PressOffice.Records.Num() - 1; Index >= StartIndex; --Index)
        {
            const FDemocracyPressReleaseRecordState& Record = PressOffice.Records[Index];
            Lines.Add(FString::Printf(TEXT("Turn %d | %s | %s | truthful %s | approval %+d stability %+d diplomacy %+d unrest %+d credibility %+d | %s"),
                Record.Turn,
                *Record.AnnouncementType,
                *Record.MessageQuality,
                Record.bTruthful ? TEXT("yes") : TEXT("no"),
                Record.ApprovalDelta,
                Record.StabilityDelta,
                Record.DiplomacyDelta,
                Record.UnrestDelta,
                Record.CredibilityDelta,
                *Record.Summary));
        }
        return FString::Join(Lines, TEXT("\n"));
    }

    void InitializePressOfficeIfMissing(FDemocracySimulationState& State)
    {
        if (State.PressOffice.MaxRecords <= 0)
        {
            State.PressOffice.MaxRecords = 40;
        }
        if (State.PressOffice.Credibility <= 0)
        {
            State.PressOffice.Credibility = FMath::Clamp(72 - State.PlayerCountry.CountrySizeScore * 4, 52, 76);
        }
        if (State.PressOffice.LastAnnouncementSummary.IsEmpty())
        {
            State.PressOffice.LastAnnouncementSummary = TEXT("Press office initialized for this save. Announcements now affect public trust and diplomacy.");
        }
    }
    FString BuildMeetingSystemSummaryText(const FDemocracyMeetingSystemState& MeetingSystem)
    {
        TArray<FString> Lines;
        Lines.Add(FString::Printf(TEXT("Meetings %d | advisor coordination %d | foreign trust %d | updated turn %d"), MeetingSystem.TotalMeetings, MeetingSystem.AdvisorCoordination, MeetingSystem.ForeignTrust, MeetingSystem.LastUpdatedTurn));
        Lines.Add(MeetingSystem.LastMeetingSummary);
        const int32 StartIndex = FMath::Max(0, MeetingSystem.Records.Num() - 6);
        for (int32 Index = MeetingSystem.Records.Num() - 1; Index >= StartIndex; --Index)
        {
            const FDemocracyMeetingRecordState& Record = MeetingSystem.Records[Index];
            Lines.Add(FString::Printf(TEXT("Turn %d | %s with %s | %s | approval %+d stability %+d unrest %+d diplomacy %+d treasury %+d economy %+d military %+d infrastructure %+d | %s"),
                Record.Turn,
                *Record.MeetingType,
                *Record.ParticipantName,
                *Record.AgendaItem,
                Record.ApprovalDelta,
                Record.StabilityDelta,
                Record.UnrestDelta,
                Record.DiplomacyDelta,
                Record.TreasuryDelta,
                Record.EconomyDelta,
                Record.MilitaryDelta,
                Record.InfrastructureDelta,
                *Record.OutcomeSummary));
        }
        return FString::Join(Lines, TEXT("\n"));
    }

    void InitializeMeetingSystemIfMissing(FDemocracySimulationState& State)
    {
        if (State.MeetingSystem.MaxRecords <= 0)
        {
            State.MeetingSystem.MaxRecords = 50;
        }
        if (State.MeetingSystem.AdvisorCoordination <= 0)
        {
            State.MeetingSystem.AdvisorCoordination = FMath::Clamp(56 - State.PlayerCountry.CountrySizeScore * 3, 40, 60);
        }
        if (State.MeetingSystem.ForeignTrust <= 0)
        {
            State.MeetingSystem.ForeignTrust = FMath::Clamp(54 - State.PlayerCountry.CountrySizeScore * 4, 34, 58);
        }
        if (State.MeetingSystem.LastMeetingSummary.IsEmpty())
        {
            State.MeetingSystem.LastMeetingSummary = TEXT("Meeting system initialized for this save.");
        }
    }
    FDemocracyDevelopmentTrackState MakeDevelopmentTrack(const FString& TrackName, int32 Level, int32 Progress, int32 ProgressTarget, int32 TreasuryCost, int32 WoodCost, int32 MetalsCost, int32 FuelCost, const FString& Project, const FString& Benefit, const TArray<FString>& Unlocks)
    {
        FDemocracyDevelopmentTrackState Track;
        Track.TrackName = TrackName;
        Track.FocusArea = TrackName;
        Track.Level = FMath::Clamp(Level, 1, 10);
        Track.Progress = FMath::Clamp(Progress, 0, FMath::Max(1, ProgressTarget));
        Track.ProgressTarget = FMath::Max(1, ProgressTarget);
        Track.TreasuryCost = FMath::Max(0, TreasuryCost);
        Track.WoodCost = FMath::Max(0, WoodCost);
        Track.MetalsCost = FMath::Max(0, MetalsCost);
        Track.FuelCost = FMath::Max(0, FuelCost);
        Track.CurrentProject = Project;
        Track.StrategicBenefit = Benefit;
        Track.Unlocks = Unlocks;
        return Track;
    }

    void InitializeDevelopmentSystemIfMissing(FDemocracySimulationState& State)
    {
        if (State.DevelopmentSystem.Tracks.Num() == 0)
        {
            State.DevelopmentSystem.ActiveFocus = TEXT("Infrastructure");
            State.DevelopmentSystem.DevelopmentPoints = 0;
            State.DevelopmentSystem.LastUpdatedTurn = State.Turn;
            State.DevelopmentSystem.Summary = TEXT("Development system initialized from existing save. Choose a focus and step the simulation to gain progress.");
            State.DevelopmentSystem.Tracks = {
                MakeDevelopmentTrack(TEXT("Infrastructure"), 1, 0, 100, 28, 4, 3, 1, TEXT("National Logistics Upgrade"), TEXT("Raises infrastructure, production efficiency, and resource-chain resilience."), { TEXT("Better roads"), TEXT("Grid reliability"), TEXT("Disaster response capacity") }),
                MakeDevelopmentTrack(TEXT("Military"), 1, 0, 110, 32, 1, 5, 4, TEXT("Readiness Modernization"), TEXT("Raises military readiness and reduces foreign takeover pressure."), { TEXT("Modern logistics"), TEXT("Border surveillance"), TEXT("Rapid response doctrine") }),
                MakeDevelopmentTrack(TEXT("Agriculture"), 1, 0, 95, 24, 2, 2, 1, TEXT("Food Security Program"), TEXT("Improves food and water production, lowering shortage-driven unrest."), { TEXT("Irrigation"), TEXT("Storage networks"), TEXT("Climate-hardy crops") }),
                MakeDevelopmentTrack(TEXT("Industry"), 1, 0, 115, 34, 2, 5, 3, TEXT("Industrial Base Expansion"), TEXT("Raises economic health, production efficiency, and metals output."), { TEXT("Manufacturing clusters"), TEXT("Materials processing"), TEXT("Skilled workforce") }),
                MakeDevelopmentTrack(TEXT("Communications"), 1, 0, 90, 22, 1, 2, 1, TEXT("National Communications Network"), TEXT("Improves stability, press credibility, advisor coordination, and public trust."), { TEXT("Emergency alerts"), TEXT("Public information systems"), TEXT("Government coordination") })
            };
        }
        if (State.DevelopmentSystem.ActiveFocus.IsEmpty())
        {
            State.DevelopmentSystem.ActiveFocus = State.DevelopmentSystem.Tracks.Num() > 0 ? State.DevelopmentSystem.Tracks[0].TrackName : TEXT("Infrastructure");
        }
    }

    FDemocracyDevelopmentTrackState* FindDevelopmentTrack(FDemocracyDevelopmentSystemState& DevelopmentSystem, const FString& TrackName)
    {
        for (FDemocracyDevelopmentTrackState& Track : DevelopmentSystem.Tracks)
        {
            if (Track.TrackName.Equals(TrackName, ESearchCase::IgnoreCase))
            {
                return &Track;
            }
        }
        return nullptr;
    }

    FString BuildDevelopmentSummaryText(const FDemocracyDevelopmentSystemState& DevelopmentSystem)
    {
        TArray<FString> Lines;
        Lines.Add(FString::Printf(TEXT("Active focus: %s | development points %d | updated turn %d"), *DevelopmentSystem.ActiveFocus, DevelopmentSystem.DevelopmentPoints, DevelopmentSystem.LastUpdatedTurn));
        Lines.Add(DevelopmentSystem.Summary);
        for (const FDemocracyDevelopmentTrackState& Track : DevelopmentSystem.Tracks)
        {
            Lines.Add(FString::Printf(TEXT("%s: level %d | progress %d/%d | cost T%d W%d M%d F%d | project: %s | %s"),
                *Track.TrackName,
                Track.Level,
                Track.Progress,
                Track.ProgressTarget,
                Track.TreasuryCost,
                Track.WoodCost,
                Track.MetalsCost,
                Track.FuelCost,
                *Track.CurrentProject,
                *Track.StrategicBenefit));
        }
        return FString::Join(Lines, TEXT("\n"));
    }

    void ApplyDevelopmentLevelBonus(FDemocracySimulationState& State, const FDemocracyDevelopmentTrackState& Track)
    {
        FDemocracyCountryState& Country = State.PlayerCountry;
        if (Track.TrackName.Equals(TEXT("Infrastructure"), ESearchCase::IgnoreCase))
        {
            Country.Infrastructure = FMath::Clamp(Country.Infrastructure + 4, 0, 100);
            State.EconomyBudget.ProductionEfficiency = FMath::Clamp(State.EconomyBudget.ProductionEfficiency + 2, 0, 100);
        }
        else if (Track.TrackName.Equals(TEXT("Military"), ESearchCase::IgnoreCase))
        {
            Country.MilitaryReadiness = FMath::Clamp(Country.MilitaryReadiness + 5, 0, 100);
            State.InvasionRisk.CurrentInvasionRisk = FMath::Clamp(State.InvasionRisk.CurrentInvasionRisk - 5, 0, State.InvasionRisk.InvasionRiskTrigger);
        }
        else if (Track.TrackName.Equals(TEXT("Agriculture"), ESearchCase::IgnoreCase))
        {
            Country.Resources.Food = FMath::Max(0, Country.Resources.Food + 18);
            Country.Resources.Water = FMath::Max(0, Country.Resources.Water + 10);
            Country.Unrest = FMath::Clamp(Country.Unrest - 1, 0, 100);
        }
        else if (Track.TrackName.Equals(TEXT("Industry"), ESearchCase::IgnoreCase))
        {
            Country.EconomicHealth = FMath::Clamp(Country.EconomicHealth + 4, 0, 100);
            State.EconomyBudget.ProductionEfficiency = FMath::Clamp(State.EconomyBudget.ProductionEfficiency + 3, 0, 100);
            Country.Resources.Metals = FMath::Max(0, Country.Resources.Metals + 8);
        }
        else if (Track.TrackName.Equals(TEXT("Communications"), ESearchCase::IgnoreCase))
        {
            Country.Stability = FMath::Clamp(Country.Stability + 3, 0, 100);
            State.PressOffice.Credibility = FMath::Clamp(State.PressOffice.Credibility + 3, 0, 100);
            State.MeetingSystem.AdvisorCoordination = FMath::Clamp(State.MeetingSystem.AdvisorCoordination + 3, 0, 100);
        }
        Country.Technology = FMath::Clamp(Country.Technology + 1, 1, 25);
    }

    void TickDevelopmentSystem(FDemocracySimulationState& State)
    {
        InitializeDevelopmentSystemIfMissing(State);
        FDemocracyDevelopmentTrackState* ActiveTrack = FindDevelopmentTrack(State.DevelopmentSystem, State.DevelopmentSystem.ActiveFocus);
        if (!ActiveTrack)
        {
            return;
        }

        FDemocracyCountryState& Country = State.PlayerCountry;
        FDemocracyResourceInventory& Resources = Country.Resources;
        const int32 EducationEffectiveness = GetDepartmentEffectiveness(State.Departments, TEXT("Education"));
        const int32 InfrastructureEffectiveness = GetDepartmentEffectiveness(State.Departments, TEXT("Infrastructure"));
        const int32 ProgressGain = FMath::Clamp(4 + Country.Technology / 2 + EducationEffectiveness / 25 + InfrastructureEffectiveness / 35, 4, 18);
        const bool bCanFund = Country.Treasury >= ActiveTrack->TreasuryCost && Resources.Wood >= ActiveTrack->WoodCost && Resources.Metals >= ActiveTrack->MetalsCost && Resources.GasOil >= ActiveTrack->FuelCost;

        if (!bCanFund)
        {
            State.DevelopmentSystem.Summary = FString::Printf(TEXT("%s stalled: needs treasury %d, wood %d, metals %d, fuel %d."), *ActiveTrack->TrackName, ActiveTrack->TreasuryCost, ActiveTrack->WoodCost, ActiveTrack->MetalsCost, ActiveTrack->FuelCost);
            State.DevelopmentSystem.LastUpdatedTurn = State.Turn;
            return;
        }

        Country.Treasury = FMath::Max(0, Country.Treasury - ActiveTrack->TreasuryCost);
        Resources.Wood = FMath::Max(0, Resources.Wood - ActiveTrack->WoodCost);
        Resources.Metals = FMath::Max(0, Resources.Metals - ActiveTrack->MetalsCost);
        Resources.GasOil = FMath::Max(0, Resources.GasOil - ActiveTrack->FuelCost);
        ActiveTrack->Progress += ProgressGain;
        ++State.DevelopmentSystem.DevelopmentPoints;
        State.DevelopmentSystem.LastUpdatedTurn = State.Turn;

        if (ActiveTrack->Progress >= ActiveTrack->ProgressTarget)
        {
            ActiveTrack->Progress -= ActiveTrack->ProgressTarget;
            ++ActiveTrack->Level;
            ActiveTrack->ProgressTarget += 35;
            ActiveTrack->TreasuryCost += 6;
            ActiveTrack->WoodCost += ActiveTrack->TrackName.Equals(TEXT("Infrastructure"), ESearchCase::IgnoreCase) ? 2 : 1;
            ActiveTrack->MetalsCost += ActiveTrack->TrackName.Equals(TEXT("Military"), ESearchCase::IgnoreCase) || ActiveTrack->TrackName.Equals(TEXT("Industry"), ESearchCase::IgnoreCase) ? 2 : 1;
            ApplyDevelopmentLevelBonus(State, *ActiveTrack);
            State.DevelopmentSystem.Summary = FString::Printf(TEXT("%s advanced to level %d. Strategic benefit applied: %s"), *ActiveTrack->TrackName, ActiveTrack->Level, *ActiveTrack->StrategicBenefit);
        }
        else
        {
            State.DevelopmentSystem.Summary = FString::Printf(TEXT("%s gained %d progress toward %s."), *ActiveTrack->TrackName, ProgressGain, *ActiveTrack->CurrentProject);
        }
    }
    void LogDecision(FDemocracySimulationState& State, const FString& Category, const FString& Title, const FString& Detail, const FString& Consequence, int32 Severity, const TArray<FString>& Tags)
    {
        FDemocracyDecisionHistoryState& History = State.DecisionHistory;
        if (History.MaxRecords <= 0)
        {
            History.MaxRecords = 80;
        }

        const FDemocracyCountryState& Country = State.PlayerCountry;
        FDemocracyDecisionRecordState Record;
        Record.Turn = State.Turn;
        Record.Category = Category;
        Record.DecisionTitle = Title;
        Record.DecisionDetail = Detail;
        Record.ConsequenceSummary = Consequence;
        Record.ApprovalAfter = Country.PublicApproval;
        Record.StabilityAfter = Country.Stability;
        Record.UnrestAfter = Country.Unrest;
        Record.TreasuryAfter = Country.Treasury;
        Record.EconomyAfter = Country.EconomicHealth;
        Record.MilitaryAfter = Country.MilitaryReadiness;
        Record.Severity = FMath::Clamp(Severity, 0, 100);
        Record.TimestampUtc = SaveTimestamp();
        Record.Tags = Tags;

        History.Records.Add(Record);
        while (History.Records.Num() > History.MaxRecords)
        {
            History.Records.RemoveAt(0);
        }
        History.LastUpdatedTurn = State.Turn;
        History.Summary = FString::Printf(TEXT("%d major decisions logged. Latest: %s - %s."), History.Records.Num(), *Category, *Title);
    }

    void TrimRtsBackflowHistory(FDemocracyRtsBackflowState& Backflow)
    {
        while (Backflow.OutcomeHistory.Num() > 40)
        {
            Backflow.OutcomeHistory.RemoveAt(0);
        }
    }

    FString NormalizeRtsImportEventType(const FString& OutcomeType)
    {
        if (OutcomeType.Equals(TEXT("Battle Lost"), ESearchCase::IgnoreCase))
        {
            return TEXT("Battle Lost");
        }
        if (OutcomeType.Equals(TEXT("Capital Threatened"), ESearchCase::IgnoreCase))
        {
            return TEXT("Capital Threatened");
        }
        if (OutcomeType.Equals(TEXT("Supply Route Broken"), ESearchCase::IgnoreCase))
        {
            return TEXT("Supply Route Broken");
        }
        if (OutcomeType.Equals(TEXT("Movement Complete"), ESearchCase::IgnoreCase))
        {
            return TEXT("Movement Complete");
        }
        if (OutcomeType.Equals(TEXT("Province Occupied"), ESearchCase::IgnoreCase))
        {
            return TEXT("Province Occupied");
        }
        if (OutcomeType.Equals(TEXT("Occupation Finalized"), ESearchCase::IgnoreCase))
        {
            return TEXT("Occupation Finalized");
        }
        if (OutcomeType.Equals(TEXT("Resource Disrupted"), ESearchCase::IgnoreCase))
        {
            return TEXT("Resource Disrupted");
        }
        if (OutcomeType.Equals(TEXT("Territory Gained"), ESearchCase::IgnoreCase) || OutcomeType.Equals(TEXT("Province Captured"), ESearchCase::IgnoreCase))
        {
            return TEXT("Province Captured");
        }
        if (OutcomeType.Equals(TEXT("Territory Lost"), ESearchCase::IgnoreCase) || OutcomeType.Equals(TEXT("Province Lost"), ESearchCase::IgnoreCase))
        {
            return TEXT("Province Lost");
        }
        return TEXT("Battle Stalemate");
    }

    FString RtsAttentionCategoryForImportEvent(const FString& ImportEventType)
    {
        if (ImportEventType.Contains(TEXT("Province")) || ImportEventType.Equals(TEXT("Occupation Finalized"), ESearchCase::IgnoreCase))
        {
            return TEXT("Territory Control");
        }
        if (ImportEventType.Equals(TEXT("Movement Complete"), ESearchCase::IgnoreCase))
        {
            return TEXT("Operations");
        }
        if (ImportEventType.Equals(TEXT("Resource Disrupted"), ESearchCase::IgnoreCase))
        {
            return TEXT("Resources");
        }
        if (ImportEventType.Equals(TEXT("Battle Lost"), ESearchCase::IgnoreCase))
        {
            return TEXT("Military Readiness");
        }
        if (ImportEventType.Equals(TEXT("Capital Threatened"), ESearchCase::IgnoreCase))
        {
            return TEXT("Capital Security");
        }
        if (ImportEventType.Equals(TEXT("Supply Route Broken"), ESearchCase::IgnoreCase))
        {
            return TEXT("Logistics");
        }
        return TEXT("Strategic Pressure");
    }

    const FDemocracyProvinceOwnershipState* FindRtsAttentionProvince(const FDemocracySimulationState& State, const FDemocracyRtsOutcomeState& Outcome)
    {
        const bool bNeedsPlayerProvince = Outcome.ImportEventType.Equals(TEXT("Province Lost"), ESearchCase::IgnoreCase) || Outcome.ImportEventType.Equals(TEXT("Capital Threatened"), ESearchCase::IgnoreCase) || Outcome.ImportEventType.Equals(TEXT("Supply Route Broken"), ESearchCase::IgnoreCase);
        for (const FDemocracyProvinceOwnershipState& Province : State.RtsWorld.Ownership.Provinces)
        {
            if (bNeedsPlayerProvince && Province.bPlayerControlled && (Province.bBorderProvince || Outcome.ImportEventType.Equals(TEXT("Capital Threatened"), ESearchCase::IgnoreCase)))
            {
                return &Province;
            }
            if (!bNeedsPlayerProvince && Province.CurrentControllerCountryName.Equals(Outcome.OpponentCountry, ESearchCase::IgnoreCase))
            {
                return &Province;
            }
        }
        for (const FDemocracyProvinceOwnershipState& Province : State.RtsWorld.Ownership.Provinces)
        {
            if (Province.bPlayerControlled == bNeedsPlayerProvince)
            {
                return &Province;
            }
        }
        return State.RtsWorld.Ownership.Provinces.Num() > 0 ? &State.RtsWorld.Ownership.Provinces[0] : nullptr;
    }

    int32 RtsAttentionSeverityForOutcome(const FDemocracyRtsOutcomeState& Outcome)
    {
        return FMath::Clamp(FMath::Abs(Outcome.TerritoryDelta) * 22 + Outcome.Casualties / 8 + Outcome.ResourceDisruption + Outcome.WarFatigueDelta + FMath::Max(0, Outcome.InvasionRiskDelta) + FMath::Abs(Outcome.StabilityDelta) * 3 + Outcome.BudgetStrain / 2, 1, 100);
    }

    void PopulateRtsOutcomeAttentionFields(const FDemocracySimulationState& State, FDemocracyRtsOutcomeState& Outcome)
    {
        Outcome.ImportEventType = NormalizeRtsImportEventType(Outcome.OutcomeType);
        Outcome.AttentionCategory = RtsAttentionCategoryForImportEvent(Outcome.ImportEventType);
        Outcome.AffectedCountryName = Outcome.OpponentCountry.IsEmpty() ? State.PlayerCountry.CountryName : Outcome.OpponentCountry;
        if (const FDemocracyProvinceOwnershipState* Province = FindRtsAttentionProvince(State, Outcome))
        {
            Outcome.AffectedProvinceId = Province->ProvinceId;
            Outcome.AffectedProvinceName = Province->ProvinceName;
            Outcome.AffectedResource = Province->ResourceFocus;
        }
        if (Outcome.AffectedResource.IsEmpty() && Outcome.ImportEventType.Equals(TEXT("Supply Route Broken"), ESearchCase::IgnoreCase))
        {
            Outcome.AffectedResource = TEXT("Fuel");
        }
        Outcome.AttentionSeverity = RtsAttentionSeverityForOutcome(Outcome);
        Outcome.AttentionDeadlineTurn = State.Turn + FMath::Clamp(6 - Outcome.AttentionSeverity / 20, 2, 6);
        Outcome.bRequiresSimulationAttention = true;
        Outcome.bAcknowledgedBySimulation = false;
        Outcome.SimulationAttentionStatus = TEXT("Queued");
        Outcome.AttentionSummary = FString::Printf(TEXT("%s requires %s attention by turn %d. Affected province: %s. Affected resource: %s."), *Outcome.ImportEventType, *Outcome.AttentionCategory, Outcome.AttentionDeadlineTurn, Outcome.AffectedProvinceName.IsEmpty() ? TEXT("Unassigned") : *Outcome.AffectedProvinceName, Outcome.AffectedResource.IsEmpty() ? TEXT("None") : *Outcome.AffectedResource);
    }

    FString BuildRtsOutcomeOfficeAlertLine(const FDemocracyRtsOutcomeState& Outcome)
    {
        const FString DeadlineText = Outcome.AttentionDeadlineTurn > 0 ? FString::Printf(TEXT("turn %d"), Outcome.AttentionDeadlineTurn) : TEXT("no deadline");
        return FString::Printf(TEXT("RTS Alert [%s severity %d]: %s | %s | deadline %s"),
            *Outcome.AttentionCategory,
            Outcome.AttentionSeverity,
            *Outcome.ImportEventType,
            Outcome.AttentionSummary.IsEmpty() ? *Outcome.Summary : *Outcome.AttentionSummary,
            *DeadlineText);
    }

    void AddRtsHudAlert(FDemocracyRtsWorldState& RtsWorld, const FString& AlertText)
    {
        if (AlertText.IsEmpty())
        {
            return;
        }
        RtsWorld.Hud.Alerts.Add(AlertText);
        while (RtsWorld.Hud.Alerts.Num() > 10)
        {
            RtsWorld.Hud.Alerts.RemoveAt(0);
        }
        RtsWorld.Hud.AlertSummary = FString::Join(RtsWorld.Hud.Alerts, TEXT(" | "));
    }
    FString GetRtsSlotResourceFocus(int32 SlotIndex)
    {
        static const TArray<FString> Focuses = { TEXT("Food"), TEXT("Fuel"), TEXT("Wood"), TEXT("Metals"), TEXT("Readiness"), TEXT("Reserve"), TEXT("Research"), TEXT("Security"), TEXT("Supply"), TEXT("Command"), TEXT("Industry"), TEXT("Logistics") };
        return Focuses.IsValidIndex(SlotIndex) ? Focuses[SlotIndex] : TEXT("Supply");
    }

    FString GetRtsResourceBuildingName(const FString& ResourceFocus)
    {
        if (ResourceFocus.Equals(TEXT("Food"), ESearchCase::IgnoreCase)) { return TEXT("Agriculture Site"); }
        if (ResourceFocus.Equals(TEXT("Fuel"), ESearchCase::IgnoreCase)) { return TEXT("Fuel Extraction Site"); }
        if (ResourceFocus.Equals(TEXT("Wood"), ESearchCase::IgnoreCase)) { return TEXT("Forestry Site"); }
        if (ResourceFocus.Equals(TEXT("Metals"), ESearchCase::IgnoreCase)) { return TEXT("Metals Extraction Site"); }
        if (ResourceFocus.Equals(TEXT("Readiness"), ESearchCase::IgnoreCase)) { return TEXT("Training Ground"); }
        if (ResourceFocus.Equals(TEXT("Research"), ESearchCase::IgnoreCase)) { return TEXT("Comms Research Annex"); }
        if (ResourceFocus.Equals(TEXT("Security"), ESearchCase::IgnoreCase)) { return TEXT("Security Checkpoint"); }
        if (ResourceFocus.Equals(TEXT("Industry"), ESearchCase::IgnoreCase)) { return TEXT("Industrial Yard"); }
        if (ResourceFocus.Equals(TEXT("Logistics"), ESearchCase::IgnoreCase)) { return TEXT("Logistics Yard"); }
        return TEXT("Supply Depot");
    }

    FDemocracyRtsBuildingState MakeRtsPlaceholderBuildingForSlot(int32 SlotIndex, const FString& ResourceFocus)
    {
        FDemocracyRtsBuildingState Building;
        Building.BuildingId = FString::Printf(TEXT("city_slot_%02d_%s"), SlotIndex + 1, *ResourceFocus.ToLower());
        Building.DisplayName = GetRtsResourceBuildingName(ResourceFocus);
        Building.BuildingType = ResourceFocus.Equals(TEXT("Readiness"), ESearchCase::IgnoreCase) || ResourceFocus.Equals(TEXT("Security"), ESearchCase::IgnoreCase) ? TEXT("Defense") : TEXT("Resource");
        Building.ResourceFocus = ResourceFocus;
        Building.CandidateAssetHint = FString::Printf(TEXT("Placeholder %s building slot; replace with final RTS city/base asset."), *ResourceFocus);
        Building.Level = 0;
        Building.BuildCost = 45 + SlotIndex * 4;
        Building.UpgradeCost = 80 + SlotIndex * 6;
        Building.BuildTimeTurns = FMath::Clamp(1 + SlotIndex / 4, 1, 4);
        Building.ProductionPerTick = FMath::Clamp(4 + SlotIndex % 4, 3, 9);
        Building.DefenseValue = ResourceFocus.Equals(TEXT("Security"), ESearchCase::IgnoreCase) || ResourceFocus.Equals(TEXT("Readiness"), ESearchCase::IgnoreCase) ? 16 : 0;
        Building.bConstructed = false;
        Building.bUpgradeQueued = false;
        Building.Status = TEXT("Queued Site");
        Building.RuntimeTags = { TEXT("placeholder"), TEXT("city-base"), ResourceFocus };
        Building.MaxHealth = 100;
        Building.CurrentHealth = 0;
        Building.DamagePercent = 0;
        Building.RepairCost = FMath::Max(10, Building.UpgradeCost / 3);
        return Building;
    }

    void RecountRtsCityBaseQueues(FDemocracyRtsCityBaseState& Base)
    {
        int32 ActiveBuildCount = 0;
        int32 ActiveUpgradeCount = 0;
        for (const FDemocracyRtsConstructionQueueEntryState& QueueEntry : Base.ConstructionQueue)
        {
            if (QueueEntry.bCancelled || QueueEntry.bComplete)
            {
                continue;
            }
            if (QueueEntry.QueueType.Equals(TEXT("Upgrade"), ESearchCase::IgnoreCase))
            {
                ++ActiveUpgradeCount;
            }
            else
            {
                ++ActiveBuildCount;
            }
        }
        Base.BuildQueueCount = ActiveBuildCount + ActiveUpgradeCount;
        Base.UpgradeQueueCount = ActiveUpgradeCount;
    }

    bool HasActiveRtsConstructionQueue(const FDemocracyRtsCityBaseState& Base, const FString& BuildingId)
    {
        for (const FDemocracyRtsConstructionQueueEntryState& QueueEntry : Base.ConstructionQueue)
        {
            if (QueueEntry.BuildingId.Equals(BuildingId, ESearchCase::IgnoreCase) && !QueueEntry.bCancelled && !QueueEntry.bComplete)
            {
                return true;
            }
        }
        return false;
    }

    FString BuildRtsCostText(int32 TreasuryCost, int32 FoodCost, int32 FuelCost, int32 WoodCost, int32 MetalsCost)
    {
        return FString::Printf(TEXT("Cost T%d F%d G%d W%d M%d"), TreasuryCost, FoodCost, FuelCost, WoodCost, MetalsCost);
    }

    FString NormalizeRtsRuleKey(const FString& ActionName)
    {
        FString Key = ActionName.ToLower();
        Key.ReplaceInline(TEXT("/"), TEXT("_"));
        Key.ReplaceInline(TEXT(" "), TEXT("_"));
        if (Key.Equals(TEXT("patrol_scout")) || Key.Contains(TEXT("scout"))) { return TEXT("patrol_scout"); }
        if (Key.Contains(TEXT("reinforce"))) { return TEXT("reinforce"); }
        if (Key.Contains(TEXT("defend"))) { return TEXT("defend"); }
        if (Key.Contains(TEXT("rally"))) { return TEXT("rally"); }
        if (Key.Contains(TEXT("battle")) || Key.Contains(TEXT("attack"))) { return TEXT("battle"); }
        if (Key.Contains(TEXT("occupation")) || Key.Contains(TEXT("occupy"))) { return TEXT("occupation"); }
        if (Key.Contains(TEXT("diplomacy")) || Key.Contains(TEXT("treaty")) || Key.Contains(TEXT("sanction"))) { return TEXT("diplomacy"); }
        if (Key.Contains(TEXT("upgrade"))) { return TEXT("upgrade"); }
        if (Key.Contains(TEXT("build_tank")) || Key.Contains(TEXT("tank"))) { return TEXT("build_tank"); }
        if (Key.Contains(TEXT("convoy")) || Key.Contains(TEXT("carrier"))) { return TEXT("spawn_convoy"); }
        if (Key.Contains(TEXT("worker"))) { return TEXT("spawn_worker"); }
        if (Key.Contains(TEXT("recruit")) || Key.Contains(TEXT("soldier"))) { return TEXT("recruit_soldier"); }
        if (Key.Contains(TEXT("build"))) { return TEXT("build"); }
        if (Key.Contains(TEXT("move"))) { return TEXT("move"); }
        return Key;
    }

    const FDemocracyRtsActionRuleState* FindRtsActionRule(const FDemocracyRtsWorldState& RtsWorld, const FString& ActionName)
    {
        const FString Key = NormalizeRtsRuleKey(ActionName);
        for (const FDemocracyRtsActionRuleState& Rule : RtsWorld.RulesetProfile.ActionRules)
        {
            if (Rule.ActionId.Equals(Key, ESearchCase::IgnoreCase))
            {
                return &Rule;
            }
        }
        return nullptr;
    }

    const FDemocracyRtsTerrainRuleState* FindRtsTerrainRule(const FDemocracyRtsWorldState& RtsWorld, const FString& TerrainType)
    {
        const FString LowerTerrain = TerrainType.ToLower();
        for (const FDemocracyRtsTerrainRuleState& Rule : RtsWorld.RulesetProfile.TerrainRules)
        {
            if (Rule.TerrainId.Equals(TerrainType, ESearchCase::IgnoreCase) || Rule.DisplayName.Equals(TerrainType, ESearchCase::IgnoreCase))
            {
                return &Rule;
            }
            for (const FString& Tag : Rule.Tags)
            {
                if (!Tag.IsEmpty() && LowerTerrain.Contains(Tag.ToLower()))
                {
                    return &Rule;
                }
            }
        }
        return nullptr;
    }
    int32 RtsTerrainBattleModifier(const FString& TerrainType, const FString& OrderType);

    bool DoesRtsRulesetHaveWarAuthority(const FDemocracySimulationState& State)
    {
        for (const FDemocracyWarConflictState& Conflict : State.WarSystem.ActiveConflicts)
        {
            const bool bCeasefire = Conflict.Status.Contains(TEXT("Ceasefire"), ESearchCase::IgnoreCase);
            const bool bResolved = Conflict.Status.Contains(TEXT("Resolved"), ESearchCase::IgnoreCase) || Conflict.Status.Contains(TEXT("Ended"), ESearchCase::IgnoreCase);
            if (!bCeasefire && !bResolved)
            {
                return true;
            }
        }
        return false;
    }

    int32 RtsRulesetTerrainBattleModifier(const FDemocracyRtsWorldState& RtsWorld, const FString& TerrainType, const FString& OrderType)
    {
        if (const FDemocracyRtsTerrainRuleState* Rule = FindRtsTerrainRule(RtsWorld, TerrainType))
        {
            int32 Modifier = Rule->DefenseModifier;
            if (OrderType.Equals(TEXT("Move"), ESearchCase::IgnoreCase) || OrderType.Equals(TEXT("Rally"), ESearchCase::IgnoreCase))
            {
                Modifier -= FMath::Max(0, Rule->MoveCost - 1) * 3;
            }
            if (OrderType.Equals(TEXT("Patrol/Scout"), ESearchCase::IgnoreCase))
            {
                Modifier -= FMath::Max(0, Rule->MoveCost - 1);
            }
            return FMath::Clamp(Modifier, -24, 28);
        }
        return RtsTerrainBattleModifier(TerrainType, OrderType);
    }
    bool CanPayRtsCost(const FDemocracyCountryState& Country, int32 TreasuryCost, int32 FoodCost, int32 FuelCost, int32 WoodCost, int32 MetalsCost)
    {
        return Country.Treasury >= TreasuryCost
            && Country.Resources.Food >= FoodCost
            && Country.Resources.GasOil >= FuelCost
            && Country.Resources.Wood >= WoodCost
            && Country.Resources.Metals >= MetalsCost;
    }

    void ApplyRtsCost(FDemocracyCountryState& Country, int32 TreasuryCost, int32 FoodCost, int32 FuelCost, int32 WoodCost, int32 MetalsCost)
    {
        Country.Treasury = FMath::Max(0, Country.Treasury - TreasuryCost);
        Country.Resources.Food = FMath::Max(0, Country.Resources.Food - FoodCost);
        Country.Resources.GasOil = FMath::Max(0, Country.Resources.GasOil - FuelCost);
        Country.Resources.Wood = FMath::Max(0, Country.Resources.Wood - WoodCost);
        Country.Resources.Metals = FMath::Max(0, Country.Resources.Metals - MetalsCost);
    }

    void RefundRtsCost(FDemocracyCountryState& Country, const FDemocracyRtsConstructionQueueEntryState& QueueEntry)
    {
        Country.Treasury += QueueEntry.TreasuryCost / 2;
        Country.Resources.Food += QueueEntry.FoodCost / 2;
        Country.Resources.GasOil += QueueEntry.FuelCost / 2;
        Country.Resources.Wood += QueueEntry.WoodCost / 2;
        Country.Resources.Metals += QueueEntry.MetalsCost / 2;
    }
    void GetRtsActionCost(const FString& ActionName, int32& TreasuryCost, int32& FoodCost, int32& FuelCost, int32& WoodCost, int32& MetalsCost)
    {
        TreasuryCost = 0;
        FoodCost = 0;
        FuelCost = 0;
        WoodCost = 0;
        MetalsCost = 0;
        if (ActionName.Equals(TEXT("Move"), ESearchCase::IgnoreCase) || ActionName.Equals(TEXT("Rally"), ESearchCase::IgnoreCase))
        {
            TreasuryCost = 6;
            FoodCost = 2;
            FuelCost = 3;
        }
        else if (ActionName.Equals(TEXT("Defend"), ESearchCase::IgnoreCase))
        {
            TreasuryCost = 5;
            FoodCost = 2;
            MetalsCost = 1;
        }
        else if (ActionName.Equals(TEXT("Patrol/Scout"), ESearchCase::IgnoreCase))
        {
            TreasuryCost = 4;
            FuelCost = 2;
        }
        else if (ActionName.Equals(TEXT("Reinforce"), ESearchCase::IgnoreCase))
        {
            TreasuryCost = 12;
            FoodCost = 4;
            FuelCost = 5;
            MetalsCost = 4;
        }
        else if (ActionName.Equals(TEXT("Battle"), ESearchCase::IgnoreCase))
        {
            TreasuryCost = 18;
            FoodCost = 6;
            FuelCost = 8;
            MetalsCost = 7;
        }
        else if (ActionName.Equals(TEXT("Occupation"), ESearchCase::IgnoreCase))
        {
            TreasuryCost = 10;
            FoodCost = 4;
            WoodCost = 2;
        }
        else if (ActionName.Equals(TEXT("Diplomacy"), ESearchCase::IgnoreCase))
        {
            TreasuryCost = 16;
        }
    }

    bool TryApplyRtsActionCost(FDemocracySimulationState& State, const FString& ActionName, FString& OutCostSummary, int32 Multiplier = 1)
    {
        int32 TreasuryCost = 0;
        int32 FoodCost = 0;
        int32 FuelCost = 0;
        int32 WoodCost = 0;
        int32 MetalsCost = 0;
        if (const FDemocracyRtsActionRuleState* Rule = FindRtsActionRule(State.RtsWorld, ActionName))
        {
            TreasuryCost = Rule->TreasuryCost;
            FoodCost = Rule->FoodCost;
            FuelCost = Rule->FuelCost;
            WoodCost = Rule->WoodCost;
            MetalsCost = Rule->MetalsCost;
            if (State.PlayerCountry.MilitaryReadiness < Rule->MinReadiness)
            {
                OutCostSummary = FString::Printf(TEXT("%s requires military readiness %d before it can be ordered."), *Rule->DisplayName, Rule->MinReadiness);
                return false;
            }
            if (Rule->bRequiresWar && !DoesRtsRulesetHaveWarAuthority(State))
            {
                OutCostSummary = FString::Printf(TEXT("%s requires active war authority from the office."), *Rule->DisplayName);
                return false;
            }
        }
        else
        {
            GetRtsActionCost(ActionName, TreasuryCost, FoodCost, FuelCost, WoodCost, MetalsCost);
        }
        const int32 CostMultiplier = FMath::Max(1, Multiplier);
        TreasuryCost *= CostMultiplier;
        FoodCost *= CostMultiplier;
        FuelCost *= CostMultiplier;
        WoodCost *= CostMultiplier;
        MetalsCost *= CostMultiplier;
        OutCostSummary = BuildRtsCostText(TreasuryCost, FoodCost, FuelCost, WoodCost, MetalsCost);
        if (!CanPayRtsCost(State.PlayerCountry, TreasuryCost, FoodCost, FuelCost, WoodCost, MetalsCost))
        {
            return false;
        }
        ApplyRtsCost(State.PlayerCountry, TreasuryCost, FoodCost, FuelCost, WoodCost, MetalsCost);
        State.RtsWorld.Backflow.BudgetStrainPressure = FMath::Clamp(State.RtsWorld.Backflow.BudgetStrainPressure + FMath::Max(1, TreasuryCost / 12), 0, 100);
        return true;
    }
    void RefreshRtsBackflowCounters(FDemocracyRtsBackflowState& Backflow)
    {
        Backflow.PendingOutcomeCount = Backflow.PendingOutcomes.Num();
        Backflow.PendingAttentionCount = 0;
        Backflow.BattleLossCount = 0;
        Backflow.ProvinceCaptureCount = 0;
        Backflow.CapitalThreatCount = 0;
        Backflow.SupplyRouteBreakCount = 0;
        for (const FDemocracyRtsOutcomeState& Outcome : Backflow.PendingOutcomes)
        {
            if (Outcome.bRequiresSimulationAttention && !Outcome.bAcknowledgedBySimulation)
            {
                ++Backflow.PendingAttentionCount;
            }
            if (Outcome.ImportEventType.Equals(TEXT("Battle Lost"), ESearchCase::IgnoreCase))
            {
                ++Backflow.BattleLossCount;
            }
            else if (Outcome.ImportEventType.Contains(TEXT("Province")))
            {
                ++Backflow.ProvinceCaptureCount;
            }
            else if (Outcome.ImportEventType.Equals(TEXT("Capital Threatened"), ESearchCase::IgnoreCase))
            {
                ++Backflow.CapitalThreatCount;
            }
            else if (Outcome.ImportEventType.Equals(TEXT("Supply Route Broken"), ESearchCase::IgnoreCase))
            {
                ++Backflow.SupplyRouteBreakCount;
            }
        }
        Backflow.TotalTerritoryDelta = 0;
        Backflow.TotalCasualties = 0;
        int32 RecentFatigue = 0;
        int32 RecentDisruption = 0;
        int32 RecentBudgetStrain = 0;
        int32 RecentDiplomaticDamage = 0;
        const int32 StartIndex = FMath::Max(0, Backflow.OutcomeHistory.Num() - 8);
        for (int32 Index = StartIndex; Index < Backflow.OutcomeHistory.Num(); ++Index)
        {
            const FDemocracyRtsOutcomeState& Outcome = Backflow.OutcomeHistory[Index];
            Backflow.TotalTerritoryDelta += Outcome.TerritoryDelta;
            Backflow.TotalCasualties += Outcome.Casualties;
            RecentFatigue += Outcome.WarFatigueDelta;
            RecentDisruption += Outcome.ResourceDisruption;
            RecentBudgetStrain += Outcome.BudgetStrain;
            RecentDiplomaticDamage += Outcome.DiplomaticDamage;
        }
        Backflow.WarFatigue = FMath::Clamp(RecentFatigue, 0, 100);
        Backflow.ResourceDisruptionPressure = FMath::Clamp(RecentDisruption, 0, 100);
        Backflow.BudgetStrainPressure = FMath::Clamp(RecentBudgetStrain, 0, 100);
        Backflow.DiplomaticDamagePressure = FMath::Clamp(RecentDiplomaticDamage, 0, 100);
        Backflow.LastImportQueueSummary = Backflow.PendingAttentionCount > 0
            ? FString::Printf(TEXT("%d RTS result(s) are queued for simulation attention before deadlines advance."), Backflow.PendingAttentionCount)
            : TEXT("No RTS import queue items are waiting for simulation attention.");
    }

    FDemocracyRtsOutcomeState MakePrototypeRtsOutcome(const FDemocracySimulationState& State, const FString& OutcomeType)
    {
        FDemocracyRtsOutcomeState Outcome;
        Outcome.Turn = State.Turn;
        Outcome.OutcomeId = FString::Printf(TEXT("RTS-%d-%d"), State.Turn, State.RtsWorld.Backflow.OutcomeHistory.Num() + State.RtsWorld.Backflow.PendingOutcomes.Num() + 1);
        Outcome.ConflictName = TEXT("Border Pressure Exchange");
        Outcome.OpponentCountry = State.RtsWorld.Rivals.Num() > 0 ? State.RtsWorld.Rivals[0].CountryName : TEXT("Unknown Rival");
        Outcome.OutcomeType = OutcomeType;
        if (OutcomeType.Equals(TEXT("Territory Gained"), ESearchCase::IgnoreCase) || OutcomeType.Equals(TEXT("Province Captured"), ESearchCase::IgnoreCase))
        {
            Outcome.TerritoryDelta = 1;
            Outcome.Casualties = 45;
            Outcome.ResourceDisruption = 8;
            Outcome.WarFatigueDelta = 5;
            Outcome.DiplomaticDamage = 4;
            Outcome.StabilityDelta = 1;
            Outcome.InvasionRiskDelta = -6;
            Outcome.BudgetStrain = 9;
            Outcome.Summary = TEXT("RTS layer reports a limited border gain. Simulation receives control gains, casualties, resource disruption, and diplomatic concern.");
            Outcome.ConsequenceTags = { TEXT("rts"), TEXT("territory-gained"), TEXT("casualties"), TEXT("budget-strain") };
        }
        else if (OutcomeType.Equals(TEXT("Territory Lost"), ESearchCase::IgnoreCase) || OutcomeType.Equals(TEXT("Province Lost"), ESearchCase::IgnoreCase))
        {
            Outcome.TerritoryDelta = -1;
            Outcome.Casualties = 80;
            Outcome.ResourceDisruption = 16;
            Outcome.WarFatigueDelta = 10;
            Outcome.DiplomaticDamage = 8;
            Outcome.StabilityDelta = -4;
            Outcome.InvasionRiskDelta = 14;
            Outcome.BudgetStrain = 14;
            Outcome.Summary = TEXT("RTS layer reports a border territory loss. Simulation receives stability damage, heightened takeover risk, casualties, disrupted resources, and budget strain.");
            Outcome.ConsequenceTags = { TEXT("rts"), TEXT("territory-lost"), TEXT("invasion-risk"), TEXT("war-fatigue") };
        }
        else if (OutcomeType.Equals(TEXT("Battle Lost"), ESearchCase::IgnoreCase))
        {
            Outcome.TerritoryDelta = 0;
            Outcome.Casualties = 120;
            Outcome.ResourceDisruption = 10;
            Outcome.WarFatigueDelta = 14;
            Outcome.DiplomaticDamage = 5;
            Outcome.StabilityDelta = -5;
            Outcome.InvasionRiskDelta = 12;
            Outcome.BudgetStrain = 16;
            Outcome.Summary = TEXT("RTS layer reports a battle loss. Simulation receives casualties, readiness loss, unrest pressure, and heightened invasion risk.");
            Outcome.ConsequenceTags = { TEXT("rts"), TEXT("battle-lost"), TEXT("casualties"), TEXT("readiness-loss") };
        }
        else if (OutcomeType.Equals(TEXT("Capital Threatened"), ESearchCase::IgnoreCase))
        {
            Outcome.TerritoryDelta = 0;
            Outcome.Casualties = 60;
            Outcome.ResourceDisruption = 14;
            Outcome.WarFatigueDelta = 12;
            Outcome.DiplomaticDamage = 9;
            Outcome.StabilityDelta = -8;
            Outcome.InvasionRiskDelta = 24;
            Outcome.BudgetStrain = 20;
            Outcome.Summary = TEXT("RTS layer reports the capital is threatened. Simulation receives emergency stability damage, takeover risk, and budget strain.");
            Outcome.ConsequenceTags = { TEXT("rts"), TEXT("capital-threatened"), TEXT("invasion-risk"), TEXT("emergency") };
        }
        else if (OutcomeType.Equals(TEXT("Supply Route Broken"), ESearchCase::IgnoreCase))
        {
            Outcome.TerritoryDelta = 0;
            Outcome.Casualties = 25;
            Outcome.ResourceDisruption = 28;
            Outcome.WarFatigueDelta = 7;
            Outcome.DiplomaticDamage = 4;
            Outcome.StabilityDelta = -3;
            Outcome.InvasionRiskDelta = 8;
            Outcome.BudgetStrain = 18;
            Outcome.Summary = TEXT("RTS layer reports a broken supply route. Simulation receives resource disruption, budget strain, readiness pressure, and unrest risk.");
            Outcome.ConsequenceTags = { TEXT("rts"), TEXT("supply-route-broken"), TEXT("resource-disruption"), TEXT("logistics") };
        }
        else
        {
            Outcome.TerritoryDelta = 0;
            Outcome.Casualties = 35;
            Outcome.ResourceDisruption = 6;
            Outcome.WarFatigueDelta = 4;
            Outcome.DiplomaticDamage = 2;
            Outcome.StabilityDelta = -1;
            Outcome.InvasionRiskDelta = 3;
            Outcome.BudgetStrain = 6;
            Outcome.Summary = TEXT("RTS layer reports a stalemate. Simulation receives casualties, fatigue, resource disruption, and continuing invasion pressure.");
            Outcome.ConsequenceTags = { TEXT("rts"), TEXT("stalemate"), TEXT("attrition") };
        }
        PopulateRtsOutcomeAttentionFields(State, Outcome);
        return Outcome;
    }

    void QueueRtsOutcomeImport(FDemocracySimulationState& State, FDemocracyRtsOutcomeState Outcome)
    {
        if (Outcome.ImportEventType.Equals(TEXT("Unspecified"), ESearchCase::IgnoreCase) || Outcome.AttentionSummary.IsEmpty())
        {
            PopulateRtsOutcomeAttentionFields(State, Outcome);
        }
        Outcome.bAppliedToSimulation = false;
        Outcome.bAcknowledgedBySimulation = false;
        Outcome.bRequiresSimulationAttention = true;
        Outcome.SimulationAttentionStatus = TEXT("Queued");
        State.RtsWorld.Backflow.PendingOutcomes.Add(Outcome);
        const FString OfficeAlertLine = BuildRtsOutcomeOfficeAlertLine(Outcome);
        AddRtsHudAlert(State.RtsWorld, OfficeAlertLine);
        RefreshRtsBackflowCounters(State.RtsWorld.Backflow);
        State.RtsWorld.Backflow.LastImportQueueSummary = OfficeAlertLine;
        if (Outcome.ResourceDisruption > 0)
        {
            State.RtsWorld.Backflow.LastImportQueueSummary += FString::Printf(TEXT(" Resource disruption %+d is queued for simulation."), Outcome.ResourceDisruption);
        }
    }

    void RecordRtsOfficeAlert(FDemocracySimulationState& State, const FString& EventType, const FString& Summary, const FString& ProvinceId = TEXT(""), int32 Severity = 25)
    {
        if (Summary.IsEmpty())
        {
            return;
        }

        FDemocracyRtsOutcomeState Alert;
        Alert.OutcomeId = FString::Printf(TEXT("RTS-ALERT-%d-%d"), State.Turn, State.RtsWorld.Backflow.OutcomeHistory.Num() + State.RtsWorld.Hud.Alerts.Num() + 1);
        Alert.OutcomeType = EventType;
        Alert.ImportEventType = NormalizeRtsImportEventType(EventType);
        Alert.AttentionCategory = RtsAttentionCategoryForImportEvent(Alert.ImportEventType);
        Alert.AffectedCountryName = State.PlayerCountry.CountryName;
        Alert.AffectedProvinceId = ProvinceId;
        Alert.AffectedProvinceName = ProvinceId;
        Alert.Summary = Summary;
        Alert.AttentionSummary = Summary;
        Alert.AttentionSeverity = FMath::Clamp(Severity, 1, 100);
        Alert.AttentionDeadlineTurn = State.Turn + FMath::Clamp(6 - Alert.AttentionSeverity / 20, 2, 6);
        Alert.Turn = State.Turn;
        Alert.bRequiresSimulationAttention = true;
        Alert.bAcknowledgedBySimulation = true;
        Alert.bAppliedToSimulation = true;
        Alert.SimulationAttentionStatus = TEXT("Recorded");
        Alert.ConsequenceTags = { TEXT("rts"), TEXT("office-alert"), EventType };

        for (const FDemocracyProvinceOwnershipState& Province : State.RtsWorld.Ownership.Provinces)
        {
            if (!ProvinceId.IsEmpty() && Province.ProvinceId.Equals(ProvinceId, ESearchCase::IgnoreCase))
            {
                Alert.AffectedCountryName = Province.CurrentControllerCountryName.IsEmpty() ? Province.CurrentOwnerCountryName : Province.CurrentControllerCountryName;
                Alert.AffectedProvinceName = Province.ProvinceName;
                Alert.AffectedResource = Province.ResourceFocus;
                break;
            }
        }

        const FString OfficeAlertLine = BuildRtsOutcomeOfficeAlertLine(Alert);
        State.RtsWorld.Backflow.LastImportQueueSummary = OfficeAlertLine;
        State.RtsWorld.Backflow.OutcomeHistory.Add(Alert);
        TrimRtsBackflowHistory(State.RtsWorld.Backflow);
        RefreshRtsBackflowCounters(State.RtsWorld.Backflow);
        AddRtsHudAlert(State.RtsWorld, OfficeAlertLine);
    }

    void QueuePrototypeRtsOutcomeIfNeeded(FDemocracySimulationState& State)
    {
        if (State.RtsWorld.Backflow.PendingOutcomes.Num() > 0 || State.RtsWorld.Backflow.OutcomeHistory.Num() > 0)
        {
            return;
        }
        State.RtsWorld.Backflow.LastOutcomeSummary = TEXT("RTS backflow bridge is initialized. No tactical result has been reported yet.");
        RefreshRtsBackflowCounters(State.RtsWorld.Backflow);
    }

    int32 RuntimeDesiredProvinceCountForCountry(const FDemocracyGeneratedCountryState& Country, bool bPlayerCountry, int32 PlayerProvinceTarget)
    {
        const bool bNorthernCold = Country.Climate.Equals(TEXT("Northern Cold"), ESearchCase::IgnoreCase);
        const bool bSouthernTropical = Country.Climate.Equals(TEXT("Southern Tropical"), ESearchCase::IgnoreCase);
        const int32 ClimateMinimum = (!bNorthernCold && !bSouthernTropical) ? 12 : 8;
        const int32 SizeScaledTarget = ClimateMinimum + Country.AreaWeight / 18 + Country.PowerScore / 40 + Country.BorderPressure / 70 + (bPlayerCountry ? PlayerProvinceTarget / 5 : 0);
        if (Country.DesiredProvinceCount > 0)
        {
            return FMath::Clamp(FMath::Max(Country.DesiredProvinceCount, ClimateMinimum), ClimateMinimum, 18);
        }
        return FMath::Clamp(SizeScaledTarget, ClimateMinimum, 18);
    }

    FString RuntimeProvinceResourceFocus(int32 ProvinceIndex, const FString& Climate, int32 ProvinceCount)
    {
        const bool bNorthernCold = Climate.Equals(TEXT("Northern Cold"), ESearchCase::IgnoreCase);
        const bool bSouthernTropical = Climate.Equals(TEXT("Southern Tropical"), ESearchCase::IgnoreCase);

        if (!bNorthernCold && !bSouthernTropical)
        {
            static const TCHAR* ModerateFocuses[] = {
                TEXT("Food"), TEXT("Wood"), TEXT("Metals"), TEXT("Fuel"),
                TEXT("Food"), TEXT("Wood"), TEXT("Metals"), TEXT("Fuel"),
                TEXT("Food"), TEXT("Wood"), TEXT("Metals"), TEXT("Fuel")
            };
            return ModerateFocuses[ProvinceIndex % 12];
        }

        if (bNorthernCold)
        {
            static const TCHAR* NorthernBaseAndExclusive[] = {
                TEXT("Food"), TEXT("Wood"), TEXT("Metals"),
                TEXT("Fuel"), TEXT("Fuel"), TEXT("Metals"), TEXT("Wood"), TEXT("Water")
            };
            return NorthernBaseAndExclusive[ProvinceIndex % 8];
        }

        static const TCHAR* SouthernBaseAndExclusive[] = {
            TEXT("Food"), TEXT("Wood"), TEXT("Metals"),
            TEXT("Food"), TEXT("Food"), TEXT("Wood"), TEXT("Water"), TEXT("Metals")
        };
        return SouthernBaseAndExclusive[ProvinceIndex % 8];
    }
    FString RuntimeTerrainTypeForProvince(int32 ProvinceIndex, const FString& Climate, const FString& ResourceFocus)
    {
        if (Climate.Equals(TEXT("Northern Cold"), ESearchCase::IgnoreCase))
        {
            return ProvinceIndex % 3 == 0 ? TEXT("Mountain") : TEXT("Tundra");
        }
        if (Climate.Equals(TEXT("Southern Tropical"), ESearchCase::IgnoreCase))
        {
            return ResourceFocus.Equals(TEXT("Food"), ESearchCase::IgnoreCase) ? TEXT("Rainforest Farmland") : TEXT("Coastal Jungle");
        }
        if (ResourceFocus.Equals(TEXT("Metals"), ESearchCase::IgnoreCase))
        {
            return TEXT("Highlands");
        }
        if (ResourceFocus.Equals(TEXT("Fuel"), ESearchCase::IgnoreCase))
        {
            return TEXT("Basin");
        }
        return ProvinceIndex % 2 == 0 ? TEXT("Plains") : TEXT("Urban Corridor");
    }
    void RefreshRuntimeMapOwnership(FDemocracyMapOwnershipState& Ownership)
    {
        Ownership.TotalCountries = Ownership.Countries.Num();
        Ownership.TotalProvinces = Ownership.Provinces.Num();
        Ownership.PlayerControlledProvinces = 0;
        Ownership.ContestedProvinces = 0;
        Ownership.BorderProvinceCount = 0;
        Ownership.TotalMapRegionCount = Ownership.Continents.Num();
        Ownership.TotalPopulationWeight = 0;
        Ownership.TotalAreaWeight = 0;
        for (FDemocracyCountryOwnershipState& Country : Ownership.Countries)
        {
            Country.TotalProvinces = Country.ProvinceIds.Num();
            Country.ControlledProvinces = 0;
            Country.OccupiedProvinces = 0;
            Country.LostProvinces = 0;
            Country.BorderProvinces = 0;
            Country.ResourceBase = 0;
            Country.MilitaryValue = 0;
        }
        for (FDemocracyContinentOwnershipState& Continent : Ownership.Continents)
        {
            Continent.ProvinceCount = 0;
            Continent.PlayerControlledProvinces = 0;
            Continent.ContestedProvinces = 0;
        }
        for (const FDemocracyProvinceOwnershipState& Province : Ownership.Provinces)
        {
            if (Province.bPlayerControlled) ++Ownership.PlayerControlledProvinces;
            if (!Province.CurrentControllerCountryName.Equals(Province.OriginalCountryName, ESearchCase::IgnoreCase)) ++Ownership.ContestedProvinces;
            if (Province.bBorderProvince) ++Ownership.BorderProvinceCount;
            Ownership.TotalPopulationWeight += Province.PopulationWeight;
            Ownership.TotalAreaWeight += Province.AreaWeight;
            for (FDemocracyCountryOwnershipState& Country : Ownership.Countries)
            {
                if (Country.CountryName.Equals(Province.OriginalCountryName, ESearchCase::IgnoreCase))
                {
                    if (Province.CurrentControllerCountryName.Equals(Country.CountryName, ESearchCase::IgnoreCase)) ++Country.ControlledProvinces; else ++Country.LostProvinces;
                    if (Province.bBorderProvince) ++Country.BorderProvinces;
                    Country.ResourceBase += Province.StrategicValue;
                    Country.MilitaryValue += Province.bBorderProvince ? Province.StrategicValue * 2 : Province.StrategicValue;
                    break;
                }
            }
            for (FDemocracyCountryOwnershipState& Country : Ownership.Countries)
            {
                if (!Province.CurrentControllerCountryName.Equals(Province.OriginalCountryName, ESearchCase::IgnoreCase) && Country.CountryName.Equals(Province.CurrentControllerCountryName, ESearchCase::IgnoreCase))
                {
                    ++Country.OccupiedProvinces;
                    break;
                }
            }
            for (FDemocracyContinentOwnershipState& Continent : Ownership.Continents)
            {
                if (Continent.ContinentName.Equals(Province.ContinentName, ESearchCase::IgnoreCase))
                {
                    ++Continent.ProvinceCount;
                    if (Province.bPlayerControlled) ++Continent.PlayerControlledProvinces;
                    if (!Province.CurrentControllerCountryName.Equals(Province.OriginalCountryName, ESearchCase::IgnoreCase)) ++Continent.ContestedProvinces;
                    break;
                }
            }
        }
        Ownership.Summary = FString::Printf(TEXT("%s ownership: %d durable countries, %d provinces, %d regions, %d player controlled, %d contested, %d border provinces."), *Ownership.PlanetName, Ownership.TotalCountries, Ownership.TotalProvinces, Ownership.TotalMapRegionCount, Ownership.PlayerControlledProvinces, Ownership.ContestedProvinces, Ownership.BorderProvinceCount);
    }

    void InitializeRuntimeMapOwnershipIfMissing(FDemocracySimulationState& State)
    {
        FDemocracyMapOwnershipState& Ownership = State.RtsWorld.Ownership;
        if (Ownership.Provinces.Num() > 0 && Ownership.Countries.Num() > 0)
        {
            EnsureRuntimeProvinceGeometry(Ownership);
            RefreshRuntimeMapOwnership(Ownership);
            State.RtsWorld.ControlledTerritories = Ownership.PlayerControlledProvinces;
            State.RtsWorld.BorderTerritories = Ownership.BorderProvinceCount;
            return;
        }
        Ownership = FDemocracyMapOwnershipState();
        Ownership.PlanetName = State.WorldMap.PlanetName;
        Ownership.MapDataVersion = State.WorldMap.MapDataVersion;
        Ownership.DurableCountryTarget = State.WorldMap.DurableCountryTarget;
        Ownership.LastUpdatedTurn = State.Turn;
        Ownership.PlayerCountryName = State.PlayerCountry.CountryName;
        int32 ProvinceBudget = FMath::Max(1, State.RtsWorld.ControlledTerritories);
        int32 GlobalCountryIndex = 0;
        for (const FDemocracyContinentState& Continent : State.WorldMap.Continents)
        {
            FDemocracyContinentOwnershipState ContinentOwnership;
            ContinentOwnership.ContinentName = Continent.ContinentName;
            ContinentOwnership.Climate = Continent.Climate;
            ContinentOwnership.CountryCount = Continent.Countries.Num();
            for (const FDemocracyGeneratedCountryState& Country : Continent.Countries)
            {
                FDemocracyCountryOwnershipState CountryOwnership;
                CountryOwnership.CountryId = Country.CountryId.IsEmpty() ? FString::Printf(TEXT("DUL-C%03d"), GlobalCountryIndex + 1) : Country.CountryId;
                CountryOwnership.MapRegionId = Country.MapRegionId.IsEmpty() ? FString::Printf(TEXT("DUL-R%02d"), Ownership.Continents.Num() + 1) : Country.MapRegionId;
                CountryOwnership.MapCountryIndex = Country.MapCountryIndex > 0 ? Country.MapCountryIndex : GlobalCountryIndex + 1;
                CountryOwnership.PopulationWeight = Country.PopulationWeight;
                CountryOwnership.AreaWeight = Country.AreaWeight;
                CountryOwnership.CountryName = Country.CountryName;
                CountryOwnership.ContinentName = Country.ContinentName;
                CountryOwnership.GovernmentType = Country.PoliticalType;
                CountryOwnership.bPlayerCountry = Country.CountryName.Equals(State.PlayerCountry.CountryName, ESearchCase::IgnoreCase);
                ContinentOwnership.CountryNames.Add(Country.CountryName);
                const int32 ProvinceCount = RuntimeDesiredProvinceCountForCountry(Country, CountryOwnership.bPlayerCountry, State.RtsWorld.ControlledTerritories);
                for (int32 ProvinceIndex = 0; ProvinceIndex < ProvinceCount; ++ProvinceIndex)
                {
                    FDemocracyProvinceOwnershipState Province;
                    Province.CountryId = CountryOwnership.CountryId;
                    Province.MapRegionId = CountryOwnership.MapRegionId;
                    Province.ProvinceIndex = ProvinceIndex + 1;
                    Province.ProvinceId = FString::Printf(TEXT("%s-P%02d"), *CountryOwnership.CountryId, Province.ProvinceIndex);
                    Province.ProvinceName = ProvinceIndex == 0 ? FString::Printf(TEXT("%s Capital District"), *Country.CountryName) : FString::Printf(TEXT("%s Province %d"), *Country.CountryName, ProvinceIndex + 1);
                    Province.ContinentName = Country.ContinentName;
                    Province.OriginalCountryName = Country.CountryName;
                    Province.CurrentOwnerCountryName = Country.CountryName;
                    Province.CurrentControllerCountryName = Country.CountryName;
                    Province.GovernmentType = Country.PoliticalType;
                    Province.Climate = Country.Climate;
                    Province.ResourceFocus = RuntimeProvinceResourceFocus(ProvinceIndex, Country.Climate, ProvinceCount);
                    Province.TerrainType = RuntimeTerrainTypeForProvince(ProvinceIndex, Country.Climate, Province.ResourceFocus);
                    Province.PopulationWeight = FMath::Max(1, Country.PopulationWeight / ProvinceCount + (ProvinceIndex == 0 ? 4 : 0));
                    Province.AreaWeight = FMath::Max(1, Country.AreaWeight / ProvinceCount + (ProvinceIndex % 3));
                    Province.StrategicValue = FMath::Clamp(1 + Country.PowerScore / 25 + (ProvinceIndex == 0 ? 2 : 0), 1, 8);
                    Province.Stability = Country.Stability;
                    Province.Unrest = FMath::Clamp(100 - Country.Stability + Country.BorderPressure / 3, 0, 100);
                    Province.bPlayerControlled = ProvinceBudget > 0 || CountryOwnership.bPlayerCountry;
                    if (Province.bPlayerControlled)
                    {
                        Province.CurrentOwnerCountryName = State.PlayerCountry.CountryName;
                        Province.CurrentControllerCountryName = State.PlayerCountry.CountryName;
                        Province.GovernmentType = TEXT("Democracy");
                        --ProvinceBudget;
                    }
                    Province.bBorderProvince = Province.bPlayerControlled || ProvinceIndex == ProvinceCount - 1 || Country.BorderPressure >= 40;
                    Province.LastChangedTurn = State.Turn;
                    AssignRuntimeProvinceGeometry(Province, CountryOwnership, ProvinceCount);
                    Ownership.Provinces.Add(Province);
                    CountryOwnership.ProvinceIds.Add(Province.ProvinceId);
                }
                Ownership.Countries.Add(CountryOwnership);
                ++GlobalCountryIndex;
            }
            Ownership.Continents.Add(ContinentOwnership);
        }
        RefreshRuntimeMapOwnership(Ownership);
        State.RtsWorld.ControlledTerritories = Ownership.PlayerControlledProvinces;
        State.RtsWorld.BorderTerritories = Ownership.BorderProvinceCount;
    }

    bool HasActiveWarAuthority(const FDemocracySimulationState& State);
    bool HasCeasefireLock(const FDemocracySimulationState& State);

    FString ApplyOwnershipTransferFromRtsOutcome(FDemocracySimulationState& State, const FDemocracyRtsOutcomeState& Outcome)
    {
        InitializeRuntimeMapOwnershipIfMissing(State);
        FDemocracyMapOwnershipState& Ownership = State.RtsWorld.Ownership;
        const FString PlayerName = State.PlayerCountry.CountryName;
        FString TransferSummary = TEXT("No province ownership changed.");
        if (Outcome.TerritoryDelta > 0)
        {
            int32 TransfersRemaining = Outcome.TerritoryDelta;
            TArray<FString> Captured;
            for (FDemocracyProvinceOwnershipState& Province : Ownership.Provinces)
            {
                if (TransfersRemaining <= 0) break;
                const bool bOpponentProvince = Province.CurrentControllerCountryName.Equals(Outcome.OpponentCountry, ESearchCase::IgnoreCase) || Province.OriginalCountryName.Equals(Outcome.OpponentCountry, ESearchCase::IgnoreCase);
                if (!Province.bPlayerControlled && (bOpponentProvince || Province.bBorderProvince))
                {
                    if (Province.CurrentOwnerCountryName.IsEmpty())
                    {
                        Province.CurrentOwnerCountryName = Province.OriginalCountryName;
                    }
                    Province.CurrentControllerCountryName = PlayerName;
                    Province.GovernmentType = TEXT("Democracy");
                    Province.bPlayerControlled = true;
                    Province.bBorderProvince = true;
                    Province.LastChangedTurn = State.Turn;
                    Captured.Add(Province.ProvinceName);
                    RecordRtsOfficeAlert(State, TEXT("Province Occupied"), FString::Printf(TEXT("%s entered occupied/contested control under %s. Ownership transfers after peace/ceasefire/surrender timing or long occupation control."), *Province.ProvinceName, *PlayerName), Province.ProvinceId, 50);
                    --TransfersRemaining;
                }
            }
            if (Captured.Num() > 0) TransferSummary = FString::Printf(TEXT("Occupied %s. Ownership transfer pending occupation timer."), *FString::Join(Captured, TEXT(", ")));
        }
        else if (Outcome.TerritoryDelta < 0)
        {
            int32 TransfersRemaining = FMath::Abs(Outcome.TerritoryDelta);
            TArray<FString> Lost;
            for (int32 Index = Ownership.Provinces.Num() - 1; Index >= 0 && TransfersRemaining > 0; --Index)
            {
                FDemocracyProvinceOwnershipState& Province = Ownership.Provinces[Index];
                if (Province.bPlayerControlled && Province.bBorderProvince)
                {
                    if (Province.CurrentOwnerCountryName.IsEmpty())
                    {
                        Province.CurrentOwnerCountryName = PlayerName;
                    }
                    Province.CurrentControllerCountryName = Outcome.OpponentCountry.IsEmpty() ? Province.OriginalCountryName : Outcome.OpponentCountry;
                    Province.bPlayerControlled = false;
                    Province.bBorderProvince = true;
                    Province.LastChangedTurn = State.Turn;
                    Lost.Add(Province.ProvinceName);
                    RecordRtsOfficeAlert(State, TEXT("Province Occupied"), FString::Printf(TEXT("%s entered occupied/contested control under %s. Ownership transfer is pending and can be prevented by restoring control, peace, or surrender terms."), *Province.ProvinceName, *Province.CurrentControllerCountryName), Province.ProvinceId, 70);
                    --TransfersRemaining;
                }
            }
            if (Lost.Num() > 0) TransferSummary = FString::Printf(TEXT("Lost control of %s. Ownership transfer pending occupation timer."), *FString::Join(Lost, TEXT(", ")));
        }
        Ownership.LastUpdatedTurn = State.Turn;
        RefreshRuntimeMapOwnership(Ownership);
        State.RtsWorld.ControlledTerritories = Ownership.PlayerControlledProvinces;
        State.RtsWorld.BorderTerritories = Ownership.BorderProvinceCount;
        return TransferSummary;
    }

    void ResolveRtsOccupationOwnershipTimers(FDemocracySimulationState& State)
    {
        InitializeRuntimeMapOwnershipIfMissing(State);
        FDemocracyMapOwnershipState& Ownership = State.RtsWorld.Ownership;
        constexpr int32 PeaceOccupationTurns = 3;
        constexpr int32 LongOccupationTurns = 8;
        TArray<FString> Transfers;
        TArray<FString> ActiveOccupations;
        TArray<FString> ContestedWarnings;
        const bool bPeaceOrCeasefireAllowsTransfer = !HasActiveWarAuthority(State) || HasCeasefireLock(State);

        for (FDemocracyProvinceOwnershipState& Province : Ownership.Provinces)
        {
            if (Province.CurrentControllerCountryName.IsEmpty())
            {
                Province.CurrentControllerCountryName = Province.CurrentOwnerCountryName.IsEmpty() ? Province.OriginalCountryName : Province.CurrentOwnerCountryName;
            }
            if (Province.CurrentOwnerCountryName.IsEmpty())
            {
                Province.CurrentOwnerCountryName = Province.OriginalCountryName;
            }
            if (Province.CurrentControllerCountryName.Equals(Province.CurrentOwnerCountryName, ESearchCase::IgnoreCase))
            {
                continue;
            }

            const int32 OccupationTurns = FMath::Max(0, State.Turn - Province.LastChangedTurn);
            const bool bControllerIsPlayer = Province.CurrentControllerCountryName.Equals(State.PlayerCountry.CountryName, ESearchCase::IgnoreCase);
            const bool bOwnerIsPlayer = Province.CurrentOwnerCountryName.Equals(State.PlayerCountry.CountryName, ESearchCase::IgnoreCase);
            const bool bPeaceTransferReady = bPeaceOrCeasefireAllowsTransfer && OccupationTurns >= PeaceOccupationTurns;
            const bool bLongOccupationReady = OccupationTurns >= LongOccupationTurns;
            const int32 TargetTurns = bPeaceOrCeasefireAllowsTransfer ? PeaceOccupationTurns : LongOccupationTurns;
            const int32 TurnsRemaining = FMath::Max(0, TargetTurns - OccupationTurns);

            Province.bPlayerControlled = bControllerIsPlayer;
            Province.bBorderProvince = true;
            Province.Unrest = FMath::Clamp(Province.Unrest + (bLongOccupationReady ? 0 : 1) + (bOwnerIsPlayer && !bControllerIsPlayer ? 2 : 0), 0, 100);
            Province.Stability = FMath::Clamp(Province.Stability - (bLongOccupationReady ? 0 : 1) - (bOwnerIsPlayer && !bControllerIsPlayer ? 2 : 0), 0, 100);

            if (!bPeaceTransferReady && !bLongOccupationReady)
            {
                ActiveOccupations.Add(FString::Printf(TEXT("%s contested: owner %s, controller %s, held %d turn(s), transfer in %d turn(s) %s"), *Province.ProvinceName, *Province.CurrentOwnerCountryName, *Province.CurrentControllerCountryName, OccupationTurns, TurnsRemaining, bPeaceOrCeasefireAllowsTransfer ? TEXT("under peace/ceasefire terms") : TEXT("under long occupation rules")));
                if (OccupationTurns == 0 || OccupationTurns == PeaceOccupationTurns || OccupationTurns == LongOccupationTurns - 1)
                {
                    ContestedWarnings.Add(FString::Printf(TEXT("%s remains contested between %s and %s."), *Province.ProvinceName, *Province.CurrentOwnerCountryName, *Province.CurrentControllerCountryName));
                }
                continue;
            }

            Province.CurrentOwnerCountryName = Province.CurrentControllerCountryName;
            Province.bPlayerControlled = Province.CurrentOwnerCountryName.Equals(State.PlayerCountry.CountryName, ESearchCase::IgnoreCase);
            Province.GovernmentType = Province.bPlayerControlled ? TEXT("Democracy") : (Province.GovernmentType.IsEmpty() ? TEXT("Dictatorship") : Province.GovernmentType);
            Province.LastChangedTurn = State.Turn;
            Transfers.Add(FString::Printf(TEXT("%s -> %s (%s)"), *Province.ProvinceName, *Province.CurrentOwnerCountryName, bPeaceTransferReady ? TEXT("peace/surrender transfer") : TEXT("long occupation transfer")));
        }

        if (ActiveOccupations.Num() > 0)
        {
            const FString OccupationSummary = FString::Printf(TEXT("Occupation active: %s."), *FString::Join(ActiveOccupations, TEXT("; ")));
            State.RtsWorld.Backflow.LastImportQueueSummary = OccupationSummary;
            AddRtsHudAlert(State.RtsWorld, OccupationSummary);
        }

        for (const FString& Warning : ContestedWarnings)
        {
            RecordRtsOfficeAlert(State, TEXT("Province Contested"), Warning, TEXT(""), 45);
        }

        if (Transfers.Num() > 0)
        {
            Ownership.LastUpdatedTurn = State.Turn;
            EnsureRuntimeProvinceGeometry(Ownership);
            RefreshRuntimeMapOwnership(Ownership);
            State.RtsWorld.ControlledTerritories = Ownership.PlayerControlledProvinces;
            State.RtsWorld.BorderTerritories = Ownership.BorderProvinceCount;
            const FString FinalizedSummary = FString::Printf(TEXT("Occupation ownership finalized: %s."), *FString::Join(Transfers, TEXT(", ")));
            RecordRtsOfficeAlert(State, TEXT("Occupation Finalized"), FinalizedSummary, TEXT(""), 55);
        }
    }

    void ApplyRtsBackflowOutcome(FDemocracySimulationState& State, FDemocracyRtsOutcomeState& Outcome)
    {
        FDemocracyCountryState& Country = State.PlayerCountry;
        FDemocracyResourceInventory& Resources = Country.Resources;
        FDemocracyRtsBackflowState& Backflow = State.RtsWorld.Backflow;

        const FString OwnershipTransferSummary = ApplyOwnershipTransferFromRtsOutcome(State, Outcome);

        const int32 Disruption = FMath::Clamp(Outcome.ResourceDisruption, 0, 100);
        Resources.Food = FMath::Max(0, Resources.Food - Disruption / 2);
        Resources.Water = FMath::Max(0, Resources.Water - Disruption / 3);
        Resources.GasOil = FMath::Max(0, Resources.GasOil - Disruption);
        Resources.Wood = FMath::Max(0, Resources.Wood - Disruption / 2);
        Resources.Metals = FMath::Max(0, Resources.Metals - Disruption / 2);

        Country.Treasury = FMath::Max(0, Country.Treasury - Outcome.BudgetStrain * 6);
        Country.PublicApproval = FMath::Clamp(Country.PublicApproval - Outcome.WarFatigueDelta / 2 - Outcome.Casualties / 80, 0, 100);
        Country.Stability = FMath::Clamp(Country.Stability + Outcome.StabilityDelta - Outcome.Casualties / 120, 0, 100);
        Country.Unrest = FMath::Clamp(Country.Unrest + Outcome.WarFatigueDelta / 2 + Outcome.Casualties / 90 + FMath::Max(0, -Outcome.TerritoryDelta) * 4, 0, 100);
        Country.DiplomaticStanding = FMath::Clamp(Country.DiplomaticStanding - Outcome.DiplomaticDamage, 0, 100);
        Country.MilitaryReadiness = FMath::Clamp(Country.MilitaryReadiness - Outcome.Casualties / 70 - Disruption / 12 + FMath::Max(0, Outcome.TerritoryDelta), 0, 100);
        Country.EconomicHealth = FMath::Clamp(Country.EconomicHealth - Outcome.BudgetStrain / 3 - Disruption / 12, 0, 100);
        State.EconomyBudget.Debt = FMath::Clamp(State.EconomyBudget.Debt + Outcome.BudgetStrain * 3, 0, FMath::Max(State.EconomyBudget.DebtCapacity * 2, 1));
        State.EconomyBudget.Inflation = FMath::Clamp(State.EconomyBudget.Inflation + Outcome.BudgetStrain / 12 + Disruption / 20, 0, 100);
        State.InvasionRisk.CurrentInvasionRisk = FMath::Clamp(State.InvasionRisk.CurrentInvasionRisk + Outcome.InvasionRiskDelta + FMath::Max(0, -Outcome.TerritoryDelta) * 6, 0, State.InvasionRisk.InvasionRiskTrigger);
        State.FailureRisk.CurrentAssassinationRisk = FMath::Clamp(State.FailureRisk.CurrentAssassinationRisk + Outcome.WarFatigueDelta / 4 + Outcome.Casualties / 160, 0, State.FailureRisk.AssassinationRiskTrigger);

        for (FDemocracyRivalCountryState& Rival : State.RtsWorld.Rivals)
        {
            if (Rival.CountryName.Equals(Outcome.OpponentCountry, ESearchCase::IgnoreCase))
            {
                Rival.BorderPressure = FMath::Clamp(Rival.BorderPressure + Outcome.InvasionRiskDelta / 2 + FMath::Max(0, -Outcome.TerritoryDelta) * 5 - FMath::Max(0, Outcome.TerritoryDelta) * 2, 0, 100);
                Rival.RelationToPlayer = Outcome.TerritoryDelta < 0 ? TEXT("Hostile") : (Outcome.TerritoryDelta > 0 ? TEXT("Pressured") : Rival.RelationToPlayer);
                break;
            }
        }

        for (FDemocracyDiplomacyRelationshipState& Relationship : State.DiplomacyMatrix.Relationships)
        {
            if (Relationship.CountryName.Equals(Outcome.OpponentCountry, ESearchCase::IgnoreCase))
            {
                Relationship.Trust = FMath::Clamp(Relationship.Trust - Outcome.DiplomaticDamage, 0, 100);
                Relationship.BorderTension = FMath::Clamp(Relationship.BorderTension + Outcome.InvasionRiskDelta / 2 + FMath::Max(0, -Outcome.TerritoryDelta) * 6, 0, 100);
                Relationship.RelationshipStatus = Relationship.BorderTension >= 70 ? TEXT("Hostile") : (Relationship.BorderTension >= 45 ? TEXT("Rival") : Relationship.RelationshipStatus);
                Relationship.LastChangedTurn = State.Turn;
                Relationship.Notes.AddUnique(FString::Printf(TEXT("RTS outcome backflow: %s"), *Outcome.OutcomeType));
                break;
            }
        }

                TArray<FString> BackflowCauseLabels;
        if (Outcome.Casualties > 0)
        {
            BackflowCauseLabels.Add(FString::Printf(TEXT("RTS casualties %d increased war fatigue and public concern"), Outcome.Casualties));
            State.FailureRisk.ActiveUnrestCauses.AddUnique(TEXT("RTS casualties and war fatigue"));
        }
        if (Outcome.ResourceDisruption > 0)
        {
            BackflowCauseLabels.Add(FString::Printf(TEXT("RTS resource disruption %d reduced stockpiles and production reliability"), Outcome.ResourceDisruption));
            State.FailureRisk.ActiveUnrestCauses.AddUnique(TEXT("RTS resource disruption"));
        }
        if (Outcome.BudgetStrain > 0)
        {
            BackflowCauseLabels.Add(FString::Printf(TEXT("RTS budget strain %d increased debt/inflation pressure"), Outcome.BudgetStrain));
            State.FailureRisk.ActiveUnrestCauses.AddUnique(TEXT("RTS budget strain"));
        }
        if (Outcome.DiplomaticDamage > 0)
        {
            BackflowCauseLabels.Add(FString::Printf(TEXT("RTS diplomacy damage %d worsened foreign relations"), Outcome.DiplomaticDamage));
            State.InvasionRisk.ActiveInvasionCauses.AddUnique(TEXT("RTS diplomatic damage"));
        }
        if (Outcome.TerritoryDelta != 0)
        {
            BackflowCauseLabels.Add(FString::Printf(TEXT("RTS territory delta %+d changed province control"), Outcome.TerritoryDelta));
            if (Outcome.TerritoryDelta < 0)
            {
                State.InvasionRisk.ActiveInvasionCauses.AddUnique(TEXT("RTS territory lost"));
                State.FailureRisk.ActiveUnrestCauses.AddUnique(TEXT("RTS territory lost"));
            }
        }
        if (Outcome.InvasionRiskDelta > 0)
        {
            State.InvasionRisk.ActiveInvasionCauses.AddUnique(TEXT("RTS invasion risk increased"));
        }

        if (BackflowCauseLabels.Num() > 0)
        {
            FDemocracyApprovalCauseState RtsCause;
            RtsCause.CauseName = FString::Printf(TEXT("RTS Outcome - %s"), *Outcome.OutcomeType);
            RtsCause.Category = TEXT("RTS Backflow");
            RtsCause.ApprovalImpact = -FMath::Clamp(Outcome.WarFatigueDelta / 2 + Outcome.Casualties / 120, 0, 12);
            RtsCause.UnrestImpact = FMath::Clamp(Outcome.WarFatigueDelta / 2 + Outcome.Casualties / 100 + Outcome.ResourceDisruption / 12 + FMath::Max(0, -Outcome.TerritoryDelta) * 3, 0, 18);
            RtsCause.StabilityImpact = FMath::Clamp(Outcome.StabilityDelta - Outcome.Casualties / 140 - Outcome.BudgetStrain / 10, -18, 8);
            RtsCause.Severity = FMath::Clamp(Outcome.AttentionSeverity + Outcome.Casualties / 12 + Outcome.ResourceDisruption / 2 + Outcome.BudgetStrain / 2, 1, 100);
            RtsCause.SourceMetric = Outcome.AffectedProvinceName.IsEmpty() ? Outcome.AffectedProvinceId : Outcome.AffectedProvinceName;
            RtsCause.CurrentStatus = FString::Join(BackflowCauseLabels, TEXT("; "));
            RtsCause.SuggestedResponses = { TEXT("Open advisor warnings"), TEXT("Review budget and resources"), TEXT("Consider diplomacy or ceasefire"), TEXT("Reinforce supply routes") };
            State.ApprovalStability.Causes.Add(RtsCause);
            while (State.ApprovalStability.Causes.Num() > 14)
            {
                State.ApprovalStability.Causes.RemoveAt(0);
            }
            State.ApprovalStability.LastUpdatedTurn = State.Turn;
            State.ApprovalStability.Summary = FString::Printf(TEXT("RTS backflow added cause data: %s"), *RtsCause.CurrentStatus);
            RecordRtsOfficeAlert(State, TEXT("RTS Consequences Applied"), FString::Printf(TEXT("%s Consequences: %s"), *Outcome.OutcomeType, *RtsCause.CurrentStatus), Outcome.AffectedProvinceId, RtsCause.Severity);
        }
Outcome.bAppliedToSimulation = true;
        Outcome.bAcknowledgedBySimulation = true;
        Outcome.SimulationAttentionStatus = TEXT("Consumed");
        Outcome.Turn = State.Turn;
        Backflow.LastAppliedTurn = State.Turn;
        Backflow.LastOutcomeSummary = FString::Printf(TEXT("%s | %s | attention: %s | territories %+d, casualties %d, disruption %d, fatigue %+d, diplomacy -%d, stability %+d, invasion risk %+d, budget strain %d."), *Outcome.Summary, *OwnershipTransferSummary, *Outcome.AttentionSummary, Outcome.TerritoryDelta, Outcome.Casualties, Outcome.ResourceDisruption, Outcome.WarFatigueDelta, Outcome.DiplomaticDamage, Outcome.StabilityDelta, Outcome.InvasionRiskDelta, Outcome.BudgetStrain);
        Backflow.OutcomeHistory.Add(Outcome);
        TrimRtsBackflowHistory(Backflow);
        LogDecision(State, TEXT("RTS Backflow"), Outcome.OutcomeType, FString::Printf(TEXT("%s against %s."), *Outcome.ConflictName, *Outcome.OpponentCountry), Backflow.LastOutcomeSummary, FMath::Clamp(25 + Outcome.Casualties / 6 + FMath::Abs(Outcome.TerritoryDelta) * 15, 0, 100), Outcome.ConsequenceTags);
    }



    FDemocracyProvinceOwnershipState* FindMutableRtsProvinceById(FDemocracySimulationState& State, const FString& ProvinceId)
    {
        for (FDemocracyProvinceOwnershipState& Province : State.RtsWorld.Ownership.Provinces)
        {
            if (Province.ProvinceId.Equals(ProvinceId, ESearchCase::IgnoreCase))
            {
                return &Province;
            }
        }
        return nullptr;
    }

    const FDemocracyProvinceOwnershipState* FindRtsProvinceById(const FDemocracySimulationState& State, const FString& ProvinceId)
    {
        for (const FDemocracyProvinceOwnershipState& Province : State.RtsWorld.Ownership.Provinces)
        {
            if (Province.ProvinceId.Equals(ProvinceId, ESearchCase::IgnoreCase))
            {
                return &Province;
            }
        }
        return nullptr;
    }

    const FDemocracyDiplomacyRelationshipState* FindDiplomacyRelationshipForCountry(const FDemocracySimulationState& State, const FString& CountryName)
    {
        for (const FDemocracyDiplomacyRelationshipState& Relationship : State.DiplomacyMatrix.Relationships)
        {
            if (Relationship.CountryName.Equals(CountryName, ESearchCase::IgnoreCase))
            {
                return &Relationship;
            }
        }
        return nullptr;
    }

    bool HasActiveWarAuthority(const FDemocracySimulationState& State)
    {
        for (const FDemocracyWarConflictState& Conflict : State.WarSystem.ActiveConflicts)
        {
            const bool bCeasefire = Conflict.Status.Contains(TEXT("Ceasefire"), ESearchCase::IgnoreCase);
            const bool bResolved = Conflict.Status.Contains(TEXT("Resolved"), ESearchCase::IgnoreCase) || Conflict.Status.Contains(TEXT("Ended"), ESearchCase::IgnoreCase);
            if (!bCeasefire && !bResolved)
            {
                return true;
            }
        }
        return false;
    }

    bool HasCeasefireLock(const FDemocracySimulationState& State)
    {
        for (const FDemocracyWarConflictState& Conflict : State.WarSystem.ActiveConflicts)
        {
            if (Conflict.Status.Contains(TEXT("Ceasefire"), ESearchCase::IgnoreCase))
            {
                return true;
            }
        }
        return false;
    }

    bool IsRtsProvinceAlliedOrTreatyControlled(const FDemocracySimulationState& State, const FDemocracyProvinceOwnershipState& Province)
    {
        if (Province.CurrentControllerCountryName.Equals(State.PlayerCountry.CountryName, ESearchCase::IgnoreCase))
        {
            return true;
        }
        const FDemocracyDiplomacyRelationshipState* Relationship = FindDiplomacyRelationshipForCountry(State, Province.CurrentControllerCountryName);
        return Relationship
            && (Relationship->RelationshipStatus.Equals(TEXT("Ally"), ESearchCase::IgnoreCase)
                || Relationship->TreatyStatus.Contains(TEXT("Defense"), ESearchCase::IgnoreCase)
                || Relationship->TreatyStatus.Contains(TEXT("Treaty"), ESearchCase::IgnoreCase));
    }

    int32 GetRtsDiplomacySupplyModifier(const FDemocracySimulationState& State, const FDemocracyProvinceOwnershipState& Province)
    {
        const FDemocracyDiplomacyRelationshipState* Relationship = FindDiplomacyRelationshipForCountry(State, Province.CurrentControllerCountryName);
        if (!Relationship)
        {
            return 0;
        }
        int32 Modifier = 0;
        if (Relationship->bSanctionsActive)
        {
            Modifier += 18;
        }
        if (Relationship->RelationshipStatus.Equals(TEXT("Ally"), ESearchCase::IgnoreCase))
        {
            Modifier -= 14;
        }
        if (Relationship->TreatyStatus.Contains(TEXT("Treaty"), ESearchCase::IgnoreCase) || Relationship->TreatyStatus.Contains(TEXT("Trade"), ESearchCase::IgnoreCase))
        {
            Modifier -= 8;
        }
        if (Relationship->BorderTension >= 70)
        {
            Modifier += 8;
        }
        return FMath::Clamp(Modifier, -22, 30);
    }

    FString GetRtsOrderAuthorityBlockReason(const FDemocracySimulationState& State, const FDemocracyRtsArmyGroupState& Army, const FDemocracyProvinceOwnershipState& TargetProvince, const FString& OrderType)
    {
        const bool bHostileTarget = !TargetProvince.CurrentControllerCountryName.Equals(State.PlayerCountry.CountryName, ESearchCase::IgnoreCase)
            && !IsRtsProvinceAlliedOrTreatyControlled(State, TargetProvince);
        const bool bAttackLikeOrder = bHostileTarget && (OrderType.Equals(TEXT("Move"), ESearchCase::IgnoreCase)
            || OrderType.Equals(TEXT("Rally"), ESearchCase::IgnoreCase)
            || OrderType.Equals(TEXT("Defend"), ESearchCase::IgnoreCase)
            || OrderType.Equals(TEXT("Reinforce"), ESearchCase::IgnoreCase));
        if (bAttackLikeOrder && HasCeasefireLock(State))
        {
            return TEXT("Ceasefire is active. Attack and hostile movement orders are locked until the office breaks or resolves the ceasefire.");
        }
        if (bAttackLikeOrder && !HasActiveWarAuthority(State))
        {
            return TEXT("No active war exists. Declare war from the office before ordering an attack into hostile territory.");
        }
        if (OrderType.Equals(TEXT("Reinforce"), ESearchCase::IgnoreCase)
            && !TargetProvince.CurrentControllerCountryName.Equals(State.PlayerCountry.CountryName, ESearchCase::IgnoreCase)
            && !IsRtsProvinceAlliedOrTreatyControlled(State, TargetProvince))
        {
            return TEXT("Reinforcement is only available to player-controlled, allied, or treaty-controlled provinces.");
        }
        if (Army.SupplyStatus < 15 && !OrderType.Equals(TEXT("Defend"), ESearchCase::IgnoreCase))
        {
            return TEXT("Army supply is critically low. Defend or restore supply before issuing movement orders.");
        }
        return TEXT("");
    }
    FDemocracyRtsFogProvinceState* FindMutableFogProvince(FDemocracyRtsFogOfWarState& Fog, const FString& ProvinceId)
    {
        for (FDemocracyRtsFogProvinceState& FogProvince : Fog.Provinces)
        {
            if (FogProvince.ProvinceId.Equals(ProvinceId, ESearchCase::IgnoreCase))
            {
                return &FogProvince;
            }
        }
        return nullptr;
    }

    void EnsureRtsFogAndHudInitialized(FDemocracySimulationState& State)
    {
        FDemocracyRtsFogOfWarState& Fog = State.RtsWorld.FogOfWar;
        if (Fog.Provinces.Num() == 0 && State.RtsWorld.Ownership.Provinces.Num() > 0)
        {
            for (const FDemocracyProvinceOwnershipState& Province : State.RtsWorld.Ownership.Provinces)
            {
                FDemocracyRtsFogProvinceState Entry;
                Entry.ProvinceId = Province.ProvinceId;
                Entry.bKnown = Province.bPlayerControlled || Province.bBorderProvince;
                Entry.VisibilityState = Province.bPlayerControlled ? TEXT("Known") : (Province.bBorderProvince ? TEXT("Scouted") : TEXT("Hidden"));
                Entry.LastScoutedTurn = Entry.bKnown ? State.Turn : 0;
                Entry.ScoutStrength = Province.bPlayerControlled ? 100 : (Province.bBorderProvince ? 35 : 0);
                Entry.bContested = !Province.CurrentOwnerCountryName.Equals(Province.CurrentControllerCountryName, ESearchCase::IgnoreCase);
                Entry.LastKnownOwner = Entry.bKnown ? Province.CurrentOwnerCountryName : TEXT("Unknown");
                Entry.LastKnownController = Entry.bKnown ? Province.CurrentControllerCountryName : TEXT("Unknown");
                Entry.IntelSummary = Entry.bKnown ? FString::Printf(TEXT("%s intel: %s, %s, controller %s."), *Entry.VisibilityState, *Province.ProvinceName, *Province.TerrainType, *Entry.LastKnownController) : TEXT("Hidden province. Scout to reveal ownership, resources, and terrain.");
                Fog.Provinces.Add(Entry);
            }
        }
    }

    void RefreshRtsFogOfWar(FDemocracySimulationState& State)
    {
        EnsureRtsFogAndHudInitialized(State);
        FDemocracyRtsFogOfWarState& Fog = State.RtsWorld.FogOfWar;
        Fog.KnownProvinceCount = 0;
        Fog.ScoutedProvinceCount = 0;
        Fog.HiddenProvinceCount = 0;
        Fog.ContestedProvinceCount = 0;

        for (FDemocracyRtsFogProvinceState& FogProvince : Fog.Provinces)
        {
            if (const FDemocracyProvinceOwnershipState* Province = FindRtsProvinceById(State, FogProvince.ProvinceId))
            {
                const bool bArmyPresent = [&]()
                {
                    for (const FDemocracyRtsArmyGroupState& Army : State.RtsWorld.ArmyGroups)
                    {
                        if (Army.CurrentProvinceId.Equals(Province->ProvinceId, ESearchCase::IgnoreCase) || Army.OrderTargetProvinceId.Equals(Province->ProvinceId, ESearchCase::IgnoreCase))
                        {
                            return true;
                        }
                    }
                    return false;
                }();
                const bool bContested = !Province->CurrentOwnerCountryName.Equals(Province->CurrentControllerCountryName, ESearchCase::IgnoreCase) || State.RtsWorld.Ownership.ContestedProvinces > 0;
                if (Province->bPlayerControlled)
                {
                    FogProvince.VisibilityState = TEXT("Known");
                    FogProvince.bKnown = true;
                    FogProvince.ScoutStrength = FMath::Max(FogProvince.ScoutStrength, 100);
                }
                else if (bArmyPresent || Province->bBorderProvince || FogProvince.ScoutStrength >= 45)
                {
                    FogProvince.VisibilityState = bContested ? TEXT("Contested") : TEXT("Scouted");
                    FogProvince.bKnown = true;
                    FogProvince.ScoutStrength = FMath::Clamp(FMath::Max(FogProvince.ScoutStrength, bArmyPresent ? 70 : 45), 0, 100);
                }
                else
                {
                    FogProvince.VisibilityState = TEXT("Hidden");
                    FogProvince.bKnown = false;
                    FogProvince.ScoutStrength = FMath::Clamp(FogProvince.ScoutStrength - 1, 0, 100);
                }

                FogProvince.bContested = bContested || FogProvince.VisibilityState.Equals(TEXT("Contested"), ESearchCase::IgnoreCase);
                if (FogProvince.bKnown)
                {
                    FogProvince.LastScoutedTurn = State.Turn;
                    FogProvince.LastKnownOwner = Province->CurrentOwnerCountryName;
                    FogProvince.LastKnownController = Province->CurrentControllerCountryName;
                    FogProvince.IntelSummary = FString::Printf(TEXT("%s: %s | %s | resource %s | owner %s | controller %s."), *FogProvince.VisibilityState, *Province->ProvinceName, *Province->TerrainType, *Province->ResourceFocus, *Province->CurrentOwnerCountryName, *Province->CurrentControllerCountryName);
                }
                else
                {
                    FogProvince.LastKnownOwner = TEXT("Unknown");
                    FogProvince.LastKnownController = TEXT("Unknown");
                    FogProvince.IntelSummary = TEXT("Hidden province. Scout to reveal ownership, resources, terrain, and threats.");
                }
            }

            if (FogProvince.VisibilityState.Equals(TEXT("Known"), ESearchCase::IgnoreCase)) { ++Fog.KnownProvinceCount; }
            else if (FogProvince.VisibilityState.Equals(TEXT("Scouted"), ESearchCase::IgnoreCase)) { ++Fog.ScoutedProvinceCount; }
            else if (FogProvince.VisibilityState.Equals(TEXT("Contested"), ESearchCase::IgnoreCase)) { ++Fog.ContestedProvinceCount; }
            else { ++Fog.HiddenProvinceCount; }
        }
        Fog.LastUpdatedTurn = State.Turn;
        Fog.Summary = FString::Printf(TEXT("Fog of war: known %d | scouted %d | contested %d | hidden %d."), Fog.KnownProvinceCount, Fog.ScoutedProvinceCount, Fog.ContestedProvinceCount, Fog.HiddenProvinceCount);
    }

    void RefreshRtsHudState(FDemocracySimulationState& State)
    {
        FDemocracyRtsHudState& Hud = State.RtsWorld.Hud;
        Hud.SelectedUnitOrBuilding = State.RtsWorld.WorldInteraction.ActiveSelectionId.IsEmpty() ? TEXT("None") : State.RtsWorld.WorldInteraction.ActiveSelectionId;
        Hud.SelectedType = State.RtsWorld.WorldInteraction.ActiveSelectionType.IsEmpty() ? TEXT("None") : State.RtsWorld.WorldInteraction.ActiveSelectionType;
        Hud.ResourceSummary = FString::Printf(TEXT("Tick %d | Food %+d | fuel %+d | wood %+d | metals %+d | disruption %d"), State.RtsWorld.RtsTickCount, State.RtsWorld.ResourceCollection.FoodSentToSimulation, State.RtsWorld.ResourceCollection.FuelSentToSimulation, State.RtsWorld.ResourceCollection.WoodSentToSimulation, State.RtsWorld.ResourceCollection.MetalsSentToSimulation, State.RtsWorld.ResourceCollection.DisruptionPenalty);
        Hud.BuildMenuOptions.Reset();
        for (const FDemocracyRtsBuildingState& Building : State.RtsWorld.CityBase.Buildings)
        {
            if (Hud.BuildMenuOptions.Num() >= 6) { break; }
            Hud.BuildMenuOptions.Add(FString::Printf(TEXT("%s L%d cost %d upgrade %d"), *Building.DisplayName, Building.Level, Building.BuildCost, Building.UpgradeCost));
        }
        Hud.ArmyOrderButtons = { TEXT("Move"), TEXT("Defend"), TEXT("Rally"), TEXT("Patrol/Scout"), TEXT("Reinforce") };
        Hud.BuildMenuSummary = FString::Printf(TEXT("Build menu: %d building types | queue %d | upgrades %d | construction ticks every %d RTS tick(s). Ruleset %s: %d terrain rules, %d action gates."), State.RtsWorld.CityBase.Buildings.Num(), State.RtsWorld.CityBase.BuildQueueCount, State.RtsWorld.CityBase.UpgradeQueueCount, State.RtsWorld.RtsTicksPerConstructionTurn, *State.RtsWorld.RulesetProfile.RulesetVersion, State.RtsWorld.RulesetProfile.TerrainRules.Num(), State.RtsWorld.RulesetProfile.ActionRules.Num());
        Hud.ArmyOrderSummary = State.RtsWorld.ArmyGroups.Num() > 0 ? FString::Printf(TEXT("Selected army %s | order %s | morale %d | supply %d."), *State.RtsWorld.ArmyGroups[0].DisplayName, *State.RtsWorld.ArmyGroups[0].ActiveOrderType, State.RtsWorld.ArmyGroups[0].Morale, State.RtsWorld.ArmyGroups[0].SupplyStatus) : TEXT("No army selected.");
        Hud.MinimapSummary = FString::Printf(TEXT("Minimap: %d provinces | known %d | scouted %d | hidden %d | contested %d | rules %s."), State.RtsWorld.Ownership.TotalProvinces, State.RtsWorld.FogOfWar.KnownProvinceCount, State.RtsWorld.FogOfWar.ScoutedProvinceCount, State.RtsWorld.FogOfWar.HiddenProvinceCount, State.RtsWorld.FogOfWar.ContestedProvinceCount, *State.RtsWorld.RulesetProfile.RulesetVersion);
        Hud.Alerts.Reset();
        if (State.RtsWorld.Backflow.PendingOutcomes.Num() > 0)
        {
            Hud.Alerts.Add(FString::Printf(TEXT("%d RTS result(s) need simulation attention."), State.RtsWorld.Backflow.PendingOutcomes.Num()));
            const int32 PendingDisplayCount = FMath::Min(State.RtsWorld.Backflow.PendingOutcomes.Num(), 3);
            for (int32 Index = 0; Index < PendingDisplayCount; ++Index)
            {
                Hud.Alerts.Add(BuildRtsOutcomeOfficeAlertLine(State.RtsWorld.Backflow.PendingOutcomes[Index]));
            }
        }
        const int32 HistoryStart = FMath::Max(0, State.RtsWorld.Backflow.OutcomeHistory.Num() - 2);
        for (int32 Index = State.RtsWorld.Backflow.OutcomeHistory.Num() - 1; Index >= HistoryStart; --Index)
        {
            Hud.Alerts.Add(FString::Printf(TEXT("Recent RTS: %s"), *BuildRtsOutcomeOfficeAlertLine(State.RtsWorld.Backflow.OutcomeHistory[Index])));
        }
        for (const FDemocracyRtsSupplyRouteState& Route : State.RtsWorld.SupplyRoutes) { if (Route.bBroken) { Hud.Alerts.Add(FString::Printf(TEXT("Supply broken: %s"), *Route.ArmyId)); } }
        if (State.RtsWorld.FogOfWar.ContestedProvinceCount > 0) { Hud.Alerts.Add(FString::Printf(TEXT("%d contested province(s) visible."), State.RtsWorld.FogOfWar.ContestedProvinceCount)); }
        while (Hud.Alerts.Num() > 10) { Hud.Alerts.RemoveAt(0); }
        Hud.AlertSummary = Hud.Alerts.Num() > 0 ? FString::Join(Hud.Alerts, TEXT(" | ")) : TEXT("No RTS alerts.");
        Hud.bReturnToOfficeAvailable = true;
    }

    int32 RtsTerrainBattleModifier(const FString& TerrainType, const FString& OrderType)
    {
        if (TerrainType.Contains(TEXT("Mountain")) || TerrainType.Contains(TEXT("Highlands")))
        {
            return OrderType.Equals(TEXT("Defend"), ESearchCase::IgnoreCase) ? 16 : -12;
        }
        if (TerrainType.Contains(TEXT("Urban")) || TerrainType.Contains(TEXT("Capital")))
        {
            return OrderType.Equals(TEXT("Defend"), ESearchCase::IgnoreCase) ? 14 : -5;
        }
        if (TerrainType.Contains(TEXT("Rainforest")) || TerrainType.Contains(TEXT("Jungle")))
        {
            return OrderType.Equals(TEXT("Patrol/Scout"), ESearchCase::IgnoreCase) ? -5 : -9;
        }
        if (TerrainType.Contains(TEXT("Plains")) || TerrainType.Contains(TEXT("Basin")))
        {
            return OrderType.Equals(TEXT("Move"), ESearchCase::IgnoreCase) ? 6 : 0;
        }
        if (TerrainType.Contains(TEXT("Coast")) || TerrainType.Contains(TEXT("River")))
        {
            return OrderType.Equals(TEXT("Reinforce"), ESearchCase::IgnoreCase) ? 5 : -2;
        }
        return 0;
    }

    int32 RtsTerrainVehicleLossModifier(const FString& TerrainType)
    {
        if (TerrainType.Contains(TEXT("Mountain")) || TerrainType.Contains(TEXT("Highlands")) || TerrainType.Contains(TEXT("Rainforest")) || TerrainType.Contains(TEXT("Jungle")))
        {
            return 2;
        }
        if (TerrainType.Contains(TEXT("Urban")) || TerrainType.Contains(TEXT("Capital")))
        {
            return 1;
        }
        return 0;
    }

    int32 RtsTerrainInfantryLossModifier(const FString& TerrainType)
    {
        if (TerrainType.Contains(TEXT("Urban")) || TerrainType.Contains(TEXT("Capital")))
        {
            return 2;
        }
        if (TerrainType.Contains(TEXT("Plains")) || TerrainType.Contains(TEXT("Basin")))
        {
            return 1;
        }
        return 0;
    }

    int32 RtsArmyCompositionStrength(const FDemocracyRtsArmyGroupState& Army, const FString& TerrainType, const FString& OrderType)
    {
        int32 Strength = Army.InfantryCount * 8 + Army.VehicleCount * 22 + Army.AircraftCount * 28 + Army.LogisticsCount * 6 + Army.ScoutCount * 5 + Army.DefensiveUnitCount * 18;
        if (TerrainType.Contains(TEXT("Mountain")) || TerrainType.Contains(TEXT("Highlands")) || TerrainType.Contains(TEXT("Rainforest")) || TerrainType.Contains(TEXT("Jungle")))
        {
            Strength -= Army.VehicleCount * 4;
            Strength += Army.InfantryCount * 2;
            Strength += Army.ScoutCount * 3;
        }
        if (TerrainType.Contains(TEXT("Urban")) || TerrainType.Contains(TEXT("Capital")))
        {
            Strength += Army.InfantryCount * 2;
            Strength += Army.DefensiveUnitCount * 4;
            Strength -= Army.AircraftCount * 2;
        }
        if (OrderType.Equals(TEXT("Defend"), ESearchCase::IgnoreCase))
        {
            Strength += Army.DefensiveUnitCount * 6;
        }
        if (OrderType.Equals(TEXT("Patrol/Scout"), ESearchCase::IgnoreCase))
        {
            Strength += Army.ScoutCount * 5;
            Strength -= Army.VehicleCount * 2;
        }
        return FMath::Clamp(Strength, 0, 999);
    }
    FDemocracyRtsBattleResolutionState ResolveDeterministicRtsBattle(const FDemocracySimulationState& State, const FDemocracyRtsArmyGroupState& Army, const FDemocracyProvinceOwnershipState& Province, const FString& OrderType)
    {
        FDemocracyRtsBattleResolutionState Battle;
        Battle.BattleId = FString::Printf(TEXT("BATTLE-%d-%d-%s"), State.Turn, State.RtsWorld.RtsTickCount, *Army.ArmyId);
        Battle.ArmyId = Army.ArmyId;
        Battle.ProvinceId = Province.ProvinceId;
        Battle.OpponentCountry = Province.CurrentControllerCountryName.Equals(State.PlayerCountry.CountryName, ESearchCase::IgnoreCase) ? (State.RtsWorld.Rivals.Num() > 0 ? State.RtsWorld.Rivals[0].CountryName : TEXT("Unknown Rival")) : Province.CurrentControllerCountryName;
        Battle.TerrainType = Province.TerrainType;
        Battle.ReadinessModifier = FMath::Clamp((State.PlayerCountry.MilitaryReadiness - 45) / 2, -18, 26);
        Battle.TerrainModifier = RtsRulesetTerrainBattleModifier(State.RtsWorld, Province.TerrainType, OrderType);
        Battle.SupplyModifier = FMath::Clamp((Army.SupplyStatus - 55) / 2 - (Army.bSupplyRouteBroken ? 18 : 0) + (FindRtsTerrainRule(State.RtsWorld, Province.TerrainType) ? FindRtsTerrainRule(State.RtsWorld, Province.TerrainType)->SupplyModifier : 0), -38, 22);
        Battle.TechModifier = FMath::Clamp(State.PlayerCountry.Technology / 8, 0, 15);
        Battle.MoraleModifier = FMath::Clamp((Army.Morale - 50) / 2, -22, 22);

        int32 FriendlyReinforcements = 0;
        for (const FDemocracyRtsArmyGroupState& OtherArmy : State.RtsWorld.ArmyGroups)
        {
            if (!OtherArmy.ArmyId.Equals(Army.ArmyId, ESearchCase::IgnoreCase)
                && (OtherArmy.CurrentProvinceId.Equals(Army.CurrentProvinceId, ESearchCase::IgnoreCase) || OtherArmy.CurrentProvinceId.Equals(Province.ProvinceId, ESearchCase::IgnoreCase))
                && OtherArmy.CurrentCountryName.Equals(Army.CurrentCountryName, ESearchCase::IgnoreCase))
            {
                FriendlyReinforcements += FMath::Clamp(OtherArmy.TotalStrength / 16, 2, 10);
            }
        }
        Battle.ReinforcementModifier = FMath::Clamp(FriendlyReinforcements, 0, 18);

        int32 BuildingDefense = 0;
        if (Province.CurrentControllerCountryName.Equals(State.PlayerCountry.CountryName, ESearchCase::IgnoreCase))
        {
            for (const FDemocracyRtsBuildingState& Building : State.RtsWorld.CityBase.Buildings)
            {
                if (Building.bConstructed && !Building.bDisabled)
                {
                    BuildingDefense += FMath::Max(0, Building.DefenseValue / 2);
                }
            }
        }
        Battle.DefensiveStructureModifier = FMath::Clamp(BuildingDefense + Province.StrategicValue / 2 + (Province.bBorderProvince ? 8 : 0), 0, 34);

        const int32 CompositionStrength = RtsArmyCompositionStrength(Army, Province.TerrainType, OrderType);
        Battle.PlayerScore = CompositionStrength + Battle.ReadinessModifier + Battle.TerrainModifier + Battle.SupplyModifier + Battle.TechModifier + Battle.MoraleModifier + Battle.ReinforcementModifier;
        Battle.OpponentScore = FMath::Clamp(Province.StrategicValue * 13 + Province.Unrest / 3 + (Province.bBorderProvince ? 10 : 0) + FMath::Max(0, 60 - Province.Stability) / 2 + Battle.DefensiveStructureModifier, 26, 190);
        const bool bSevereDefeat = Battle.PlayerScore + 26 < Battle.OpponentScore;
        const bool bBadSupplyRetreat = Army.bSupplyRouteBroken && Army.SupplyStatus < 35 && !OrderType.Equals(TEXT("Defend"), ESearchCase::IgnoreCase);
        const bool bScoutRetreat = OrderType.Equals(TEXT("Patrol/Scout"), ESearchCase::IgnoreCase) && Battle.PlayerScore + 10 < Battle.OpponentScore;
        if (Battle.PlayerScore >= Battle.OpponentScore + 15)
        {
            Battle.Result = TEXT("Province Captured");
            Battle.FollowUpActions = { TEXT("Hold Occupation"), TEXT("Fortify"), TEXT("Establish Supply Route"), TEXT("Negotiate Peace"), TEXT("Bring Reinforcements") };
        }
        else if (bSevereDefeat || bBadSupplyRetreat || bScoutRetreat)
        {
            Battle.Result = (bBadSupplyRetreat || bScoutRetreat) ? TEXT("Retreated") : TEXT("Battle Lost");
            Battle.bRetreatRecommended = true;
            Battle.bRetreated = true;
            Battle.FollowUpActions = { TEXT("Retreat"), TEXT("Request Reinforcements"), TEXT("Repair Supply"), TEXT("Defend Border"), TEXT("Negotiate Ceasefire") };
        }
        else
        {
            Battle.Result = TEXT("Stalemate");
            Battle.bRetreatRecommended = Army.SupplyStatus < 45 || Army.Morale < 45;
            Battle.FollowUpActions = { TEXT("Reinforce"), TEXT("Scout"), TEXT("Withdraw"), TEXT("Attack Again"), TEXT("Request Alliance Aid") };
        }

        const int32 LossPressure = Battle.Result.Equals(TEXT("Province Captured"), ESearchCase::IgnoreCase)
            ? FMath::Clamp(Battle.OpponentScore / 18, 1, 9)
            : Battle.Result.Equals(TEXT("Battle Lost"), ESearchCase::IgnoreCase)
                ? FMath::Clamp(Battle.OpponentScore / 10, 4, 22)
                : Battle.Result.Equals(TEXT("Retreated"), ESearchCase::IgnoreCase)
                    ? FMath::Clamp(Battle.OpponentScore / 18, 2, 12)
                    : FMath::Clamp((Battle.PlayerScore + Battle.OpponentScore) / 22, 2, 14);
        Battle.InfantryLosses = FMath::Min(Army.InfantryCount, FMath::Max(0, LossPressure / 2 + RtsTerrainInfantryLossModifier(Province.TerrainType)));
        Battle.VehicleLosses = FMath::Min(Army.VehicleCount, FMath::Max(0, LossPressure / 5 + RtsTerrainVehicleLossModifier(Province.TerrainType)));
        Battle.AircraftLosses = FMath::Min(Army.AircraftCount, FMath::Max(0, LossPressure / 7));
        Battle.LogisticsLosses = FMath::Min(Army.LogisticsCount, FMath::Max(0, LossPressure / 8 + (Army.bSupplyRouteBroken ? 2 : 0) + (Battle.bRetreated ? 1 : 0)));
        Battle.ScoutLosses = FMath::Min(Army.ScoutCount, FMath::Max(0, LossPressure / 9 + (OrderType.Equals(TEXT("Patrol/Scout"), ESearchCase::IgnoreCase) ? 1 : 0)));
        Battle.DefensiveLosses = FMath::Min(Army.DefensiveUnitCount, FMath::Max(0, LossPressure / 6 + (OrderType.Equals(TEXT("Defend"), ESearchCase::IgnoreCase) ? 0 : 1)));

        const FString RulesetNote = State.RtsWorld.RulesetProfile.RulesetVersion.IsEmpty() ? TEXT("fallback terrain rules") : State.RtsWorld.RulesetProfile.RulesetVersion;
        Battle.Summary = FString::Printf(TEXT("%s [%s] resolved in %s with result %s. Player score %d vs opponent score %d. Modifiers readiness %+d, terrain %+d, supply %+d, tech %+d, morale %+d, reinforcements %+d, defenses %+d. Losses soldiers %d, tanks %d, aircraft %d, supply carriers %d, scouts %d, defensive units %d. %s Follow-ups: %s."), *OrderType, *RulesetNote, *Province.ProvinceName, *Battle.Result, Battle.PlayerScore, Battle.OpponentScore, Battle.ReadinessModifier, Battle.TerrainModifier, Battle.SupplyModifier, Battle.TechModifier, Battle.MoraleModifier, Battle.ReinforcementModifier, Battle.DefensiveStructureModifier, Battle.InfantryLosses, Battle.VehicleLosses, Battle.AircraftLosses, Battle.LogisticsLosses, Battle.ScoutLosses, Battle.DefensiveLosses, Battle.bRetreated ? TEXT("Retreat behavior triggered to preserve remaining forces.") : TEXT("No automatic retreat triggered."), *FString::Join(Battle.FollowUpActions, TEXT(", ")));
        return Battle;
    }

    void ApplyRtsBattleLossesToArmy(FDemocracyRtsArmyGroupState& Army, const FDemocracyRtsBattleResolutionState& Battle)
    {
        Army.InfantryCount = FMath::Max(0, Army.InfantryCount - Battle.InfantryLosses);
        Army.VehicleCount = FMath::Max(0, Army.VehicleCount - Battle.VehicleLosses);
        Army.AircraftCount = FMath::Max(0, Army.AircraftCount - Battle.AircraftLosses);
        Army.LogisticsCount = FMath::Max(0, Army.LogisticsCount - Battle.LogisticsLosses);
        Army.ScoutCount = FMath::Max(0, Army.ScoutCount - Battle.ScoutLosses);
        Army.DefensiveUnitCount = FMath::Max(0, Army.DefensiveUnitCount - Battle.DefensiveLosses);
        Army.TotalStrength = FMath::Clamp(Army.InfantryCount * 8 + Army.VehicleCount * 22 + Army.AircraftCount * 28 + Army.LogisticsCount * 6 + Army.ScoutCount * 5 + Army.DefensiveUnitCount * 18, 0, 999);
        if (Battle.bRetreated)
        {
            Army.MovementState = TEXT("Retreated");
        }
    }
    FDemocracyRtsOutcomeState MakeRtsOutcomeFromBattle(const FDemocracySimulationState& State, const FDemocracyRtsBattleResolutionState& Battle)
    {
        FDemocracyRtsOutcomeState Outcome = MakePrototypeRtsOutcome(State, Battle.Result);
        Outcome.OutcomeId = FString::Printf(TEXT("RTS-BATTLE-%d-%d"), State.Turn, State.RtsWorld.Backflow.OutcomeHistory.Num() + State.RtsWorld.Backflow.PendingOutcomes.Num() + 1);
        Outcome.ConflictName = TEXT("Deterministic RTS Battle Placeholder");
        Outcome.OpponentCountry = Battle.OpponentCountry;
        Outcome.AffectedProvinceId = Battle.ProvinceId;
        if (const FDemocracyProvinceOwnershipState* Province = FindRtsProvinceById(State, Battle.ProvinceId))
        {
            Outcome.AffectedProvinceName = Province->ProvinceName;
            Outcome.AffectedResource = Province->ResourceFocus;
        }
        Outcome.Summary = Battle.Summary;
        const int32 PlayerUnitLosses = Battle.InfantryLosses + Battle.VehicleLosses + Battle.AircraftLosses + Battle.LogisticsLosses + Battle.ScoutLosses + Battle.DefensiveLosses;
        if (Battle.Result.Equals(TEXT("Province Captured"), ESearchCase::IgnoreCase))
        {
            Outcome.TerritoryDelta = 1;
            Outcome.Casualties = FMath::Clamp(PlayerUnitLosses * 12 + Battle.OpponentScore / 3, 12, 110);
            Outcome.InvasionRiskDelta = -8;
            Outcome.StabilityDelta = 1;
        }
        else if (Battle.Result.Equals(TEXT("Battle Lost"), ESearchCase::IgnoreCase) || Battle.Result.Equals(TEXT("Retreated"), ESearchCase::IgnoreCase))
        {
            Outcome.TerritoryDelta = 0;
            Outcome.Casualties = FMath::Clamp(PlayerUnitLosses * 18 + Battle.OpponentScore / 2, 55, 220);
            Outcome.InvasionRiskDelta = 12;
            Outcome.StabilityDelta = -4;
        }
        else
        {
            Outcome.TerritoryDelta = 0;
            Outcome.Casualties = FMath::Clamp(PlayerUnitLosses * 15 + (Battle.PlayerScore + Battle.OpponentScore) / 6, 25, 150);
            Outcome.InvasionRiskDelta = 3;
            Outcome.StabilityDelta = -1;
        }
        Outcome.ResourceDisruption = FMath::Clamp(FMath::Max(0, 65 - Battle.SupplyModifier * 2) / 4 + FMath::Abs(Battle.PlayerScore - Battle.OpponentScore) / 12, 4, 30);
        Outcome.WarFatigueDelta = FMath::Clamp(Outcome.Casualties / 18 + (Battle.Result.Equals(TEXT("Battle Lost"), ESearchCase::IgnoreCase) ? 5 : 2), 3, 18);
        Outcome.BudgetStrain = FMath::Clamp(Outcome.Casualties / 15 + FMath::Max(0, -Battle.SupplyModifier), 5, 24);
        Outcome.DiplomaticDamage = Battle.Result.Equals(TEXT("Province Captured"), ESearchCase::IgnoreCase) ? 7 : 4;
        Outcome.Summary = FString::Printf(TEXT("%s Casualties %d | territory %+d | resource disruption %+d | war fatigue %+d | stability %+d | unrest pressure %+d | budget strain %+d | diplomacy damage %+d | invasion risk %+d."), *Battle.Summary, Outcome.Casualties, Outcome.TerritoryDelta, Outcome.ResourceDisruption, Outcome.WarFatigueDelta, Outcome.StabilityDelta, FMath::Max(0, -Outcome.StabilityDelta), Outcome.BudgetStrain, Outcome.DiplomaticDamage, Outcome.InvasionRiskDelta);
        Outcome.ConsequenceTags = { TEXT("rts"), TEXT("deterministic-battle"), Battle.Result, Battle.TerrainType, TEXT("province-control"), TEXT("casualties"), TEXT("resource-disruption"), TEXT("war-fatigue"), TEXT("budget-strain"), TEXT("diplomatic-damage") };
        PopulateRtsOutcomeAttentionFields(State, Outcome);
        return Outcome;
    }

    void TickRtsEnemyAiPlaceholder(FDemocracySimulationState& State)
    {
        if (State.Turn <= 0 || State.Turn % 2 != 0)
        {
            return;
        }

        int32 BorderDefenses = 0;
        int32 CounterPressures = 0;
        int32 CapitalReinforcements = 0;
        int32 WeakRetreats = 0;
        for (FDemocracyProvinceOwnershipState& Province : State.RtsWorld.Ownership.Provinces)
        {
            const bool bPlayerOwns = Province.CurrentOwnerCountryName.Equals(State.PlayerCountry.CountryName, ESearchCase::IgnoreCase);
            const bool bPlayerControls = Province.CurrentControllerCountryName.Equals(State.PlayerCountry.CountryName, ESearchCase::IgnoreCase) || Province.bPlayerControlled;
            if (!bPlayerControls && Province.bBorderProvince && BorderDefenses < 3)
            {
                Province.StrategicValue = FMath::Clamp(Province.StrategicValue + 1, 1, 100);
                Province.Stability = FMath::Clamp(Province.Stability + 1, 0, 100);
                ++BorderDefenses;
            }
            if (!bPlayerOwns && bPlayerControls && CounterPressures < 2)
            {
                Province.Unrest = FMath::Clamp(Province.Unrest + 4, 0, 100);
                Province.Stability = FMath::Clamp(Province.Stability - 2, 0, 100);
                ++CounterPressures;
                RecordRtsOfficeAlert(State, TEXT("Enemy Counterattack"), FString::Printf(TEXT("Enemy AI placeholder is counter-pressuring occupied %s. Unrest +4, stability -2; hold, pacify, fortify, or negotiate transfer."), *Province.ProvinceName), Province.ProvinceId, 58);
            }
            if (!bPlayerControls && Province.ProvinceIndex == 0 && CapitalReinforcements < 2)
            {
                Province.StrategicValue = FMath::Clamp(Province.StrategicValue + 3, 1, 100);
                Province.Stability = FMath::Clamp(Province.Stability + 2, 0, 100);
                ++CapitalReinforcements;
            }
            if (!bPlayerControls && Province.Stability < 25 && Province.Unrest > 70 && WeakRetreats < 2)
            {
                Province.StrategicValue = FMath::Max(1, Province.StrategicValue - 2);
                Province.Unrest = FMath::Max(0, Province.Unrest - 5);
                ++WeakRetreats;
            }
        }

        if (BorderDefenses + CounterPressures + CapitalReinforcements + WeakRetreats > 0)
        {
            State.RtsWorld.WorldInteraction.LastInteractionSummary = FString::Printf(TEXT("Enemy AI placeholder: defended borders %d, counterattacks %d, reinforced capitals %d, weak retreats %d."), BorderDefenses, CounterPressures, CapitalReinforcements, WeakRetreats);
            AddRtsHudAlert(State.RtsWorld, State.RtsWorld.WorldInteraction.LastInteractionSummary);
        }
    }
    void TickRtsMovementOrdersAndSupply(FDemocracySimulationState& State, bool bAdvancedTurn)
    {
        if (State.RtsWorld.ArmyGroups.Num() == 0)
        {
            return;
        }

        for (FDemocracyRtsSupplyRouteState& Route : State.RtsWorld.SupplyRoutes)
        {
            const bool bWasBroken = Route.bBroken;
            const int32 FuelShortage = GetResourceChainShortage(State.ResourceChains, TEXT("Fuel"));
            Route.DistancePenalty = Route.SourceProvinceId.Equals(Route.DestinationProvinceId, ESearchCase::IgnoreCase) ? 0 : 8;
            int32 RouteDiplomacySupplyModifier = 0;
            if (const FDemocracyProvinceOwnershipState* RouteDestination = FindRtsProvinceById(State, Route.DestinationProvinceId))
            {
                RouteDiplomacySupplyModifier = GetRtsDiplomacySupplyModifier(State, *RouteDestination);
            }
            Route.Disruption = FMath::Clamp(State.RtsWorld.Backflow.ResourceDisruptionPressure / 8 + FuelShortage / 10 + Route.DistancePenalty + RouteDiplomacySupplyModifier, 0, 100);
            Route.SupplyStatus = FMath::Clamp(100 - Route.Disruption, 0, 100);
            if (RouteDiplomacySupplyModifier > 0) { Route.Risks.AddUnique(TEXT("sanctions or border tension supply penalty")); }
            if (RouteDiplomacySupplyModifier < 0) { Route.Risks.AddUnique(TEXT("alliance/treaty supply support")); }
            Route.bBroken = Route.SupplyStatus < 35;
            Route.StatusSummary = Route.bBroken ? TEXT("Supply route broken. Army movement, readiness, and combat strength are reduced.") : FString::Printf(TEXT("Supply route open at %d%%."), Route.SupplyStatus);
            if (Route.bBroken)
            {
                Route.Risks.AddUnique(TEXT("broken supply route"));
                if (!bWasBroken)
                {
                    RecordRtsOfficeAlert(State, TEXT("Supply Route Broken"), FString::Printf(TEXT("Supply broken for %s from %s to %s. Fuel/logistics disruption now affects movement and combat strength."), *Route.ArmyId, *Route.SourceProvinceId, *Route.DestinationProvinceId), Route.DestinationProvinceId, 62);
                    RecordRtsOfficeAlert(State, TEXT("Resource Disrupted"), FString::Printf(TEXT("Resource disruption increased because %s lost supply status; fuel and logistics output need attention."), *Route.ArmyId), Route.DestinationProvinceId, 48);
                }
            }
        }

        for (FDemocracyRtsArmyGroupState& Army : State.RtsWorld.ArmyGroups)
        {
            for (const FDemocracyRtsSupplyRouteState& Route : State.RtsWorld.SupplyRoutes)
            {
                if (Route.ArmyId.Equals(Army.ArmyId, ESearchCase::IgnoreCase))
                {
                    Army.SupplyStatus = Route.SupplyStatus;
                    Army.bSupplyRouteBroken = Route.bBroken;
                    if (Route.bBroken)
                    {
                        Army.MovementState = TEXT("Supply Disrupted");
                        Army.Morale = FMath::Clamp(Army.Morale - 1, 0, 100);
                    }
                    break;
                }
            }
        }

        if (!bAdvancedTurn)
        {
            return;
        }

        ResolveRtsOccupationOwnershipTimers(State);

        for (FDemocracyRtsMovementOrderState& Order : State.RtsWorld.MovementOrders)
        {
            if (!Order.bActive || Order.bComplete || Order.bCancelled)
            {
                continue;
            }

            Order.TurnsRemaining = FMath::Max(0, Order.TurnsRemaining - 1);
            FDemocracyRtsArmyGroupState* Army = nullptr;
            for (FDemocracyRtsArmyGroupState& CandidateArmy : State.RtsWorld.ArmyGroups)
            {
                if (CandidateArmy.ArmyId.Equals(Order.ArmyId, ESearchCase::IgnoreCase))
                {
                    Army = &CandidateArmy;
                    break;
                }
            }
            if (!Army)
            {
                Order.StatusSummary = TEXT("Order cannot resolve because the assigned army is missing.");
                continue;
            }
            Army->ActiveOrderType = Order.OrderType;
            Army->OrderTargetProvinceId = Order.TargetProvinceId;
            Army->OrderTurnsRemaining = Order.TurnsRemaining;
            Army->MovementTurnsRemaining = Order.TurnsRemaining;
            Army->DestinationProvinceId = Order.TargetProvinceId;
            Army->MovementState = Order.TurnsRemaining > 0 ? FString::Printf(TEXT("En route: %s"), *Order.OrderType) : Order.OrderType;
            if (Order.TurnsRemaining > 0)
            {
                Order.StatusSummary = FString::Printf(TEXT("Route preview: %s from %s to %s, %d/%d turn(s) remaining."), *Order.OrderType, *Order.SourceProvinceId, *Order.TargetProvinceId, Order.TurnsRemaining, Order.TotalTurns);
                AddRtsHudAlert(State.RtsWorld, Order.StatusSummary);
                continue;
            }

            Order.bComplete = true;
            Order.bActive = false;

            const FDemocracyProvinceOwnershipState* TargetProvince = FindRtsProvinceById(State, Order.TargetProvinceId);
            const bool bHostileTarget = TargetProvince && !TargetProvince->CurrentControllerCountryName.Equals(State.PlayerCountry.CountryName, ESearchCase::IgnoreCase) && !IsRtsProvinceAlliedOrTreatyControlled(State, *TargetProvince);
            const bool bCombatCapableOrder = Order.OrderType.Equals(TEXT("Move"), ESearchCase::IgnoreCase)
                || Order.OrderType.Equals(TEXT("Rally"), ESearchCase::IgnoreCase)
                || Order.OrderType.Equals(TEXT("Reinforce"), ESearchCase::IgnoreCase)
                || Order.OrderType.Equals(TEXT("Defend"), ESearchCase::IgnoreCase)
                || Order.OrderType.Equals(TEXT("Patrol/Scout"), ESearchCase::IgnoreCase);
            if (TargetProvince && bHostileTarget && bCombatCapableOrder)
            {
                const FString AuthorityBlockReason = GetRtsOrderAuthorityBlockReason(State, *Army, *TargetProvince, Order.OrderType);
                if (!AuthorityBlockReason.IsEmpty())
                {
                    Order.bComplete = true;
                    Order.bActive = false;
                    Army->CurrentProvinceId = Order.SourceProvinceId;
                    Army->DestinationProvinceId = Order.SourceProvinceId;
                    Army->MovementState = TEXT("Order Blocked By Authority");
                    Army->OrderTurnsRemaining = 0;
                    Army->MovementTurnsRemaining = 0;
                    Order.StatusSummary = FString::Printf(TEXT("Order stopped before hostile contact: %s"), *AuthorityBlockReason);
                    AddRtsHudAlert(State.RtsWorld, Order.StatusSummary);
                    RecordRtsOfficeAlert(State, TEXT("RTS Order Blocked"), Order.StatusSummary, Order.TargetProvinceId, 35);
                    continue;
                }
                FDemocracyRtsBattleResolutionState Battle = ResolveDeterministicRtsBattle(State, *Army, *TargetProvince, Order.OrderType);
                FString BattleCostSummary;
                if (TryApplyRtsActionCost(State, TEXT("Battle"), BattleCostSummary))
                {
                    Battle.Summary += FString::Printf(TEXT(" Battle operating cost paid: %s."), *BattleCostSummary);
                }
                else
                {
                    State.PlayerCountry.MilitaryReadiness = FMath::Clamp(State.PlayerCountry.MilitaryReadiness - 2, 0, 100);
                    Army->Morale = FMath::Clamp(Army->Morale - 3, 0, 100);
                    Battle.Summary += FString::Printf(TEXT(" Battle underfunded: %s unavailable, readiness -2, morale -3."), *BattleCostSummary);
                    RecordRtsOfficeAlert(State, TEXT("Battle Underfunded"), Battle.Summary, Order.TargetProvinceId, 56);
                }
                ApplyRtsBattleLossesToArmy(*Army, Battle);
                State.RtsWorld.LastBattleTick = State.RtsWorld.RtsTickCount;
                State.RtsWorld.BattleHistory.Add(Battle);
                if (State.RtsWorld.BattleHistory.Num() > 20)
                {
                    State.RtsWorld.BattleHistory.RemoveAt(0, State.RtsWorld.BattleHistory.Num() - 20);
                }
                QueueRtsOutcomeImport(State, MakeRtsOutcomeFromBattle(State, Battle));
                if (FDemocracyRtsFogProvinceState* FogProvince = FindMutableFogProvince(State.RtsWorld.FogOfWar, Order.TargetProvinceId))
                {
                    FogProvince->VisibilityState = Battle.Result.Equals(TEXT("Province Captured"), ESearchCase::IgnoreCase) ? TEXT("Known") : TEXT("Contested");
                    FogProvince->bKnown = true;
                    FogProvince->bContested = !Battle.Result.Equals(TEXT("Province Captured"), ESearchCase::IgnoreCase);
                    FogProvince->ScoutStrength = FMath::Clamp(FogProvince->ScoutStrength + 35, 0, 100);
                    FogProvince->LastScoutedTurn = State.Turn;
                    FogProvince->IntelSummary = Battle.Summary;
                }

                if (Battle.Result.Equals(TEXT("Province Captured"), ESearchCase::IgnoreCase))
                {
                    Army->CurrentProvinceId = Order.TargetProvinceId;
                    Army->DestinationProvinceId = Order.TargetProvinceId;
                    Army->MovementState = TEXT("Occupying");
                    Army->Morale = FMath::Clamp(Army->Morale + 4, 0, 100);
                }
                else
                {
                    Army->CurrentProvinceId = Order.SourceProvinceId;
                    Army->DestinationProvinceId = Order.SourceProvinceId;
                    Army->MovementState = Battle.Result;
                    Army->Morale = FMath::Clamp(Army->Morale + (Battle.Result.Equals(TEXT("Battle Lost"), ESearchCase::IgnoreCase) ? -6 : -2), 0, 100);
                }
                Army->ActiveOrderType = TEXT("None");
                Army->OrderTurnsRemaining = 0;
                Army->MovementTurnsRemaining = 0;
                Order.StatusSummary = Battle.Summary;
                continue;
            }

            if (TargetProvince && Order.OrderType.Equals(TEXT("Patrol/Scout"), ESearchCase::IgnoreCase))
            {
                if (FDemocracyRtsFogProvinceState* FogProvince = FindMutableFogProvince(State.RtsWorld.FogOfWar, Order.TargetProvinceId))
                {
                    FogProvince->VisibilityState = TEXT("Known");
                    FogProvince->bKnown = true;
                    FogProvince->bContested = false;
                    FogProvince->ScoutStrength = FMath::Clamp(FogProvince->ScoutStrength + 25, 0, 100);
                    FogProvince->LastScoutedTurn = State.Turn;
                    FogProvince->IntelSummary = FString::Printf(TEXT("%s scouted %s without hostile contact."), *Army->DisplayName, *TargetProvince->ProvinceName);
                }
                Army->CurrentProvinceId = Order.TargetProvinceId;
                Army->DestinationProvinceId = Order.TargetProvinceId;
                Army->MovementState = TEXT("Scouted");
                Army->Morale = FMath::Clamp(Army->Morale + 1, 0, 100);
                Army->ActiveOrderType = TEXT("None");
                Army->OrderTurnsRemaining = 0;
                Army->MovementTurnsRemaining = 0;
                Order.StatusSummary = FString::Printf(TEXT("%s scouted %s without hostile contact."), *Army->DisplayName, *TargetProvince->ProvinceName);
                RecordRtsOfficeAlert(State, TEXT("Movement Complete"), Order.StatusSummary, Order.TargetProvinceId, 20);
                continue;
            }

            if (Order.OrderType.Equals(TEXT("Move"), ESearchCase::IgnoreCase) || Order.OrderType.Equals(TEXT("Rally"), ESearchCase::IgnoreCase) || Order.OrderType.Equals(TEXT("Reinforce"), ESearchCase::IgnoreCase))
            {
                Army->CurrentProvinceId = Order.TargetProvinceId;
                Army->DestinationProvinceId = Order.TargetProvinceId;
                Army->MovementState = Order.OrderType.Equals(TEXT("Reinforce"), ESearchCase::IgnoreCase) ? TEXT("Reinforcing") : TEXT("Arrived");
                Army->Morale = FMath::Clamp(Army->Morale + (Order.OrderType.Equals(TEXT("Reinforce"), ESearchCase::IgnoreCase) ? 3 : 1), 0, 100);
                Order.StatusSummary = FString::Printf(TEXT("%s completed. %s is now at %s."), *Order.OrderType, *Army->DisplayName, *Order.TargetProvinceId);
                RecordRtsOfficeAlert(State, TEXT("Movement Complete"), Order.StatusSummary, Order.TargetProvinceId, 22);
            }
            else if (Order.OrderType.Equals(TEXT("Defend"), ESearchCase::IgnoreCase))
            {
                Army->CurrentProvinceId = Order.TargetProvinceId;
                Army->MovementState = TEXT("Defending");
                Army->Morale = FMath::Clamp(Army->Morale + 2, 0, 100);
                Order.StatusSummary = FString::Printf(TEXT("%s is defending %s."), *Army->DisplayName, *Order.TargetProvinceId);
                RecordRtsOfficeAlert(State, TEXT("Movement Complete"), Order.StatusSummary, Order.TargetProvinceId, 24);
            }
            else if (Order.OrderType.Equals(TEXT("Patrol/Scout"), ESearchCase::IgnoreCase))
            {
                if (const FDemocracyProvinceOwnershipState* Province = FindRtsProvinceById(State, Order.TargetProvinceId))
                {
                    FDemocracyRtsBattleResolutionState Battle = ResolveDeterministicRtsBattle(State, *Army, *Province, Order.OrderType);
                    FString BattleCostSummary;
                if (TryApplyRtsActionCost(State, TEXT("Battle"), BattleCostSummary))
                {
                    Battle.Summary += FString::Printf(TEXT(" Battle operating cost paid: %s."), *BattleCostSummary);
                }
                else
                {
                    State.PlayerCountry.MilitaryReadiness = FMath::Clamp(State.PlayerCountry.MilitaryReadiness - 2, 0, 100);
                    Army->Morale = FMath::Clamp(Army->Morale - 3, 0, 100);
                    Battle.Summary += FString::Printf(TEXT(" Battle underfunded: %s unavailable, readiness -2, morale -3."), *BattleCostSummary);
                    RecordRtsOfficeAlert(State, TEXT("Battle Underfunded"), Battle.Summary, Order.TargetProvinceId, 56);
                }
                ApplyRtsBattleLossesToArmy(*Army, Battle);
                    State.RtsWorld.LastBattleTick = State.RtsWorld.RtsTickCount;
                    State.RtsWorld.BattleHistory.Add(Battle);
                    if (State.RtsWorld.BattleHistory.Num() > 20)
                    {
                        State.RtsWorld.BattleHistory.RemoveAt(0, State.RtsWorld.BattleHistory.Num() - 20);
                    }
                    QueueRtsOutcomeImport(State, MakeRtsOutcomeFromBattle(State, Battle));
                    if (FDemocracyRtsFogProvinceState* FogProvince = FindMutableFogProvince(State.RtsWorld.FogOfWar, Order.TargetProvinceId))
                    {
                        FogProvince->VisibilityState = Battle.Result.Equals(TEXT("Province Captured"), ESearchCase::IgnoreCase) ? TEXT("Known") : TEXT("Contested");
                        FogProvince->bKnown = true;
                        FogProvince->bContested = !Battle.Result.Equals(TEXT("Province Captured"), ESearchCase::IgnoreCase);
                        FogProvince->ScoutStrength = FMath::Clamp(FogProvince->ScoutStrength + 35, 0, 100);
                        FogProvince->LastScoutedTurn = State.Turn;
                        FogProvince->IntelSummary = Battle.Summary;
                    }
                    Army->MovementState = Battle.Result;
                    Army->Morale = FMath::Clamp(Army->Morale + (Battle.Result.Equals(TEXT("Province Captured"), ESearchCase::IgnoreCase) ? 4 : -4), 0, 100);
                    Order.StatusSummary = Battle.Summary;
                }
            }
        }

        for (FDemocracyRtsArmyGroupState& Army : State.RtsWorld.ArmyGroups)
        {
            if (Army.CurrentProvinceId.IsEmpty() || Army.MovementTurnsRemaining > 0)
            {
                continue;
            }

            const FDemocracyProvinceOwnershipState* CurrentProvince = FindRtsProvinceById(State, Army.CurrentProvinceId);
            if (!CurrentProvince || CurrentProvince->CurrentControllerCountryName.Equals(State.PlayerCountry.CountryName, ESearchCase::IgnoreCase))
            {
                continue;
            }

            bool bAlreadyQueuedForThisContact = false;
            for (const FDemocracyRtsOutcomeState& Outcome : State.RtsWorld.Backflow.PendingOutcomes)
            {
                if (Outcome.AffectedProvinceId.Equals(CurrentProvince->ProvinceId, ESearchCase::IgnoreCase) && Outcome.Summary.Contains(Army.ArmyId))
                {
                    bAlreadyQueuedForThisContact = true;
                    break;
                }
            }
            if (bAlreadyQueuedForThisContact)
            {
                continue;
            }

            FDemocracyRtsBattleResolutionState Battle = ResolveDeterministicRtsBattle(State, Army, *CurrentProvince, Army.ActiveOrderType.IsEmpty() ? TEXT("Move") : Army.ActiveOrderType);
            FString BattleCostSummary;
            if (TryApplyRtsActionCost(State, TEXT("Battle"), BattleCostSummary))
            {
                Battle.Summary += FString::Printf(TEXT(" Battle operating cost paid: %s."), *BattleCostSummary);
            }
            else
            {
                State.PlayerCountry.MilitaryReadiness = FMath::Clamp(State.PlayerCountry.MilitaryReadiness - 2, 0, 100);
                Army.Morale = FMath::Clamp(Army.Morale - 3, 0, 100);
                Battle.Summary += FString::Printf(TEXT(" Battle underfunded: %s unavailable, readiness -2, morale -3."), *BattleCostSummary);
                RecordRtsOfficeAlert(State, TEXT("Battle Underfunded"), Battle.Summary, CurrentProvince->ProvinceId, 56);
            }
            ApplyRtsBattleLossesToArmy(Army, Battle);
            State.RtsWorld.LastBattleTick = State.RtsWorld.RtsTickCount;
            Battle.BattleId = FString::Printf(TEXT("BATTLE-CONTACT-%d-%s"), State.Turn, *Army.ArmyId);
            Battle.Summary = FString::Printf(TEXT("Automatic hostile contact: %s. %s"), *Army.ArmyId, *Battle.Summary);
            State.RtsWorld.BattleHistory.Add(Battle);
            if (State.RtsWorld.BattleHistory.Num() > 20)
            {
                State.RtsWorld.BattleHistory.RemoveAt(0, State.RtsWorld.BattleHistory.Num() - 20);
            }
            QueueRtsOutcomeImport(State, MakeRtsOutcomeFromBattle(State, Battle));
            Army.MovementState = Battle.Result;
            Army.Morale = FMath::Clamp(Army.Morale + (Battle.Result.Equals(TEXT("Province Captured"), ESearchCase::IgnoreCase) ? 3 : -4), 0, 100);
            AddRtsHudAlert(State.RtsWorld, FString::Printf(TEXT("Battle triggered in %s: %s"), *CurrentProvince->ProvinceName, *Battle.Result));
        }
        TickRtsEnemyAiPlaceholder(State);
    }

    bool ApplyPendingRtsBackflow(FDemocracySimulationState& State)
    {
        QueuePrototypeRtsOutcomeIfNeeded(State);
        FDemocracyRtsBackflowState& Backflow = State.RtsWorld.Backflow;
        if (Backflow.PendingOutcomes.Num() == 0)
        {
            RefreshRtsBackflowCounters(Backflow);
            return false;
        }

        TArray<FDemocracyRtsOutcomeState> Outcomes = Backflow.PendingOutcomes;
        Backflow.PendingOutcomes.Reset();
        for (FDemocracyRtsOutcomeState& Outcome : Outcomes)
        {
            if (!Outcome.bAppliedToSimulation)
            {
                ApplyRtsBackflowOutcome(State, Outcome);
            }
        }
        RefreshRtsBackflowCounters(Backflow);
        RefreshRtsFogOfWar(State);
        RefreshRtsHudState(State);
        return true;
    }

    FString BuildRtsBackflowSummaryText(const FDemocracyRtsWorldState& RtsWorld)
    {
        const FDemocracyRtsBackflowState& Backflow = RtsWorld.Backflow;
        TArray<FString> Lines;
        Lines.Add(FString::Printf(TEXT("Territories %d controlled | border territories %d | pending outcomes %d | applied history %d"), RtsWorld.ControlledTerritories, RtsWorld.BorderTerritories, Backflow.PendingOutcomes.Num(), Backflow.OutcomeHistory.Num()));
        Lines.Add(FString::Printf(TEXT("Import queue: attention %d | battle losses %d | province changes %d | capital threats %d | supply breaks %d"), Backflow.PendingAttentionCount, Backflow.BattleLossCount, Backflow.ProvinceCaptureCount, Backflow.CapitalThreatCount, Backflow.SupplyRouteBreakCount));
        Lines.Add(FString::Printf(TEXT("Ownership: %d countries | %d provinces | player controlled %d | contested %d | border provinces %d"), RtsWorld.Ownership.TotalCountries, RtsWorld.Ownership.TotalProvinces, RtsWorld.Ownership.PlayerControlledProvinces, RtsWorld.Ownership.ContestedProvinces, RtsWorld.Ownership.BorderProvinceCount));
        Lines.Add(FString::Printf(TEXT("RTS foundation: active view %s | modes %d | base %s | buildings %d | units %d | armies %d | queue %d"), *RtsWorld.ActiveViewMode, RtsWorld.ViewModes.Num(), *RtsWorld.CityBase.DisplayName, RtsWorld.CityBase.Buildings.Num(), RtsWorld.UnitCatalog.Num(), RtsWorld.ArmyGroups.Num(), RtsWorld.CityBase.BuildQueueCount));
        Lines.Add(FString::Printf(TEXT("RTS cadence: tick %d | office turn every %d | construction every %d | last battle tick %d | %s"), RtsWorld.RtsTickCount, RtsWorld.RtsTicksPerSimulationTurn, RtsWorld.RtsTicksPerConstructionTurn, RtsWorld.LastBattleTick, *RtsWorld.TickSummary));
        Lines.Add(FString::Printf(TEXT("RTS collection: food %+d | fuel %+d | wood %+d | metals %+d | penalty %d"), RtsWorld.ResourceCollection.FoodSentToSimulation, RtsWorld.ResourceCollection.FuelSentToSimulation, RtsWorld.ResourceCollection.WoodSentToSimulation, RtsWorld.ResourceCollection.MetalsSentToSimulation, RtsWorld.ResourceCollection.DisruptionPenalty));
        Lines.Add(FString::Printf(TEXT("Backflow impacts: casualties %d | territory %+d | war fatigue %d | resource disruption %d | budget strain %d | diplomatic damage %d"), Backflow.TotalCasualties, Backflow.TotalTerritoryDelta, Backflow.WarFatigue, Backflow.ResourceDisruptionPressure, Backflow.BudgetStrainPressure, Backflow.DiplomaticDamagePressure));
        Lines.Add(FString::Printf(TEXT("Fog: known %d | scouted %d | contested %d | hidden %d | %s"), RtsWorld.FogOfWar.KnownProvinceCount, RtsWorld.FogOfWar.ScoutedProvinceCount, RtsWorld.FogOfWar.ContestedProvinceCount, RtsWorld.FogOfWar.HiddenProvinceCount, *RtsWorld.FogOfWar.Summary));
        Lines.Add(FString::Printf(TEXT("RTS HUD: selected %s %s | %s | alerts: %s"), *RtsWorld.Hud.SelectedType, *RtsWorld.Hud.SelectedUnitOrBuilding, *RtsWorld.Hud.ResourceSummary, *RtsWorld.Hud.AlertSummary));
        Lines.Add(FString::Printf(TEXT("World selection: %s %s | selectable targets %d"), *RtsWorld.WorldInteraction.ActiveSelectionType, *RtsWorld.WorldInteraction.ActiveSelectionId, RtsWorld.WorldInteraction.SelectableTargets.Num()));
        Lines.Add(RtsWorld.ScopeBoundary.ScopeSummary);
        const int32 ViewModeDisplayCount = FMath::Min(RtsWorld.ViewModes.Num(), 2);
        for (int32 Index = 0; Index < ViewModeDisplayCount; ++Index)
        {
            const FDemocracyRtsViewModeState& ViewMode = RtsWorld.ViewModes[Index];
            Lines.Add(FString::Printf(TEXT("View mode | %s | %s | layers: %s"), *ViewMode.DisplayName, *ViewMode.Purpose, *FString::Join(ViewMode.VisibleLayers, TEXT(", "))));
        }
        const int32 BuildingDisplayCount = FMath::Min(RtsWorld.CityBase.Buildings.Num(), 5);
        for (int32 Index = 0; Index < BuildingDisplayCount; ++Index)
        {
            const FDemocracyRtsBuildingState& Building = RtsWorld.CityBase.Buildings[Index];
            Lines.Add(FString::Printf(TEXT("Base building | %s L%d | %s | prod %d | defense %d | health %d/%d | %s"), *Building.DisplayName, Building.Level, *Building.ResourceFocus, Building.ProductionPerTick, Building.DefenseValue, Building.CurrentHealth, Building.MaxHealth, Building.bDisabled ? TEXT("Disabled") : TEXT("Operational")));
        }
        const int32 QueueDisplayCount = FMath::Min(RtsWorld.CityBase.ConstructionQueue.Num(), 4);
        for (int32 Index = 0; Index < QueueDisplayCount; ++Index)
        {
            const FDemocracyRtsConstructionQueueEntryState& QueueEntry = RtsWorld.CityBase.ConstructionQueue[Index];
            Lines.Add(FString::Printf(TEXT("Queue | %s | %s to L%d | %d/%d turns remaining | cost T%d F%d W%d M%d"), *QueueEntry.QueueType, *QueueEntry.DisplayName, QueueEntry.TargetLevel, QueueEntry.TurnsRemaining, QueueEntry.TotalTurns, QueueEntry.TreasuryCost, QueueEntry.FuelCost, QueueEntry.WoodCost, QueueEntry.MetalsCost));
        }
        const int32 OrderDisplayCount = FMath::Min(RtsWorld.MovementOrders.Num(), 5);
        for (int32 Index = 0; Index < OrderDisplayCount; ++Index)
        {
            const FDemocracyRtsMovementOrderState& Order = RtsWorld.MovementOrders[Index];
            Lines.Add(FString::Printf(TEXT("Order | %s | army %s | %s -> %s | %d/%d turns | %s"), *Order.OrderType, *Order.ArmyId, *Order.SourceProvinceId, *Order.TargetProvinceId, Order.TurnsRemaining, Order.TotalTurns, Order.bComplete ? TEXT("Complete") : (Order.bActive ? TEXT("Active") : TEXT("Inactive"))));
        }
        const int32 SupplyDisplayCount = FMath::Min(RtsWorld.SupplyRoutes.Num(), 4);
        for (int32 Index = 0; Index < SupplyDisplayCount; ++Index)
        {
            const FDemocracyRtsSupplyRouteState& Route = RtsWorld.SupplyRoutes[Index];
            Lines.Add(FString::Printf(TEXT("Supply | %s | %s -> %s | status %d | disruption %d | %s"), *Route.ArmyId, *Route.SourceProvinceId, *Route.DestinationProvinceId, Route.SupplyStatus, Route.Disruption, Route.bBroken ? TEXT("Broken") : TEXT("Open")));
        }
        const int32 BattleStartIndex = FMath::Max(0, RtsWorld.BattleHistory.Num() - 3);
        for (int32 Index = RtsWorld.BattleHistory.Num() - 1; Index >= BattleStartIndex; --Index)
        {
            const FDemocracyRtsBattleResolutionState& Battle = RtsWorld.BattleHistory[Index];
            Lines.Add(FString::Printf(TEXT("Battle | %s | %s | score %d vs %d | %s"), *Battle.ProvinceId, *Battle.Result, Battle.PlayerScore, Battle.OpponentScore, *Battle.TerrainType));
        }
        const int32 ArmyDisplayCount = FMath::Min(RtsWorld.ArmyGroups.Num(), 4);
        for (int32 Index = 0; Index < ArmyDisplayCount; ++Index)
        {
            const FDemocracyRtsArmyGroupState& Army = RtsWorld.ArmyGroups[Index];
            Lines.Add(FString::Printf(TEXT("Army | %s | %s -> %s | order %s | strength %d | supply %d | morale %d | units I%d V%d A%d L%d S%d D%d"), *Army.DisplayName, *Army.CurrentProvinceId, Army.DestinationProvinceId.IsEmpty() ? TEXT("hold") : *Army.DestinationProvinceId, *Army.ActiveOrderType, Army.TotalStrength, Army.SupplyStatus, Army.Morale, Army.InfantryCount, Army.VehicleCount, Army.AircraftCount, Army.LogisticsCount, Army.ScoutCount, Army.DefensiveUnitCount));
        }
        const int32 TargetDisplayCount = FMath::Min(RtsWorld.WorldInteraction.SelectableTargets.Num(), 6);
        for (int32 Index = 0; Index < TargetDisplayCount; ++Index)
        {
            const FDemocracyRtsSelectableTargetState& Target = RtsWorld.WorldInteraction.SelectableTargets[Index];
            Lines.Add(FString::Printf(TEXT("Target | %s | %s | %s | actions: %s"), *Target.TargetType, *Target.DisplayName, Target.bSelected ? TEXT("Selected") : TEXT("Selectable"), *FString::Join(Target.AvailableActions, TEXT(", "))));
        }
        const int32 UnitDisplayCount = FMath::Min(RtsWorld.UnitCatalog.Num(), 6);
        for (int32 Index = 0; Index < UnitDisplayCount; ++Index)
        {
            const FDemocracyRtsUnitDefinitionState& Unit = RtsWorld.UnitCatalog[Index];
            Lines.Add(FString::Printf(TEXT("Unit | %s | %s | atk %d def %d move %d | produced by %s"), *Unit.UnitCategory, *Unit.DisplayName, Unit.AttackPower, Unit.DefensePower, Unit.Mobility, *Unit.ProducedByBuildingId));
        }
        Lines.Add(FString::Printf(TEXT("Totals: territory %+d | casualties %d | fatigue %d | disruption %d | budget strain %d | diplomatic damage %d"), Backflow.TotalTerritoryDelta, Backflow.TotalCasualties, Backflow.WarFatigue, Backflow.ResourceDisruptionPressure, Backflow.BudgetStrainPressure, Backflow.DiplomaticDamagePressure));
        Lines.Add(Backflow.LastImportQueueSummary);
        Lines.Add(Backflow.LastOutcomeSummary);
        const int32 PendingDisplayCount = FMath::Min(Backflow.PendingOutcomes.Num(), 5);
        for (int32 Index = 0; Index < PendingDisplayCount; ++Index)
        {
            const FDemocracyRtsOutcomeState& Outcome = Backflow.PendingOutcomes[Index];
            Lines.Add(FString::Printf(TEXT("Queued | %s | %s | due turn %d | severity %d"), *Outcome.ImportEventType, *Outcome.AttentionSummary, Outcome.AttentionDeadlineTurn, Outcome.AttentionSeverity));
        }
        const int32 StartIndex = FMath::Max(0, Backflow.OutcomeHistory.Num() - 4);
        for (int32 Index = Backflow.OutcomeHistory.Num() - 1; Index >= StartIndex; --Index)
        {
            const FDemocracyRtsOutcomeState& Outcome = Backflow.OutcomeHistory[Index];
            Lines.Add(FString::Printf(TEXT("Turn %d | %s vs %s | territories %+d | casualties %d | invasion risk %+d"), Outcome.Turn, *Outcome.OutcomeType, *Outcome.OpponentCountry, Outcome.TerritoryDelta, Outcome.Casualties, Outcome.InvasionRiskDelta));
        }
        return FString::Join(Lines, TEXT("\n"));
    }

    void RefreshWarConflictState(FDemocracySimulationState& State)
    {
        State.WarSystem = FDemocracyGameStateFactory::BuildWarConflictState(State);
    }

    void RefreshSimulationToRtsContract(FDemocracySimulationState& State)
    {
        State.RtsSaveBoundary = FDemocracyGameStateFactory::BuildRtsSaveBoundaryState(State);
        State.SimulationToRtsContract = FDemocracyGameStateFactory::BuildSimulationToRtsContractState(State);
        State.RtsSaveBoundary = FDemocracyGameStateFactory::BuildRtsSaveBoundaryState(State);
        State.SimulationToRtsContract = FDemocracyGameStateFactory::BuildSimulationToRtsContractState(State);
    }

    void RefreshCommandAuthority(FDemocracySimulationState& State)
    {
        if (State.CommandAuthority.Actions.Num() == 0)
        {
            FDemocracyCommandAuthorityState Authority;
            Authority.LastUpdatedTurn = State.Turn;
            Authority.ActiveCommandPosture = State.PlayerCountry.Policies.CivilPolicy.Equals(TEXT("Emergency Powers"), ESearchCase::IgnoreCase) ? TEXT("Emergency Administration") : TEXT("Civil Administration");
            Authority.OfficeAuthoritySummary = TEXT("Office: mobilize, defend, negotiate, embargo, trade, send aid, and declare emergency measures. Direct battles stay in RTS view.");
            Authority.RtsAuthoritySummary = TEXT("RTS View: deploy and attack commands are visible but simulation-office execution is blocked until the RTS layer exists.");
            Authority.Actions = {
                { TEXT("office_mobilize_reserves"), TEXT("Mobilize Reserves"), TEXT("Office"), TEXT("Security"), TEXT("Computer/Defense"), true, false, true, 2, -100, 90, -2, 1, 3, -1, 8, -8, -4, TEXT("Requires treasury and Defense ministry authority."), TEXT("+military, -invasion risk, -treasury, +unrest."), TEXT("") },
                { TEXT("office_defend_borders"), TEXT("Order Border Defense"), TEXT("Office"), TEXT("Security"), TEXT("Computer/Globe"), true, true, true, 1, -100, 55, 0, 1, 1, 0, 5, -5, -2, TEXT("Requires border provinces."), TEXT("Improves readiness and slows invasion pressure."), TEXT("") },
                { TEXT("office_negotiate"), TEXT("Negotiate With Rivals"), TEXT("Office"), TEXT("Diplomacy"), TEXT("Meeting Room/Globe"), true, false, true, 1, -100, 35, 1, 1, -1, 6, -1, -6, 0, TEXT("Requires rival or hostile relation."), TEXT("Improves diplomacy and lowers border tension."), TEXT("") },
                { TEXT("office_embargo"), TEXT("Authorize Embargo"), TEXT("Office"), TEXT("Diplomacy"), TEXT("Computer/Globe"), true, false, true, 3, -100, 25, -1, 0, 1, -4, 0, 3, -2, TEXT("Requires hostile relation or high border tension."), TEXT("Pressures hostile states but damages trade/diplomacy."), TEXT("") },
                { TEXT("office_trade"), TEXT("Expand Trade"), TEXT("Office"), TEXT("Economy"), TEXT("Computer/Treasury"), true, false, true, 1, -100, 40, 1, 0, -1, 4, 0, -1, 8, TEXT("Requires diplomacy above 30."), TEXT("Adds resources and diplomacy at treasury cost."), TEXT("") },
                { TEXT("office_aid"), TEXT("Send Foreign Aid"), TEXT("Office"), TEXT("Diplomacy"), TEXT("Meeting Room/Press"), true, false, true, 2, -100, 70, 1, 1, -1, 5, 0, -2, -3, TEXT("Requires treasury reserve."), TEXT("Improves diplomacy and stability, spends supplies."), TEXT("") },
                { TEXT("office_emergency"), TEXT("Declare Emergency Measures"), TEXT("Office"), TEXT("Emergency"), TEXT("Computer/Phone"), true, false, true, 4, -100, 120, -5, 5, -8, -3, 3, -6, -5, TEXT("Requires unrest or invasion pressure."), TEXT("Strong unrest/risk reduction with legitimacy costs."), TEXT("") },
                { TEXT("office_declare_war"), TEXT("Declare War"), TEXT("Office"), TEXT("War"), TEXT("Computer/Globe"), true, true, true, 8, -100, 180, -8, -6, 10, -15, -4, 18, -8, TEXT("Requires hostile relation or severe border tension and 45 readiness."), TEXT("Creates active war state and pushes battles to RTS backflow."), TEXT("") },
                { TEXT("office_request_alliance_aid"), TEXT("Request Alliance Aid"), TEXT("Office"), TEXT("War"), TEXT("Meeting Room/Globe"), true, false, true, 4, -100, 60, 1, 1, -1, 3, 5, -7, 4, TEXT("Requires ally, treaty partner, or high-trust trade partner."), TEXT("Improves readiness and supplies through diplomatic support."), TEXT("") },
                { TEXT("office_negotiate_ceasefire"), TEXT("Negotiate Ceasefire"), TEXT("Office"), TEXT("War"), TEXT("Meeting Room/Phone"), true, false, true, 3, -100, 45, 2, 3, -3, 5, -2, -8, 0, TEXT("Requires active war or severe border conflict."), TEXT("Lowers escalation, invasion risk, and unrest."), TEXT("") },
                { TEXT("office_surrender_territory"), TEXT("Surrender Territory"), TEXT("Office"), TEXT("War"), TEXT("Meeting Room/Computer"), true, false, true, 10, -100, 20, -12, -8, 12, 2, -10, -15, -12, TEXT("Requires active war with high defeat or takeover risk."), TEXT("Cuts immediate invasion pressure by losing territory/resources."), TEXT("") },
                { TEXT("office_impose_sanctions"), TEXT("Impose Sanctions"), TEXT("Office"), TEXT("War"), TEXT("Computer/Globe"), true, false, true, 4, -100, 35, -2, 0, 2, -6, 0, 4, -4, TEXT("Requires hostile/rival relation, active war, or border tension."), TEXT("Applies sanctions and reduces trade access."), TEXT("") },
                { TEXT("rts_deploy_forces"), TEXT("Deploy Forces"), TEXT("RTS"), TEXT("Military"), TEXT("RTS View"), false, true, false, 1, -100, 0, 0, 0, 0, 0, 0, 0, 0, TEXT("Future RTS layer only."), TEXT("Direct troop deployment belongs to RTS view."), TEXT("Simulation office cannot deploy troops directly.") },
                { TEXT("rts_attack_operation"), TEXT("Launch Attack Operation"), TEXT("RTS"), TEXT("Military"), TEXT("RTS View"), false, true, false, 2, -100, 0, 0, 0, 0, 0, 0, 0, 0, TEXT("Future RTS layer only."), TEXT("Battles later report outcomes through RTS backflow."), TEXT("Simulation office cannot launch direct attacks.") }
            };
            State.CommandAuthority = Authority;
        }

        auto AddMissingCommandAuthorityAction = [&State](const FDemocracyCommandAuthorityActionState& NewAction)
        {
            for (const FDemocracyCommandAuthorityActionState& ExistingAction : State.CommandAuthority.Actions)
            {
                if (ExistingAction.CommandId.Equals(NewAction.CommandId, ESearchCase::IgnoreCase))
                {
                    return;
                }
            }
            State.CommandAuthority.Actions.Add(NewAction);
        };
        AddMissingCommandAuthorityAction({ TEXT("office_declare_war"), TEXT("Declare War"), TEXT("Office"), TEXT("War"), TEXT("Computer/Globe"), true, true, true, 8, -100, 180, -8, -6, 10, -15, -4, 18, -8, TEXT("Requires hostile relation or severe border tension and 45 readiness."), TEXT("Creates active war state and pushes battles to RTS backflow."), TEXT("") });
        AddMissingCommandAuthorityAction({ TEXT("office_request_alliance_aid"), TEXT("Request Alliance Aid"), TEXT("Office"), TEXT("War"), TEXT("Meeting Room/Globe"), true, false, true, 4, -100, 60, 1, 1, -1, 3, 5, -7, 4, TEXT("Requires ally, treaty partner, or high-trust trade partner."), TEXT("Improves readiness and supplies through diplomatic support."), TEXT("") });
        AddMissingCommandAuthorityAction({ TEXT("office_negotiate_ceasefire"), TEXT("Negotiate Ceasefire"), TEXT("Office"), TEXT("War"), TEXT("Meeting Room/Phone"), true, false, true, 3, -100, 45, 2, 3, -3, 5, -2, -8, 0, TEXT("Requires active war or severe border conflict."), TEXT("Lowers escalation, invasion risk, and unrest."), TEXT("") });
        AddMissingCommandAuthorityAction({ TEXT("office_surrender_territory"), TEXT("Surrender Territory"), TEXT("Office"), TEXT("War"), TEXT("Meeting Room/Computer"), true, false, true, 10, -100, 20, -12, -8, 12, 2, -10, -15, -12, TEXT("Requires active war with high defeat or takeover risk."), TEXT("Cuts immediate invasion pressure by losing territory/resources."), TEXT("") });
        AddMissingCommandAuthorityAction({ TEXT("office_impose_sanctions"), TEXT("Impose Sanctions"), TEXT("Office"), TEXT("War"), TEXT("Computer/Globe"), true, false, true, 4, -100, 35, -2, 0, 2, -6, 0, 4, -4, TEXT("Requires hostile/rival relation, active war, or border tension."), TEXT("Applies sanctions and reduces trade access."), TEXT("") });

        State.CommandAuthority.LastUpdatedTurn = State.Turn;
        int32 OfficeCount = 0;
        int32 RtsCount = 0;
        bool bHasWarTarget = false;
        bool bHasAllianceSupport = false;
        int32 HighestBorderTension = 0;
        for (const FDemocracyDiplomacyRelationshipState& Relationship : State.DiplomacyMatrix.Relationships)
        {
            HighestBorderTension = FMath::Max(HighestBorderTension, Relationship.BorderTension);
            bHasWarTarget = bHasWarTarget
                || Relationship.RelationshipStatus.Equals(TEXT("Hostile"), ESearchCase::IgnoreCase)
                || Relationship.RelationshipStatus.Equals(TEXT("Rival"), ESearchCase::IgnoreCase)
                || Relationship.BorderTension >= 70;
            bHasAllianceSupport = bHasAllianceSupport
                || Relationship.RelationshipStatus.Equals(TEXT("Ally"), ESearchCase::IgnoreCase)
                || Relationship.TreatyStatus.Contains(TEXT("Defense"), ESearchCase::IgnoreCase)
                || Relationship.Trust >= 70;
        }
        const bool bHasActiveWar = State.WarSystem.ActiveConflicts.Num() > 0 || State.InvasionRisk.CurrentInvasionRisk >= 55 || HighestBorderTension >= 75;
        const bool bHighDefeatPressure = State.InvasionRisk.CurrentInvasionRisk >= 70 || State.RtsWorld.Ownership.ContestedProvinces > 0 || State.RtsWorld.Ownership.PlayerControlledProvinces < FMath::Max(1, State.RtsWorld.Ownership.TotalProvinces / 5);
        for (FDemocracyCommandAuthorityActionState& Action : State.CommandAuthority.Actions)
        {
            if (Action.bOfficeAllowed) ++OfficeCount;
            if (Action.bRtsViewAllowed) ++RtsCount;
            Action.bEnabled = true;
            Action.DisabledReason.Empty();
            if (State.Turn - Action.LastExecutedTurn < Action.CooldownTurns)
            {
                Action.bEnabled = false;
                Action.DisabledReason = FString::Printf(TEXT("Cooldown: available in %d turn(s)."), Action.CooldownTurns - (State.Turn - Action.LastExecutedTurn));
            }
            if (Action.TreasuryCost > State.PlayerCountry.Treasury)
            {
                Action.bEnabled = false;
                Action.DisabledReason = TEXT("Insufficient treasury for this order.");
            }
            if (Action.CommandId.Equals(TEXT("office_emergency"), ESearchCase::IgnoreCase) && State.PlayerCountry.Unrest < 45 && State.InvasionRisk.CurrentInvasionRisk < 35)
            {
                Action.bEnabled = false;
                Action.DisabledReason = TEXT("Emergency authority requires high unrest or takeover pressure.");
            }
            if (Action.CommandId.Equals(TEXT("office_declare_war"), ESearchCase::IgnoreCase) && (!bHasWarTarget || State.PlayerCountry.MilitaryReadiness < 45))
            {
                Action.bEnabled = false;
                Action.DisabledReason = TEXT("Declare war requires a hostile/rival target or severe border tension and at least 45 military readiness.");
            }
            if (Action.CommandId.Equals(TEXT("office_request_alliance_aid"), ESearchCase::IgnoreCase) && !bHasAllianceSupport)
            {
                Action.bEnabled = false;
                Action.DisabledReason = TEXT("Alliance aid requires an ally, defense treaty, or high-trust partner.");
            }
            if (Action.CommandId.Equals(TEXT("office_negotiate_ceasefire"), ESearchCase::IgnoreCase) && !bHasActiveWar)
            {
                Action.bEnabled = false;
                Action.DisabledReason = TEXT("Ceasefire talks require an active war, severe border conflict, or high invasion risk.");
            }
            if (Action.CommandId.Equals(TEXT("office_surrender_territory"), ESearchCase::IgnoreCase) && !bHighDefeatPressure)
            {
                Action.bEnabled = false;
                Action.DisabledReason = TEXT("Territory surrender is available only under high defeat or takeover pressure.");
            }
            if (Action.CommandId.Equals(TEXT("office_impose_sanctions"), ESearchCase::IgnoreCase) && !bHasWarTarget && !bHasActiveWar)
            {
                Action.bEnabled = false;
                Action.DisabledReason = TEXT("Sanctions require hostile/rival pressure, active war, or severe border tension.");
            }
            if (Action.AuthorityLayer.Equals(TEXT("RTS"), ESearchCase::IgnoreCase) && !Action.bOfficeAllowed)
            {
                Action.bEnabled = false;
                if (Action.DisabledReason.IsEmpty()) Action.DisabledReason = TEXT("Future RTS-view command only.");
            }
        }
        State.CommandAuthority.OfficeAuthoritySummary = FString::Printf(TEXT("Office authority: %d executable simulation-office command categories. Direct movement/battles/building remain blocked here."), OfficeCount);
        State.CommandAuthority.RtsAuthoritySummary = FString::Printf(TEXT("RTS authority: %d future RTS-view command categories; outcomes will feed back into simulation state."), RtsCount);
    }

    void InitializeDecisionHistoryIfMissing(FDemocracySimulationState& State)
    {
        if (State.DecisionHistory.MaxRecords <= 0)
        {
            State.DecisionHistory.MaxRecords = 80;
        }
        if (State.DecisionHistory.Records.Num() == 0)
        {
            LogDecision(State, TEXT("Loaded State"), TEXT("Decision History Started"), TEXT("Existing save loaded before decision history existed."), TEXT("Future policies, events, budget changes, department actions, and save actions will be recorded."), 5, { TEXT("compatibility"), TEXT("save") });
        }
    }

    FString BuildDecisionHistorySummaryText(const FDemocracyDecisionHistoryState& History)
    {
        TArray<FString> Lines;
        Lines.Add(FString::Printf(TEXT("Records %d/%d | updated turn %d"), History.Records.Num(), History.MaxRecords, History.LastUpdatedTurn));
        Lines.Add(History.Summary);
        const int32 StartIndex = FMath::Max(0, History.Records.Num() - 6);
        for (int32 Index = History.Records.Num() - 1; Index >= StartIndex; --Index)
        {
            const FDemocracyDecisionRecordState& Record = History.Records[Index];
            Lines.Add(FString::Printf(TEXT("Turn %d | %s | %s | severity %d | %s"),
                Record.Turn,
                *Record.Category,
                *Record.DecisionTitle,
                Record.Severity,
                *Record.ConsequenceSummary));
        }
        return FString::Join(Lines, TEXT("\n"));
    }
    FDemocracyApprovalCauseState MakeApprovalCause(const FString& CauseName, const FString& Category, int32 Severity, int32 ApprovalImpact, int32 UnrestImpact, int32 StabilityImpact, const FString& SourceMetric, const FString& CurrentStatus, const TArray<FString>& SuggestedResponses)
    {
        FDemocracyApprovalCauseState Cause;
        Cause.CauseName = CauseName;
        Cause.Category = Category;
        Cause.Severity = FMath::Clamp(Severity, 0, 100);
        Cause.ApprovalImpact = FMath::Clamp(ApprovalImpact, -100, 100);
        Cause.UnrestImpact = FMath::Clamp(UnrestImpact, -100, 100);
        Cause.StabilityImpact = FMath::Clamp(StabilityImpact, -100, 100);
        Cause.SourceMetric = SourceMetric;
        Cause.CurrentStatus = CurrentStatus;
        Cause.SuggestedResponses = SuggestedResponses;
        return Cause;
    }

    int32 GetActiveEventSeverityByType(const FDemocracyEventSystemState& EventSystem, const FString& EventType)
    {
        int32 Severity = 0;
        for (const FDemocracyActiveEventState& Event : EventSystem.ActiveEvents)
        {
            if (!Event.bResolved && Event.EventType.Equals(EventType, ESearchCase::IgnoreCase))
            {
                Severity = FMath::Max(Severity, Event.Severity);
            }
        }
        return Severity;
    }

    void RecalculateApprovalStability(FDemocracySimulationState& State)
    {
        FDemocracyApprovalStabilityModelState& Model = State.ApprovalStability;
        const FDemocracyCountryState& Country = State.PlayerCountry;
        const FDemocracyEconomyBudgetState& Budget = State.EconomyBudget;
        const FDemocracyDemographicsState& Demographics = State.Demographics;
        const FDemocracyPolicyState& Policies = Country.Policies;

        const int32 FoodSeverity = FMath::Clamp(GetResourceChainShortage(State.ResourceChains, TEXT("Food")) / 2 + FMath::Max(0, 120 - Country.Resources.Food) / 5, 0, 100);
        const int32 WaterSeverity = FMath::Clamp(GetResourceChainShortage(State.ResourceChains, TEXT("Water")) / 2 + FMath::Max(0, 105 - Country.Resources.Water) / 5, 0, 100);
        const int32 TaxSeverity = FMath::Clamp(FMath::Max(0, Budget.TaxRate - 22) * 4 + (Budget.TaxPolicy.Equals(TEXT("High Taxes"), ESearchCase::IgnoreCase) ? 12 : 0), 0, 100);
        const int32 InflationSeverity = FMath::Clamp(FMath::Max(0, Budget.Inflation - 4) * 7 + FMath::Max(0, State.ResourceChains.TotalShortagePressure - 20) / 2, 0, 100);
        const int32 UnemploymentSeverity = FMath::Clamp(FMath::Max(0, 62 - Country.EconomicHealth) + FMath::Max(0, Demographics.NationalNeedsPressure - 45) / 2, 0, 100);
        const int32 WarFatigueSeverity = FMath::Clamp(State.InvasionRisk.CurrentInvasionRisk / 2 + (Policies.MilitaryPolicy.Equals(TEXT("National Mobilization"), ESearchCase::IgnoreCase) ? 24 : 0) + FMath::Max(0, 45 - Country.MilitaryReadiness) / 2, 0, 100);
        const int32 ScandalSeverity = GetActiveEventSeverityByType(State.EventSystem, TEXT("Scandal"));
        const int32 CorruptionSeverity = FMath::Clamp(ScandalSeverity + (Policies.CivilPolicy.Equals(TEXT("Emergency Powers"), ESearchCase::IgnoreCase) ? 18 : 0) + FMath::Max(0, Policies.PolicyChangeCount - 5) * 3, 0, 100);
        const int32 ServicesSeverity = FMath::Clamp(FMath::Max(0, 62 - Budget.PublicServices) + FMath::Max(0, 55 - GetDepartmentEffectiveness(State.Departments, TEXT("Health"))) / 2, 0, 100);
        const int32 InfrastructureSeverity = FMath::Clamp(FMath::Max(0, 68 - Country.Infrastructure) + GetResourceChainShortage(State.ResourceChains, TEXT("Wood")) / 6 + GetResourceChainShortage(State.ResourceChains, TEXT("Metals")) / 6, 0, 100);
        const int32 CivilRightsRelief = Policies.CivilPolicy.Equals(TEXT("Civil Liberties"), ESearchCase::IgnoreCase) ? 12 : 0;

        Model.Causes.Reset();
        Model.Causes.Add(MakeApprovalCause(TEXT("Food Shortage"), TEXT("Resources"), FoodSeverity, -FoodSeverity / 3, FoodSeverity / 3, -FoodSeverity / 4, TEXT("Food reserves and production chain shortage"), FString::Printf(TEXT("Food shortage severity %d; reserve %d."), FoodSeverity, Country.Resources.Food), { TEXT("Boost Agriculture"), TEXT("Import Food"), TEXT("Use emergency food relief") }));
        Model.Causes.Add(MakeApprovalCause(TEXT("Water Access"), TEXT("Resources"), WaterSeverity, -WaterSeverity / 4, WaterSeverity / 3, -WaterSeverity / 3, TEXT("Water reserves and regional access"), FString::Printf(TEXT("Water access severity %d; reserve %d."), WaterSeverity, Country.Resources.Water), { TEXT("Improve public services"), TEXT("Repair utilities"), TEXT("Import water") }));
        Model.Causes.Add(MakeApprovalCause(TEXT("Tax Burden"), TEXT("Economy"), TaxSeverity, -TaxSeverity / 4, TaxSeverity / 5, -TaxSeverity / 6, TEXT("Tax rate and tax policy"), FString::Printf(TEXT("Tax burden severity %d; rate %d%%."), TaxSeverity, Budget.TaxRate), { TEXT("Lower taxes"), TEXT("Offset high taxes with services"), TEXT("Stabilize spending before raising rates") }));
        Model.Causes.Add(MakeApprovalCause(TEXT("Inflation"), TEXT("Economy"), InflationSeverity, -InflationSeverity / 4, InflationSeverity / 4, -InflationSeverity / 5, TEXT("Inflation and shortage pressure"), FString::Printf(TEXT("Cost-of-living severity %d; inflation %d."), InflationSeverity, Budget.Inflation), { TEXT("Reduce shortages"), TEXT("Improve production efficiency"), TEXT("Balance deficit pressure") }));
        Model.Causes.Add(MakeApprovalCause(TEXT("Unemployment"), TEXT("Economy"), UnemploymentSeverity, -UnemploymentSeverity / 5, UnemploymentSeverity / 4, -UnemploymentSeverity / 5, TEXT("Economic health and jobs needs"), FString::Printf(TEXT("Jobs severity %d; economic health %d."), UnemploymentSeverity, Country.EconomicHealth), { TEXT("Use workforce training"), TEXT("Support industry"), TEXT("Improve infrastructure") }));
        Model.Causes.Add(MakeApprovalCause(TEXT("War Fatigue"), TEXT("Security"), WarFatigueSeverity, -WarFatigueSeverity / 4, WarFatigueSeverity / 5, -WarFatigueSeverity / 6, TEXT("Mobilization, readiness, and invasion risk"), FString::Printf(TEXT("War fatigue severity %d; invasion risk %d."), WarFatigueSeverity, State.InvasionRisk.CurrentInvasionRisk), { TEXT("Improve diplomacy"), TEXT("Avoid sustained mobilization"), TEXT("Fund readiness before crisis") }));
        Model.Causes.Add(MakeApprovalCause(TEXT("Corruption"), TEXT("Legitimacy"), CorruptionSeverity, -CorruptionSeverity / 4, CorruptionSeverity / 4, -CorruptionSeverity / 3, TEXT("Scandals, emergency powers, and rapid policy churn"), FString::Printf(TEXT("Legitimacy severity %d; active scandal severity %d."), CorruptionSeverity, ScandalSeverity), { TEXT("Resolve scandals transparently"), TEXT("Avoid repeated emergency powers"), TEXT("Use administrative reform") }));
        Model.Causes.Add(MakeApprovalCause(TEXT("Public Services"), TEXT("Services"), ServicesSeverity, -ServicesSeverity / 5, ServicesSeverity / 5, -ServicesSeverity / 4, TEXT("Public services and Health ministry"), FString::Printf(TEXT("Services severity %d; public services %d."), ServicesSeverity, Budget.PublicServices), { TEXT("Increase public services spending"), TEXT("Strengthen Health ministry"), TEXT("Protect water access") }));
        Model.Causes.Add(MakeApprovalCause(TEXT("Infrastructure Reliability"), TEXT("Infrastructure"), InfrastructureSeverity, -InfrastructureSeverity / 6, InfrastructureSeverity / 5, -InfrastructureSeverity / 4, TEXT("Infrastructure, wood, and metals"), FString::Printf(TEXT("Infrastructure severity %d; infrastructure %d."), InfrastructureSeverity, Country.Infrastructure), { TEXT("Repair critical systems"), TEXT("Secure wood and metals"), TEXT("Fund infrastructure push") }));

        if (CivilRightsRelief > 0)
        {
            Model.Causes.Add(MakeApprovalCause(TEXT("Civil Rights Confidence"), TEXT("Legitimacy"), CivilRightsRelief, CivilRightsRelief, -CivilRightsRelief / 2, CivilRightsRelief / 2, TEXT("Civil policy"), TEXT("Civil liberties are lowering legitimacy pressure."), { TEXT("Keep civil protections stable"), TEXT("Avoid emergency powers unless necessary") }));
        }

        int32 ApprovalPressure = 0;
        int32 UnrestPressure = 0;
        int32 StabilityPressure = 0;
        int32 CriticalCauseCount = 0;
        FString TopCause = TEXT("No major pressure source");
        int32 TopSeverity = -1;
        for (const FDemocracyApprovalCauseState& Cause : Model.Causes)
        {
            ApprovalPressure += Cause.ApprovalImpact;
            UnrestPressure += Cause.UnrestImpact;
            StabilityPressure += Cause.StabilityImpact;
            if (Cause.Severity >= 45)
            {
                ++CriticalCauseCount;
            }
            if (Cause.Severity > TopSeverity)
            {
                TopSeverity = Cause.Severity;
                TopCause = Cause.CauseName;
            }
        }

        Model.NetApprovalPressure = FMath::Clamp(ApprovalPressure, -100, 100);
        Model.NetUnrestPressure = FMath::Clamp(UnrestPressure, -100, 100);
        Model.NetStabilityPressure = FMath::Clamp(StabilityPressure, -100, 100);
        Model.LastUpdatedTurn = State.Turn;
        Model.Summary = FString::Printf(TEXT("Top cause: %s severity %d | critical causes %d | approval pressure %+d | unrest pressure %+d | stability pressure %+d."), *TopCause, FMath::Max(0, TopSeverity), CriticalCauseCount, Model.NetApprovalPressure, Model.NetUnrestPressure, Model.NetStabilityPressure);
    }

    FString BuildApprovalStabilitySummaryText(const FDemocracyApprovalStabilityModelState& Model)
    {
        TArray<FString> Lines;
        Lines.Add(FString::Printf(TEXT("Approval pressure %+d | unrest pressure %+d | stability pressure %+d | updated turn %d"), Model.NetApprovalPressure, Model.NetUnrestPressure, Model.NetStabilityPressure, Model.LastUpdatedTurn));
        Lines.Add(Model.Summary);
        for (const FDemocracyApprovalCauseState& Cause : Model.Causes)
        {
            Lines.Add(FString::Printf(TEXT("%s (%s): severity %d | approval %+d | unrest %+d | stability %+d | %s"), *Cause.CauseName, *Cause.Category, Cause.Severity, Cause.ApprovalImpact, Cause.UnrestImpact, Cause.StabilityImpact, *Cause.CurrentStatus));
        }
        return FString::Join(Lines, TEXT("\n"));
    }
    struct FSimulationTickSnapshot
    {
        int32 Turn = 1;
        int32 SimulationSecond = 0;
        int32 Food = 0;
        int32 Water = 0;
        int32 GasOil = 0;
        int32 Wood = 0;
        int32 Metals = 0;
        int32 Approval = 0;
        int32 Stability = 0;
        int32 Unrest = 0;
        int32 Treasury = 0;
        int32 Economy = 0;
        int32 Diplomacy = 0;
        int32 Military = 0;
        int32 AssassinationRisk = 0;
        int32 InvasionRisk = 0;
        int32 ShortagePressure = 0;
        int32 ActiveEvents = 0;
        int32 EventHistory = 0;
        int32 NetApprovalPressure = 0;
        int32 NetUnrestPressure = 0;
        int32 NetStabilityPressure = 0;
        FString Phase;
        FString TopCause;
        int32 TopCauseSeverity = 0;
    };

    FString FormatTickDelta(int32 Delta)
    {
        if (Delta > 0)
        {
            return FString::Printf(TEXT("+%d"), Delta);
        }
        return FString::FromInt(Delta);
    }

    FString GetTopApprovalCauseName(const FDemocracyApprovalStabilityModelState& Model, int32& OutSeverity)
    {
        OutSeverity = 0;
        FString TopCause = TEXT("No major pressure source");
        for (const FDemocracyApprovalCauseState& Cause : Model.Causes)
        {
            if (Cause.Severity > OutSeverity)
            {
                OutSeverity = Cause.Severity;
                TopCause = Cause.CauseName;
            }
        }
        return TopCause;
    }

    FSimulationTickSnapshot MakeSimulationTickSnapshot(const FDemocracySimulationState& State)
    {
        FSimulationTickSnapshot Snapshot;
        const FDemocracyCountryState& Country = State.PlayerCountry;
        Snapshot.Turn = State.Turn;
        Snapshot.SimulationSecond = State.RtsWorld.SimulationSecond;
        Snapshot.Food = Country.Resources.Food;
        Snapshot.Water = Country.Resources.Water;
        Snapshot.GasOil = Country.Resources.GasOil;
        Snapshot.Wood = Country.Resources.Wood;
        Snapshot.Metals = Country.Resources.Metals;
        Snapshot.Approval = Country.PublicApproval;
        Snapshot.Stability = Country.Stability;
        Snapshot.Unrest = Country.Unrest;
        Snapshot.Treasury = Country.Treasury;
        Snapshot.Economy = Country.EconomicHealth;
        Snapshot.Diplomacy = Country.DiplomaticStanding;
        Snapshot.Military = Country.MilitaryReadiness;
        Snapshot.AssassinationRisk = State.FailureRisk.CurrentAssassinationRisk;
        Snapshot.InvasionRisk = State.InvasionRisk.CurrentInvasionRisk;
        Snapshot.ShortagePressure = State.ResourceChains.TotalShortagePressure;
        Snapshot.ActiveEvents = State.EventSystem.ActiveEvents.Num();
        Snapshot.EventHistory = State.EventSystem.EventHistory.Num();
        Snapshot.NetApprovalPressure = State.ApprovalStability.NetApprovalPressure;
        Snapshot.NetUnrestPressure = State.ApprovalStability.NetUnrestPressure;
        Snapshot.NetStabilityPressure = State.ApprovalStability.NetStabilityPressure;
        Snapshot.Phase = State.Phase;
        Snapshot.TopCause = GetTopApprovalCauseName(State.ApprovalStability, Snapshot.TopCauseSeverity);
        return Snapshot;
    }

    void AddChangedValueLine(TArray<FString>& Lines, const FString& Label, int32 Before, int32 After)
    {
        const int32 Delta = After - Before;
        if (Delta != 0)
        {
            Lines.Add(FString::Printf(TEXT("%s %d (%s)"), *Label, After, *FormatTickDelta(Delta)));
        }
    }

    FString BuildTickResultFeedback(const FSimulationTickSnapshot& Before, const FDemocracySimulationState& State, int32 TickCount, bool bAdvancedTurn, bool bEventDeadlineApplied)
    {
        const FSimulationTickSnapshot After = MakeSimulationTickSnapshot(State);
        TArray<FString> Lines;
        TArray<FString> ResourceLines;
        TArray<FString> CoreLines;
        TArray<FString> RiskLines;
        TArray<FString> CauseLines;
        TArray<FString> EventLines;

        AddChangedValueLine(ResourceLines, TEXT("Food"), Before.Food, After.Food);
        AddChangedValueLine(ResourceLines, TEXT("Water"), Before.Water, After.Water);
        AddChangedValueLine(ResourceLines, TEXT("Fuel"), Before.GasOil, After.GasOil);
        AddChangedValueLine(ResourceLines, TEXT("Wood"), Before.Wood, After.Wood);
        AddChangedValueLine(ResourceLines, TEXT("Metals"), Before.Metals, After.Metals);
        if (ResourceLines.Num() == 0)
        {
            ResourceLines.Add(TEXT("No reserve changes this tick."));
        }

        AddChangedValueLine(CoreLines, TEXT("Approval"), Before.Approval, After.Approval);
        AddChangedValueLine(CoreLines, TEXT("Stability"), Before.Stability, After.Stability);
        AddChangedValueLine(CoreLines, TEXT("Unrest"), Before.Unrest, After.Unrest);
        AddChangedValueLine(CoreLines, TEXT("Treasury"), Before.Treasury, After.Treasury);
        AddChangedValueLine(CoreLines, TEXT("Economy"), Before.Economy, After.Economy);
        AddChangedValueLine(CoreLines, TEXT("Diplomacy"), Before.Diplomacy, After.Diplomacy);
        AddChangedValueLine(CoreLines, TEXT("Military"), Before.Military, After.Military);
        if (CoreLines.Num() == 0)
        {
            CoreLines.Add(TEXT("Core national metrics held steady."));
        }

        AddChangedValueLine(RiskLines, TEXT("Internal failure risk"), Before.AssassinationRisk, After.AssassinationRisk);
        AddChangedValueLine(RiskLines, TEXT("Takeover risk"), Before.InvasionRisk, After.InvasionRisk);
        if (RiskLines.Num() == 0)
        {
            RiskLines.Add(TEXT("No new failure risk movement."));
        }

        AddChangedValueLine(CauseLines, TEXT("Shortage pressure"), Before.ShortagePressure, After.ShortagePressure);
        AddChangedValueLine(CauseLines, TEXT("Approval pressure"), Before.NetApprovalPressure, After.NetApprovalPressure);
        AddChangedValueLine(CauseLines, TEXT("Unrest pressure"), Before.NetUnrestPressure, After.NetUnrestPressure);
        AddChangedValueLine(CauseLines, TEXT("Stability pressure"), Before.NetStabilityPressure, After.NetStabilityPressure);
        if (!Before.TopCause.Equals(After.TopCause, ESearchCase::IgnoreCase) || Before.TopCauseSeverity != After.TopCauseSeverity)
        {
            CauseLines.Add(FString::Printf(TEXT("Top unrest/approval cause: %s severity %d"), *After.TopCause, After.TopCauseSeverity));
        }
        if (CauseLines.Num() == 0)
        {
            CauseLines.Add(FString::Printf(TEXT("Primary cause unchanged: %s severity %d."), *After.TopCause, After.TopCauseSeverity));
        }

        const int32 ActiveEventDelta = After.ActiveEvents - Before.ActiveEvents;
        const int32 EventHistoryDelta = After.EventHistory - Before.EventHistory;
        if (ActiveEventDelta > 0)
        {
            EventLines.Add(FString::Printf(TEXT("%d new active event(s) appeared."), ActiveEventDelta));
        }
        else if (ActiveEventDelta < 0)
        {
            EventLines.Add(FString::Printf(TEXT("%d active event(s) resolved or cleared."), -ActiveEventDelta));
        }
        if (EventHistoryDelta > 0)
        {
            EventLines.Add(FString::Printf(TEXT("%d event history update(s) recorded."), EventHistoryDelta));
        }
        if (bEventDeadlineApplied)
        {
            EventLines.Add(TEXT("At least one event deadline follow-up was applied."));
        }
        if (!Before.Phase.Equals(After.Phase, ESearchCase::IgnoreCase))
        {
            EventLines.Add(FString::Printf(TEXT("Phase changed: %s -> %s."), *Before.Phase, *After.Phase));
        }
        if (EventLines.Num() == 0)
        {
            EventLines.Add(FString::Printf(TEXT("No event change. Active events: %d."), After.ActiveEvents));
        }

        Lines.Add(FString::Printf(TEXT("Tick %d result | Turn %d%s | Sim second %d"), TickCount, After.Turn, bAdvancedTurn ? TEXT(" advanced") : TEXT(""), After.SimulationSecond));
        Lines.Add(FString::Printf(TEXT("Resources: %s"), *FString::Join(ResourceLines, TEXT(", "))));
        Lines.Add(FString::Printf(TEXT("National effects: %s"), *FString::Join(CoreLines, TEXT(", "))));
        Lines.Add(FString::Printf(TEXT("Unrest/cause model: %s"), *FString::Join(CauseLines, TEXT(", "))));
        Lines.Add(FString::Printf(TEXT("Risks: %s"), *FString::Join(RiskLines, TEXT(", "))));
        Lines.Add(FString::Printf(TEXT("Events: %s"), *FString::Join(EventLines, TEXT(", "))));
        return FString::Join(Lines, TEXT("\n"));
    }
    FString AdvisorGuidanceForDifficultyScore(int32 DifficultyScore)
    {
        if (DifficultyScore <= 1)
        {
            return TEXT("Early Detailed");
        }
        if (DifficultyScore == 2)
        {
            return TEXT("Standard");
        }
        if (DifficultyScore == 3)
        {
            return TEXT("Late Limited");
        }
        return TEXT("Minimal");
    }

    FString GuidanceText(const FString& GuidanceLevel, const FString& Detailed, const FString& Standard, const FString& Limited, const FString& Minimal)
    {
        if (GuidanceLevel.Contains(TEXT("Detailed")))
        {
            return Detailed;
        }
        if (GuidanceLevel.Contains(TEXT("Limited")))
        {
            return Limited;
        }
        if (GuidanceLevel.Contains(TEXT("Minimal")))
        {
            return Minimal;
        }
        return Standard;
    }

    bool IsDetailedGuidance(const FString& GuidanceLevel)
    {
        return GuidanceLevel.Contains(TEXT("Detailed"));
    }

    bool IsLimitedGuidance(const FString& GuidanceLevel)
    {
        return GuidanceLevel.Contains(TEXT("Limited"));
    }

    bool IsMinimalGuidance(const FString& GuidanceLevel)
    {
        return GuidanceLevel.Contains(TEXT("Minimal"));
    }

    FString DifficultyGuidancePreview(const FDifficultyProfile& Profile)
    {
        if (Profile.PlayerHelpLevel.Equals(TEXT("High"), ESearchCase::IgnoreCase))
        {
            return TEXT("Tutorial guidance: detailed advisor explanations, direct action steps, extra recovery tips, and full UI cause/effect text.");
        }
        if (Profile.PlayerHelpLevel.Equals(TEXT("Standard"), ESearchCase::IgnoreCase))
        {
            return TEXT("Tutorial guidance: standard advisor explanations, visible warnings, and concise UI cause/effect summaries.");
        }
        if (Profile.PlayerHelpLevel.Equals(TEXT("Low"), ESearchCase::IgnoreCase))
        {
            return TEXT("Tutorial guidance: limited explanations. Advisors identify problems but provide fewer direct answers.");
        }
        return TEXT("Tutorial guidance: minimal. UI exposes signals and consequences, but advisors give almost no step-by-step direction.");
    }

    FString GetRuntimeGuidanceLevel(const FDemocracySimulationState& State)
    {
        return State.AdvisorSystem.GuidanceLevel.IsEmpty()
            ? AdvisorGuidanceForDifficultyScore(State.PlayerCountry.CountrySizeScore)
            : State.AdvisorSystem.GuidanceLevel;
    }

    FString RuntimeGuidanceSummary(const FDemocracySimulationState& State)
    {
        const FString GuidanceLevel = GetRuntimeGuidanceLevel(State);
        return GuidanceText(GuidanceLevel,
            TEXT("Detailed guidance active: advisor reports, dashboard entries, events, meetings, phone warnings, and failure warnings explain what changed, why it matters, what to do first, and the tradeoff."),
            TEXT("Standard guidance active: reports and choices show the issue, recommended response, main warning, and concise consequences."),
            TEXT("Limited guidance active: reports and choices give short warnings, broad direction, and fewer exact numbers."),
            TEXT("Minimal guidance active: UI exposes only high-level signals, deadlines, and final choice labels."));
    }

    FString BuildDashboardGuidanceText(const FDemocracySimulationState& State)
    {
        const FString GuidanceLevel = GetRuntimeGuidanceLevel(State);
        return GuidanceText(GuidanceLevel,
            TEXT("Dashboard tutorial: start with Risk / Causes, then check Events, Resources, Budget, and Advisors. Fix active fail-state warnings before long-term development."),
            TEXT("Dashboard guidance: review active warnings first, then use policy, budget, resource, and department screens to correct them."),
            TEXT("Dashboard guidance: core pressure is visible. Prioritize warnings and pending events."),
            TEXT("Dashboard guidance: minimal operational signals only."));
    }

    FString BuildGuidedFailureWarningText(const FDemocracySimulationState& State)
    {
        const FString GuidanceLevel = GetRuntimeGuidanceLevel(State);
        const int32 InternalPct = State.FailureRisk.AssassinationRiskTrigger > 0 ? (State.FailureRisk.CurrentAssassinationRisk * 100) / State.FailureRisk.AssassinationRiskTrigger : 0;
        const int32 TakeoverPct = State.InvasionRisk.InvasionRiskTrigger > 0 ? (State.InvasionRisk.CurrentInvasionRisk * 100) / State.InvasionRisk.InvasionRiskTrigger : 0;
        const FString InternalWarnings = State.FailureRisk.AdvisorWarnings.Num() > 0 ? FString::Join(State.FailureRisk.AdvisorWarnings, TEXT("\n")) : TEXT("No internal failure warning text is active.");
        const FString InternalTips = State.FailureRisk.RecoveryTips.Num() > 0 ? FString::Join(State.FailureRisk.RecoveryTips, TEXT("\n")) : TEXT("No internal recovery tips are active.");
        const FString TakeoverWarnings = State.InvasionRisk.AdvisorWarnings.Num() > 0 ? FString::Join(State.InvasionRisk.AdvisorWarnings, TEXT("\n")) : TEXT("No foreign takeover warning text is active.");
        const FString TakeoverTips = State.InvasionRisk.RecoveryTips.Num() > 0 ? FString::Join(State.InvasionRisk.RecoveryTips, TEXT("\n")) : TEXT("No takeover recovery tips are active.");

        return GuidanceText(GuidanceLevel,
            FString::Printf(TEXT("Internal risk %d/%d (%d%%) | warning thresholds: unrest %d, stability %d.\n%s\nRecovery: %s\n\nForeign takeover risk %d/%d (%d%%) | warning thresholds: readiness %d, diplomacy %d.\n%s\nRecovery: %s"),
                State.FailureRisk.CurrentAssassinationRisk,
                State.FailureRisk.AssassinationRiskTrigger,
                InternalPct,
                State.FailureRisk.UnrestWarningThreshold,
                State.FailureRisk.StabilityWarningThreshold,
                *InternalWarnings,
                *InternalTips,
                State.InvasionRisk.CurrentInvasionRisk,
                State.InvasionRisk.InvasionRiskTrigger,
                TakeoverPct,
                State.InvasionRisk.MilitaryReadinessWarningThreshold,
                45,
                *TakeoverWarnings,
                *TakeoverTips),
            FString::Printf(TEXT("Internal risk %d/%d (%d%%): %s\nForeign takeover risk %d/%d (%d%%): %s"),
                State.FailureRisk.CurrentAssassinationRisk,
                State.FailureRisk.AssassinationRiskTrigger,
                InternalPct,
                *InternalWarnings,
                State.InvasionRisk.CurrentInvasionRisk,
                State.InvasionRisk.InvasionRiskTrigger,
                TakeoverPct,
                *TakeoverWarnings),
            FString::Printf(TEXT("Internal risk %d%%. Foreign takeover risk %d%%. Review warnings before stepping the simulation."), InternalPct, TakeoverPct),
            FString::Printf(TEXT("Internal %d%% | Takeover %d%%"), InternalPct, TakeoverPct));
    }


    FString BuildGuidedMeetingIssueText(const FString& GuidanceLevel, const FString& Issue, const FString& Recommendation, const FString& Warning, const FString& Tradeoff)
    {
        return GuidanceText(GuidanceLevel,
            FString::Printf(TEXT("Issue: %s\nRecommendation: %s\nWarning: %s\nTradeoff: %s"), *Issue, *Recommendation, *Warning, *Tradeoff),
            FString::Printf(TEXT("Issue: %s\nRecommendation: %s\nWarning: %s"), *Issue, *Recommendation, *Warning),
            FString::Printf(TEXT("Warning: %s\nDirection: %s"), *Warning, *Recommendation),
            FString::Printf(TEXT("Signal: %s"), *Warning));
    }

    FString BuildGuidedGameOverDetails(const FDemocracySimulationState& State, const FString& Reason, const FString& Details)
    {
        const FString GuidanceLevel = GetRuntimeGuidanceLevel(State);
        return GuidanceText(GuidanceLevel,
            FString::Printf(TEXT("%s\n\n%s\n\nReload guidance: use the protected previous save, inspect the listed pressure source, then address the matching dashboard screen before stepping time again."), *Reason, *Details),
            FString::Printf(TEXT("%s\n\n%s\n\nReload guidance: use the protected previous save and correct the related warning before stepping time again."), *Reason, *Details),
            FString::Printf(TEXT("%s\nReload a protected save and correct the active warning category."), *Reason),
            FString::Printf(TEXT("%s\nProtected reload recommended."), *Reason));
    }

    int32 RiskPercent(int32 Current, int32 Trigger)
    {
        return Trigger > 0 ? FMath::Clamp((Current * 100) / Trigger, 0, 200) : 0;
    }

    FString FailureStageFromSignals(int32 RiskPct, bool bWarningSignal, bool bCriticalSignal)
    {
        if (bCriticalSignal || RiskPct >= 85)
        {
            return TEXT("Critical");
        }
        if (bWarningSignal || RiskPct >= 60)
        {
            return TEXT("Warning");
        }
        if (RiskPct >= 35)
        {
            return TEXT("Watch");
        }
        return TEXT("Stable");
    }

    void NormalizeFailureThresholds(FDemocracySimulationState& State)
    {
        FDemocracyFailureRiskState& Failure = State.FailureRisk;
        FDemocracyInvasionRiskState& Invasion = State.InvasionRisk;
        Failure.AssassinationRiskTrigger = FMath::Clamp(Failure.AssassinationRiskTrigger, 40, 200);
        Failure.StabilityWarningThreshold = FMath::Clamp(Failure.StabilityWarningThreshold, 10, 65);
        Failure.StabilityCriticalThreshold = FMath::Clamp(Failure.StabilityCriticalThreshold, 1, FMath::Max(1, Failure.StabilityWarningThreshold - 8));
        Failure.UnrestWarningThreshold = FMath::Clamp(Failure.UnrestWarningThreshold, 30, 90);
        Failure.UnrestCriticalThreshold = FMath::Clamp(Failure.UnrestCriticalThreshold, FMath::Min(100, Failure.UnrestWarningThreshold + 8), 100);
        Failure.CurrentAssassinationRisk = FMath::Clamp(Failure.CurrentAssassinationRisk, 0, Failure.AssassinationRiskTrigger);

        Invasion.InvasionRiskTrigger = FMath::Clamp(Invasion.InvasionRiskTrigger, 40, 200);
        Invasion.MilitaryReadinessWarningThreshold = FMath::Clamp(Invasion.MilitaryReadinessWarningThreshold, 10, 75);
        Invasion.MilitaryReadinessCriticalThreshold = FMath::Clamp(Invasion.MilitaryReadinessCriticalThreshold, 1, FMath::Max(1, Invasion.MilitaryReadinessWarningThreshold - 8));
        Invasion.BorderPressureWarningThreshold = FMath::Clamp(Invasion.BorderPressureWarningThreshold, 10, 90);
        Invasion.BorderPressureCriticalThreshold = FMath::Clamp(Invasion.BorderPressureCriticalThreshold, FMath::Min(100, Invasion.BorderPressureWarningThreshold + 8), 100);
        Invasion.TerritorialLossWarningThreshold = FMath::Clamp(Invasion.TerritorialLossWarningThreshold, 1, 10);
        Invasion.TerritorialLossCriticalThreshold = FMath::Clamp(Invasion.TerritorialLossCriticalThreshold, Invasion.TerritorialLossWarningThreshold, 15);
        Invasion.CurrentInvasionRisk = FMath::Clamp(Invasion.CurrentInvasionRisk, 0, Invasion.InvasionRiskTrigger);
    }

    int32 EstimateBorderPressure(const FDemocracySimulationState& State)
    {
        int32 Pressure = State.InvasionRisk.CurrentInvasionRisk;
        Pressure += FMath::Max(0, 50 - State.PlayerCountry.DiplomaticStanding) / 2;
        Pressure += FMath::Max(0, 45 - State.PlayerCountry.MilitaryReadiness) / 2;
        Pressure += FMath::Max(0, State.RtsWorld.BorderTerritories - FMath::Max(1, State.RtsWorld.ControlledTerritories / 5)) * 3;
        for (const FDemocracyActiveEventState& Event : State.EventSystem.ActiveEvents)
        {
            if (!Event.bResolved && (Event.EventType.Contains(TEXT("Border"), ESearchCase::IgnoreCase) || Event.EventType.Contains(TEXT("Tension"), ESearchCase::IgnoreCase)))
            {
                Pressure += FMath::Clamp(Event.Severity / 3, 3, 25);
            }
        }
        return FMath::Clamp(Pressure, 0, 100);
    }

    void RefreshFailureValidationState(FDemocracySimulationState& State)
    {
        NormalizeFailureThresholds(State);
        const FDemocracyCountryState& Country = State.PlayerCountry;
        FDemocracyFailureRiskState& Failure = State.FailureRisk;
        FDemocracyInvasionRiskState& Invasion = State.InvasionRisk;
        const int32 InternalPct = RiskPercent(Failure.CurrentAssassinationRisk, Failure.AssassinationRiskTrigger);
        const int32 TakeoverPct = RiskPercent(Invasion.CurrentInvasionRisk, Invasion.InvasionRiskTrigger);
        const int32 BorderPressure = EstimateBorderPressure(State);
        const bool bInternalWarning = Country.Unrest >= Failure.UnrestWarningThreshold || Country.Stability <= Failure.StabilityWarningThreshold || State.Demographics.NationalNeedsPressure >= 70;
        const bool bInternalCritical = Country.Unrest >= Failure.UnrestCriticalThreshold || Country.Stability <= Failure.StabilityCriticalThreshold || State.Demographics.NationalNeedsPressure >= 92;
        const bool bTakeoverWarning = Country.MilitaryReadiness <= Invasion.MilitaryReadinessWarningThreshold || BorderPressure >= Invasion.BorderPressureWarningThreshold || TakeoverPct >= 60 || Country.DiplomaticStanding <= 35;
        const bool bTakeoverCritical = Country.MilitaryReadiness <= Invasion.MilitaryReadinessCriticalThreshold || BorderPressure >= Invasion.BorderPressureCriticalThreshold || TakeoverPct >= 85 || Country.DiplomaticStanding <= 12;

        Failure.WarningLevel = FailureStageFromSignals(InternalPct, bInternalWarning, bInternalCritical);
        Invasion.WarningLevel = FailureStageFromSignals(TakeoverPct, bTakeoverWarning, bTakeoverCritical);
        Failure.ActiveUnrestCauses.Reset();
        Failure.AdvisorWarnings.Reset();
        Failure.RecoveryTips.Reset();
        Invasion.ActiveInvasionCauses.Reset();
        Invasion.AdvisorWarnings.Reset();
        Invasion.RecoveryTips.Reset();

        if (Country.Unrest >= Failure.UnrestWarningThreshold) { Failure.ActiveUnrestCauses.Add(TEXT("Unrest threshold exceeded")); }
        if (Country.Stability <= Failure.StabilityWarningThreshold) { Failure.ActiveUnrestCauses.Add(TEXT("Stability below warning threshold")); }
        if (State.Demographics.NationalNeedsPressure >= 70) { Failure.ActiveUnrestCauses.Add(TEXT("Population needs pressure")); }
        if (Country.PublicApproval <= 20) { Failure.ActiveUnrestCauses.Add(TEXT("Legitimacy collapse risk")); }
        if (State.ResourceChains.TotalShortagePressure >= 45) { Failure.ActiveUnrestCauses.Add(TEXT("Resource shortage pressure")); }
        if (Failure.ActiveUnrestCauses.Num() == 0) { Failure.ActiveUnrestCauses.Add(TEXT("No active internal failure cause")); }

        if (!Failure.WarningLevel.Equals(TEXT("Stable"), ESearchCase::IgnoreCase))
        {
            Failure.AdvisorWarnings.Add(FString::Printf(TEXT("%s internal warning: stability %d/%d critical, unrest %d/%d critical, assassination risk %d%%."), *Failure.WarningLevel, Country.Stability, Failure.StabilityCriticalThreshold, Country.Unrest, Failure.UnrestCriticalThreshold, InternalPct));
            Failure.RecoveryTips.Add(TEXT("Reduce unrest drivers through resources, public services, targeted event choices, and credible press reassurance."));
            Failure.RecoveryTips.Add(TEXT("Raise stability before stepping time: avoid empty announcements and address active protest, scandal, or shortage events."));
        }

        if (Country.MilitaryReadiness <= Invasion.MilitaryReadinessWarningThreshold) { Invasion.ActiveInvasionCauses.Add(TEXT("Military readiness below warning threshold")); }
        if (BorderPressure >= Invasion.BorderPressureWarningThreshold) { Invasion.ActiveInvasionCauses.Add(TEXT("Border pressure above warning threshold")); }
        if (Country.DiplomaticStanding <= 35) { Invasion.ActiveInvasionCauses.Add(TEXT("Diplomacy too weak to deter rivals")); }
        if (Invasion.CurrentInvasionRisk >= Invasion.InvasionRiskTrigger * 60 / 100) { Invasion.ActiveInvasionCauses.Add(TEXT("Takeover risk approaching trigger")); }
        if (Invasion.ActiveInvasionCauses.Num() == 0) { Invasion.ActiveInvasionCauses.Add(TEXT("No active takeover cause")); }

        if (!Invasion.WarningLevel.Equals(TEXT("Stable"), ESearchCase::IgnoreCase))
        {
            Invasion.AdvisorWarnings.Add(FString::Printf(TEXT("%s takeover warning: readiness %d/%d critical, diplomacy %d, border pressure %d/%d critical, takeover risk %d%%."), *Invasion.WarningLevel, Country.MilitaryReadiness, Invasion.MilitaryReadinessCriticalThreshold, Country.DiplomaticStanding, BorderPressure, Invasion.BorderPressureCriticalThreshold, TakeoverPct));
            Invasion.RecoveryTips.Add(TEXT("Improve military readiness, diplomacy, and border response before advancing time."));
            Invasion.RecoveryTips.Add(TEXT("Use foreign meetings, alliance outreach, defense funding, and border-event choices to reduce takeover risk."));
        }
    }

    FString BuildFailureCauseText(const TArray<FString>& Causes)
    {
        return Causes.Num() > 0 ? FString::Join(Causes, TEXT(", ")) : TEXT("No cause list recorded");
    }
    int32 VisibleAdvisorReportCount(const FDemocracyAdvisorSystemState& AdvisorSystem, const FDemocracyCountryState& Country)
    {
        const int32 DifficultyScore = FMath::Clamp(Country.CountrySizeScore, 1, 4);
        const int32 AdvisorLimit = FMath::Max(1, AdvisorSystem.AdvisorCount);
        if (DifficultyScore <= 1)
        {
            return FMath::Max(AdvisorLimit, 5);
        }
        if (DifficultyScore == 2)
        {
            return FMath::Max(AdvisorLimit, 4);
        }
        if (DifficultyScore == 3)
        {
            return FMath::Min(AdvisorLimit, 2);
        }
        return 1;
    }

    void AppendGuidedAdvisorReport(TArray<FString>& Lines, const FDemocracyAdvisorReport& Report, const FString& GuidanceLevel)
    {
        Lines.Add(FString::Printf(TEXT("\n%s (%s) | Severity %d | Guidance %s"), *Report.AdvisorName, *Report.Category, Report.Severity, *GuidanceLevel));
        if (IsMinimalGuidance(GuidanceLevel))
        {
            Lines.Add(FString::Printf(TEXT("Signal: %s"), *Report.Warning));
            return;
        }
        if (IsLimitedGuidance(GuidanceLevel))
        {
            Lines.Add(FString::Printf(TEXT("Warning: %s"), *Report.Warning));
            Lines.Add(FString::Printf(TEXT("Direction: %s"), *Report.Recommendation));
            return;
        }

        Lines.Add(FString::Printf(TEXT("Issue: %s"), *Report.IssueReport));
        Lines.Add(FString::Printf(TEXT("Recommendation: %s"), *Report.Recommendation));
        Lines.Add(FString::Printf(TEXT("Warning: %s"), *Report.Warning));
        if (IsDetailedGuidance(GuidanceLevel))
        {
            Lines.Add(FString::Printf(TEXT("Tradeoff: %s"), *Report.TradeoffExplanation));
            Lines.Add(TEXT("Tutorial cue: compare this report against the matching desk screen before committing a policy, budget, meeting, or press decision."));
        }
    }

    FDemocracyAdvisorReport MakeAdvisorReport(const FString& AdvisorName, const FString& Category, int32 Severity, const FString& GuidanceLevel, const FString& Issue, const FString& Recommendation, const FString& Warning, const FString& Tradeoff)
    {
        FDemocracyAdvisorReport Report;
        Report.AdvisorName = AdvisorName;
        Report.Category = Category;
        Report.Severity = FMath::Clamp(Severity, 0, 100);
        Report.GuidanceLevel = GuidanceLevel;
        Report.IssueReport = Issue;
        Report.Recommendation = Recommendation;
        Report.Warning = Warning;
        Report.TradeoffExplanation = Tradeoff;
        return Report;
    }

    TArray<FDemocracyAdvisorReport> GenerateAdvisorReports(const FDemocracySimulationState& State)
    {
        const FDemocracyCountryState& Country = State.PlayerCountry;
        const FDemocracyResourceInventory& Resources = Country.Resources;
        const int32 DifficultyScore = FMath::Clamp(Country.CountrySizeScore, 1, 4);
        const FString GuidanceLevel = AdvisorGuidanceForDifficultyScore(DifficultyScore);
        const int32 ShortageCount = State.ResourceChains.Chains.Num() > 0 ? State.ResourceChains.TotalShortagePressure / 18 : (Resources.Food < 120 ? 1 : 0) + (Resources.Water < 100 ? 1 : 0) + (Resources.GasOil < 60 ? 1 : 0) + (Resources.Wood < 55 ? 1 : 0) + (Resources.Metals < 55 ? 1 : 0);
        const int32 InternalRiskPct = State.FailureRisk.AssassinationRiskTrigger > 0 ? (State.FailureRisk.CurrentAssassinationRisk * 100) / State.FailureRisk.AssassinationRiskTrigger : 0;
        const int32 InvasionRiskPct = State.InvasionRisk.InvasionRiskTrigger > 0 ? (State.InvasionRisk.CurrentInvasionRisk * 100) / State.InvasionRisk.InvasionRiskTrigger : 0;

        TArray<FDemocracyAdvisorReport> Reports;
        Reports.Add(MakeAdvisorReport(
            TEXT("Resource Manager"),
            TEXT("Resources"),
            ShortageCount * 20,
            GuidanceLevel,
            State.ResourceChains.Chains.Num() > 0 ? State.ResourceChains.Summary : FString::Printf(TEXT("Food %d, water %d, gas/oil %d, wood %d, metals %d."), Resources.Food, Resources.Water, Resources.GasOil, Resources.Wood, Resources.Metals),
            GuidanceText(GuidanceLevel,
                TEXT("Prioritize food above 120 and water above 100 first; those two shortages directly increase unrest during each tick."),
                TEXT("Protect food and water reserves before expanding production."),
                TEXT("Resource pressure is visible. Stabilize core stockpiles."),
                TEXT("Watch shortages.")),
            ShortageCount > 0 ? TEXT("Shortages are already adding unrest and stability pressure.") : TEXT("No immediate shortage warning."),
            TEXT("Stockpiling improves stability but can delay industrial or military spending.")));

        Reports.Add(MakeAdvisorReport(
            TEXT("Economic Advisor"),
            TEXT("Economy"),
            FMath::Max(0, 70 - Country.EconomicHealth) + (Country.Treasury < 250 ? 20 : 0),
            GuidanceLevel,
            FString::Printf(TEXT("Treasury %d, economic health %d, active economic policy: %s."), Country.Treasury, Country.EconomicHealth, *Country.Policies.EconomicPolicy),
            GuidanceText(GuidanceLevel,
                TEXT("If approval is stable, Balanced Budget or Industrial Subsidies can rebuild treasury and production. If approval is weak, avoid austerity until unrest is controlled."),
                TEXT("Balance treasury recovery against approval loss."),
                TEXT("Budget pressure is rising. Review economic policy."),
                TEXT("Watch treasury.")),
            Country.Treasury < 200 ? TEXT("Treasury reserve is low for crisis response.") : TEXT("Treasury reserve is currently usable."),
            TEXT("Spending can raise approval and stability now, but it reduces emergency reserves.")));

        Reports.Add(MakeAdvisorReport(
            TEXT("Social Advisor"),
            TEXT("Stability"),
            FMath::Max(Country.Unrest, 100 - Country.Stability),
            GuidanceLevel,
            FString::Printf(TEXT("Approval %d, stability %d, unrest %d."), Country.PublicApproval, Country.Stability, Country.Unrest),
            GuidanceText(GuidanceLevel,
                TEXT("Keep unrest below the warning threshold and stability above the warning threshold. Civil Liberties helps approval; Emergency Powers lowers unrest but damages trust."),
                TEXT("Reduce unrest before it crosses the warning threshold."),
                TEXT("Public order is weakening. Act soon."),
                TEXT("Watch unrest.")),
            Country.Unrest >= State.FailureRisk.UnrestWarningThreshold || Country.Stability <= State.FailureRisk.StabilityWarningThreshold ? TEXT("Internal failure warnings are active.") : TEXT("Internal stability is above warning thresholds."),
            TEXT("Public freedoms improve legitimacy over time; emergency authority buys control at the cost of approval and diplomacy.")));

        Reports.Add(MakeAdvisorReport(
            TEXT("Military Advisor"),
            TEXT("Military"),
            FMath::Max(0, 75 - Country.MilitaryReadiness) + InvasionRiskPct / 2,
            GuidanceLevel,
            FString::Printf(TEXT("Military readiness %d, invasion risk %d/%d."), Country.MilitaryReadiness, State.InvasionRisk.CurrentInvasionRisk, State.InvasionRisk.InvasionRiskTrigger),
            GuidanceText(GuidanceLevel,
                TEXT("Readiness below the warning threshold should be corrected before rival pressure rises. National Mobilization is fast but expensive and unpopular."),
                TEXT("Raise readiness or lower border pressure through diplomacy."),
                TEXT("Defense posture is weak. Review military policy."),
                TEXT("Watch readiness.")),
            Country.MilitaryReadiness <= State.InvasionRisk.MilitaryReadinessWarningThreshold ? TEXT("Military readiness warning is active.") : TEXT("Military readiness is above warning level."),
            TEXT("Mobilization lowers takeover risk but drains treasury and can increase unrest.")));

        Reports.Add(MakeAdvisorReport(
            TEXT("Diplomacy Advisor"),
            TEXT("Diplomacy"),
            FMath::Max(0, 75 - Country.DiplomaticStanding) + InvasionRiskPct / 3,
            GuidanceLevel,
            FString::Printf(TEXT("Diplomatic standing %d, democratic allies %d, non-democratic countries %d."), Country.DiplomaticStanding, State.WorldMap.DemocraticAllyCount, State.WorldMap.NonDemocraticCountryCount),
            GuidanceText(GuidanceLevel,
                TEXT("Alliance Outreach improves diplomacy and reduces takeover risk, especially when military readiness is low."),
                TEXT("Use diplomacy to reduce foreign pressure."),
                TEXT("Foreign pressure is increasing. Review posture."),
                TEXT("Watch diplomacy.")),
            Country.DiplomaticStanding < 40 ? TEXT("Diplomatic isolation is becoming dangerous.") : TEXT("Diplomatic standing is currently manageable."),
            TEXT("Concessions can buy safety but may slow domestic spending or appear weak to hardline blocs.")));

        Reports.Add(MakeAdvisorReport(
            TEXT("Infrastructure Advisor"),
            TEXT("Infrastructure"),
            FMath::Max(0, 80 - Country.Infrastructure),
            GuidanceLevel,
            FString::Printf(TEXT("Infrastructure %d, technology %d."), Country.Infrastructure, Country.Technology),
            GuidanceText(GuidanceLevel,
                TEXT("Infrastructure improves resource resilience. Stimulus Spending or Industrial Subsidies can help, but each carries treasury or environmental costs."),
                TEXT("Improve infrastructure to soften shortages."),
                TEXT("Infrastructure is limiting resilience."),
                TEXT("Watch infrastructure.")),
            Country.Infrastructure < 35 ? TEXT("Infrastructure is near crisis levels.") : TEXT("Infrastructure is functional."),
            TEXT("Construction improves long-term stability but competes with defense and public relief funding.")));

        Reports.Add(MakeAdvisorReport(
            TEXT("Security Advisor"),
            TEXT("Security"),
            FMath::Max(InternalRiskPct, InvasionRiskPct),
            GuidanceLevel,
            FString::Printf(TEXT("Assassination risk %d/%d, foreign takeover risk %d/%d."), State.FailureRisk.CurrentAssassinationRisk, State.FailureRisk.AssassinationRiskTrigger, State.InvasionRisk.CurrentInvasionRisk, State.InvasionRisk.InvasionRiskTrigger),
            GuidanceText(GuidanceLevel,
                TEXT("Internal threats follow unrest and instability; foreign threats follow readiness and diplomacy. Treat whichever risk percentage is higher first."),
                TEXT("Address the highest active fail-state risk first."),
                TEXT("Failure risk is rising. Identify the pressure source."),
                TEXT("Watch risk.")),
            (InternalRiskPct >= 50 || InvasionRiskPct >= 50) ? TEXT("A fail-state risk is above half of its trigger value.") : TEXT("No immediate fail-state trigger warning."),
            TEXT("Security crackdowns may reduce immediate risk while worsening public trust or diplomacy.")));

        Reports.Add(MakeAdvisorReport(
            TEXT("Public Welfare Advisor"),
            TEXT("Welfare"),
            FMath::Max(0, 70 - Country.PublicApproval) + FMath::Max(0, 60 - Country.EnvironmentalHealth),
            GuidanceLevel,
            FString::Printf(TEXT("Approval %d, environmental health %d, climate %s."), Country.PublicApproval, Country.EnvironmentalHealth, *Country.Climate),
            GuidanceText(GuidanceLevel,
                TEXT("Public welfare improves when shortages, pollution, and approval pressure are controlled together. Conservation helps environment but can slow extraction."),
                TEXT("Balance welfare, environment, and resource output."),
                TEXT("Welfare pressure is visible. Review shortages and environment."),
                TEXT("Watch approval.")),
            Country.PublicApproval < 40 ? TEXT("Low approval can accelerate unrest.") : TEXT("Approval is not in immediate collapse."),
            TEXT("Relief programs improve quality of life but cost treasury and may slow military or industrial priorities.")));

        Reports.Sort([](const FDemocracyAdvisorReport& Left, const FDemocracyAdvisorReport& Right)
        {
            return Left.Severity > Right.Severity;
        });

        return Reports;
    }

    struct FAdvisorAgendaOption
    {
        FString Label;
        FString AgendaItem;
        FString Recommendation;
        FString ConsequencePreview;
    };
    FString BuildGuidedAdvisorAgendaText(const FString& GuidanceLevel, const FAdvisorAgendaOption& Option)
    {
        return GuidanceText(GuidanceLevel,
            FString::Printf(TEXT("%s\nConsequence: %s\nTutorial cue: compare this agenda to current signals before choosing."), *Option.Recommendation, *Option.ConsequencePreview),
            FString::Printf(TEXT("%s\nConsequence: %s"), *Option.Recommendation, *Option.ConsequencePreview),
            FString::Printf(TEXT("Direction: %s"), *Option.Recommendation),
            TEXT("Agenda detail hidden by difficulty guidance."));
    }

    const FDemocracyAdvisorReport* FindAdvisorReport(const FDemocracySimulationState& State, const FString& AdvisorName)
    {
        for (const FDemocracyAdvisorReport& Report : State.AdvisorSystem.Reports)
        {
            if (Report.AdvisorName.Equals(AdvisorName, ESearchCase::IgnoreCase))
            {
                return &Report;
            }
        }
        return nullptr;
    }

    TArray<FAdvisorAgendaOption> GetAdvisorAgendaOptions(const FString& AdvisorName)
    {
        if (AdvisorName.Equals(TEXT("Resource Manager"), ESearchCase::IgnoreCase))
        {
            return {
                { TEXT("Stockpile Audit"), TEXT("Resource Stockpile Audit"), TEXT("Review food, water, fuel, wood, and metals reserves against immediate shortage pressure."), TEXT("+food, +water, +advisor coordination. Low-cost review with small unrest relief.") },
                { TEXT("Import Relief Package"), TEXT("Resource Import Relief Package"), TEXT("Authorize emergency imports and rationing support for essential goods."), TEXT("+food, +water, -unrest, +stability, -treasury. Best when shortages are already active.") },
                { TEXT("Production Surge"), TEXT("Resource Production Surge"), TEXT("Redirect capacity toward resource extraction and reserve rebuilding."), TEXT("+food, +water, +fuel, +wood, +metals, -environment, -treasury. Stronger long-term supply pressure relief.") }
            };
        }
        if (AdvisorName.Equals(TEXT("Military Advisor"), ESearchCase::IgnoreCase))
        {
            return {
                { TEXT("Readiness Review"), TEXT("Military Readiness Review"), TEXT("Assess readiness, logistics, and the current foreign takeover risk."), TEXT("+military readiness, slight invasion risk reduction. Low-cost defensive briefing.") },
                { TEXT("Border Defense Plan"), TEXT("Military Border Defense Plan"), TEXT("Move resources into border defense, logistics, and deterrence."), TEXT("+military readiness, -invasion risk, -treasury. Strong defensive improvement.") },
                { TEXT("Emergency Mobilization"), TEXT("Military Emergency Mobilization"), TEXT("Activate immediate mobilization for a high-risk military posture."), TEXT("Large +military readiness, -invasion risk, -treasury, +unrest. Use when takeover risk is severe.") }
            };
        }
        if (AdvisorName.Equals(TEXT("Social Advisor"), ESearchCase::IgnoreCase))
        {
            return {
                { TEXT("Public Sentiment Briefing"), TEXT("Social Public Sentiment Briefing"), TEXT("Review approval groups, unrest causes, and stability warnings."), TEXT("+stability, -unrest. Low-cost visibility into unrest sources.") },
                { TEXT("Community Stabilization"), TEXT("Social Community Stabilization"), TEXT("Fund local outreach, dispute response, and trust rebuilding."), TEXT("+approval, +stability, -unrest, -treasury. Reliable internal stability action.") },
                { TEXT("Emergency Calm Initiative"), TEXT("Social Emergency Calm Initiative"), TEXT("Launch urgent public order and community support operations."), TEXT("Strong -unrest and +stability, moderate +approval, -treasury. Best during active internal risk.") }
            };
        }
        if (AdvisorName.Equals(TEXT("Economic Advisor"), ESearchCase::IgnoreCase))
        {
            return {
                { TEXT("Budget Review"), TEXT("Economic Budget Review"), TEXT("Review taxes, spending, deficit, inflation, and treasury runway."), TEXT("+economy, +treasury. Low-risk fiscal clarity.") },
                { TEXT("Revenue Plan"), TEXT("Economic Revenue Plan"), TEXT("Improve tax collection and reduce waste without a full austerity push."), TEXT("+treasury, +economy, slight -approval. Good when reserves are low.") },
                { TEXT("Stabilization Package"), TEXT("Economic Stabilization Package"), TEXT("Use a targeted package to support services and stabilize production."), TEXT("+economy, +approval, +public services, -treasury, slight -inflation. Best when the economy is weak.") }
            };
        }
        if (AdvisorName.Equals(TEXT("Diplomacy Advisor"), ESearchCase::IgnoreCase))
        {
            return {
                { TEXT("Foreign Pressure Briefing"), TEXT("Diplomacy Foreign Pressure Briefing"), TEXT("Review rivals, allies, diplomatic standing, and border pressure."), TEXT("+diplomacy, +foreign trust. Low-cost foreign risk clarity.") },
                { TEXT("Treaty Outreach"), TEXT("Diplomacy Treaty Outreach"), TEXT("Coordinate with friendly states to improve diplomatic standing."), TEXT("+diplomacy, +foreign trust, -treasury. Reduces takeover pressure indirectly.") },
                { TEXT("Crisis De-escalation"), TEXT("Diplomacy Crisis De-escalation"), TEXT("Open urgent channels with hostile or pressured borders."), TEXT("Strong -invasion risk, +diplomacy, -treasury. Best when foreign pressure is active.") }
            };
        }
        if (AdvisorName.Equals(TEXT("Infrastructure Advisor"), ESearchCase::IgnoreCase))
        {
            return {
                { TEXT("Asset Condition Review"), TEXT("Infrastructure Asset Condition Review"), TEXT("Review roads, power, communications, and logistics weak points."), TEXT("+infrastructure. Low-cost resilience review.") },
                { TEXT("Repair Priority Plan"), TEXT("Infrastructure Repair Priority Plan"), TEXT("Prioritize repairs to the infrastructure most tied to production and public stability."), TEXT("+infrastructure, +economy, -wood, -metals, -treasury. Balanced repair push.") },
                { TEXT("National Works Surge"), TEXT("Infrastructure National Works Surge"), TEXT("Launch a broad construction and logistics surge."), TEXT("Large +infrastructure, +stability, -wood, -metals, -treasury. Strong long-term resilience.") }
            };
        }
        if (AdvisorName.Equals(TEXT("Security Advisor"), ESearchCase::IgnoreCase))
        {
            return {
                { TEXT("Threat Assessment"), TEXT("Security Threat Assessment"), TEXT("Review assassination risk, unrest sources, and hostile foreign activity."), TEXT("-assassination risk, +stability. Low-cost security clarity.") },
                { TEXT("Protective Detail Upgrade"), TEXT("Security Protective Detail Upgrade"), TEXT("Improve leader protection, internal monitoring, and rapid incident response."), TEXT("-assassination risk, +stability, -treasury. Direct internal fail-state protection.") },
                { TEXT("Counter Threat Operation"), TEXT("Security Counter Threat Operation"), TEXT("Run an urgent operation against high-risk internal threats."), TEXT("Strong -assassination risk, -unrest, -treasury, slight -approval. Use when internal risk is high.") }
            };
        }
        if (AdvisorName.Equals(TEXT("Public Welfare Advisor"), ESearchCase::IgnoreCase))
        {
            return {
                { TEXT("Needs Assessment"), TEXT("Welfare Needs Assessment"), TEXT("Review health, education, food access, public services, and quality-of-life gaps."), TEXT("+approval, +public services. Low-cost welfare clarity.") },
                { TEXT("Targeted Relief Program"), TEXT("Welfare Targeted Relief Program"), TEXT("Fund focused relief where unmet needs are driving unrest."), TEXT("+approval, -unrest, +public services, -treasury. Strong public trust action.") },
                { TEXT("Public Services Expansion"), TEXT("Welfare Public Services Expansion"), TEXT("Expand health, education, and quality-of-life support broadly."), TEXT("Large +approval and +public services, -treasury, +stability. Best when approval is weak.") }
            };
        }

        return {
            { TEXT("Situation Briefing"), TEXT("Situation Briefing"), TEXT("Review the current state and active risks."), TEXT("+advisor coordination. Low-cost meeting.") },
            { TEXT("Focused Action Plan"), TEXT("Focused Action Plan"), TEXT("Choose a practical next action for this advisor domain."), TEXT("+advisor coordination, moderate domain improvement, -treasury.") },
            { TEXT("Emergency Response"), TEXT("Emergency Response"), TEXT("Use immediate emergency authority for this advisor domain."), TEXT("Strong domain improvement, higher treasury cost and possible tradeoffs.") }
        };
    }
    struct FPressAnnouncementOption
    {
        FString Label;
        FString AnnouncementType;
        FString Purpose;
        FString ConsequencePreview;
    };

    TArray<FPressAnnouncementOption> GetPressAnnouncementOptions()
    {
        return {
            { TEXT("Crisis Reassurance"), TEXT("Crisis Reassurance"), TEXT("Address active unrest, instability, shortages, or public fear with a substantive update."), TEXT("Usually +stability, -unrest, small +credibility. Stronger when press credibility is already healthy.") },
            { TEXT("Policy Explanation"), TEXT("Policy Explanation"), TEXT("Explain recent policy choices and the tradeoffs behind them."), TEXT("Usually +approval, +stability, -unrest, +credibility. Good after policy or budget changes.") },
            { TEXT("Diplomatic Address"), TEXT("Diplomatic Address"), TEXT("Speak to foreign officials and domestic observers about external posture."), TEXT("Usually +diplomacy, +stability, -unrest, +credibility. Helps foreign standing more than domestic approval.") },
            { TEXT("Victory Claim"), TEXT("Victory Claim"), TEXT("Highlight a recent success only if current national signals can support it."), TEXT("If supported: +approval, +stability, +credibility. If unsupported: -approval, -diplomacy, +unrest, heavy -credibility.") },
            { TEXT("Empty Statement"), TEXT("Empty Statement"), TEXT("Say very little while appearing before the press."), TEXT("-approval, -stability, +unrest, -credibility. Repeated empty statements stack harsher penalties.") },
            { TEXT("False Claim"), TEXT("False Claim"), TEXT("Make a claim that is not supported by the current state."), TEXT("Heavy -credibility, -approval, -stability, -diplomacy, +unrest. Repeated false claims stack severe penalties.") }
        };
    }
    FDemocracyEventChoiceState MakeEventChoice(const FString& Id, const FString& Label, const FString& Description, const FString& Preview,
        int32 Approval, int32 Stability, int32 Unrest, int32 Treasury, int32 Economy, int32 Diplomacy, int32 Military, int32 Infrastructure,
        int32 Environment, int32 Food, int32 Water, int32 GasOil, int32 Wood, int32 Metals, int32 AssassinationRisk, int32 InvasionRisk)
    {
        FDemocracyEventChoiceState Choice;
        Choice.ChoiceId = Id;
        Choice.Label = Label;
        Choice.Description = Description;
        Choice.ConsequencePreview = Preview;
        Choice.ApprovalDelta = Approval;
        Choice.StabilityDelta = Stability;
        Choice.UnrestDelta = Unrest;
        Choice.TreasuryDelta = Treasury;
        Choice.EconomicDelta = Economy;
        Choice.DiplomacyDelta = Diplomacy;
        Choice.MilitaryDelta = Military;
        Choice.InfrastructureDelta = Infrastructure;
        Choice.EnvironmentDelta = Environment;
        Choice.FoodDelta = Food;
        Choice.WaterDelta = Water;
        Choice.GasOilDelta = GasOil;
        Choice.WoodDelta = Wood;
        Choice.MetalsDelta = Metals;
        Choice.AssassinationRiskDelta = AssassinationRisk;
        Choice.InvasionRiskDelta = InvasionRisk;
        return Choice;
    }

    int32 CalculateEventDeadlineTurn(const FDemocracySimulationState& State, int32 Severity)
    {
        const int32 DifficultyScore = FMath::Clamp(State.PlayerCountry.CountrySizeScore, 1, 4);
        const int32 SeverityPressure = FMath::Clamp(Severity / 35, 0, 2);
        const int32 ResponseWindow = FMath::Clamp(7 - DifficultyScore - SeverityPressure, 2, 6);
        return State.Turn + ResponseWindow;
    }

    void ConfigureEventResolutionLoop(FDemocracyActiveEventState& Event);
    FDemocracyActiveEventState MakeEvent(FDemocracySimulationState& State, const FString& Type, const FString& Title, const FString& Description, const FString& TriggerReason, int32 Severity, bool bTriggered, const TArray<FDemocracyEventChoiceState>& Choices)
    {
        ++State.EventSystem.EventCounter;
        FDemocracyActiveEventState Event;
        Event.EventId = FString::Printf(TEXT("EVT-%04d-T%d"), State.EventSystem.EventCounter, State.Turn);
        Event.EventType = Type;
        Event.Title = Title;
        Event.Description = Description;
        Event.TriggerReason = TriggerReason;
        Event.CreatedTurn = State.Turn;
        Event.Severity = FMath::Clamp(Severity, 1, 100);
        Event.DeadlineTurn = CalculateEventDeadlineTurn(State, Event.Severity);
        Event.bTriggered = bTriggered;
        Event.Choices = Choices;
        ConfigureEventResolutionLoop(Event);
        return Event;
    }

    bool HasActiveEventType(const FDemocracyEventSystemState& EventSystem, const FString& EventType)
    {
        for (const FDemocracyActiveEventState& Event : EventSystem.ActiveEvents)
        {
            if (!Event.bResolved && Event.EventType.Equals(EventType, ESearchCase::IgnoreCase))
            {
                return true;
            }
        }
        return false;
    }

    void PruneResolvedEvents(FDemocracyEventSystemState& EventSystem)
    {
        EventSystem.ActiveEvents.RemoveAll([](const FDemocracyActiveEventState& Event)
        {
            return Event.bResolved;
        });
    }

    void GenerateSimulationEvents(FDemocracySimulationState& State)
    {
        FDemocracyCountryState& Country = State.PlayerCountry;
        FDemocracyResourceInventory& Resources = Country.Resources;
        FDemocracyEventSystemState& EventSystem = State.EventSystem;
        PruneResolvedEvents(EventSystem);

        if (EventSystem.ActiveEvents.Num() >= EventSystem.ActiveEventLimit)
        {
            return;
        }

        const int32 DifficultyScore = FMath::Clamp(Country.CountrySizeScore, 1, 4);
        const int32 EventRoll = (State.Turn * 37 + State.RtsWorld.SimulationSecond * 11 + DifficultyScore * 17 + EventSystem.EventCounter * 23) % 100;
        const bool bCooldownReady = State.Turn > EventSystem.LastEventTurn;

        auto AddEvent = [&](FDemocracyActiveEventState&& Event)
        {
            if (EventSystem.ActiveEvents.Num() < EventSystem.ActiveEventLimit && !HasActiveEventType(EventSystem, Event.EventType))
            {
                EventSystem.ActiveEvents.Add(Event);
                EventSystem.LastEventTurn = State.Turn;
                State.Phase = TEXT("Event Decision Pending");
            }
        };

        if ((Resources.Food < 90 || Resources.Water < 75) && !HasActiveEventType(EventSystem, TEXT("Shortage")))
        {
            AddEvent(MakeEvent(State, TEXT("Shortage"), TEXT("Household Shortage Reports"), TEXT("Regional administrators report that families are struggling to access essential supplies."), TEXT("Triggered by low food or water reserves."), 70, true, {
                MakeEventChoice(TEXT("relief"), TEXT("Release emergency reserves"), TEXT("Spend treasury to distribute food and water immediately."), TEXT("Food/water improve, unrest drops, treasury falls."), 4, 3, -8, -140, 0, 0, 0, 0, 0, 80, 70, 0, 0, 0, -4, 0),
                MakeEventChoice(TEXT("ration"), TEXT("Order rationing"), TEXT("Stretch supplies while accepting public frustration."), TEXT("Resources stabilize modestly, approval falls, unrest rises."), -6, -2, 4, -20, 0, 0, 0, 0, 0, 35, 30, 0, 0, 0, 2, 0),
                MakeEventChoice(TEXT("market"), TEXT("Let markets adjust"), TEXT("Avoid direct spending and let prices signal scarcity."), TEXT("Treasury protected, economy improves slightly, unrest rises."), -8, -4, 8, 40, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0)
            }));
            return;
        }

        const int32 FuelShortage = GetResourceChainShortage(State.ResourceChains, TEXT("Fuel"));
        const int32 WoodShortage = GetResourceChainShortage(State.ResourceChains, TEXT("Wood"));
        const int32 MetalsShortage = GetResourceChainShortage(State.ResourceChains, TEXT("Metals"));
        if ((FuelShortage >= 20 || Resources.GasOil < 55) && !HasActiveEventType(EventSystem, TEXT("Fuel Shortage")))
        {
            AddEvent(MakeEvent(State, TEXT("Fuel Shortage"), TEXT("Fuel Logistics Emergency"), TEXT("Fuel reserves are disrupting logistics, industry, emergency services, and military readiness."), TEXT("Triggered by fuel shortage pressure."), FMath::Clamp(55 + FuelShortage / 2, 55, 90), true, {
                MakeEventChoice(TEXT("fuel-imports"), TEXT("Buy emergency fuel"), TEXT("Use treasury and diplomatic channels to secure emergency fuel shipments."), TEXT("Fuel improves, readiness stabilizes, treasury falls."), 1, 2, -3, -150, 0, 1, 3, 0, -1, 0, 0, 75, 0, 0, -2, -2),
                MakeEventChoice(TEXT("fuel-ration"), TEXT("Ration civilian fuel"), TEXT("Prioritize military, hospitals, and logistics while restricting civilian fuel use."), TEXT("Readiness protected, unrest rises, economy suffers."), -5, -2, 5, -25, -3, 0, 4, 0, 0, 0, 0, 35, 0, 0, 3, -1),
                MakeEventChoice(TEXT("fuel-ignore"), TEXT("Wait for markets"), TEXT("Avoid immediate intervention and hope suppliers adapt."), TEXT("Treasury protected, readiness and production suffer."), -4, -4, 6, 20, -5, 0, -5, -1, 0, 0, 0, 0, 0, 0, 5, 4)
            }));
            return;
        }

        if ((WoodShortage >= 22 || MetalsShortage >= 22 || Resources.Wood < 45 || Resources.Metals < 45) && !HasActiveEventType(EventSystem, TEXT("Material Shortage")))
        {
            AddEvent(MakeEvent(State, TEXT("Material Shortage"), TEXT("Construction Materials Shortage"), TEXT("Wood and metals shortages are slowing infrastructure repairs, industry, and defense production."), TEXT("Triggered by wood or metals shortage pressure."), FMath::Clamp(50 + (WoodShortage + MetalsShortage) / 3, 50, 88), true, {
                MakeEventChoice(TEXT("materials-import"), TEXT("Import key materials"), TEXT("Buy wood and metals to keep infrastructure and defense projects moving."), TEXT("Wood/metals improve, infrastructure holds, treasury falls."), 1, 2, -2, -130, 1, 0, 1, 3, -1, 0, 0, 0, 60, 60, -1, -1),
                MakeEventChoice(TEXT("materials-prioritize"), TEXT("Prioritize critical projects"), TEXT("Divert limited materials to utilities, defense repair, and food logistics."), TEXT("Infrastructure and readiness stabilize, approval suffers from delays."), -3, 1, 2, -40, -2, 0, 2, 4, 0, 0, 0, 0, 20, 25, 1, -1),
                MakeEventChoice(TEXT("materials-delay"), TEXT("Delay construction"), TEXT("Pause nonessential projects to preserve cash."), TEXT("Treasury protected, infrastructure and production suffer."), -4, -3, 4, 35, -4, 0, -2, -5, 0, 0, 0, 0, 0, 0, 3, 2)
            }));
            return;
        }
        if ((Country.Unrest >= State.FailureRisk.UnrestWarningThreshold || Country.PublicApproval < 42) && !HasActiveEventType(EventSystem, TEXT("Protest")))
        {
            AddEvent(MakeEvent(State, TEXT("Protest"), TEXT("Capital Protest Movement"), TEXT("Organized demonstrations are forming near government offices and transportation hubs."), TEXT("Triggered by unrest or weak public approval."), 65, true, {
                MakeEventChoice(TEXT("address"), TEXT("Address the public"), TEXT("Promise reforms and open talks with organizers."), TEXT("Approval and stability improve, treasury cost is moderate."), 7, 4, -6, -60, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, -5, 0),
                MakeEventChoice(TEXT("security"), TEXT("Deploy security cordons"), TEXT("Contain protests with police and military support."), TEXT("Unrest drops now, approval and diplomacy suffer."), -8, 2, -7, -30, 0, -3, 2, 0, 0, 0, 0, 0, 0, 0, 4, 1),
                MakeEventChoice(TEXT("ignore"), TEXT("Wait it out"), TEXT("Avoid giving the movement legitimacy."), TEXT("Treasury protected, unrest and assassination risk rise."), -5, -5, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 0)
            }));
            return;
        }

        if ((Country.DiplomaticStanding < 45 || State.InvasionRisk.CurrentInvasionRisk > State.InvasionRisk.InvasionRiskTrigger / 3) && !HasActiveEventType(EventSystem, TEXT("Border Tension")))
        {
            AddEvent(MakeEvent(State, TEXT("Border Tension"), TEXT("Border Tension Escalates"), TEXT("A rival state has moved forces close to a disputed crossing."), TEXT("Triggered by low diplomacy or rising invasion risk."), 75, true, {
                MakeEventChoice(TEXT("deescalate"), TEXT("Send envoys"), TEXT("Open a crisis channel and offer inspection guarantees."), TEXT("Diplomacy improves and invasion risk falls, treasury cost rises."), 1, 2, -2, -80, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, -10),
                MakeEventChoice(TEXT("reinforce"), TEXT("Reinforce the border"), TEXT("Move units to visible defensive positions."), TEXT("Readiness rises, invasion risk can fall, diplomacy suffers."), 0, 1, 1, -90, 0, -5, 8, 0, 0, 0, 0, -20, 0, -20, 0, -4),
                MakeEventChoice(TEXT("concede"), TEXT("Offer concessions"), TEXT("Trade a limited concession for calm."), TEXT("Diplomacy improves but approval and stability fall."), -6, -3, 3, -20, 0, 10, -1, 0, 0, 0, 0, 0, 0, 0, 2, -8)
            }));
            return;
        }

        if (bCooldownReady && EventRoll < 12 + DifficultyScore * 3 && !HasActiveEventType(EventSystem, TEXT("Economic Shock")))
        {
            AddEvent(MakeEvent(State, TEXT("Economic Shock"), TEXT("Sudden Market Shock"), TEXT("Credit markets tighten and major employers pause investment."), TEXT("Random economic shock."), 45 + DifficultyScore * 8, false, {
                MakeEventChoice(TEXT("stimulus"), TEXT("Emergency stimulus"), TEXT("Inject public funds to protect jobs."), TEXT("Economy and approval improve, treasury drops."), 5, 2, -3, -180, 8, 0, 0, 2, 0, 0, 0, 0, 0, 0, -2, 0),
                MakeEventChoice(TEXT("austerity"), TEXT("Protect the treasury"), TEXT("Cut spending and preserve reserves."), TEXT("Treasury improves, approval and economy suffer."), -7, -3, 5, 120, -6, 0, 0, -2, 0, 0, 0, 0, 0, 0, 5, 0),
                MakeEventChoice(TEXT("targeted"), TEXT("Target key industries"), TEXT("Support logistics, agriculture, and materials producers."), TEXT("Economy and resources improve with moderate cost."), 2, 1, -2, -100, 5, 0, 0, 1, -1, 25, 10, 10, 15, 15, -1, 0)
            }));
            return;
        }

        if (bCooldownReady && EventRoll >= 12 && EventRoll < 20 + DifficultyScore * 3 && !HasActiveEventType(EventSystem, TEXT("Disaster")))
        {
            AddEvent(MakeEvent(State, TEXT("Disaster"), TEXT("Severe Weather Disaster"), TEXT("A destructive weather system damages infrastructure and disrupts supply routes."), TEXT("Random disaster event."), 55 + DifficultyScore * 7, false, {
                MakeEventChoice(TEXT("mobilize"), TEXT("Mobilize relief agencies"), TEXT("Fund a fast national response."), TEXT("Stability improves, infrastructure damage is reduced, treasury falls."), 4, 5, -6, -160, 0, 1, 0, 5, 0, 30, 25, 0, 10, 0, -3, 0),
                MakeEventChoice(TEXT("local"), TEXT("Leave it to local officials"), TEXT("Let regional offices manage the response."), TEXT("Treasury cost is low, infrastructure and approval suffer."), -5, -4, 6, -30, -2, 0, 0, -8, -2, -20, -15, 0, -10, 0, 4, 0),
                MakeEventChoice(TEXT("military"), TEXT("Use military logistics"), TEXT("Deploy military transport and engineers."), TEXT("Readiness and supplies fall, stability improves."), 2, 4, -5, -90, 0, 0, -5, 4, 0, 20, 20, -15, 0, -10, -2, 1)
            }));
            return;
        }

        if (bCooldownReady && EventRoll >= 20 && EventRoll < 28 + DifficultyScore * 3 && !HasActiveEventType(EventSystem, TEXT("Scandal")))
        {
            AddEvent(MakeEvent(State, TEXT("Scandal"), TEXT("Administration Scandal"), TEXT("Reports allege misconduct inside a senior ministry."), TEXT("Random scandal event."), 50 + DifficultyScore * 7, false, {
                MakeEventChoice(TEXT("investigate"), TEXT("Open investigation"), TEXT("Authorize a visible independent review."), TEXT("Approval recovers over time, stability holds, short-term economy cost."), 4, 3, -3, -70, -1, 2, 0, 0, 0, 0, 0, 0, 0, 0, -4, 0),
                MakeEventChoice(TEXT("deny"), TEXT("Deny and attack critics"), TEXT("Reject the accusations as political sabotage."), TEXT("Treasury protected, unrest and assassination risk rise."), -8, -5, 8, 0, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0),
                MakeEventChoice(TEXT("dismiss"), TEXT("Dismiss the minister"), TEXT("Remove the official and promise reform."), TEXT("Approval improves but infrastructure and economy lose momentum."), 6, 1, -4, -40, -2, 1, 0, -2, 0, 0, 0, 0, 0, 0, -3, 0)
            }));
        }
    }


    int32 GetEventDeadlineTurn(const FDemocracySimulationState& State, const FDemocracyActiveEventState& Event)
    {
        return Event.DeadlineTurn > 0 ? Event.DeadlineTurn : CalculateEventDeadlineTurn(State, Event.Severity);
    }
    FString BuildEventDeadlineText(const FDemocracySimulationState& State, const FDemocracyActiveEventState& Event)
    {
        const int32 DeadlineTurn = GetEventDeadlineTurn(State, Event);
        const int32 TurnsRemaining = DeadlineTurn - State.Turn;
        if (TurnsRemaining < 0)
        {
            return FString::Printf(TEXT("Deadline missed on turn %d. Follow-up effects will apply on the next simulation tick."), DeadlineTurn);
        }
        if (TurnsRemaining == 0)
        {
            return FString::Printf(TEXT("Deadline turn %d. Choose before the next simulation tick or the event escalates."), DeadlineTurn);
        }
        return FString::Printf(TEXT("Deadline turn %d. %d turn(s) remaining before automatic escalation."), DeadlineTurn, TurnsRemaining);
    }

    void AddDeltaLine(TArray<FString>& Lines, const FString& Label, int32 Delta)
    {
        if (Delta != 0)
        {
            Lines.Add(FString::Printf(TEXT("%s %+d"), *Label, Delta));
        }
    }

    FString BuildEventChoiceImpactText(const FDemocracyEventChoiceState& Choice)
    {
        TArray<FString> Lines;
        AddDeltaLine(Lines, TEXT("Approval"), Choice.ApprovalDelta);
        AddDeltaLine(Lines, TEXT("Stability"), Choice.StabilityDelta);
        AddDeltaLine(Lines, TEXT("Unrest"), Choice.UnrestDelta);
        AddDeltaLine(Lines, TEXT("Treasury"), Choice.TreasuryDelta);
        AddDeltaLine(Lines, TEXT("Economy"), Choice.EconomicDelta);
        AddDeltaLine(Lines, TEXT("Diplomacy"), Choice.DiplomacyDelta);
        AddDeltaLine(Lines, TEXT("Military"), Choice.MilitaryDelta);
        AddDeltaLine(Lines, TEXT("Infrastructure"), Choice.InfrastructureDelta);
        AddDeltaLine(Lines, TEXT("Environment"), Choice.EnvironmentDelta);
        AddDeltaLine(Lines, TEXT("Food"), Choice.FoodDelta);
        AddDeltaLine(Lines, TEXT("Water"), Choice.WaterDelta);
        AddDeltaLine(Lines, TEXT("Fuel"), Choice.GasOilDelta);
        AddDeltaLine(Lines, TEXT("Wood"), Choice.WoodDelta);
        AddDeltaLine(Lines, TEXT("Metals"), Choice.MetalsDelta);
        AddDeltaLine(Lines, TEXT("Assassination risk"), Choice.AssassinationRiskDelta);
        AddDeltaLine(Lines, TEXT("Invasion risk"), Choice.InvasionRiskDelta);
        return Lines.Num() > 0 ? FString::Join(Lines, TEXT(" | ")) : TEXT("No direct stat changes.");
    }

    FString BuildMeetingDirectImpactText(const FDemocracyMeetingRecordState& Record)
    {
        TArray<FString> Lines;
        AddDeltaLine(Lines, TEXT("Approval"), Record.ApprovalDelta);
        AddDeltaLine(Lines, TEXT("Stability"), Record.StabilityDelta);
        AddDeltaLine(Lines, TEXT("Unrest"), Record.UnrestDelta);
        AddDeltaLine(Lines, TEXT("Diplomacy"), Record.DiplomacyDelta);
        AddDeltaLine(Lines, TEXT("Treasury"), Record.TreasuryDelta);
        AddDeltaLine(Lines, TEXT("Economy"), Record.EconomyDelta);
        AddDeltaLine(Lines, TEXT("Military"), Record.MilitaryDelta);
        AddDeltaLine(Lines, TEXT("Infrastructure"), Record.InfrastructureDelta);
        AddDeltaLine(Lines, TEXT("Advisor coordination"), Record.AdvisorCoordinationDelta);
        AddDeltaLine(Lines, TEXT("Foreign trust"), Record.ForeignTrustDelta);
        return Lines.Num() > 0 ? FString::Join(Lines, TEXT(" | ")) : TEXT("No direct stat changes.");
    }

    FString BuildMeetingResourceSnapshotText(const FDemocracyResourceInventory& Resources)
    {
        return FString::Printf(TEXT("food %d | water %d | fuel %d | wood %d | metals %d"),
            Resources.Food,
            Resources.Water,
            Resources.GasOil,
            Resources.Wood,
            Resources.Metals);
    }

    FString BuildMeetingDecisionConsequenceText(const FDemocracyMeetingRecordState& Record, const FDemocracySimulationState& State)
    {
        const FDemocracyCountryState& Country = State.PlayerCountry;
        return FString::Printf(TEXT("%s Direct modifiers: %s. Current state: approval %d, stability %d, unrest %d, treasury %d, economy %d, military %d, infrastructure %d. Resources: %s."),
            *Record.OutcomeSummary,
            *BuildMeetingDirectImpactText(Record),
            Country.PublicApproval,
            Country.Stability,
            Country.Unrest,
            Country.Treasury,
            Country.EconomicHealth,
            Country.MilitaryReadiness,
            Country.Infrastructure,
            *BuildMeetingResourceSnapshotText(Country.Resources));
    }

    void RefreshResourceChainsFromCurrentCadence(FDemocracySimulationState& State)
    {
        const int32 DifficultyScore = FMath::Clamp(State.PlayerCountry.CountrySizeScore, 1, 4);
        const int32 FoodUse = 6 + DifficultyScore * 3;
        const int32 WaterUse = 5 + DifficultyScore * 2;
        const int32 GasUse = 3 + DifficultyScore;
        const int32 WoodUse = 2 + DifficultyScore;
        const int32 MetalsUse = 2 + DifficultyScore;
        RecalculateResourceProductionChains(State, BuildPolicyModifiers(State.PlayerCountry.Policies), FoodUse, WaterUse, GasUse, WoodUse, MetalsUse);
    }

    TArray<FString> GetAdvisorMeetingDepartmentNames(const FString& AdvisorName, const FString& AgendaItem)
    {
        TArray<FString> DepartmentNames;
        if (AdvisorName.Equals(TEXT("Resource Manager"), ESearchCase::IgnoreCase))
        {
            DepartmentNames.Add(TEXT("Agriculture"));
            DepartmentNames.Add(AgendaItem.Contains(TEXT("Production"), ESearchCase::IgnoreCase) || AgendaItem.Contains(TEXT("Fuel"), ESearchCase::IgnoreCase) ? TEXT("Energy") : TEXT("Infrastructure"));
        }
        else if (AdvisorName.Equals(TEXT("Military Advisor"), ESearchCase::IgnoreCase))
        {
            DepartmentNames.Add(TEXT("Defense"));
        }
        else if (AdvisorName.Equals(TEXT("Social Advisor"), ESearchCase::IgnoreCase))
        {
            DepartmentNames.Add(TEXT("Health"));
            DepartmentNames.Add(TEXT("Education"));
        }
        else if (AdvisorName.Equals(TEXT("Economic Advisor"), ESearchCase::IgnoreCase))
        {
            DepartmentNames.Add(TEXT("Treasury"));
        }
        else if (AdvisorName.Equals(TEXT("Diplomacy Advisor"), ESearchCase::IgnoreCase))
        {
            DepartmentNames.Add(TEXT("Foreign Affairs"));
        }
        else if (AdvisorName.Equals(TEXT("Infrastructure Advisor"), ESearchCase::IgnoreCase))
        {
            DepartmentNames.Add(TEXT("Infrastructure"));
        }
        else if (AdvisorName.Equals(TEXT("Security Advisor"), ESearchCase::IgnoreCase))
        {
            DepartmentNames.Add(TEXT("Justice"));
        }
        else if (AdvisorName.Equals(TEXT("Public Welfare Advisor"), ESearchCase::IgnoreCase))
        {
            DepartmentNames.Add(TEXT("Health"));
        }
        return DepartmentNames;
    }

    void ApplyAdvisorMeetingDepartmentConsequences(FDemocracySimulationState& State, const FDemocracyMeetingRecordState& Record)
    {
        if (!Record.MeetingType.Equals(TEXT("Advisor"), ESearchCase::IgnoreCase))
        {
            return;
        }

        InitializeDefaultDepartments(State);
        const bool bEmergency = Record.AgendaItem.Contains(TEXT("Emergency"), ESearchCase::IgnoreCase)
            || Record.AgendaItem.Contains(TEXT("Crisis"), ESearchCase::IgnoreCase)
            || Record.AgendaItem.Contains(TEXT("Mobilization"), ESearchCase::IgnoreCase)
            || Record.AgendaItem.Contains(TEXT("Surge"), ESearchCase::IgnoreCase)
            || Record.AgendaItem.Contains(TEXT("Counter Threat"), ESearchCase::IgnoreCase);
        const bool bFocused = Record.AgendaItem.Contains(TEXT("Plan"), ESearchCase::IgnoreCase)
            || Record.AgendaItem.Contains(TEXT("Package"), ESearchCase::IgnoreCase)
            || Record.AgendaItem.Contains(TEXT("Program"), ESearchCase::IgnoreCase)
            || Record.AgendaItem.Contains(TEXT("Outreach"), ESearchCase::IgnoreCase)
            || Record.AgendaItem.Contains(TEXT("Upgrade"), ESearchCase::IgnoreCase)
            || Record.AgendaItem.Contains(TEXT("Expansion"), ESearchCase::IgnoreCase);

        const int32 PriorityDelta = bEmergency ? 7 : (bFocused ? 4 : 2);
        const int32 EffectivenessDelta = bEmergency ? 4 : (bFocused ? 3 : 1);
        const int32 TrustDelta = FMath::Clamp((Record.ApprovalDelta + Record.StabilityDelta - FMath::Max(0, Record.UnrestDelta)) / 2, -4, 5);
        const FString ConsequenceText = BuildMeetingDirectImpactText(Record);

        for (const FString& DepartmentName : GetAdvisorMeetingDepartmentNames(Record.ParticipantName, Record.AgendaItem))
        {
            FDemocracyDepartmentState* Department = FindDepartment(State.Departments, DepartmentName);
            if (!Department)
            {
                continue;
            }

            Department->Priority = FMath::Clamp(Department->Priority + PriorityDelta, 0, 100);
            Department->Effectiveness = FMath::Clamp(Department->Effectiveness + EffectivenessDelta, 0, 100);
            Department->PublicTrust = FMath::Clamp(Department->PublicTrust + TrustDelta, 0, 100);
            Department->CurrentAction = Record.AgendaItem;
            Department->AdvisorySummary = FString::Printf(TEXT("%s meeting: %s"), *Record.ParticipantName, *Record.OutcomeSummary);
            Department->ActionEffects.Insert(FString::Printf(TEXT("Turn %d advisor consequence: %s"), Record.Turn, *ConsequenceText), 0);
            while (Department->ActionEffects.Num() > 6)
            {
                Department->ActionEffects.RemoveAt(Department->ActionEffects.Num() - 1);
            }
        }
    }
    struct FPolicyRuleEvaluation
    {
        bool bCanSelect = true;
        FString Reason = TEXT("Available.");
        int32 TreasuryCost = 0;
        int32 FoodCost = 0;
        int32 WaterCost = 0;
        int32 GasOilCost = 0;
        int32 WoodCost = 0;
        int32 MetalsCost = 0;
    };

    FString GetSelectedPolicyForCategory(const FDemocracyPolicyState& Policies, const FString& Category)
    {
        if (Category.Equals(TEXT("Economic"), ESearchCase::IgnoreCase)) { return Policies.EconomicPolicy; }
        if (Category.Equals(TEXT("Environmental"), ESearchCase::IgnoreCase)) { return Policies.EnvironmentalPolicy; }
        if (Category.Equals(TEXT("Military"), ESearchCase::IgnoreCase)) { return Policies.MilitaryPolicy; }
        if (Category.Equals(TEXT("Diplomacy"), ESearchCase::IgnoreCase)) { return Policies.DiplomacyPolicy; }
        if (Category.Equals(TEXT("Civil"), ESearchCase::IgnoreCase)) { return Policies.CivilPolicy; }
        return TEXT("");
    }

    void SetSelectedPolicyForCategory(FDemocracyPolicyState& Policies, const FString& Category, const FString& PolicyName)
    {
        if (Category.Equals(TEXT("Economic"), ESearchCase::IgnoreCase)) { Policies.EconomicPolicy = PolicyName; }
        else if (Category.Equals(TEXT("Environmental"), ESearchCase::IgnoreCase)) { Policies.EnvironmentalPolicy = PolicyName; }
        else if (Category.Equals(TEXT("Military"), ESearchCase::IgnoreCase)) { Policies.MilitaryPolicy = PolicyName; }
        else if (Category.Equals(TEXT("Diplomacy"), ESearchCase::IgnoreCase)) { Policies.DiplomacyPolicy = PolicyName; }
        else if (Category.Equals(TEXT("Civil"), ESearchCase::IgnoreCase)) { Policies.CivilPolicy = PolicyName; }
    }

    int32 GetPolicyCategoryLastTurn(const FDemocracyPolicyState& Policies, const FString& Category)
    {
        if (Category.Equals(TEXT("Economic"), ESearchCase::IgnoreCase)) { return Policies.LastEconomicPolicyTurn; }
        if (Category.Equals(TEXT("Environmental"), ESearchCase::IgnoreCase)) { return Policies.LastEnvironmentalPolicyTurn; }
        if (Category.Equals(TEXT("Military"), ESearchCase::IgnoreCase)) { return Policies.LastMilitaryPolicyTurn; }
        if (Category.Equals(TEXT("Diplomacy"), ESearchCase::IgnoreCase)) { return Policies.LastDiplomacyPolicyTurn; }
        if (Category.Equals(TEXT("Civil"), ESearchCase::IgnoreCase)) { return Policies.LastCivilPolicyTurn; }
        return -100;
    }

    void SetPolicyCategoryLastTurn(FDemocracyPolicyState& Policies, const FString& Category, int32 Turn)
    {
        if (Category.Equals(TEXT("Economic"), ESearchCase::IgnoreCase)) { Policies.LastEconomicPolicyTurn = Turn; }
        else if (Category.Equals(TEXT("Environmental"), ESearchCase::IgnoreCase)) { Policies.LastEnvironmentalPolicyTurn = Turn; }
        else if (Category.Equals(TEXT("Military"), ESearchCase::IgnoreCase)) { Policies.LastMilitaryPolicyTurn = Turn; }
        else if (Category.Equals(TEXT("Diplomacy"), ESearchCase::IgnoreCase)) { Policies.LastDiplomacyPolicyTurn = Turn; }
        else if (Category.Equals(TEXT("Civil"), ESearchCase::IgnoreCase)) { Policies.LastCivilPolicyTurn = Turn; }
    }

    void AddPolicyRuleBlock(FPolicyRuleEvaluation& Evaluation, const FString& Reason)
    {
        Evaluation.bCanSelect = false;
        Evaluation.Reason = Reason;
    }

    FString BuildPolicyCostText(const FPolicyRuleEvaluation& Evaluation)
    {
        TArray<FString> Costs;
        AddDeltaLine(Costs, TEXT("Treasury cost"), -Evaluation.TreasuryCost);
        AddDeltaLine(Costs, TEXT("Food cost"), -Evaluation.FoodCost);
        AddDeltaLine(Costs, TEXT("Water cost"), -Evaluation.WaterCost);
        AddDeltaLine(Costs, TEXT("Fuel cost"), -Evaluation.GasOilCost);
        AddDeltaLine(Costs, TEXT("Wood cost"), -Evaluation.WoodCost);
        AddDeltaLine(Costs, TEXT("Metals cost"), -Evaluation.MetalsCost);
        return Costs.Num() > 0 ? FString::Join(Costs, TEXT(" | ")) : TEXT("No upfront cost.");
    }

    bool HasPolicyResourcesForCost(const FDemocracyCountryState& Country, const FPolicyRuleEvaluation& Evaluation, FString& OutReason)
    {
        if (Country.Treasury < Evaluation.TreasuryCost) { OutReason = FString::Printf(TEXT("Requires treasury %d, currently %d."), Evaluation.TreasuryCost, Country.Treasury); return false; }
        if (Country.Resources.Food < Evaluation.FoodCost) { OutReason = FString::Printf(TEXT("Requires food %d, currently %d."), Evaluation.FoodCost, Country.Resources.Food); return false; }
        if (Country.Resources.Water < Evaluation.WaterCost) { OutReason = FString::Printf(TEXT("Requires water %d, currently %d."), Evaluation.WaterCost, Country.Resources.Water); return false; }
        if (Country.Resources.GasOil < Evaluation.GasOilCost) { OutReason = FString::Printf(TEXT("Requires fuel %d, currently %d."), Evaluation.GasOilCost, Country.Resources.GasOil); return false; }
        if (Country.Resources.Wood < Evaluation.WoodCost) { OutReason = FString::Printf(TEXT("Requires wood %d, currently %d."), Evaluation.WoodCost, Country.Resources.Wood); return false; }
        if (Country.Resources.Metals < Evaluation.MetalsCost) { OutReason = FString::Printf(TEXT("Requires metals %d, currently %d."), Evaluation.MetalsCost, Country.Resources.Metals); return false; }
        return true;
    }

    FPolicyRuleEvaluation EvaluatePolicyRules(const FDemocracySimulationState& State, const FString& Category, const FString& PolicyName)
    {
        FPolicyRuleEvaluation Evaluation;
        const FDemocracyCountryState& Country = State.PlayerCountry;
        const FDemocracyPolicyState& Policies = Country.Policies;
        const FString CurrentPolicy = GetSelectedPolicyForCategory(Policies, Category);
        if (CurrentPolicy.Equals(PolicyName, ESearchCase::IgnoreCase))
        {
            AddPolicyRuleBlock(Evaluation, TEXT("Already active."));
            return Evaluation;
        }

        const int32 CooldownTurns = FMath::Max(0, Policies.PolicyCooldownTurns);
        const int32 TurnsSinceChange = State.Turn - GetPolicyCategoryLastTurn(Policies, Category);
        if (CooldownTurns > 0 && TurnsSinceChange < CooldownTurns)
        {
            AddPolicyRuleBlock(Evaluation, FString::Printf(TEXT("%s policy cooldown: wait %d more turn(s)."), *Category, CooldownTurns - TurnsSinceChange));
            return Evaluation;
        }

        if (PolicyName.Equals(TEXT("Stimulus Spending"), ESearchCase::IgnoreCase))
        {
            Evaluation.TreasuryCost = 80;
            if (Country.EconomicHealth > 78) { AddPolicyRuleBlock(Evaluation, TEXT("Prerequisite not met: stimulus requires economic health at 78 or lower.")); }
        }
        else if (PolicyName.Equals(TEXT("Austerity Program"), ESearchCase::IgnoreCase))
        {
            if (Country.PublicApproval < 30 || Country.Unrest > 70) { AddPolicyRuleBlock(Evaluation, TEXT("Prerequisite not met: austerity is locked while approval is below 30 or unrest is above 70.")); }
        }
        else if (PolicyName.Equals(TEXT("Industrial Subsidies"), ESearchCase::IgnoreCase))
        {
            Evaluation.TreasuryCost = 55;
            Evaluation.MetalsCost = 12;
            if (Country.Infrastructure < 35) { AddPolicyRuleBlock(Evaluation, TEXT("Prerequisite not met: industrial subsidies require infrastructure 35+.")); }
        }
        else if (PolicyName.Equals(TEXT("Conservation Mandate"), ESearchCase::IgnoreCase))
        {
            Evaluation.TreasuryCost = 35;
            if (Country.EnvironmentalHealth < 30) { AddPolicyRuleBlock(Evaluation, TEXT("Prerequisite not met: environmental health is too damaged for a stable conservation mandate.")); }
        }
        else if (PolicyName.Equals(TEXT("Extraction Expansion"), ESearchCase::IgnoreCase))
        {
            Evaluation.TreasuryCost = 20;
            if (Country.Unrest > 75) { AddPolicyRuleBlock(Evaluation, TEXT("Prerequisite not met: extraction expansion is locked while unrest is above 75.")); }
        }
        else if (PolicyName.Equals(TEXT("National Mobilization"), ESearchCase::IgnoreCase))
        {
            Evaluation.TreasuryCost = 75;
            Evaluation.GasOilCost = 10;
            Evaluation.MetalsCost = 12;
            if (Policies.CivilPolicy.Equals(TEXT("Civil Liberties"), ESearchCase::IgnoreCase)) { AddPolicyRuleBlock(Evaluation, TEXT("Conflict: National Mobilization cannot start while Civil Liberties is active.")); }
        }
        else if (PolicyName.Equals(TEXT("Demilitarization"), ESearchCase::IgnoreCase))
        {
            Evaluation.TreasuryCost = 15;
            if (Policies.DiplomacyPolicy.Equals(TEXT("Hardline Sovereignty"), ESearchCase::IgnoreCase)) { AddPolicyRuleBlock(Evaluation, TEXT("Conflict: Demilitarization conflicts with Hardline Sovereignty.")); }
            else if (State.InvasionRisk.CurrentInvasionRisk > 60) { AddPolicyRuleBlock(Evaluation, TEXT("Prerequisite not met: takeover risk must be 60 or lower before demilitarization.")); }
        }
        else if (PolicyName.Equals(TEXT("Alliance Outreach"), ESearchCase::IgnoreCase))
        {
            Evaluation.TreasuryCost = 45;
            if (Policies.CivilPolicy.Equals(TEXT("Emergency Powers"), ESearchCase::IgnoreCase)) { AddPolicyRuleBlock(Evaluation, TEXT("Conflict: Alliance Outreach is blocked while Emergency Powers is active.")); }
            else if (Country.DiplomaticStanding < 30) { AddPolicyRuleBlock(Evaluation, TEXT("Prerequisite not met: diplomatic standing must be 30+.")); }
        }
        else if (PolicyName.Equals(TEXT("Hardline Sovereignty"), ESearchCase::IgnoreCase))
        {
            Evaluation.TreasuryCost = 20;
            if (Policies.MilitaryPolicy.Equals(TEXT("Demilitarization"), ESearchCase::IgnoreCase)) { AddPolicyRuleBlock(Evaluation, TEXT("Conflict: Hardline Sovereignty conflicts with Demilitarization.")); }
            else if (Country.MilitaryReadiness < 35) { AddPolicyRuleBlock(Evaluation, TEXT("Prerequisite not met: military readiness must be 35+.")); }
        }
        else if (PolicyName.Equals(TEXT("Civil Liberties"), ESearchCase::IgnoreCase))
        {
            Evaluation.TreasuryCost = 25;
            if (Policies.MilitaryPolicy.Equals(TEXT("National Mobilization"), ESearchCase::IgnoreCase)) { AddPolicyRuleBlock(Evaluation, TEXT("Conflict: Civil Liberties cannot be restored while National Mobilization is active.")); }
            else if (Country.Stability < 35) { AddPolicyRuleBlock(Evaluation, TEXT("Prerequisite not met: stability must be 35+ before expanding civil liberties.")); }
        }
        else if (PolicyName.Equals(TEXT("Emergency Powers"), ESearchCase::IgnoreCase))
        {
            Evaluation.TreasuryCost = 40;
            if (Policies.DiplomacyPolicy.Equals(TEXT("Alliance Outreach"), ESearchCase::IgnoreCase)) { AddPolicyRuleBlock(Evaluation, TEXT("Conflict: Emergency Powers conflicts with active Alliance Outreach.")); }
            else if (Country.Unrest < 35 && Country.Stability > 45 && State.FailureRisk.CurrentAssassinationRisk < 20) { AddPolicyRuleBlock(Evaluation, TEXT("Prerequisite not met: emergency powers require unrest 35+, stability 45 or lower, or assassination risk 20+.")); }
        }

        if (Evaluation.bCanSelect)
        {
            FString CostFailure;
            if (!HasPolicyResourcesForCost(Country, Evaluation, CostFailure))
            {
                AddPolicyRuleBlock(Evaluation, CostFailure);
            }
        }
        if (Evaluation.bCanSelect)
        {
            Evaluation.Reason = FString::Printf(TEXT("Available. %s"), *BuildPolicyCostText(Evaluation));
        }
        return Evaluation;
    }

    void ApplyPolicySelectionCost(FDemocracySimulationState& State, const FPolicyRuleEvaluation& Evaluation)
    {
        FDemocracyCountryState& Country = State.PlayerCountry;
        Country.Treasury = FMath::Max(0, Country.Treasury - Evaluation.TreasuryCost);
        Country.Resources.Food = FMath::Max(0, Country.Resources.Food - Evaluation.FoodCost);
        Country.Resources.Water = FMath::Max(0, Country.Resources.Water - Evaluation.WaterCost);
        Country.Resources.GasOil = FMath::Max(0, Country.Resources.GasOil - Evaluation.GasOilCost);
        Country.Resources.Wood = FMath::Max(0, Country.Resources.Wood - Evaluation.WoodCost);
        Country.Resources.Metals = FMath::Max(0, Country.Resources.Metals - Evaluation.MetalsCost);
    }

    TArray<FString> BuildPolicyRuleStatusLines(const FDemocracySimulationState& State)
    {
        static const TArray<TPair<FString, FString>> Options = {
            { TEXT("Economic"), TEXT("Balanced Budget") }, { TEXT("Economic"), TEXT("Stimulus Spending") }, { TEXT("Economic"), TEXT("Austerity Program") }, { TEXT("Economic"), TEXT("Industrial Subsidies") },
            { TEXT("Environmental"), TEXT("Managed Development") }, { TEXT("Environmental"), TEXT("Conservation Mandate") }, { TEXT("Environmental"), TEXT("Extraction Expansion") },
            { TEXT("Military"), TEXT("Defensive Readiness") }, { TEXT("Military"), TEXT("National Mobilization") }, { TEXT("Military"), TEXT("Demilitarization") },
            { TEXT("Diplomacy"), TEXT("Neutral Engagement") }, { TEXT("Diplomacy"), TEXT("Alliance Outreach") }, { TEXT("Diplomacy"), TEXT("Hardline Sovereignty") },
            { TEXT("Civil"), TEXT("Public Stability") }, { TEXT("Civil"), TEXT("Civil Liberties") }, { TEXT("Civil"), TEXT("Emergency Powers") }
        };

        TArray<FString> Lines;
        const FDemocracyPolicyState& Policies = State.PlayerCountry.Policies;
        Lines.Add(FString::Printf(TEXT("Cooldown %d turn(s) per category. Last changes: Economic %d | Environment %d | Military %d | Diplomacy %d | Civil %d."), Policies.PolicyCooldownTurns, Policies.LastEconomicPolicyTurn, Policies.LastEnvironmentalPolicyTurn, Policies.LastMilitaryPolicyTurn, Policies.LastDiplomacyPolicyTurn, Policies.LastCivilPolicyTurn));
        for (const TPair<FString, FString>& Option : Options)
        {
            const FPolicyRuleEvaluation Evaluation = EvaluatePolicyRules(State, Option.Key, Option.Value);
            const FString Status = Evaluation.bCanSelect ? TEXT("Available") : (Evaluation.Reason.Equals(TEXT("Already active."), ESearchCase::IgnoreCase) ? TEXT("Active") : TEXT("Locked"));
            Lines.Add(FString::Printf(TEXT("%s / %s: %s - %s"), *Option.Key, *Option.Value, *Status, *Evaluation.Reason));
        }
        return Lines;
    }

    void RefreshPolicyRules(FDemocracySimulationState& State)
    {
        if (State.PlayerCountry.Policies.PolicyCooldownTurns <= 0)
        {
            State.PlayerCountry.Policies.PolicyCooldownTurns = 2;
        }
        State.PlayerCountry.Policies.PolicyRuleStatus = BuildPolicyRuleStatusLines(State);
    }
    FString BuildEventFollowUpPreview(const FDemocracyActiveEventState& Event)
    {
        if (Event.EventType.Equals(TEXT("Shortage"), ESearchCase::IgnoreCase))
        {
            return TEXT("Missed follow-up: shortages spread, food and water reserves fall, unrest and assassination risk rise.");
        }
        if (Event.EventType.Equals(TEXT("Protest"), ESearchCase::IgnoreCase))
        {
            return TEXT("Missed follow-up: protests harden into broader unrest, stability falls, assassination risk rises.");
        }
        if (Event.EventType.Equals(TEXT("Border Tension"), ESearchCase::IgnoreCase))
        {
            return TEXT("Missed follow-up: rivals read inaction as weakness, diplomacy falls, invasion risk rises.");
        }
        if (Event.EventType.Equals(TEXT("Economic Shock"), ESearchCase::IgnoreCase))
        {
            return TEXT("Missed follow-up: markets deteriorate, treasury and economic health fall, public approval weakens.");
        }
        if (Event.EventType.Equals(TEXT("Disaster"), ESearchCase::IgnoreCase))
        {
            return TEXT("Missed follow-up: damaged infrastructure worsens, supplies are lost, unrest rises.");
        }
        if (Event.EventType.Equals(TEXT("Scandal"), ESearchCase::IgnoreCase))
        {
            return TEXT("Missed follow-up: public trust and press credibility fall, unrest and internal risk rise.");
        }
        return TEXT("Missed follow-up: the event escalates and applies negative stability, unrest, approval, and risk effects.");
    }
    void ConfigureEventResolutionLoop(FDemocracyActiveEventState& Event)
    {
        Event.CompletionState = Event.bResolved ? Event.CompletionState : TEXT("Active");
        Event.UnresolvedPenaltySummary = BuildEventFollowUpPreview(Event);
        if (Event.EventType.Contains(TEXT("Follow-up"), ESearchCase::IgnoreCase))
        {
            Event.FollowUpEventType.Empty();
            Event.FollowUpTitle.Empty();
            Event.FollowUpDescription.Empty();
            Event.FollowUpSeverityDelta = 0;
            return;
        }

        Event.FollowUpEventType = Event.EventType + TEXT(" Follow-up");
        Event.FollowUpTitle = FString::Printf(TEXT("Escalated %s"), *Event.Title);
        Event.FollowUpDescription = FString::Printf(TEXT("The unresolved %s has escalated. Advisors now need a stronger response before it causes wider damage."), *Event.EventType);
        Event.FollowUpSeverityDelta = 12;
    }
    FString BuildGuidedEventHeaderText(const FDemocracySimulationState& State, const FDemocracyActiveEventState& Event)
    {
        const FString GuidanceLevel = GetRuntimeGuidanceLevel(State);
        return GuidanceText(GuidanceLevel,
            FString::Printf(TEXT("%s | Severity %d | State: %s | Trigger: %s\n%s\n%s\nUnresolved penalty: %s\nFollow-up event: %s\nTutorial cue: act before the deadline when severity is high or the follow-up touches failure risk."), *Event.EventType, Event.Severity, *Event.CompletionState, *Event.TriggerReason, *Event.Description, *BuildEventDeadlineText(State, Event), *(Event.UnresolvedPenaltySummary.IsEmpty() ? BuildEventFollowUpPreview(Event) : Event.UnresolvedPenaltySummary), *(Event.FollowUpTitle.IsEmpty() ? TEXT("No chained follow-up; penalty only.") : Event.FollowUpTitle)),
            FString::Printf(TEXT("%s | Severity %d | State: %s | %s\n%s\n%s\nUnresolved penalty: %s\nFollow-up: %s"), *Event.EventType, Event.Severity, *Event.CompletionState, *Event.TriggerReason, *Event.Description, *BuildEventDeadlineText(State, Event), *(Event.UnresolvedPenaltySummary.IsEmpty() ? BuildEventFollowUpPreview(Event) : Event.UnresolvedPenaltySummary), *(Event.FollowUpTitle.IsEmpty() ? TEXT("Penalty only") : Event.FollowUpTitle)),
            FString::Printf(TEXT("%s | Severity %d\n%s\nConsequence preview is limited by difficulty guidance."), *Event.EventType, Event.Severity, *BuildEventDeadlineText(State, Event)),
            FString::Printf(TEXT("%s | Severity %d | %s"), *Event.EventType, Event.Severity, *BuildEventDeadlineText(State, Event)));
    }

    FString BuildGuidedEventChoiceText(const FDemocracySimulationState& State, const FDemocracyEventChoiceState& Choice)
    {
        const FString GuidanceLevel = GetRuntimeGuidanceLevel(State);
        return GuidanceText(GuidanceLevel,
            FString::Printf(TEXT("%s\nPreview: %s\nDirect effects: %s\nTutorial cue: compare the direct effects against the current warning category."), *Choice.Description, *Choice.ConsequencePreview, *BuildEventChoiceImpactText(Choice)),
            FString::Printf(TEXT("%s\nPreview: %s\nDirect effects: %s"), *Choice.Description, *Choice.ConsequencePreview, *BuildEventChoiceImpactText(Choice)),
            FString::Printf(TEXT("%s\nBroad preview: %s"), *Choice.Description, *Choice.ConsequencePreview),
            TEXT("Choice consequences hidden by difficulty guidance."));
    }

    void ApplyEventChoiceDeltas(FDemocracySimulationState& State, const FDemocracyEventChoiceState& Choice)
    {
        FDemocracyCountryState& Country = State.PlayerCountry;
        FDemocracyResourceInventory& Resources = Country.Resources;
        Country.PublicApproval = FMath::Clamp(Country.PublicApproval + Choice.ApprovalDelta, 0, 100);
        Country.Stability = FMath::Clamp(Country.Stability + Choice.StabilityDelta, 0, 100);
        Country.Unrest = FMath::Clamp(Country.Unrest + Choice.UnrestDelta, 0, 100);
        Country.Treasury = FMath::Max(0, Country.Treasury + Choice.TreasuryDelta);
        Country.EconomicHealth = FMath::Clamp(Country.EconomicHealth + Choice.EconomicDelta, 0, 100);
        Country.DiplomaticStanding = FMath::Clamp(Country.DiplomaticStanding + Choice.DiplomacyDelta, 0, 100);
        Country.MilitaryReadiness = FMath::Clamp(Country.MilitaryReadiness + Choice.MilitaryDelta, 0, 100);
        Country.Infrastructure = FMath::Clamp(Country.Infrastructure + Choice.InfrastructureDelta, 0, 100);
        Country.EnvironmentalHealth = FMath::Clamp(Country.EnvironmentalHealth + Choice.EnvironmentDelta, 0, 100);
        Resources.Food = FMath::Max(0, Resources.Food + Choice.FoodDelta);
        Resources.Water = FMath::Max(0, Resources.Water + Choice.WaterDelta);
        Resources.GasOil = FMath::Max(0, Resources.GasOil + Choice.GasOilDelta);
        Resources.Wood = FMath::Max(0, Resources.Wood + Choice.WoodDelta);
        Resources.Metals = FMath::Max(0, Resources.Metals + Choice.MetalsDelta);
        State.FailureRisk.CurrentAssassinationRisk = FMath::Clamp(State.FailureRisk.CurrentAssassinationRisk + Choice.AssassinationRiskDelta, 0, State.FailureRisk.AssassinationRiskTrigger);
        State.InvasionRisk.CurrentInvasionRisk = FMath::Clamp(State.InvasionRisk.CurrentInvasionRisk + Choice.InvasionRiskDelta, 0, State.InvasionRisk.InvasionRiskTrigger);
    }

    void RecalculateDemographics(FDemocracySimulationState& State);
    void RecalculateEconomyBudget(FDemocracySimulationState& State);

    void RebuildSimulationAfterEventChange(FDemocracySimulationState& State)
    {
        RefreshResourceChainsFromCurrentCadence(State);
        RecalculateEconomyBudget(State);
        RecalculateDemographics(State);
        RecalculateApprovalStability(State);
        RecalculateDepartments(State);
        State.AdvisorSystem.GuidanceLevel = AdvisorGuidanceForDifficultyScore(State.PlayerCountry.CountrySizeScore);
        State.AdvisorSystem.LastUpdatedTurn = State.Turn;
        State.AdvisorSystem.Reports = GenerateAdvisorReports(State);
    }

    bool ApplyExpiredEventFollowUps(FDemocracySimulationState& State)
    {
        bool bAppliedAny = false;
        TArray<FDemocracyActiveEventState> FollowUpEvents;
        for (FDemocracyActiveEventState& Event : State.EventSystem.ActiveEvents)
        {
            if (Event.bResolved || Event.DeadlineTurn <= 0 || State.Turn < Event.DeadlineTurn)
            {
                continue;
            }

            const int32 SeverityPressure = FMath::Clamp(Event.Severity / 12, 1, 10);
            FDemocracyEventChoiceState Expired;
            Expired.ChoiceId = TEXT("expired");
            Expired.Label = TEXT("Deadline missed");
            Expired.Description = TEXT("No response was selected before the deadline.");
            Expired.ConsequencePreview = Event.UnresolvedPenaltySummary.IsEmpty() ? TEXT("The unresolved crisis damaged approval, stability, and risk controls.") : Event.UnresolvedPenaltySummary;
            Expired.ApprovalDelta = -FMath::Clamp(SeverityPressure + 2, 3, 12);
            Expired.StabilityDelta = -FMath::Clamp(SeverityPressure + 1, 2, 10);
            Expired.UnrestDelta = FMath::Clamp(SeverityPressure + 2, 3, 12);
            Expired.TreasuryDelta = -FMath::Clamp(Event.Severity * 2, 30, 180);
            Expired.EconomicDelta = -FMath::Clamp(SeverityPressure, 1, 8);
            Expired.DiplomacyDelta = Event.EventType.Contains(TEXT("Border"), ESearchCase::IgnoreCase) ? -FMath::Clamp(SeverityPressure, 2, 8) : 0;
            Expired.MilitaryDelta = Event.EventType.Contains(TEXT("Border"), ESearchCase::IgnoreCase) ? -FMath::Clamp(SeverityPressure, 2, 8) : 0;
            Expired.InfrastructureDelta = Event.EventType.Contains(TEXT("Disaster"), ESearchCase::IgnoreCase) ? -FMath::Clamp(SeverityPressure, 2, 8) : 0;
            Expired.EnvironmentDelta = Event.EventType.Contains(TEXT("Disaster"), ESearchCase::IgnoreCase) ? -FMath::Clamp(SeverityPressure, 1, 6) : 0;
            Expired.FoodDelta = Event.EventType.Contains(TEXT("Shortage"), ESearchCase::IgnoreCase) ? -FMath::Clamp(Event.Severity / 2, 10, 70) : 0;
            Expired.WaterDelta = Event.EventType.Contains(TEXT("Shortage"), ESearchCase::IgnoreCase) || Event.EventType.Contains(TEXT("Disaster"), ESearchCase::IgnoreCase) ? -FMath::Clamp(Event.Severity / 3, 8, 55) : 0;
            Expired.GasOilDelta = Event.EventType.Contains(TEXT("Fuel"), ESearchCase::IgnoreCase) ? -FMath::Clamp(Event.Severity / 2, 10, 70) : 0;
            Expired.WoodDelta = Event.EventType.Contains(TEXT("Material"), ESearchCase::IgnoreCase) || Event.EventType.Contains(TEXT("Disaster"), ESearchCase::IgnoreCase) ? -FMath::Clamp(Event.Severity / 3, 8, 55) : 0;
            Expired.MetalsDelta = Event.EventType.Contains(TEXT("Material"), ESearchCase::IgnoreCase) || Event.EventType.Contains(TEXT("Border"), ESearchCase::IgnoreCase) ? -FMath::Clamp(Event.Severity / 3, 8, 55) : 0;
            Expired.AssassinationRiskDelta = Event.EventType.Contains(TEXT("Protest"), ESearchCase::IgnoreCase) || Event.EventType.Contains(TEXT("Scandal"), ESearchCase::IgnoreCase) ? FMath::Clamp(SeverityPressure + 2, 3, 10) : FMath::Clamp(SeverityPressure / 2, 1, 5);
            Expired.InvasionRiskDelta = Event.EventType.Contains(TEXT("Border"), ESearchCase::IgnoreCase) ? FMath::Clamp(SeverityPressure + 3, 4, 12) : FMath::Clamp(SeverityPressure / 2, 0, 4);

            if (Event.EventType.Contains(TEXT("Scandal"), ESearchCase::IgnoreCase))
            {
                State.PressOffice.Credibility = FMath::Clamp(State.PressOffice.Credibility - FMath::Clamp(SeverityPressure + 3, 6, 14), 0, 100);
                State.PressOffice.LastAnnouncementSummary = TEXT("Press credibility fell because a scandal was left unanswered.");
            }

            ApplyEventChoiceDeltas(State, Expired);
            Event.bResolved = true;
            Event.CompletionState = TEXT("Expired - Penalty Applied");
            Event.SelectedChoiceId = Expired.ChoiceId;
            Event.ResolutionSummary = FString::Printf(TEXT("%s expired on turn %d after missing deadline turn %d. %s Direct effects: %s"), *Event.Title, State.Turn, Event.DeadlineTurn, *Expired.ConsequencePreview, *BuildEventChoiceImpactText(Expired));
            State.EventSystem.EventHistory.Add(FString::Printf(TEXT("Turn %d: %s"), State.Turn, *Event.ResolutionSummary));
            LogDecision(State, TEXT("Event Deadline"), Event.Title, TEXT("No choice selected before the deadline."), Event.ResolutionSummary, Event.Severity, { TEXT("event"), Event.EventType, TEXT("deadline"), Event.CompletionState });

            if (!Event.FollowUpEventType.IsEmpty())
            {
                FDemocracyActiveEventState FollowUp = MakeEvent(State,
                    Event.FollowUpEventType,
                    Event.FollowUpTitle.IsEmpty() ? FString::Printf(TEXT("Escalated %s"), *Event.Title) : Event.FollowUpTitle,
                    Event.FollowUpDescription.IsEmpty() ? FString::Printf(TEXT("The unresolved %s requires a stronger second response."), *Event.EventType) : Event.FollowUpDescription,
                    FString::Printf(TEXT("Follow-up created because %s missed its deadline."), *Event.Title),
                    FMath::Clamp(Event.Severity + Event.FollowUpSeverityDelta, 1, 100),
                    true,
                    {
                        MakeEventChoice(TEXT("contain"), TEXT("Contain escalation"), TEXT("Fund a fast containment response to prevent wider damage."), TEXT("Costs treasury but reduces unrest and risk."), 2, 3, -5, -100, 1, 1, 1, 1, 0, 15, 10, 0, 5, 5, -4, -3),
                        MakeEventChoice(TEXT("targeted"), TEXT("Target root cause"), TEXT("Use a focused response aimed at the specific driver of the escalation."), TEXT("Moderate cost with balanced recovery effects."), 3, 2, -4, -70, 2, 2, 0, 2, 1, 10, 8, 5, 5, 5, -3, -2),
                        MakeEventChoice(TEXT("delay"), TEXT("Delay again"), TEXT("Keep resources available and accept the risk of further damage."), TEXT("Treasury protected, but approval, stability, unrest, and risks worsen."), -6, -4, 7, 0, -2, -1, 0, -1, -1, -10, -8, 0, 0, 0, 5, 4)
                    });
                FollowUp.CompletionState = TEXT("Follow-up Active");
                FollowUpEvents.Add(FollowUp);
                State.EventSystem.EventHistory.Add(FString::Printf(TEXT("Turn %d: Follow-up opened: %s."), State.Turn, *FollowUp.Title));
            }

            bAppliedAny = true;
        }

        if (bAppliedAny)
        {
            State.Phase = TEXT("Event Deadline Escalated");
            RebuildSimulationAfterEventChange(State);
            PruneResolvedEvents(State.EventSystem);
            for (const FDemocracyActiveEventState& FollowUp : FollowUpEvents)
            {
                if (State.EventSystem.ActiveEvents.Num() < State.EventSystem.ActiveEventLimit && !HasActiveEventType(State.EventSystem, FollowUp.EventType))
                {
                    State.EventSystem.ActiveEvents.Add(FollowUp);
                    State.EventSystem.LastEventTurn = State.Turn;
                    State.Phase = TEXT("Event Follow-up Pending");
                }
            }
        }
        return bAppliedAny;
    }

    FString BuildEventSummaryText(const FDemocracyEventSystemState& EventSystem)
    {
        int32 PendingCount = 0;
        for (const FDemocracyActiveEventState& Event : EventSystem.ActiveEvents)
        {
            if (!Event.bResolved)
            {
                ++PendingCount;
            }
        }
        return FString::Printf(TEXT("Pending events: %d | last event turn: %d | history entries: %d"), PendingCount, EventSystem.LastEventTurn, EventSystem.EventHistory.Num());
    }

    void AddUniqueSource(TArray<FString>& Sources, const FString& Source)
    {
        if (!Sources.Contains(Source))
        {
            Sources.Add(Source);
        }
    }
    void RecalculateDemographics(FDemocracySimulationState& State)
    {
        FDemocracyCountryState& Country = State.PlayerCountry;
        FDemocracyDemographicsState& Demographics = State.Demographics;
        FDemocracyResourceInventory& Resources = Country.Resources;
        if (Demographics.CitizenGroups.Num() == 0 || Demographics.Regions.Num() == 0)
        {
            return;
        }

        const int32 FoodShortageAmount = FMath::Max(GetResourceChainShortage(State.ResourceChains, TEXT("Food")), FMath::Max(0, 120 - Resources.Food));
        const int32 WaterShortageAmount = FMath::Max(GetResourceChainShortage(State.ResourceChains, TEXT("Water")), FMath::Max(0, 100 - Resources.Water));
        const int32 FuelShortageAmount = FMath::Max(GetResourceChainShortage(State.ResourceChains, TEXT("Fuel")), FMath::Max(0, 60 - Resources.GasOil));
        const int32 WoodShortageAmount = FMath::Max(GetResourceChainShortage(State.ResourceChains, TEXT("Wood")), FMath::Max(0, 55 - Resources.Wood));
        const int32 MetalsShortageAmount = FMath::Max(GetResourceChainShortage(State.ResourceChains, TEXT("Metals")), FMath::Max(0, 55 - Resources.Metals));

        const bool bFoodShortage = FoodShortageAmount > 0;
        const bool bWaterShortage = WaterShortageAmount > 0;
        const bool bFuelShortage = FuelShortageAmount > 0;
        const bool bMaterialsShortage = WoodShortageAmount > 0 || MetalsShortageAmount > 0;
        const bool bWeakJobs = Country.EconomicHealth < 50 || State.EconomyBudget.ProductionEfficiency < 45;
        const bool bSecurityConcern = Country.Unrest > 45 || State.InvasionRisk.CurrentInvasionRisk > State.InvasionRisk.InvasionRiskTrigger / 3;
        const bool bPublicServicesWeak = State.EconomyBudget.PublicServices < 45 || State.EconomyBudget.bSpendingLimited;
        const bool bHighTaxBurden = State.EconomyBudget.TaxRate >= 32;
        const bool bInflationPressure = State.EconomyBudget.Inflation >= 9;

        bool bProtestCrisis = false;
        bool bBorderCrisis = false;
        bool bEconomicCrisis = false;
        bool bDisasterCrisis = false;
        bool bScandalCrisis = false;
        bool bShortageCrisis = false;
        int32 CrisisSeverity = 0;
        for (const FDemocracyActiveEventState& Event : State.EventSystem.ActiveEvents)
        {
            if (Event.bResolved)
            {
                continue;
            }
            CrisisSeverity = FMath::Max(CrisisSeverity, Event.Severity);
            bProtestCrisis |= Event.EventType.Contains(TEXT("Protest"), ESearchCase::IgnoreCase);
            bBorderCrisis |= Event.EventType.Contains(TEXT("Border"), ESearchCase::IgnoreCase) || Event.EventType.Contains(TEXT("Tension"), ESearchCase::IgnoreCase);
            bEconomicCrisis |= Event.EventType.Contains(TEXT("Economic"), ESearchCase::IgnoreCase) || Event.EventType.Contains(TEXT("Market"), ESearchCase::IgnoreCase);
            bDisasterCrisis |= Event.EventType.Contains(TEXT("Disaster"), ESearchCase::IgnoreCase) || Event.EventType.Contains(TEXT("Storm"), ESearchCase::IgnoreCase);
            bScandalCrisis |= Event.EventType.Contains(TEXT("Scandal"), ESearchCase::IgnoreCase) || Event.EventType.Contains(TEXT("Corruption"), ESearchCase::IgnoreCase);
            bShortageCrisis |= Event.EventType.Contains(TEXT("Shortage"), ESearchCase::IgnoreCase) || Event.EventType.Contains(TEXT("Supply"), ESearchCase::IgnoreCase);
        }

        const FDemocracyPressReleaseRecordState* LastPressRecord = State.PressOffice.Records.Num() > 0 ? &State.PressOffice.Records.Last() : nullptr;
        const bool bRecentPress = LastPressRecord && State.Turn - LastPressRecord->Turn <= 2;
        const bool bRecentGoodPress = bRecentPress && LastPressRecord->bTruthful && !LastPressRecord->MessageQuality.Equals(TEXT("Empty"), ESearchCase::IgnoreCase) && State.PressOffice.Credibility >= 45;
        const bool bRecentBadPress = bRecentPress && (!LastPressRecord->bTruthful || LastPressRecord->MessageQuality.Equals(TEXT("Empty"), ESearchCase::IgnoreCase) || State.PressOffice.Credibility < 35);

        const auto HasPolicy = [&Country](const FString& PolicyName) -> bool
        {
            return Country.Policies.EconomicPolicy.Equals(PolicyName, ESearchCase::IgnoreCase)
                || Country.Policies.EnvironmentalPolicy.Equals(PolicyName, ESearchCase::IgnoreCase)
                || Country.Policies.MilitaryPolicy.Equals(PolicyName, ESearchCase::IgnoreCase)
                || Country.Policies.DiplomacyPolicy.Equals(PolicyName, ESearchCase::IgnoreCase)
                || Country.Policies.CivilPolicy.Equals(PolicyName, ESearchCase::IgnoreCase);
        };

        int32 WeightedGroupApproval = 0;
        int32 WeightedRegionApproval = 0;
        int32 TotalGroupShare = 0;
        int32 TotalRegionShare = 0;
        int32 NeedsPressure = 0;
        int32 DemographicUnrest = 0;
        Demographics.NationalUnrestSources.Reset();

        for (FDemocracyCitizenGroupState& Group : Demographics.CitizenGroups)
        {
            Group.UnrestSources.Reset();
            const bool bUrban = Group.GroupName.Contains(TEXT("Urban"), ESearchCase::IgnoreCase);
            const bool bRural = Group.GroupName.Contains(TEXT("Rural"), ESearchCase::IgnoreCase);
            const bool bBusiness = Group.GroupName.Contains(TEXT("Business"), ESearchCase::IgnoreCase);
            const bool bPublicSector = Group.GroupName.Contains(TEXT("Public"), ESearchCase::IgnoreCase);
            const bool bYouth = Group.GroupName.Contains(TEXT("Youth"), ESearchCase::IgnoreCase) || Group.GroupName.Contains(TEXT("Students"), ESearchCase::IgnoreCase);
            const bool bRetirees = Group.GroupName.Contains(TEXT("Retirees"), ESearchCase::IgnoreCase);
            const bool bLowIncome = Group.GroupName.Contains(TEXT("Low-Income"), ESearchCase::IgnoreCase);
            const bool bSecurityFamilies = Group.GroupName.Contains(TEXT("Veterans"), ESearchCase::IgnoreCase) || Group.GroupName.Contains(TEXT("Security"), ESearchCase::IgnoreCase);

            int32 FoodNeedDelta = bFoodShortage ? 2 + FoodShortageAmount / 30 : -1;
            int32 WaterNeedDelta = bWaterShortage ? 2 + WaterShortageAmount / 28 : -1;
            int32 JobsNeedDelta = bWeakJobs ? 2 + FMath::Max(0, 50 - Country.EconomicHealth) / 12 : -1;
            int32 SecurityNeedDelta = bSecurityConcern ? 2 + Country.Unrest / 25 + State.InvasionRisk.CurrentInvasionRisk / 30 : -1;
            int32 HealthcareNeedDelta = Country.EnvironmentalHealth < 45 || bPublicServicesWeak ? 1 + FMath::Max(0, 48 - State.EconomyBudget.PublicServices) / 14 : 0;

            if (bLowIncome) { FoodNeedDelta += bFoodShortage ? 2 : 0; WaterNeedDelta += bWaterShortage ? 2 : 0; JobsNeedDelta += bInflationPressure ? 2 : 0; }
            if (bRural) { FoodNeedDelta += bFoodShortage ? 1 : -1; WaterNeedDelta += bDisasterCrisis ? 1 : 0; JobsNeedDelta += bMaterialsShortage ? 1 : 0; }
            if (bUrban) { JobsNeedDelta += bEconomicCrisis ? 2 : 0; SecurityNeedDelta += bProtestCrisis ? 2 : 0; }
            if (bBusiness) { JobsNeedDelta += bFuelShortage || bMaterialsShortage ? 2 : 0; }
            if (bPublicSector) { HealthcareNeedDelta += bPublicServicesWeak ? 2 : 0; }
            if (bYouth) { JobsNeedDelta += bWeakJobs ? 2 : 0; SecurityNeedDelta += HasPolicy(TEXT("Emergency Powers")) ? 2 : 0; }
            if (bRetirees) { HealthcareNeedDelta += bPublicServicesWeak ? 2 : 0; SecurityNeedDelta += bSecurityConcern ? 1 : 0; }
            if (bSecurityFamilies) { SecurityNeedDelta += bBorderCrisis ? 3 : 0; }

            Group.NeedFood = FMath::Clamp(Group.NeedFood + FoodNeedDelta, 0, 100);
            Group.NeedWater = FMath::Clamp(Group.NeedWater + WaterNeedDelta, 0, 100);
            Group.NeedJobs = FMath::Clamp(Group.NeedJobs + JobsNeedDelta, 0, 100);
            Group.NeedSecurity = FMath::Clamp(Group.NeedSecurity + SecurityNeedDelta, 0, 100);
            Group.NeedHealthcare = FMath::Clamp(Group.NeedHealthcare + HealthcareNeedDelta, 0, 100);

            int32 ApprovalDelta = 0;
            ApprovalDelta += Country.PublicApproval > 60 ? 1 : 0;
            ApprovalDelta -= bFoodShortage ? (bLowIncome ? 3 : 1) : 0;
            ApprovalDelta -= bWaterShortage ? (bLowIncome || bRural ? 3 : 1) : 0;
            ApprovalDelta -= bWeakJobs ? (bYouth || bUrban ? 2 : 1) : 0;
            ApprovalDelta -= bSecurityConcern ? (bSecurityFamilies || bRetirees ? 2 : 1) : 0;
            ApprovalDelta -= bInflationPressure ? (bLowIncome || bRetirees ? 2 : 1) : 0;
            ApprovalDelta -= bPublicServicesWeak ? (bPublicSector || bRetirees || bLowIncome ? 2 : 0) : 0;

            if (bBusiness)
            {
                ApprovalDelta += Country.EconomicHealth > 60 ? 2 : -1;
                ApprovalDelta += HasPolicy(TEXT("Industrial Subsidies")) ? 3 : 0;
                ApprovalDelta += HasPolicy(TEXT("Austerity Program")) ? 1 : 0;
                ApprovalDelta -= bHighTaxBurden ? 3 : 0;
                ApprovalDelta -= bFuelShortage || bMaterialsShortage ? 2 : 0;
            }
            if (bUrban || bYouth)
            {
                ApprovalDelta += HasPolicy(TEXT("Civil Liberties")) ? 2 : 0;
                ApprovalDelta += HasPolicy(TEXT("Stimulus Spending")) ? 1 : 0;
                ApprovalDelta -= HasPolicy(TEXT("Emergency Powers")) ? 3 : 0;
                ApprovalDelta -= bScandalCrisis ? 2 : 0;
            }
            if (bRural)
            {
                ApprovalDelta += Resources.Food > 160 ? 1 : 0;
                ApprovalDelta += HasPolicy(TEXT("Conservation Mandate")) ? 2 : 0;
                ApprovalDelta -= HasPolicy(TEXT("Extraction Expansion")) ? 2 : 0;
                ApprovalDelta -= Country.Infrastructure < 40 ? 2 : 0;
                ApprovalDelta -= bDisasterCrisis ? 2 : 0;
            }
            if (bPublicSector)
            {
                ApprovalDelta += State.EconomyBudget.PublicServices > 58 ? 2 : 0;
                ApprovalDelta += HasPolicy(TEXT("Civil Liberties")) ? 1 : 0;
                ApprovalDelta -= HasPolicy(TEXT("Austerity Program")) ? 3 : 0;
                ApprovalDelta -= bScandalCrisis ? 2 : 0;
            }
            if (bRetirees)
            {
                ApprovalDelta += Country.Stability > 60 ? 1 : 0;
                ApprovalDelta += State.EconomyBudget.PublicServices > 55 ? 1 : 0;
                ApprovalDelta -= Group.NeedHealthcare > 65 ? 2 : 0;
                ApprovalDelta -= HasPolicy(TEXT("National Mobilization")) && !bBorderCrisis ? 1 : 0;
            }
            if (bSecurityFamilies)
            {
                ApprovalDelta += HasPolicy(TEXT("National Mobilization")) ? 2 : 0;
                ApprovalDelta += Country.MilitaryReadiness > 62 ? 1 : 0;
                ApprovalDelta -= HasPolicy(TEXT("Demilitarization")) && bBorderCrisis ? 3 : 0;
                ApprovalDelta -= State.InvasionRisk.CurrentInvasionRisk > 55 ? 2 : 0;
            }

            if (bRecentGoodPress)
            {
                ApprovalDelta += LastPressRecord->AnnouncementType.Equals(TEXT("Crisis Reassurance"), ESearchCase::IgnoreCase) && (bRetirees || bPublicSector || bUrban) ? 2 : 1;
                ApprovalDelta += LastPressRecord->AnnouncementType.Equals(TEXT("Policy Explanation"), ESearchCase::IgnoreCase) && (bBusiness || bPublicSector || bYouth) ? 1 : 0;
                ApprovalDelta += LastPressRecord->AnnouncementType.Equals(TEXT("Diplomatic Address"), ESearchCase::IgnoreCase) && (bBusiness || bUrban) ? 1 : 0;
            }
            if (bRecentBadPress)
            {
                ApprovalDelta -= bYouth || bUrban || bBusiness ? 3 : 2;
                AddUniqueSource(Group.UnrestSources, TEXT("Press credibility"));
                AddUniqueSource(Demographics.NationalUnrestSources, TEXT("Press credibility"));
            }

            if (bProtestCrisis && (bUrban || bYouth || bPublicSector)) { ApprovalDelta -= 2; AddUniqueSource(Group.UnrestSources, TEXT("Protest momentum")); }
            if (bEconomicCrisis && (bBusiness || bUrban || bYouth || bLowIncome)) { ApprovalDelta -= 2; AddUniqueSource(Group.UnrestSources, TEXT("Economic shock")); }
            if (bDisasterCrisis && (bRural || bRetirees || bLowIncome)) { ApprovalDelta -= 2; AddUniqueSource(Group.UnrestSources, TEXT("Disaster recovery")); }
            if (bScandalCrisis && (bYouth || bPublicSector || bBusiness)) { ApprovalDelta -= 2; AddUniqueSource(Group.UnrestSources, TEXT("Institutional trust")); }
            if (bShortageCrisis) { AddUniqueSource(Group.UnrestSources, TEXT("Supply reliability")); }

            Group.Approval = FMath::Clamp(Group.Approval + ApprovalDelta, 0, 100);
            Group.UnrestPressure = FMath::Clamp((100 - Group.Approval) / 4
                + FMath::Max(0, Group.NeedFood - 60) / 4
                + FMath::Max(0, Group.NeedWater - 60) / 4
                + FMath::Max(0, Group.NeedJobs - 60) / 5
                + FMath::Max(0, Group.NeedSecurity - 60) / 5
                + FMath::Max(0, Group.NeedHealthcare - 60) / 6
                + CrisisSeverity / 18, 0, 100);

            if (bFoodShortage) { AddUniqueSource(Group.UnrestSources, TEXT("Food access")); AddUniqueSource(Demographics.NationalUnrestSources, TEXT("Food access")); }
            if (bWaterShortage) { AddUniqueSource(Group.UnrestSources, TEXT("Water access")); AddUniqueSource(Demographics.NationalUnrestSources, TEXT("Water access")); }
            if (bFuelShortage && (bBusiness || bUrban || bSecurityFamilies)) { AddUniqueSource(Group.UnrestSources, TEXT("Fuel and logistics")); AddUniqueSource(Demographics.NationalUnrestSources, TEXT("Fuel and logistics")); }
            if (bMaterialsShortage && (bBusiness || bRural || bUrban)) { AddUniqueSource(Group.UnrestSources, TEXT("Construction materials")); AddUniqueSource(Demographics.NationalUnrestSources, TEXT("Construction materials")); }
            if (bWeakJobs) { AddUniqueSource(Group.UnrestSources, TEXT("Jobs and wages")); AddUniqueSource(Demographics.NationalUnrestSources, TEXT("Jobs and wages")); }
            if (bSecurityConcern) { AddUniqueSource(Group.UnrestSources, TEXT("Security concerns")); AddUniqueSource(Demographics.NationalUnrestSources, TEXT("Security concerns")); }
            if (bPublicServicesWeak) { AddUniqueSource(Group.UnrestSources, TEXT("Public services")); AddUniqueSource(Demographics.NationalUnrestSources, TEXT("Public services")); }

            const int32 Share = FMath::Max(1, Group.PopulationShare);
            WeightedGroupApproval += Group.Approval * Share;
            NeedsPressure += Group.UnrestPressure * Share;
            TotalGroupShare += Share;
        }

        for (FDemocracyRegionState& Region : Demographics.Regions)
        {
            Region.UnrestSources.Reset();
            const bool bCapital = Region.RegionName.Contains(TEXT("Capital"), ESearchCase::IgnoreCase);
            const bool bNorthern = Region.RegionName.Contains(TEXT("Northern"), ESearchCase::IgnoreCase) || Region.Climate.Contains(TEXT("Cold"), ESearchCase::IgnoreCase);
            const bool bCentral = Region.RegionName.Contains(TEXT("Central"), ESearchCase::IgnoreCase) || Region.Climate.Contains(TEXT("Moderate"), ESearchCase::IgnoreCase);
            const bool bSouthern = Region.RegionName.Contains(TEXT("Southern"), ESearchCase::IgnoreCase) || Region.Climate.Contains(TEXT("Tropical"), ESearchCase::IgnoreCase);
            const bool bBorder = Region.RegionName.Contains(TEXT("Border"), ESearchCase::IgnoreCase);
            const bool bIndustrial = Region.RegionName.Contains(TEXT("Industrial"), ESearchCase::IgnoreCase);

            int32 FoodAccessDelta = bFoodShortage ? -(2 + FoodShortageAmount / 28) : 1;
            int32 WaterAccessDelta = bWaterShortage ? -(2 + WaterShortageAmount / 25) : 1;
            int32 JobsDelta = bWeakJobs ? -(2 + FMath::Max(0, 50 - Country.EconomicHealth) / 14) : 1;
            int32 SecurityDelta = bSecurityConcern ? -(2 + Country.Unrest / 28 + State.InvasionRisk.CurrentInvasionRisk / 35) : 1;
            int32 InfrastructureDelta = (Country.Infrastructure - Region.Infrastructure) / 8;
            int32 ApprovalDelta = (Country.PublicApproval - Region.Approval) / 10;

            if (bNorthern)
            {
                SecurityDelta -= bFuelShortage ? 1 : 0;
                InfrastructureDelta -= bFuelShortage ? 1 : 0;
                AddUniqueSource(Region.UnrestSources, bFuelShortage ? TEXT("Heating and logistics") : TEXT("Cold-region transport watch"));
            }
            if (bSouthern)
            {
                WaterAccessDelta -= bWaterShortage || bDisasterCrisis ? 2 : 0;
                ApprovalDelta -= bDisasterCrisis ? 2 : 0;
            }
            if (bCentral)
            {
                JobsDelta += Country.EconomicHealth > 58 ? 1 : 0;
            }
            if (bCapital)
            {
                ApprovalDelta -= bProtestCrisis || bScandalCrisis ? 3 : 0;
                SecurityDelta -= bProtestCrisis ? 2 : 0;
                ApprovalDelta += bRecentGoodPress ? 1 : 0;
                ApprovalDelta -= bRecentBadPress ? 2 : 0;
            }
            if (bBorder)
            {
                SecurityDelta -= bBorderCrisis ? 4 : 0;
                JobsDelta -= bBorderCrisis ? 1 : 0;
                ApprovalDelta += HasPolicy(TEXT("Alliance Outreach")) && !bBorderCrisis ? 1 : 0;
                ApprovalDelta -= HasPolicy(TEXT("Demilitarization")) && bBorderCrisis ? 2 : 0;
            }
            if (bIndustrial)
            {
                JobsDelta -= bFuelShortage || bMaterialsShortage ? 3 : 0;
                ApprovalDelta += HasPolicy(TEXT("Industrial Subsidies")) ? 2 : 0;
                ApprovalDelta -= HasPolicy(TEXT("Conservation Mandate")) && Country.EconomicHealth < 55 ? 1 : 0;
            }

            if (HasPolicy(TEXT("Infrastructure Push")) || State.EconomyBudget.SpendingPosture.Equals(TEXT("Infrastructure Push"), ESearchCase::IgnoreCase))
            {
                InfrastructureDelta += bBorder || bNorthern || bSouthern ? 2 : 1;
                JobsDelta += 1;
            }
            if (HasPolicy(TEXT("Extraction Expansion")))
            {
                JobsDelta += bIndustrial || bCentral ? 2 : 0;
                WaterAccessDelta -= bSouthern ? 1 : 0;
                ApprovalDelta -= bSouthern ? 1 : 0;
            }
            if (HasPolicy(TEXT("Conservation Mandate")))
            {
                WaterAccessDelta += bSouthern ? 2 : 1;
                FoodAccessDelta += bSouthern || bCentral ? 1 : 0;
                JobsDelta -= bIndustrial ? 1 : 0;
            }
            if (HasPolicy(TEXT("Emergency Powers")))
            {
                SecurityDelta += bCapital || bBorder ? 1 : 0;
                ApprovalDelta -= bCapital || bCentral ? 2 : 1;
            }
            if (HasPolicy(TEXT("Civil Liberties")))
            {
                ApprovalDelta += bCapital || bCentral ? 2 : 1;
                SecurityDelta -= bProtestCrisis ? 1 : 0;
            }

            if (bRecentGoodPress)
            {
                ApprovalDelta += LastPressRecord->AnnouncementType.Equals(TEXT("Crisis Reassurance"), ESearchCase::IgnoreCase) ? 1 : 0;
                SecurityDelta += LastPressRecord->AnnouncementType.Equals(TEXT("Crisis Reassurance"), ESearchCase::IgnoreCase) ? 1 : 0;
            }
            if (bRecentBadPress)
            {
                ApprovalDelta -= bCapital || bIndustrial ? 2 : 1;
                AddUniqueSource(Region.UnrestSources, TEXT("Press credibility"));
            }

            Region.FoodAccess = FMath::Clamp(Region.FoodAccess + FoodAccessDelta, 0, 100);
            Region.WaterAccess = FMath::Clamp(Region.WaterAccess + WaterAccessDelta, 0, 100);
            Region.Jobs = FMath::Clamp(Region.Jobs + JobsDelta, 0, 100);
            Region.Security = FMath::Clamp(Region.Security + SecurityDelta, 0, 100);
            Region.Infrastructure = FMath::Clamp(Region.Infrastructure + InfrastructureDelta + (bMaterialsShortage ? -1 : 0), 0, 100);
            Region.Approval = FMath::Clamp(Region.Approval + ApprovalDelta + (Region.Jobs > 58 ? 1 : -1) + (Region.Security > 58 ? 1 : -1), 0, 100);
            Region.Stability = FMath::Clamp((Region.Stability + Country.Stability + Region.Security / 2 + Region.Infrastructure / 2) / 3 - (bProtestCrisis ? 1 : 0) - (bBorder && bBorderCrisis ? 2 : 0), 0, 100);
            Region.Unrest = FMath::Clamp((100 - Region.Approval) / 3
                + FMath::Max(0, 55 - Region.FoodAccess) / 3
                + FMath::Max(0, 55 - Region.WaterAccess) / 3
                + FMath::Max(0, 55 - Region.Jobs) / 4
                + FMath::Max(0, 55 - Region.Security) / 4
                + FMath::Max(0, 52 - Region.Infrastructure) / 6
                + CrisisSeverity / 20, 0, 100);

            if (Region.FoodAccess < 50) { AddUniqueSource(Region.UnrestSources, TEXT("Regional food access")); AddUniqueSource(Demographics.NationalUnrestSources, TEXT("Regional food access")); }
            if (Region.WaterAccess < 50) { AddUniqueSource(Region.UnrestSources, TEXT("Regional water access")); AddUniqueSource(Demographics.NationalUnrestSources, TEXT("Regional water access")); }
            if (Region.Jobs < 50) { AddUniqueSource(Region.UnrestSources, TEXT("Regional jobs")); AddUniqueSource(Demographics.NationalUnrestSources, TEXT("Regional jobs")); }
            if (Region.Security < 50) { AddUniqueSource(Region.UnrestSources, TEXT("Regional security")); AddUniqueSource(Demographics.NationalUnrestSources, TEXT("Regional security")); }
            if (Region.Infrastructure < 45) { AddUniqueSource(Region.UnrestSources, TEXT("Infrastructure access")); AddUniqueSource(Demographics.NationalUnrestSources, TEXT("Infrastructure access")); }
            if (bBorder && bBorderCrisis) { AddUniqueSource(Region.UnrestSources, TEXT("Border tension")); AddUniqueSource(Demographics.NationalUnrestSources, TEXT("Border tension")); }
            if (bSouthern && bDisasterCrisis) { AddUniqueSource(Region.UnrestSources, TEXT("Storm and disaster readiness")); AddUniqueSource(Demographics.NationalUnrestSources, TEXT("Storm and disaster readiness")); }
            if (bIndustrial && (bFuelShortage || bMaterialsShortage)) { AddUniqueSource(Region.UnrestSources, TEXT("Industrial supply chain")); AddUniqueSource(Demographics.NationalUnrestSources, TEXT("Industrial supply chain")); }

            const int32 Share = FMath::Max(1, Region.PopulationShare);
            WeightedRegionApproval += Region.Approval * Share;
            DemographicUnrest += Region.Unrest * Share;
            TotalRegionShare += Share;
        }

        Demographics.AverageGroupApproval = FMath::Clamp(WeightedGroupApproval / FMath::Max(1, TotalGroupShare), 0, 100);
        Demographics.AverageRegionalApproval = FMath::Clamp(WeightedRegionApproval / FMath::Max(1, TotalRegionShare), 0, 100);
        Demographics.NationalNeedsPressure = FMath::Clamp(NeedsPressure / FMath::Max(1, TotalGroupShare), 0, 100);
        Demographics.DemographicUnrestPressure = FMath::Clamp(DemographicUnrest / FMath::Max(1, TotalRegionShare), 0, 100);
        if (Demographics.NationalUnrestSources.Num() == 0)
        {
            Demographics.NationalUnrestSources.Add(TEXT("No dominant demographic unrest source"));
        }
    }
    FString BuildDemographicsSummaryText(const FDemocracyDemographicsState& Demographics)
    {
        return FString::Printf(TEXT("Population %.1fM | group approval %d | regional approval %d | needs pressure %d | demographic unrest %d\nUnrest sources: %s"),
            Demographics.TotalPopulationThousands / 1000.0f,
            Demographics.AverageGroupApproval,
            Demographics.AverageRegionalApproval,
            Demographics.NationalNeedsPressure,
            Demographics.DemographicUnrestPressure,
            *FString::Join(Demographics.NationalUnrestSources, TEXT(", ")));
    }

    void ApplyBudgetPreset(FDemocracyEconomyBudgetState& Budget, const FString& PresetName)
    {
        if (PresetName.Equals(TEXT("Low Taxes"), ESearchCase::IgnoreCase))
        {
            Budget.TaxPolicy = TEXT("Low Taxes");
            Budget.TaxRate = 16;
        }
        else if (PresetName.Equals(TEXT("High Taxes"), ESearchCase::IgnoreCase))
        {
            Budget.TaxPolicy = TEXT("High Taxes");
            Budget.TaxRate = 34;
        }
        else
        {
            Budget.TaxPolicy = TEXT("Balanced Taxation");
            Budget.TaxRate = 24;
        }
    }

    void ApplySpendingPreset(FDemocracyEconomyBudgetState& Budget, const FString& PresetName)
    {
        if (PresetName.Equals(TEXT("Austerity"), ESearchCase::IgnoreCase))
        {
            Budget.SpendingPosture = TEXT("Austerity");
            Budget.PublicServicesSpending = 22;
            Budget.InfrastructureSpending = 18;
            Budget.DefenseSpending = 22;
        }
        else if (PresetName.Equals(TEXT("Public Services"), ESearchCase::IgnoreCase))
        {
            Budget.SpendingPosture = TEXT("Public Services");
            Budget.PublicServicesSpending = 48;
            Budget.InfrastructureSpending = 24;
            Budget.DefenseSpending = 24;
        }
        else if (PresetName.Equals(TEXT("Infrastructure Push"), ESearchCase::IgnoreCase))
        {
            Budget.SpendingPosture = TEXT("Infrastructure Push");
            Budget.PublicServicesSpending = 34;
            Budget.InfrastructureSpending = 42;
            Budget.DefenseSpending = 24;
        }
        else if (PresetName.Equals(TEXT("Defense Funding"), ESearchCase::IgnoreCase))
        {
            Budget.SpendingPosture = TEXT("Defense Funding");
            Budget.PublicServicesSpending = 30;
            Budget.InfrastructureSpending = 24;
            Budget.DefenseSpending = 44;
        }
        else
        {
            Budget.SpendingPosture = TEXT("Balanced Services");
            Budget.PublicServicesSpending = 35;
            Budget.InfrastructureSpending = 28;
            Budget.DefenseSpending = 28;
        }
    }

    int32 GetBudgetCoreSpending(const FDemocracyEconomyBudgetState& Budget)
    {
        return Budget.PublicServicesSpending + Budget.InfrastructureSpending + Budget.DefenseSpending;
    }

    int32 CalculateDebtCapacity(const FDemocracySimulationState& State, const FDemocracyEconomyBudgetState& Budget)
    {
        const FDemocracyCountryState& Country = State.PlayerCountry;
        return FMath::Clamp(
            600 + Country.EconomicHealth * 12 + Country.DiplomaticStanding * 5 + Budget.TaxRate * 7 + Country.Treasury / 2 - Country.Unrest * 5 - Budget.Inflation * 22,
            250,
            4000);
    }

    int32 CalculateSpendingLimit(const FDemocracySimulationState& State, const FDemocracyEconomyBudgetState& Budget)
    {
        const FDemocracyCountryState& Country = State.PlayerCountry;
        const int32 DebtRoom = FMath::Max(0, Budget.DebtCapacity - Budget.Debt);
        const int32 CreditSpend = DebtRoom / 9;
        const int32 ReserveSpend = Country.Treasury / 10;
        return FMath::Clamp(Budget.Income + CreditSpend + ReserveSpend, 42, 240);
    }

    void EnforceBudgetSpendingLimit(FDemocracyEconomyBudgetState& Budget)
    {
        const int32 RequestedCoreSpending = GetBudgetCoreSpending(Budget);
        Budget.bSpendingLimited = RequestedCoreSpending > Budget.SpendingLimit;
        if (!Budget.bSpendingLimited || RequestedCoreSpending <= 0)
        {
            return;
        }

        const float SpendingScale = static_cast<float>(Budget.SpendingLimit) / static_cast<float>(RequestedCoreSpending);
        Budget.PublicServicesSpending = FMath::Clamp(FMath::RoundToInt(Budget.PublicServicesSpending * SpendingScale), 8, Budget.PublicServicesSpending);
        Budget.InfrastructureSpending = FMath::Clamp(FMath::RoundToInt(Budget.InfrastructureSpending * SpendingScale), 8, Budget.InfrastructureSpending);
        Budget.DefenseSpending = FMath::Clamp(FMath::RoundToInt(Budget.DefenseSpending * SpendingScale), 8, Budget.DefenseSpending);
    }

    struct FBudgetOptionEvaluation
    {
        bool bCanSelect = true;
        FString Reason = TEXT("Available.");
        FDemocracyEconomyBudgetState ProjectedBudget;
    };

    void RecalculateEconomyBudget(FDemocracySimulationState& State)
    {
        FDemocracyCountryState& Country = State.PlayerCountry;
        FDemocracyEconomyBudgetState& Budget = State.EconomyBudget;
        const int32 PopulationScale = FMath::Max(1, State.Demographics.TotalPopulationThousands / 1000);
        const int32 TaxStress = FMath::Max(0, Budget.TaxRate - 26) / 2;
        const int32 LowTaxStimulus = Budget.TaxRate < 20 ? 2 : 0;
        const int32 ProductionBase = Country.EconomicHealth + Country.Infrastructure / 2 + FMath::Max(0, Country.Technology * 4) + LowTaxStimulus * 4;
        Budget.ProductionEfficiency = FMath::Clamp(ProductionBase / 2 + Budget.InfrastructureSpending / 3 - FMath::Max(0, Budget.Inflation - 5), 0, 100);
        Budget.Income = FMath::Max(0, (Budget.ProductionEfficiency * Budget.TaxRate * PopulationScale) / 35 + Country.Resources.GasOil / 12 + Country.Resources.Metals / 14 + Country.Resources.Food / 18);
        Budget.DebtCapacity = CalculateDebtCapacity(State, Budget);
        Budget.SpendingLimit = CalculateSpendingLimit(State, Budget);

        const int32 RequestedCoreSpending = GetBudgetCoreSpending(Budget);
        EnforceBudgetSpendingLimit(Budget);
        const int32 EnforcedCoreSpending = GetBudgetCoreSpending(Budget);
        const int32 DebtService = FMath::Max(0, Budget.Debt / 35);
        const int32 BasePopulationExpense = PopulationScale * 4;
        Budget.Expenses = EnforcedCoreSpending + DebtService + BasePopulationExpense;
        Budget.Deficit = Budget.Expenses - Budget.Income;

        if (Budget.Deficit > 0)
        {
            Budget.Debt = FMath::Clamp(Budget.Debt + Budget.Deficit, 0, 30000);
        }
        else
        {
            Budget.Debt = FMath::Max(0, Budget.Debt + Budget.Deficit / 2);
        }

        const int32 DebtOverCapacity = FMath::Max(0, Budget.Debt - Budget.DebtCapacity);
        Budget.CreditStress = FMath::Clamp(FMath::Max(0, Budget.Deficit) / 8 + DebtOverCapacity / 20 + FMath::Max(0, Budget.Inflation - 6) * 3, 0, 100);
        Budget.Inflation = FMath::Clamp(2 + FMath::Max(0, Budget.Deficit) / 36 + FMath::Max(0, Budget.Debt - 800) / 320 + DebtOverCapacity / 160 + (Budget.TaxRate < 18 ? 1 : 0), 0, 45);
        Budget.PublicServices = FMath::Clamp(Budget.PublicServices + (Budget.PublicServicesSpending - 30) / 4 - FMath::Max(0, Budget.Inflation - 8) / 2 - FMath::Max(0, State.Demographics.NationalNeedsPressure - 50) / 8 - Budget.CreditStress / 35, 0, 100);

        Country.Treasury = FMath::Max(0, Country.Treasury - Budget.Deficit - DebtOverCapacity / 10);
        Country.EconomicHealth = FMath::Clamp(Country.EconomicHealth + Budget.ProductionEfficiency / 30 - TaxStress - FMath::Max(0, Budget.Inflation - 8) / 3 - (Budget.Deficit > 80 ? 1 : 0) - Budget.CreditStress / 35, 0, 100);
        Country.PublicApproval = FMath::Clamp(Country.PublicApproval + Budget.PublicServices / 35 - TaxStress - FMath::Max(0, Budget.Inflation - 6) / 3 + (Budget.TaxRate < 20 ? 1 : 0) - Budget.CreditStress / 40 - (Budget.bSpendingLimited ? 1 : 0), 0, 100);
        Country.Stability = FMath::Clamp(Country.Stability - Budget.CreditStress / 45 - FMath::Max(0, Budget.Inflation - 10) / 4 - (Budget.bSpendingLimited ? 1 : 0), 0, 100);
        Country.Unrest = FMath::Clamp(Country.Unrest + Budget.CreditStress / 50 + FMath::Max(0, Budget.Inflation - 10) / 5 + (Budget.bSpendingLimited ? 1 : 0), 0, 100);
        Country.Infrastructure = FMath::Clamp(Country.Infrastructure + Budget.InfrastructureSpending / 28 - 1, 0, 100);
        Country.MilitaryReadiness = FMath::Clamp(Country.MilitaryReadiness + Budget.DefenseSpending / 32 - 1, 0, 100);

        if (Budget.bSpendingLimited)
        {
            Budget.BudgetConstraintStatus = FString::Printf(TEXT("Spending request %d exceeded capacity %d. Budget was capped to services %d, infrastructure %d, defense %d."), RequestedCoreSpending, Budget.SpendingLimit, Budget.PublicServicesSpending, Budget.InfrastructureSpending, Budget.DefenseSpending);
        }
        else if (Budget.Debt > Budget.DebtCapacity)
        {
            Budget.BudgetConstraintStatus = FString::Printf(TEXT("Debt exceeds capacity by %d. Low taxes and high-spending options are restricted until credit stress falls."), DebtOverCapacity);
        }
        else if (Budget.CreditStress >= 60)
        {
            Budget.BudgetConstraintStatus = FString::Printf(TEXT("Credit stress %d. Future spending options are narrowing."), Budget.CreditStress);
        }
        else
        {
            Budget.BudgetConstraintStatus = FString::Printf(TEXT("Within spending cap %d and debt capacity %d."), Budget.SpendingLimit, Budget.DebtCapacity);
        }

        Budget.LastBudgetSummary = FString::Printf(TEXT("Income %d, expenses %d, %s %d, debt %d/%d, inflation %d, services %d, production %d, credit stress %d. %s"),
            Budget.Income,
            Budget.Expenses,
            Budget.Deficit >= 0 ? TEXT("deficit") : TEXT("surplus"),
            FMath::Abs(Budget.Deficit),
            Budget.Debt,
            Budget.DebtCapacity,
            Budget.Inflation,
            Budget.PublicServices,
            Budget.ProductionEfficiency,
            Budget.CreditStress,
            *Budget.BudgetConstraintStatus);
    }

    FString BuildEconomyBudgetSummaryText(const FDemocracyEconomyBudgetState& Budget)
    {
        return FString::Printf(TEXT("%s | tax %d%% | %s | income %d | expenses %d | %s %d | debt %d/%d | spending cap %d | inflation %d | credit stress %d | services %d | production %d\n%s"),
            *Budget.TaxPolicy,
            Budget.TaxRate,
            *Budget.SpendingPosture,
            Budget.Income,
            Budget.Expenses,
            Budget.Deficit >= 0 ? TEXT("deficit") : TEXT("surplus"),
            FMath::Abs(Budget.Deficit),
            Budget.Debt,
            Budget.DebtCapacity,
            Budget.SpendingLimit,
            Budget.Inflation,
            Budget.CreditStress,
            Budget.PublicServices,
            Budget.ProductionEfficiency,
            *Budget.LastBudgetSummary);
    }

    FBudgetOptionEvaluation EvaluateSpendingPostureRules(const FDemocracySimulationState& State, const FString& SpendingPostureName)
    {
        FBudgetOptionEvaluation Evaluation;
        FDemocracySimulationState ProjectedState = State;
        ApplySpendingPreset(ProjectedState.EconomyBudget, SpendingPostureName);
        const int32 RequestedCoreSpending = GetBudgetCoreSpending(ProjectedState.EconomyBudget);
        RecalculateEconomyBudget(ProjectedState);
        Evaluation.ProjectedBudget = ProjectedState.EconomyBudget;
        if (RequestedCoreSpending > ProjectedState.EconomyBudget.SpendingLimit)
        {
            Evaluation.bCanSelect = false;
            Evaluation.Reason = FString::Printf(TEXT("Spending cap %d blocks requested spending %d. Raise income, lower debt, or choose austerity/balanced services."), ProjectedState.EconomyBudget.SpendingLimit, RequestedCoreSpending);
        }
        else if (ProjectedState.EconomyBudget.CreditStress >= 85 && !SpendingPostureName.Equals(TEXT("Austerity"), ESearchCase::IgnoreCase))
        {
            Evaluation.bCanSelect = false;
            Evaluation.Reason = FString::Printf(TEXT("Credit stress %d is too high for this spending option. Stabilize debt first."), ProjectedState.EconomyBudget.CreditStress);
        }
        else
        {
            Evaluation.Reason = FString::Printf(TEXT("Available. Projected %s %d, debt %d/%d, inflation %d, credit stress %d."), ProjectedState.EconomyBudget.Deficit >= 0 ? TEXT("deficit") : TEXT("surplus"), FMath::Abs(ProjectedState.EconomyBudget.Deficit), ProjectedState.EconomyBudget.Debt, ProjectedState.EconomyBudget.DebtCapacity, ProjectedState.EconomyBudget.Inflation, ProjectedState.EconomyBudget.CreditStress);
        }
        return Evaluation;
    }

    FBudgetOptionEvaluation EvaluateTaxPolicyRules(const FDemocracySimulationState& State, const FString& TaxPolicyName)
    {
        FBudgetOptionEvaluation Evaluation;
        FDemocracySimulationState ProjectedState = State;
        ApplyBudgetPreset(ProjectedState.EconomyBudget, TaxPolicyName);
        RecalculateEconomyBudget(ProjectedState);
        Evaluation.ProjectedBudget = ProjectedState.EconomyBudget;
        if (TaxPolicyName.Equals(TEXT("Low Taxes"), ESearchCase::IgnoreCase) && (State.EconomyBudget.CreditStress >= 70 || State.EconomyBudget.Debt > State.EconomyBudget.DebtCapacity))
        {
            Evaluation.bCanSelect = false;
            Evaluation.Reason = FString::Printf(TEXT("Low taxes locked while credit stress is %d and debt is %d/%d."), State.EconomyBudget.CreditStress, State.EconomyBudget.Debt, State.EconomyBudget.DebtCapacity);
        }
        else
        {
            Evaluation.Reason = FString::Printf(TEXT("Available. Projected income %d, %s %d, inflation %d, credit stress %d."), ProjectedState.EconomyBudget.Income, ProjectedState.EconomyBudget.Deficit >= 0 ? TEXT("deficit") : TEXT("surplus"), FMath::Abs(ProjectedState.EconomyBudget.Deficit), ProjectedState.EconomyBudget.Inflation, ProjectedState.EconomyBudget.CreditStress);
        }
        return Evaluation;
    }

    bool HasCitizenGroup(const FDemocracyDemographicsState& Demographics, const FString& GroupName)
    {
        for (const FDemocracyCitizenGroupState& Group : Demographics.CitizenGroups)
        {
            if (Group.GroupName.Equals(GroupName, ESearchCase::IgnoreCase))
            {
                return true;
            }
        }
        return false;
    }

    bool HasRegion(const FDemocracyDemographicsState& Demographics, const FString& RegionName)
    {
        for (const FDemocracyRegionState& Region : Demographics.Regions)
        {
            if (Region.RegionName.Equals(RegionName, ESearchCase::IgnoreCase))
            {
                return true;
            }
        }
        return false;
    }

    void EnsureDepartmentExists(FDemocracySimulationState& State, const FDemocracyDepartmentState& Department)
    {
        if (!FindDepartmentConst(State.Departments, Department.DepartmentName))
        {
            State.Departments.Departments.Add(Department);
        }
    }

    void SeedBalanceTestResourceChains(FDemocracySimulationState& State)
    {
        FDemocracyCountryState& Country = State.PlayerCountry;
        const FDemocracyResourceInventory& Resources = Country.Resources;
        const int32 DifficultyScore = FMath::Clamp(Country.CountrySizeScore, 1, 4);
        const int32 PopulationScale = FMath::Max(1, State.Demographics.TotalPopulationThousands / 1000);
        const int32 InfrastructureBonus = FMath::Clamp(Country.Infrastructure / 18, 1, 5);
        const int32 EconomyBonus = FMath::Clamp(Country.EconomicHealth / 22, 1, 4);
        const int32 ClimateFoodBonus = Country.Climate.Equals(TEXT("Southern Tropical"), ESearchCase::IgnoreCase) ? 4 : (Country.Climate.Equals(TEXT("Northern Cold"), ESearchCase::IgnoreCase) ? -1 : 2);
        const int32 ClimateWaterBonus = Country.Climate.Equals(TEXT("Northern Cold"), ESearchCase::IgnoreCase) ? 3 : (Country.Climate.Equals(TEXT("Southern Tropical"), ESearchCase::IgnoreCase) ? 2 : 1);

        const int32 FoodTarget = 150 + DifficultyScore * 18;
        const int32 WaterTarget = 130 + DifficultyScore * 15;
        const int32 FuelTarget = 80 + DifficultyScore * 14;
        const int32 WoodTarget = 70 + DifficultyScore * 10;
        const int32 MetalsTarget = 75 + DifficultyScore * 12;

        const int32 FoodUse = 8 + PopulationScale + DifficultyScore * 2;
        const int32 WaterUse = 7 + PopulationScale + DifficultyScore * 2;
        const int32 FuelUse = 5 + DifficultyScore * 2 + State.EconomyBudget.DefenseSpending / 18;
        const int32 WoodUse = 4 + State.EconomyBudget.InfrastructureSpending / 20;
        const int32 MetalsUse = 4 + State.EconomyBudget.DefenseSpending / 20 + DifficultyScore;

        State.ResourceChains.Chains.Reset();
        State.ResourceChains.Chains.Add(MakeResourceChainEntry(TEXT("Food"), Resources.Food, FoodTarget, 9 + InfrastructureBonus + ClimateFoodBonus, FoodUse, Resources.Food < FoodTarget ? 5 : 1, Resources.Food > FoodTarget + 70 ? 3 : 0, 90, TEXT("Feeds the population; shortages quickly raise unrest and lower approval."), { TEXT("farms"), TEXT("climate"), TEXT("roads"), TEXT("emergency imports") }));
        State.ResourceChains.Chains.Add(MakeResourceChainEntry(TEXT("Water"), Resources.Water, WaterTarget, 8 + ClimateWaterBonus + State.EconomyBudget.PublicServices / 28, WaterUse, Resources.Water < WaterTarget ? 3 : 0, 0, 95, TEXT("Supports health, services, and regional stability; shortages hurt demographics."), { TEXT("utilities"), TEXT("climate"), TEXT("public services"), TEXT("regional access") }));
        State.ResourceChains.Chains.Add(MakeResourceChainEntry(TEXT("Fuel"), Resources.GasOil, FuelTarget, 6 + EconomyBonus, FuelUse, Resources.GasOil < FuelTarget ? 5 : 1, Resources.GasOil > FuelTarget + 55 ? 2 : 0, 100, TEXT("Powers logistics, industry, and military readiness; shortages increase inflation."), { TEXT("extraction"), TEXT("trade lanes"), TEXT("defense demand"), TEXT("industry demand") }));
        State.ResourceChains.Chains.Add(MakeResourceChainEntry(TEXT("Wood"), Resources.Wood, WoodTarget, 5 + Country.EnvironmentalHealth / 25, WoodUse, Resources.Wood < WoodTarget ? 2 : 0, Resources.Wood > WoodTarget + 45 ? 2 : 0, 60, TEXT("Supports construction, repairs, and disaster recovery."), { TEXT("forestry"), TEXT("conservation"), TEXT("infrastructure repairs") }));
        State.ResourceChains.Chains.Add(MakeResourceChainEntry(TEXT("Metals"), Resources.Metals, MetalsTarget, 5 + EconomyBonus + Country.Infrastructure / 35, MetalsUse, Resources.Metals < MetalsTarget ? 3 : 0, Resources.Metals > MetalsTarget + 45 ? 2 : 0, 85, TEXT("Supports industry, infrastructure, and military production."), { TEXT("mining"), TEXT("manufacturing"), TEXT("defense production"), TEXT("construction") }));

        int32 WeightedShortage = 0;
        int32 ImportCost = 0;
        int32 ExportIncome = 0;
        for (const FDemocracyResourceChainEntry& Entry : State.ResourceChains.Chains)
        {
            WeightedShortage += (Entry.Shortage * Entry.StrategicValue) / FMath::Max(1, Entry.ReserveTarget);
            ImportCost += Entry.Imports * FMath::Clamp(Entry.StrategicValue / 12, 3, 9);
            ExportIncome += Entry.Exports * FMath::Clamp(Entry.StrategicValue / 15, 2, 7);
        }
        State.ResourceChains.TotalShortagePressure = FMath::Clamp(WeightedShortage, 0, 100);
        State.ResourceChains.TradeBalance = ExportIncome - ImportCost;
        State.ResourceChains.LastUpdatedTurn = State.Turn;
        State.ResourceChains.Summary = TEXT("Early-game test chain seeded with production, consumption, import/export pressure, reserve targets, and shortage drivers.");
    }

    void SeedBalanceTestDemographics(FDemocracySimulationState& State)
    {
        FDemocracyDemographicsState& Demographics = State.Demographics;
        FDemocracyCountryState& Country = State.PlayerCountry;
        if (Demographics.TotalPopulationThousands <= 0)
        {
            Demographics.TotalPopulationThousands = FMath::Clamp(1800 * FMath::Max(1, Country.CountrySizeScore) + Country.PublicApproval * 20, 1500, 12000);
        }

        if (Demographics.CitizenGroups.Num() == 0 || (State.Turn <= 1 && Demographics.CitizenGroups.Num() < 8))
        {
            Demographics.CitizenGroups = {
                { TEXT("Urban Workers"), 22, Country.PublicApproval, 48, 48, 58, 46, 50, 12, { TEXT("Housing costs"), TEXT("Wage pressure") } },
                { TEXT("Rural Communities"), 15, Country.PublicApproval - 2, 55, 45, 48, 52, 46, 10, { TEXT("Transport access"), TEXT("Farm input costs") } },
                { TEXT("Business Owners"), 11, Country.PublicApproval + 3, 42, 42, 62, 48, 42, 8, { TEXT("Tax uncertainty"), TEXT("Market confidence") } },
                { TEXT("Public Sector"), 12, Country.PublicApproval + 1, 45, 45, 52, 56, 55, 8, { TEXT("Budget reliability") } },
                { TEXT("Youth and Students"), 14, Country.PublicApproval - 3, 44, 44, 64, 42, 52, 14, { TEXT("Jobs outlook"), TEXT("Civil rights") } },
                { TEXT("Retirees"), 10, Country.PublicApproval, 50, 50, 35, 55, 64, 9, { TEXT("Healthcare access"), TEXT("Savings security") } },
                { TEXT("Low-Income Households"), 10, Country.PublicApproval - 4, 62, 60, 58, 52, 58, 16, { TEXT("Food prices"), TEXT("Utility costs") } },
                { TEXT("Veterans and Security Families"), 6, Country.PublicApproval + 1, 46, 46, 48, 66, 52, 8, { TEXT("Readiness concerns"), TEXT("Public safety") } }
            };
        }
        else if (!HasCitizenGroup(Demographics, TEXT("Low-Income Households")) && State.Turn <= 1)
        {
            Demographics.CitizenGroups.Add({ TEXT("Low-Income Households"), 10, Country.PublicApproval - 4, 62, 60, 58, 52, 58, 16, { TEXT("Food prices"), TEXT("Utility costs") } });
        }

        if (Demographics.Regions.Num() == 0 || (State.Turn <= 1 && Demographics.Regions.Num() < 6))
        {
            Demographics.Regions = {
                { TEXT("Capital District"), Country.Climate, 20, Country.PublicApproval + 2, Country.Stability, Country.Unrest, 58, 58, 62, 56, Country.Infrastructure, { TEXT("Visibility of national politics") } },
                { TEXT("Northern Region"), TEXT("Northern Cold"), 16, Country.PublicApproval, Country.Stability, Country.Unrest, 52, 50, 50, 54, Country.Infrastructure - 4, { TEXT("Heating and transport costs") } },
                { TEXT("Central Region"), TEXT("Middle Moderate"), 24, Country.PublicApproval + 1, Country.Stability, Country.Unrest, 58, 57, 56, 52, Country.Infrastructure, { TEXT("Cost of living") } },
                { TEXT("Southern Region"), TEXT("Southern Tropical"), 18, Country.PublicApproval - 1, Country.Stability - 1, Country.Unrest + 1, 55, 60, 52, 50, Country.Infrastructure - 2, { TEXT("Storm readiness"), TEXT("Water management") } },
                { TEXT("Border Region"), Country.Climate, 12, Country.PublicApproval - 2, Country.Stability - 2, Country.Unrest + 2, 50, 50, 48, 45, Country.Infrastructure - 5, { TEXT("Border security"), TEXT("Trade disruption") } },
                { TEXT("Industrial Corridor"), TEXT("Middle Moderate"), 10, Country.PublicApproval - 1, Country.Stability, Country.Unrest + 1, 48, 50, 60, 50, Country.Infrastructure - 1, { TEXT("Factory employment"), TEXT("fuel costs") } }
            };
        }
        else if (!HasRegion(Demographics, TEXT("Industrial Corridor")) && State.Turn <= 1)
        {
            Demographics.Regions.Add({ TEXT("Industrial Corridor"), TEXT("Middle Moderate"), 10, Country.PublicApproval - 1, Country.Stability, Country.Unrest + 1, 48, 50, 60, 50, Country.Infrastructure - 1, { TEXT("Factory employment"), TEXT("fuel costs") } });
        }
    }

    void SeedBalanceTestDepartments(FDemocracySimulationState& State)
    {
        InitializeDefaultDepartments(State);
        const FDemocracyCountryState& Country = State.PlayerCountry;
        EnsureDepartmentExists(State, MakeDepartment(TEXT("Foreign Affairs"), TEXT("Foreign Minister"), TEXT("Diplomacy, alliances, trade access, sanctions, and foreign official meetings."), 10, 50, Country.DiplomaticStanding, 50, 50, TEXT("Alliance Outreach"), TEXT("Diplomacy policy and foreign meetings"), TEXT("Use diplomatic work to reduce takeover risk and improve trade access."), { TEXT("Improves diplomacy and foreign trust."), TEXT("Requires credibility and treasury support.") }));
        EnsureDepartmentExists(State, MakeDepartment(TEXT("Justice"), TEXT("Justice Minister"), TEXT("Public order, investigations, legitimacy, corruption, and legal stability."), 9, 48, FMath::Clamp(70 - Country.Unrest, 25, 75), 50, 50, TEXT("Anti-Corruption Review"), TEXT("Civil policy and legitimacy"), TEXT("Use transparent investigations to reduce unrest and credibility damage."), { TEXT("Improves legitimacy and lowers internal risk."), TEXT("Can expose scandals in the short term.") }));
        EnsureDepartmentExists(State, MakeDepartment(TEXT("Commerce"), TEXT("Commerce Minister"), TEXT("Jobs, industry confidence, imports, exports, and market-shock response."), 11, 52, Country.EconomicHealth, 51, 50, TEXT("Stabilize Markets"), TEXT("Economic policy and trade"), TEXT("Use commerce actions to turn resource chains into income and jobs."), { TEXT("Improves jobs and production efficiency."), TEXT("Can increase import exposure.") }));
        EnsureDepartmentExists(State, MakeDepartment(TEXT("Environment"), TEXT("Environment Minister"), TEXT("Conservation, water pressure, disaster resilience, and extraction tradeoffs."), 8, 46, Country.EnvironmentalHealth, 49, 45, TEXT("Monitor Climate Risk"), TEXT("Environmental policy and resource chains"), TEXT("Use environmental action to protect water, agriculture, and disaster response."), { TEXT("Improves environmental health and resilience."), TEXT("Can slow extraction-heavy growth.") }));
    }

    void SeedBalanceTestEvents(FDemocracySimulationState& State)
    {
        FDemocracyEventSystemState& EventSystem = State.EventSystem;
        if (EventSystem.ActiveEventLimit <= 0)
        {
            EventSystem.ActiveEventLimit = 3;
        }
        if (State.Turn > 1 || EventSystem.ActiveEvents.Num() > 0 || EventSystem.EventHistory.Num() > 0)
        {
            return;
        }

        EventSystem.ActiveEvents.Add(MakeEvent(State, TEXT("Shortage"), TEXT("Opening Supply Review"), TEXT("Regional staff report that food, water, and fuel reserves are adequate but thin enough to test the first policy and resource-chain decisions."), TEXT("Seeded early-game test event."), 38, true, {
            MakeEventChoice(TEXT("audit"), TEXT("Audit and target reserves"), TEXT("Order a fast audit and redirect supplies to the most exposed regions."), TEXT("Small stability gain, unrest drops, modest treasury cost, food and water improve."), 2, 3, -3, -45, 0, 0, 0, 1, 0, 25, 20, 0, 0, 0, -1, 0),
            MakeEventChoice(TEXT("imports"), TEXT("Authorize emergency imports"), TEXT("Use treasury to secure food, water, and fuel buffers before shortages become public."), TEXT("Resources improve more, treasury drops, diplomacy improves slightly."), 1, 2, -2, -95, 0, 2, 0, 0, 0, 45, 30, 20, 0, 0, -1, -1),
            MakeEventChoice(TEXT("wait"), TEXT("Wait for first reports"), TEXT("Delay action until the first simulation tick exposes clearer pressure points."), TEXT("Treasury is protected, but unrest and internal risk rise slightly."), -2, -1, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0)
        }));

        if (EventSystem.ActiveEvents.Num() < EventSystem.ActiveEventLimit)
        {
            EventSystem.ActiveEvents.Add(MakeEvent(State, TEXT("Economic Shock"), TEXT("Early Market Confidence Test"), TEXT("Business leaders and labor representatives want a clear first signal on jobs, taxes, and investment priorities."), TEXT("Seeded early-game test event."), 34, true, {
                MakeEventChoice(TEXT("jobs"), TEXT("Announce jobs package"), TEXT("Fund a targeted employment and logistics program."), TEXT("Approval and economy improve, treasury falls, infrastructure gains slightly."), 4, 2, -2, -110, 4, 0, 0, 2, 0, 10, 0, 0, 5, 5, -1, 0),
                MakeEventChoice(TEXT("business"), TEXT("Back business credit"), TEXT("Protect employers and suppliers with credit guarantees."), TEXT("Economy and commerce improve, approval gains are smaller, treasury risk rises."), 1, 1, -1, -75, 5, 1, 0, 0, -1, 0, 0, 5, 0, 5, 0, 0),
                MakeEventChoice(TEXT("reserve"), TEXT("Protect treasury reserve"), TEXT("Avoid early spending and hold cash for later crises."), TEXT("Treasury is protected, approval and jobs confidence fall."), -4, -2, 3, 40, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0)
            }));
        }

        EventSystem.LastEventTurn = State.Turn;
    }

    FString RelationshipStatusForAlignmentRuntime(const FString& Alignment)
    {
        if (Alignment.Equals(TEXT("Player"), ESearchCase::IgnoreCase) || Alignment.Equals(TEXT("Allied"), ESearchCase::IgnoreCase)) { return TEXT("Ally"); }
        if (Alignment.Equals(TEXT("Hostile"), ESearchCase::IgnoreCase)) { return TEXT("Hostile"); }
        if (Alignment.Equals(TEXT("Tense"), ESearchCase::IgnoreCase)) { return TEXT("Rival"); }
        return TEXT("Neutral");
    }

    void RecalculateDiplomacyMatrixSummary(FDemocracyDiplomacyMatrixState& Matrix)
    {
        Matrix.AllyCount = 0;
        Matrix.NeutralCount = 0;
        Matrix.RivalCount = 0;
        Matrix.HostileCount = 0;
        Matrix.TradePartnerCount = 0;
        Matrix.SanctionsCount = 0;
        Matrix.TreatyCount = 0;
        int32 BorderTotal = 0;
        for (const FDemocracyDiplomacyRelationshipState& Relationship : Matrix.Relationships)
        {
            if (Relationship.RelationshipStatus.Equals(TEXT("Ally"), ESearchCase::IgnoreCase)) { ++Matrix.AllyCount; }
            else if (Relationship.RelationshipStatus.Equals(TEXT("Rival"), ESearchCase::IgnoreCase)) { ++Matrix.RivalCount; }
            else if (Relationship.RelationshipStatus.Equals(TEXT("Hostile"), ESearchCase::IgnoreCase)) { ++Matrix.HostileCount; }
            else { ++Matrix.NeutralCount; }
            if (Relationship.bTradePartner) { ++Matrix.TradePartnerCount; }
            if (Relationship.bSanctionsActive) { ++Matrix.SanctionsCount; }
            if (Relationship.ActiveTreaties.Num() > 0 || !Relationship.TreatyStatus.Equals(TEXT("None"), ESearchCase::IgnoreCase)) { ++Matrix.TreatyCount; }
            BorderTotal += Relationship.BorderTension;
        }
        Matrix.AverageBorderTension = Matrix.Relationships.Num() > 0 ? BorderTotal / Matrix.Relationships.Num() : 0;
        Matrix.Summary = FString::Printf(TEXT("Diplomacy matrix: %d allies, %d neutral, %d rivals, %d hostile, %d trade partners, %d sanctions, average border tension %d."), Matrix.AllyCount, Matrix.NeutralCount, Matrix.RivalCount, Matrix.HostileCount, Matrix.TradePartnerCount, Matrix.SanctionsCount, Matrix.AverageBorderTension);
    }

    void InitializeDiplomacyMatrixIfMissing(FDemocracySimulationState& State)
    {
        if (State.DiplomacyMatrix.Relationships.Num() > 0)
        {
            RecalculateDiplomacyMatrixSummary(State.DiplomacyMatrix);
            return;
        }

        State.DiplomacyMatrix.LastUpdatedTurn = State.Turn;
        for (const FDemocracyContinentState& Continent : State.WorldMap.Continents)
        {
            for (const FDemocracyGeneratedCountryState& Country : Continent.Countries)
            {
                FDemocracyDiplomacyRelationshipState Relationship;
                Relationship.CountryName = Country.CountryName;
                Relationship.ContinentName = Country.ContinentName;
                Relationship.GovernmentType = Country.PoliticalType;
                Relationship.RelationshipStatus = RelationshipStatusForAlignmentRuntime(Country.DiplomaticAlignment);
                Relationship.BorderTension = Country.BorderPressure;
                Relationship.Trust = FMath::Clamp(Country.Stability + (Country.bAlliedWithPlayer ? 20 : 0) - Country.BorderPressure / 2, 0, 100);
                Relationship.bTradePartner = Relationship.RelationshipStatus.Equals(TEXT("Ally"), ESearchCase::IgnoreCase) || (Relationship.RelationshipStatus.Equals(TEXT("Neutral"), ESearchCase::IgnoreCase) && Relationship.Trust >= 45);
                Relationship.bSanctionsActive = Relationship.RelationshipStatus.Equals(TEXT("Hostile"), ESearchCase::IgnoreCase) && Relationship.BorderTension >= 55;
                Relationship.TreatyStatus = Relationship.RelationshipStatus.Equals(TEXT("Ally"), ESearchCase::IgnoreCase) ? TEXT("Mutual Recognition") : TEXT("None");
                Relationship.TradeValue = Relationship.bTradePartner ? FMath::Clamp(Country.PowerScore / 2 + Relationship.Trust / 3 - Country.BorderPressure / 4, 5, 80) : 0;
                Relationship.LastChangedTurn = State.Turn;
                if (!Relationship.TreatyStatus.Equals(TEXT("None"), ESearchCase::IgnoreCase)) { Relationship.ActiveTreaties.Add(Relationship.TreatyStatus); }
                if (Relationship.bTradePartner) { Relationship.Notes.Add(TEXT("Trade route available for resource-chain imports and exports.")); }
                if (Relationship.bSanctionsActive) { Relationship.Notes.Add(TEXT("Sanctions active due to hostile posture and border pressure.")); }
                State.DiplomacyMatrix.Relationships.Add(Relationship);
            }
        }
        RecalculateDiplomacyMatrixSummary(State.DiplomacyMatrix);
    }
    bool IsDemocraticGovernmentType(const FString& PoliticalType)
    {
        return PoliticalType.Contains(TEXT("Democracy"), ESearchCase::IgnoreCase) ||
            PoliticalType.Contains(TEXT("Democratic"), ESearchCase::IgnoreCase);
    }

    bool IsDictatorshipGovernmentType(const FString& PoliticalType)
    {
        return PoliticalType.Contains(TEXT("Dictatorship"), ESearchCase::IgnoreCase) ||
            PoliticalType.Contains(TEXT("Authoritarian"), ESearchCase::IgnoreCase) ||
            PoliticalType.Contains(TEXT("Autocracy"), ESearchCase::IgnoreCase) ||
            PoliticalType.Contains(TEXT("Regime"), ESearchCase::IgnoreCase);
    }

    void RefreshObjectiveState(FDemocracySimulationState& State, const FString& ModeOverride = TEXT(""))
    {
        FDemocracyObjectiveState& Objective = State.ObjectiveState;
        if (!ModeOverride.IsEmpty())
        {
            Objective.Mode = ModeOverride;
        }
        if (Objective.Mode.IsEmpty())
        {
            Objective.Mode = TEXT("SinglePlayer");
        }
        if (Objective.PlayerGovernmentType.IsEmpty())
        {
            Objective.PlayerGovernmentType = TEXT("Democracy");
        }

        int32 DemocraticCount = 0;
        int32 DictatorshipCount = 0;
        int32 OtherCount = 0;
        for (const FDemocracyContinentState& Continent : State.WorldMap.Continents)
        {
            for (const FDemocracyGeneratedCountryState& GeneratedCountry : Continent.Countries)
            {
                if (IsDemocraticGovernmentType(GeneratedCountry.PoliticalType))
                {
                    ++DemocraticCount;
                }
                else if (IsDictatorshipGovernmentType(GeneratedCountry.PoliticalType))
                {
                    ++DictatorshipCount;
                }
                else
                {
                    ++OtherCount;
                }
            }
        }
        if (DemocraticCount + DictatorshipCount + OtherCount <= 0)
        {
            DemocraticCount = FMath::Max(0, State.WorldMap.DemocraticAllyCount);
            DictatorshipCount = FMath::Max(0, State.WorldMap.NonDemocraticCountryCount);
            OtherCount = FMath::Max(0, State.WorldMap.TotalCountryCount - DemocraticCount - DictatorshipCount);
        }

        Objective.DemocraticCountryCount = DemocraticCount;
        Objective.DictatorshipCountryCount = DictatorshipCount;
        Objective.OtherGovernmentCount = OtherCount;
        Objective.TotalTrackedCountryCount = DemocraticCount + DictatorshipCount + OtherCount;
        Objective.DictatorshipsRemainingForVictory = DictatorshipCount;
        Objective.DemocracyConversionProgress = Objective.TotalTrackedCountryCount > 0 ? FMath::Clamp((DemocraticCount * 100) / Objective.TotalTrackedCountryCount, 0, 100) : 0;
        Objective.VictoryCondition = TEXT("Single-player: convert all dictatorships to democracy.");
        Objective.PostVictoryObjective = TEXT("Keep democratic systems stable after victory and prevent authoritarian regression.");
        Objective.MultiplayerServerObjective = TEXT("Multiplayer: persistent server-state objectives with no final win condition.");
        Objective.bSimulationContinuesAfterVictory = true;
        Objective.ObjectiveHooks.Reset();
        Objective.AllianceRules = {
            TEXT("Democracies can form alliances only with democracies."),
            TEXT("Dictatorships can form alliances only with dictatorships."),
            TEXT("Changing government alignment takes time and applies stability, trust, and diplomacy consequences.")
        };
        Objective.ActiveObjectiveNotes.Reset();

        const bool bMultiplayerMode = Objective.Mode.Equals(TEXT("Multiplayer"), ESearchCase::IgnoreCase);
        if (bMultiplayerMode)
        {
            Objective.bMultiplayerOngoingNoFinalWin = true;
            Objective.bSoftVictoryAchieved = false;
            Objective.SoftVictoryTurn = 0;
            Objective.PostVictoryTurnsElapsed = 0;
            Objective.bPostVictoryContinuationActive = false;
            Objective.bRegressionMonitoringActive = false;
            Objective.bRegressionWarningActive = false;
            Objective.LongTermObjective = TEXT("Persistent multiplayer: survive, expand influence, manage alliances, and evolve government alignment without a final victory screen.");
            Objective.ObjectiveHooks = { TEXT("server_state_objectives"), TEXT("no_final_win_condition"), TEXT("alignment_slots"), TEXT("side_switch_consequences") };
            if (Objective.GovernmentTransitionTurnsRemaining > 0)
            {
                Objective.ActiveObjectiveNotes.Add(FString::Printf(TEXT("Government transition toward %s is in progress: %d%% complete, %d turns remaining."), *Objective.GovernmentTransitionTarget, Objective.GovernmentTransitionProgress, Objective.GovernmentTransitionTurnsRemaining));
            }
            Objective.ObjectiveSummary = FString::Printf(TEXT("Multiplayer objective: ongoing server state with no final victory. Current alignment: %s. Server slots and save authority remain server controlled. Alliances are limited to matching government types."), *Objective.PlayerGovernmentType);
            return;
        }

        Objective.Mode = TEXT("SinglePlayer");
        Objective.bMultiplayerOngoingNoFinalWin = false;
        Objective.PlayerGovernmentType = TEXT("Democracy");
        Objective.LongTermObjective = TEXT("Convert every dictatorship to democracy, then keep the world stable enough to prevent democratic backsliding.");
        Objective.ObjectiveHooks.Add(TEXT("single_player_conversion_victory"));
        if (DictatorshipCount <= 0)
        {
            if (!Objective.bSoftVictoryAchieved)
            {
                Objective.bSoftVictoryAchieved = true;
                Objective.SoftVictoryTurn = State.Turn;
            }
            Objective.PostVictoryTurnsElapsed = FMath::Max(0, State.Turn - Objective.SoftVictoryTurn);
            Objective.bPostVictoryContinuationActive = true;
            Objective.bRegressionMonitoringActive = true;
            Objective.RegressionRisk = FMath::Clamp((100 - State.PlayerCountry.Stability) / 3 + State.PlayerCountry.Unrest / 4 + FMath::Max(0, 45 - State.PlayerCountry.DiplomaticStanding) / 3, 0, 100);
            Objective.bRegressionWarningActive = Objective.RegressionRisk >= 35;
            Objective.ObjectiveHooks.Add(TEXT("post_victory_continuation"));
            Objective.ObjectiveHooks.Add(TEXT("regression_monitoring"));
            Objective.ObjectiveSummary = FString::Printf(TEXT("Soft victory achieved on turn %d: all known dictatorships have converted to democracy. Time continues; post-victory turn %d, regression risk is %d%%."), Objective.SoftVictoryTurn, Objective.PostVictoryTurnsElapsed, Objective.RegressionRisk);
            if (Objective.bRegressionWarningActive)
            {
                Objective.ActiveObjectiveNotes.Add(TEXT("Regression risk is elevated. Improve stability, lower unrest, and maintain democratic diplomatic pressure."));
            }
        }
        else
        {
            Objective.bSoftVictoryAchieved = false;
            Objective.SoftVictoryTurn = 0;
            Objective.PostVictoryTurnsElapsed = 0;
            Objective.bPostVictoryContinuationActive = false;
            Objective.bRegressionMonitoringActive = false;
            Objective.RegressionRisk = FMath::Clamp(State.PlayerCountry.Unrest / 5 + FMath::Max(0, 45 - State.PlayerCountry.DiplomaticStanding) / 4, 0, 100);
            Objective.bRegressionWarningActive = Objective.RegressionRisk >= 50;
            Objective.ObjectiveHooks.Add(TEXT("conversion_progress_tracking"));
            Objective.ObjectiveSummary = FString::Printf(TEXT("Single-player objective: convert %d dictatorship%s to democracy. Democratic countries: %d/%d (%d%%). Other governments: %d."), DictatorshipCount, DictatorshipCount == 1 ? TEXT("") : TEXT("s"), DemocraticCount, Objective.TotalTrackedCountryCount, Objective.DemocracyConversionProgress, OtherCount);
            Objective.ActiveObjectiveNotes.Add(TEXT("Use diplomacy, policy credibility, advisor meetings, press releases, and crisis management to move authoritarian states toward democracy."));
        }
    }
    void InitializeEarlyGameBalanceTestData(FDemocracySimulationState& State)
    {
        FDemocracyPolicyState& Policies = State.PlayerCountry.Policies;
        if (Policies.EconomicPolicy.IsEmpty()) { Policies.EconomicPolicy = TEXT("Balanced Budget"); }
        if (Policies.EnvironmentalPolicy.IsEmpty()) { Policies.EnvironmentalPolicy = TEXT("Managed Development"); }
        if (Policies.MilitaryPolicy.IsEmpty()) { Policies.MilitaryPolicy = TEXT("Defensive Readiness"); }
        if (Policies.DiplomacyPolicy.IsEmpty()) { Policies.DiplomacyPolicy = TEXT("Neutral Engagement"); }
        if (Policies.CivilPolicy.IsEmpty()) { Policies.CivilPolicy = TEXT("Public Stability"); }
        BuildPolicyModifiers(Policies, &Policies.ActivePolicyEffects);
        RefreshPolicyRules(State);

        if (State.RealTimeTickSeconds <= 1.0f)
        {
            State.RealTimeTickSeconds = 5.0f;
        }

        InitializeDecisionHistoryIfMissing(State);
        InitializePressOfficeIfMissing(State);
        InitializeMeetingSystemIfMissing(State);
        InitializeDevelopmentSystemIfMissing(State);
        SeedBalanceTestDemographics(State);
        SeedBalanceTestResourceChains(State);
        SeedBalanceTestDepartments(State);
        RecalculateDepartments(State);
        RecalculateDemographics(State);
        RecalculateApprovalStability(State);
        SeedBalanceTestEvents(State);
        InitializeDiplomacyMatrixIfMissing(State);
        RefreshObjectiveState(State, TEXT("SinglePlayer"));

        State.AdvisorSystem.GuidanceLevel = AdvisorGuidanceForDifficultyScore(State.PlayerCountry.CountrySizeScore);
        State.AdvisorSystem.LastUpdatedTurn = State.Turn;
        State.AdvisorSystem.Reports = GenerateAdvisorReports(State);
        if (State.AdvisorSystem.AdvisorCount <= 0)
        {
            State.AdvisorSystem.AdvisorCount = FMath::Clamp(6 - State.PlayerCountry.CountrySizeScore, 1, 5);
        }
    }
BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void ALoginHUD::BeginPlay()
{
    Super::BeginPlay();

    BackgroundBrush = MakeShared<FSlateImageBrush>(
        FPaths::ProjectContentDir() / TEXT("UI/Login/Office_Login_Background.png"),
        FVector2D(1680.0f, 945.0f));
    const FString WorldMapImagePath = FPaths::ProjectDir() / TEXT("Generated/Dulia_World_Government_Map.png");
    WorldMapBrush = MakeShared<FSlateDynamicImageBrush>(FName(*WorldMapImagePath), FVector2D(2242.0f, 1104.0f));
    const FString RtsLandMapImagePath = FPaths::ProjectDir() / TEXT("Generated/Dulia_World_Government_LandOverlay.png");
    RtsLandMapBrush = MakeShared<FSlateDynamicImageBrush>(FName(*RtsLandMapImagePath), FVector2D(2242.0f, 1104.0f));
    RtsWaterBrush = MakeShared<FSlateColorBrush>(FLinearColor(0.274f, 0.612f, 0.690f, 1.0f));
    OverlayBrush = MakeShared<FSlateColorBrush>(FLinearColor(0.0f, 0.0f, 0.0f, 0.24f));
    PanelBrush = MakeShared<FSlateColorBrush>(FLinearColor(0.025f, 0.028f, 0.032f, 0.88f));
    RowBrush = MakeShared<FSlateColorBrush>(FLinearColor(0.06f, 0.065f, 0.075f, 0.80f));

    LoginButtonStyle = MakeShared<FButtonStyle>();
    *LoginButtonStyle = FButtonStyle()
        .SetNormal(FSlateColorBrush(FLinearColor(0.05f, 0.06f, 0.07f, 0.88f)))
        .SetHovered(FSlateColorBrush(FLinearColor(0.14f, 0.16f, 0.19f, 0.94f)))
        .SetPressed(FSlateColorBrush(FLinearColor(0.02f, 0.03f, 0.04f, 0.98f)))
        .SetDisabled(FSlateColorBrush(FLinearColor(0.025f, 0.028f, 0.032f, 0.54f)))
        .SetNormalPadding(FMargin(8.0f, 4.0f))
        .SetPressedPadding(FMargin(8.0f, 5.0f, 8.0f, 3.0f));

    LoadRememberedLoginDetails();

    SettingsMenuWidgetClass = LoadClass<UUserWidget>(
        nullptr,
        TEXT("/Game/SpireSystems/SettingsMenuKit/Widgets/WBP_SettingsMenu.WBP_SettingsMenu_C"));
    if (!SettingsMenuWidgetClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("Settings Menu Kit widget was not found. Falling back to Slate settings screen."));
    }

    RefreshLoginWidget();
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void ALoginHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    TearDownLoginWidget();
    StopSimulationTimer();

    LoginButtonStyle.Reset();
    RowBrush.Reset();
    PanelBrush.Reset();
    OverlayBrush.Reset();
    RtsWaterBrush.Reset();
    SetupCountryMapBrush.Reset();
    RtsLandMapBrush.Reset();
    WorldMapBrush.Reset();
    BackgroundBrush.Reset();

    Super::EndPlay(EndPlayReason);
}

bool ALoginHUD::IsSinglePlayerDebugContext() const
{
#if UE_BUILD_SHIPPING
    return false;
#else
    if (CurrentScreen == ELoginFlowScreen::MultiplayerStateSelection || CurrentScreen == ELoginFlowScreen::ServerSelection)
    {
        return false;
    }

    if (bHasLoadedRuntimeState)
    {
        return LoadedSaveState.Mode.IsEmpty() || LoadedSaveState.Mode.Equals(TEXT("SinglePlayer"), ESearchCase::IgnoreCase);
    }

    return true;
#endif
}

bool ALoginHUD::HasGameMasterDebugAccess() const
{
#if UE_BUILD_SHIPPING
    return false;
#else
    return IsSinglePlayerDebugContext()
        && (DebugToolRole.Equals(TEXT("GameMaster"), ESearchCase::IgnoreCase)
            || DebugToolRole.Equals(TEXT("Administrator"), ESearchCase::IgnoreCase));
#endif
}

bool ALoginHUD::HasAdministratorDebugAccess() const
{
#if UE_BUILD_SHIPPING
    return false;
#else
    return IsSinglePlayerDebugContext() && DebugToolRole.Equals(TEXT("Administrator"), ESearchCase::IgnoreCase);
#endif
}

FString ALoginHUD::BuildDebugAccessStatusText() const
{
#if UE_BUILD_SHIPPING
    return TEXT("Debug/test tools are unavailable in shipping builds.");
#else
    const FString DebugRoleName = DebugToolRole.IsEmpty() ? TEXT("Player") : DebugToolRole;
    if (!IsSinglePlayerDebugContext())
    {
        return FString::Printf(TEXT("Role %s blocked: debug/test tools are disabled in multiplayer and online-state flows."), *DebugRoleName);
    }
    if (HasAdministratorDebugAccess())
    {
        return TEXT("Role Administrator: full developer-only access. All test and mutation tools are enabled for single-player development saves.");
    }
    if (HasGameMasterDebugAccess())
    {
        return TEXT("Role GameMaster: limited test access. Can run recovery/scenario/event/tick tools, but cannot force major state changes.");
    }
    return TEXT("Role Player: no debug/test tool access. Use console command DemocracySetDebugRole GameMaster or DemocracySetDebugRole Administrator in single-player development builds.");
#endif
}

void ALoginHUD::DemocracySetDebugRole(const FString& RoleName)
{
#if UE_BUILD_SHIPPING
    LastSaveStatus = TEXT("Debug role command unavailable in shipping builds.");
    RefreshLoginWidget();
#else
    const FString RequestedRole = RoleName.TrimStartAndEnd();
    if (!IsSinglePlayerDebugContext())
    {
        DebugToolRole = TEXT("Player");
        LastSaveStatus = TEXT("Debug role denied: developer tools are never available in multiplayer or online-state flows.");
        RefreshLoginWidget();
        return;
    }

    if (RequestedRole.Equals(TEXT("GameMaster"), ESearchCase::IgnoreCase) || RequestedRole.Equals(TEXT("GM"), ESearchCase::IgnoreCase))
    {
        DebugToolRole = TEXT("GameMaster");
    }
    else if (RequestedRole.Equals(TEXT("Administrator"), ESearchCase::IgnoreCase) || RequestedRole.Equals(TEXT("Admin"), ESearchCase::IgnoreCase))
    {
        DebugToolRole = TEXT("Administrator");
    }
    else
    {
        DebugToolRole = TEXT("Player");
    }

    LastSaveStatus = FString::Printf(TEXT("Debug role set to %s. %s"), *DebugToolRole, *BuildDebugAccessStatusText());
    RefreshLoginWidget();
#endif
}

void ALoginHUD::DemocracyClearDebugRole()
{
    DebugToolRole = TEXT("Player");
    LastSaveStatus = TEXT("Debug role cleared. Player role has no debug/test tool access.");
    RefreshLoginWidget();
}
void ALoginHUD::ShowScreen(ELoginFlowScreen Screen)
{
    CurrentScreen = Screen;
    RefreshLoginWidget();
}

void ALoginHUD::RefreshLoginWidget()
{
    if (!GEngine || !GEngine->GameViewport)
    {
        return;
    }

    TearDownLoginWidget();

    if (bInOfficeMode && CurrentScreen == ELoginFlowScreen::OfficeNoOverlay)
    {
        return;
    }

    if (bInOfficeMode && CurrentScreen == ELoginFlowScreen::OfficeWorldRts && WorldRtsEntryMode.Equals(TEXT("RtsEntry"), ESearchCase::IgnoreCase))
    {
        LoginScreenWidget =
            SNew(SOverlay)
            + SOverlay::Slot()
            [
                BuildCurrentScreen()
            ];
    }
    else if (bInOfficeMode && (CurrentScreen == ELoginFlowScreen::OfficeOpeningBriefing || CurrentScreen == ELoginFlowScreen::OfficeDashboard || CurrentScreen == ELoginFlowScreen::OfficeComputerMenu || CurrentScreen == ELoginFlowScreen::OfficePolicies || CurrentScreen == ELoginFlowScreen::OfficeEvents || CurrentScreen == ELoginFlowScreen::OfficeDemographics || CurrentScreen == ELoginFlowScreen::OfficeBudget || CurrentScreen == ELoginFlowScreen::OfficeResourceChains || CurrentScreen == ELoginFlowScreen::OfficeDepartments || CurrentScreen == ELoginFlowScreen::OfficeDevelopment || CurrentScreen == ELoginFlowScreen::OfficeApprovalStability || CurrentScreen == ELoginFlowScreen::OfficeDecisionHistory || CurrentScreen == ELoginFlowScreen::OfficeWorldRts || CurrentScreen == ELoginFlowScreen::OfficeAdvisorWarnings || CurrentScreen == ELoginFlowScreen::OfficeMeetingAdvisor || CurrentScreen == ELoginFlowScreen::OfficePressRelease))
    {
        LoginScreenWidget =
            SNew(SOverlay)
            + SOverlay::Slot()
            .HAlign(HAlign_Right)
            .VAlign(VAlign_Center)
            .Padding(FMargin(0.0f, 0.0f, 36.0f, 0.0f))
            [
                BuildCurrentScreen()
            ];
    }
    else
    {
        LoginScreenWidget =
            SNew(SOverlay)
            + SOverlay::Slot()
            [
                SNew(SImage)
                .Image(BackgroundBrush.Get())
            ]
            + SOverlay::Slot()
            [
                SNew(SBorder)
                .BorderImage(OverlayBrush.Get())
            ]
            + SOverlay::Slot()
            .HAlign(HAlign_Center)
            .VAlign(VAlign_Center)
            [
                BuildCurrentScreen()
            ];
    }

    GEngine->GameViewport->AddViewportWidgetContent(LoginScreenWidget.ToSharedRef(), 100);
}

void ALoginHUD::TearDownLoginWidget()
{
    if (GEngine && GEngine->GameViewport && LoginScreenWidget.IsValid())
    {
        GEngine->GameViewport->RemoveViewportWidgetContent(LoginScreenWidget.ToSharedRef());
    }

    LoginScreenWidget.Reset();
}

void ALoginHUD::HandleOfficeInteractable(const FString& InteractionName)
{
    APlayerController* PlayerController = GetOwningPlayerController();

    if (InteractionName.Equals(TEXT("Computer"), ESearchCase::IgnoreCase))
    {
        bInOfficeMode = true;
        ShowScreen(ELoginFlowScreen::OfficeComputerMenu);
        if (PlayerController)
        {
            PlayerController->bShowMouseCursor = true;
            FInputModeGameAndUI InputMode;
            PlayerController->SetInputMode(InputMode);
        }
        return;
    }

    if (InteractionName.Equals(TEXT("Globe"), ESearchCase::IgnoreCase))
    {
        bInOfficeMode = true;
        WorldRtsEntryMode = TEXT("Globe");
        ShowScreen(ELoginFlowScreen::OfficeWorldRts);
        if (PlayerController)
        {
            PlayerController->bShowMouseCursor = true;
            FInputModeGameAndUI InputMode;
            PlayerController->SetInputMode(InputMode);
        }
        return;
    }

    if (InteractionName.Equals(TEXT("Phone"), ESearchCase::IgnoreCase))
    {
        bInOfficeMode = true;
        ShowScreen(ELoginFlowScreen::OfficeAdvisorWarnings);
        if (PlayerController)
        {
            PlayerController->bShowMouseCursor = true;
            FInputModeGameAndUI InputMode;
            PlayerController->SetInputMode(InputMode);
        }
        return;
    }

    if (InteractionName.Equals(TEXT("BriefingFolder"), ESearchCase::IgnoreCase))
    {
        bInOfficeMode = true;
        bShowFirstLoginBriefing = false;
        ShowScreen(ELoginFlowScreen::OfficeOpeningBriefing);
        if (PlayerController)
        {
            PlayerController->bShowMouseCursor = true;
            FInputModeGameAndUI InputMode;
            PlayerController->SetInputMode(InputMode);
        }
        return;
    }
    if (InteractionName.Equals(TEXT("Lamp"), ESearchCase::IgnoreCase))
    {
        if (UWorld* World = GetWorld())
        {
            for (TActorIterator<APointLight> It(World); It; ++It)
            {
                APointLight* PointLight = *It;
                if (PointLight && PointLight->GetActorLabel().Equals(TEXT("Desk Lamp Toggle Light"), ESearchCase::IgnoreCase) && PointLight->PointLightComponent)
                {
                    const bool bTurnOn = PointLight->PointLightComponent->Intensity <= 1.0f;
                    PointLight->PointLightComponent->SetIntensity(bTurnOn ? 4200.0f : 0.0f);
                    if (GEngine)
                    {
                        GEngine->AddOnScreenDebugMessage(-1, 2.0f, bTurnOn ? FColor::Yellow : FColor::Silver, bTurnOn ? TEXT("Desk lamp on.") : TEXT("Desk lamp off."));
                    }
                    return;
                }
            }
        }
        UE_LOG(LogTemp, Warning, TEXT("Desk lamp toggle light was not found."));
        return;
    }

    if (InteractionName.Equals(TEXT("Door"), ESearchCase::IgnoreCase))
    {
        TearDownLoginWidget();
        bInOfficeMode = true;
        if (PlayerController)
        {
            APawn* ControlledPawn = PlayerController->GetPawn();
            if (ControlledPawn)
            {
                ControlledPawn->SetActorLocationAndRotation(FVector(0.0f, -1030.0f, 95.0f), FRotator(0.0f, -90.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
                PlayerController->SetViewTarget(ControlledPawn);
            }
            PlayerController->bShowMouseCursor = false;
            FInputModeGameOnly InputMode;
            PlayerController->SetInputMode(InputMode);
        }
        return;
    }

    if (InteractionName.Equals(TEXT("HallwayReturn"), ESearchCase::IgnoreCase))
    {
        TearDownLoginWidget();
        bInOfficeMode = true;
        if (PlayerController)
        {
            APawn* ControlledPawn = PlayerController->GetPawn();
            if (ControlledPawn)
            {
                ControlledPawn->SetActorLocationAndRotation(FVector(0.0f, -455.0f, 95.0f), FRotator(0.0f, 90.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
                PlayerController->SetViewTarget(ControlledPawn);
            }
            PlayerController->bShowMouseCursor = false;
            FInputModeGameOnly InputMode;
            PlayerController->SetInputMode(InputMode);
        }
        return;
    }

    if (InteractionName.Equals(TEXT("MeetingRoomDoor"), ESearchCase::IgnoreCase))
    {
        TearDownLoginWidget();
        bInOfficeMode = true;
        if (PlayerController)
        {
            APawn* ControlledPawn = PlayerController->GetPawn();
            if (ControlledPawn)
            {
                ControlledPawn->SetActorLocationAndRotation(FVector(-520.0f, -2760.0f, 95.0f), FRotator(0.0f, -90.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
                PlayerController->SetViewTarget(ControlledPawn);
            }
            PlayerController->bShowMouseCursor = false;
            FInputModeGameOnly InputMode;
            PlayerController->SetInputMode(InputMode);
        }
        return;
    }

    if (InteractionName.StartsWith(TEXT("MeetingAdvisor_")))
    {
        bInOfficeMode = true;
        if (InteractionName.Equals(TEXT("MeetingAdvisor_Resources"), ESearchCase::IgnoreCase))
        {
            SelectedMeetingAdvisorName = TEXT("Resource Manager");
            SelectedMeetingAdvisorFocus = TEXT("Food, gas/oil, wood, metals, stockpile pressure, shortage relief, and trade dependency.");
        }
        else if (InteractionName.Equals(TEXT("MeetingAdvisor_Military"), ESearchCase::IgnoreCase))
        {
            SelectedMeetingAdvisorName = TEXT("Military Advisor");
            SelectedMeetingAdvisorFocus = TEXT("Military readiness, invasion risk, border defense, force posture, and emergency mobilization.");
        }
        else if (InteractionName.Equals(TEXT("MeetingAdvisor_Social"), ESearchCase::IgnoreCase))
        {
            SelectedMeetingAdvisorName = TEXT("Social Advisor");
            SelectedMeetingAdvisorFocus = TEXT("Approval, unrest, stability, public trust, social policy impact, and assassination risk warnings.");
        }
        else if (InteractionName.Equals(TEXT("MeetingAdvisor_Economy"), ESearchCase::IgnoreCase))
        {
            SelectedMeetingAdvisorName = TEXT("Economic Advisor");
            SelectedMeetingAdvisorFocus = TEXT("Budget health, production, inflation placeholders, taxation, public spending, and economic shocks.");
        }
        else if (InteractionName.Equals(TEXT("MeetingAdvisor_Diplomacy"), ESearchCase::IgnoreCase))
        {
            SelectedMeetingAdvisorName = TEXT("Diplomacy Advisor");
            SelectedMeetingAdvisorFocus = TEXT("Alliances, rival countries, negotiations, foreign official meetings, treaties, and diplomatic pressure.");
        }
        else if (InteractionName.Equals(TEXT("MeetingAdvisor_Infrastructure"), ESearchCase::IgnoreCase))
        {
            SelectedMeetingAdvisorName = TEXT("Infrastructure Advisor");
            SelectedMeetingAdvisorFocus = TEXT("Roads, power, communications, logistics, construction capacity, and national resilience.");
        }
        else if (InteractionName.Equals(TEXT("MeetingAdvisor_Intelligence"), ESearchCase::IgnoreCase))
        {
            SelectedMeetingAdvisorName = TEXT("Security Advisor");
            SelectedMeetingAdvisorFocus = TEXT("Internal threats, coup risk, assassination plots, counterintelligence, and hostile foreign action.");
        }
        else if (InteractionName.Equals(TEXT("MeetingAdvisor_Welfare"), ESearchCase::IgnoreCase))
        {
            SelectedMeetingAdvisorName = TEXT("Public Welfare Advisor");
            SelectedMeetingAdvisorFocus = TEXT("Health, education, housing placeholders, disaster response, and quality-of-life effects.");
        }
        else
        {
            SelectedMeetingAdvisorName = TEXT("Meeting Advisor");
            SelectedMeetingAdvisorFocus = TEXT("General advisor placeholder for future meeting logic.");
        }
        ShowScreen(ELoginFlowScreen::OfficeMeetingAdvisor);
        if (PlayerController)
        {
            PlayerController->bShowMouseCursor = true;
            FInputModeGameAndUI InputMode;
            PlayerController->SetInputMode(InputMode);
        }
        return;
    }
    if (InteractionName.Equals(TEXT("MeetingRoomReturn"), ESearchCase::IgnoreCase))
    {
        TearDownLoginWidget();
        bInOfficeMode = true;
        if (PlayerController)
        {
            APawn* ControlledPawn = PlayerController->GetPawn();
            if (ControlledPawn)
            {
                ControlledPawn->SetActorLocationAndRotation(FVector(-520.0f, -2185.0f, 95.0f), FRotator(0.0f, 90.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
                PlayerController->SetViewTarget(ControlledPawn);
            }
            PlayerController->bShowMouseCursor = false;
            FInputModeGameOnly InputMode;
            PlayerController->SetInputMode(InputMode);
        }
        return;
    }


    if (InteractionName.Equals(TEXT("PressRoomDoor"), ESearchCase::IgnoreCase))
    {
        TearDownLoginWidget();
        bInOfficeMode = true;
        if (PlayerController)
        {
            APawn* ControlledPawn = PlayerController->GetPawn();
            if (ControlledPawn)
            {
                ControlledPawn->SetActorLocationAndRotation(FVector(1680.0f, -2745.0f, 95.0f), FRotator(0.0f, -90.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
                PlayerController->SetViewTarget(ControlledPawn);
            }
            PlayerController->bShowMouseCursor = false;
            FInputModeGameOnly InputMode;
            PlayerController->SetInputMode(InputMode);
        }
        return;
    }

    if (InteractionName.Equals(TEXT("PressRoomReturn"), ESearchCase::IgnoreCase))
    {
        TearDownLoginWidget();
        bInOfficeMode = true;
        if (PlayerController)
        {
            APawn* ControlledPawn = PlayerController->GetPawn();
            if (ControlledPawn)
            {
                ControlledPawn->SetActorLocationAndRotation(FVector(520.0f, -2185.0f, 95.0f), FRotator(0.0f, 90.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
                PlayerController->SetViewTarget(ControlledPawn);
            }
            PlayerController->bShowMouseCursor = false;
            FInputModeGameOnly InputMode;
            PlayerController->SetInputMode(InputMode);
        }
        return;
    }

    if (InteractionName.Equals(TEXT("PressPodium"), ESearchCase::IgnoreCase))
    {
        bInOfficeMode = true;
        ShowScreen(ELoginFlowScreen::OfficePressRelease);
        if (PlayerController)
        {
            PlayerController->bShowMouseCursor = true;
            FInputModeGameAndUI InputMode;
            PlayerController->SetInputMode(InputMode);
        }
        return;
    }
    if (InteractionName.Equals(TEXT("HallwaySideDoor"), ESearchCase::IgnoreCase))
    {
        bInOfficeMode = true;
        WorldRtsEntryMode = TEXT("RtsEntry");
        RtsMapZoom = 0.88f;
        RtsMapPan = FVector2D::ZeroVector;
        bIsDraggingRtsMap = false;
        bRtsMapDragMoved = false;

        if (bHasLoadedRuntimeState)
        {
            FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
            RtsSelectedCountryName = State.PlayerCountry.CountryName;
            RtsSelectedProvinceId.Empty();
            RtsSelectedArmyId.Empty();
            for (const FDemocracyCountryOwnershipState& Country : State.RtsWorld.Ownership.Countries)
            {
                if (Country.bPlayerCountry || Country.CountryName.Equals(State.PlayerCountry.CountryName, ESearchCase::IgnoreCase))
                {
                    RtsSelectedCountryName = Country.CountryName;
                    RtsSelectedProvinceId = Country.ProvinceIds.Num() > 0 ? Country.ProvinceIds[0] : TEXT("");
                    State.RtsWorld.WorldInteraction.ActiveSelectionId = RtsSelectedProvinceId.IsEmpty() ? Country.CountryId : RtsSelectedProvinceId;
                    State.RtsWorld.WorldInteraction.ActiveSelectionType = RtsSelectedProvinceId.IsEmpty() ? TEXT("Country") : TEXT("Province");
                    State.RtsWorld.WorldInteraction.LastInteractionSummary = TEXT("Entered RTS command mode centered on Planet Dulia. Select your country to enter country/base command view.");
                    break;
                }
            }
            State.RtsWorld.ActiveViewMode = TEXT("World View");
            RefreshRtsHudState(State);
        }

        ShowScreen(ELoginFlowScreen::OfficeWorldRts);
        if (PlayerController)
        {
            PlayerController->bShowMouseCursor = true;
            FInputModeGameAndUI InputMode;
            PlayerController->SetInputMode(InputMode);
        }
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("Unhandled office interactable: %s"), *InteractionName);
}

TSharedRef<SWidget> ALoginHUD::BuildCurrentScreen()
{
    switch (CurrentScreen)
    {
    case ELoginFlowScreen::Settings:
        return BuildSettingsScreen();
    case ELoginFlowScreen::GameModeSelection:
        return BuildGameModeSelectionScreen();
    case ELoginFlowScreen::LocalSaveSelection:
        return BuildLocalSaveSelectionScreen();
    case ELoginFlowScreen::DifficultySelection:
        return BuildDifficultySelectionScreen();
    case ELoginFlowScreen::NewStateSetup:
        return BuildNewStateSetupScreen();
    case ELoginFlowScreen::LoadedGame:
        return BuildLoadedGameScreen();
    case ELoginFlowScreen::MultiplayerStateSelection:
        return BuildMultiplayerStateSelectionScreen();
    case ELoginFlowScreen::ServerSelection:
        return BuildServerSelectionScreen();
    case ELoginFlowScreen::OfficeOpeningBriefing:
        return BuildOfficeOpeningBriefingScreen();
    case ELoginFlowScreen::OfficeDashboard:
        return BuildOfficeDashboardScreen();
    case ELoginFlowScreen::OfficeComputerMenu:
        return BuildOfficeComputerMenuScreen();
    case ELoginFlowScreen::OfficePolicies:
        return BuildOfficePoliciesScreen();
    case ELoginFlowScreen::OfficeEvents:
        return BuildOfficeEventsScreen();
    case ELoginFlowScreen::OfficeDemographics:
        return BuildOfficeDemographicsScreen();
    case ELoginFlowScreen::OfficeBudget:
        return BuildOfficeBudgetScreen();
    case ELoginFlowScreen::OfficeResourceChains:
        return BuildOfficeResourceChainsScreen();
    case ELoginFlowScreen::OfficeDepartments:
        return BuildOfficeDepartmentsScreen();
    case ELoginFlowScreen::OfficeDevelopment:
        return BuildOfficeDevelopmentScreen();
    case ELoginFlowScreen::OfficeApprovalStability:
        return BuildOfficeApprovalStabilityScreen();
    case ELoginFlowScreen::OfficeDecisionHistory:
        return BuildOfficeDecisionHistoryScreen();
    case ELoginFlowScreen::OfficeWorldRts:
        return BuildOfficeWorldRtsScreen();
    case ELoginFlowScreen::OfficeAdvisorWarnings:
        return BuildOfficeAdvisorWarningsScreen();
    case ELoginFlowScreen::OfficeMeetingAdvisor:
        return BuildOfficeMeetingAdvisorScreen();
    case ELoginFlowScreen::OfficePressRelease:
        return BuildOfficePressReleaseScreen();
    case ELoginFlowScreen::GameOver:
        return BuildGameOverScreen();
    case ELoginFlowScreen::Login:
    default:
        return BuildLoginScreen();
    }
}

TSharedRef<SWidget> ALoginHUD::BuildLoginScreen()
{
    return BuildPanel(TEXT("Democracy"), TEXT("Mock account form. Authentication is not connected yet."),
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [
            SNew(SEditableTextBox)
            .HintText(BodyText(TEXT("Username or email")))
            .Text(BodyText(MockUserName))
            .OnTextChanged(FOnTextChanged::CreateUObject(this, &ALoginHUD::HandleMockUserNameChanged))
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)
        [
            SNew(SEditableTextBox)
            .HintText(BodyText(TEXT("Password")))
            .Text(BodyText(MockPassword))
            .IsPassword(true)
            .OnTextChanged(FOnTextChanged::CreateUObject(this, &ALoginHUD::HandleMockPasswordChanged))
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)
        [
            SNew(SCheckBox)
            .IsChecked(bRememberLoginDetails ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
            .OnCheckStateChanged(FOnCheckStateChanged::CreateUObject(this, &ALoginHUD::HandleRememberLoginDetailsChanged))
            [
                SNew(STextBlock)
                .Text(BodyText(TEXT("Remember login details")))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 15))
                .ColorAndOpacity(FLinearColor::White)
            ]
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 4.0f)
        [BuildButton(TEXT("Sign In"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSignInClicked))]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f)
        [BuildButton(TEXT("Sign Up"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSignUpClicked))]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f)
        [BuildButton(TEXT("Settings"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSettingsClicked))]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f)
        [BuildButton(TEXT("Exit"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleExitClicked))]);
}

TSharedRef<SWidget> ALoginHUD::BuildSettingsScreen()
{
    if (SettingsMenuWidgetClass)
    {
        return BuildSettingsMenuProScreen();
    }
    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);

    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildInfoRow(TEXT("Visual"), TEXT("Common display settings. Values are UI placeholders and are not persisted yet."))];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildCheckRow(TEXT("Fullscreen"), TEXT("Toggle fullscreen/windowed preference."), bFullscreen, FOnCheckStateChanged::CreateUObject(this, &ALoginHUD::HandleFullscreenChanged))];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildCheckRow(TEXT("VSync"), TEXT("Toggle vertical sync preference."), bVSync, FOnCheckStateChanged::CreateUObject(this, &ALoginHUD::HandleVSyncChanged))];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildSliderRow(TEXT("Brightness"), Brightness, FOnFloatValueChanged::CreateUObject(this, &ALoginHUD::HandleBrightnessChanged))];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildSliderRow(TEXT("UI Scale"), UiScale, FOnFloatValueChanged::CreateUObject(this, &ALoginHUD::HandleUiScaleChanged))];

    Body->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 4.0f)
    [BuildInfoRow(TEXT("Audio"), TEXT("Common audio mix settings. Hook these into game user settings/audio bus later."))];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildSliderRow(TEXT("Master Volume"), MasterVolume, FOnFloatValueChanged::CreateUObject(this, &ALoginHUD::HandleMasterVolumeChanged))];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildSliderRow(TEXT("Music Volume"), MusicVolume, FOnFloatValueChanged::CreateUObject(this, &ALoginHUD::HandleMusicVolumeChanged))];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildSliderRow(TEXT("Effects Volume"), EffectsVolume, FOnFloatValueChanged::CreateUObject(this, &ALoginHUD::HandleEffectsVolumeChanged))];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildSliderRow(TEXT("Voice Volume"), VoiceVolume, FOnFloatValueChanged::CreateUObject(this, &ALoginHUD::HandleVoiceVolumeChanged))];

    Body->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 4.0f)
    [BuildInfoRow(TEXT("Controls"), TEXT("Keybind rows are placeholders until input mapping capture is connected."))];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildKeybindRow(TEXT("Move Camera Forward"), TEXT("W"))];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildKeybindRow(TEXT("Move Camera Back"), TEXT("S"))];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildKeybindRow(TEXT("Move Camera Left"), TEXT("A"))];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildKeybindRow(TEXT("Move Camera Right"), TEXT("D"))];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildCheckRow(TEXT("Invert Look Y"), TEXT("Reverse vertical look direction for office camera controls."), bInvertLookY, FOnCheckStateChanged::CreateUObject(this, &ALoginHUD::HandleInvertLookYChanged))];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildKeybindRow(TEXT("Select / Confirm"), TEXT("Left Mouse"))];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildKeybindRow(TEXT("Cancel / Back"), TEXT("Right Mouse"))];
    Body->AddSlot().AutoHeight().Padding(0.0f, 14.0f)
    [BuildBackButton()];

    return BuildPanel(TEXT("Settings"), TEXT("Audio, visual, and control options."),
        SNew(SScrollBox)
        + SScrollBox::Slot()
        [Body], 760.0f);
}

TSharedRef<SWidget> ALoginHUD::BuildSettingsMenuProScreen()
{
    if (!SettingsMenuWidget && SettingsMenuWidgetClass)
    {
        SettingsMenuWidget = CreateWidget<UUserWidget>(GetOwningPlayerController(), SettingsMenuWidgetClass);
    }

    if (!SettingsMenuWidget)
    {
        return BuildPanel(TEXT("Settings"), TEXT("Settings Menu Kit widget could not be created."),
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildInfoRow(TEXT("Widget Missing"), TEXT("The SettingsMenuKit UMG asset is present, but Unreal could not instantiate it."))]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 14.0f)
            [BuildBackButton()], 760.0f);
    }

    return BuildPanel(TEXT("Settings"), TEXT("Settings Menu Kit placeholder wired from imported UI assets."),
        SNew(SVerticalBox)
        + SVerticalBox::Slot().FillHeight(1.0f).Padding(0.0f, 0.0f, 0.0f, 12.0f)
        [
            SNew(SBox)
            .MinDesiredWidth(920.0f)
            .MinDesiredHeight(560.0f)
            [
                SettingsMenuWidget->TakeWidget()
            ]
        ]
        + SVerticalBox::Slot().AutoHeight()
        [BuildBackButton()], 1040.0f);
}
TSharedRef<SWidget> ALoginHUD::BuildGameModeSelectionScreen()
{
    return BuildPanel(TEXT("Select Game Mode"), TEXT("Temporary testing route keeps multiplayer locked."),
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f)
        [BuildButton(TEXT("Single Player"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSinglePlayerClicked))]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f)
        [BuildButton(TEXT("Multiplayer - Disabled"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleMultiplayerClicked), 300.0f, 52.0f, false)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)
        [BuildInfoRow(TEXT("Multiplayer Locked"), TEXT("Visible for layout testing. It will activate when backend login, account states, and server services are ready."))]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 14.0f)
        [BuildBackButton()]);
}

TSharedRef<SWidget> ALoginHUD::BuildLocalSaveSelectionScreen()
{
    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
    TArray<FString> Saves = GetLocalSaveNames();

    if (Saves.IsEmpty())
    {
        Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("No Local Saves Found"), TEXT("Choose a difficulty to create a new single-player state."))];
        Body->AddSlot().AutoHeight().Padding(0.0f, 12.0f)
        [BuildButton(TEXT("Choose Difficulty"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleCreateNewStateClicked), 320.0f, 52.0f)];
        Body->AddSlot().AutoHeight().Padding(0.0f, 14.0f)
        [BuildButton(TEXT("Back"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleBackFromLocalSavesClicked), 180.0f, 44.0f)];

        return BuildPanel(TEXT("New Single Player State"), TEXT("No local saves are present in the game Saves folder."), Body);
    }

    Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
    [BuildButton(TEXT("Create New State"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleCreateNewStateClicked), 320.0f, 50.0f)];

    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [
        SNew(SEditableTextBox)
        .HintText(BodyText(TEXT("Search saved states")))
        .Text(BodyText(LocalSaveSearchText))
        .OnTextChanged(FOnTextChanged::CreateUObject(this, &ALoginHUD::HandleLocalSaveSearchChanged))
    ];

    Body->AddSlot().AutoHeight().Padding(0.0f, 8.0f)
    [
        SNew(SCheckBox)
        .IsChecked(bShowRecentLocalSavesOnly ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
        .OnCheckStateChanged(FOnCheckStateChanged::CreateUObject(this, &ALoginHUD::HandleRecentLocalSavesChanged))
        [
            SNew(STextBlock)
            .Text(BodyText(TEXT("Recent saves only")))
            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 16))
            .ColorAndOpacity(FLinearColor::White)
        ]
    ];

    TSharedRef<SScrollBox> SaveList = SNew(SScrollBox);
    const FString SearchLower = LocalSaveSearchText.ToLower();
    int32 VisibleSaves = 0;

    for (int32 SaveIndex = 0; SaveIndex < Saves.Num(); ++SaveIndex)
    {
        const FString& SaveName = Saves[SaveIndex];
        const bool bMatchesSearch = SearchLower.IsEmpty() || SaveName.ToLower().Contains(SearchLower);
        const bool bMatchesRecent = !bShowRecentLocalSavesOnly || SaveIndex < 5;

        if (!bMatchesSearch || !bMatchesRecent)
        {
            continue;
        }

        ++VisibleSaves;
        const FString Detail = FString::Printf(TEXT("Local file in %s. Placeholder list uses search and filter behavior like server selection."), *(FPaths::ProjectDir() / TEXT("Saves")));

        SaveList->AddSlot().Padding(0.0f, 4.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 8.0f, 0.0f)
                [BuildButton(SaveName, FOnClicked::CreateUObject(this, &ALoginHUD::HandleSelectLocalSave, SaveName), 400.0f, 46.0f)]
                + SHorizontalBox::Slot().AutoWidth()
                [BuildButton(TEXT("Delete"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleDeleteLocalSave, SaveName), 110.0f, 46.0f)]
            ]
            + SVerticalBox::Slot().AutoHeight()
            [BuildInfoRow(TEXT("Saved State"), Detail)]
        ];
    }

    if (VisibleSaves == 0)
    {
        SaveList->AddSlot().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("No Saves Found"), TEXT("Adjust search or filters, or create a new state."))];
    }

    Body->AddSlot().MaxHeight(310.0f).Padding(0.0f, 8.0f)
    [SaveList];
    Body->AddSlot().AutoHeight().Padding(0.0f, 12.0f)
    [BuildInfoRow(TEXT("Local Save Rule"), TEXT("Single-player saves are read from the game directory Saves folder."))];
    Body->AddSlot().AutoHeight().Padding(0.0f, 14.0f)
    [BuildButton(TEXT("Back"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleBackFromLocalSavesClicked), 180.0f, 44.0f)];

    return BuildPanel(TEXT("Single Player Saves"), TEXT("Select a saved state or create a new one."), Body);
}

TSharedRef<SWidget> ALoginHUD::BuildDifficultySelectionScreen()
{
    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);

    for (const FString& DifficultyName : FDifficultyProfileLibrary::GetDifficultyNames())
    {
        const FDifficultyProfile Profile = FDifficultyProfileLibrary::GetProfile(DifficultyName);
        Body->AddSlot().AutoHeight().Padding(0.0f, 6.0f)
        [BuildButton(DifficultyName, FOnClicked::CreateUObject(this, &ALoginHUD::HandleSelectDifficulty, DifficultyName), 320.0f, 50.0f)];
        Body->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
        [BuildInfoRow(DifficultyName + TEXT(" Profile"), Profile.ToSummaryText())];
        Body->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 8.0f)
        [BuildInfoRow(TEXT("Guidance"), DifficultyGuidancePreview(Profile))];
    }

    Body->AddSlot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 4.0f)
    [BuildInfoRow(TEXT("Testing"), TEXT("Difficulty values are loaded from Config/DifficultyProfiles.ini."))];
    Body->AddSlot().AutoHeight().Padding(0.0f, 14.0f)
    [BuildButton(TEXT("Back"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleBackToLocalSavesClicked), 180.0f, 44.0f)];

    return BuildPanel(TEXT("Select Difficulty"), TEXT("Harder profiles increase country size, reduce support, and broaden but reduce starting resources."),
        SNew(SScrollBox)
        + SScrollBox::Slot()
        [Body], 780.0f);
}

TArray<FDemocracyGeneratedCountryState> ALoginHUD::BuildStartingCountryOptions() const
{
    TArray<FDemocracyGeneratedCountryState> Options;
    if (PendingDifficulty.IsEmpty())
    {
        return Options;
    }

    const FDifficultyProfile DifficultyProfile = FDifficultyProfileLibrary::GetProfile(PendingDifficulty);
    const FString PreviewClimate = PendingClimate.IsEmpty() ? TEXT("Middle Moderate") : PendingClimate;
    const FDemocracyWorldMapState PreviewMap = FDemocracyGameStateFactory::BuildStartingCountryPreviewMap(PreviewClimate, DifficultyProfile);
    const FString SearchLower = PendingStartingCountrySearchText.TrimStartAndEnd().ToLower();

    for (const FDemocracyContinentState& Continent : PreviewMap.Continents)
    {
        for (const FDemocracyGeneratedCountryState& Country : Continent.Countries)
        {
            const FString Searchable = FString::Printf(TEXT("%s %s %s %s %s"), *Country.CountryName, *Country.CountryId, *Country.ContinentName, *Country.Climate, *Country.PoliticalType).ToLower();
            if (SearchLower.IsEmpty() || Searchable.Contains(SearchLower))
            {
                Options.Add(Country);
            }
        }
    }

    return Options;
}

TSharedRef<SWidget> ALoginHUD::BuildStartingCountrySelectionWidget()
{
    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);

    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildInfoRow(TEXT("Starting Country"), PendingStartingCountryMapIndex > 0
        ? FString::Printf(TEXT("%s | Map slot %03d"), *PendingStartingCountryName, PendingStartingCountryMapIndex)
        : TEXT("Choose where your country starts on Planet Dulia. This location becomes the player country."))];

    Body->AddSlot().AutoHeight().Padding(0.0f, 6.0f)
    [
        SNew(SEditableTextBox)
        .HintText(BodyText(TEXT("Search country, continent, climate, or government")))
        .Text(BodyText(PendingStartingCountrySearchText))
        .OnTextChanged(FOnTextChanged::CreateUObject(this, &ALoginHUD::HandleStartingCountrySearchChanged))
    ];

    const TArray<FDemocracyGeneratedCountryState> Options = BuildStartingCountryOptions();
    FString SelectedDescription = PendingStartingCountryMapIndex > 0
        ? FString::Printf(TEXT("%s is selected. Map slot %03d. Use search to compare other Dulia countries before creation."), *PendingStartingCountryName, PendingStartingCountryMapIndex)
        : TEXT("Select a country from the filtered results below. The marker on the map shows the selected location.");

    for (const FDemocracyGeneratedCountryState& Country : Options)
    {
        if (Country.MapCountryIndex == PendingStartingCountryMapIndex)
        {
            SelectedDescription = FString::Printf(
                TEXT("%s starts on %s in a %s climate. It is currently %s in preview data and will become your player country on creation."),
                *Country.CountryName,
                *Country.ContinentName,
                *Country.Climate,
                *Country.PoliticalType);
            break;
        }
    }

    const float PreviewWidth = 760.0f;
    const float PreviewHeight = 374.0f;
    FString SetupCountryMapPath;
    if (EnsureDuliaSetupCountryMapImage(PendingStartingCountryMapIndex, SetupCountryMapPath))
    {
        SetupCountryMapBrush = MakeShared<FSlateDynamicImageBrush>(FName(*SetupCountryMapPath), FVector2D(2242.0f, 1104.0f));
    }
    else
    {
        SetupCountryMapBrush.Reset();
    }
    TSharedRef<SOverlay> MapOverlay = SNew(SOverlay);
    MapOverlay->AddSlot()
    [
        SNew(SRtsMapInputSurface)
        .OnMapMouseButtonDown(FPointerEventHandler::CreateUObject(this, &ALoginHUD::HandleStartingCountryMapClicked))
        [
            SNew(SBox)
            .WidthOverride(PreviewWidth)
            .HeightOverride(PreviewHeight)
            [
                SNew(SImage)
                .Image(SetupCountryMapBrush.IsValid() ? SetupCountryMapBrush.Get() : WorldMapBrush.Get())
            ]
        ]
    ];

    Body->AddSlot().AutoHeight().Padding(0.0f, 8.0f)
    [
        SNew(SBorder)
        .BorderImage(RowBrush.Get())
        .BorderBackgroundColor(FLinearColor(0.02f, 0.05f, 0.06f, 0.92f))
        .Padding(FMargin(6.0f))
        [MapOverlay]
    ];

    Body->AddSlot().AutoHeight().Padding(0.0f, 6.0f)
    [BuildInfoRow(TEXT("Selected Country"), SelectedDescription)];

    TSharedRef<SScrollBox> CountryList = SNew(SScrollBox)
        .OnUserScrolled(FOnUserScrolled::CreateUObject(this, &ALoginHUD::HandleStartingCountryListScrolled));
    int32 VisibleRows = 0;
    for (const FDemocracyGeneratedCountryState& Country : Options)
    {
        if (VisibleRows >= 18)
        {
            break;
        }

        ++VisibleRows;
        const bool bSelected = Country.MapCountryIndex == PendingStartingCountryMapIndex;
        const FString Label = FString::Printf(TEXT("%s%s"), *Country.CountryName, bSelected ? TEXT(" - Selected") : TEXT(""));
        const FString Detail = FString::Printf(
            TEXT("Slot %03d | %s | %s | currently %s"),
            Country.MapCountryIndex,
            *Country.ContinentName,
            *Country.Climate,
            *Country.PoliticalType);

        CountryList->AddSlot().Padding(0.0f, 4.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [BuildButton(Label, FOnClicked::CreateUObject(this, &ALoginHUD::HandleSelectStartingCountry, Country.CountryName, Country.MapCountryIndex), 560.0f, 40.0f)]
            + SVerticalBox::Slot().AutoHeight()
            [BuildInfoRow(TEXT("Map Location"), Detail)]
        ];
    }

    if (VisibleRows == 0)
    {
        CountryList->AddSlot().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("No Countries Found"), TEXT("Adjust the starting-country search text."))];
    }
    else if (Options.Num() > VisibleRows)
    {
        CountryList->AddSlot().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Filtered"), FString::Printf(TEXT("Showing first %d of %d matches. Use search to narrow the list."), VisibleRows, Options.Num()))];
    }

    CountryList->SetScrollOffset(PendingStartingCountryListScrollOffset);

    Body->AddSlot().MaxHeight(210.0f).Padding(0.0f, 8.0f)
    [CountryList];

    return Body;
}
TSharedRef<SWidget> ALoginHUD::BuildNewStateSetupScreen()
{
    const bool bHasRequiredSelections = !PendingDifficulty.IsEmpty() && !PendingClimate.IsEmpty() && !PendingLeaderGender.IsEmpty() && PendingStartingCountryMapIndex > 0;

    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildInfoRow(TEXT("Difficulty"), PendingDifficulty.IsEmpty() ? TEXT("None selected") : PendingDifficulty)];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildInfoRow(TEXT("Difficulty Profile"), PendingDifficulty.IsEmpty() ? TEXT("Select a difficulty first.") : FDifficultyProfileLibrary::GetProfile(PendingDifficulty).ToSummaryText())];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildInfoRow(TEXT("Tutorial Guidance"), PendingDifficulty.IsEmpty() ? TEXT("Select a difficulty first.") : DifficultyGuidancePreview(FDifficultyProfileLibrary::GetProfile(PendingDifficulty)))];
    Body->AddSlot().AutoHeight().Padding(0.0f, 8.0f)
    [
        SNew(SEditableTextBox)
        .HintText(BodyText(TEXT("Country name")))
        .Text(BodyText(PendingStateName))
        .OnTextChanged(FOnTextChanged::CreateUObject(this, &ALoginHUD::HandlePendingStateNameChanged))
    ];
    const bool bShowClimateOptions = PendingClimate.IsEmpty() || bNewCountryClimateExpanded;
    const FString ClimateHeader = PendingClimate.IsEmpty()
        ? TEXT("Climate: Choose")
        : FString::Printf(TEXT("Climate: %s%s"), *PendingClimate, bShowClimateOptions ? TEXT(" - Collapse") : TEXT(" - Change"));
    Body->AddSlot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 4.0f)
    [BuildButton(ClimateHeader, FOnClicked::CreateUObject(this, &ALoginHUD::HandleToggleNewCountryClimateSectionClicked), 460.0f, 44.0f)];
    if (bShowClimateOptions)
    {
        Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildButton(PendingClimate == TEXT("Northern Cold") ? TEXT("Northern Cold - Selected") : TEXT("Northern Cold"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSelectClimate, FString(TEXT("Northern Cold"))), 360.0f, 46.0f)];
        Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildButton(PendingClimate == TEXT("Middle Moderate") ? TEXT("Middle Moderate - Selected") : TEXT("Middle Moderate"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSelectClimate, FString(TEXT("Middle Moderate"))), 360.0f, 46.0f)];
        Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildButton(PendingClimate == TEXT("Southern Tropical") ? TEXT("Southern Tropical - Selected") : TEXT("Southern Tropical"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSelectClimate, FString(TEXT("Southern Tropical"))), 360.0f, 46.0f)];
    }

    const bool bShowGenderOptions = PendingLeaderGender.IsEmpty() || bNewCountryGenderExpanded;
    const FString AddressSummary = PendingAddressTitle.IsEmpty() ? TEXT("Choose") : PendingAddressTitle;
    const FString GenderHeader = FString::Printf(TEXT("President Address: %s%s"), *AddressSummary, bShowGenderOptions ? TEXT(" - Collapse") : TEXT(" - Change"));
    Body->AddSlot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 4.0f)
    [BuildButton(GenderHeader, FOnClicked::CreateUObject(this, &ALoginHUD::HandleToggleNewCountryGenderSectionClicked), 460.0f, 44.0f)];
    if (bShowGenderOptions)
    {
        Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildButton(PendingLeaderGender == TEXT("Male") ? TEXT("Male - Mr. President") : TEXT("Male"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSelectLeaderGender, FString(TEXT("Male"))), 360.0f, 46.0f)];
        Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildButton(PendingLeaderGender == TEXT("Female") ? TEXT("Female - Miss President") : TEXT("Female"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSelectLeaderGender, FString(TEXT("Female"))), 360.0f, 46.0f)];
    }

    const bool bShowLocationOptions = PendingStartingCountryMapIndex <= 0 || bNewCountryLocationExpanded;
    const FString LocationSummary = PendingStartingCountryMapIndex > 0
        ? FString::Printf(TEXT("%s | Map slot %03d"), *PendingStartingCountryName, PendingStartingCountryMapIndex)
        : TEXT("Choose");
    const FString LocationHeader = FString::Printf(TEXT("Starting Country: %s%s"), *LocationSummary, bShowLocationOptions ? TEXT(" - Collapse") : TEXT(" - Change"));
    Body->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 4.0f)
    [BuildButton(LocationHeader, FOnClicked::CreateUObject(this, &ALoginHUD::HandleToggleNewCountryLocationSectionClicked), 560.0f, 44.0f)];
    if (bShowLocationOptions)
    {
        Body->AddSlot().AutoHeight().Padding(0.0f, 8.0f)
        [BuildStartingCountrySelectionWidget()];
    }
    else
    {
        Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Selected Country"), LocationSummary)];
    }
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildInfoRow(TEXT("Create Status"), LastSaveStatus.IsEmpty() ? TEXT("Ready") : LastSaveStatus)];
    Body->AddSlot().AutoHeight().Padding(0.0f, 12.0f)
    [BuildButton(TEXT("Create Country and Load Game"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleCreateInitialSaveClicked), 360.0f, 52.0f, bHasRequiredSelections)];
    Body->AddSlot().AutoHeight().Padding(0.0f, 14.0f)
    [BuildButton(TEXT("Back"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleBackToDifficultyClicked), 180.0f, 44.0f)];

    NewCountrySetupScrollBox.Reset();
    return BuildPanel(
        TEXT("New Country Setup"),
        TEXT("Name the country, choose its climate, and choose where it starts on Planet Dulia."),
        Body,
        820.0f,
        FOnUserScrolled::CreateUObject(this, &ALoginHUD::HandleNewCountrySetupScrolled),
        PendingNewCountrySetupScrollOffset,
        &NewCountrySetupScrollBox);
}
TSharedRef<SWidget> ALoginHUD::BuildLoadedGameScreen()
{
    return BuildPanel(TEXT("Loaded State"), TEXT("Runtime state is loaded. Enter the office to continue playing."),
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Loaded State"), LoadedStateName.IsEmpty() ? TEXT("Unnamed") : LoadedStateName)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Save File"), LoadedSavePath.IsEmpty() ? TEXT("No file path recorded.") : LoadedSavePath)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Runtime Load"), bHasLoadedRuntimeState ? LoadedSaveSummary : (LoadedSaveError.IsEmpty() ? TEXT("Runtime state has not been loaded.") : LoadedSaveError))]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Simulation"), BuildSimulationStatusText())]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 8.0f, 0.0f, 0.0f)
        [BuildInfoRow(TEXT("Objectives"), BuildObjectiveStatusText())]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Resources"), BuildResourceStatusText())]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Policy"), BuildPolicyStatusText())]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Events"), BuildEventStatusText())]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Demographics"), BuildDemographicsStatusText())]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Budget"), BuildEconomyBudgetStatusText())]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Departments"), BuildDepartmentStatusText())]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Development"), BuildDevelopmentStatusText())]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Approval Causes"), BuildApprovalStabilityStatusText())]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Decision History"), BuildDecisionHistoryStatusText())]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [BuildButton(TEXT("Resume"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleResumeSimulationClicked), 150.0f, 42.0f, bHasLoadedRuntimeState && LoadedSaveState.RuntimeState.bPaused)]
            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [BuildButton(TEXT("Pause"), FOnClicked::CreateUObject(this, &ALoginHUD::HandlePauseSimulationClicked), 150.0f, 42.0f, bHasLoadedRuntimeState && !LoadedSaveState.RuntimeState.bPaused)]
            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [BuildButton(TEXT("Step Tick"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleStepSimulationClicked), 150.0f, 42.0f, bHasLoadedRuntimeState)]
            + SHorizontalBox::Slot().AutoWidth()
            [BuildButton(TEXT("Save"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSaveRuntimeStateClicked), 120.0f, 42.0f, bHasLoadedRuntimeState && !LoadedSavePath.IsEmpty())]
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Save Status"), LastSaveStatus.IsEmpty() ? TEXT("Runtime changes are in memory until Save is pressed.") : LastSaveStatus)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f)
        [BuildInfoRow(TEXT("Briefing"), TEXT("Ongoing briefings are available from the folder on the office desk."))]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 14.0f)
        [BuildButton(TEXT("Enter Office"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleEnterOfficeClicked), 220.0f, 44.0f)], 860.0f);
}
TSharedRef<SWidget> ALoginHUD::BuildOfficeOpeningBriefingScreen()
{
    const FString ScriptToShow = bShowFirstLoginBriefing
        ? (OpeningScriptText.IsEmpty() ? GetOpeningScriptText() : OpeningScriptText)
        : BuildOngoingBriefingText();
    const FString BriefingTitle = bShowFirstLoginBriefing ? TEXT("Assistant Briefing") : TEXT("Ongoing Briefing");
    const FString BriefingSubtitle = bShowFirstLoginBriefing
        ? TEXT("The office is now loaded. The assistant begins the first briefing.")
        : TEXT("Updated state briefing from the desk folder.");

    return BuildPanel(BriefingTitle, BriefingSubtitle,
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Loaded State"), LoadedStateName.IsEmpty() ? TEXT("Unnamed") : LoadedStateName)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Assistant"), bShowFirstLoginBriefing ? TEXT("Opening dialogue uses the current script file. Bracketed player-choice placeholders are preserved for the next dialogue pass.") : TEXT("Current briefing is generated from the loaded runtime state."))]
        + SVerticalBox::Slot().MaxHeight(420.0f).Padding(0.0f, 12.0f)
        [
            SNew(SScrollBox)
            + SScrollBox::Slot()
            [
                SNew(SBorder)
                .BorderImage(RowBrush.Get())
                .Padding(FMargin(14.0f, 12.0f))
                [
                    SNew(STextBlock)
                    .Text(BodyText(ScriptToShow))
                    .AutoWrapText(true)
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
                    .ColorAndOpacity(FLinearColor(0.88f, 0.90f, 0.92f, 1.0f))
                ]
            ]
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f)
        [BuildButton(bShowFirstLoginBriefing ? TEXT("Begin In Office") : TEXT("Close Briefing"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleBeginOfficeFromBriefingClicked), 220.0f, 42.0f)], 760.0f);
}

TSharedRef<SWidget> ALoginHUD::BuildOfficeDashboardScreen()
{
    return BuildOfficeComputerMenuScreen();
}

TSharedRef<SWidget> ALoginHUD::BuildOfficeComputerMenuScreen()
{
    if (bHasLoadedRuntimeState)
    {
        RefreshWarConflictState(LoadedSaveState.RuntimeState);
        RefreshSimulationToRtsContract(LoadedSaveState.RuntimeState);
        RefreshCommandAuthority(LoadedSaveState.RuntimeState);
    }
    const FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    const FDemocracyCountryState& Country = State.PlayerCountry;
    const FString GovernmentSummary = bHasLoadedRuntimeState
        ? FString::Printf(TEXT("%s | %s | %s | guidance %s"), *Country.CountryName, *Country.Difficulty, *Country.Climate, *GetRuntimeGuidanceLevel(State))
        : FString(TEXT("No runtime state loaded."));
    const FString RiskSummary = bHasLoadedRuntimeState
        ? BuildGuidedFailureWarningText(State)
        : FString(TEXT("Unavailable"));
    const FString GuidanceSummary = bHasLoadedRuntimeState
        ? BuildDashboardGuidanceText(State)
        : FString(TEXT("Unavailable"));
    const FString ReportsSummary = bHasLoadedRuntimeState
        ? FString::Printf(TEXT("Advisor reports %d | decisions %d/%d | press records %d | meetings %d"),
            State.AdvisorSystem.Reports.Num(),
            State.DecisionHistory.Records.Num(),
            State.DecisionHistory.MaxRecords,
            State.PressOffice.Records.Num(),
            State.MeetingSystem.TotalMeetings)
        : FString(TEXT("Unavailable"));

    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);

    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildInfoRow(TEXT("Government"), GovernmentSummary)];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildInfoRow(TEXT("Simulation"), BuildSimulationStatusText())];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildInfoRow(TEXT("Objectives"), BuildObjectiveStatusText())];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildInfoRow(TEXT("Diplomacy"), BuildDiplomacyStatusText())];
    Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
    [BuildInfoRow(TEXT("Government / Diplomacy Rules"), BuildGovernmentDiplomacyRulesStatusText())];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildInfoRow(TEXT("Command Authority"), BuildCommandAuthorityStatusText())];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildInfoRow(TEXT("RTS Backflow"), BuildRtsBackflowStatusText())];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildInfoRow(TEXT("War / Conflict State"), BuildWarConflictStatusText())];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildInfoRow(TEXT("Simulation-to-RTS Contract"), BuildSimulationToRtsContractStatusText())];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildInfoRow(TEXT("RTS Save Boundary"), BuildRtsSaveBoundaryStatusText())];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildInfoRow(TEXT("Difficulty Guidance"), GuidanceSummary)];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildInfoRow(TEXT("Risk / Causes"), RiskSummary)];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildInfoRow(TEXT("Save"), LastSaveStatus.IsEmpty() ? TEXT("Manual save and autosave protection are available from this computer.") : LastSaveStatus)];

    Body->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 4.0f)
    [BuildInfoRow(TEXT("War / Diplomacy Commands"), TEXT("Office decisions gate RTS authority. Each command shows availability, cost, consequence, and the RTS permission or lock it changes."))];
    if (bHasLoadedRuntimeState)
    {
        TSharedRef<SVerticalBox> CommandRows = SNew(SVerticalBox);
        const TArray<FString> PriorityCommandIds = {
            TEXT("office_declare_war"),
            TEXT("office_negotiate_ceasefire"),
            TEXT("office_surrender_territory"),
            TEXT("office_impose_sanctions"),
            TEXT("office_request_alliance_aid"),
            TEXT("office_propose_alliance"),
            TEXT("office_propose_treaty")
        };
        for (const FString& CommandId : PriorityCommandIds)
        {
            const FDemocracyCommandAuthorityActionState* Action = LoadedSaveState.RuntimeState.CommandAuthority.Actions.FindByPredicate([&CommandId](const FDemocracyCommandAuthorityActionState& Candidate)
            {
                return Candidate.CommandId.Equals(CommandId, ESearchCase::IgnoreCase);
            });
            if (!Action || !Action->bOfficeAllowed)
            {
                continue;
            }

            const FString EnabledText = Action->bEnabled ? TEXT("Enabled") : FString::Printf(TEXT("Disabled: %s"), Action->DisabledReason.IsEmpty() ? TEXT("requirements not met") : *Action->DisabledReason);
            const FString CostText = Action->TreasuryCost > 0
                ? FString::Printf(TEXT("Cost: treasury %d"), Action->TreasuryCost)
                : TEXT("Cost: none");
            const FString ConsequenceText = FString::Printf(TEXT("Approval %+d | Stability %+d | Unrest %+d | Diplomacy %+d | Military %+d | Invasion risk %+d | Resources %+d"),
                Action->ApprovalDelta,
                Action->StabilityDelta,
                Action->UnrestDelta,
                Action->DiplomacyDelta,
                Action->MilitaryDelta,
                Action->InvasionRiskDelta,
                Action->ResourceDelta);
            const FString RequirementText = Action->Prerequisite.IsEmpty() ? TEXT("Requirement: current authority posture allows review.") : FString::Printf(TEXT("Requirement: %s"), *Action->Prerequisite);
            const FString Details = FString::Printf(TEXT("%s\n%s\n%s\n%s\nRTS effect: %s"), *EnabledText, *CostText, *RequirementText, *ConsequenceText, *Action->EffectPreview);

            CommandRows->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 10.0f, 0.0f)
                [BuildInfoRow(Action->Label, Details)]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [BuildButton(Action->bEnabled ? TEXT("Execute") : TEXT("Blocked"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleExecuteAuthorityCommand, Action->CommandId, FString(TEXT("Office"))), 120.0f, 38.0f, Action->bEnabled)]
            ];
        }
        Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)[CommandRows];
    }

    Body->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 4.0f)
    [BuildInfoRow(TEXT("Simulation Controls"), BuildTimeControlStatusText())];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
        [BuildButton(TEXT("Resume"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleResumeSimulationClicked), 125.0f, 38.0f, bHasLoadedRuntimeState && LoadedSaveState.RuntimeState.bPaused)]
        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
        [BuildButton(TEXT("Pause"), FOnClicked::CreateUObject(this, &ALoginHUD::HandlePauseSimulationClicked), 125.0f, 38.0f, bHasLoadedRuntimeState && !LoadedSaveState.RuntimeState.bPaused)]
        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
        [BuildButton(TEXT("Step Tick"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleStepSimulationClicked), 135.0f, 38.0f, bHasLoadedRuntimeState)]
        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
        [BuildButton(TEXT("Speed -"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSlowerSimulationClicked), 115.0f, 38.0f, bHasLoadedRuntimeState)]
        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
        [BuildButton(TEXT("Default"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleDefaultSimulationSpeedClicked), 115.0f, 38.0f, bHasLoadedRuntimeState)]
        + SHorizontalBox::Slot().AutoWidth()
        [BuildButton(TEXT("Speed +"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleFasterSimulationClicked), 115.0f, 38.0f, bHasLoadedRuntimeState)]
    ];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
        [BuildButton(TEXT("Save Current"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSaveRuntimeStateClicked), 145.0f, 38.0f, bHasLoadedRuntimeState && !LoadedSavePath.IsEmpty())]
        + SHorizontalBox::Slot().AutoWidth()
        [BuildButton(TEXT("Load State"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleBackToLocalSavesClicked), 130.0f, 38.0f)]
    ];

#if !UE_BUILD_SHIPPING
    if (HasGameMasterDebugAccess())
    {
        const bool bAdminDebug = HasAdministratorDebugAccess();
        Body->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 4.0f)
        [BuildInfoRow(TEXT("Developer Test Tools"), BuildDebugAccessStatusText())];
        Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [BuildButton(TEXT("Test Recovery"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleRunAutosaveRecoveryTestClicked), 155.0f, 38.0f)]
            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [BuildButton(TEXT("Run Early Test Path"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleRunEarlyGameTestScenarioClicked), 200.0f, 38.0f)]
            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [BuildButton(TEXT("Trigger Event"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleDebugTriggerEventClicked), 150.0f, 38.0f, bHasLoadedRuntimeState)]
            + SHorizontalBox::Slot().AutoWidth()
            [BuildButton(TEXT("Advance 3 Ticks"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleDebugAdvanceTimeClicked), 170.0f, 38.0f, bHasLoadedRuntimeState)]
        ];
        if (bAdminDebug)
        {
            Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
                [BuildButton(TEXT("+ Resources"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleDebugAddResourcesClicked), 140.0f, 38.0f, bHasLoadedRuntimeState)]
                + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
                [BuildButton(TEXT("Force Unrest"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleDebugForceUnrestClicked), 150.0f, 38.0f, bHasLoadedRuntimeState)]
                + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
                [BuildButton(TEXT("Force Takeover Risk"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleDebugForceInvasionRiskClicked), 190.0f, 38.0f, bHasLoadedRuntimeState)]
                + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
                [BuildButton(TEXT("RTS Save/Load Test"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleRunRtsSaveLoadPlaytestClicked), 190.0f, 38.0f, bHasLoadedRuntimeState && !LoadedSavePath.IsEmpty())]
                + SHorizontalBox::Slot().AutoWidth()
                [BuildButton(TEXT("Test Game Over"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleDebugTestGameOverClicked), 170.0f, 38.0f, bHasLoadedRuntimeState)]
            ];
        }
    }
#endif
    Body->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 4.0f)
    [BuildInfoRow(TEXT("Control Desks"), TEXT("Open the main simulation systems from the computer."))];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(0.5f).Padding(0.0f, 0.0f, 6.0f, 0.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildButton(TEXT("Policies"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleOpenPoliciesClicked), 300.0f, 40.0f, bHasLoadedRuntimeState)]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildButton(TEXT("Budget / Economy"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleOpenBudgetClicked), 300.0f, 40.0f, bHasLoadedRuntimeState)]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildButton(TEXT("Resources"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleOpenResourceChainsClicked), 300.0f, 40.0f, bHasLoadedRuntimeState)]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildButton(TEXT("Events"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleOpenEventsClicked), 300.0f, 40.0f, bHasLoadedRuntimeState)]
        ]
        + SHorizontalBox::Slot().FillWidth(0.5f).Padding(6.0f, 0.0f, 0.0f, 0.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildButton(TEXT("Advisors / Warnings"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleOpenAdvisorWarningsClicked), 300.0f, 40.0f, bHasLoadedRuntimeState)]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildButton(TEXT("Departments"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleOpenDepartmentsClicked), 300.0f, 40.0f, bHasLoadedRuntimeState)]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildButton(TEXT("Technology / Development"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleOpenDevelopmentClicked), 300.0f, 40.0f, bHasLoadedRuntimeState)]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildButton(TEXT("Demographics"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleOpenDemographicsClicked), 300.0f, 40.0f, bHasLoadedRuntimeState)]
        ]
    ];

    Body->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 4.0f)
    [BuildInfoRow(TEXT("Reports"), ReportsSummary)];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(0.5f).Padding(0.0f, 0.0f, 6.0f, 0.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildButton(TEXT("Approval Causes"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleOpenApprovalStabilityClicked), 300.0f, 40.0f, bHasLoadedRuntimeState)]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildButton(TEXT("Decision History"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleOpenDecisionHistoryClicked), 300.0f, 40.0f, bHasLoadedRuntimeState)]
        ]
        + SHorizontalBox::Slot().FillWidth(0.5f).Padding(6.0f, 0.0f, 0.0f, 0.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildButton(TEXT("Meetings"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleOpenMeetingAdvisorClicked), 300.0f, 40.0f, bHasLoadedRuntimeState)]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildButton(TEXT("Press Releases"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleOpenPressReleaseClicked), 300.0f, 40.0f, bHasLoadedRuntimeState)]
        ]
    ];

    Body->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 4.0f)
    [BuildInfoRow(TEXT("Current Reports"), TEXT("Snapshot of every major simulation channel. Open Decision History for the full report log."))];
    Body->AddSlot().MaxHeight(300.0f).Padding(0.0f, 4.0f)
    [
        SNew(SScrollBox)
        + SScrollBox::Slot()
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildInfoRow(TEXT("Policies"), BuildPolicyStatusText())]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildInfoRow(TEXT("Budget"), BuildEconomyBudgetStatusText())]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildInfoRow(TEXT("Resources"), BuildResourceChainStatusText())]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildInfoRow(TEXT("Events"), BuildEventStatusText())]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildInfoRow(TEXT("Advisors"), BuildAdvisorWarningText())]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildInfoRow(TEXT("Departments"), BuildDepartmentStatusText())]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildInfoRow(TEXT("Reports"), BuildDecisionHistoryStatusText())]
        ]
    ];

    Body->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 4.0f)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
        [BuildButton(TEXT("Open Settings"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSettingsClicked), 180.0f, 40.0f)]
        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
        [BuildButton(TEXT("Return To Menu"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleExitOfficeClicked), 190.0f, 40.0f)]
        + SHorizontalBox::Slot().AutoWidth()
        [BuildButton(TEXT("Close"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleCloseOfficeOverlayClicked), 140.0f, 40.0f)]
    ];

    return BuildPanel(TEXT("Simulation Dashboard"), TEXT("Computer control surface for policies, budget, resources, events, advisors, departments, and reports."),
        SNew(SScrollBox)
        + SScrollBox::Slot()
        [Body], 900.0f);
}


TSharedRef<SWidget> ALoginHUD::BuildOfficePoliciesScreen()
{
    RefreshPolicyRules(LoadedSaveState.RuntimeState);
    const FDemocracyPolicyState& Policies = LoadedSaveState.RuntimeState.PlayerCountry.Policies;
    auto BuildPolicyButton = [this](const FString& Category, const FString& PolicyName) -> TSharedRef<SWidget>
    {
        const FPolicyRuleEvaluation Evaluation = EvaluatePolicyRules(LoadedSaveState.RuntimeState, Category, PolicyName);
        const bool bIsActive = GetSelectedPolicyForCategory(LoadedSaveState.RuntimeState.PlayerCountry.Policies, Category).Equals(PolicyName, ESearchCase::IgnoreCase);
        const FString Label = bIsActive
            ? FString::Printf(TEXT("Active: %s"), *PolicyName)
            : (Evaluation.bCanSelect ? PolicyName : FString::Printf(TEXT("Locked: %s"), *PolicyName));
        return BuildButton(Label, FOnClicked::CreateUObject(this, &ALoginHUD::HandleSetPolicy, Category, PolicyName), 420.0f, 42.0f, Evaluation.bCanSelect && !bIsActive);
    };

    return BuildPanel(TEXT("Policy Desk"), TEXT("Choose active laws and national policies. Effects apply on every simulation tick."),
        SNew(SVerticalBox)
        + SVerticalBox::Slot().MaxHeight(470.0f).Padding(0.0f, 4.0f)
        [
            SNew(SScrollBox)
            + SScrollBox::Slot()
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [BuildInfoRow(TEXT("Current Platform"), BuildPolicyStatusText())]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f)
                [BuildInfoRow(TEXT("Economic Policy"), TEXT("Controls treasury, approval, economy, infrastructure, and production pressure."))]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [BuildPolicyButton(TEXT("Economic"), TEXT("Balanced Budget"))]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [BuildPolicyButton(TEXT("Economic"), TEXT("Stimulus Spending"))]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [BuildPolicyButton(TEXT("Economic"), TEXT("Austerity Program"))]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [BuildPolicyButton(TEXT("Economic"), TEXT("Industrial Subsidies"))]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f)
                [BuildInfoRow(TEXT("Environmental Policy"), TEXT("Controls extraction, environment, water/food pressure, and stability."))]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [BuildPolicyButton(TEXT("Environmental"), TEXT("Managed Development"))]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [BuildPolicyButton(TEXT("Environmental"), TEXT("Conservation Mandate"))]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [BuildPolicyButton(TEXT("Environmental"), TEXT("Extraction Expansion"))]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f)
                [BuildInfoRow(TEXT("Military Policy"), TEXT("Controls readiness, invasion risk, treasury cost, and domestic pressure."))]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [BuildPolicyButton(TEXT("Military"), TEXT("Defensive Readiness"))]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [BuildPolicyButton(TEXT("Military"), TEXT("National Mobilization"))]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [BuildPolicyButton(TEXT("Military"), TEXT("Demilitarization"))]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f)
                [BuildInfoRow(TEXT("Diplomacy Policy"), TEXT("Controls diplomatic standing, alliances, border risk, and foreign pressure."))]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [BuildPolicyButton(TEXT("Diplomacy"), TEXT("Neutral Engagement"))]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [BuildPolicyButton(TEXT("Diplomacy"), TEXT("Alliance Outreach"))]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [BuildPolicyButton(TEXT("Diplomacy"), TEXT("Hardline Sovereignty"))]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f)
                [BuildInfoRow(TEXT("Civil Policy"), TEXT("Controls approval, unrest, stability, legitimacy, and emergency authority."))]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [BuildPolicyButton(TEXT("Civil"), TEXT("Public Stability"))]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [BuildPolicyButton(TEXT("Civil"), TEXT("Civil Liberties"))]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [BuildPolicyButton(TEXT("Civil"), TEXT("Emergency Powers"))]
            ]
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f)
        [BuildButton(TEXT("Close"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleCloseOfficeOverlayClicked), 160.0f, 40.0f)], 760.0f);
}
TSharedRef<SWidget> ALoginHUD::BuildOfficeEventsScreen()
{
    FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    FDemocracyEventSystemState& EventSystem = State.EventSystem;
    TSharedRef<SVerticalBox> EventList = SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Event Status"), BuildEventStatusText())]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)
        [BuildInfoRow(TEXT("Choice Flow"), bHasLoadedRuntimeState ? GuidanceText(GetRuntimeGuidanceLevel(State), TEXT("Each event has a response deadline. Detailed mode shows trigger, deadline, follow-up, exact stat changes, and tutorial comparison cues."), TEXT("Each event has a response deadline. Standard mode shows trigger, deadline, follow-up, and direct consequences."), TEXT("Each event has a response deadline. Limited mode shows broad consequences with fewer exact numbers."), TEXT("Events show only core signals and choice labels on this difficulty.")) : TEXT("No runtime state loaded."))];

    bool bHasPendingEvent = false;
    for (const FDemocracyActiveEventState& Event : EventSystem.ActiveEvents)
    {
        if (Event.bResolved)
        {
            continue;
        }

        bHasPendingEvent = true;
        EventList->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 4.0f)
        [BuildInfoRow(Event.Title, BuildGuidedEventHeaderText(State, Event))];

        for (const FDemocracyEventChoiceState& Choice : Event.Choices)
        {
            EventList->AddSlot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 2.0f)
            [BuildButton(FString::Printf(TEXT("Choose: %s"), *Choice.Label), FOnClicked::CreateUObject(this, &ALoginHUD::HandleResolveEventChoice, Event.EventId, Choice.ChoiceId), 600.0f, 40.0f)];
            EventList->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 6.0f)
            [BuildInfoRow(Choice.Description, BuildGuidedEventChoiceText(State, Choice))];
        }
    }

    if (!bHasPendingEvent)
    {
        EventList->AddSlot().AutoHeight().Padding(0.0f, 10.0f)
        [BuildInfoRow(TEXT("No Pending Events"), TEXT("Events can be triggered by shortages, protests, border pressure, or random shocks during simulation ticks."))];
    }

    EventList->AddSlot().AutoHeight().Padding(0.0f, 12.0f)
    [BuildButton(TEXT("Close"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleCloseOfficeOverlayClicked), 160.0f, 40.0f)];

    return BuildPanel(TEXT("Event Desk"), TEXT("Random and triggered events require choices with visible consequences, deadlines, and follow-up effects."),
        SNew(SScrollBox)
        + SScrollBox::Slot()
        [EventList], 860.0f);
}TSharedRef<SWidget> ALoginHUD::BuildOfficeBudgetScreen()
{
    const FDemocracyEconomyBudgetState& Budget = LoadedSaveState.RuntimeState.EconomyBudget;

    auto BuildTaxButton = [this, &Budget](const FString& TaxPolicyName) -> TSharedRef<SWidget>
    {
        const FBudgetOptionEvaluation Evaluation = EvaluateTaxPolicyRules(LoadedSaveState.RuntimeState, TaxPolicyName);
        const bool bIsActive = Budget.TaxPolicy.Equals(TaxPolicyName, ESearchCase::IgnoreCase);
        const FString Label = bIsActive
            ? FString::Printf(TEXT("Active: %s"), *TaxPolicyName)
            : (Evaluation.bCanSelect ? TaxPolicyName : FString::Printf(TEXT("Locked: %s"), *TaxPolicyName));
        return BuildButton(Label, FOnClicked::CreateUObject(this, &ALoginHUD::HandleSetTaxPolicy, TaxPolicyName), 420.0f, 40.0f, Evaluation.bCanSelect && !bIsActive);
    };

    auto BuildSpendingButton = [this, &Budget](const FString& SpendingPostureName) -> TSharedRef<SWidget>
    {
        const FBudgetOptionEvaluation Evaluation = EvaluateSpendingPostureRules(LoadedSaveState.RuntimeState, SpendingPostureName);
        const bool bIsActive = Budget.SpendingPosture.Equals(SpendingPostureName, ESearchCase::IgnoreCase);
        const FString Label = bIsActive
            ? FString::Printf(TEXT("Active: %s"), *SpendingPostureName)
            : (Evaluation.bCanSelect ? SpendingPostureName : FString::Printf(TEXT("Locked: %s"), *SpendingPostureName));
        return BuildButton(Label, FOnClicked::CreateUObject(this, &ALoginHUD::HandleSetSpendingPosture, SpendingPostureName), 420.0f, 40.0f, Evaluation.bCanSelect && !bIsActive);
    };

    const FString BudgetGuidance = FString::Printf(TEXT("Debt capacity %d, spending cap %d, credit stress %d. Deficits add debt and inflation; high credit stress restricts low taxes and high-spending options."), Budget.DebtCapacity, Budget.SpendingLimit, Budget.CreditStress);

    return BuildPanel(TEXT("Budget Desk"), TEXT("Taxes, spending, debt, inflation, public services, production, and enforced fiscal capacity."),
        SNew(SVerticalBox)
        + SVerticalBox::Slot().MaxHeight(470.0f).Padding(0.0f, 4.0f)
        [
            SNew(SScrollBox)
            + SScrollBox::Slot()
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [BuildInfoRow(TEXT("Current Budget"), BuildEconomyBudgetStatusText())]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f)
                [BuildInfoRow(TEXT("Budget Enforcement"), BudgetGuidance)]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f)
                [BuildInfoRow(TEXT("Tax Policy"), TEXT("Taxes affect income, tax burden, approval, production, deficit pressure, and future credit room."))]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [BuildTaxButton(TEXT("Low Taxes"))]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [BuildTaxButton(TEXT("Balanced Taxation"))]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [BuildTaxButton(TEXT("High Taxes"))]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f)
                [BuildInfoRow(TEXT("Spending Posture"), TEXT("Spending is capped by income, treasury reserve, and remaining debt capacity. Unaffordable options are locked."))]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [BuildSpendingButton(TEXT("Austerity"))]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [BuildSpendingButton(TEXT("Balanced Services"))]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [BuildSpendingButton(TEXT("Public Services"))]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [BuildSpendingButton(TEXT("Infrastructure Push"))]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [BuildSpendingButton(TEXT("Defense Funding"))]
            ]
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f)
        [BuildButton(TEXT("Close"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleCloseOfficeOverlayClicked), 160.0f, 40.0f)], 780.0f);
}TSharedRef<SWidget> ALoginHUD::BuildOfficeDepartmentsScreen()
{
    if (bHasLoadedRuntimeState)
    {
        InitializeDefaultDepartments(LoadedSaveState.RuntimeState);
        RecalculateDepartments(LoadedSaveState.RuntimeState);
    }

    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Department Summary"), BuildDepartmentStatusText())];

    if (bHasLoadedRuntimeState)
    {
        for (const FDemocracyDepartmentState& Department : LoadedSaveState.RuntimeState.Departments.Departments)
        {
            Body->AddSlot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
            [BuildInfoRow(Department.DepartmentName, FString::Printf(TEXT("%s\n%s\nBudget %d | staffing %d | effectiveness %d | trust %d | priority %d\nAction: %s\nPolicy interface: %s\n%s"),
                *Department.MinisterTitle,
                *Department.Domain,
                Department.BudgetShare,
                Department.Staffing,
                Department.Effectiveness,
                Department.PublicTrust,
                Department.Priority,
                *Department.CurrentAction,
                *Department.PolicyInterface,
                *Department.AdvisorySummary))];

            Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
                [BuildButton(TEXT("Routine"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSetDepartmentAction, Department.DepartmentName, FString(TEXT("Routine Operations"))), 112.0f, 34.0f)]
                + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
                [BuildButton(TEXT("Focus"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSetDepartmentAction, Department.DepartmentName, FString(TEXT("Focused Initiative"))), 112.0f, 34.0f)]
                + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
                [BuildButton(TEXT("Emergency"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSetDepartmentAction, Department.DepartmentName, FString(TEXT("Emergency Response"))), 132.0f, 34.0f)]
                + SHorizontalBox::Slot().AutoWidth()
                [BuildButton(TEXT("Reform"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSetDepartmentAction, Department.DepartmentName, FString(TEXT("Administrative Reform"))), 118.0f, 34.0f)]
            ];
        }
    }

    Body->AddSlot().AutoHeight().Padding(0.0f, 12.0f)
    [BuildButton(TEXT("Close"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleCloseOfficeOverlayClicked), 160.0f, 40.0f)];

    return BuildPanel(TEXT("Departments Desk"), TEXT("Domestic ministries are the action interface for policies, resources, services, and crisis response."),
        SNew(SScrollBox)
        + SScrollBox::Slot()
        [Body], 900.0f);
}
TSharedRef<SWidget> ALoginHUD::BuildOfficeDevelopmentScreen()
{
    if (bHasLoadedRuntimeState)
    {
        InitializeDevelopmentSystemIfMissing(LoadedSaveState.RuntimeState);
    }

    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Development Summary"), BuildDevelopmentStatusText())]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f)
        [BuildInfoRow(TEXT("Focus"), TEXT("Choose one long-term development track. The active focus progresses during simulation ticks if treasury and resources are available."))];

    if (bHasLoadedRuntimeState)
    {
        for (const FDemocracyDevelopmentTrackState& Track : LoadedSaveState.RuntimeState.DevelopmentSystem.Tracks)
        {
            const bool bIsActive = Track.TrackName.Equals(LoadedSaveState.RuntimeState.DevelopmentSystem.ActiveFocus, ESearchCase::IgnoreCase);
            Body->AddSlot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
            [BuildInfoRow(Track.TrackName, FString::Printf(TEXT("%s\nLevel %d | progress %d/%d | project %s\nCost per tick: treasury %d, wood %d, metals %d, fuel %d\nUnlocks: %s"),
                *Track.StrategicBenefit,
                Track.Level,
                Track.Progress,
                Track.ProgressTarget,
                *Track.CurrentProject,
                Track.TreasuryCost,
                Track.WoodCost,
                Track.MetalsCost,
                Track.FuelCost,
                *FString::Join(Track.Unlocks, TEXT(", "))))];
            Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildButton(bIsActive ? FString::Printf(TEXT("Active: %s"), *Track.TrackName) : FString::Printf(TEXT("Set Focus: %s"), *Track.TrackName), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSetDevelopmentFocus, Track.TrackName), 360.0f, 36.0f, !bIsActive)];
        }
    }

    Body->AddSlot().AutoHeight().Padding(0.0f, 12.0f)
    [BuildButton(TEXT("Close"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleCloseOfficeOverlayClicked), 160.0f, 40.0f)];

    return BuildPanel(TEXT("Development Desk"), TEXT("Long-term technology and national development goals."),
        SNew(SScrollBox)
        + SScrollBox::Slot()
        [Body], 900.0f);
}
TSharedRef<SWidget> ALoginHUD::BuildOfficeApprovalStabilityScreen()
{
    if (bHasLoadedRuntimeState)
    {
        RecalculateApprovalStability(LoadedSaveState.RuntimeState);
    }

    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Cause Summary"), BuildApprovalStabilityStatusText())];

    if (bHasLoadedRuntimeState)
    {
        for (const FDemocracyApprovalCauseState& Cause : LoadedSaveState.RuntimeState.ApprovalStability.Causes)
        {
            Body->AddSlot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
            [BuildInfoRow(Cause.CauseName, FString::Printf(TEXT("%s | severity %d | approval %+d | unrest %+d | stability %+d\nMetric: %s\nStatus: %s\nResponses: %s"),
                *Cause.Category,
                Cause.Severity,
                Cause.ApprovalImpact,
                Cause.UnrestImpact,
                Cause.StabilityImpact,
                *Cause.SourceMetric,
                *Cause.CurrentStatus,
                *FString::Join(Cause.SuggestedResponses, TEXT(", "))))];
        }
    }

    Body->AddSlot().AutoHeight().Padding(0.0f, 12.0f)
    [BuildButton(TEXT("Close"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleCloseOfficeOverlayClicked), 160.0f, 40.0f)];

    return BuildPanel(TEXT("Approval Causes"), TEXT("Breakdown of why approval, unrest, and stability are moving."),
        SNew(SScrollBox)
        + SScrollBox::Slot()
        [Body], 900.0f);
}
TSharedRef<SWidget> ALoginHUD::BuildOfficeDecisionHistoryScreen()
{
    if (bHasLoadedRuntimeState)
    {
        InitializeDecisionHistoryIfMissing(LoadedSaveState.RuntimeState);
    }

    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Decision Continuity"), BuildDecisionHistoryStatusText())];

    if (bHasLoadedRuntimeState)
    {
        const TArray<FDemocracyDecisionRecordState>& Records = LoadedSaveState.RuntimeState.DecisionHistory.Records;
        for (int32 Index = Records.Num() - 1; Index >= 0; --Index)
        {
            const FDemocracyDecisionRecordState& Record = Records[Index];
            Body->AddSlot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
            [BuildInfoRow(FString::Printf(TEXT("Turn %d - %s"), Record.Turn, *Record.DecisionTitle), FString::Printf(TEXT("%s | severity %d | %s\nDecision: %s\nConsequence: %s\nAfter: approval %d, stability %d, unrest %d, treasury %d, economy %d, military %d\nTags: %s"),
                *Record.Category,
                Record.Severity,
                *Record.TimestampUtc,
                *Record.DecisionDetail,
                *Record.ConsequenceSummary,
                Record.ApprovalAfter,
                Record.StabilityAfter,
                Record.UnrestAfter,
                Record.TreasuryAfter,
                Record.EconomyAfter,
                Record.MilitaryAfter,
                *FString::Join(Record.Tags, TEXT(", "))))];
        }
    }

    Body->AddSlot().AutoHeight().Padding(0.0f, 12.0f)
    [BuildButton(TEXT("Close"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleCloseOfficeOverlayClicked), 160.0f, 40.0f)];

    return BuildPanel(TEXT("Decision History"), TEXT("Major choices and consequences for ongoing briefings, advisors, and save continuity."),
        SNew(SScrollBox)
        + SScrollBox::Slot()
        [Body], 900.0f);
}
TSharedRef<SWidget> ALoginHUD::BuildOfficeResourceChainsScreen()
{
    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Resource Chain Summary"), BuildResourceChainStatusText())]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 4.0f)
        [BuildInfoRow(TEXT("Resource Actions"), TEXT("These computer actions immediately update reserves, trade balance, treasury, shortage pressure, advisors, approval/stability causes, and decision history."))]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [BuildButton(TEXT("Emergency Imports"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleApplyResourceAction, FString(TEXT("Emergency Imports"))), 180.0f, 36.0f, bHasLoadedRuntimeState)]
            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [BuildButton(TEXT("Production Surge"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleApplyResourceAction, FString(TEXT("Production Surge"))), 180.0f, 36.0f, bHasLoadedRuntimeState)]
            + SHorizontalBox::Slot().AutoWidth()
            [BuildButton(TEXT("Reserve Rationing"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleApplyResourceAction, FString(TEXT("Reserve Rationing"))), 180.0f, 36.0f, bHasLoadedRuntimeState)]
        ];

    if (bHasLoadedRuntimeState)
    {
        for (const FDemocracyResourceChainEntry& Entry : LoadedSaveState.RuntimeState.ResourceChains.Chains)
        {
            Body->AddSlot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
            [BuildInfoRow(Entry.ResourceName, FString::Printf(TEXT("%s\nReserve %d/%d | production %d | consumption %d | imports %d | exports %d | shortage %d | surplus %d | strategic value %d\nDrivers: %s"),
                *Entry.Status,
                Entry.Reserve,
                Entry.ReserveTarget,
                Entry.Production,
                Entry.Consumption,
                Entry.Imports,
                Entry.Exports,
                Entry.Shortage,
                Entry.Surplus,
                Entry.StrategicValue,
                *FString::Join(Entry.Drivers, TEXT(", "))))];
        }
    }

    Body->AddSlot().AutoHeight().Padding(0.0f, 12.0f)
    [BuildButton(TEXT("Close"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleCloseOfficeOverlayClicked), 160.0f, 40.0f)];

    return BuildPanel(TEXT("Resource Chains"), TEXT("Production, consumption, shortages, imports, exports, reserves, and strategic role."),
        SNew(SScrollBox)
        + SScrollBox::Slot()
        [Body], 820.0f);
}
TSharedRef<SWidget> ALoginHUD::BuildOfficeDemographicsScreen()
{
    const FDemocracyDemographicsState& Demographics = LoadedSaveState.RuntimeState.Demographics;
    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("National Demographics"), BuildDemographicsStatusText())]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f)
        [BuildInfoRow(TEXT("Citizen Groups"), TEXT("Approval, needs, and unrest sources by group."))];

    for (const FDemocracyCitizenGroupState& Group : Demographics.CitizenGroups)
    {
        Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(Group.GroupName, FString::Printf(TEXT("share %d%% | approval %d | unrest pressure %d | needs F%d W%d J%d S%d H%d\nSources: %s"),
            Group.PopulationShare,
            Group.Approval,
            Group.UnrestPressure,
            Group.NeedFood,
            Group.NeedWater,
            Group.NeedJobs,
            Group.NeedSecurity,
            Group.NeedHealthcare,
            *FString::Join(Group.UnrestSources, TEXT(", "))))];
    }

    Body->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 4.0f)
    [BuildInfoRow(TEXT("Regions"), TEXT("Regional approval, access, stability, and unrest."))];

    for (const FDemocracyRegionState& Region : Demographics.Regions)
    {
        Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(Region.RegionName, FString::Printf(TEXT("%s | share %d%% | approval %d | stability %d | unrest %d | access F%d W%d jobs %d security %d infra %d\nSources: %s"),
            *Region.Climate,
            Region.PopulationShare,
            Region.Approval,
            Region.Stability,
            Region.Unrest,
            Region.FoodAccess,
            Region.WaterAccess,
            Region.Jobs,
            Region.Security,
            Region.Infrastructure,
            *FString::Join(Region.UnrestSources, TEXT(", "))))];
    }

    Body->AddSlot().AutoHeight().Padding(0.0f, 12.0f)
    [BuildButton(TEXT("Close"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleCloseOfficeOverlayClicked), 160.0f, 40.0f)];

    return BuildPanel(TEXT("Demographics Desk"), TEXT("Citizen groups, regions, needs, approval, and unrest sources."),
        SNew(SScrollBox)
        + SScrollBox::Slot()
        [Body], 860.0f);
}
FReply ALoginHUD::HandleRtsMapMouseWheel(const FGeometry& Geometry, const FPointerEvent& MouseEvent)
{
    const float OldZoom = FMath::Max(RtsMapZoom, 0.01f);
    const float ZoomStep = MouseEvent.GetWheelDelta() > 0.0f ? 0.25f : -0.25f;
    const float NewZoom = FMath::Clamp(RtsMapZoom + ZoomStep, 0.75f, 8.0f);
    if (!FMath::IsNearlyEqual(OldZoom, NewZoom))
    {
        const FVector2D ViewCenter = Geometry.GetLocalSize() * 0.5f;
        const FVector2D CursorLocal = Geometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
        if (bHasLoadedRuntimeState && LoadedSaveState.RuntimeState.RtsWorld.ActiveViewMode.Equals(TEXT("city_base"), ESearchCase::IgnoreCase))
        {
            const bool bZoomingOut = MouseEvent.GetWheelDelta() < 0.0f;
            RtsMapZoom = FMath::Clamp(RtsMapZoom + (bZoomingOut ? -0.35f : 0.35f), 4.5f, 6.2f);
            if (bZoomingOut && RtsMapZoom <= 4.51f)
            {
                EnterRtsRegionalCountryView(true);
            }
            else
            {
                const float CityViewScale = FMath::Clamp(0.72f + ((RtsMapZoom - 4.5f) / 1.7f) * 0.58f, 0.72f, 1.30f);
                RtsMapPan.X = FMath::Clamp(RtsMapPan.X, -980.0f * CityViewScale, 980.0f * CityViewScale);
                RtsMapPan.Y = FMath::Clamp(RtsMapPan.Y, -620.0f * CityViewScale, 620.0f * CityViewScale);
            }
        }
        else
        {
            const FVector2D NativeMapCenter(1121.0f, 552.0f);
            const FVector2D MapPointUnderCursor = (CursorLocal - ViewCenter - RtsMapPan) / OldZoom + NativeMapCenter;
            const bool bZoomingIn = MouseEvent.GetWheelDelta() > 0.0f;
            RtsMapZoom = NewZoom;
            if (bZoomingIn && OldZoom < 1.45f && RtsMapZoom >= 1.0f)
            {
                if (bHasLoadedRuntimeState)
                {
                    EnterRtsCountryOperationsView(true);
                }
                else
                {
                    RtsMapZoom = 2.35f;
                    RtsMapPan = CursorLocal - ViewCenter - ((MapPointUnderCursor - NativeMapCenter) * RtsMapZoom);
                    ClampRtsMapView();
                }
            }
            else if (bZoomingIn && OldZoom < 4.5f && RtsMapZoom >= 3.85f && bHasLoadedRuntimeState)
            {
                EnterRtsCityBaseView();
            }
            else
            {
                RtsMapPan = CursorLocal - ViewCenter - ((MapPointUnderCursor - NativeMapCenter) * RtsMapZoom);
                ClampRtsMapView();
            }
        }
        RefreshLoginWidget();
    }

    return FReply::Handled();
}
FReply ALoginHUD::HandleRtsMapMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& MouseEvent)
{
    if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        bIsDraggingRtsMap = true;
        bRtsMapDragMoved = false;
        RtsMapMouseDownScreenPosition = MouseEvent.GetScreenSpacePosition();
        LastRtsMapDragScreenPosition = RtsMapMouseDownScreenPosition;
        return FReply::Handled();
    }

    return FReply::Unhandled();
}

FReply ALoginHUD::HandleRtsMapMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& MouseEvent)
{
    if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        if (!bRtsMapDragMoved && FVector2D::Distance(RtsMapMouseDownScreenPosition, MouseEvent.GetScreenSpacePosition()) < 5.0f)
        {
            SelectRtsMapAtViewportPosition(Geometry, MouseEvent.GetScreenSpacePosition());
        }
        bIsDraggingRtsMap = false;
        bRtsMapDragMoved = false;
        return FReply::Handled();
    }

    return FReply::Unhandled();
}

FReply ALoginHUD::HandleRtsMapMouseMove(const FGeometry& Geometry, const FPointerEvent& MouseEvent)
{
    if (bIsDraggingRtsMap && MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
    {
        const FVector2D CurrentPosition = MouseEvent.GetScreenSpacePosition();
        const FVector2D Delta = CurrentPosition - LastRtsMapDragScreenPosition;
        if (!Delta.IsNearlyZero())
        {
            RtsMapPan += Delta;
            if (bHasLoadedRuntimeState && LoadedSaveState.RuntimeState.RtsWorld.ActiveViewMode.Equals(TEXT("city_base"), ESearchCase::IgnoreCase))
            {
                const float CityViewScale = FMath::Clamp(0.72f + ((RtsMapZoom - 4.5f) / 1.7f) * 0.58f, 0.72f, 1.30f);
                RtsMapPan.X = FMath::Clamp(RtsMapPan.X, -980.0f * CityViewScale, 980.0f * CityViewScale);
                RtsMapPan.Y = FMath::Clamp(RtsMapPan.Y, -620.0f * CityViewScale, 620.0f * CityViewScale);
                const float CityDragDistance = FMath::Max(FMath::Abs(RtsMapPan.X), FMath::Abs(RtsMapPan.Y));
                if (CityDragDistance > 260.0f)
                {
                    EnterRtsRegionalCountryView(true);
                }
                else if (CityDragDistance > 80.0f)
                {
                    const float DragAlpha = FMath::Clamp((CityDragDistance - 80.0f) / 180.0f, 0.0f, 1.0f);
                    RtsMapZoom = FMath::Lerp(5.25f, 4.50f, DragAlpha);
                }
                else
                {
                    RtsMapZoom = FMath::Min(5.35f, RtsMapZoom + 0.008f);
                }
            }
            else
            {
                ClampRtsMapView();
            }
            LastRtsMapDragScreenPosition = CurrentPosition;
            bRtsMapDragMoved = bRtsMapDragMoved || FVector2D::Distance(RtsMapMouseDownScreenPosition, CurrentPosition) >= 5.0f;
            RefreshLoginWidget();
        }
        return FReply::Handled();
    }

    if (bHasLoadedRuntimeState && WorldRtsEntryMode.Equals(TEXT("RtsEntry"), ESearchCase::IgnoreCase))
    {
        FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
        const FVector2D ViewCenter = Geometry.GetLocalSize() * 0.5f;
        const FVector2D CursorLocal = Geometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
        const FVector2D MapPoint = (CursorLocal - ViewCenter - RtsMapPan) / FMath::Max(RtsMapZoom, 0.01f) + FVector2D(1121.0f, 552.0f);
        FString NewHoveredTarget;
        if (MapPoint.X >= 0.0f && MapPoint.Y >= 0.0f && MapPoint.X <= 2242.0f && MapPoint.Y <= 1104.0f)
        {
            const int32 HoverCountryIndex = SampleDuliaCountryIdAtMapPoint(MapPoint);
            if (HoverCountryIndex > 0)
            {
                for (const FDemocracyCountryOwnershipState& Country : State.RtsWorld.Ownership.Countries)
                {
                    if (Country.MapCountryIndex == HoverCountryIndex)
                    {
                        bool bExactProvinceHit = false;
                        NewHoveredTarget = SelectDuliaProvinceIdAtMapPoint(State.RtsWorld.Ownership, Country, MapPoint, bExactProvinceHit);
                        break;
                    }
                }
            }
        }

        if (!State.RtsWorld.WorldInteraction.HoveredTargetId.Equals(NewHoveredTarget, ESearchCase::CaseSensitive))
        {
            State.RtsWorld.WorldInteraction.HoveredTargetId = NewHoveredTarget;
            RefreshLoginWidget();
            return FReply::Handled();
        }
    }

    bIsDraggingRtsMap = false;
    bRtsMapDragMoved = false;
    return FReply::Unhandled();
}

FReply ALoginHUD::HandleZoomRtsMapInClicked()
{
    const float OldZoom = FMath::Max(RtsMapZoom, 0.01f);
    const float NewZoom = FMath::Clamp(RtsMapZoom + 0.5f, 0.75f, 8.0f);
    if (!FMath::IsNearlyEqual(OldZoom, NewZoom))
    {
        if (NewZoom >= 4.5f && bHasLoadedRuntimeState)
        {
            EnterRtsCityBaseView();
        }
        else if (NewZoom >= 3.0f && bHasLoadedRuntimeState)
        {
            EnterRtsRegionalCountryView(true);
        }
        else if (NewZoom >= 1.35f && bHasLoadedRuntimeState)
        {
            RtsMapZoom = FMath::Max(NewZoom, 2.35f);
            EnterRtsCountryOperationsView(true);
        }
        else
        {
            RtsMapPan *= NewZoom / OldZoom;
            RtsMapZoom = NewZoom;
            ClampRtsMapView();
        }
        RefreshLoginWidget();
    }
    return FReply::Handled();
}

FReply ALoginHUD::HandleZoomRtsMapOutClicked()
{
    const float OldZoom = FMath::Max(RtsMapZoom, 0.01f);
    const float NewZoom = FMath::Clamp(RtsMapZoom - 0.5f, 0.75f, 8.0f);
    if (!FMath::IsNearlyEqual(OldZoom, NewZoom))
    {
        if (bHasLoadedRuntimeState && LoadedSaveState.RuntimeState.RtsWorld.ActiveViewMode.Equals(TEXT("city_base"), ESearchCase::IgnoreCase))
        {
            EnterRtsRegionalCountryView(true);
        }
        else if (NewZoom < 1.35f)
        {
            EnterRtsWorldMapView();
        }
        else
        {
            RtsMapPan *= NewZoom / OldZoom;
            RtsMapZoom = NewZoom;
            ClampRtsMapView();
        }
        RefreshLoginWidget();
    }
    return FReply::Handled();
}

FReply ALoginHUD::HandleResetRtsMapViewClicked()
{
    EnterRtsWorldMapView();
    RefreshLoginWidget();
    return FReply::Handled();
}
void ALoginHUD::EnterRtsWorldMapView()
{
    RtsMapZoom = 1.0f;
    RtsMapPan = FVector2D::ZeroVector;
    bIsDraggingRtsMap = false;
    bRtsMapDragMoved = false;
    if (bHasLoadedRuntimeState)
    {
        LoadedSaveState.RuntimeState.RtsWorld.ActiveViewMode = TEXT("World View");
    }
}

void ALoginHUD::EnterRtsCountryOperationsView(bool bFocusSelection)
{
    RtsMapZoom = 2.65f;
    RtsMapPan = FVector2D::ZeroVector;
    bIsDraggingRtsMap = false;
    bRtsMapDragMoved = false;
    if (bHasLoadedRuntimeState)
    {
        LoadedSaveState.RuntimeState.RtsWorld.ActiveViewMode = TEXT("Country Operations View");
        if (RtsSelectedCountryName.IsEmpty())
        {
            RtsSelectedCountryName = LoadedSaveState.RuntimeState.PlayerCountry.CountryName;
        }
        if (bFocusSelection)
        {
            FocusRtsMapOnSelection();
        }
        else
        {
            ClampRtsMapView();
        }
    }
}

void ALoginHUD::EnterRtsRegionalCountryView(bool bFocusSelection)
{
    RtsMapZoom = 3.65f;
    RtsMapPan = FVector2D::ZeroVector;
    bIsDraggingRtsMap = false;
    bRtsMapDragMoved = false;
    if (bHasLoadedRuntimeState)
    {
        LoadedSaveState.RuntimeState.RtsWorld.ActiveViewMode = TEXT("Regional Country View");
        if (RtsSelectedCountryName.IsEmpty())
        {
            RtsSelectedCountryName = LoadedSaveState.RuntimeState.PlayerCountry.CountryName;
        }
        if (bFocusSelection)
        {
            FocusRtsMapOnSelection();
            RtsMapZoom = 3.65f;
            FocusRtsMapOnSelection();
        }
        else
        {
            ClampRtsMapView();
        }
    }
}
void ALoginHUD::EnterRtsCityBaseView()
{
    RtsMapZoom = 5.10f;
    RtsMapPan = FVector2D::ZeroVector;
    bIsDraggingRtsMap = false;
    bRtsMapDragMoved = false;
    if (bHasLoadedRuntimeState)
    {
        LoadedSaveState.RuntimeState.RtsWorld.ActiveViewMode = TEXT("city_base");
    }
}
void ALoginHUD::ClampRtsMapView()
{
    RtsMapZoom = FMath::Clamp(RtsMapZoom, 0.75f, 8.0f);
    const float MaxPanX = FMath::Max(220.0f, 1121.0f * RtsMapZoom);
    const float MaxPanY = FMath::Max(160.0f, 552.0f * RtsMapZoom);
    RtsMapPan.X = FMath::Clamp(RtsMapPan.X, -MaxPanX, MaxPanX);
    RtsMapPan.Y = FMath::Clamp(RtsMapPan.Y, -MaxPanY, MaxPanY);
}

void ALoginHUD::FocusRtsMapOnSelection()
{
    if (!bHasLoadedRuntimeState)
    {
        return;
    }

    const FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    FString CountryName = RtsSelectedCountryName;
    if (!RtsSelectedProvinceId.IsEmpty())
    {
        for (const FDemocracyProvinceOwnershipState& Province : State.RtsWorld.Ownership.Provinces)
        {
            if (Province.ProvinceId.Equals(RtsSelectedProvinceId, ESearchCase::IgnoreCase))
            {
                CountryName = Province.OriginalCountryName;
                break;
            }
        }
    }
    if (CountryName.IsEmpty())
    {
        CountryName = State.PlayerCountry.CountryName;
    }

    const FDemocracyCountryOwnershipState* FocusCountry = nullptr;
    for (const FDemocracyCountryOwnershipState& Country : State.RtsWorld.Ownership.Countries)
    {
        if (Country.CountryName.Equals(CountryName, ESearchCase::IgnoreCase))
        {
            FocusCountry = &Country;
            break;
        }
        if (!FocusCountry && Country.bPlayerCountry)
        {
            FocusCountry = &Country;
        }
    }
    if (!FocusCountry)
    {
        return;
    }

    for (const FDuliaCountryMapPoint& Point : GetDuliaCountryMapPoints())
    {
        if (Point.CountryIndex == FocusCountry->MapCountryIndex)
        {
            RtsMapZoom = FMath::Max(RtsMapZoom, 1.65f);
            RtsMapPan = (FVector2D(1121.0f, 552.0f) - Point.Centroid) * RtsMapZoom;
            ClampRtsMapView();
            return;
        }
    }
}

FReply ALoginHUD::HandleFocusRtsMapSelectionClicked()
{
    FocusRtsMapOnSelection();
    RefreshLoginWidget();
    return FReply::Handled();
}

FReply ALoginHUD::HandleFocusRtsMapPlayerClicked()
{
    if (bHasLoadedRuntimeState)
    {
        FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
        RtsSelectedCountryName = State.PlayerCountry.CountryName;
        RtsSelectedProvinceId.Empty();
        RtsSelectedArmyId.Empty();

        for (const FDemocracyCountryOwnershipState& Country : State.RtsWorld.Ownership.Countries)
        {
            if (Country.bPlayerCountry || Country.CountryName.Equals(State.PlayerCountry.CountryName, ESearchCase::IgnoreCase))
            {
                RtsSelectedCountryName = Country.CountryName;
                RtsSelectedProvinceId = Country.ProvinceIds.Num() > 0 ? Country.ProvinceIds[0] : TEXT("");
                break;
            }
        }

        for (FDemocracyRtsArmyGroupState& Army : State.RtsWorld.ArmyGroups)
        {
            Army.bSelected = !RtsSelectedProvinceId.IsEmpty() && Army.CurrentProvinceId.Equals(RtsSelectedProvinceId, ESearchCase::IgnoreCase);
            if (Army.bSelected && RtsSelectedArmyId.IsEmpty())
            {
                RtsSelectedArmyId = Army.ArmyId;
            }
        }

        State.RtsWorld.WorldInteraction.ActiveSelectionId = RtsSelectedProvinceId.IsEmpty() ? RtsSelectedCountryName : RtsSelectedProvinceId;
        State.RtsWorld.WorldInteraction.ActiveSelectionType = RtsSelectedProvinceId.IsEmpty() ? TEXT("Country") : TEXT("Province");
        State.RtsWorld.WorldInteraction.LastInteractionSummary = FString::Printf(TEXT("Focused player country: %s."), *RtsSelectedCountryName);
        RefreshRtsHudState(State);
        FocusRtsMapOnSelection();
    }
    RefreshLoginWidget();
    return FReply::Handled();
}

FString ALoginHUD::GetRtsZoomModeLabel() const
{
    if (RtsMapZoom >= 4.5f)
    {
        return TEXT("Province / City View");
    }
    if (RtsMapZoom >= 3.0f)
    {
        return TEXT("Regional Country View");
    }
    if (RtsMapZoom >= 1.35f)
    {
        return TEXT("Country Operations View");
    }
    return TEXT("World View");
}

FString ALoginHUD::BuildRtsSelectedTerritoryText() const
{
    if (!bHasLoadedRuntimeState)
    {
        return TEXT("No state loaded.");
    }

    const FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    const FDemocracyProvinceOwnershipState* SelectedProvince = nullptr;
    const FDemocracyCountryOwnershipState* SelectedCountry = nullptr;

    for (const FDemocracyProvinceOwnershipState& Province : State.RtsWorld.Ownership.Provinces)
    {
        if (Province.ProvinceId.Equals(RtsSelectedProvinceId, ESearchCase::IgnoreCase))
        {
            SelectedProvince = &Province;
            break;
        }
    }

    const FString CountryNameToFind = SelectedProvince ? SelectedProvince->CurrentControllerCountryName : RtsSelectedCountryName;
    for (const FDemocracyCountryOwnershipState& Country : State.RtsWorld.Ownership.Countries)
    {
        if (Country.CountryName.Equals(CountryNameToFind, ESearchCase::IgnoreCase) || Country.CountryName.Equals(RtsSelectedCountryName, ESearchCase::IgnoreCase))
        {
            SelectedCountry = &Country;
            break;
        }
    }

    if (!SelectedProvince && !SelectedCountry)
    {
        const FString HoverText = State.RtsWorld.WorldInteraction.HoveredTargetId.IsEmpty() ? TEXT("none") : State.RtsWorld.WorldInteraction.HoveredTargetId;
        return FString::Printf(TEXT("Click a country or province to inspect it.\nHovered: %s\nMode: %s | Zoom %.0f%%"), *HoverText, *GetRtsZoomModeLabel(), RtsMapZoom * 100.0f);
    }

    TArray<FString> Lines;
    if (!State.RtsWorld.WorldInteraction.HoveredTargetId.IsEmpty())
    {
        Lines.Add(FString::Printf(TEXT("Hovered: %s"), *State.RtsWorld.WorldInteraction.HoveredTargetId));
    }
    if (SelectedCountry)
    {
        Lines.Add(FString::Printf(TEXT("Country: %s"), *SelectedCountry->CountryName));
        Lines.Add(FString::Printf(TEXT("Government: %s | map id %d"), *SelectedCountry->GovernmentType, SelectedCountry->MapCountryIndex));
        Lines.Add(FString::Printf(TEXT("Control: %d/%d provinces | occupied %d | lost %d | border %d"), SelectedCountry->ControlledProvinces, SelectedCountry->TotalProvinces, SelectedCountry->OccupiedProvinces, SelectedCountry->LostProvinces, SelectedCountry->BorderProvinces));
        Lines.Add(FString::Printf(TEXT("Resources: base %d | military %d | population %d | area %d"), SelectedCountry->ResourceBase, SelectedCountry->MilitaryValue, SelectedCountry->PopulationWeight, SelectedCountry->AreaWeight));
    }
    if (SelectedProvince)
    {
        Lines.Add(FString::Printf(TEXT("Province: %s (%s)"), *SelectedProvince->ProvinceName, *SelectedProvince->ProvinceId));
        Lines.Add(FString::Printf(TEXT("Owner: %s | controller: %s"), *SelectedProvince->CurrentOwnerCountryName, *SelectedProvince->CurrentControllerCountryName));
        Lines.Add(FString::Printf(TEXT("Terrain: %s | resource: %s | climate: %s"), *SelectedProvince->TerrainType, *SelectedProvince->ResourceFocus, *SelectedProvince->Climate));
        Lines.Add(FString::Printf(TEXT("Stability %d | unrest %d | strategic %d | border %s"), SelectedProvince->Stability, SelectedProvince->Unrest, SelectedProvince->StrategicValue, SelectedProvince->bBorderProvince ? TEXT("yes") : TEXT("no")));
        const bool bProvinceOccupied = !SelectedProvince->CurrentOwnerCountryName.Equals(SelectedProvince->CurrentControllerCountryName, ESearchCase::IgnoreCase);
        const int32 OccupationHeldTurns = bProvinceOccupied ? FMath::Max(0, State.Turn - SelectedProvince->LastChangedTurn) : 0;
        const int32 CaptureTurnsRemaining = bProvinceOccupied ? FMath::Max(0, 3 - OccupationHeldTurns) : 0;
        Lines.Add(FString::Printf(TEXT("Occupation: %s%s"), bProvinceOccupied ? TEXT("occupied/contested") : TEXT("owned/settled"), bProvinceOccupied ? *FString::Printf(TEXT(" | capture timer %d | peace required"), CaptureTurnsRemaining) : TEXT("")));
        for (const FDemocracyRtsFogProvinceState& FogProvince : State.RtsWorld.FogOfWar.Provinces)
        {
            if (FogProvince.ProvinceId.Equals(SelectedProvince->ProvinceId, ESearchCase::IgnoreCase))
            {
                Lines.Add(FString::Printf(TEXT("Fog/intel: %s | scout %d | %s"), *FogProvince.VisibilityState, FogProvince.ScoutStrength, *FogProvince.IntelSummary));
                break;
            }
        }
    }

    TArray<FString> PresentArmies;
    for (const FDemocracyRtsArmyGroupState& Army : State.RtsWorld.ArmyGroups)
    {
        if (SelectedProvince && Army.CurrentProvinceId.Equals(SelectedProvince->ProvinceId, ESearchCase::IgnoreCase))
        {
            PresentArmies.Add(FString::Printf(TEXT("%s strength %d order %s"), *Army.DisplayName, Army.TotalStrength, *Army.ActiveOrderType));
        }
    }
    Lines.Add(PresentArmies.Num() > 0 ? FString::Printf(TEXT("Armies: %s"), *FString::Join(PresentArmies, TEXT("; "))) : TEXT("Armies: none visible here."));

    TArray<FString> Buildings;
    for (const FDemocracyRtsBuildingState& Building : State.RtsWorld.CityBase.Buildings)
    {
        if (Buildings.Num() >= 5) { break; }
        Buildings.Add(FString::Printf(TEXT("%s L%d %s"), *Building.DisplayName, Building.Level, *Building.Status));
    }
    Lines.Add(Buildings.Num() > 0 ? FString::Printf(TEXT("Buildings: %s"), *FString::Join(Buildings, TEXT("; "))) : TEXT("Buildings: none loaded."));

    TArray<FString> Orders;
    for (const FDemocracyRtsMovementOrderState& Order : State.RtsWorld.MovementOrders)
    {
        if (SelectedProvince && (Order.SourceProvinceId.Equals(SelectedProvince->ProvinceId, ESearchCase::IgnoreCase) || Order.TargetProvinceId.Equals(SelectedProvince->ProvinceId, ESearchCase::IgnoreCase)))
        {
            Orders.Add(FString::Printf(TEXT("%s %s -> %s, %d turns"), *Order.OrderType, *Order.SourceProvinceId, *Order.TargetProvinceId, Order.TurnsRemaining));
        }
    }
    Lines.Add(Orders.Num() > 0 ? FString::Printf(TEXT("Current orders: %s"), *FString::Join(Orders, TEXT("; "))) : TEXT("Current orders: none for selected territory."));
    return FString::Join(Lines, TEXT("\n"));
}

FString ALoginHUD::BuildRtsSelectedArmyText() const
{
    if (!bHasLoadedRuntimeState)
    {
        return TEXT("No army data loaded.");
    }

    const FDemocracyRtsArmyGroupState* SelectedArmy = nullptr;
    for (const FDemocracyRtsArmyGroupState& Army : LoadedSaveState.RuntimeState.RtsWorld.ArmyGroups)
    {
        if ((!RtsSelectedArmyId.IsEmpty() && Army.ArmyId.Equals(RtsSelectedArmyId, ESearchCase::IgnoreCase)) || (RtsSelectedArmyId.IsEmpty() && Army.bSelected))
        {
            SelectedArmy = &Army;
            break;
        }
    }

    if (!SelectedArmy)
    {
        return TEXT("No army selected. Click a province with an army marker.");
    }

    return FString::Printf(TEXT("Army: %s\nStrength %d | morale %d | supply %d%s\nUnits: infantry %d | vehicles %d | aircraft %d | logistics %d | scouts %d | defense %d\nLocation: %s | destination: %s\nOrder: %s %s | movement: %s | turns %d\nOrders queued: %s"),
        *SelectedArmy->DisplayName,
        SelectedArmy->TotalStrength,
        SelectedArmy->Morale,
        SelectedArmy->SupplyStatus,
        SelectedArmy->bSupplyRouteBroken ? TEXT(" | BROKEN") : TEXT(""),
        SelectedArmy->InfantryCount,
        SelectedArmy->VehicleCount,
        SelectedArmy->AircraftCount,
        SelectedArmy->LogisticsCount,
        SelectedArmy->ScoutCount,
        SelectedArmy->DefensiveUnitCount,
        *SelectedArmy->CurrentProvinceId,
        SelectedArmy->DestinationProvinceId.IsEmpty() ? TEXT("none") : *SelectedArmy->DestinationProvinceId,
        *SelectedArmy->ActiveOrderType,
        SelectedArmy->OrderTargetProvinceId.IsEmpty() ? TEXT("") : *SelectedArmy->OrderTargetProvinceId,
        *SelectedArmy->MovementState,
        SelectedArmy->MovementTurnsRemaining,
        SelectedArmy->Orders.Num() > 0 ? *FString::Join(SelectedArmy->Orders, TEXT(", ")) : TEXT("none"));
}

FString ALoginHUD::BuildRtsActionText() const
{
    if (!bHasLoadedRuntimeState)
    {
        return TEXT("No RTS actions loaded.");
    }

    const FDemocracyRtsHudState& Hud = LoadedSaveState.RuntimeState.RtsWorld.Hud;
    TArray<FString> Lines;
    Lines.Add(PendingRtsOrderType.IsEmpty() ? TEXT("Order flow: click an army marker, choose an order, then click a destination province/country.") : FString::Printf(TEXT("Pending %s order: click a destination province/country."), *PendingRtsOrderType));
    Lines.Add(FString::Printf(TEXT("Orders: %s"), Hud.ArmyOrderButtons.Num() > 0 ? *FString::Join(Hud.ArmyOrderButtons, TEXT(" | ")) : TEXT("none")));
    Lines.Add(Hud.BuildMenuSummary);
    Lines.Add(FString::Printf(TEXT("Build: %s"), Hud.BuildMenuOptions.Num() > 0 ? *FString::Join(Hud.BuildMenuOptions, TEXT(" | ")) : TEXT("none")));
    Lines.Add(Hud.MinimapSummary);
    Lines.Add(FString::Printf(TEXT("Save/Load verification: armies %d | active orders %d/%d | supply %d | provinces %d controlled %d contested %d | fog %d | queues %d | battles %d | backflow history %d."),
        LoadedSaveState.RuntimeState.RtsWorld.ArmyGroups.Num(),
        LoadedSaveState.RuntimeState.RtsWorld.MovementOrders.FilterByPredicate([](const FDemocracyRtsMovementOrderState& Order) { return Order.bActive && !Order.bComplete && !Order.bCancelled; }).Num(),
        LoadedSaveState.RuntimeState.RtsWorld.MovementOrders.Num(),
        LoadedSaveState.RuntimeState.RtsWorld.SupplyRoutes.Num(),
        LoadedSaveState.RuntimeState.RtsWorld.Ownership.TotalProvinces,
        LoadedSaveState.RuntimeState.RtsWorld.Ownership.PlayerControlledProvinces,
        LoadedSaveState.RuntimeState.RtsWorld.Ownership.ContestedProvinces,
        LoadedSaveState.RuntimeState.RtsWorld.FogOfWar.Provinces.Num(),
        LoadedSaveState.RuntimeState.RtsWorld.CityBase.ConstructionQueue.Num(),
        LoadedSaveState.RuntimeState.RtsWorld.BattleHistory.Num(),
        LoadedSaveState.RuntimeState.RtsWorld.Backflow.OutcomeHistory.Num()));
    Lines.Add(FString::Printf(TEXT("Alerts: %s"), *Hud.AlertSummary));
    return FString::Join(Lines, TEXT("\n"));
}

FString ALoginHUD::BuildRtsBattlePresentationText() const
{
    if (!bHasLoadedRuntimeState)
    {
        return TEXT("No battle data loaded.");
    }

    const FDemocracyRtsWorldState& RtsWorld = LoadedSaveState.RuntimeState.RtsWorld;
    if (RtsWorld.BattleHistory.Num() == 0)
    {
        return TEXT("No battles resolved yet. Move an army into a hostile or contested province to generate a battle report.");
    }

    const FDemocracyRtsBattleResolutionState& Battle = RtsWorld.BattleHistory.Last();
    const FDemocracyRtsArmyGroupState* Army = nullptr;
    const FDemocracyProvinceOwnershipState* Province = nullptr;

    for (const FDemocracyRtsArmyGroupState& CandidateArmy : RtsWorld.ArmyGroups)
    {
        if (CandidateArmy.ArmyId.Equals(Battle.ArmyId, ESearchCase::IgnoreCase))
        {
            Army = &CandidateArmy;
            break;
        }
    }

    for (const FDemocracyProvinceOwnershipState& CandidateProvince : RtsWorld.Ownership.Provinces)
    {
        if (CandidateProvince.ProvinceId.Equals(Battle.ProvinceId, ESearchCase::IgnoreCase))
        {
            Province = &CandidateProvince;
            break;
        }
    }

    FString FollowUp;
    if (Battle.Result.Equals(TEXT("Province Captured"), ESearchCase::IgnoreCase))
    {
        FollowUp = TEXT("Hold the occupation timer, reinforce supply, prepare diplomacy pressure, or return to the office for stability management.");
    }
    else if (Battle.Result.Equals(TEXT("Battle Lost"), ESearchCase::IgnoreCase) || Battle.Result.Equals(TEXT("Retreated"), ESearchCase::IgnoreCase))
    {
        FollowUp = TEXT("Reinforce, defend the border, request alliance aid, negotiate ceasefire, or accept the province risk.");
    }
    else
    {
        FollowUp = TEXT("Scout, reinforce, reroute supply, attack again, or pause the front while simulation policy catches up.");
    }

    const FString ProvinceLabel = Province ? FString::Printf(TEXT("%s (%s)"), *Province->ProvinceName, *Province->ProvinceId) : Battle.ProvinceId;
    const bool bProvinceContested = Province && !Province->CurrentOwnerCountryName.Equals(Province->CurrentControllerCountryName, ESearchCase::IgnoreCase);
    const FString ProvinceStatus = Province ? FString::Printf(TEXT("%s, controller %s"), bProvinceContested ? TEXT("occupied/contested") : TEXT("stable"), *Province->CurrentControllerCountryName) : FString(TEXT("unknown"));

    TArray<FString> Lines;
    Lines.Add(FString::Printf(TEXT("Latest Battle: %s"), *Battle.Result));
    Lines.Add(FString::Printf(TEXT("Province: %s"), *ProvinceLabel));
    Lines.Add(FString::Printf(TEXT("Attacker: %s | Defender: %s"), Army ? *Army->DisplayName : *Battle.ArmyId, Battle.OpponentCountry.IsEmpty() ? TEXT("unknown") : *Battle.OpponentCountry));
    Lines.Add(FString::Printf(TEXT("Scores: attacker %d | defender %d"), Battle.PlayerScore, Battle.OpponentScore));
    Lines.Add(FString::Printf(TEXT("Modifiers: readiness %+d | terrain %+d | supply %+d | tech %+d | morale %+d | reinforcements %+d | defenses %+d"), Battle.ReadinessModifier, Battle.TerrainModifier, Battle.SupplyModifier, Battle.TechModifier, Battle.MoraleModifier, Battle.ReinforcementModifier, Battle.DefensiveStructureModifier));
    Lines.Add(FString::Printf(TEXT("Losses: infantry %d | vehicles %d | aircraft %d | logistics %d | scouts %d | defenses %d | retreat %s"), Battle.InfantryLosses, Battle.VehicleLosses, Battle.AircraftLosses, Battle.LogisticsLosses, Battle.ScoutLosses, Battle.DefensiveLosses, Battle.bRetreated ? TEXT("executed") : (Battle.bRetreatRecommended ? TEXT("recommended") : TEXT("not needed"))));
    Lines.Add(FString::Printf(TEXT("Terrain: %s | Province status: %s"),
        Battle.TerrainType.IsEmpty() ? TEXT("unknown") : *Battle.TerrainType,
        *ProvinceStatus));
    Lines.Add(FString::Printf(TEXT("Summary: %s"), Battle.Summary.IsEmpty() ? TEXT("No battle summary generated.") : *Battle.Summary));
    Lines.Add(FString::Printf(TEXT("Follow-up: %s"), Battle.FollowUpActions.Num() > 0 ? *FString::Join(Battle.FollowUpActions, TEXT(" | ")) : *FollowUp));
    return FString::Join(Lines, TEXT("\n"));
}

TSharedRef<SWidget> ALoginHUD::BuildRtsBattlePresentationWidget()
{
    if (!bHasLoadedRuntimeState)
    {
        return SNew(STextBlock)
            .Text(BodyText(TEXT("No battle data loaded.")))
            .AutoWrapText(true)
            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
            .ColorAndOpacity(FLinearColor(1.0f, 0.92f, 0.76f, 1.0f));
    }

    const FDemocracyRtsWorldState& RtsWorld = LoadedSaveState.RuntimeState.RtsWorld;
    if (RtsWorld.BattleHistory.Num() == 0)
    {
        return SNew(SBorder)
            .BorderImage(RowBrush.Get())
            .BorderBackgroundColor(FLinearColor(0.08f, 0.11f, 0.13f, 0.78f))
            .Padding(FMargin(10.0f, 8.0f))
            [
                SNew(STextBlock)
                .Text(BodyText(TEXT("No battles resolved yet. Move an army into a hostile or contested province to generate a battle report.")))
                .AutoWrapText(true)
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
                .ColorAndOpacity(FLinearColor(1.0f, 0.92f, 0.76f, 1.0f))
            ];
    }

    const FDemocracyRtsBattleResolutionState& Battle = RtsWorld.BattleHistory.Last();
    const FDemocracyRtsArmyGroupState* Army = nullptr;
    const FDemocracyProvinceOwnershipState* Province = nullptr;
    for (const FDemocracyRtsArmyGroupState& CandidateArmy : RtsWorld.ArmyGroups)
    {
        if (CandidateArmy.ArmyId.Equals(Battle.ArmyId, ESearchCase::IgnoreCase))
        {
            Army = &CandidateArmy;
            break;
        }
    }
    for (const FDemocracyProvinceOwnershipState& CandidateProvince : RtsWorld.Ownership.Provinces)
    {
        if (CandidateProvince.ProvinceId.Equals(Battle.ProvinceId, ESearchCase::IgnoreCase))
        {
            Province = &CandidateProvince;
            break;
        }
    }

    const FDemocracyRtsOutcomeState OutcomePreview = MakeRtsOutcomeFromBattle(LoadedSaveState.RuntimeState, Battle);
    const bool bCaptured = Battle.Result.Equals(TEXT("Province Captured"), ESearchCase::IgnoreCase);
    const bool bLost = Battle.Result.Equals(TEXT("Battle Lost"), ESearchCase::IgnoreCase);
    const FLinearColor ResultColor = bCaptured ? FLinearColor(0.05f, 0.75f, 0.26f, 0.95f) : (bLost ? FLinearColor(0.88f, 0.08f, 0.08f, 0.95f) : FLinearColor(0.95f, 0.68f, 0.16f, 0.95f));
    const FLinearColor ResultTextColor = bLost ? FLinearColor::White : FLinearColor::Black;
    const FString ProvinceLabel = Province ? FString::Printf(TEXT("%s (%s)"), *Province->ProvinceName, *Province->ProvinceId) : Battle.ProvinceId;
    const bool bProvinceContested = Province && !Province->CurrentOwnerCountryName.Equals(Province->CurrentControllerCountryName, ESearchCase::IgnoreCase);
    const FString ProvinceStatus = Province ? FString::Printf(TEXT("%s | owner %s | controller %s | stability %d | unrest %d"),
        bProvinceContested ? TEXT("Occupied/contested") : TEXT("Stable control"),
        *Province->CurrentOwnerCountryName,
        *Province->CurrentControllerCountryName,
        Province->Stability,
        Province->Unrest) : FString(TEXT("Unknown province state"));

    auto MetricRow = [this](const FString& Label, const FString& Value, const FLinearColor& Color) -> TSharedRef<SWidget>
    {
        return SNew(SBorder)
            .BorderImage(RowBrush.Get())
            .BorderBackgroundColor(Color)
            .Padding(FMargin(7.0f, 4.0f))
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(STextBlock)
                    .Text(BodyText(Label))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
                    .ColorAndOpacity(FLinearColor(0.82f, 0.92f, 0.96f, 1.0f))
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(STextBlock)
                    .Text(BodyText(Value))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
                    .ColorAndOpacity(FLinearColor::White)
                ]
            ];
    };

    auto FollowUpOrder = [this, Battle](const FString& OrderType) -> FReply
    {
        RtsSelectedArmyId = Battle.ArmyId;
        for (FDemocracyRtsArmyGroupState& CandidateArmy : LoadedSaveState.RuntimeState.RtsWorld.ArmyGroups)
        {
            CandidateArmy.bSelected = CandidateArmy.ArmyId.Equals(Battle.ArmyId, ESearchCase::IgnoreCase);
        }
        return HandleSelectRtsOrder(OrderType);
    };

    const FString PrimaryFollowUp = bCaptured ? TEXT("Defend") : TEXT("Reinforce");
    const FString SecondaryFollowUp = bCaptured ? TEXT("Reinforce") : (bLost ? TEXT("Defend") : TEXT("Patrol/Scout"));
    const FString TertiaryFollowUp = bCaptured ? TEXT("Patrol/Scout") : TEXT("Move");

    return SNew(SBorder)
        .BorderImage(RowBrush.Get())
        .BorderBackgroundColor(FLinearColor(0.07f, 0.09f, 0.11f, 0.86f))
        .Padding(FMargin(10.0f, 8.0f))
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
            [
                SNew(SBorder)
                .BorderImage(RowBrush.Get())
                .BorderBackgroundColor(ResultColor)
                .Padding(FMargin(8.0f, 5.0f))
                [
                    SNew(STextBlock)
                    .Text(BodyText(FString::Printf(TEXT("%s | %s"), *Battle.Result, *ProvinceLabel)))
                    .AutoWrapText(true)
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
                    .ColorAndOpacity(ResultTextColor)
                ]
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
            [
                SNew(STextBlock)
                .Text(BodyText(FString::Printf(TEXT("Attacker: %s\nDefender: %s\nProvince: %s"), Army ? *Army->DisplayName : *Battle.ArmyId, Battle.OpponentCountry.IsEmpty() ? TEXT("unknown") : *Battle.OpponentCountry, *ProvinceStatus)))
                .AutoWrapText(true)
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                .ColorAndOpacity(FLinearColor::White)
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 4.0f, 0.0f)[MetricRow(TEXT("ATTACK"), FString::FromInt(Battle.PlayerScore), FLinearColor(0.11f, 0.25f, 0.18f, 0.95f))]
                + SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 4.0f, 0.0f)[MetricRow(TEXT("DEFENSE"), FString::FromInt(Battle.OpponentScore), FLinearColor(0.24f, 0.12f, 0.14f, 0.95f))]
                + SHorizontalBox::Slot().FillWidth(1.0f)[MetricRow(TEXT("CASUALTIES"), FString::FromInt(OutcomePreview.Casualties), FLinearColor(0.22f, 0.18f, 0.12f, 0.95f))]
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
            [
                SNew(STextBlock)
                .Text(BodyText(FString::Printf(TEXT("Modifiers: readiness %+d | terrain %+d (%s) | supply %+d | tech %+d | morale %+d"), Battle.ReadinessModifier, Battle.TerrainModifier, Battle.TerrainType.IsEmpty() ? TEXT("unknown") : *Battle.TerrainType, Battle.SupplyModifier, Battle.TechModifier, Battle.MoraleModifier)))
                .AutoWrapText(true)
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                .ColorAndOpacity(FLinearColor(0.92f, 0.96f, 1.0f, 1.0f))
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
            [
                SNew(STextBlock)
                .Text(BodyText(FString::Printf(TEXT("Simulation backflow: territory %+d | resource disruption %+d | war fatigue %+d | stability %+d | budget strain %+d | diplomacy damage %+d | invasion risk %+d"), OutcomePreview.TerritoryDelta, OutcomePreview.ResourceDisruption, OutcomePreview.WarFatigueDelta, OutcomePreview.StabilityDelta, OutcomePreview.BudgetStrain, OutcomePreview.DiplomaticDamage, OutcomePreview.InvasionRiskDelta)))
                .AutoWrapText(true)
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                .ColorAndOpacity(FLinearColor(1.0f, 0.92f, 0.76f, 1.0f))
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
            [
                SNew(STextBlock)
                .Text(BodyText(Battle.Summary.IsEmpty() ? TEXT("No battle summary generated.") : Battle.Summary))
                .AutoWrapText(true)
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                .ColorAndOpacity(FLinearColor::White)
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 5.0f, 5.0f)
                [BuildButton(TEXT("Retreat"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleRtsBattleFollowUpClicked, FString(TEXT("Retreat"))), 88.0f, 30.0f, Army != nullptr)]
                + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 5.0f, 5.0f)
                [BuildButton(TEXT("Reinforce"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleRtsBattleFollowUpClicked, FString(TEXT("Reinforce"))), 104.0f, 30.0f, Army != nullptr)]
                + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 5.0f, 5.0f)
                [BuildButton(TEXT("Fortify"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleRtsBattleFollowUpClicked, FString(TEXT("Fortify"))), 88.0f, 30.0f)]
                + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 5.0f, 5.0f)
                [BuildButton(TEXT("Ceasefire"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleRtsBattleFollowUpClicked, FString(TEXT("Negotiate Ceasefire"))), 104.0f, 30.0f)]
                + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 5.0f, 5.0f)
                [BuildButton(TEXT("Hold Occ."), FOnClicked::CreateUObject(this, &ALoginHUD::HandleRtsBattleFollowUpClicked, FString(TEXT("Hold Occupation"))), 102.0f, 30.0f)]
                + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 5.0f, 5.0f)
                [BuildButton(TEXT("Abandon"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleRtsBattleFollowUpClicked, FString(TEXT("Abandon Occupation"))), 92.0f, 30.0f)]
            ]
        ];
}
FString ALoginHUD::BuildRtsCommandConsequenceText(const FString& OrderType, const FString& TargetProvinceId) const
{
    if (!bHasLoadedRuntimeState || OrderType.IsEmpty() || TargetProvinceId.IsEmpty())
    {
        return TEXT("No command pending.");
    }

    const FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    const FDemocracyProvinceOwnershipState* TargetProvince = FindRtsProvinceById(State, TargetProvinceId);
    const FDemocracyRtsArmyGroupState* Army = nullptr;
    for (const FDemocracyRtsArmyGroupState& CandidateArmy : State.RtsWorld.ArmyGroups)
    {
        if (CandidateArmy.ArmyId.Equals(RtsSelectedArmyId, ESearchCase::IgnoreCase))
        {
            Army = &CandidateArmy;
            break;
        }
    }

    if (!TargetProvince || !Army)
    {
        return TEXT("Command target or selected army is no longer valid.");
    }

    const bool bHostileTarget = !TargetProvince->CurrentControllerCountryName.Equals(State.PlayerCountry.CountryName, ESearchCase::IgnoreCase) && !IsRtsProvinceAlliedOrTreatyControlled(State, *TargetProvince);
    const bool bOccupiedTarget = !TargetProvince->CurrentOwnerCountryName.Equals(TargetProvince->CurrentControllerCountryName, ESearchCase::IgnoreCase);
    const bool bSameProvince = Army->CurrentProvinceId.Equals(TargetProvinceId, ESearchCase::IgnoreCase);
    const int32 DiplomacySupplyModifier = GetRtsDiplomacySupplyModifier(State, *TargetProvince);
    const int32 EstimatedTurns = FMath::Clamp((bSameProvince ? 1 : 2) + (Army->bSupplyRouteBroken ? 1 : 0) + (bHostileTarget ? 1 : 0) + FMath::Max(0, DiplomacySupplyModifier) / 12, 1, 6);
    const int32 ReadinessRisk = FMath::Clamp((100 - Army->SupplyStatus) / 4 + (Army->bSupplyRouteBroken ? 18 : 0) + (bHostileTarget ? 15 : 0) + FMath::Max(0, DiplomacySupplyModifier), 0, 100);
    const FString AuthorityBlockReason = GetRtsOrderAuthorityBlockReason(State, *Army, *TargetProvince, OrderType);

    TArray<FString> Lines;
    Lines.Add(FString::Printf(TEXT("Confirm %s order for %s."), *OrderType, *Army->DisplayName));
    Lines.Add(FString::Printf(TEXT("Target: %s | controller %s | owner %s | %s"), *TargetProvince->ProvinceName, *TargetProvince->CurrentControllerCountryName, *TargetProvince->CurrentOwnerCountryName, bHostileTarget ? TEXT("HOSTILE CONTACT LIKELY") : TEXT("friendly/controlled")));
    Lines.Add(FString::Printf(TEXT("ETA %d turn(s) | readiness risk %d | supply %d%% | morale %d | diplomacy supply modifier %+d"), EstimatedTurns, ReadinessRisk, Army->SupplyStatus, Army->Morale, DiplomacySupplyModifier));
    Lines.Add(AuthorityBlockReason.IsEmpty() ? TEXT("Authority: command can be confirmed under current war/diplomacy rules.") : FString::Printf(TEXT("Authority blocked: %s"), *AuthorityBlockReason));
    if (bHostileTarget)
    {
        Lines.Add(TEXT("Possible consequences: battle, casualties, occupation timer, war fatigue, diplomatic damage, and office alerts."));
    }
    else if (OrderType.Equals(TEXT("Reinforce"), ESearchCase::IgnoreCase))
    {
        Lines.Add(TEXT("Possible consequences: supply spending, stronger defense posture, and reduced readiness if routes are disrupted."));
    }
    else if (OrderType.Equals(TEXT("Patrol/Scout"), ESearchCase::IgnoreCase))
    {
        Lines.Add(TEXT("Possible consequences: reveals fog, may trigger contact in contested provinces, improves future battle information."));
    }
    else
    {
        Lines.Add(TEXT("Possible consequences: movement commitment, supply route recalculation, and delayed response elsewhere."));
    }
    if (bOccupiedTarget)
    {
        Lines.Add(TEXT("Province is already occupied/contested. Peace or held control may be required before ownership is final."));
    }
    Lines.Add(TEXT("Confirm to execute, or cancel to choose a different target."));
    return FString::Join(Lines, TEXT("\n"));
}

FString ALoginHUD::BuildRtsConstructionAvailabilityText(const FDemocracyRtsBuildingState* Building, int32 SlotIndex, const FString& SlotFocus, bool bUpgrade) const
{
    if (!bHasLoadedRuntimeState)
    {
        return TEXT("No runtime state loaded.");
    }

    const FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    const FDemocracyRtsCityBaseState& Base = State.RtsWorld.CityBase;
    FDemocracyRtsBuildingState PreviewBuilding = Building ? *Building : MakeRtsPlaceholderBuildingForSlot(SlotIndex, SlotFocus);
    const int32 TreasuryCost = bUpgrade ? PreviewBuilding.UpgradeCost : PreviewBuilding.BuildCost;
    const int32 FoodCost = !bUpgrade && SlotFocus.Equals(TEXT("Food"), ESearchCase::IgnoreCase) ? 8 : 0;
    const int32 FuelCost = bUpgrade ? FMath::Max(0, TreasuryCost / 10) : ((SlotFocus.Equals(TEXT("Fuel"), ESearchCase::IgnoreCase) || SlotFocus.Equals(TEXT("Logistics"), ESearchCase::IgnoreCase)) ? 8 : 4);
    const int32 WoodCost = bUpgrade ? FMath::Max(0, TreasuryCost / 5) : (SlotFocus.Equals(TEXT("Wood"), ESearchCase::IgnoreCase) ? 8 : 14);
    const int32 MetalsCost = bUpgrade ? FMath::Max(0, TreasuryCost / 6) : ((SlotFocus.Equals(TEXT("Metals"), ESearchCase::IgnoreCase) || SlotFocus.Equals(TEXT("Security"), ESearchCase::IgnoreCase)) ? 12 : 8);
    const bool bQueued = HasActiveRtsConstructionQueue(Base, PreviewBuilding.BuildingId);
    const bool bCanPay = CanPayRtsCost(State.PlayerCountry, TreasuryCost, FoodCost, FuelCost, WoodCost, MetalsCost);
    const bool bDisabled = PreviewBuilding.bDisabled || PreviewBuilding.DamagePercent >= 75;
    const bool bUnavailable = bQueued || !bCanPay || bDisabled || (bUpgrade && (!PreviewBuilding.bConstructed || PreviewBuilding.bUpgradeQueued));

    TArray<FString> Reasons;
    if (bQueued) { Reasons.Add(TEXT("queue active")); }
    if (!bCanPay) { Reasons.Add(TEXT("insufficient resources")); }
    if (bDisabled) { Reasons.Add(PreviewBuilding.DisabledReason.IsEmpty() ? TEXT("building disabled/damaged") : PreviewBuilding.DisabledReason); }
    if (bUpgrade && !PreviewBuilding.bConstructed) { Reasons.Add(TEXT("not constructed")); }
    if (bUpgrade && PreviewBuilding.bUpgradeQueued) { Reasons.Add(TEXT("upgrade already queued")); }

    return FString::Printf(TEXT("%s: %s | timer %d turn(s) | output preview %+d -> %+d | %s%s"),
        bUpgrade ? TEXT("Upgrade") : TEXT("Build"),
        *BuildRtsCostText(TreasuryCost, FoodCost, FuelCost, WoodCost, MetalsCost),
        FMath::Max(1, PreviewBuilding.BuildTimeTurns + (bUpgrade ? 1 : 0)),
        PreviewBuilding.ProductionPerTick,
        PreviewBuilding.ProductionPerTick + FMath::Max(2, PreviewBuilding.Level + 1),
        bUnavailable ? TEXT("DISABLED") : TEXT("AVAILABLE"),
        Reasons.Num() > 0 ? *FString::Printf(TEXT(" (%s)"), *FString::Join(Reasons, TEXT(", "))) : TEXT(""));
}
FString ALoginHUD::BuildRtsCityBaseSummaryText() const
{
    if (!bHasLoadedRuntimeState)
    {
        return TEXT("No city/base data loaded.");
    }

    const FDemocracyRtsWorldState& RtsWorld = LoadedSaveState.RuntimeState.RtsWorld;
    const FDemocracyRtsCityBaseState& Base = RtsWorld.CityBase;
    TArray<FString> Lines;
    Lines.Add(FString::Printf(TEXT("Base: %s | linked country %s | province %s | grid %dx%d"), *Base.DisplayName, Base.LinkedCountryName.IsEmpty() ? TEXT("unassigned") : *Base.LinkedCountryName, Base.LinkedProvinceId.IsEmpty() ? TEXT("unassigned") : *Base.LinkedProvinceId, Base.GridWidth, Base.GridHeight));
    Lines.Add(Base.BaseSummary);
    Lines.Add(FString::Printf(TEXT("Buildings %d | build queue %d | upgrade queue %d | resources %s"), Base.Buildings.Num(), Base.BuildQueueCount, Base.UpgradeQueueCount, *RtsWorld.Hud.ResourceSummary));
    if (Base.RuntimeNotes.Num() > 0)
    {
        Lines.Add(FString::Printf(TEXT("Notes: %s"), *FString::Join(Base.RuntimeNotes, TEXT(" | "))));
    }
    if (Base.ConstructionQueue.Num() > 0)
    {
        TArray<FString> QueueLines;
        for (const FDemocracyRtsConstructionQueueEntryState& QueueEntry : Base.ConstructionQueue)
        {
            QueueLines.Add(FString::Printf(TEXT("%s %s -> L%d, %d/%d turns remaining%s"), *QueueEntry.QueueType, *QueueEntry.DisplayName, QueueEntry.TargetLevel, QueueEntry.TurnsRemaining, QueueEntry.TotalTurns, QueueEntry.bComplete ? TEXT(" complete") : TEXT("")));
        }
        Lines.Add(FString::Printf(TEXT("Construction: %s"), *FString::Join(QueueLines, TEXT("; "))));
    }
    else if (Base.BuildQueue.Num() > 0)
    {
        Lines.Add(FString::Printf(TEXT("Queued placeholders: %s"), *FString::Join(Base.BuildQueue, TEXT("; "))));
    }
    else
    {
        Lines.Add(TEXT("Construction: no active queue items."));
    }
    return FString::Join(Lines, TEXT("\n"));
}

FString ALoginHUD::BuildRtsOfficeAlertText() const
{
    if (!bHasLoadedRuntimeState)
    {
        return TEXT("No RTS alert state loaded.");
    }

    const FDemocracyRtsBackflowState& Backflow = LoadedSaveState.RuntimeState.RtsWorld.Backflow;
    TArray<FString> Lines;
    Lines.Add(FString::Printf(TEXT("Pending RTS alerts %d | applied RTS results %d | casualties %d | territory %+d | disruption %d | fatigue %d | budget strain %d"), Backflow.PendingOutcomes.Num(), Backflow.OutcomeHistory.Num(), Backflow.TotalCasualties, Backflow.TotalTerritoryDelta, Backflow.ResourceDisruptionPressure, Backflow.WarFatigue, Backflow.BudgetStrainPressure));
    const int32 PendingDisplayCount = FMath::Min(Backflow.PendingOutcomes.Num(), 5);
    for (int32 Index = 0; Index < PendingDisplayCount; ++Index)
    {
        Lines.Add(BuildRtsOutcomeOfficeAlertLine(Backflow.PendingOutcomes[Index]));
    }
    const int32 HistoryStart = FMath::Max(0, Backflow.OutcomeHistory.Num() - 3);
    for (int32 Index = Backflow.OutcomeHistory.Num() - 1; Index >= HistoryStart; --Index)
    {
        const FDemocracyRtsOutcomeState& Outcome = Backflow.OutcomeHistory[Index];
        Lines.Add(FString::Printf(TEXT("Applied RTS result turn %d: %s"), Outcome.Turn, *BuildRtsOutcomeOfficeAlertLine(Outcome)));
    }
    if (Lines.Num() == 1)
    {
        Lines.Add(TEXT("No tactical results are waiting for office attention."));
    }
    return FString::Join(Lines, TEXT("\n"));
}

FReply ALoginHUD::HandleSelectRtsBuildingSlot(int32 SlotIndex)
{
    if (!bHasLoadedRuntimeState)
    {
        return FReply::Handled();
    }

    FDemocracyRtsWorldState& RtsWorld = LoadedSaveState.RuntimeState.RtsWorld;
    const FDemocracyRtsBuildingState* Building = RtsWorld.CityBase.Buildings.IsValidIndex(SlotIndex) ? &RtsWorld.CityBase.Buildings[SlotIndex] : nullptr;
    RtsWorld.WorldInteraction.ActiveSelectionType = Building ? TEXT("City Building") : TEXT("Empty Building Slot");
    RtsWorld.WorldInteraction.ActiveSelectionId = Building ? Building->BuildingId : FString::Printf(TEXT("empty_slot_%02d"), SlotIndex + 1);
    RtsWorld.WorldInteraction.LastInteractionSummary = Building
        ? FString::Printf(TEXT("Selected %s: L%d | %s | output %+d | %s."), *Building->DisplayName, Building->Level, *Building->Status, Building->ProductionPerTick, *BuildRtsCostText(Building->UpgradeCost, 0, FMath::Max(0, Building->UpgradeCost / 10), FMath::Max(0, Building->UpgradeCost / 5), FMath::Max(0, Building->UpgradeCost / 6)))
        : FString::Printf(TEXT("Selected empty city/base slot %02d. Suggested focus: %s."), SlotIndex + 1, *GetRtsSlotResourceFocus(SlotIndex));
    RefreshRtsHudState(LoadedSaveState.RuntimeState);
    RefreshLoginWidget();
    return FReply::Handled();
}

FReply ALoginHUD::HandleQueueRtsBuildSlot(int32 SlotIndex, FString ResourceFocus)
{
    if (!bHasLoadedRuntimeState)
    {
        return FReply::Handled();
    }

    FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    FDemocracyRtsCityBaseState& Base = State.RtsWorld.CityBase;
    if (Base.Buildings.IsValidIndex(SlotIndex) && Base.Buildings[SlotIndex].bConstructed)
    {
        Base.RuntimeNotes.Add(FString::Printf(TEXT("Slot %02d already has %s. Use upgrade instead."), SlotIndex + 1, *Base.Buildings[SlotIndex].DisplayName));
        RefreshLoginWidget();
        return FReply::Handled();
    }

    FDemocracyRtsBuildingState Building = Base.Buildings.IsValidIndex(SlotIndex) ? Base.Buildings[SlotIndex] : MakeRtsPlaceholderBuildingForSlot(SlotIndex, ResourceFocus);
    if (HasActiveRtsConstructionQueue(Base, Building.BuildingId))
    {
        Base.RuntimeNotes.Add(FString::Printf(TEXT("%s already has an active queue item."), *Building.DisplayName));
        RefreshLoginWidget();
        return FReply::Handled();
    }

    const int32 TreasuryCost = Building.BuildCost;
    const int32 FoodCost = ResourceFocus.Equals(TEXT("Food"), ESearchCase::IgnoreCase) ? 8 : 0;
    const int32 FuelCost = ResourceFocus.Equals(TEXT("Fuel"), ESearchCase::IgnoreCase) || ResourceFocus.Equals(TEXT("Logistics"), ESearchCase::IgnoreCase) ? 8 : 4;
    const int32 WoodCost = ResourceFocus.Equals(TEXT("Wood"), ESearchCase::IgnoreCase) ? 8 : 14;
    const int32 MetalsCost = ResourceFocus.Equals(TEXT("Metals"), ESearchCase::IgnoreCase) || ResourceFocus.Equals(TEXT("Security"), ESearchCase::IgnoreCase) ? 12 : 8;
    if (!CanPayRtsCost(State.PlayerCountry, TreasuryCost, FoodCost, FuelCost, WoodCost, MetalsCost))
    {
        Base.RuntimeNotes.Add(FString::Printf(TEXT("Cannot build %s: insufficient resources. %s."), *Building.DisplayName, *BuildRtsCostText(TreasuryCost, FoodCost, FuelCost, WoodCost, MetalsCost)));
        RefreshLoginWidget();
        return FReply::Handled();
    }

    ApplyRtsCost(State.PlayerCountry, TreasuryCost, FoodCost, FuelCost, WoodCost, MetalsCost);
    Building.bConstructed = false;
    Building.Status = TEXT("Building");
    if (Base.Buildings.IsValidIndex(SlotIndex))
    {
        Base.Buildings[SlotIndex] = Building;
    }
    else
    {
        while (Base.Buildings.Num() < SlotIndex)
        {
            Base.Buildings.Add(MakeRtsPlaceholderBuildingForSlot(Base.Buildings.Num(), GetRtsSlotResourceFocus(Base.Buildings.Num())));
        }
        Base.Buildings.Add(Building);
    }

    FDemocracyRtsConstructionQueueEntryState QueueEntry;
    QueueEntry.QueueId = FString::Printf(TEXT("queue_build_%s_%d"), *Building.BuildingId, State.Turn);
    QueueEntry.BuildingId = Building.BuildingId;
    QueueEntry.DisplayName = FString::Printf(TEXT("Build %s"), *Building.DisplayName);
    QueueEntry.QueueType = TEXT("Build");
    QueueEntry.TargetLevel = 1;
    QueueEntry.TotalTurns = FMath::Max(1, Building.BuildTimeTurns);
    QueueEntry.TurnsRemaining = QueueEntry.TotalTurns;
    QueueEntry.FoodCost = FoodCost;
    QueueEntry.FuelCost = FuelCost;
    QueueEntry.WoodCost = WoodCost;
    QueueEntry.MetalsCost = MetalsCost;
    QueueEntry.TreasuryCost = TreasuryCost;
    QueueEntry.Prerequisites = Building.Prerequisites;
    Base.ConstructionQueue.Add(QueueEntry);
    RecountRtsCityBaseQueues(Base);
    Base.RuntimeNotes.Add(FString::Printf(TEXT("Queued %s in slot %02d. %s. Completes in %d turn(s)."), *Building.DisplayName, SlotIndex + 1, *BuildRtsCostText(TreasuryCost, FoodCost, FuelCost, WoodCost, MetalsCost), QueueEntry.TotalTurns));
    State.RtsWorld.WorldInteraction.ActiveSelectionType = TEXT("City Building Queue");
    State.RtsWorld.WorldInteraction.ActiveSelectionId = Building.BuildingId;
    State.RtsWorld.WorldInteraction.LastInteractionSummary = Base.RuntimeNotes.Last();
    RefreshRtsHudState(State);
    RefreshLoginWidget();
    return FReply::Handled();
}

FReply ALoginHUD::HandleQueueRtsUpgrade(FString BuildingId)
{
    if (!bHasLoadedRuntimeState || BuildingId.IsEmpty())
    {
        return FReply::Handled();
    }

    FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    FDemocracyRtsCityBaseState& Base = State.RtsWorld.CityBase;
    FDemocracyRtsBuildingState* Building = nullptr;
    for (FDemocracyRtsBuildingState& Candidate : Base.Buildings)
    {
        if (Candidate.BuildingId.Equals(BuildingId, ESearchCase::IgnoreCase))
        {
            Building = &Candidate;
            break;
        }
    }
    if (!Building || !Building->bConstructed || Building->bDisabled || HasActiveRtsConstructionQueue(Base, BuildingId))
    {
        Base.RuntimeNotes.Add(Building ? FString::Printf(TEXT("Cannot upgrade %s right now."), *Building->DisplayName) : TEXT("Cannot upgrade: building not found."));
        RefreshLoginWidget();
        return FReply::Handled();
    }

    const int32 TreasuryCost = Building->UpgradeCost;
    const int32 FoodCost = 0;
    const int32 FuelCost = FMath::Max(4, Building->UpgradeCost / 12);
    const int32 WoodCost = FMath::Max(8, Building->UpgradeCost / 5);
    const int32 MetalsCost = FMath::Max(8, Building->UpgradeCost / 6);
    if (!CanPayRtsCost(State.PlayerCountry, TreasuryCost, FoodCost, FuelCost, WoodCost, MetalsCost))
    {
        Base.RuntimeNotes.Add(FString::Printf(TEXT("Cannot upgrade %s: insufficient resources. %s."), *Building->DisplayName, *BuildRtsCostText(TreasuryCost, FoodCost, FuelCost, WoodCost, MetalsCost)));
        RefreshLoginWidget();
        return FReply::Handled();
    }

    ApplyRtsCost(State.PlayerCountry, TreasuryCost, FoodCost, FuelCost, WoodCost, MetalsCost);
    Building->bUpgradeQueued = true;
    Building->Status = TEXT("Upgrade Queued");
    FDemocracyRtsConstructionQueueEntryState QueueEntry;
    QueueEntry.QueueId = FString::Printf(TEXT("queue_upgrade_%s_%d"), *Building->BuildingId, State.Turn);
    QueueEntry.BuildingId = Building->BuildingId;
    QueueEntry.DisplayName = FString::Printf(TEXT("Upgrade %s"), *Building->DisplayName);
    QueueEntry.QueueType = TEXT("Upgrade");
    QueueEntry.TargetLevel = Building->Level + 1;
    QueueEntry.TotalTurns = FMath::Clamp(Building->BuildTimeTurns + Building->Level, 1, 6);
    QueueEntry.TurnsRemaining = QueueEntry.TotalTurns;
    QueueEntry.FoodCost = FoodCost;
    QueueEntry.FuelCost = FuelCost;
    QueueEntry.WoodCost = WoodCost;
    QueueEntry.MetalsCost = MetalsCost;
    QueueEntry.TreasuryCost = TreasuryCost;
    QueueEntry.Prerequisites = Building->Prerequisites;
    Base.ConstructionQueue.Add(QueueEntry);
    RecountRtsCityBaseQueues(Base);
    Base.RuntimeNotes.Add(FString::Printf(TEXT("Queued upgrade for %s to level %d. %s. Completes in %d turn(s)."), *Building->DisplayName, QueueEntry.TargetLevel, *BuildRtsCostText(TreasuryCost, FoodCost, FuelCost, WoodCost, MetalsCost), QueueEntry.TotalTurns));
    State.RtsWorld.WorldInteraction.ActiveSelectionType = TEXT("City Upgrade Queue");
    State.RtsWorld.WorldInteraction.ActiveSelectionId = Building->BuildingId;
    State.RtsWorld.WorldInteraction.LastInteractionSummary = Base.RuntimeNotes.Last();
    RefreshRtsHudState(State);
    RefreshLoginWidget();
    return FReply::Handled();
}

FReply ALoginHUD::HandleCancelRtsConstruction(FString QueueId)
{
    if (!bHasLoadedRuntimeState || QueueId.IsEmpty())
    {
        return FReply::Handled();
    }

    FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    FDemocracyRtsCityBaseState& Base = State.RtsWorld.CityBase;
    for (FDemocracyRtsConstructionQueueEntryState& QueueEntry : Base.ConstructionQueue)
    {
        if (!QueueEntry.QueueId.Equals(QueueId, ESearchCase::IgnoreCase) || QueueEntry.bCancelled || QueueEntry.bComplete || !QueueEntry.bCanCancel)
        {
            continue;
        }

        QueueEntry.bCancelled = true;
        RefundRtsCost(State.PlayerCountry, QueueEntry);
        for (FDemocracyRtsBuildingState& Building : Base.Buildings)
        {
            if (Building.BuildingId.Equals(QueueEntry.BuildingId, ESearchCase::IgnoreCase))
            {
                Building.bUpgradeQueued = false;
                if (QueueEntry.QueueType.Equals(TEXT("Build"), ESearchCase::IgnoreCase) && !Building.bConstructed)
                {
                    Building.Status = TEXT("Cancelled Site");
                    Building.CurrentHealth = 0;
                }
                else
                {
                    Building.Status = TEXT("Operational");
                }
                break;
            }
        }
        RecountRtsCityBaseQueues(Base);
        Base.RuntimeNotes.Add(FString::Printf(TEXT("Cancelled %s. Half of the unspent cost was refunded."), *QueueEntry.DisplayName));
        State.RtsWorld.WorldInteraction.LastInteractionSummary = Base.RuntimeNotes.Last();
        RefreshRtsHudState(State);
        RefreshLoginWidget();
        return FReply::Handled();
    }

    Base.RuntimeNotes.Add(FString::Printf(TEXT("Could not cancel queue item %s."), *QueueId));
    RefreshLoginWidget();
    return FReply::Handled();
}
FReply ALoginHUD::HandleRtsBattleFollowUpClicked(FString FollowUpAction)
{
    if (!bHasLoadedRuntimeState || LoadedSaveState.RuntimeState.RtsWorld.BattleHistory.Num() == 0)
    {
        return FReply::Handled();
    }

    FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    const FDemocracyRtsBattleResolutionState& Battle = State.RtsWorld.BattleHistory.Last();
    FDemocracyRtsArmyGroupState* Army = nullptr;
    for (FDemocracyRtsArmyGroupState& CandidateArmy : State.RtsWorld.ArmyGroups)
    {
        CandidateArmy.bSelected = CandidateArmy.ArmyId.Equals(Battle.ArmyId, ESearchCase::IgnoreCase);
        if (CandidateArmy.bSelected)
        {
            Army = &CandidateArmy;
            RtsSelectedArmyId = CandidateArmy.ArmyId;
        }
    }

    FDemocracyProvinceOwnershipState* Province = FindMutableRtsProvinceById(State, Battle.ProvinceId);
    FString Summary;
    int32 Severity = 30;

    if (FollowUpAction.Equals(TEXT("Retreat"), ESearchCase::IgnoreCase) && Army)
    {
        FString RetreatProvince = Army->CurrentProvinceId;
        for (const FDemocracyProvinceOwnershipState& CandidateProvince : State.RtsWorld.Ownership.Provinces)
        {
            if (CandidateProvince.bPlayerControlled)
            {
                RetreatProvince = CandidateProvince.ProvinceId;
                break;
            }
        }
        Army->DestinationProvinceId = RetreatProvince;
        Army->OrderTargetProvinceId = RetreatProvince;
        Army->ActiveOrderType = TEXT("Retreat");
        Army->MovementState = TEXT("Retreating");
        Army->MovementTurnsRemaining = FMath::Max(1, Army->MovementTurnsRemaining);
        Army->Morale = FMath::Clamp(Army->Morale - 4, 0, 100);
        Army->SupplyStatus = FMath::Clamp(Army->SupplyStatus + 10, 0, 100);
        Army->Orders.AddUnique(TEXT("Retreat from recent battle"));
        Summary = FString::Printf(TEXT("%s is retreating toward %s. Morale -4, supply +10 while command regroups."), *Army->DisplayName, *RetreatProvince);
        Severity = 45;
    }
    else if (FollowUpAction.Equals(TEXT("Reinforce"), ESearchCase::IgnoreCase) && Army)
    {
        const int32 TreasuryCost = 45, FoodCost = 10, FuelCost = 8, WoodCost = 0, MetalsCost = 12;
        if (CanPayRtsCost(State.PlayerCountry, TreasuryCost, FoodCost, FuelCost, WoodCost, MetalsCost))
        {
            ApplyRtsCost(State.PlayerCountry, TreasuryCost, FoodCost, FuelCost, WoodCost, MetalsCost);
            Army->InfantryCount += 6;
            Army->LogisticsCount += 1;
            Army->SupplyStatus = FMath::Clamp(Army->SupplyStatus + 8, 0, 100);
            Army->Morale = FMath::Clamp(Army->Morale + 3, 0, 100);
            Army->TotalStrength = FMath::Clamp(Army->InfantryCount * 8 + Army->VehicleCount * 22 + Army->AircraftCount * 28 + Army->LogisticsCount * 6 + Army->ScoutCount * 5 + Army->DefensiveUnitCount * 18, 0, 999);
            Summary = FString::Printf(TEXT("%s reinforced after battle. Infantry +6, logistics +1, supply +8. %s."), *Army->DisplayName, *BuildRtsCostText(TreasuryCost, FoodCost, FuelCost, WoodCost, MetalsCost));
        }
        else
        {
            Summary = TEXT("Reinforcement blocked: treasury/resources are too low for emergency reinforcement.");
            Severity = 55;
        }
    }
    else if (FollowUpAction.Equals(TEXT("Fortify"), ESearchCase::IgnoreCase))
    {
        const int32 TreasuryCost = 35, FoodCost = 0, FuelCost = 4, WoodCost = 18, MetalsCost = 14;
        if (Province && CanPayRtsCost(State.PlayerCountry, TreasuryCost, FoodCost, FuelCost, WoodCost, MetalsCost))
        {
            ApplyRtsCost(State.PlayerCountry, TreasuryCost, FoodCost, FuelCost, WoodCost, MetalsCost);
            Province->StrategicValue = FMath::Clamp(Province->StrategicValue + 2, 1, 100);
            Province->Stability = FMath::Clamp(Province->Stability + 3, 0, 100);
            Province->Unrest = FMath::Clamp(Province->Unrest - 4, 0, 100);
            if (Army)
            {
                Army->DefensiveUnitCount += 1;
                Army->TotalStrength = FMath::Clamp(Army->InfantryCount * 8 + Army->VehicleCount * 22 + Army->AircraftCount * 28 + Army->LogisticsCount * 6 + Army->ScoutCount * 5 + Army->DefensiveUnitCount * 18, 0, 999);
            }
            Summary = FString::Printf(TEXT("Fortified %s. Strategic value +2, stability +3, unrest -4. %s."), *Province->ProvinceName, *BuildRtsCostText(TreasuryCost, FoodCost, FuelCost, WoodCost, MetalsCost));
        }
        else
        {
            Summary = TEXT("Fortify blocked: select a valid province and confirm enough wood/metals/treasury are available.");
            Severity = 50;
        }
    }
    else if (FollowUpAction.Equals(TEXT("Negotiate Ceasefire"), ESearchCase::IgnoreCase))
    {
        State.DiplomacyMatrix.Summary = FString::Printf(TEXT("Ceasefire channel opened after RTS battle in %s. Follow through from the office diplomacy dashboard."), Province ? *Province->ProvinceName : *Battle.ProvinceId);
        State.InvasionRisk.CurrentInvasionRisk = FMath::Clamp(State.InvasionRisk.CurrentInvasionRisk - 3, 0, 100);
        Summary = FString::Printf(TEXT("Ceasefire negotiation channel opened after battle at %s. Invasion risk -3 while talks are active."), Province ? *Province->ProvinceName : *Battle.ProvinceId);
        Severity = 35;
    }
    else if (FollowUpAction.Equals(TEXT("Hold Occupation"), ESearchCase::IgnoreCase) && Province)
    {
        Province->CurrentControllerCountryName = State.PlayerCountry.CountryName;
        Province->bPlayerControlled = true;
        Province->Stability = FMath::Clamp(Province->Stability + 2, 0, 100);
        Province->Unrest = FMath::Clamp(Province->Unrest - 2, 0, 100);
        Province->LastChangedTurn = State.Turn;
        Summary = FString::Printf(TEXT("Holding occupation in %s. Controller remains %s; stability +2, unrest -2."), *Province->ProvinceName, *State.PlayerCountry.CountryName);
        Severity = 42;
    }
    else if (FollowUpAction.Equals(TEXT("Abandon Occupation"), ESearchCase::IgnoreCase) && Province)
    {
        Province->CurrentControllerCountryName = Province->CurrentOwnerCountryName;
        Province->bPlayerControlled = Province->CurrentOwnerCountryName.Equals(State.PlayerCountry.CountryName, ESearchCase::IgnoreCase);
        Province->Unrest = FMath::Clamp(Province->Unrest + 3, 0, 100);
        Province->LastChangedTurn = State.Turn;
        Summary = FString::Printf(TEXT("Abandoned occupation in %s. Controller restored to %s; local unrest +3 from withdrawal shock."), *Province->ProvinceName, *Province->CurrentOwnerCountryName);
        Severity = 50;
    }
    else
    {
        Summary = FString::Printf(TEXT("No valid battle follow-up target for action: %s."), *FollowUpAction);
        Severity = 30;
    }

    RefreshRuntimeMapOwnership(State.RtsWorld.Ownership);
    RecordRtsOfficeAlert(State, TEXT("RTS Battle Follow-Up"), Summary, Battle.ProvinceId, Severity);
    LogDecision(State, TEXT("RTS Battle Follow-Up"), FollowUpAction, FString::Printf(TEXT("Latest battle %s at %s."), *Battle.Result, *Battle.ProvinceId), Summary, Severity, { TEXT("rts"), TEXT("battle"), FollowUpAction });
    RefreshRtsHudState(State);
    RefreshLoginWidget();
    return FReply::Handled();
}

TSharedRef<SWidget> ALoginHUD::BuildRtsOccupationActionsWidget()
{
    const bool bEnabled = bHasLoadedRuntimeState && !RtsSelectedProvinceId.IsEmpty();
    FString Status = TEXT("Select an occupied or contested province to manage pacification, fortification, transfer, or withdrawal.");
    if (bEnabled)
    {
        if (const FDemocracyProvinceOwnershipState* Province = FindRtsProvinceById(LoadedSaveState.RuntimeState, RtsSelectedProvinceId))
        {
            Status = FString::Printf(TEXT("%s | owner %s | controller %s | stability %d | unrest %d"), *Province->ProvinceName, *Province->CurrentOwnerCountryName, *Province->CurrentControllerCountryName, Province->Stability, Province->Unrest);
        }
    }

    return SNew(SBorder).BorderImage(RowBrush.Get()).BorderBackgroundColor(FLinearColor(0.07f, 0.10f, 0.12f, 0.84f)).Padding(8.0f)
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
        [SNew(STextBlock).Text(BodyText(Status)).AutoWrapText(true).Font(FCoreStyle::GetDefaultFontStyle("Regular", 10)).ColorAndOpacity(FLinearColor(0.90f, 0.96f, 1.0f, 1.0f))]
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 5.0f, 5.0f)[BuildButton(TEXT("Pacify"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleRtsOccupationActionClicked, FString(TEXT("Pacify"))), 88.0f, 28.0f, bEnabled)]
            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 5.0f, 5.0f)[BuildButton(TEXT("Fortify"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleRtsOccupationActionClicked, FString(TEXT("Fortify"))), 88.0f, 28.0f, bEnabled)]
            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 5.0f, 5.0f)[BuildButton(TEXT("Interim Gov"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleRtsOccupationActionClicked, FString(TEXT("Install Interim Government"))), 116.0f, 28.0f, bEnabled)]
            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 5.0f, 5.0f)[BuildButton(TEXT("Transfer"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleRtsOccupationActionClicked, FString(TEXT("Negotiate Transfer"))), 92.0f, 28.0f, bEnabled)]
            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 5.0f, 5.0f)[BuildButton(TEXT("Withdraw"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleRtsOccupationActionClicked, FString(TEXT("Withdraw"))), 94.0f, 28.0f, bEnabled)]
        ]
    ];
}

FReply ALoginHUD::HandleRtsOccupationActionClicked(FString ActionName)
{
    if (!bHasLoadedRuntimeState || RtsSelectedProvinceId.IsEmpty()) { return FReply::Handled(); }
    FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    FDemocracyProvinceOwnershipState* Province = FindMutableRtsProvinceById(State, RtsSelectedProvinceId);
    if (!Province) { return FReply::Handled(); }

    FString Summary;
    int32 Severity = 35;
    if (ActionName.Equals(TEXT("Pacify"), ESearchCase::IgnoreCase))
    {
        if (CanPayRtsCost(State.PlayerCountry, 30, 12, 0, 0, 0))
        {
            ApplyRtsCost(State.PlayerCountry, 30, 12, 0, 0, 0);
            Province->Unrest = FMath::Clamp(Province->Unrest - 10, 0, 100);
            Province->Stability = FMath::Clamp(Province->Stability + 4, 0, 100);
            Summary = FString::Printf(TEXT("Pacification program started in %s. Unrest -10, stability +4. Cost T30 F12."), *Province->ProvinceName);
        }
        else { Summary = TEXT("Pacify blocked: treasury or food reserves are too low."); Severity = 50; }
    }
    else if (ActionName.Equals(TEXT("Fortify"), ESearchCase::IgnoreCase))
    {
        if (CanPayRtsCost(State.PlayerCountry, 35, 0, 5, 16, 16))
        {
            ApplyRtsCost(State.PlayerCountry, 35, 0, 5, 16, 16);
            Province->StrategicValue = FMath::Clamp(Province->StrategicValue + 3, 1, 100);
            Province->Stability = FMath::Clamp(Province->Stability + 2, 0, 100);
            Summary = FString::Printf(TEXT("Fortified occupation positions in %s. Strategic value +3, stability +2. Cost T35 G5 W16 M16."), *Province->ProvinceName);
        }
        else { Summary = TEXT("Fortify blocked: fuel, wood, metals, or treasury are too low."); Severity = 50; }
    }
    else if (ActionName.Equals(TEXT("Install Interim Government"), ESearchCase::IgnoreCase))
    {
        Province->GovernmentType = TEXT("Democracy Transition");
        Province->Stability = FMath::Clamp(Province->Stability + 6, 0, 100);
        Province->Unrest = FMath::Clamp(Province->Unrest - 5, 0, 100);
        State.DiplomacyMatrix.Summary = FString::Printf(TEXT("Interim democratic administration installed in %s; rivals may treat this as forced political transition."), *Province->ProvinceName);
        Summary = FString::Printf(TEXT("Installed interim government in %s. Government is now Democracy Transition, stability +6, unrest -5."), *Province->ProvinceName);
        Severity = 45;
    }
    else if (ActionName.Equals(TEXT("Negotiate Transfer"), ESearchCase::IgnoreCase))
    {
        const bool bControllerIsPlayer = Province->CurrentControllerCountryName.Equals(State.PlayerCountry.CountryName, ESearchCase::IgnoreCase);
        const int32 HeldTurns = FMath::Max(0, State.Turn - Province->LastChangedTurn);
        if (bControllerIsPlayer && HeldTurns >= 3)
        {
            Province->CurrentOwnerCountryName = State.PlayerCountry.CountryName;
            Province->bPlayerControlled = true;
            Province->GovernmentType = State.GovernmentDiplomacyRules.PlayerGovernmentType;
            Province->LastChangedTurn = State.Turn;
            Summary = FString::Printf(TEXT("Negotiated transfer finalized for %s after %d held turn(s). Owner/controller now %s."), *Province->ProvinceName, HeldTurns, *State.PlayerCountry.CountryName);
            Severity = 55;
        }
        else
        {
            Summary = FString::Printf(TEXT("Transfer negotiation opened for %s. Hold the province for at least 3 turns or secure peace/surrender terms before ownership can change."), *Province->ProvinceName);
            Severity = 40;
        }
    }
    else if (ActionName.Equals(TEXT("Withdraw"), ESearchCase::IgnoreCase))
    {
        Province->CurrentControllerCountryName = Province->CurrentOwnerCountryName;
        Province->bPlayerControlled = Province->CurrentOwnerCountryName.Equals(State.PlayerCountry.CountryName, ESearchCase::IgnoreCase);
        Province->Unrest = FMath::Clamp(Province->Unrest + 4, 0, 100);
        Province->LastChangedTurn = State.Turn;
        Summary = FString::Printf(TEXT("Withdrew from %s. Controller restored to %s; unrest +4 from rapid withdrawal."), *Province->ProvinceName, *Province->CurrentOwnerCountryName);
        Severity = 48;
    }
    else { Summary = FString::Printf(TEXT("Unknown occupation action: %s."), *ActionName); }

    RefreshRuntimeMapOwnership(State.RtsWorld.Ownership);
    RecordRtsOfficeAlert(State, TEXT("Occupation Action"), Summary, Province->ProvinceId, Severity);
    LogDecision(State, TEXT("RTS Occupation"), ActionName, FString::Printf(TEXT("Managed province %s."), *Province->ProvinceName), Summary, Severity, { TEXT("rts"), TEXT("occupation"), ActionName });
    RefreshRtsHudState(State);
    RefreshLoginWidget();
    return FReply::Handled();
}

TSharedRef<SWidget> ALoginHUD::BuildRtsSupplyActionsWidget()
{
    TSharedRef<SVerticalBox> RouteList = SNew(SVerticalBox);
    if (!bHasLoadedRuntimeState || LoadedSaveState.RuntimeState.RtsWorld.SupplyRoutes.Num() == 0)
    {
        RouteList->AddSlot().AutoHeight()[SNew(STextBlock).Text(BodyText(TEXT("No active supply routes. Issue a movement order to create a route."))).AutoWrapText(true).Font(FCoreStyle::GetDefaultFontStyle("Regular", 10)).ColorAndOpacity(FLinearColor(0.82f, 0.92f, 0.96f, 1.0f))];
    }
    else
    {
        const int32 RouteCount = FMath::Min(LoadedSaveState.RuntimeState.RtsWorld.SupplyRoutes.Num(), 3);
        for (int32 Index = 0; Index < RouteCount; ++Index)
        {
            const FDemocracyRtsSupplyRouteState& Route = LoadedSaveState.RuntimeState.RtsWorld.SupplyRoutes[Index];
            RouteList->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 7.0f)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
                [SNew(STextBlock).Text(BodyText(FString::Printf(TEXT("%s | %s -> %s | supply %d | disruption %d%s"), *Route.ArmyId, *Route.SourceProvinceId, *Route.DestinationProvinceId, Route.SupplyStatus, Route.Disruption, Route.bBroken ? TEXT(" | BROKEN") : TEXT("")))).AutoWrapText(true).Font(FCoreStyle::GetDefaultFontStyle("Regular", 10)).ColorAndOpacity(FLinearColor(0.88f, 0.96f, 1.0f, 1.0f))]
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 4.0f)[BuildButton(TEXT("Repair"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleRtsSupplyActionClicked, Route.RouteId, FString(TEXT("Repair Route"))), 74.0f, 26.0f)]
                    + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 4.0f)[BuildButton(TEXT("Reroute"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleRtsSupplyActionClicked, Route.RouteId, FString(TEXT("Reroute Supply"))), 78.0f, 26.0f)]
                    + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 4.0f)[BuildButton(TEXT("Escort"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleRtsSupplyActionClicked, Route.RouteId, FString(TEXT("Escort Convoy"))), 76.0f, 26.0f)]
                    + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 4.0f)[BuildButton(TEXT("Fuel"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleRtsSupplyActionClicked, Route.RouteId, FString(TEXT("Prioritize Fuel"))), 62.0f, 26.0f)]
                    + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 4.0f)[BuildButton(TEXT("Ammo"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleRtsSupplyActionClicked, Route.RouteId, FString(TEXT("Prioritize Ammo"))), 66.0f, 26.0f)]
                    + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 4.0f)[BuildButton(TEXT("Food"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleRtsSupplyActionClicked, Route.RouteId, FString(TEXT("Prioritize Food"))), 64.0f, 26.0f)]
                ]
            ];
        }
    }
    return SNew(SBorder).BorderImage(RowBrush.Get()).BorderBackgroundColor(FLinearColor(0.06f, 0.09f, 0.11f, 0.84f)).Padding(8.0f)[RouteList];
}

FReply ALoginHUD::HandleRtsSupplyActionClicked(FString RouteId, FString ActionName)
{
    if (!bHasLoadedRuntimeState || RouteId.IsEmpty()) { return FReply::Handled(); }
    FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    FDemocracyRtsSupplyRouteState* Route = nullptr;
    FDemocracyRtsArmyGroupState* Army = nullptr;
    for (FDemocracyRtsSupplyRouteState& CandidateRoute : State.RtsWorld.SupplyRoutes)
    {
        if (CandidateRoute.RouteId.Equals(RouteId, ESearchCase::IgnoreCase)) { Route = &CandidateRoute; break; }
    }
    if (Route)
    {
        for (FDemocracyRtsArmyGroupState& CandidateArmy : State.RtsWorld.ArmyGroups)
        {
            if (CandidateArmy.ArmyId.Equals(Route->ArmyId, ESearchCase::IgnoreCase)) { Army = &CandidateArmy; break; }
        }
    }
    if (!Route) { return FReply::Handled(); }

    FString Summary;
    int32 Severity = 28;
    if (ActionName.Equals(TEXT("Repair Route"), ESearchCase::IgnoreCase))
    {
        if (CanPayRtsCost(State.PlayerCountry, 22, 0, 8, 8, 6))
        {
            ApplyRtsCost(State.PlayerCountry, 22, 0, 8, 8, 6);
            Route->Disruption = FMath::Clamp(Route->Disruption - 24, 0, 100);
            Route->SupplyStatus = FMath::Clamp(Route->SupplyStatus + 24, 0, 100);
            Route->bBroken = Route->SupplyStatus < 35;
            Summary = FString::Printf(TEXT("Repaired supply route %s. Supply +24, disruption -24. Cost T22 G8 W8 M6."), *Route->RouteId);
        }
        else { Summary = TEXT("Repair route blocked: fuel, wood, metals, or treasury are too low."); Severity = 45; }
    }
    else if (ActionName.Equals(TEXT("Reroute Supply"), ESearchCase::IgnoreCase))
    {
        Route->DistancePenalty = FMath::Clamp(Route->DistancePenalty + 4, 0, 100);
        Route->Disruption = FMath::Clamp(Route->Disruption - 12, 0, 100);
        Route->SupplyStatus = FMath::Clamp(Route->SupplyStatus + 12, 0, 100);
        Route->bBroken = Route->SupplyStatus < 35;
        Route->Risks.AddUnique(TEXT("longer rerouted convoy path"));
        Summary = FString::Printf(TEXT("Rerouted %s. Supply +12, disruption -12, distance penalty +4."), *Route->RouteId);
    }
    else if (ActionName.Equals(TEXT("Escort Convoy"), ESearchCase::IgnoreCase))
    {
        if (CanPayRtsCost(State.PlayerCountry, 28, 0, 10, 0, 8))
        {
            ApplyRtsCost(State.PlayerCountry, 28, 0, 10, 0, 8);
            Route->SupplyStatus = FMath::Clamp(Route->SupplyStatus + 14, 0, 100);
            Route->Disruption = FMath::Clamp(Route->Disruption - 10, 0, 100);
            Route->bBroken = Route->SupplyStatus < 35;
            if (Army)
            {
                Army->LogisticsCount += 1;
                Army->DefensiveUnitCount += 1;
                Army->TotalStrength = FMath::Clamp(Army->InfantryCount * 8 + Army->VehicleCount * 22 + Army->AircraftCount * 28 + Army->LogisticsCount * 6 + Army->ScoutCount * 5 + Army->DefensiveUnitCount * 18, 0, 999);
            }
            Summary = FString::Printf(TEXT("Escort convoy assigned to %s. Supply +14, disruption -10, logistics/defense support added."), *Route->RouteId);
        }
        else { Summary = TEXT("Escort convoy blocked: fuel, metals, or treasury are too low."); Severity = 45; }
    }
    else
    {
        const bool bFuel = ActionName.Contains(TEXT("Fuel"));
        const bool bAmmo = ActionName.Contains(TEXT("Ammo"));
        const int32 FoodCost = ActionName.Contains(TEXT("Food")) ? 14 : 0;
        const int32 FuelCost = bFuel ? 14 : 4;
        const int32 MetalsCost = bAmmo ? 12 : 2;
        if (CanPayRtsCost(State.PlayerCountry, 16, FoodCost, FuelCost, 0, MetalsCost))
        {
            ApplyRtsCost(State.PlayerCountry, 16, FoodCost, FuelCost, 0, MetalsCost);
            Route->SupplyStatus = FMath::Clamp(Route->SupplyStatus + 10, 0, 100);
            Route->Disruption = FMath::Clamp(Route->Disruption - 6, 0, 100);
            Route->bBroken = Route->SupplyStatus < 35;
            Route->Risks.AddUnique(ActionName);
            if (Army)
            {
                if (bFuel) { Army->MovementTurnsRemaining = FMath::Max(0, Army->MovementTurnsRemaining - 1); }
                if (bAmmo) { Army->Morale = FMath::Clamp(Army->Morale + 2, 0, 100); }
                if (ActionName.Contains(TEXT("Food"))) { Army->SupplyStatus = FMath::Clamp(Army->SupplyStatus + 8, 0, 100); }
            }
            Summary = FString::Printf(TEXT("%s applied to %s. Route supply +10, disruption -6."), *ActionName, *Route->RouteId);
        }
        else { Summary = FString::Printf(TEXT("%s blocked: required supplies are unavailable."), *ActionName); Severity = 45; }
    }
    Route->StatusSummary = Summary;
    if (Army)
    {
        Army->SupplyStatus = FMath::Clamp(FMath::Max(Army->SupplyStatus, Route->SupplyStatus), 0, 100);
        Army->bSupplyRouteBroken = Route->bBroken;
    }

    RecordRtsOfficeAlert(State, TEXT("Supply Route Action"), Summary, Route->DestinationProvinceId, Severity);
    LogDecision(State, TEXT("RTS Supply"), ActionName, FString::Printf(TEXT("Managed route %s."), *Route->RouteId), Summary, Severity, { TEXT("rts"), TEXT("supply"), ActionName });
    RefreshRtsHudState(State);
    RefreshLoginWidget();
    return FReply::Handled();
}

TSharedRef<SWidget> ALoginHUD::BuildRtsDiplomacyActionsWidget()
{
    const bool bEnabled = bHasLoadedRuntimeState && !RtsSelectedCountryName.IsEmpty();
    FString Status = bEnabled
        ? FString::Printf(TEXT("Selected country: %s. RTS diplomacy actions use office command authority rules and immediately change RTS availability."), *RtsSelectedCountryName)
        : TEXT("Select a country or province to use RTS diplomacy actions: ceasefire, sanctions, alliance aid, alliances, or treaties.");

    return SNew(SBorder).BorderImage(RowBrush.Get()).BorderBackgroundColor(FLinearColor(0.07f, 0.10f, 0.12f, 0.84f)).Padding(8.0f)
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
        [SNew(STextBlock).Text(BodyText(Status)).AutoWrapText(true).Font(FCoreStyle::GetDefaultFontStyle("Regular", 10)).ColorAndOpacity(FLinearColor(0.90f, 0.96f, 1.0f, 1.0f))]
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 5.0f, 5.0f)[BuildButton(TEXT("Ceasefire"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleRtsMapDiplomacyActionClicked, FString(TEXT("office_negotiate_ceasefire"))), 96.0f, 28.0f, bEnabled)]
            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 5.0f, 5.0f)[BuildButton(TEXT("Sanction"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleRtsMapDiplomacyActionClicked, FString(TEXT("office_impose_sanctions"))), 92.0f, 28.0f, bEnabled)]
            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 5.0f, 5.0f)[BuildButton(TEXT("Aid"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleRtsMapDiplomacyActionClicked, FString(TEXT("office_request_alliance_aid"))), 68.0f, 28.0f, bEnabled)]
            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 5.0f, 5.0f)[BuildButton(TEXT("Alliance"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleRtsMapDiplomacyActionClicked, FString(TEXT("office_propose_alliance"))), 92.0f, 28.0f, bEnabled)]
            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 5.0f, 5.0f)[BuildButton(TEXT("Treaty"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleRtsMapDiplomacyActionClicked, FString(TEXT("office_propose_treaty"))), 82.0f, 28.0f, bEnabled)]
        ]
    ];
}

TSharedRef<SWidget> ALoginHUD::BuildRtsNotificationsWidget()
{
    TSharedRef<SVerticalBox> NotificationList = SNew(SVerticalBox);
    if (!bHasLoadedRuntimeState)
    {
        NotificationList->AddSlot().AutoHeight()[SNew(STextBlock).Text(BodyText(TEXT("No RTS state loaded."))).AutoWrapText(true).Font(FCoreStyle::GetDefaultFontStyle("Regular", 10)).ColorAndOpacity(FLinearColor(0.82f, 0.92f, 0.96f, 1.0f))];
    }
    else
    {
        TArray<FDemocracyRtsOutcomeState> DisplayAlerts;
        for (const FDemocracyRtsOutcomeState& Outcome : LoadedSaveState.RuntimeState.RtsWorld.Backflow.PendingOutcomes)
        {
            DisplayAlerts.Add(Outcome);
        }
        for (int32 Index = LoadedSaveState.RuntimeState.RtsWorld.Backflow.OutcomeHistory.Num() - 1; Index >= 0 && DisplayAlerts.Num() < 5; --Index)
        {
            DisplayAlerts.Add(LoadedSaveState.RuntimeState.RtsWorld.Backflow.OutcomeHistory[Index]);
        }

        if (DisplayAlerts.Num() == 0)
        {
            NotificationList->AddSlot().AutoHeight()[SNew(STextBlock).Text(BodyText(TEXT("No actionable RTS notifications yet."))).AutoWrapText(true).Font(FCoreStyle::GetDefaultFontStyle("Regular", 10)).ColorAndOpacity(FLinearColor(0.82f, 0.92f, 0.96f, 1.0f))];
        }
        else
        {
            const int32 AlertCount = FMath::Min(DisplayAlerts.Num(), 5);
            for (int32 Index = 0; Index < AlertCount; ++Index)
            {
                const FDemocracyRtsOutcomeState& Alert = DisplayAlerts[Index];
                const FString ProvinceId = Alert.AffectedProvinceId;
                const FString Label = FString::Printf(TEXT("%s | %s | %s"), *Alert.OutcomeType, ProvinceId.IsEmpty() ? TEXT("no province") : *ProvinceId, *Alert.AttentionSummary.Left(90));
                NotificationList->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
                [
                    BuildButton(Label, FOnClicked::CreateUObject(this, &ALoginHUD::HandleFocusRtsAlertClicked, ProvinceId, FString(), FString()), 520.0f, 30.0f, !ProvinceId.IsEmpty())
                ];
            }
        }
    }

    return SNew(SBorder).BorderImage(RowBrush.Get()).BorderBackgroundColor(FLinearColor(0.06f, 0.09f, 0.11f, 0.84f)).Padding(8.0f)[NotificationList];
}

FReply ALoginHUD::HandleRtsMapDiplomacyActionClicked(FString CommandId)
{
    if (!bHasLoadedRuntimeState)
    {
        return FReply::Handled();
    }

    FString CostSummary;
    if (!TryApplyRtsActionCost(LoadedSaveState.RuntimeState, TEXT("Diplomacy"), CostSummary))
    {
        LoadedSaveState.RuntimeState.RtsWorld.WorldInteraction.LastInteractionSummary = FString::Printf(TEXT("RTS diplomacy action blocked: %s unavailable."), *CostSummary);
        AddRtsHudAlert(LoadedSaveState.RuntimeState.RtsWorld, LoadedSaveState.RuntimeState.RtsWorld.WorldInteraction.LastInteractionSummary);
        RefreshRtsHudState(LoadedSaveState.RuntimeState);
        RefreshLoginWidget();
        return FReply::Handled();
    }

    FReply Result = HandleExecuteAuthorityCommand(CommandId, TEXT("RTS Map"));
    LoadedSaveState.RuntimeState.RtsWorld.WorldInteraction.LastInteractionSummary += FString::Printf(TEXT(" RTS map diplomacy cost paid: %s."), *CostSummary);
    RecordRtsOfficeAlert(LoadedSaveState.RuntimeState, TEXT("RTS Diplomacy"), LoadedSaveState.RuntimeState.RtsWorld.WorldInteraction.LastInteractionSummary, RtsSelectedProvinceId, 36);
    RefreshRtsHudState(LoadedSaveState.RuntimeState);
    RefreshLoginWidget();
    return Result;
}

FReply ALoginHUD::HandleFocusRtsAlertClicked(FString ProvinceId, FString ArmyId, FString CityBaseId)
{
    if (!bHasLoadedRuntimeState)
    {
        return FReply::Handled();
    }

    FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    if (!ProvinceId.IsEmpty())
    {
        RtsSelectedProvinceId = ProvinceId;
        if (const FDemocracyProvinceOwnershipState* Province = FindRtsProvinceById(State, ProvinceId))
        {
            RtsSelectedCountryName = Province->CurrentOwnerCountryName.IsEmpty() ? Province->CurrentControllerCountryName : Province->CurrentOwnerCountryName;
            State.RtsWorld.WorldInteraction.ActiveSelectionId = ProvinceId;
            State.RtsWorld.WorldInteraction.ActiveSelectionType = TEXT("Province Alert");
            State.RtsWorld.WorldInteraction.LastInteractionSummary = FString::Printf(TEXT("Focused RTS notification on %s: %s."), *Province->ProvinceName, *ProvinceId);
        }
        FocusRtsMapOnSelection();
    }
    if (!ArmyId.IsEmpty())
    {
        RtsSelectedArmyId = ArmyId;
        for (FDemocracyRtsArmyGroupState& Army : State.RtsWorld.ArmyGroups)
        {
            Army.bSelected = Army.ArmyId.Equals(ArmyId, ESearchCase::IgnoreCase);
        }
    }
    if (!CityBaseId.IsEmpty())
    {
        State.RtsWorld.ActiveViewMode = TEXT("City/Base View");
    }
    RefreshRtsHudState(State);
    RefreshLoginWidget();
    return FReply::Handled();
}
FReply ALoginHUD::HandleRecruitRtsUnitsClicked(FString UnitType)
{
    if (!bHasLoadedRuntimeState) { return FReply::Handled(); }
    FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    FDemocracyRtsArmyGroupState* Army = nullptr;
    for (FDemocracyRtsArmyGroupState& CandidateArmy : State.RtsWorld.ArmyGroups)
    {
        if ((!RtsSelectedArmyId.IsEmpty() && CandidateArmy.ArmyId.Equals(RtsSelectedArmyId, ESearchCase::IgnoreCase)) || CandidateArmy.bSelected)
        {
            Army = &CandidateArmy;
            break;
        }
    }
    if (!Army && State.RtsWorld.ArmyGroups.Num() > 0)
    {
        Army = &State.RtsWorld.ArmyGroups[0];
        RtsSelectedArmyId = Army->ArmyId;
        Army->bSelected = true;
    }
    if (!Army) { return FReply::Handled(); }

    int32 TreasuryCost = 0, FoodCost = 0, FuelCost = 0, WoodCost = 0, MetalsCost = 0;
    FString RequiredFocus;
    if (UnitType.Equals(TEXT("Soldiers"), ESearchCase::IgnoreCase) || UnitType.Equals(TEXT("Soldier"), ESearchCase::IgnoreCase))
    {
        UnitType = TEXT("Soldiers");
        TreasuryCost = 32;
        FoodCost = 14;
        MetalsCost = 5;
        RequiredFocus = TEXT("Food");
    }
    else if (UnitType.Equals(TEXT("Tanks"), ESearchCase::IgnoreCase) || UnitType.Equals(TEXT("Tank"), ESearchCase::IgnoreCase))
    {
        UnitType = TEXT("Tanks");
        TreasuryCost = 90;
        FuelCost = 24;
        MetalsCost = 32;
        RequiredFocus = TEXT("Metals");
    }
    else if (UnitType.Equals(TEXT("Workers"), ESearchCase::IgnoreCase) || UnitType.Equals(TEXT("Worker"), ESearchCase::IgnoreCase))
    {
        UnitType = TEXT("Workers");
        TreasuryCost = 22;
        FoodCost = 8;
        WoodCost = 4;
        RequiredFocus = TEXT("Reserve");
    }
    else
    {
        UnitType = TEXT("Supply Carriers");
        TreasuryCost = 48;
        FuelCost = 18;
        MetalsCost = 10;
        RequiredFocus = TEXT("Fuel");
    }

    bool bHasProductionBuilding = false;
    for (const FDemocracyRtsBuildingState& Building : State.RtsWorld.CityBase.Buildings)
    {
        if (Building.bConstructed && !Building.bDisabled && (Building.ResourceFocus.Equals(RequiredFocus, ESearchCase::IgnoreCase) || Building.ResourceFocus.Equals(TEXT("Security"), ESearchCase::IgnoreCase) || Building.ResourceFocus.Equals(TEXT("Logistics"), ESearchCase::IgnoreCase) || Building.ResourceFocus.Equals(TEXT("Reserve"), ESearchCase::IgnoreCase)))
        {
            bHasProductionBuilding = true;
            break;
        }
    }

    FString Summary;
    int32 Severity = 24;
    if (!bHasProductionBuilding)
    {
        Summary = FString::Printf(TEXT("Spawn %s blocked: city/base needs an operational %s, Security, Logistics, or Reserve building."), *UnitType, *RequiredFocus);
        Severity = 45;
    }
    else if (!CanPayRtsCost(State.PlayerCountry, TreasuryCost, FoodCost, FuelCost, WoodCost, MetalsCost))
    {
        Summary = FString::Printf(TEXT("Spawn %s blocked: %s unavailable."), *UnitType, *BuildRtsCostText(TreasuryCost, FoodCost, FuelCost, WoodCost, MetalsCost));
        Severity = 45;
    }
    else
    {
        ApplyRtsCost(State.PlayerCountry, TreasuryCost, FoodCost, FuelCost, WoodCost, MetalsCost);
        if (UnitType.Equals(TEXT("Soldiers"), ESearchCase::IgnoreCase))
        {
            Army->InfantryCount += 10;
            Summary = FString::Printf(TEXT("Spawned soldier company into %s. Infantry +10. %s."), *Army->DisplayName, *BuildRtsCostText(TreasuryCost, FoodCost, FuelCost, WoodCost, MetalsCost));
        }
        else if (UnitType.Equals(TEXT("Tanks"), ESearchCase::IgnoreCase))
        {
            Army->VehicleCount += 3;
            Summary = FString::Printf(TEXT("Spawned tank platoon into %s. Vehicles +3. %s."), *Army->DisplayName, *BuildRtsCostText(TreasuryCost, FoodCost, FuelCost, WoodCost, MetalsCost));
        }
        else if (UnitType.Equals(TEXT("Workers"), ESearchCase::IgnoreCase))
        {
            Army->LogisticsCount += 2;
            Army->SupplyStatus = FMath::Clamp(Army->SupplyStatus + 6, 0, 100);
            State.RtsWorld.ResourceCollection.WoodFromBuildings += 1;
            State.RtsWorld.ResourceCollection.MetalsFromBuildings += 1;
            Summary = FString::Printf(TEXT("Spawned worker crew into %s. Workers repair structures and haul node materials to storage. Logistics +2, supply +6. %s."), *Army->DisplayName, *BuildRtsCostText(TreasuryCost, FoodCost, FuelCost, WoodCost, MetalsCost));
        }
        else
        {
            Army->LogisticsCount += 4;
            Army->SupplyStatus = FMath::Clamp(Army->SupplyStatus + 12, 0, 100);
            Summary = FString::Printf(TEXT("Spawned convoy group into %s. Logistics +4, supply +12. %s."), *Army->DisplayName, *BuildRtsCostText(TreasuryCost, FoodCost, FuelCost, WoodCost, MetalsCost));
        }
        Army->TotalStrength = FMath::Clamp(Army->InfantryCount * 8 + Army->VehicleCount * 22 + Army->AircraftCount * 28 + Army->LogisticsCount * 6 + Army->ScoutCount * 5 + Army->DefensiveUnitCount * 18, 0, 999);
        State.RtsWorld.CityBase.RuntimeNotes.Add(Summary);
    }

    State.RtsWorld.WorldInteraction.LastInteractionSummary = Summary;
    RecordRtsOfficeAlert(State, TEXT("Unit Recruitment"), Summary, Army->CurrentProvinceId, Severity);
    LogDecision(State, TEXT("RTS Recruitment"), UnitType, FString::Printf(TEXT("Assigned units to %s."), *Army->DisplayName), Summary, Severity, { TEXT("rts"), TEXT("recruitment"), UnitType });
    RefreshRtsHudState(State);
    RefreshLoginWidget();
    return FReply::Handled();
}
TSharedRef<SWidget> ALoginHUD::BuildRtsResourceNodesWidget() const
{
    TSharedRef<SUniformGridPanel> NodeGrid = SNew(SUniformGridPanel).SlotPadding(FMargin(6.0f));
    if (!bHasLoadedRuntimeState)
    {
        return NodeGrid;
    }

    const FDemocracyRtsWorldState& RtsWorld = LoadedSaveState.RuntimeState.RtsWorld;
    const TArray<FString> NodeNames = { TEXT("Food"), TEXT("Fuel"), TEXT("Wood"), TEXT("Metals") };
    for (int32 Index = 0; Index < NodeNames.Num(); ++Index)
    {
        const FString& ResourceName = NodeNames[Index];
        int32 BuildingOutput = 0;
        int32 ProvinceOutput = 0;
        if (ResourceName.Equals(TEXT("Food"), ESearchCase::IgnoreCase)) { BuildingOutput = RtsWorld.ResourceCollection.FoodFromBuildings; ProvinceOutput = RtsWorld.ResourceCollection.FoodFromProvinces; }
        else if (ResourceName.Equals(TEXT("Fuel"), ESearchCase::IgnoreCase)) { BuildingOutput = RtsWorld.ResourceCollection.FuelFromBuildings; ProvinceOutput = RtsWorld.ResourceCollection.FuelFromProvinces; }
        else if (ResourceName.Equals(TEXT("Wood"), ESearchCase::IgnoreCase)) { BuildingOutput = RtsWorld.ResourceCollection.WoodFromBuildings; ProvinceOutput = RtsWorld.ResourceCollection.WoodFromProvinces; }
        else { BuildingOutput = RtsWorld.ResourceCollection.MetalsFromBuildings; ProvinceOutput = RtsWorld.ResourceCollection.MetalsFromProvinces; }

        int32 NodeCount = 0;
        for (const FDemocracyProvinceOwnershipState& Province : RtsWorld.Ownership.Provinces)
        {
            if (Province.bPlayerControlled && Province.ResourceFocus.Equals(ResourceName, ESearchCase::IgnoreCase))
            {
                ++NodeCount;
            }
        }

        const FString NodeText = FString::Printf(TEXT("%s Node\nOwned zones %d\nBuildings %+d | provinces %+d\nWorker haul: node -> storage\nSupply link: %s"),
            *ResourceName,
            NodeCount,
            BuildingOutput,
            ProvinceOutput,
            RtsWorld.SupplyRoutes.Num() > 0 ? TEXT("active") : TEXT("base-only"));
        NodeGrid->AddSlot(Index, 0)
        [
            SNew(SBorder)
            .BorderImage(RowBrush.Get())
            .BorderBackgroundColor(FLinearColor(0.07f, 0.13f, 0.15f, 0.88f))
            .Padding(8.0f)
            [
                SNew(SBox)
                .WidthOverride(170.0f)
                .HeightOverride(92.0f)
                [
                    SNew(STextBlock)
                    .Text(BodyText(NodeText))
                    .AutoWrapText(true)
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                    .ColorAndOpacity(FLinearColor(0.86f, 0.96f, 1.0f, 1.0f))
                ]
            ]
        ];
    }
    return NodeGrid;
}

TSharedRef<SWidget> ALoginHUD::BuildRtsRegionalCountryViewWidget()
{
    if (!bHasLoadedRuntimeState)
    {
        return SNew(STextBlock).Text(BodyText(TEXT("No RTS state loaded.")));
    }

    FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    FDemocracyRtsWorldState& RtsWorld = State.RtsWorld;
    const FString CountryName = RtsSelectedCountryName.IsEmpty() ? State.PlayerCountry.CountryName : RtsSelectedCountryName;
    const FString ProvinceName = RtsSelectedProvinceId.IsEmpty() ? TEXT("capital region") : RtsSelectedProvinceId;
    const float RegionalWidth = 2600.0f;
    const float RegionalHeight = 1560.0f;

    auto MakePatch = [this](const FLinearColor& Color) -> TSharedRef<SWidget>
    {
        return SNew(SBorder).BorderImage(RowBrush.Get()).BorderBackgroundColor(Color).Padding(0.0f);
    };

    auto MakeLabel = [this](const FString& Label, const FLinearColor& Color, int32 FontSize = 9) -> TSharedRef<SWidget>
    {
        return SNew(SBorder)
            .BorderImage(RowBrush.Get())
            .BorderBackgroundColor(Color)
            .Padding(FMargin(6.0f, 3.0f))
            [
                SNew(STextBlock)
                .Text(BodyText(Label))
                .Justification(ETextJustify::Center)
                .AutoWrapText(true)
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", FontSize))
                .ColorAndOpacity(FLinearColor(0.94f, 0.98f, 0.92f, 1.0f))
            ];
    };

    auto MakeCity = [this](const FString& Name, const FString& Detail, const FLinearColor& Color) -> TSharedRef<SWidget>
    {
        return SNew(SBorder)
            .BorderImage(RowBrush.Get())
            .BorderBackgroundColor(Color)
            .Padding(FMargin(10.0f, 8.0f))
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                [
                    SNew(SBox).WidthOverride(78.0f).HeightOverride(34.0f)
                    [SNew(SBorder).BorderImage(RowBrush.Get()).BorderBackgroundColor(FLinearColor(0.12f, 0.16f, 0.17f, 0.98f)).Padding(0.0f)]
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 0.0f)
                [
                    SNew(STextBlock)
                    .Text(BodyText(Name))
                    .Justification(ETextJustify::Center)
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                    .ColorAndOpacity(FLinearColor::White)
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(STextBlock)
                    .Text(BodyText(Detail))
                    .Justification(ETextJustify::Center)
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                    .ColorAndOpacity(FLinearColor(0.78f, 0.88f, 0.82f, 1.0f))
                ]
            ];
    };

    auto MakeResource = [this](const FString& ResourceName, int32 Count, const FLinearColor& Color) -> TSharedRef<SWidget>
    {
        return SNew(SBorder)
            .BorderImage(RowBrush.Get())
            .BorderBackgroundColor(Color)
            .Padding(FMargin(8.0f, 5.0f))
            [
                SNew(STextBlock)
                .Text(BodyText(FString::Printf(TEXT("%s\nx%d"), *ResourceName, FMath::Max(1, Count))))
                .Justification(ETextJustify::Center)
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                .ColorAndOpacity(FLinearColor::White)
            ];
    };

    TSharedRef<SConstraintCanvas> RegionalCanvas = SNew(SConstraintCanvas);
    RegionalCanvas->AddSlot().Anchors(FAnchors(0.0f, 0.0f)).Offset(FMargin(0.0f, 0.0f, RegionalWidth, RegionalHeight))
    [MakePatch(FLinearColor(0.10f, 0.20f, 0.12f, 1.0f))];

    for (int32 Y = 0; Y < 13; ++Y)
    {
        for (int32 X = 0; X < 22; ++X)
        {
            const float Tone = ((X * 19 + Y * 11) % 12) * 0.006f;
            RegionalCanvas->AddSlot().Anchors(FAnchors(0.0f, 0.0f)).Offset(FMargin(X * 120.0f, Y * 120.0f, 124.0f, 124.0f))
            [MakePatch(FLinearColor(0.08f + Tone, 0.18f + Tone, 0.10f + Tone, 0.50f))];
        }
    }

    RegionalCanvas->AddSlot().Anchors(FAnchors(0.0f, 0.0f)).Offset(FMargin(0.0f, 0.0f, 2600.0f, 190.0f))[MakePatch(FLinearColor(0.02f, 0.07f, 0.035f, 0.96f))];
    RegionalCanvas->AddSlot().Anchors(FAnchors(0.0f, 0.0f)).Offset(FMargin(2040.0f, 270.0f, 420.0f, 320.0f))[MakePatch(FLinearColor(0.03f, 0.24f, 0.34f, 0.96f))];
    RegionalCanvas->AddSlot().Anchors(FAnchors(0.0f, 0.0f)).Offset(FMargin(210.0f, 1150.0f, 2000.0f, 78.0f))[MakePatch(FLinearColor(0.03f, 0.24f, 0.32f, 0.92f))];
    RegionalCanvas->AddSlot().Anchors(FAnchors(0.0f, 0.0f)).Offset(FMargin(1460.0f, 190.0f, 56.0f, 1120.0f))[MakePatch(FLinearColor(0.18f, 0.19f, 0.17f, 0.88f))];
    RegionalCanvas->AddSlot().Anchors(FAnchors(0.0f, 0.0f)).Offset(FMargin(510.0f, 720.0f, 1540.0f, 48.0f))[MakePatch(FLinearColor(0.18f, 0.19f, 0.17f, 0.88f))];
    RegionalCanvas->AddSlot().Anchors(FAnchors(0.0f, 0.0f)).Offset(FMargin(1120.0f, 470.0f, 220.0f, 170.0f))[MakeLabel(TEXT("Capital command district"), FLinearColor(0.05f, 0.10f, 0.10f, 0.90f), 10)];

    RegionalCanvas->AddSlot().Anchors(FAnchors(0.0f, 0.0f)).Offset(FMargin(1060.0f, 650.0f, 180.0f, 116.0f))[MakeCity(TEXT("Capital"), TEXT("command / storage"), FLinearColor(0.05f, 0.24f, 0.17f, 0.98f))];
    RegionalCanvas->AddSlot().Anchors(FAnchors(0.0f, 0.0f)).Offset(FMargin(520.0f, 420.0f, 170.0f, 108.0f))[MakeCity(TEXT("North City"), TEXT("industry / repair"), FLinearColor(0.10f, 0.20f, 0.24f, 0.96f))];
    RegionalCanvas->AddSlot().Anchors(FAnchors(0.0f, 0.0f)).Offset(FMargin(1700.0f, 700.0f, 170.0f, 108.0f))[MakeCity(TEXT("East Depot"), TEXT("logistics / supply"), FLinearColor(0.12f, 0.22f, 0.18f, 0.96f))];
    RegionalCanvas->AddSlot().Anchors(FAnchors(0.0f, 0.0f)).Offset(FMargin(760.0f, 1030.0f, 170.0f, 108.0f))[MakeCity(TEXT("Farm Town"), TEXT("food / workers"), FLinearColor(0.16f, 0.28f, 0.10f, 0.96f))];
    RegionalCanvas->AddSlot().Anchors(FAnchors(0.0f, 0.0f)).Offset(FMargin(1880.0f, 1110.0f, 170.0f, 108.0f))[MakeCity(TEXT("South Port"), TEXT("trade / fuel"), FLinearColor(0.07f, 0.24f, 0.30f, 0.96f))];

    int32 FoodNodes = 0;
    int32 FuelNodes = 0;
    int32 WoodNodes = 0;
    int32 MetalNodes = 0;
    for (const FDemocracyProvinceOwnershipState& Province : RtsWorld.Ownership.Provinces)
    {
        if (!Province.CurrentControllerCountryName.Equals(CountryName, ESearchCase::IgnoreCase) && !Province.OriginalCountryName.Equals(CountryName, ESearchCase::IgnoreCase))
        {
            continue;
        }
        if (Province.ResourceFocus.Contains(TEXT("Fuel"), ESearchCase::IgnoreCase) || Province.ResourceFocus.Contains(TEXT("Gas"), ESearchCase::IgnoreCase) || Province.ResourceFocus.Contains(TEXT("Oil"), ESearchCase::IgnoreCase)) { ++FuelNodes; }
        else if (Province.ResourceFocus.Contains(TEXT("Wood"), ESearchCase::IgnoreCase) || Province.ResourceFocus.Contains(TEXT("Forest"), ESearchCase::IgnoreCase)) { ++WoodNodes; }
        else if (Province.ResourceFocus.Contains(TEXT("Metal"), ESearchCase::IgnoreCase) || Province.ResourceFocus.Contains(TEXT("Mine"), ESearchCase::IgnoreCase)) { ++MetalNodes; }
        else { ++FoodNodes; }
    }
    RegionalCanvas->AddSlot().Anchors(FAnchors(0.0f, 0.0f)).Offset(FMargin(320.0f, 600.0f, 120.0f, 58.0f))[MakeResource(TEXT("Food node"), FoodNodes, FLinearColor(0.35f, 0.48f, 0.10f, 0.95f))];
    RegionalCanvas->AddSlot().Anchors(FAnchors(0.0f, 0.0f)).Offset(FMargin(2170.0f, 620.0f, 120.0f, 58.0f))[MakeResource(TEXT("Fuel node"), FuelNodes, FLinearColor(0.06f, 0.34f, 0.42f, 0.95f))];
    RegionalCanvas->AddSlot().Anchors(FAnchors(0.0f, 0.0f)).Offset(FMargin(260.0f, 250.0f, 120.0f, 58.0f))[MakeResource(TEXT("Wood node"), WoodNodes, FLinearColor(0.18f, 0.44f, 0.16f, 0.95f))];
    RegionalCanvas->AddSlot().Anchors(FAnchors(0.0f, 0.0f)).Offset(FMargin(2020.0f, 920.0f, 120.0f, 58.0f))[MakeResource(TEXT("Metal node"), MetalNodes, FLinearColor(0.42f, 0.42f, 0.46f, 0.95f))];

    int32 ArmyIndex = 0;
    for (const FDemocracyRtsArmyGroupState& Army : RtsWorld.ArmyGroups)
    {
        if (ArmyIndex >= 5)
        {
            break;
        }
        const float X = 930.0f + (ArmyIndex * 138.0f);
        const float Y = 825.0f + ((ArmyIndex % 2) * 72.0f);
        const FString ArmyLabel = FString::Printf(TEXT("Army %d\nS%d T%d C%d"), ArmyIndex + 1, Army.InfantryCount, Army.VehicleCount, Army.LogisticsCount);
        RegionalCanvas->AddSlot().Anchors(FAnchors(0.0f, 0.0f)).Offset(FMargin(X, Y, 106.0f, 60.0f))
        [MakeLabel(ArmyLabel, Army.bSelected ? FLinearColor(0.18f, 0.52f, 0.20f, 0.98f) : FLinearColor(0.06f, 0.22f, 0.16f, 0.92f), 8)];
        ++ArmyIndex;
    }

    const FString HeaderText = FString::Printf(TEXT("Regional Country View | %s | %s | Food %+d Fuel %+d Wood %+d Metals %+d"),
        *CountryName,
        *ProvinceName,
        RtsWorld.ResourceCollection.FoodSentToSimulation,
        RtsWorld.ResourceCollection.FuelSentToSimulation,
        RtsWorld.ResourceCollection.WoodSentToSimulation,
        RtsWorld.ResourceCollection.MetalsSentToSimulation);

    return SNew(SOverlay)
        + SOverlay::Slot()
        [
            SNew(SRtsMapInputSurface)
            .OnMapMouseWheel(FPointerEventHandler::CreateUObject(this, &ALoginHUD::HandleRtsMapMouseWheel))
            .OnMapMouseButtonDown(FPointerEventHandler::CreateUObject(this, &ALoginHUD::HandleRtsMapMouseButtonDown))
            .OnMapMouseButtonUp(FPointerEventHandler::CreateUObject(this, &ALoginHUD::HandleRtsMapMouseButtonUp))
            .OnMapMouseMove(FPointerEventHandler::CreateUObject(this, &ALoginHUD::HandleRtsMapMouseMove))
            [
                SNew(SBorder)
                .BorderImage(RtsWaterBrush.Get())
                .BorderBackgroundColor(FLinearColor(0.08f, 0.18f, 0.13f, 1.0f))
                .Clipping(EWidgetClipping::ClipToBounds)
                .Padding(0.0f)
                [
                    SNew(SOverlay)
                    + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(FMargin(RtsMapPan.X, RtsMapPan.Y, 0.0f, 0.0f))
                    [
                        SNew(SBox).WidthOverride(RegionalWidth).HeightOverride(RegionalHeight)[RegionalCanvas]
                    ]
                ]
            ]
        ]
        + SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(16.0f)
        [MakeLabel(HeaderText, FLinearColor(0.03f, 0.07f, 0.08f, 0.84f), 12)]
        + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(16.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [BuildButton(TEXT("World View"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleResetRtsMapViewClicked), 136.0f, 36.0f)]
            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [BuildButton(TEXT("City View"), FOnClicked::CreateLambda([this]() { EnterRtsCityBaseView(); RefreshLoginWidget(); return FReply::Handled(); }), 126.0f, 36.0f)]
            + SHorizontalBox::Slot().AutoWidth()
            [BuildButton(TEXT("Return To Office"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleCloseOfficeOverlayClicked), 172.0f, 36.0f)]
        ]
        + SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(16.0f)
        [
            SNew(SBorder)
            .BorderImage(RowBrush.Get())
            .BorderBackgroundColor(FLinearColor(0.03f, 0.07f, 0.08f, 0.82f))
            .Padding(12.0f)
            [
                SNew(STextBlock)
                .Text(BodyText(TEXT("Regional controls: drag to scan the country, wheel up for city/base, wheel down for national map. Cities, nodes, roads, rivers, and army groups are shown here instead of the political world map.")))
                .AutoWrapText(true)
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                .ColorAndOpacity(FLinearColor(0.90f, 0.96f, 0.92f, 1.0f))
            ]
        ];
}
TSharedRef<SWidget> ALoginHUD::BuildRtsCityBasePlaceholderWidget()
{
    FDemocracyRtsWorldState& RtsWorld = LoadedSaveState.RuntimeState.RtsWorld;
    FDemocracyRtsCityBaseState& Base = RtsWorld.CityBase;
    RecountRtsCityBaseQueues(Base);

    FString SelectedBuildingText = RtsWorld.WorldInteraction.LastInteractionSummary.IsEmpty()
        ? TEXT("Select a building pad to inspect cost, output, construction state, and upgrade options.")
        : RtsWorld.WorldInteraction.LastInteractionSummary;
    for (const FDemocracyRtsBuildingState& Building : Base.Buildings)
    {
        if (Building.BuildingId.Equals(RtsWorld.WorldInteraction.ActiveSelectionId, ESearchCase::IgnoreCase))
        {
            SelectedBuildingText = FString::Printf(TEXT("Selected building: %s\nType %s | focus %s | level %d | %s\nOutput %+d/tick | defense %d | health %d/%d | damage %d%%\nBuild cost %d | upgrade cost %d | build time %d turn(s)\nPrerequisites: %s"),
                *Building.DisplayName,
                *Building.BuildingType,
                *Building.ResourceFocus,
                Building.Level,
                *Building.Status,
                Building.ProductionPerTick,
                Building.DefenseValue,
                Building.CurrentHealth,
                Building.MaxHealth,
                Building.DamagePercent,
                Building.BuildCost,
                Building.UpgradeCost,
                Building.BuildTimeTurns,
                Building.Prerequisites.Num() > 0 ? *FString::Join(Building.Prerequisites, TEXT(", ")) : TEXT("none"));
            break;
        }
    }

    auto FindActiveQueueForBuilding = [&Base](const FString& BuildingId) -> const FDemocracyRtsConstructionQueueEntryState*
    {
        for (const FDemocracyRtsConstructionQueueEntryState& QueueEntry : Base.ConstructionQueue)
        {
            if (!QueueEntry.bCancelled && !QueueEntry.bComplete && QueueEntry.BuildingId.Equals(BuildingId, ESearchCase::IgnoreCase))
            {
                return &QueueEntry;
            }
        }
        return nullptr;
    };

    auto MakeBuildingPad = [this, &Base, &RtsWorld, &FindActiveQueueForBuilding](int32 Index) -> TSharedRef<SWidget>
    {
        const FDemocracyRtsBuildingState* Building = Base.Buildings.IsValidIndex(Index) ? &Base.Buildings[Index] : nullptr;
        const FString SlotFocus = Building ? Building->ResourceFocus : GetRtsSlotResourceFocus(Index);
        const FDemocracyRtsConstructionQueueEntryState* QueueEntry = Building ? FindActiveQueueForBuilding(Building->BuildingId) : nullptr;
        const bool bSelected = Building && Building->BuildingId.Equals(RtsWorld.WorldInteraction.ActiveSelectionId, ESearchCase::IgnoreCase);
        const bool bDisabled = Building && (Building->bDisabled || Building->DamagePercent >= 75);
        const bool bQueued = QueueEntry != nullptr;
        const FString PadTitle = Building ? Building->DisplayName : FString::Printf(TEXT("Pad %02d"), Index + 1);
        const FString StatusText = Building
            ? FString::Printf(TEXT("L%d %s%+d"), Building->Level, *Building->ResourceFocus.Left(3).ToUpper(), Building->ProductionPerTick)
            : FString::Printf(TEXT("%s site"), *SlotFocus.Left(4).ToUpper());
        const FLinearColor PadColor = bSelected
            ? FLinearColor(0.22f, 0.58f, 0.64f, 0.98f)
            : (bQueued ? FLinearColor(0.56f, 0.38f, 0.08f, 0.96f) : (bDisabled ? FLinearColor(0.36f, 0.05f, 0.04f, 0.94f) : (Building ? FLinearColor(0.06f, 0.19f, 0.13f, 0.96f) : FLinearColor(0.07f, 0.09f, 0.09f, 0.80f))));
        const FLinearColor RoofColor = SlotFocus.Equals(TEXT("Food"), ESearchCase::IgnoreCase) ? FLinearColor(0.26f, 0.46f, 0.16f, 0.92f)
            : (SlotFocus.Equals(TEXT("Fuel"), ESearchCase::IgnoreCase) ? FLinearColor(0.06f, 0.32f, 0.38f, 0.92f)
            : (SlotFocus.Equals(TEXT("Wood"), ESearchCase::IgnoreCase) ? FLinearColor(0.40f, 0.28f, 0.10f, 0.92f)
            : (SlotFocus.Equals(TEXT("Metals"), ESearchCase::IgnoreCase) ? FLinearColor(0.34f, 0.34f, 0.36f, 0.92f) : FLinearColor(0.22f, 0.25f, 0.30f, 0.92f))));

        return SNew(SButton)
            .ButtonStyle(FCoreStyle::Get(), "NoBorder")
            .OnClicked(FOnClicked::CreateUObject(this, &ALoginHUD::HandleSelectRtsBuildingSlot, Index))
            .ContentPadding(0.0f)
            [
                SNew(SOverlay)
                + SOverlay::Slot()
                [
                    SNew(SBorder)
                    .BorderImage(RowBrush.Get())
                    .BorderBackgroundColor(PadColor)
                    .Padding(0.0f)
                ]
                + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(8.0f, 5.0f, 8.0f, 0.0f)
                [
                    SNew(SBox)
                    .HeightOverride(14.0f)
                    .WidthOverride(84.0f)
                    [SNew(SBorder).BorderImage(RowBrush.Get()).BorderBackgroundColor(RoofColor).Padding(0.0f)]
                ]
                + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(BodyText(PadTitle.Left(18)))
                    .Justification(ETextJustify::Center)
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
                    .ColorAndOpacity(FLinearColor(0.94f, 0.98f, 0.92f, 1.0f))
                ]
                + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0.0f, 0.0f, 0.0f, 5.0f)
                [
                    SNew(STextBlock)
                    .Text(BodyText(StatusText))
                    .Justification(ETextJustify::Center)
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                    .ColorAndOpacity(FLinearColor(0.86f, 0.94f, 0.90f, 1.0f))
                ]
            ];
    };
    auto MakeResourceNode = [this, &RtsWorld](const FString& ResourceName, const FString& NodeLabel) -> TSharedRef<SWidget>
    {
        int32 BuildingOutput = 0;
        int32 ProvinceOutput = 0;
        if (ResourceName.Equals(TEXT("Food"), ESearchCase::IgnoreCase)) { BuildingOutput = RtsWorld.ResourceCollection.FoodFromBuildings; ProvinceOutput = RtsWorld.ResourceCollection.FoodFromProvinces; }
        else if (ResourceName.Equals(TEXT("Fuel"), ESearchCase::IgnoreCase)) { BuildingOutput = RtsWorld.ResourceCollection.FuelFromBuildings; ProvinceOutput = RtsWorld.ResourceCollection.FuelFromProvinces; }
        else if (ResourceName.Equals(TEXT("Wood"), ESearchCase::IgnoreCase)) { BuildingOutput = RtsWorld.ResourceCollection.WoodFromBuildings; ProvinceOutput = RtsWorld.ResourceCollection.WoodFromProvinces; }
        else { BuildingOutput = RtsWorld.ResourceCollection.MetalsFromBuildings; ProvinceOutput = RtsWorld.ResourceCollection.MetalsFromProvinces; }

        int32 NodeCount = 0;
        for (const FDemocracyProvinceOwnershipState& Province : RtsWorld.Ownership.Provinces)
        {
            if (Province.bPlayerControlled && Province.ResourceFocus.Equals(ResourceName, ESearchCase::IgnoreCase))
            {
                ++NodeCount;
            }
        }

        const FLinearColor NodeColor = ResourceName.Equals(TEXT("Food"), ESearchCase::IgnoreCase) ? FLinearColor(0.22f, 0.42f, 0.10f, 0.94f)
            : (ResourceName.Equals(TEXT("Fuel"), ESearchCase::IgnoreCase) ? FLinearColor(0.04f, 0.30f, 0.36f, 0.94f)
            : (ResourceName.Equals(TEXT("Wood"), ESearchCase::IgnoreCase) ? FLinearColor(0.36f, 0.24f, 0.08f, 0.94f) : FLinearColor(0.28f, 0.28f, 0.30f, 0.94f)));

        return SNew(SBorder)
            .BorderImage(RowBrush.Get())
            .BorderBackgroundColor(NodeColor)
            .Padding(5.0f)
            [
                SNew(STextBlock)
                .Text(BodyText(FString::Printf(TEXT("%s x%d\n%+d/t"), *ResourceName, NodeCount, BuildingOutput + ProvinceOutput)))
                .Justification(ETextJustify::Center)
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
                .ColorAndOpacity(FLinearColor(0.90f, 0.98f, 0.94f, 1.0f))
            ];
    };
    auto MakeTerrainFeature = [this](const FString& Label, const FLinearColor& Color, const FName& FontWeight, int32 FontSize) -> TSharedRef<SWidget>
    {
        return SNew(SBorder)
            .BorderImage(RowBrush.Get())
            .BorderBackgroundColor(Color)
            .Padding(0.0f);
    };
    auto MakeTerrainPatch = [this](const FLinearColor& Color) -> TSharedRef<SWidget>
    {
        return SNew(SBorder)
            .BorderImage(RowBrush.Get())
            .BorderBackgroundColor(Color)
            .Padding(0.0f);
    };
    auto MakeSmallLabel = [this](const FString& Label, const FLinearColor& Color, int32 FontSize = 8) -> TSharedRef<SWidget>
    {
        return SNew(SBorder)
            .BorderImage(RowBrush.Get())
            .BorderBackgroundColor(Color)
            .Padding(FMargin(5.0f, 2.0f))
            [
                SNew(STextBlock)
                .Text(BodyText(Label))
                .Justification(ETextJustify::Center)
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", FontSize))
                .ColorAndOpacity(FLinearColor(0.90f, 0.98f, 0.92f, 1.0f))
            ];
    };
    auto MakeUnitMarker = [this](const FString& Label, int32 Count, const FLinearColor& Color, const FString& ShapeHint) -> TSharedRef<SWidget>
    {
        const FString CountText = Count > 0 ? FString::Printf(TEXT("%s\nx%d"), *Label, Count) : Label;
        return SNew(SBorder)
            .BorderImage(RowBrush.Get())
            .BorderBackgroundColor(Color)
            .Padding(4.0f)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                [
                    SNew(SBox)
                    .WidthOverride(34.0f)
                    .HeightOverride(ShapeHint.Equals(TEXT("wide"), ESearchCase::IgnoreCase) ? 18.0f : 28.0f)
                    [SNew(SBorder).BorderImage(RowBrush.Get()).BorderBackgroundColor(FLinearColor(Color.R + 0.12f, Color.G + 0.12f, Color.B + 0.12f, 0.98f)).Padding(0.0f)]
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)
                [
                    SNew(STextBlock)
                    .Text(BodyText(CountText))
                    .Justification(ETextJustify::Center)
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
                    .ColorAndOpacity(FLinearColor(0.94f, 0.98f, 0.94f, 1.0f))
                ]
            ];
    };

    TSharedRef<SConstraintCanvas> CityLayout = SNew(SConstraintCanvas);
    CityLayout->AddSlot().Anchors(FAnchors(0.0f, 0.0f)).Offset(FMargin(0.0f, 0.0f, 2200.0f, 1320.0f))
    [MakeTerrainPatch(FLinearColor(0.10f, 0.18f, 0.11f, 1.0f))];

    for (int32 TileY = 0; TileY < 12; ++TileY)
    {
        for (int32 TileX = 0; TileX < 20; ++TileX)
        {
            const float Variation = ((TileX * 17 + TileY * 31) % 9) * 0.006f;
            const float PatchAlpha = 0.42f + (((TileX + TileY) % 3) * 0.08f);
            CityLayout->AddSlot().Anchors(FAnchors(0.0f, 0.0f)).Offset(FMargin(TileX * 112.0f, TileY * 112.0f, 116.0f, 116.0f))
            [MakeTerrainPatch(FLinearColor(0.08f + Variation, 0.17f + Variation, 0.10f + Variation, PatchAlpha))];
        }
    }

    CityLayout->AddSlot().Anchors(FAnchors(0.0f, 0.0f)).Offset(FMargin(0.0f, 0.0f, 2200.0f, 160.0f))
    [MakeTerrainFeature(TEXT("forest edge"), FLinearColor(0.02f, 0.08f, 0.04f, 0.94f), TEXT("Regular"), 8)];
    for (int32 TreeIndex = 0; TreeIndex < 26; ++TreeIndex)
    {
        const float TreeX = 60.0f + TreeIndex * 82.0f;
        const float TreeY = 24.0f + ((TreeIndex * 37) % 94);
        CityLayout->AddSlot().Anchors(FAnchors(0.0f, 0.0f)).Offset(FMargin(TreeX, TreeY, 34.0f, 44.0f))
        [MakeTerrainPatch(FLinearColor(0.01f, 0.16f + ((TreeIndex % 4) * 0.018f), 0.05f, 0.92f))];
    }
    CityLayout->AddSlot().Anchors(FAnchors(0.0f, 0.0f)).Offset(FMargin(1600.0f, 220.0f, 460.0f, 280.0f))
    [MakeTerrainFeature(TEXT("lake"), FLinearColor(0.02f, 0.25f, 0.38f, 0.94f), TEXT("Bold"), 11)];
    CityLayout->AddSlot().Anchors(FAnchors(0.0f, 0.0f)).Offset(FMargin(128.0f, 1118.0f, 1600.0f, 58.0f))
    [MakeTerrainFeature(TEXT("river"), FLinearColor(0.03f, 0.25f, 0.36f, 0.90f), TEXT("Regular"), 8)];
    CityLayout->AddSlot().Anchors(FAnchors(0.0f, 0.0f)).Offset(FMargin(170.0f, 372.0f, 340.0f, 236.0f))
    [MakeTerrainFeature(TEXT("farmland"), FLinearColor(0.18f, 0.30f, 0.09f, 0.70f), TEXT("Regular"), 8)];
    for (int32 Furrow = 0; Furrow < 9; ++Furrow)
    {
        CityLayout->AddSlot().Anchors(FAnchors(0.0f, 0.0f)).Offset(FMargin(190.0f, 392.0f + Furrow * 22.0f, 300.0f, 4.0f))
        [MakeTerrainPatch(FLinearColor(0.24f, 0.36f, 0.12f, 0.68f))];
    }
    CityLayout->AddSlot().Anchors(FAnchors(0.0f, 0.0f)).Offset(FMargin(1690.0f, 800.0f, 330.0f, 158.0f))
    [MakeTerrainFeature(TEXT("mine ridge"), FLinearColor(0.20f, 0.20f, 0.17f, 0.78f), TEXT("Regular"), 8)];
    CityLayout->AddSlot().Anchors(FAnchors(0.0f, 0.0f)).Offset(FMargin(1470.0f, 990.0f, 270.0f, 96.0f))
    [MakeTerrainFeature(TEXT("fuel node"), FLinearColor(0.10f, 0.18f, 0.18f, 0.80f), TEXT("Regular"), 8)];

    CityLayout->AddSlot().Anchors(FAnchors(0.0f, 0.0f)).Offset(FMargin(560.0f, 250.0f, 980.0f, 610.0f))
    [MakeTerrainFeature(TEXT("buildable base clearing"), FLinearColor(0.15f, 0.23f, 0.16f, 0.54f), TEXT("Regular"), 8)];
    const FVector2D BaseOrigin(590.0f, 286.0f);
    CityLayout->AddSlot().Anchors(FAnchors(0.0f, 0.0f)).Offset(FMargin(BaseOrigin.X + 20.0f, BaseOrigin.Y + 270.0f, 1080.0f, 36.0f))
    [SNew(SBorder).BorderImage(RowBrush.Get()).BorderBackgroundColor(FLinearColor(0.18f, 0.19f, 0.17f, 0.92f)).Padding(0.0f)];
    CityLayout->AddSlot().Anchors(FAnchors(0.0f, 0.0f)).Offset(FMargin(BaseOrigin.X + 520.0f, BaseOrigin.Y + 24.0f, 36.0f, 572.0f))
    [SNew(SBorder).BorderImage(RowBrush.Get()).BorderBackgroundColor(FLinearColor(0.18f, 0.19f, 0.17f, 0.92f)).Padding(0.0f)];
    CityLayout->AddSlot().Anchors(FAnchors(0.0f, 0.0f)).Offset(FMargin(BaseOrigin.X + 492.0f, BaseOrigin.Y + 254.0f, 136.0f, 98.0f))
    [
        SNew(SBorder)
        .BorderImage(RowBrush.Get())
        .BorderBackgroundColor(FLinearColor(0.17f, 0.16f, 0.13f, 0.96f))
        .Padding(8.0f)
        [
            SNew(STextBlock)
            .Text(BodyText(TEXT("Capital\nCore")))
            .Justification(ETextJustify::Center)
            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
            .ColorAndOpacity(FLinearColor(0.96f, 0.92f, 0.78f, 1.0f))
        ]
    ];
    CityLayout->AddSlot().Anchors(FAnchors(0.0f, 0.0f)).Offset(FMargin(BaseOrigin.X + 420.0f, BaseOrigin.Y + 384.0f, 280.0f, 40.0f))
    [MakeSmallLabel(TEXT("Unit staging: SOL  TNK  SUP  WRK"), FLinearColor(0.04f, 0.10f, 0.11f, 0.78f), 8)];

    const TArray<FVector2D> SlotPositions = {
        FVector2D(BaseOrigin.X + 96.0f, BaseOrigin.Y + 80.0f), FVector2D(BaseOrigin.X + 336.0f, BaseOrigin.Y + 76.0f), FVector2D(BaseOrigin.X + 684.0f, BaseOrigin.Y + 76.0f), FVector2D(BaseOrigin.X + 924.0f, BaseOrigin.Y + 80.0f),
        FVector2D(BaseOrigin.X + 94.0f, BaseOrigin.Y + 246.0f), FVector2D(BaseOrigin.X + 318.0f, BaseOrigin.Y + 226.0f), FVector2D(BaseOrigin.X + 702.0f, BaseOrigin.Y + 226.0f), FVector2D(BaseOrigin.X + 924.0f, BaseOrigin.Y + 246.0f),
        FVector2D(BaseOrigin.X + 96.0f, BaseOrigin.Y + 390.0f), FVector2D(BaseOrigin.X + 336.0f, BaseOrigin.Y + 394.0f), FVector2D(BaseOrigin.X + 684.0f, BaseOrigin.Y + 394.0f), FVector2D(BaseOrigin.X + 924.0f, BaseOrigin.Y + 390.0f)
    };
    for (int32 Index = 0; Index < SlotPositions.Num(); ++Index)
    {
        CityLayout->AddSlot().Anchors(FAnchors(0.0f, 0.0f)).Offset(FMargin(SlotPositions[Index].X, SlotPositions[Index].Y, 108.0f, 70.0f))
        [MakeBuildingPad(Index)];
    }

    const FDemocracyRtsArmyGroupState* CityArmy = nullptr;
    for (const FDemocracyRtsArmyGroupState& Army : RtsWorld.ArmyGroups)
    {
        if (Army.bSelected || CityArmy == nullptr)
        {
            CityArmy = &Army;
            if (Army.bSelected)
            {
                break;
            }
        }
    }
    const int32 InfantryVisualCount = CityArmy ? FMath::Max(0, CityArmy->InfantryCount) : 0;
    const int32 TankVisualCount = CityArmy ? FMath::Max(0, CityArmy->VehicleCount) : 0;
    const int32 LogisticsVisualCount = CityArmy ? FMath::Max(0, CityArmy->LogisticsCount) : 0;
    if (InfantryVisualCount > 0)
    {
        CityLayout->AddSlot().Anchors(FAnchors(0.0f, 0.0f)).Offset(FMargin(BaseOrigin.X + 370.0f, BaseOrigin.Y + 446.0f, 64.0f, 58.0f))
        [MakeUnitMarker(TEXT("SOL"), InfantryVisualCount, FLinearColor(0.06f, 0.42f, 0.22f, 0.98f), TEXT("tall"))];
    }
    if (TankVisualCount > 0)
    {
        CityLayout->AddSlot().Anchors(FAnchors(0.0f, 0.0f)).Offset(FMargin(BaseOrigin.X + 454.0f, BaseOrigin.Y + 452.0f, 70.0f, 54.0f))
        [MakeUnitMarker(TEXT("TNK"), TankVisualCount, FLinearColor(0.05f, 0.30f, 0.52f, 0.98f), TEXT("wide"))];
    }
    if (LogisticsVisualCount > 0)
    {
        CityLayout->AddSlot().Anchors(FAnchors(0.0f, 0.0f)).Offset(FMargin(BaseOrigin.X + 540.0f, BaseOrigin.Y + 452.0f, 72.0f, 54.0f))
        [MakeUnitMarker(TEXT("CON"), LogisticsVisualCount, FLinearColor(0.08f, 0.35f, 0.24f, 0.98f), TEXT("wide"))];
        CityLayout->AddSlot().Anchors(FAnchors(0.0f, 0.0f)).Offset(FMargin(BaseOrigin.X + 628.0f, BaseOrigin.Y + 448.0f, 64.0f, 58.0f))
        [MakeUnitMarker(TEXT("WRK"), FMath::Max(1, LogisticsVisualCount / 2), FLinearColor(0.18f, 0.46f, 0.22f, 0.98f), TEXT("tall"))];
    }
    CityLayout->AddSlot().Anchors(FAnchors(0.0f, 0.0f)).Offset(FMargin(286.0f, 476.0f, 84.0f, 34.0f))[MakeResourceNode(TEXT("Food"), TEXT("farmland"))];
    CityLayout->AddSlot().Anchors(FAnchors(0.0f, 0.0f)).Offset(FMargin(1742.0f, 336.0f, 84.0f, 34.0f))[MakeResourceNode(TEXT("Fuel"), TEXT("depot"))];
    CityLayout->AddSlot().Anchors(FAnchors(0.0f, 0.0f)).Offset(FMargin(254.0f, 1168.0f, 84.0f, 34.0f))[MakeResourceNode(TEXT("Wood"), TEXT("timber"))];
    CityLayout->AddSlot().Anchors(FAnchors(0.0f, 0.0f)).Offset(FMargin(1792.0f, 878.0f, 84.0f, 34.0f))[MakeResourceNode(TEXT("Metals"), TEXT("mine"))];
    TSharedRef<SVerticalBox> QueueList = SNew(SVerticalBox);
    int32 DisplayedQueueRows = 0;
    for (const FDemocracyRtsConstructionQueueEntryState& QueueEntry : Base.ConstructionQueue)
    {
        if (QueueEntry.bCancelled || QueueEntry.bComplete)
        {
            continue;
        }
        ++DisplayedQueueRows;
        QueueList->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 5.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1.0f)
            [
                SNew(STextBlock)
                .Text(BodyText(FString::Printf(TEXT("%s | %s -> L%d | %d/%d turns | %s"), *QueueEntry.QueueType, *QueueEntry.DisplayName, QueueEntry.TargetLevel, QueueEntry.TurnsRemaining, QueueEntry.TotalTurns, *BuildRtsCostText(QueueEntry.TreasuryCost, QueueEntry.FoodCost, QueueEntry.FuelCost, QueueEntry.WoodCost, QueueEntry.MetalsCost))))
                .AutoWrapText(true)
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                .ColorAndOpacity(FLinearColor(0.93f, 0.95f, 0.88f, 1.0f))
            ]
            + SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f, 0.0f, 0.0f)
            [BuildButton(TEXT("Cancel"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleCancelRtsConstruction, QueueEntry.QueueId), 78.0f, 28.0f, QueueEntry.bCanCancel)]
        ];
    }
    if (DisplayedQueueRows == 0)
    {
        QueueList->AddSlot().AutoHeight()
        [
            SNew(STextBlock)
            .Text(BodyText(TEXT("No active construction or upgrade queue.")))
            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
            .ColorAndOpacity(FLinearColor(0.78f, 0.86f, 0.82f, 1.0f))
        ];
    }

    const float CityViewScale = FMath::Clamp(0.72f + ((RtsMapZoom - 4.5f) / 1.7f) * 0.58f, 0.72f, 1.30f);
    const FString CompactHeader = FString::Printf(TEXT("%s | Food %+d  Fuel %+d  Wood %+d  Metals %+d  Tick %d"),
        *Base.DisplayName,
        RtsWorld.ResourceCollection.FoodSentToSimulation,
        RtsWorld.ResourceCollection.FuelSentToSimulation,
        RtsWorld.ResourceCollection.WoodSentToSimulation,
        RtsWorld.ResourceCollection.MetalsSentToSimulation,
        RtsWorld.RtsTickCount);
    const FString CompactSelection = SelectedBuildingText.Left(220);
    const FString CompactSelectionText = CompactSelection.IsEmpty() ? FString(TEXT("Select a building, resource node, or unit group.")) : CompactSelection;

    return SNew(SOverlay)
        + SOverlay::Slot()
        [
            SNew(SRtsMapInputSurface)
            .OnMapMouseWheel(FPointerEventHandler::CreateUObject(this, &ALoginHUD::HandleRtsMapMouseWheel))
            .OnMapMouseButtonDown(FPointerEventHandler::CreateUObject(this, &ALoginHUD::HandleRtsMapMouseButtonDown))
            .OnMapMouseButtonUp(FPointerEventHandler::CreateUObject(this, &ALoginHUD::HandleRtsMapMouseButtonUp))
            .OnMapMouseMove(FPointerEventHandler::CreateUObject(this, &ALoginHUD::HandleRtsMapMouseMove))
            [
                SNew(SBorder)
                .BorderImage(RtsWaterBrush.Get())
                .BorderBackgroundColor(FLinearColor(0.06f, 0.12f, 0.08f, 1.0f))
                .Clipping(EWidgetClipping::ClipToBounds)
                .Padding(0.0f)
                [
                    SNew(SOverlay)
                    + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(FMargin(RtsMapPan.X, RtsMapPan.Y, 0.0f, 0.0f))
                    [
                        SNew(SBox)
                        .WidthOverride(2200.0f)
                        .HeightOverride(1320.0f)
                        [CityLayout]
                    ]
                ]
            ]
        ]
        + SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(16.0f)
        [
            SNew(SBorder)
            .BorderImage(RowBrush.Get())
            .BorderBackgroundColor(FLinearColor(0.03f, 0.08f, 0.09f, 0.76f))
            .Padding(FMargin(12.0f, 8.0f))
            [
                SNew(STextBlock)
                .Text(BodyText(CompactHeader))
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
                .ColorAndOpacity(FLinearColor(0.90f, 0.98f, 0.94f, 1.0f))
            ]
        ]
        + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(16.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [BuildButton(TEXT("Regional View"), FOnClicked::CreateLambda([this]() { EnterRtsRegionalCountryView(true); RefreshLoginWidget(); return FReply::Handled(); }), 154.0f, 36.0f)]
            + SHorizontalBox::Slot().AutoWidth()
            [BuildButton(TEXT("Return To Office"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleCloseOfficeOverlayClicked), 172.0f, 36.0f)]
        ]
        + SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(16.0f)
        [
            SNew(SBorder)
            .BorderImage(RowBrush.Get())
            .BorderBackgroundColor(FLinearColor(0.03f, 0.07f, 0.08f, 0.78f))
            .Padding(FMargin(12.0f, 8.0f))
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(STextBlock)
                    .Text(BodyText(CompactSelectionText))
                    .AutoWrapText(true)
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
                    .ColorAndOpacity(FLinearColor(0.92f, 0.98f, 1.0f, 1.0f))
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
                    [BuildButton(TEXT("Spawn Soldier"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleRecruitRtsUnitsClicked, FString(TEXT("Soldiers"))), 132.0f, 34.0f, bHasLoadedRuntimeState)]
                    + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
                    [BuildButton(TEXT("Spawn Tank"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleRecruitRtsUnitsClicked, FString(TEXT("Tanks"))), 116.0f, 34.0f, bHasLoadedRuntimeState)]
                    + SHorizontalBox::Slot().AutoWidth()
                    [BuildButton(TEXT("Spawn Convoy"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleRecruitRtsUnitsClicked, FString(TEXT("Supply Carriers"))), 132.0f, 34.0f, bHasLoadedRuntimeState)]
                    + SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f, 0.0f, 0.0f)
                    [BuildButton(TEXT("Spawn Worker"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleRecruitRtsUnitsClicked, FString(TEXT("Workers"))), 132.0f, 34.0f, bHasLoadedRuntimeState)]
                ]
            ]
        ]
        + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(16.0f)
        [
            SNew(SBorder)
            .BorderImage(RowBrush.Get())
            .BorderBackgroundColor(FLinearColor(0.03f, 0.07f, 0.08f, 0.74f))
            .Padding(FMargin(12.0f, 8.0f))
            [
                SNew(SBox)
                .WidthOverride(460.0f)
                .MaxDesiredHeight(86.0f)
                [QueueList]
            ]
        ];
}
TSharedRef<SWidget> ALoginHUD::BuildRtsProvinceStateOverlaysWidget(float MapWidth, float MapHeight) const
{
    TSharedRef<SOverlay> Overlay = SNew(SOverlay);
    if (!bHasLoadedRuntimeState)
    {
        return Overlay;
    }

    const FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    const TArray<FDuliaCountryMapPoint>& Points = GetDuliaCountryMapPoints();
    const float ScaleX = MapWidth / 2242.0f;
    const float ScaleY = MapHeight / 1104.0f;
    constexpr int32 RequiredOccupationTurns = 3;

    for (const FDemocracyCountryOwnershipState& Country : State.RtsWorld.Ownership.Countries)
    {
        const FDuliaCountryMapPoint* Point = nullptr;
        for (const FDuliaCountryMapPoint& CandidatePoint : Points)
        {
            if (CandidatePoint.CountryIndex == Country.MapCountryIndex)
            {
                Point = &CandidatePoint;
                break;
            }
        }
        if (!Point)
        {
            continue;
        }

        int32 Controlled = 0;
        int32 Occupied = 0;
        int32 PeaceRequired = 0;
        int32 CaptureTurnsRemaining = 0;
        for (const FString& ProvinceId : Country.ProvinceIds)
        {
            const FDemocracyProvinceOwnershipState* Province = FindRtsProvinceById(State, ProvinceId);
            if (!Province)
            {
                continue;
            }
            if (Province->CurrentControllerCountryName.Equals(State.PlayerCountry.CountryName, ESearchCase::IgnoreCase))
            {
                ++Controlled;
            }
            if (!Province->CurrentOwnerCountryName.Equals(Province->CurrentControllerCountryName, ESearchCase::IgnoreCase))
            {
                ++Occupied;
                ++PeaceRequired;
                const int32 HeldTurns = FMath::Max(0, State.Turn - Province->LastChangedTurn);
                CaptureTurnsRemaining = FMath::Max(CaptureTurnsRemaining, FMath::Max(0, RequiredOccupationTurns - HeldTurns));
            }
        }

        FString StateLabel = TEXT("Neutral");
        FLinearColor StateColor = FLinearColor(0.15f, 0.22f, 0.30f, 0.58f);
        if (Country.bPlayerCountry || Controlled > 0)
        {
            StateLabel = Controlled >= Country.TotalProvinces ? TEXT("Owned") : TEXT("Controlled");
            StateColor = FLinearColor(0.02f, 0.75f, 0.22f, 0.68f);
        }
        if (Occupied > 0)
        {
            StateLabel = TEXT("Occupied");
            StateColor = FLinearColor(1.0f, 0.55f, 0.05f, 0.78f);
        }
        if (PeaceRequired > 0)
        {
            StateLabel = TEXT("Peace Req.");
            StateColor = FLinearColor(0.90f, 0.12f, 0.04f, 0.78f);
        }

        const bool bSelectedCountry = Country.CountryName.Equals(RtsSelectedCountryName, ESearchCase::IgnoreCase) || Country.ProvinceIds.Contains(RtsSelectedProvinceId);
        const bool bHoveredCountry = Country.CountryName.Equals(State.RtsWorld.WorldInteraction.HoveredTargetId, ESearchCase::IgnoreCase) || Country.CountryId.Equals(State.RtsWorld.WorldInteraction.HoveredTargetId, ESearchCase::IgnoreCase) || Country.ProvinceIds.Contains(State.RtsWorld.WorldInteraction.HoveredTargetId);
        if (bSelectedCountry)
        {
            StateLabel = FString::Printf(TEXT("Selected %s"), *StateLabel);
            StateColor = FLinearColor(1.0f, 0.84f, 0.12f, 0.90f);
        }
        else if (bHoveredCountry)
        {
            StateLabel = FString::Printf(TEXT("Hover %s"), *StateLabel);
            StateColor = FLinearColor(0.20f, 0.92f, 1.0f, 0.76f);
        }

        const bool bShowStateLabel = bHoveredCountry || Occupied > 0 || PeaceRequired > 0 || CaptureTurnsRemaining > 0;
        if (!bShowStateLabel)
        {
            continue;
        }
        const FString LabelText = FString::Printf(TEXT("%s\n%d/%d ctrl%s"), *StateLabel, Controlled, FMath::Max(1, Country.TotalProvinces), CaptureTurnsRemaining > 0 ? *FString::Printf(TEXT(" | cap %d"), CaptureTurnsRemaining) : TEXT(""));
        const float X = FMath::Clamp(Point->Centroid.X * ScaleX - 42.0f, 0.0f, FMath::Max(0.0f, MapWidth - 92.0f));
        const float Y = FMath::Clamp(Point->Centroid.Y * ScaleY - 58.0f, 0.0f, FMath::Max(0.0f, MapHeight - 42.0f));
        Overlay->AddSlot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(FMargin(X, Y, 0.0f, 0.0f))
        [
            SNew(SBorder)
            .BorderImage(RowBrush.Get())
            .BorderBackgroundColor(StateColor)
            .Padding(FMargin(5.0f, 2.0f))
            [
                SNew(STextBlock)
                .Text(BodyText(LabelText))
                .AutoWrapText(true)
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
                .ColorAndOpacity(FLinearColor::White)
            ]
        ];
    }
    return Overlay;
}

TSharedRef<SWidget> ALoginHUD::BuildRtsCountryOperationsOverlayWidget(float MapWidth, float MapHeight) const
{
    TSharedRef<SOverlay> Overlay = SNew(SOverlay);
    if (!bHasLoadedRuntimeState || RtsMapZoom < 1.35f || RtsMapZoom >= 4.5f)
    {
        return Overlay;
    }

    const FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    const TArray<FDuliaCountryMapPoint>& Points = GetDuliaCountryMapPoints();
    const float ScaleX = MapWidth / 2242.0f;
    const float ScaleY = MapHeight / 1104.0f;
    const bool bDetailedMiddleView = RtsMapZoom >= 2.05f;

    auto FindPointForCountry = [&Points](const FDemocracyCountryOwnershipState& Country) -> const FDuliaCountryMapPoint*
    {
        for (const FDuliaCountryMapPoint& CandidatePoint : Points)
        {
            if (CandidatePoint.CountryIndex == Country.MapCountryIndex)
            {
                return &CandidatePoint;
            }
        }
        return nullptr;
    };

    auto AddBoxMarker = [this, &Overlay, MapWidth, MapHeight](const FVector2D& Center, const FVector2D& Size, const FLinearColor& Color, const FString& Label, int32 FontSize)
    {
        const float X = FMath::Clamp(Center.X - (Size.X * 0.5f), 0.0f, FMath::Max(0.0f, MapWidth - Size.X));
        const float Y = FMath::Clamp(Center.Y - (Size.Y * 0.5f), 0.0f, FMath::Max(0.0f, MapHeight - Size.Y));
        Overlay->AddSlot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(FMargin(X, Y, 0.0f, 0.0f))
        [
            SNew(SBorder)
            .BorderImage(RowBrush.Get())
            .BorderBackgroundColor(Color)
            .Padding(FMargin(4.0f, 2.0f))
            [
                SNew(STextBlock)
                .Text(BodyText(Label))
                .Justification(ETextJustify::Center)
                .AutoWrapText(true)
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", FontSize))
                .ColorAndOpacity(FLinearColor::White)
            ]
        ];
    };

    auto AddTerrainPatch = [this, &Overlay, MapWidth, MapHeight](const FVector2D& Center, const FVector2D& Size, const FLinearColor& Color, const FString& Label)
    {
        const float X = FMath::Clamp(Center.X - (Size.X * 0.5f), 0.0f, FMath::Max(0.0f, MapWidth - Size.X));
        const float Y = FMath::Clamp(Center.Y - (Size.Y * 0.5f), 0.0f, FMath::Max(0.0f, MapHeight - Size.Y));
        Overlay->AddSlot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(FMargin(X, Y, 0.0f, 0.0f))
        [
            SNew(SBorder)
            .BorderImage(RowBrush.Get())
            .BorderBackgroundColor(Color)
            .Padding(FMargin(3.0f, 1.0f))
            [
                SNew(STextBlock)
                .Text(BodyText(Label))
                .Justification(ETextJustify::Center)
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 7))
                .ColorAndOpacity(FLinearColor(0.92f, 0.98f, 0.94f, 0.95f))
            ]
        ];
    };

    auto GetResourceStyle = [](const FString& ResourceFocus, FString& OutLabel, FLinearColor& OutColor)
    {
        if (ResourceFocus.Contains(TEXT("Fuel"), ESearchCase::IgnoreCase) || ResourceFocus.Contains(TEXT("Gas"), ESearchCase::IgnoreCase) || ResourceFocus.Contains(TEXT("Oil"), ESearchCase::IgnoreCase))
        {
            OutLabel = TEXT("Fuel");
            OutColor = FLinearColor(0.08f, 0.40f, 0.72f, 0.90f);
        }
        else if (ResourceFocus.Contains(TEXT("Wood"), ESearchCase::IgnoreCase) || ResourceFocus.Contains(TEXT("Lumber"), ESearchCase::IgnoreCase) || ResourceFocus.Contains(TEXT("Forest"), ESearchCase::IgnoreCase))
        {
            OutLabel = TEXT("Wood");
            OutColor = FLinearColor(0.18f, 0.55f, 0.18f, 0.90f);
        }
        else if (ResourceFocus.Contains(TEXT("Metal"), ESearchCase::IgnoreCase) || ResourceFocus.Contains(TEXT("Mine"), ESearchCase::IgnoreCase))
        {
            OutLabel = TEXT("Metal");
            OutColor = FLinearColor(0.56f, 0.56f, 0.62f, 0.92f);
        }
        else
        {
            OutLabel = TEXT("Food");
            OutColor = FLinearColor(0.56f, 0.68f, 0.16f, 0.90f);
        }
    };

    for (const FDemocracyCountryOwnershipState& Country : State.RtsWorld.Ownership.Countries)
    {
        const FDuliaCountryMapPoint* Point = FindPointForCountry(Country);
        if (!Point)
        {
            continue;
        }

        const bool bPlayerCountry = Country.bPlayerCountry || Country.CountryName.Equals(State.PlayerCountry.CountryName, ESearchCase::IgnoreCase);
        const bool bSelectedCountry = Country.CountryName.Equals(RtsSelectedCountryName, ESearchCase::IgnoreCase) || Country.ProvinceIds.Contains(RtsSelectedProvinceId);
        const bool bHoveredCountry = Country.CountryName.Equals(State.RtsWorld.WorldInteraction.HoveredTargetId, ESearchCase::IgnoreCase) || Country.CountryId.Equals(State.RtsWorld.WorldInteraction.HoveredTargetId, ESearchCase::IgnoreCase) || Country.ProvinceIds.Contains(State.RtsWorld.WorldInteraction.HoveredTargetId);
        const bool bDictatorship = Country.GovernmentType.Equals(TEXT("Dictatorship"), ESearchCase::IgnoreCase);
        const FLinearColor CityColor = bPlayerCountry ? FLinearColor(0.04f, 0.80f, 0.22f, 0.96f) : (bDictatorship ? FLinearColor(0.82f, 0.10f, 0.10f, 0.88f) : FLinearColor(0.08f, 0.34f, 0.86f, 0.86f));
        const FVector2D CountryCenter(Point->Centroid.X * ScaleX, Point->Centroid.Y * ScaleY);
        const bool bShowNamedMarker = bPlayerCountry || bSelectedCountry || bHoveredCountry;
        if (bShowNamedMarker)
        {
            const FString CityLabel = bPlayerCountry ? TEXT("CAP") : (bSelectedCountry ? TEXT("CITY") : TEXT("hover"));
            AddBoxMarker(CountryCenter, FVector2D(54.0f, 24.0f), CityColor, CityLabel, 8);
        }
        else if (bDetailedMiddleView)
        {
            AddBoxMarker(CountryCenter, FVector2D(12.0f, 12.0f), CityColor.CopyWithNewOpacity(0.62f), TEXT(""), 1);
        }

        if (!bDetailedMiddleView && !bPlayerCountry && !bSelectedCountry && !bHoveredCountry)
        {
            continue;
        }

        AddTerrainPatch(CountryCenter + FVector2D(-44.0f * RtsMapZoom, -34.0f * RtsMapZoom), FVector2D(68.0f, 16.0f), FLinearColor(0.08f, 0.24f, 0.10f, 0.56f), TEXT("terrain"));
        AddTerrainPatch(CountryCenter + FVector2D(48.0f * RtsMapZoom, 28.0f * RtsMapZoom), FVector2D(54.0f, 14.0f), FLinearColor(0.05f, 0.30f, 0.36f, 0.62f), TEXT("water"));

        int32 ResourceCount = 0;
        for (const FString& ProvinceId : Country.ProvinceIds)
        {
            const FDemocracyProvinceOwnershipState* Province = FindRtsProvinceById(State, ProvinceId);
            if (!Province)
            {
                continue;
            }

            FString ResourceLabel;
            FLinearColor ResourceColor;
            GetResourceStyle(Province->ResourceFocus, ResourceLabel, ResourceColor);
            const int32 OffsetIndex = ResourceCount % 6;
            FVector2D ResourceCenter = CountryCenter;
            if (Province->MapCenterX > 0.0f && Province->MapCenterY > 0.0f)
            {
                ResourceCenter = FVector2D(Province->MapCenterX * ScaleX, Province->MapCenterY * ScaleY);
            }
            ResourceCenter += FVector2D(((OffsetIndex % 3) - 1) * 26.0f * RtsMapZoom, (OffsetIndex / 3 == 0 ? -1.0f : 1.0f) * 24.0f * RtsMapZoom);
            AddBoxMarker(ResourceCenter, FVector2D(46.0f, 18.0f), ResourceColor, ResourceLabel, 7);

            ++ResourceCount;
            if (ResourceCount >= (bPlayerCountry || bSelectedCountry ? 6 : 3))
            {
                break;
            }
        }
    }

    return Overlay;
}
TSharedRef<SWidget> ALoginHUD::BuildRtsFogOverlaysWidget(float MapWidth, float MapHeight) const
{
    TSharedRef<SOverlay> Overlay = SNew(SOverlay);
    if (!bHasLoadedRuntimeState)
    {
        return Overlay;
    }

    const FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    const TArray<FDuliaCountryMapPoint>& Points = GetDuliaCountryMapPoints();
    const float ScaleX = MapWidth / 2242.0f;
    const float ScaleY = MapHeight / 1104.0f;

    for (const FDemocracyRtsFogProvinceState& FogProvince : State.RtsWorld.FogOfWar.Provinces)
    {
        const FDemocracyProvinceOwnershipState* Province = FindRtsProvinceById(State, FogProvince.ProvinceId);
        if (!Province)
        {
            continue;
        }
        const FDemocracyCountryOwnershipState* Country = nullptr;
        for (const FDemocracyCountryOwnershipState& CandidateCountry : State.RtsWorld.Ownership.Countries)
        {
            if (CandidateCountry.CountryName.Equals(Province->OriginalCountryName, ESearchCase::IgnoreCase) || CandidateCountry.CountryName.Equals(Province->CurrentControllerCountryName, ESearchCase::IgnoreCase))
            {
                Country = &CandidateCountry;
                break;
            }
        }
        if (!Country)
        {
            continue;
        }
        const FDuliaCountryMapPoint* Point = nullptr;
        for (const FDuliaCountryMapPoint& CandidatePoint : Points)
        {
            if (CandidatePoint.CountryIndex == Country->MapCountryIndex)
            {
                Point = &CandidatePoint;
                break;
            }
        }
        if (!Point)
        {
            continue;
        }

        FLinearColor FogColor = FLinearColor(0.02f, 0.02f, 0.025f, 0.52f);
        FString FogLabel = TEXT("Hidden");
        if (FogProvince.VisibilityState.Equals(TEXT("Known"), ESearchCase::IgnoreCase))
        {
            FogColor = FLinearColor(0.03f, 0.28f, 0.10f, 0.34f);
            FogLabel = TEXT("Known");
        }
        else if (FogProvince.VisibilityState.Equals(TEXT("Scouted"), ESearchCase::IgnoreCase))
        {
            FogColor = FLinearColor(0.06f, 0.32f, 0.72f, 0.42f);
            FogLabel = TEXT("Scouted");
        }
        else if (FogProvince.VisibilityState.Equals(TEXT("Contested"), ESearchCase::IgnoreCase) || FogProvince.bContested)
        {
            FogColor = FLinearColor(0.95f, 0.13f, 0.04f, 0.52f);
            FogLabel = TEXT("Contested");
        }

        const bool bHoveredFogProvince = FogProvince.ProvinceId.Equals(State.RtsWorld.WorldInteraction.HoveredTargetId, ESearchCase::IgnoreCase) || Country->CountryName.Equals(State.RtsWorld.WorldInteraction.HoveredTargetId, ESearchCase::IgnoreCase) || Country->CountryId.Equals(State.RtsWorld.WorldInteraction.HoveredTargetId, ESearchCase::IgnoreCase);
        if (!bHoveredFogProvince)
        {
            continue;
        }
        const float OffsetX = ((Province->ProvinceIndex % 3) - 1) * 22.0f * RtsMapZoom;
        const float OffsetY = ((Province->ProvinceIndex % 4) - 1) * 14.0f * RtsMapZoom;
        const float X = FMath::Clamp(Point->Centroid.X * ScaleX + OffsetX - 26.0f, 0.0f, FMath::Max(0.0f, MapWidth - 68.0f));
        const float Y = FMath::Clamp(Point->Centroid.Y * ScaleY + OffsetY + 24.0f, 0.0f, FMath::Max(0.0f, MapHeight - 28.0f));
        Overlay->AddSlot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(FMargin(X, Y, 0.0f, 0.0f))
        [
            SNew(SBorder)
            .BorderImage(RowBrush.Get())
            .BorderBackgroundColor(FogColor)
            .Padding(FMargin(4.0f, 1.0f))
            [
                SNew(STextBlock)
                .Text(BodyText(FString::Printf(TEXT("%s %d"), *FogLabel, FogProvince.ScoutStrength)))
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 7))
                .ColorAndOpacity(FLinearColor::White)
            ]
        ];
    }
    return Overlay;
}
TSharedRef<SWidget> ALoginHUD::BuildRtsArmyMarkersWidget(float MapWidth, float MapHeight)
{
    TSharedRef<SOverlay> MarkerOverlay = SNew(SOverlay);
    if (!bHasLoadedRuntimeState)
    {
        return MarkerOverlay;
    }

    const TArray<FDuliaCountryMapPoint>& Points = GetDuliaCountryMapPoints();
    const FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    const float ScaleX = MapWidth / 2242.0f;
    const float ScaleY = MapHeight / 1104.0f;

    auto FindProvince = [&State](const FString& ProvinceId) -> const FDemocracyProvinceOwnershipState*
    {
        for (const FDemocracyProvinceOwnershipState& CandidateProvince : State.RtsWorld.Ownership.Provinces)
        {
            if (CandidateProvince.ProvinceId.Equals(ProvinceId, ESearchCase::IgnoreCase))
            {
                return &CandidateProvince;
            }
        }
        return nullptr;
    };

    auto FindCountryForProvince = [&State](const FDemocracyProvinceOwnershipState& Province, const FString& ArmyCountryName) -> const FDemocracyCountryOwnershipState*
    {
        for (const FDemocracyCountryOwnershipState& CandidateCountry : State.RtsWorld.Ownership.Countries)
        {
            if (CandidateCountry.CountryName.Equals(Province.CurrentControllerCountryName, ESearchCase::IgnoreCase) ||
                CandidateCountry.CountryName.Equals(Province.OriginalCountryName, ESearchCase::IgnoreCase) ||
                CandidateCountry.CountryName.Equals(ArmyCountryName, ESearchCase::IgnoreCase))
            {
                return &CandidateCountry;
            }
        }
        return nullptr;
    };

    auto FindPointForCountry = [&Points](const FDemocracyCountryOwnershipState& Country) -> const FDuliaCountryMapPoint*
    {
        for (const FDuliaCountryMapPoint& CandidatePoint : Points)
        {
            if (CandidatePoint.CountryIndex == Country.MapCountryIndex)
            {
                return &CandidatePoint;
            }
        }
        return nullptr;
    };

    auto GetProvinceMapCenter = [&FindPointForCountry](const FDemocracyProvinceOwnershipState& Province, const FDemocracyCountryOwnershipState& Country) -> FVector2D
    {
        if (Province.MapCenterX > 0.0f && Province.MapCenterY > 0.0f)
        {
            return FVector2D(Province.MapCenterX, Province.MapCenterY);
        }
        if (const FDuliaCountryMapPoint* Point = FindPointForCountry(Country))
        {
            return Point->Centroid;
        }
        return FVector2D(-1.0f, -1.0f);
    };

    auto FindCenterForProvince = [&FindProvince, &FindCountryForProvince, &GetProvinceMapCenter](const FString& ProvinceId, const FString& ArmyCountryName) -> FVector2D
    {
        const FDemocracyProvinceOwnershipState* Province = FindProvince(ProvinceId);
        if (!Province)
        {
            return FVector2D(-1.0f, -1.0f);
        }
        const FDemocracyCountryOwnershipState* Country = FindCountryForProvince(*Province, ArmyCountryName);
        return Country ? GetProvinceMapCenter(*Province, *Country) : FVector2D(-1.0f, -1.0f);
    };
    auto AddPathSegment = [this, &MarkerOverlay](float X, float Y, float Width, float Height, const FLinearColor& Color)
    {
        if (Width <= 0.0f || Height <= 0.0f)
        {
            return;
        }

        MarkerOverlay->AddSlot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(FMargin(X, Y, 0.0f, 0.0f))
        [
            SNew(SBorder)
            .BorderImage(RowBrush.Get())
            .BorderBackgroundColor(Color)
            .Padding(0.0f)
            [
                SNew(SSpacer).Size(FVector2D(Width, Height))
            ]
        ];
    };

    auto AddUnitBadge = [this](const FString& Label, int32 Count, const FLinearColor& Color) -> TSharedRef<SWidget>
    {
        return SNew(SBorder)
            .BorderImage(RowBrush.Get())
            .BorderBackgroundColor(Color)
            .Padding(FMargin(4.0f, 1.0f))
            [
                SNew(STextBlock)
                .Text(BodyText(FString::Printf(TEXT("%s %d"), *Label, Count)))
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 7))
                .ColorAndOpacity(FLinearColor::White)
            ];
    };

    for (const FDemocracyRtsArmyGroupState& Army : State.RtsWorld.ArmyGroups)
    {
        const FDemocracyProvinceOwnershipState* Province = FindProvince(Army.CurrentProvinceId);
        if (!Province)
        {
            continue;
        }

        const FDemocracyCountryOwnershipState* Country = FindCountryForProvince(*Province, Army.CurrentCountryName);
        if (!Country)
        {
            continue;
        }

        const FVector2D ProvinceMapCenter = GetProvinceMapCenter(*Province, *Country);
        if (ProvinceMapCenter.X < 0.0f || ProvinceMapCenter.Y < 0.0f)
        {
            continue;
        }

        const bool bSelected = Army.ArmyId.Equals(RtsSelectedArmyId, ESearchCase::IgnoreCase) || Army.bSelected;
        const bool bHostile = Province->GovernmentType.Equals(TEXT("Dictatorship"), ESearchCase::IgnoreCase) && !Province->bPlayerControlled;
        const bool bMoving = Army.MovementTurnsRemaining > 0 || (!Army.DestinationProvinceId.IsEmpty() && !Army.DestinationProvinceId.Equals(Army.CurrentProvinceId, ESearchCase::IgnoreCase));
        const FLinearColor OwnerColor = bSelected ? FLinearColor(1.0f, 0.84f, 0.12f, 0.98f) : (Province->bPlayerControlled ? FLinearColor(0.03f, 0.75f, 0.22f, 0.92f) : (bHostile ? FLinearColor(0.85f, 0.06f, 0.06f, 0.90f) : FLinearColor(0.08f, 0.34f, 0.90f, 0.90f)));
        const FLinearColor SoldierBadgeColor = Province->bPlayerControlled ? FLinearColor(0.04f, 0.58f, 0.24f, 0.95f) : (bHostile ? FLinearColor(0.82f, 0.12f, 0.06f, 0.95f) : FLinearColor(0.08f, 0.34f, 0.88f, 0.95f));
        const FLinearColor VehicleBadgeColor = Province->bPlayerControlled ? FLinearColor(0.02f, 0.48f, 0.18f, 0.95f) : (bHostile ? FLinearColor(0.72f, 0.04f, 0.04f, 0.95f) : FLinearColor(0.06f, 0.25f, 0.80f, 0.95f));
        const FLinearColor LogisticsBadgeColor = Province->bPlayerControlled ? FLinearColor(0.02f, 0.44f, 0.40f, 0.95f) : (bHostile ? FLinearColor(0.72f, 0.14f, 0.06f, 0.95f) : FLinearColor(0.08f, 0.40f, 0.78f, 0.95f));
        const FLinearColor ScoutBadgeColor = Province->bPlayerControlled ? FLinearColor(0.10f, 0.56f, 0.28f, 0.95f) : (bHostile ? FLinearColor(0.90f, 0.22f, 0.08f, 0.95f) : FLinearColor(0.14f, 0.48f, 0.78f, 0.95f));
        const FLinearColor PathColor = bSelected ? FLinearColor(1.0f, 0.82f, 0.10f, 0.58f) : FLinearColor(0.78f, 0.88f, 1.0f, 0.36f);

        FVector2D ArmyCenter(ProvinceMapCenter.X * ScaleX, ProvinceMapCenter.Y * ScaleY);
        const FDemocracyRtsMovementOrderState* ActiveMovementOrder = nullptr;
        for (const FDemocracyRtsMovementOrderState& Order : State.RtsWorld.MovementOrders)
        {
            if (Order.bActive && !Order.bComplete && !Order.bCancelled && Order.ArmyId.Equals(Army.ArmyId, ESearchCase::IgnoreCase))
            {
                ActiveMovementOrder = &Order;
                break;
            }
        }
        if (ActiveMovementOrder && !ActiveMovementOrder->TargetProvinceId.IsEmpty() && ActiveMovementOrder->TotalTurns > 0)
        {
            const FVector2D DestinationMapCenterForProgress = FindCenterForProvince(ActiveMovementOrder->TargetProvinceId, Army.CurrentCountryName);
            if (DestinationMapCenterForProgress.X >= 0.0f && DestinationMapCenterForProgress.Y >= 0.0f)
            {
                const FVector2D DestinationCenter(DestinationMapCenterForProgress.X * ScaleX, DestinationMapCenterForProgress.Y * ScaleY);
                const float Progress = FMath::Clamp(1.0f - (static_cast<float>(ActiveMovementOrder->TurnsRemaining) / static_cast<float>(FMath::Max(1, ActiveMovementOrder->TotalTurns))), 0.10f, 0.90f);
                ArmyCenter = FMath::Lerp(ArmyCenter, DestinationCenter, Progress);
            }
        }
        const float MarkerX = FMath::Clamp(ArmyCenter.X - 58.0f, 0.0f, FMath::Max(0.0f, MapWidth - 170.0f));
        const float MarkerY = FMath::Clamp(ArmyCenter.Y - 34.0f, 0.0f, FMath::Max(0.0f, MapHeight - 102.0f));
        const FString StatusLine = FString::Printf(TEXT("Str %d | Ready %d%% | Morale %d%% | Supply %d%%%s"),
            Army.TotalStrength,
            FMath::Clamp(Army.SupplyStatus, 0, 100),
            FMath::Clamp(Army.Morale, 0, 100),
            FMath::Clamp(Army.SupplyStatus, 0, 100),
            Army.bSupplyRouteBroken ? TEXT(" BROKEN") : TEXT(""));
        const FString OrderLine = FString::Printf(TEXT("%s%s%s"),
            Army.ActiveOrderType.IsEmpty() ? TEXT("Idle") : *Army.ActiveOrderType,
            Army.MovementState.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" | %s"), *Army.MovementState),
            Army.MovementTurnsRemaining > 0 ? *FString::Printf(TEXT(" | ETA %d"), Army.MovementTurnsRemaining) : TEXT(""));

        if (bMoving)
        {
            const FString DestinationId = Army.DestinationProvinceId.IsEmpty() ? Army.OrderTargetProvinceId : Army.DestinationProvinceId;
            const FVector2D DestinationMapCenter = FindCenterForProvince(DestinationId, Army.CurrentCountryName);
            if (DestinationMapCenter.X >= 0.0f && DestinationMapCenter.Y >= 0.0f)
            {
                const FVector2D DestCenter(DestinationMapCenter.X * ScaleX, DestinationMapCenter.Y * ScaleY);
                const float PathThickness = bSelected ? 5.0f : 3.0f;
                const float CornerX = DestCenter.X;
                AddPathSegment(FMath::Min(ArmyCenter.X, CornerX), ArmyCenter.Y - PathThickness * 0.5f, FMath::Abs(CornerX - ArmyCenter.X), PathThickness, PathColor);
                AddPathSegment(CornerX - PathThickness * 0.5f, FMath::Min(ArmyCenter.Y, DestCenter.Y), PathThickness, FMath::Abs(DestCenter.Y - ArmyCenter.Y), PathColor);

                const float DestX = FMath::Clamp(DestCenter.X - 32.0f, 0.0f, FMath::Max(0.0f, MapWidth - 118.0f));
                const float DestY = FMath::Clamp(DestCenter.Y + 18.0f, 0.0f, FMath::Max(0.0f, MapHeight - 45.0f));
                MarkerOverlay->AddSlot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(FMargin(DestX, DestY, 0.0f, 0.0f))
                [
                    SNew(SBorder)
                    .BorderImage(RowBrush.Get())
                    .BorderBackgroundColor(FLinearColor(0.96f, 0.90f, 0.16f, 0.88f))
                    .Padding(FMargin(6.0f, 3.0f))
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight()
                        [
                            SNew(STextBlock)
                            .Text(BodyText(TEXT("DESTINATION")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 7))
                            .ColorAndOpacity(FLinearColor::Black)
                        ]
                        + SVerticalBox::Slot().AutoHeight()
                        [
                            SNew(STextBlock)
                            .Text(BodyText(FString::Printf(TEXT("%s | ETA %d"), *DestinationId, FMath::Max(Army.MovementTurnsRemaining, Army.OrderTurnsRemaining))))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                            .ColorAndOpacity(FLinearColor::Black)
                        ]
                    ]
                ];
            }
        }

        MarkerOverlay->AddSlot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(FMargin(MarkerX, MarkerY, 0.0f, 0.0f))
        [
            SNew(SBorder)
            .BorderImage(RowBrush.Get())
            .BorderBackgroundColor(OwnerColor)
            .Padding(FMargin(7.0f, 5.0f))
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 3.0f)
                [
                    SNew(STextBlock)
                    .Text(BodyText(FString::Printf(TEXT("%s%s"), bSelected ? TEXT("> ") : TEXT(""), *Army.DisplayName)))
                    .AutoWrapText(true)
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                    .ColorAndOpacity(bSelected ? FLinearColor::Black : FLinearColor::White)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 3.0f)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 3.0f, 0.0f)[AddUnitBadge(TEXT("INF"), Army.InfantryCount, SoldierBadgeColor)]
                    + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 3.0f, 0.0f)[AddUnitBadge(TEXT("TNK"), Army.VehicleCount, VehicleBadgeColor)]
                    + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 3.0f, 0.0f)[AddUnitBadge(TEXT("SUP"), Army.LogisticsCount, LogisticsBadgeColor)]
                    + SHorizontalBox::Slot().AutoWidth()[AddUnitBadge(TEXT("SCT"), Army.ScoutCount, ScoutBadgeColor)]
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(STextBlock)
                    .Text(BodyText(StatusLine))
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                    .ColorAndOpacity(bSelected ? FLinearColor::Black : FLinearColor::White)
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(STextBlock)
                    .Text(BodyText(OrderLine))
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                    .ColorAndOpacity(bSelected ? FLinearColor::Black : FLinearColor::White)
                ]
            ]
        ];
    }

    return MarkerOverlay;
}
TSharedRef<SWidget> ALoginHUD::BuildRtsOrderButtonsWidget()
{
    const bool bCanIssueOrder = bHasLoadedRuntimeState && !RtsSelectedArmyId.IsEmpty();
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
        [BuildButton(TEXT("Move"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSelectRtsOrder, FString(TEXT("Move"))), 78.0f, 32.0f, bCanIssueOrder)]
        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
        [BuildButton(TEXT("Defend"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSelectRtsOrder, FString(TEXT("Defend"))), 86.0f, 32.0f, bCanIssueOrder)]
        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
        [BuildButton(TEXT("Rally"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSelectRtsOrder, FString(TEXT("Rally"))), 78.0f, 32.0f, bCanIssueOrder)]
        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
        [BuildButton(TEXT("Scout"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSelectRtsOrder, FString(TEXT("Patrol/Scout"))), 78.0f, 32.0f, bCanIssueOrder)]
        + SHorizontalBox::Slot().AutoWidth()
        [BuildButton(TEXT("Reinforce"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSelectRtsOrder, FString(TEXT("Reinforce"))), 108.0f, 32.0f, bCanIssueOrder)];
}

FReply ALoginHUD::HandleSelectRtsOrder(FString OrderType)
{
    if (!bHasLoadedRuntimeState || RtsSelectedArmyId.IsEmpty())
    {
        return FReply::Handled();
    }

    PendingRtsOrderType = OrderType;
    LoadedSaveState.RuntimeState.RtsWorld.WorldInteraction.LastInteractionSummary = FString::Printf(TEXT("%s order selected. Click a destination province or country."), *PendingRtsOrderType);
    RefreshRtsHudState(LoadedSaveState.RuntimeState);
    RefreshLoginWidget();
    return FReply::Handled();
}


FReply ALoginHUD::HandleConfirmRtsOrderClicked()
{
    if (!bHasLoadedRuntimeState || PendingRtsOrderTargetProvinceId.IsEmpty())
    {
        return FReply::Handled();
    }

    const FString TargetProvinceId = PendingRtsOrderTargetProvinceId;
    TryIssueRtsOrderToProvince(TargetProvinceId);
    RefreshLoginWidget();
    return FReply::Handled();
}

FReply ALoginHUD::HandleCancelRtsOrderConfirmationClicked()
{
    PendingRtsOrderTargetProvinceId.Empty();
    PendingRtsOrderConfirmationText.Empty();
    if (bHasLoadedRuntimeState)
    {
        LoadedSaveState.RuntimeState.RtsWorld.WorldInteraction.LastInteractionSummary = PendingRtsOrderType.IsEmpty() ? TEXT("RTS command confirmation cancelled.") : FString::Printf(TEXT("%s target cancelled. Choose another destination or select a different order."), *PendingRtsOrderType);
        RefreshRtsHudState(LoadedSaveState.RuntimeState);
    }
    RefreshLoginWidget();
    return FReply::Handled();
}
bool ALoginHUD::TryIssueRtsOrderToProvince(const FString& TargetProvinceId)
{
    if (!bHasLoadedRuntimeState || PendingRtsOrderType.IsEmpty() || RtsSelectedArmyId.IsEmpty() || TargetProvinceId.IsEmpty())
    {
        return false;
    }

    FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    FDemocracyRtsArmyGroupState* Army = nullptr;
    for (FDemocracyRtsArmyGroupState& CandidateArmy : State.RtsWorld.ArmyGroups)
    {
        if (CandidateArmy.ArmyId.Equals(RtsSelectedArmyId, ESearchCase::IgnoreCase))
        {
            Army = &CandidateArmy;
            break;
        }
    }
    if (!Army)
    {
        PendingRtsOrderType.Empty();
        return false;
    }

    const FDemocracyProvinceOwnershipState* TargetProvince = FindRtsProvinceById(State, TargetProvinceId);
    if (!TargetProvince)
    {
        return false;
    }

    const FString AuthorityBlockReason = GetRtsOrderAuthorityBlockReason(State, *Army, *TargetProvince, PendingRtsOrderType);
    if (!AuthorityBlockReason.IsEmpty())
    {
        State.RtsWorld.WorldInteraction.LastInteractionSummary = FString::Printf(TEXT("RTS order blocked: %s"), *AuthorityBlockReason);
        Army->MovementState = TEXT("Path Blocked");
        Army->DestinationProvinceId = Army->CurrentProvinceId;
        Army->OrderTurnsRemaining = 0;
        Army->MovementTurnsRemaining = 0;
        Army->Orders.Add(State.RtsWorld.WorldInteraction.LastInteractionSummary);
        if (Army->Orders.Num() > 8)
        {
            Army->Orders.RemoveAt(0, Army->Orders.Num() - 8);
        }
        AddRtsHudAlert(State.RtsWorld, State.RtsWorld.WorldInteraction.LastInteractionSummary);
        PendingRtsOrderConfirmationText = State.RtsWorld.WorldInteraction.LastInteractionSummary;
        RefreshRtsHudState(State);
        return false;
    }

    FString MovementCostSummary;
    if (!TryApplyRtsActionCost(State, PendingRtsOrderType, MovementCostSummary))
    {
        State.RtsWorld.WorldInteraction.LastInteractionSummary = FString::Printf(TEXT("RTS order blocked: %s cannot start because %s is unavailable."), *PendingRtsOrderType, *MovementCostSummary);
        Army->MovementState = TEXT("Insufficient Resources");
        Army->DestinationProvinceId = Army->CurrentProvinceId;
        Army->OrderTurnsRemaining = 0;
        Army->MovementTurnsRemaining = 0;
        AddRtsHudAlert(State.RtsWorld, State.RtsWorld.WorldInteraction.LastInteractionSummary);
        RecordRtsOfficeAlert(State, TEXT("RTS Order Blocked"), State.RtsWorld.WorldInteraction.LastInteractionSummary, TargetProvinceId, 34);
        PendingRtsOrderConfirmationText = State.RtsWorld.WorldInteraction.LastInteractionSummary;
        RefreshRtsHudState(State);
        return false;
    }
    for (FDemocracyRtsMovementOrderState& ExistingOrder : State.RtsWorld.MovementOrders)
    {
        if (ExistingOrder.ArmyId.Equals(Army->ArmyId, ESearchCase::IgnoreCase) && ExistingOrder.bActive && !ExistingOrder.bComplete)
        {
            ExistingOrder.bCancelled = true;
            ExistingOrder.bActive = false;
            ExistingOrder.StatusSummary = FString::Printf(TEXT("Cancelled by new %s order."), *PendingRtsOrderType);
        }
    }

    const bool bSameProvince = Army->CurrentProvinceId.Equals(TargetProvinceId, ESearchCase::IgnoreCase);
    const bool bHostileTarget = !TargetProvince->CurrentControllerCountryName.Equals(State.PlayerCountry.CountryName, ESearchCase::IgnoreCase) && !IsRtsProvinceAlliedOrTreatyControlled(State, *TargetProvince);
    const int32 DiplomacySupplyModifier = GetRtsDiplomacySupplyModifier(State, *TargetProvince);
    FDemocracyRtsMovementOrderState Order;
    Order.OrderId = FString::Printf(TEXT("RTS-ORDER-%d-%d"), State.Turn, State.RtsWorld.MovementOrders.Num() + 1);
    Order.ArmyId = Army->ArmyId;
    Order.OrderType = PendingRtsOrderType;
    Order.SourceProvinceId = Army->CurrentProvinceId;
    Order.TargetProvinceId = TargetProvinceId;
    Order.RallyPointId = PendingRtsOrderType.Equals(TEXT("Rally"), ESearchCase::IgnoreCase) ? TargetProvinceId : TEXT("");
    Order.IssuedTurn = State.Turn;
    Order.TotalTurns = FMath::Clamp((bSameProvince ? 1 : 2) + (Army->bSupplyRouteBroken ? 1 : 0) + (bHostileTarget ? 1 : 0) + FMath::Max(0, DiplomacySupplyModifier) / 12, 1, 6);
    Order.TurnsRemaining = Order.TotalTurns;
    Order.StatusSummary = FString::Printf(TEXT("%s ordered to %s. ETA %d turn(s)%s. %s paid."), *Army->DisplayName, *TargetProvince->ProvinceName, Order.TurnsRemaining, bHostileTarget ? TEXT("; hostile contact possible") : TEXT(""), *MovementCostSummary);
    Order.AllowedFollowUps = { TEXT("Cancel"), TEXT("Change Order"), TEXT("Inspect Target") };
    State.RtsWorld.MovementOrders.Add(Order);

    Army->DestinationProvinceId = TargetProvinceId;
    Army->ActiveOrderType = PendingRtsOrderType;
    Army->OrderTargetProvinceId = TargetProvinceId;
    Army->OrderTargetType = TEXT("Province");
    Army->OrderTurnsRemaining = Order.TurnsRemaining;
    Army->MovementTurnsRemaining = Order.TurnsRemaining;
    Army->MovementState = FString::Printf(TEXT("%s Ordered"), *PendingRtsOrderType);
    Army->Orders.Add(Order.StatusSummary);
    if (Army->Orders.Num() > 8)
    {
        Army->Orders.RemoveAt(0, Army->Orders.Num() - 8);
    }

    bool bUpdatedRoute = false;
    for (FDemocracyRtsSupplyRouteState& Route : State.RtsWorld.SupplyRoutes)
    {
        if (Route.ArmyId.Equals(Army->ArmyId, ESearchCase::IgnoreCase))
        {
            Route.SourceProvinceId = Order.SourceProvinceId;
            Route.DestinationProvinceId = TargetProvinceId;
            Route.Disruption = FMath::Clamp(Route.Disruption + DiplomacySupplyModifier, 0, 100);
            Route.SupplyStatus = FMath::Clamp(100 - Route.Disruption, 0, 100);
            Route.bBroken = Route.SupplyStatus < 35;
            if (DiplomacySupplyModifier > 0) { Route.Risks.AddUnique(TEXT("sanctions or border tension supply penalty")); }
            if (DiplomacySupplyModifier < 0) { Route.Risks.AddUnique(TEXT("alliance/treaty supply support")); }
            Route.StatusSummary = FString::Printf(TEXT("Supply route recalculated for %s order. Diplomacy supply modifier %+d, supply %d%%."), *PendingRtsOrderType, DiplomacySupplyModifier, Route.SupplyStatus);
            bUpdatedRoute = true;
            break;
        }
    }
    if (!bUpdatedRoute)
    {
        FDemocracyRtsSupplyRouteState Route;
        Route.RouteId = FString::Printf(TEXT("SUPPLY-%s"), *Army->ArmyId);
        Route.ArmyId = Army->ArmyId;
        Route.SourceProvinceId = Order.SourceProvinceId;
        Route.DestinationProvinceId = TargetProvinceId;
        Route.Disruption = FMath::Clamp(DiplomacySupplyModifier, 0, 100);
        Route.SupplyStatus = FMath::Clamp(100 - Route.Disruption, 0, 100);
        Route.bBroken = Route.SupplyStatus < 35;
        if (DiplomacySupplyModifier > 0) { Route.Risks.AddUnique(TEXT("sanctions or border tension supply penalty")); }
        if (DiplomacySupplyModifier < 0) { Route.Risks.AddUnique(TEXT("alliance/treaty supply support")); }
        Route.StatusSummary = FString::Printf(TEXT("Supply route created for new movement order. Diplomacy supply modifier %+d, supply %d%%."), DiplomacySupplyModifier, Route.SupplyStatus);
        State.RtsWorld.SupplyRoutes.Add(Route);
    }

    State.RtsWorld.WorldInteraction.ActiveSelectionId = TargetProvinceId;
    State.RtsWorld.WorldInteraction.ActiveSelectionType = TEXT("Order Destination");
    State.RtsWorld.WorldInteraction.LastInteractionSummary = Order.StatusSummary;
    PendingRtsOrderType.Empty();
    RefreshRtsHudState(State);
    return true;
}
void ALoginHUD::SelectRtsMapAtViewportPosition(const FGeometry& Geometry, const FVector2D& ScreenPosition)
{
    if (!bHasLoadedRuntimeState)
    {
        return;
    }

    FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    const FVector2D ViewCenter = Geometry.GetLocalSize() * 0.5f;
    const FVector2D CursorLocal = Geometry.AbsoluteToLocal(ScreenPosition);
    const FVector2D MapPoint = (CursorLocal - ViewCenter - RtsMapPan) / FMath::Max(RtsMapZoom, 0.01f) + FVector2D(1121.0f, 552.0f);
    if (MapPoint.X < 0.0f || MapPoint.Y < 0.0f || MapPoint.X > 2242.0f || MapPoint.Y > 1104.0f)
    {
        return;
    }

    const int32 SelectedCountryIndex = SampleDuliaCountryIdAtMapPoint(MapPoint);
    const FDemocracyCountryOwnershipState* BestCountry = nullptr;
    if (SelectedCountryIndex > 0)
    {
        for (const FDemocracyCountryOwnershipState& Country : State.RtsWorld.Ownership.Countries)
        {
            if (Country.MapCountryIndex == SelectedCountryIndex)
            {
                BestCountry = &Country;
                break;
            }
        }
    }

    if (!BestCountry)
    {
        State.RtsWorld.WorldInteraction.LastInteractionSummary = SelectedCountryIndex > 0
            ? FString::Printf(TEXT("Clicked Dulia country id %03d, but no runtime country ownership record matched it."), SelectedCountryIndex)
            : TEXT("Clicked ocean or empty map space. No country/province footprint selected.");
        RefreshRtsHudState(State);
        RefreshLoginWidget();
        return;
    }
    EnsureRuntimeProvinceGeometry(State.RtsWorld.Ownership);
    bool bExactProvinceHit = false;
    const FString TargetProvinceId = SelectDuliaProvinceIdAtMapPoint(State.RtsWorld.Ownership, *BestCountry, MapPoint, bExactProvinceHit);
    if (!PendingRtsOrderType.IsEmpty() && !RtsSelectedArmyId.IsEmpty() && !TargetProvinceId.IsEmpty())
    {
        RtsSelectedCountryName = BestCountry->CountryName;
        RtsSelectedProvinceId = TargetProvinceId;
        TryIssueRtsOrderToProvince(TargetProvinceId);
        RefreshLoginWidget();
        return;
    }

    RtsSelectedCountryName = BestCountry->CountryName;
    RtsSelectedProvinceId = TargetProvinceId;
    RtsSelectedArmyId.Empty();

    for (FDemocracyRtsArmyGroupState& Army : State.RtsWorld.ArmyGroups)
    {
        Army.bSelected = !RtsSelectedProvinceId.IsEmpty() && Army.CurrentProvinceId.Equals(RtsSelectedProvinceId, ESearchCase::IgnoreCase);
        if (Army.bSelected && RtsSelectedArmyId.IsEmpty())
        {
            RtsSelectedArmyId = Army.ArmyId;
        }
    }

    for (FDemocracyRtsSelectableTargetState& Target : State.RtsWorld.WorldInteraction.SelectableTargets)
    {
        Target.bSelected = Target.CountryName.Equals(RtsSelectedCountryName, ESearchCase::IgnoreCase) || (!RtsSelectedProvinceId.IsEmpty() && Target.ProvinceId.Equals(RtsSelectedProvinceId, ESearchCase::IgnoreCase));
    }

    State.RtsWorld.WorldInteraction.ActiveSelectionId = RtsSelectedProvinceId.IsEmpty() ? BestCountry->CountryId : RtsSelectedProvinceId;
    State.RtsWorld.WorldInteraction.ActiveSelectionType = RtsSelectedProvinceId.IsEmpty() ? TEXT("Country") : TEXT("Province");

    const bool bSelectedPlayerCountry = BestCountry->bPlayerCountry || BestCountry->CountryName.Equals(State.PlayerCountry.CountryName, ESearchCase::IgnoreCase);
    if (bSelectedPlayerCountry && PendingRtsOrderType.IsEmpty())
    {
        State.RtsWorld.WorldInteraction.LastInteractionSummary = FString::Printf(TEXT("Opened country/base command view for %s."), *RtsSelectedCountryName);
        State.RtsWorld.ActiveViewMode = TEXT("city_base");
        RtsMapZoom = 5.25f;
        RtsMapPan = FVector2D::ZeroVector;
        RefreshRtsHudState(State);
        RefreshLoginWidget();
        return;
    }

    State.RtsWorld.WorldInteraction.LastInteractionSummary = FString::Printf(TEXT("Selected %s in %s%s."), *RtsSelectedCountryName, *GetRtsZoomModeLabel(), bExactProvinceHit ? TEXT(" by province footprint") : TEXT(" by nearest province fallback"));
    State.RtsWorld.ActiveViewMode = GetRtsZoomModeLabel();
    RefreshRtsHudState(State);
    RefreshLoginWidget();
}
TSharedRef<SWidget> ALoginHUD::BuildOfficeWorldRtsScreen()
{
    if (bHasLoadedRuntimeState)
    {
        RefreshWarConflictState(LoadedSaveState.RuntimeState);
        RefreshSimulationToRtsContract(LoadedSaveState.RuntimeState);
        RefreshRtsHudState(LoadedSaveState.RuntimeState);
    }

    const bool bRtsEntry = WorldRtsEntryMode.Equals(TEXT("RtsEntry"), ESearchCase::IgnoreCase);
    const FString PlayerCountry = bHasLoadedRuntimeState ? LoadedSaveState.RuntimeState.PlayerCountry.CountryName : TEXT("Player Country");
    const FString DemocracyCount = bHasLoadedRuntimeState ? FString::Printf(TEXT("%d democratic/allied countries"), LoadedSaveState.RuntimeState.WorldMap.DemocraticAllyCount) : TEXT("Democracy count unavailable");
    const FString DictatorshipCount = bHasLoadedRuntimeState ? FString::Printf(TEXT("%d dictatorship/hostile countries"), LoadedSaveState.RuntimeState.WorldMap.NonDemocraticCountryCount) : TEXT("Dictatorship count unavailable");
    const FString OwnershipSummary = bHasLoadedRuntimeState ? BuildMapOwnershipStatusText() : TEXT("Map ownership unavailable until a state is loaded.");

    TSharedRef<SWidget> MapImage =
        SNew(SImage)
        .Image(WorldMapBrush.Get());

    if (bRtsEntry)
    {
        if (bHasLoadedRuntimeState)
        {
            LoadedSaveState.RuntimeState.RtsWorld.ActiveViewMode = GetRtsZoomModeLabel();
            FString RuntimeRtsMapPath;
            if (EnsureDuliaRuntimeGovernmentMapImage(LoadedSaveState.RuntimeState, RuntimeRtsMapPath))
            {
                RtsLandMapBrush = MakeShared<FSlateDynamicImageBrush>(FName(*RuntimeRtsMapPath), FVector2D(2242.0f, 1104.0f));
            }
        }

        const float MapWidth = 2242.0f * RtsMapZoom;
        const float MapHeight = 1104.0f * RtsMapZoom;
        const FString CompactStatus = FString::Printf(TEXT("%s | %s | Zoom %.0f%%"),
            *GetRtsZoomModeLabel(),
            bHasLoadedRuntimeState ? *LoadedSaveState.RuntimeState.RtsWorld.Hud.ResourceSummary : TEXT("No RTS resources loaded."),
            RtsMapZoom * 100.0f);

        if (RtsMapZoom >= 4.5f && bHasLoadedRuntimeState)
        {
            LoadedSaveState.RuntimeState.RtsWorld.ActiveViewMode = TEXT("city_base");
            return BuildRtsCityBasePlaceholderWidget();
        }
        if (RtsMapZoom >= 3.0f && bHasLoadedRuntimeState)
        {
            LoadedSaveState.RuntimeState.RtsWorld.ActiveViewMode = TEXT("Regional Country View");
            return BuildRtsRegionalCountryViewWidget();
        }

        return SNew(SOverlay)
            + SOverlay::Slot()
            [
                SNew(SBorder)
                .BorderImage(RowBrush.Get())
                .BorderBackgroundColor(FLinearColor(0.274f, 0.612f, 0.690f, 1.0f))
                .Padding(0.0f)
            ]
            + SOverlay::Slot()
            [
                SNew(SRtsMapInputSurface)
                .OnMapMouseWheel(FPointerEventHandler::CreateUObject(this, &ALoginHUD::HandleRtsMapMouseWheel))
                .OnMapMouseButtonDown(FPointerEventHandler::CreateUObject(this, &ALoginHUD::HandleRtsMapMouseButtonDown))
                .OnMapMouseButtonUp(FPointerEventHandler::CreateUObject(this, &ALoginHUD::HandleRtsMapMouseButtonUp))
                .OnMapMouseMove(FPointerEventHandler::CreateUObject(this, &ALoginHUD::HandleRtsMapMouseMove))
                [
                    SNew(SBorder)
                    .BorderImage(RtsWaterBrush.Get())
                    .Clipping(EWidgetClipping::ClipToBounds)
                    [
                        SNew(SOverlay)
                        + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(FMargin(RtsMapPan.X, RtsMapPan.Y, 0.0f, 0.0f))
                        [
                            SNew(SBox)
                            .WidthOverride(MapWidth)
                            .HeightOverride(MapHeight)
                            [
                                SNew(SOverlay)
                                + SOverlay::Slot()
                                [
                                    SNew(SImage)
                                    .Image(RtsLandMapBrush.IsValid() ? RtsLandMapBrush.Get() : WorldMapBrush.Get())
                                ]
                                + SOverlay::Slot()
                                [
                                    BuildRtsProvinceStateOverlaysWidget(MapWidth, MapHeight)
                                ]
                                + SOverlay::Slot()
                                [
                                    BuildRtsCountryOperationsOverlayWidget(MapWidth, MapHeight)
                                ]
                                + SOverlay::Slot()
                                [
                                    BuildRtsFogOverlaysWidget(MapWidth, MapHeight)
                                ]
                                + SOverlay::Slot()
                                [
                                    BuildRtsArmyMarkersWidget(MapWidth, MapHeight)
                                ]
                            ]
                        ]
                    ]
                ]
            ]
            + SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(18.0f)
            [
                SNew(SBorder)
                .BorderImage(RowBrush.Get())
                .BorderBackgroundColor(FLinearColor(0.015f, 0.018f, 0.022f, 0.84f))
                .Padding(12.0f)
                [
                    SNew(SBox)
                    .WidthOverride(720.0f)
                    [
                        SNew(STextBlock)
                        .Text(BodyText(FString::Printf(TEXT("RTS Command Map | %s\nClick country/province to inspect | Drag to pan | Mouse wheel zooms | Esc returns"), *CompactStatus)))
                        .AutoWrapText(true)
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
                        .ColorAndOpacity(FLinearColor::White)
                    ]
                ]
            ]
            + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(18.0f)
            [
                SNew(SBorder)
                .BorderImage(RowBrush.Get())
                .BorderBackgroundColor(FLinearColor(0.015f, 0.018f, 0.022f, 0.86f))
                .Padding(8.0f)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
                    [BuildButton(TEXT("-"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleZoomRtsMapOutClicked), 42.0f, 34.0f)]
                    + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
                    [BuildButton(TEXT("Reset"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleResetRtsMapViewClicked), 88.0f, 34.0f)]
                    + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
                    [BuildButton(TEXT("Focus"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleFocusRtsMapSelectionClicked), 78.0f, 34.0f)]
                    + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
                    [BuildButton(TEXT("Player"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleFocusRtsMapPlayerClicked), 82.0f, 34.0f)]
                    + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 10.0f, 0.0f)
                    [BuildButton(TEXT("+"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleZoomRtsMapInClicked), 42.0f, 34.0f)]
                    + SHorizontalBox::Slot().AutoWidth()
                    [BuildButton(TEXT("Return To Office"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleCloseOfficeOverlayClicked), 172.0f, 34.0f)]
                ]
            ]
            + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Center).Padding(18.0f, 82.0f, 18.0f, 18.0f)
            [
                SNew(SBorder)
                .BorderImage(RowBrush.Get())
                .BorderBackgroundColor(FLinearColor(0.02f, 0.025f, 0.03f, 0.84f))
                .Padding(12.0f)
                [
                    SNew(SBox)
                    .WidthOverride(450.0f)
                    .MaxDesiredHeight(620.0f)
                    [
                        SNew(SScrollBox)
                        + SScrollBox::Slot()
                        [
                            SNew(SVerticalBox)
                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
                            [
                                SNew(STextBlock)
                                .Text(BodyText(TEXT("Selected Territory")))
                                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
                                .ColorAndOpacity(FLinearColor(0.86f, 0.95f, 1.0f, 1.0f))
                            ]
                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 14.0f)
                            [
                                SNew(STextBlock)
                                .Text(BodyText(BuildRtsSelectedTerritoryText()))
                                .AutoWrapText(true)
                                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
                                .ColorAndOpacity(FLinearColor::White)
                            ]
                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
                            [
                                SNew(STextBlock)
                                .Text(BodyText(TEXT("Selected Army")))
                                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
                                .ColorAndOpacity(FLinearColor(0.86f, 0.95f, 1.0f, 1.0f))
                            ]
                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
                            [
                                SNew(STextBlock)
                                .Text(BodyText(BuildRtsSelectedArmyText()))
                                .AutoWrapText(true)
                                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
                                .ColorAndOpacity(FLinearColor::White)
                            ]
                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
                            [
                                SNew(STextBlock)
                                .Text(BodyText(PendingRtsOrderType.IsEmpty() ? FString(TEXT("Choose Order")) : FString::Printf(TEXT("%s selected: click destination"), *PendingRtsOrderType)))
                                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
                                .ColorAndOpacity(PendingRtsOrderType.IsEmpty() ? FLinearColor(0.86f, 0.95f, 1.0f, 1.0f) : FLinearColor(1.0f, 0.86f, 0.16f, 1.0f))
                            ]
                            + SVerticalBox::Slot().AutoHeight()
                            [
                                BuildRtsOrderButtonsWidget()
                            ]
                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 8.0f)
                            [
                                SNew(STextBlock)
                                .Text(BodyText(TEXT("Battle Report")))
                                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
                                .ColorAndOpacity(FLinearColor(1.0f, 0.82f, 0.36f, 1.0f))
                            ]
                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
                            [
                                BuildRtsBattlePresentationWidget()
                            ]
                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 8.0f)
                            [
                                SNew(STextBlock)
                                .Text(BodyText(TEXT("Occupation Actions")))
                                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
                                .ColorAndOpacity(FLinearColor(0.86f, 0.95f, 1.0f, 1.0f))
                            ]
                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
                            [
                                BuildRtsOccupationActionsWidget()
                            ]
                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 8.0f)
                            [
                                SNew(STextBlock)
                                .Text(BodyText(TEXT("Supply Routes")))
                                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
                                .ColorAndOpacity(FLinearColor(0.86f, 0.95f, 1.0f, 1.0f))
                            ]
                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
                            [
                                BuildRtsSupplyActionsWidget()
                            ]
                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 8.0f)
                            [
                                SNew(STextBlock)
                                .Text(BodyText(TEXT("RTS Diplomacy")))
                                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
                                .ColorAndOpacity(FLinearColor(0.86f, 0.95f, 1.0f, 1.0f))
                            ]
                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
                            [
                                BuildRtsDiplomacyActionsWidget()
                            ]
                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 8.0f)
                            [
                                SNew(STextBlock)
                                .Text(BodyText(TEXT("RTS Notifications")))
                                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
                                .ColorAndOpacity(FLinearColor(1.0f, 0.82f, 0.36f, 1.0f))
                            ]
                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
                            [
                                BuildRtsNotificationsWidget()
                            ]
                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 8.0f)
                            [
                                SNew(STextBlock)
                                .Text(BodyText(TEXT("RTS Status")))
                                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
                                .ColorAndOpacity(FLinearColor(0.86f, 0.95f, 1.0f, 1.0f))
                            ]
                            + SVerticalBox::Slot().AutoHeight()
                            [
                                SNew(STextBlock)
                                .Text(BodyText(BuildRtsActionText()))
                                .AutoWrapText(true)
                                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
                                .ColorAndOpacity(FLinearColor(0.82f, 0.94f, 0.86f, 1.0f))
                            ]
                        ]
                    ]
                ]
            ];
    }

    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
    Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
    [
        SNew(SBox)
        .WidthOverride(1080.0f)
        .HeightOverride(532.0f)
        [
            SNew(SBorder)
            .BorderImage(RowBrush.Get())
            .Padding(4.0f)
            [MapImage]
        ]
    ];

    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildInfoRow(TEXT("Legend"), FString::Printf(TEXT("Green: %s | Blue: Democracy | Red: Dictatorship | Water: Ocean/neutral background"), *PlayerCountry))];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildInfoRow(TEXT("Government Spread"), FString::Printf(TEXT("%s | %s | %d total countries"), *DemocracyCount, *DictatorshipCount, bHasLoadedRuntimeState ? LoadedSaveState.RuntimeState.WorldMap.TotalCountryCount : 195))];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildInfoRow(TEXT("Map Ownership"), OwnershipSummary)];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildInfoRow(TEXT("Diplomacy Matrix"), BuildDiplomacyStatusText())];
    Body->AddSlot().AutoHeight().Padding(0.0f, 12.0f)
    [BuildButton(TEXT("Return To Office"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleCloseOfficeOverlayClicked), 210.0f, 40.0f)];

    return BuildPanel(TEXT("World Government Map"), TEXT("Globe view of Planet Dulia. Green is the player country, blue countries are democracies, and red countries are dictatorships."), Body, 1160.0f);
}
TSharedRef<SWidget> ALoginHUD::BuildOfficeAdvisorWarningsScreen()
{
    const FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    return BuildPanel(TEXT("Secure Phone"), TEXT("Prototype advisor warning feed from the loaded runtime state."),
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Difficulty Guidance"), bHasLoadedRuntimeState ? RuntimeGuidanceSummary(State) : TEXT("No runtime state loaded."))]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Advisor Warnings"), BuildAdvisorWarningText())]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Failure Warnings"), bHasLoadedRuntimeState ? BuildGuidedFailureWarningText(State) : TEXT("No runtime state loaded."))]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Simulation"), BuildSimulationStatusText())]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f)
        [BuildInfoRow(TEXT("Advisor Actions"), TEXT("Refresh reports or request emergency guidance. Results update the runtime state, warnings, reports, and decision history immediately."))]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [BuildButton(TEXT("Refresh Reports"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleApplyAdvisorAction, FString(TEXT("Refresh Reports"))), 180.0f, 38.0f, bHasLoadedRuntimeState)]
            + SHorizontalBox::Slot().AutoWidth()
            [BuildButton(TEXT("Emergency Guidance"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleApplyAdvisorAction, FString(TEXT("Emergency Guidance"))), 210.0f, 38.0f, bHasLoadedRuntimeState)]
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f)
        [BuildButton(TEXT("Close"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleCloseOfficeOverlayClicked), 160.0f, 40.0f)], 760.0f);
}

TSharedRef<SWidget> ALoginHUD::BuildOfficeMeetingAdvisorScreen()
{
    if (bHasLoadedRuntimeState)
    {
        InitializeMeetingSystemIfMissing(LoadedSaveState.RuntimeState);
        LoadedSaveState.RuntimeState.AdvisorSystem.GuidanceLevel = AdvisorGuidanceForDifficultyScore(LoadedSaveState.RuntimeState.PlayerCountry.CountrySizeScore);
        LoadedSaveState.RuntimeState.AdvisorSystem.LastUpdatedTurn = LoadedSaveState.RuntimeState.Turn;
        if (LoadedSaveState.RuntimeState.AdvisorSystem.Reports.IsEmpty())
        {
            LoadedSaveState.RuntimeState.AdvisorSystem.Reports = GenerateAdvisorReports(LoadedSaveState.RuntimeState);
        }
    }

    const FString AdvisorName = SelectedMeetingAdvisorName.IsEmpty() ? TEXT("Meeting Advisor") : SelectedMeetingAdvisorName;
    const FString AdvisorFocus = SelectedMeetingAdvisorFocus.IsEmpty() ? TEXT("General advisor meeting logic.") : SelectedMeetingAdvisorFocus;
    const FString CurrentState = bHasLoadedRuntimeState ? LoadedSaveSummary : TEXT("No runtime state loaded.");
    const bool bDiplomacyAdvisor = AdvisorName.Equals(TEXT("Diplomacy Advisor"), ESearchCase::IgnoreCase);
    const FDemocracySimulationState* State = bHasLoadedRuntimeState ? &LoadedSaveState.RuntimeState : nullptr;
    const FDemocracyAdvisorReport* AdvisorReport = State ? FindAdvisorReport(*State, AdvisorName) : nullptr;
    const FString GuidanceLevel = State ? AdvisorGuidanceForDifficultyScore(State->PlayerCountry.CountrySizeScore) : FString(TEXT("Standard"));

    FString Recommendation = TEXT("No advisor report is available yet. Hold a meeting after loading a state to generate a recommendation.");
    FString Warning = TEXT("No active warning.");
    FString Tradeoff = TEXT("No tradeoff available.");
    FString Issue = AdvisorFocus;
    if (AdvisorReport)
    {
        Issue = AdvisorReport->IssueReport;
        Recommendation = AdvisorReport->Recommendation;
        Warning = AdvisorReport->Warning;
        Tradeoff = AdvisorReport->TradeoffExplanation;
    }

    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("State"), CurrentState)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Advisor Role"), AdvisorFocus)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Current Issue"), BuildGuidedMeetingIssueText(GuidanceLevel, Issue, Recommendation, Warning, Tradeoff))]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Difficulty Guidance"), State ? RuntimeGuidanceSummary(*State) : TEXT("Standard guidance active."))]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Meeting System"), BuildMeetingSystemStatusText())]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Current Signals"), BuildSimulationStatusText())]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f)
        [BuildInfoRow(TEXT("Advisor Agenda"), GuidanceText(GuidanceLevel, TEXT("Choose one agenda. Detailed mode shows recommendations, consequences, and comparison cues before the meeting result is written to the save state."), TEXT("Choose one agenda. Standard mode shows recommendations and consequence previews."), TEXT("Choose one agenda. Limited mode shows broad direction."), TEXT("Choose one agenda. Consequence details are hidden.")))];

    const TArray<FAdvisorAgendaOption> AgendaOptions = GetAdvisorAgendaOptions(AdvisorName);
    for (const FAdvisorAgendaOption& Option : AgendaOptions)
    {
        Body->AddSlot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 2.0f)
        [BuildInfoRow(Option.Label, BuildGuidedAdvisorAgendaText(GuidanceLevel, Option))];
        Body->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 4.0f)
        [BuildButton(FString::Printf(TEXT("Choose: %s"), *Option.Label), FOnClicked::CreateUObject(this, &ALoginHUD::HandleHoldMeeting, FString(TEXT("Advisor")), AdvisorName, Option.AgendaItem), 440.0f, 38.0f, bHasLoadedRuntimeState)];
    }

    if (bDiplomacyAdvisor)
    {
        Body->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 4.0f)
        [BuildInfoRow(TEXT("Foreign Officials"), TEXT("Use diplomatic agendas to affect foreign trust, diplomacy, trade, and invasion pressure."))];
        Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildButton(TEXT("Trade Delegation"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleHoldMeeting, FString(TEXT("Foreign Official")), FString(TEXT("Trade Delegation")), FString(TEXT("Trade Delegation"))), 380.0f, 38.0f, bHasLoadedRuntimeState)];
        Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildButton(TEXT("De-escalation Talks"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleHoldMeeting, FString(TEXT("Foreign Official")), FString(TEXT("Border Envoy")), FString(TEXT("De-escalation Talks"))), 380.0f, 38.0f, bHasLoadedRuntimeState)];
        Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildButton(TEXT("Alliance Outreach"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleHoldMeeting, FString(TEXT("Foreign Official")), FString(TEXT("Allied Delegate")), FString(TEXT("Alliance Outreach"))), 380.0f, 38.0f, bHasLoadedRuntimeState)];
    }

    Body->AddSlot().AutoHeight().Padding(0.0f, 12.0f)
    [BuildButton(TEXT("Close"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleCloseOfficeOverlayClicked), 160.0f, 40.0f)];

    return BuildPanel(AdvisorName, TEXT("Advisor meeting agenda, recommendations, choices, and consequences."),
        SNew(SScrollBox)
        + SScrollBox::Slot()
        [Body], 820.0f);
}

TSharedRef<SWidget> ALoginHUD::BuildOfficePressReleaseScreen()
{
    if (bHasLoadedRuntimeState)
    {
        InitializePressOfficeIfMissing(LoadedSaveState.RuntimeState);
    }

    const FString CredibilityGuidance = bHasLoadedRuntimeState
        ? FString::Printf(TEXT("Credibility %d. Higher credibility improves truthful announcement effects; repeated empty or false statements stack penalties."), LoadedSaveState.RuntimeState.PressOffice.Credibility)
        : FString(TEXT("No runtime state loaded."));

    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Audience"), TEXT("State citizens, foreign officials, press corps, and global observers."))]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("State"), bHasLoadedRuntimeState ? LoadedSaveSummary : TEXT("No runtime state loaded."))]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Press Office"), BuildPressOfficeStatusText())]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Credibility Rule"), CredibilityGuidance)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Current Signals"), BuildSimulationStatusText())]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f)
        [BuildInfoRow(TEXT("Announcements"), TEXT("Choose one podium announcement. The result immediately updates credibility, approval, stability, diplomacy, unrest, advisor reports, and decision history."))];

    const TArray<FPressAnnouncementOption> Options = GetPressAnnouncementOptions();
    for (const FPressAnnouncementOption& Option : Options)
    {
        Body->AddSlot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 2.0f)
        [BuildInfoRow(Option.Label, FString::Printf(TEXT("%s\nConsequence: %s"), *Option.Purpose, *Option.ConsequencePreview))];
        Body->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 4.0f)
        [BuildButton(FString::Printf(TEXT("Announce: %s"), *Option.Label), FOnClicked::CreateUObject(this, &ALoginHUD::HandleMakePressRelease, Option.AnnouncementType), 440.0f, 38.0f, bHasLoadedRuntimeState)];
    }

    Body->AddSlot().AutoHeight().Padding(0.0f, 12.0f)
    [BuildButton(TEXT("Close"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleCloseOfficeOverlayClicked), 160.0f, 40.0f)];

    return BuildPanel(TEXT("Press Release Podium"), TEXT("Public announcements with credibility, diplomacy, approval, stability, and unrest effects."),
        SNew(SScrollBox)
        + SScrollBox::Slot()
        [Body], 820.0f);
}
TSharedRef<SWidget> ALoginHUD::BuildGameOverScreen()
{
    FString ProtectedReloadStatus = TEXT("No local save is loaded, so protected reload is unavailable.");
    bool bCanReload = false;
    if (!LoadedSavePath.IsEmpty())
    {
        FString ProtectedPath;
        FString ProtectedError;
        bCanReload = FDemocracySaveGameRuntime::GetProtectedReloadSavePath(LoadedSavePath, ProtectedPath, ProtectedError);
        ProtectedReloadStatus = bCanReload
            ? FString::Printf(TEXT("Protected reload available: %s"), *ProtectedPath)
            : FString::Printf(TEXT("Protected reload unavailable: %s"), *ProtectedError);
    }

    return BuildPanel(TEXT("Game Over"), GameOverReason.IsEmpty() ? TEXT("The administration has fallen.") : GameOverReason,
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Reason"), bHasLoadedRuntimeState ? BuildGuidedGameOverDetails(LoadedSaveState.RuntimeState, GameOverReason.IsEmpty() ? TEXT("The administration has fallen.") : GameOverReason, GameOverDetails.IsEmpty() ? TEXT("No details recorded.") : GameOverDetails) : (GameOverDetails.IsEmpty() ? TEXT("No details recorded.") : GameOverDetails))]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Save Protection"), ProtectedReloadStatus)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Current Save"), LoadedSavePath.IsEmpty() ? TEXT("No local save path is loaded.") : LoadedSavePath)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f)
        [BuildButton(TEXT("Reload Previous Save"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleReloadPreviousSaveClicked), 300.0f, 46.0f, bCanReload)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f)
        [BuildButton(TEXT("Return To Saves"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleBackToLocalSavesClicked), 220.0f, 42.0f)], 680.0f);
}TSharedRef<SWidget> ALoginHUD::BuildMultiplayerStateSelectionScreen()
{
    return BuildPanel(TEXT("Online States"), TEXT("Server-owned game states tied to the signed-in account."),
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Security Rule"), TEXT("Multiplayer saves are not stored locally; game servers provide account-linked state data."))]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)
        [BuildButton(TEXT("Account State Alpha"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSelectOnlineState, FString(TEXT("Account State Alpha"))), 420.0f, 46.0f)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f)
        [BuildButton(TEXT("Account State Beta"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSelectOnlineState, FString(TEXT("Account State Beta"))), 420.0f, 46.0f)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f)
        [BuildButton(TEXT("Refresh From Game Server"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleRefreshOnlineStatesClicked), 420.0f, 46.0f)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 14.0f)
        [BuildButton(TEXT("Back"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleBackToModeSelectionClicked), 180.0f, 44.0f)]);
}

TSharedRef<SWidget> ALoginHUD::BuildServerSelectionScreen()
{
    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);

    Body->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
    [BuildInfoRow(TEXT("Selected State"), SelectedOnlineState.IsEmpty() ? TEXT("None") : SelectedOnlineState)];

    Body->AddSlot().AutoHeight().Padding(0.0f, 8.0f)
    [
        SNew(SEditableTextBox)
        .HintText(BodyText(TEXT("Search servers, regions, or rulesets")))
        .Text(BodyText(ServerSearchText))
        .OnTextChanged(FOnTextChanged::CreateUObject(this, &ALoginHUD::HandleServerSearchChanged))
    ];

    Body->AddSlot().AutoHeight().Padding(0.0f, 8.0f)
    [
        SNew(SCheckBox)
        .IsChecked(bShowRecommendedServersOnly ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
        .OnCheckStateChanged(FOnCheckStateChanged::CreateUObject(this, &ALoginHUD::HandleRecommendedServersChanged))
        [
            SNew(STextBlock)
            .Text(BodyText(TEXT("Recommended ping only")))
            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 16))
            .ColorAndOpacity(FLinearColor::White)
        ]
    ];

    TSharedRef<SScrollBox> ServerList = SNew(SScrollBox);
    const FString SearchLower = ServerSearchText.ToLower();
    int32 VisibleServers = 0;

    for (const FPlaceholderServer& Server : GetPlaceholderServers())
    {
        const FString SearchTarget = FString::Printf(TEXT("%s %s %s"), *Server.Name, *Server.Region, *Server.Ruleset).ToLower();
        const bool bMatchesSearch = SearchLower.IsEmpty() || SearchTarget.Contains(SearchLower);
        const bool bMatchesRecommended = !bShowRecommendedServersOnly || Server.PingMs <= 80;

        if (!bMatchesSearch || !bMatchesRecommended)
        {
            continue;
        }

        ++VisibleServers;
        const FString Label = FString::Printf(TEXT("%s  |  %s  |  %d ms"), *Server.Name, *Server.Region, Server.PingMs);
        const FString Detail = FString::Printf(TEXT("Ruleset: %s. Placeholder row sorted by best ping."), *Server.Ruleset);

        ServerList->AddSlot().Padding(0.0f, 4.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [BuildButton(Label, FOnClicked::CreateUObject(this, &ALoginHUD::HandleSelectServer, Server.Name), 520.0f, 46.0f)]
            + SVerticalBox::Slot().AutoHeight()
            [BuildInfoRow(TEXT("Server Details"), Detail)]
        ];
    }

    if (VisibleServers == 0)
    {
        ServerList->AddSlot().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("No Servers Found"), TEXT("Adjust search or filters. Live data will replace placeholders later."))];
    }

    Body->AddSlot().MaxHeight(310.0f).Padding(0.0f, 8.0f)
    [ServerList];
    Body->AddSlot().AutoHeight().Padding(0.0f, 14.0f)
    [BuildButton(TEXT("Back"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleBackToOnlineStatesClicked), 180.0f, 44.0f)];

    return BuildPanel(TEXT("Server Selection"), TEXT("Servers are displayed from best ping to worst ping."), Body);
}

TSharedRef<SWidget> ALoginHUD::BuildPanel(const FString& Title, const FString& Subtitle, const TSharedRef<SWidget>& Body, float Width, FOnUserScrolled ScrollHandler, float InitialScrollOffset, TSharedPtr<SScrollBox>* OutScrollBox)
{
    TSharedRef<SScrollBox> PanelScroll = SNew(SScrollBox)
        .OnUserScrolled(ScrollHandler);
    PanelScroll->AddSlot()
    [Body];
    PanelScroll->SetScrollOffset(InitialScrollOffset);
    if (InitialScrollOffset > 0.0f)
    {
        TWeakPtr<SScrollBox> WeakPanelScroll = PanelScroll;
        PanelScroll->RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateLambda([WeakPanelScroll, InitialScrollOffset](double, float)
        {
            if (TSharedPtr<SScrollBox> PinnedScroll = WeakPanelScroll.Pin())
            {
                PinnedScroll->SetScrollOffset(InitialScrollOffset);
            }
            return EActiveTimerReturnType::Stop;
        }));
    }
    if (OutScrollBox)
    {
        *OutScrollBox = PanelScroll;
    }

    return SNew(SBox)
        .WidthOverride(Width)
        .MaxDesiredHeight(860.0f)
        [
            SNew(SBorder)
            .BorderImage(PanelBrush.Get())
            .Padding(28.0f)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
                [
                    SNew(STextBlock)
                    .Text(BodyText(Title))
                    .Justification(ETextJustify::Center)
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 42))
                    .ColorAndOpacity(FLinearColor::White)
                    .ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.65f))
                    .ShadowOffset(FVector2D(1.0f, 1.0f))
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 20.0f)
                [
                    SNew(STextBlock)
                    .Text(BodyText(Subtitle))
                    .Justification(ETextJustify::Center)
                    .AutoWrapText(true)
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 16))
                    .ColorAndOpacity(FLinearColor(0.84f, 0.86f, 0.88f, 1.0f))
                ]
                + SVerticalBox::Slot().FillHeight(1.0f)
                [PanelScroll]
            ]
        ];
}
TSharedRef<SWidget> ALoginHUD::BuildButton(const FString& Label, FOnClicked ClickHandler, float Width, float Height, bool bEnabled) const
{
    return SNew(SBox)
        .WidthOverride(Width)
        .MinDesiredHeight(Height)
        .HAlign(HAlign_Center)
        [
            SNew(SButton)
            .ButtonStyle(LoginButtonStyle.Get())
            .IsEnabled(bEnabled)
            .HAlign(HAlign_Center)
            .VAlign(VAlign_Center)
            .ContentPadding(FMargin(10.0f, 3.0f))
            .OnClicked(ClickHandler)
            [
                SNew(STextBlock)
                .Text(BodyText(Label))
                .Justification(ETextJustify::Center)
                .AutoWrapText(true)
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 16))
                .ColorAndOpacity(bEnabled ? FLinearColor::White : FLinearColor(0.48f, 0.50f, 0.53f, 1.0f))
            ]
        ];
}

TSharedRef<SWidget> ALoginHUD::BuildBackButton()
{
    return BuildButton(TEXT("Back"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleBackToLoginClicked), 180.0f, 44.0f);
}

TSharedRef<SWidget> ALoginHUD::BuildInfoRow(const FString& Primary, const FString& Secondary) const
{
    return SNew(SBorder)
        .BorderImage(RowBrush.Get())
        .Padding(FMargin(14.0f, 10.0f))
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(STextBlock)
                .Text(BodyText(Primary))
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
                .ColorAndOpacity(FLinearColor::White)
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text(BodyText(Secondary))
                .AutoWrapText(true)
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
                .ColorAndOpacity(FLinearColor(0.78f, 0.81f, 0.84f, 1.0f))
            ]
        ];
}

TSharedRef<SWidget> ALoginHUD::BuildSliderRow(const FString& Label, float Value, FOnFloatValueChanged ValueChanged) const
{
    return SNew(SBorder)
        .BorderImage(RowBrush.Get())
        .Padding(FMargin(14.0f, 9.0f))
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(0.42f).VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(BodyText(FString::Printf(TEXT("%s: %s"), *Label, *PercentText(Value))))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 15))
                .ColorAndOpacity(FLinearColor::White)
            ]
            + SHorizontalBox::Slot().FillWidth(0.58f).VAlign(VAlign_Center)
            [
                SNew(SSlider)
                .Value(Value)
                .OnValueChanged(ValueChanged)
            ]
        ];
}

TSharedRef<SWidget> ALoginHUD::BuildCheckRow(const FString& Label, const FString& Detail, bool bChecked, FOnCheckStateChanged CheckChanged) const
{
    return SNew(SBorder)
        .BorderImage(RowBrush.Get())
        .Padding(FMargin(14.0f, 9.0f))
        [
            SNew(SCheckBox)
            .IsChecked(bChecked ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
            .OnCheckStateChanged(CheckChanged)
            [
                SNew(STextBlock)
                .Text(BodyText(FString::Printf(TEXT("%s - %s"), *Label, *Detail)))
                .AutoWrapText(true)
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 15))
                .ColorAndOpacity(FLinearColor::White)
            ]
        ];
}

TSharedRef<SWidget> ALoginHUD::BuildKeybindRow(const FString& ActionName, const FString& CurrentKey)
{
    return SNew(SBorder)
        .BorderImage(RowBrush.Get())
        .Padding(FMargin(14.0f, 9.0f))
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(0.50f).VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(BodyText(ActionName))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 15))
                .ColorAndOpacity(FLinearColor::White)
            ]
            + SHorizontalBox::Slot().FillWidth(0.25f).VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(BodyText(CurrentKey))
                .Justification(ETextJustify::Center)
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 15))
                .ColorAndOpacity(FLinearColor(0.86f, 0.89f, 0.92f, 1.0f))
            ]
            + SHorizontalBox::Slot().FillWidth(0.25f).VAlign(VAlign_Center).HAlign(HAlign_Right)
            [BuildButton(TEXT("Change"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleChangeKeybind, ActionName), 130.0f, 38.0f)]
        ];
}

TArray<FString> ALoginHUD::GetLocalSaveNames() const
{
    TArray<FString> SaveFiles;
    const FString SaveDirectory = FPaths::ProjectDir() / TEXT("Saves");

    IFileManager::Get().FindFiles(SaveFiles, *(SaveDirectory / TEXT("*.sav")), true, false);
    IFileManager::Get().FindFiles(SaveFiles, *(SaveDirectory / TEXT("*.json")), true, false);
    IFileManager::Get().FindFiles(SaveFiles, *(SaveDirectory / TEXT("*.democracy")), true, false);

    SaveFiles.Sort();
    return SaveFiles;
}

FString ALoginHUD::BuildSafeSaveFileName(const FString& StateName) const
{
    FString SafeName = StateName.TrimStartAndEnd();

    const TCHAR InvalidReplacement = TEXT('_');
    for (TCHAR& Character : SafeName)
    {
        if (!FChar::IsAlnum(Character) && Character != TEXT('-') && Character != TEXT('_'))
        {
            Character = InvalidReplacement;
        }
    }

    while (SafeName.Contains(TEXT("__")))
    {
        SafeName.ReplaceInline(TEXT("__"), TEXT("_"));
    }

    SafeName.TrimStartAndEndInline();
    if (SafeName.IsEmpty())
    {
        SafeName = TEXT("New_State");
    }

    return SafeName;
}

bool ALoginHUD::CreateInitialSinglePlayerSave(FString& OutSavePath)
{
    const FString CleanStateName = PendingStateName.TrimStartAndEnd();
    if (CleanStateName.IsEmpty() || PendingDifficulty.IsEmpty() || PendingClimate.IsEmpty() || PendingLeaderGender.IsEmpty() || PendingStartingCountryMapIndex <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("Single-player save creation failed: missing state name, difficulty, climate, address, or starting country."));
        return false;
    }

    const FString SaveDirectory = FPaths::ProjectDir() / TEXT("Saves");
    if (!FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*SaveDirectory))
    {
        UE_LOG(LogTemp, Error, TEXT("Single-player save creation failed: could not create Saves directory at %s."), *SaveDirectory);
        return false;
    }

    const FString BaseFileName = BuildSafeSaveFileName(CleanStateName);
    FString SavePath = SaveDirectory / FString::Printf(TEXT("%s.democracy"), *BaseFileName);
    int32 DuplicateIndex = 2;

    while (FPaths::FileExists(SavePath))
    {
        SavePath = SaveDirectory / FString::Printf(TEXT("%s_%d.democracy"), *BaseFileName, DuplicateIndex++);
    }

    const FDifficultyProfile DifficultyProfile = FDifficultyProfileLibrary::GetProfile(PendingDifficulty);
    FDemocracySimulationState InitialGameState = FDemocracyGameStateFactory::CreateInitialState(
        CleanStateName,
        PendingLeaderGender,
        PendingAddressTitle,
        PendingClimate,
        DifficultyProfile,
        PendingStartingCountryMapIndex);
    InitializeEarlyGameBalanceTestData(InitialGameState);
    const FString SaveId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
    const FString CreatedAt = SaveTimestamp();
    const FString SaveContents = FString::Printf(
        TEXT("{\n")
        TEXT("  \"formatVersion\": 1,\n")
        TEXT("  \"saveId\": \"%s\",\n")
        TEXT("  \"stateName\": \"%s\",\n")
        TEXT("  \"mode\": \"SinglePlayer\",\n")
        TEXT("  \"difficulty\": \"%s\",\n")
        TEXT("  \"difficultyProfile\": %s,\n")
        TEXT("  \"climate\": \"%s\",\n")
        TEXT("  \"startingCountryName\": \"%s\",\n")
        TEXT("  \"startingCountryMapIndex\": %d,\n")
        TEXT("  \"leaderGender\": \"%s\",\n")
        TEXT("  \"addressTitle\": \"%s\",\n")
        TEXT("  \"createdAtUtc\": \"%s\",\n")
        TEXT("  \"lastPlayedAtUtc\": \"%s\",\n")
        TEXT("  \"initialGameState\": %s,\n")
        TEXT("  \"progress\": {\n")
        TEXT("    \"turn\": 1,\n")
        TEXT("    \"phase\": \"Initial Setup\"\n")
        TEXT("  }\n")
        TEXT("}\n"),
        *JsonEscape(SaveId),
        *JsonEscape(CleanStateName),
        *JsonEscape(DifficultyProfile.Name),
        *DifficultyProfile.ToJson(2),
        *JsonEscape(PendingClimate),
        *JsonEscape(PendingStartingCountryName),
        PendingStartingCountryMapIndex,
        *JsonEscape(PendingLeaderGender),
        *JsonEscape(PendingAddressTitle),
        *JsonEscape(CreatedAt),
        *JsonEscape(CreatedAt),
        *InitialGameState.ToJson(2));

    if (!FFileHelper::SaveStringToFile(SaveContents, *SavePath))
    {
        UE_LOG(LogTemp, Error, TEXT("Single-player save creation failed: could not write %s."), *SavePath);
        return false;
    }

    OutSavePath = SavePath;
    LocalSaveSearchText.Empty();

    if (!LoadSinglePlayerSaveIntoRuntime(SavePath))
    {
        return false;
    }

    FString ProtectionError;
    if (FDemocracySaveGameRuntime::SaveSinglePlayerRuntimeState(LoadedSaveState, ProtectionError))
    {
        FDemocracySaveGameRuntime::SaveSinglePlayerAutosave(LoadedSaveState, ProtectionError);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Initial save protection setup failed: %s"), *ProtectionError);
    }

    UE_LOG(LogTemp, Log, TEXT("Single-player save created and loaded: %s"), *SavePath);
    return true;
}

bool ALoginHUD::LoadSinglePlayerSaveIntoRuntime(const FString& SavePath)
{
    FDemocracyLoadedSaveState LoadedSave;
    FString LoadError;
    if (!FDemocracySaveGameRuntime::LoadSinglePlayerSaveWithFallback(SavePath, LoadedSave, LoadError))
    {
        LoadedSaveError = LoadError;
        bHasLoadedRuntimeState = false;
        UE_LOG(LogTemp, Error, TEXT("Single-player save load failed: %s"), *LoadError);
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 7.0f, FColor::Red, LoadError);
        }
        return false;
    }

    StopSimulationTimer();
    LoadedSaveState = LoadedSave;
    SimulationTickCount = 0;
    LoadedStateName = LoadedSave.StateName;
    LoadedSavePath = LoadedSave.SavePath;
    LoadedSaveSummary = LoadedSave.ToSummaryText();
    SimulationTickSummary = BuildSimulationStatusText();
    LoadedSaveError.Empty();
    LastSaveStatus = LoadError.IsEmpty() ? TEXT("Loaded local save into runtime.") : LoadError;
    GameOverReason.Empty();
    GameOverDetails.Empty();
    bHasLoadedRuntimeState = true;
    OpeningScriptText.Empty();
    bShowFirstLoginBriefing = false;
    LoadedSaveState.RuntimeState.AdvisorSystem.GuidanceLevel = AdvisorGuidanceForDifficultyScore(LoadedSaveState.RuntimeState.PlayerCountry.CountrySizeScore);
    LoadedSaveState.RuntimeState.AdvisorSystem.LastUpdatedTurn = LoadedSaveState.RuntimeState.Turn;
    if (LoadedSaveState.RuntimeState.AdvisorSystem.AdvisorCount <= 0)
    {
        LoadedSaveState.RuntimeState.AdvisorSystem.AdvisorCount = FMath::Clamp(6 - LoadedSaveState.RuntimeState.PlayerCountry.CountrySizeScore, 1, 5);
    }
    if (LoadedSaveState.RuntimeState.ResourceChains.Chains.Num() == 0)
    {
        FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
        const FDemocracyResourceInventory& Resources = State.PlayerCountry.Resources;
        const int32 DifficultyScore = FMath::Clamp(State.PlayerCountry.CountrySizeScore, 1, 4);
        State.ResourceChains.LastUpdatedTurn = State.Turn;
        State.ResourceChains.TotalShortagePressure = 0;
        State.ResourceChains.TradeBalance = 0;
        State.ResourceChains.Summary = TEXT("Resource chain initialized from existing save reserves. Step simulation to calculate live production, consumption, imports, and exports.");
        State.ResourceChains.Chains = {
            MakeResourceChainEntry(TEXT("Food"), Resources.Food, 150 + DifficultyScore * 18, 0, 0, 0, 0, 90, TEXT("Population food access; shortages raise unrest fastest."), { TEXT("loaded reserve"), TEXT("agriculture") }),
            MakeResourceChainEntry(TEXT("Water"), Resources.Water, 130 + DifficultyScore * 15, 0, 0, 0, 0, 95, TEXT("Public health and regional stability; shortages damage demographics."), { TEXT("loaded reserve"), TEXT("public services") }),
            MakeResourceChainEntry(TEXT("Fuel"), Resources.GasOil, 80 + DifficultyScore * 14, 0, 0, 0, 0, 100, TEXT("Logistics, industry, military readiness, and inflation pressure."), { TEXT("loaded reserve"), TEXT("trade") }),
            MakeResourceChainEntry(TEXT("Wood"), Resources.Wood, 70 + DifficultyScore * 10, 0, 0, 0, 0, 60, TEXT("Construction, infrastructure repairs, and disaster recovery."), { TEXT("loaded reserve"), TEXT("forestry") }),
            MakeResourceChainEntry(TEXT("Metals"), Resources.Metals, 75 + DifficultyScore * 12, 0, 0, 0, 0, 85, TEXT("Industry, infrastructure projects, and military production."), { TEXT("loaded reserve"), TEXT("mining") })
        };
    }
    InitializeEarlyGameBalanceTestData(LoadedSaveState.RuntimeState);
    RefreshObjectiveState(LoadedSaveState.RuntimeState, LoadedSaveState.Mode.IsEmpty() ? TEXT("SinglePlayer") : LoadedSaveState.Mode);
    RefreshFailureValidationState(LoadedSaveState.RuntimeState);
    RefreshWarConflictState(LoadedSaveState.RuntimeState);
    RefreshSimulationToRtsContract(LoadedSaveState.RuntimeState);
    {
        const FDemocracyRtsWorldState& RtsWorld = LoadedSaveState.RuntimeState.RtsWorld;
        LastSaveStatus = FString::Printf(TEXT("%s | RTS persisted: armies %d, orders %d, supply routes %d, construction queues %d, fog provinces %d, battles %d, ownership provinces %d."),
            *LastSaveStatus,
            RtsWorld.ArmyGroups.Num(),
            RtsWorld.MovementOrders.Num(),
            RtsWorld.SupplyRoutes.Num(),
            RtsWorld.CityBase.ConstructionQueue.Num(),
            RtsWorld.FogOfWar.Provinces.Num(),
            RtsWorld.BattleHistory.Num(),
            RtsWorld.Ownership.Provinces.Num());
        UE_LOG(LogTemp, Log, TEXT("%s"), *LastSaveStatus);
    }
    LoadedSaveSummary = LoadedSaveState.ToSummaryText();
    SimulationTickSummary = BuildSimulationStatusText();

    UE_LOG(LogTemp, Log, TEXT("Single-player runtime state ready: %s"), *LoadedSaveSummary);
    return true;
}

FString ALoginHUD::BuildSimulationStatusText() const
{
    if (!bHasLoadedRuntimeState)
    {
        return LoadedSaveError.IsEmpty() ? TEXT("No runtime state is loaded.") : LoadedSaveError;
    }

    const FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    const FDemocracyCountryState& Country = State.PlayerCountry;
    const FString GuidanceLevel = GetRuntimeGuidanceLevel(State);
    const int32 InternalPct = State.FailureRisk.AssassinationRiskTrigger > 0 ? (State.FailureRisk.CurrentAssassinationRisk * 100) / State.FailureRisk.AssassinationRiskTrigger : 0;
    const int32 TakeoverPct = State.InvasionRisk.InvasionRiskTrigger > 0 ? (State.InvasionRisk.CurrentInvasionRisk * 100) / State.InvasionRisk.InvasionRiskTrigger : 0;

    FString StatusText = GuidanceText(GuidanceLevel,
        FString::Printf(TEXT("Tick %d | Turn %d | %s | Phase: %s\nApproval %d, Stability %d, Unrest %d. Keep unrest below %d and stability above %d to reduce internal failure risk.\nTreasury %d, Economy %d, Diplomacy %d, Military %d. Keep readiness above %d and diplomacy above %d to reduce takeover risk.\nInternal risk %d%% | Takeover risk %d%%."),
            SimulationTickCount,
            State.Turn,
            State.bPaused ? TEXT("Paused") : TEXT("Running"),
            *State.Phase,
            Country.PublicApproval,
            Country.Stability,
            Country.Unrest,
            State.FailureRisk.UnrestWarningThreshold,
            State.FailureRisk.StabilityWarningThreshold,
            Country.Treasury,
            Country.EconomicHealth,
            Country.DiplomaticStanding,
            Country.MilitaryReadiness,
            State.InvasionRisk.MilitaryReadinessWarningThreshold,
            45,
            InternalPct,
            TakeoverPct),
        FString::Printf(TEXT("Tick %d | Turn %d | %s | Approval %d | Stability %d | Unrest %d | Treasury %d | Economy %d | Diplomacy %d | Military %d | Internal %d%% | Takeover %d%% | %s"),
            SimulationTickCount,
            State.Turn,
            State.bPaused ? TEXT("Paused") : TEXT("Running"),
            Country.PublicApproval,
            Country.Stability,
            Country.Unrest,
            Country.Treasury,
            Country.EconomicHealth,
            Country.DiplomaticStanding,
            Country.MilitaryReadiness,
            InternalPct,
            TakeoverPct,
            *State.Phase),
        FString::Printf(TEXT("Turn %d | %s | Approval %d | Stability %d | Unrest %d | Internal %d%% | Takeover %d%% | %s"),
            State.Turn,
            State.bPaused ? TEXT("Paused") : TEXT("Running"),
            Country.PublicApproval,
            Country.Stability,
            Country.Unrest,
            InternalPct,
            TakeoverPct,
            *State.Phase),
        FString::Printf(TEXT("Turn %d | %s | Internal %d%% | Takeover %d%% | %s"),
            State.Turn,
            State.bPaused ? TEXT("Paused") : TEXT("Running"),
            InternalPct,
            TakeoverPct,
            *State.Phase));

    if (SimulationTickSummary.StartsWith(TEXT("Tick ")) && SimulationTickSummary.Contains(TEXT("Resources:")) && SimulationTickSummary.Contains(TEXT("National effects:")))
    {
        StatusText += FString::Printf(TEXT("\n\nLast Tick Result:\n%s"), *SimulationTickSummary);
    }
    return StatusText;
}

FString ALoginHUD::BuildDiplomacyStatusText() const
{
    if (!bHasLoadedRuntimeState)
    {
        return TEXT("No diplomacy state is loaded.");
    }

    const FDemocracyDiplomacyMatrixState& Matrix = LoadedSaveState.RuntimeState.DiplomacyMatrix;
    FString Text = FString::Printf(TEXT("%s\nRelationships: %d | allies %d | neutral %d | rivals %d | hostile %d | trade partners %d | sanctions %d | treaties %d."),
        *Matrix.Summary,
        Matrix.Relationships.Num(),
        Matrix.AllyCount,
        Matrix.NeutralCount,
        Matrix.RivalCount,
        Matrix.HostileCount,
        Matrix.TradePartnerCount,
        Matrix.SanctionsCount,
        Matrix.TreatyCount);

    int32 Shown = 0;
    for (const FDemocracyDiplomacyRelationshipState& Relationship : Matrix.Relationships)
    {
        if (Shown >= 6)
        {
            break;
        }
        if (Relationship.RelationshipStatus.Equals(TEXT("Hostile"), ESearchCase::IgnoreCase) ||
            Relationship.RelationshipStatus.Equals(TEXT("Rival"), ESearchCase::IgnoreCase) ||
            Relationship.bTradePartner)
        {
            Text += FString::Printf(TEXT("\n%s: %s | %s | trust %d | border tension %d | trade %d | sanctions %s | treaty %s"),
                *Relationship.CountryName,
                *Relationship.RelationshipStatus,
                *Relationship.GovernmentType,
                Relationship.Trust,
                Relationship.BorderTension,
                Relationship.TradeValue,
                Relationship.bSanctionsActive ? TEXT("yes") : TEXT("no"),
                *Relationship.TreatyStatus);
            ++Shown;
        }
    }
    return Text;
}
FString ALoginHUD::BuildGovernmentDiplomacyRulesStatusText() const
{
    if (!bHasLoadedRuntimeState)
    {
        return TEXT("Government/diplomacy rules unavailable until a save is loaded.");
    }

    const FDemocracyGovernmentDiplomacyRulesState& Rules = LoadedSaveState.RuntimeState.GovernmentDiplomacyRules;
    TArray<FString> Lines;
    Lines.Add(Rules.Summary);
    Lines.Add(FString::Printf(TEXT("Transition: target %s | progress %d%% | turns remaining %d | costs stability %d, unrest +%d, diplomacy %d"),
        Rules.TargetGovernmentType.IsEmpty() ? TEXT("none") : *Rules.TargetGovernmentType,
        Rules.TransitionProgress,
        Rules.TransitionTurnsRemaining,
        Rules.TransitionStabilityCost,
        Rules.TransitionUnrestCost,
        Rules.TransitionDiplomacyCost));
    Lines.Add(FString::Printf(TEXT("Rules: alliances allowed %d | blocked %d | treaties %d | sanctions %d | high border tension %d"),
        Rules.AllowedAllianceCount,
        Rules.BlockedAllianceCount,
        Rules.ActiveTreatyCount,
        Rules.ActiveSanctionsCount,
        Rules.HighBorderTensionCount));
    if (Rules.ActiveRestrictions.Num() > 0)
    {
        Lines.Add(FString::Printf(TEXT("Restrictions: %s"), *FString::Join(Rules.ActiveRestrictions, TEXT(" | "))));
    }
    if (Rules.SideSwitchConsequences.Num() > 0)
    {
        Lines.Add(FString::Printf(TEXT("Side-switch consequences: %s"), *FString::Join(Rules.SideSwitchConsequences, TEXT(" | "))));
    }
    for (const FDemocracyGovernmentDiplomacyRuleState& Rule : Rules.Rules)
    {
        Lines.Add(FString::Printf(TEXT("%s [%s]: %s"), *Rule.RuleName, *Rule.RuleType, *Rule.Description));
    }
    return FString::Join(Lines, TEXT("\n"));
}
FString ALoginHUD::BuildRtsBackflowStatusText() const
{
    if (!bHasLoadedRuntimeState)
    {
        return TEXT("RTS backflow unavailable until a save is loaded.");
    }

    return BuildRtsBackflowSummaryText(LoadedSaveState.RuntimeState.RtsWorld);
}

FString ALoginHUD::BuildWarConflictStatusText() const
{
    if (!bHasLoadedRuntimeState)
    {
        return TEXT("War/conflict state unavailable until a save is loaded.");
    }

    const FDemocracyWarSystemState& WarSystem = LoadedSaveState.RuntimeState.WarSystem;
    TArray<FString> Lines;
    Lines.Add(WarSystem.Summary);
    Lines.Add(FString::Printf(TEXT("Active %d | escalation pressure %d | fatigue %d | casualties %d | readiness %s"),
        WarSystem.ActiveConflictCount,
        WarSystem.EscalationPressure,
        WarSystem.WarFatigue,
        WarSystem.TotalCasualties,
        *WarSystem.ReadinessStatus));
    for (const FDemocracyWarConflictState& Conflict : WarSystem.ActiveConflicts)
    {
        Lines.Add(FString::Printf(TEXT("%s [%s/%s]: escalation %d | war score %d | victory %d | defeat risk %d"),
            *Conflict.ConflictName,
            *Conflict.ConflictType,
            *Conflict.Status,
            Conflict.EscalationLevel,
            Conflict.WarScore,
            Conflict.VictoryProgress,
            Conflict.DefeatRisk));
        Lines.Add(FString::Printf(TEXT("Objective: %s | Enemy: %s"), *Conflict.PrimaryObjective, *Conflict.EnemyObjective));
        if (Conflict.Fronts.Num() > 0)
        {
            const FDemocracyWarFrontState& Front = Conflict.Fronts[0];
            Lines.Add(FString::Printf(TEXT("Front: %s in %s | pressure %d | player control %d | %s"), *Front.FrontName, *Front.RegionName, Front.Pressure, Front.PlayerControl, *Front.Status));
        }
        Lines.Add(FString::Printf(TEXT("Win: %s | Lose: %s"), *Conflict.VictoryCondition, *Conflict.DefeatCondition));
    }
    if (WarSystem.ActiveConflicts.Num() == 0)
    {
        Lines.Add(TEXT("No active war. Invasion risk and diplomacy tension can escalate into a durable conflict."));
    }
    return FString::Join(Lines, TEXT("\n"));
}

FString ALoginHUD::BuildSimulationToRtsContractStatusText() const
{
    if (!bHasLoadedRuntimeState)
    {
        return TEXT("Simulation-to-RTS contract unavailable until a save is loaded.");
    }

    const FDemocracySimulationToRtsContractState& Contract = LoadedSaveState.RuntimeState.SimulationToRtsContract;
    TArray<FString> Lines;
    Lines.Add(Contract.ExportSummary);
    Lines.Add(FString::Printf(TEXT("Payload: %s | %s | treasury %d | readiness %d | tech %d | stability %d | unrest %d | invasion risk %d"),
        *Contract.ContractVersion,
        *Contract.GovernmentType,
        Contract.Treasury,
        Contract.MilitaryReadiness,
        Contract.Technology,
        Contract.Stability,
        Contract.Unrest,
        Contract.InvasionRisk));
    Lines.Add(FString::Printf(TEXT("Resources: food %d | fuel %d | wood %d | metals %d | water %d"),
        Contract.Resources.Food,
        Contract.Resources.GasOil,
        Contract.Resources.Wood,
        Contract.Resources.Metals,
        Contract.Resources.Water));
    Lines.Add(FString::Printf(TEXT("Diplomacy: allies %d | enemies %d | active war risks %d | relation records %d"),
        Contract.Allies.Num(),
        Contract.Enemies.Num(),
        Contract.ActiveWars.Num(),
        Contract.Diplomacy.Num()));
    Lines.Add(FString::Printf(TEXT("Regions exported: %d | policies %d | tech unlock lines %d"),
        Contract.Regions.Num(),
        Contract.ActivePolicies.Num(),
        Contract.TechnologyUnlocks.Num()));
    if (Contract.ActiveWars.Num() > 0)
    {
        Lines.Add(FString::Printf(TEXT("War/risk feed: %s"), *FString::Join(Contract.ActiveWars, TEXT("; "))));
    }
    if (Contract.StrategicPermissions.Num() > 0)
    {
        Lines.Add(FString::Printf(TEXT("Authority boundary: %s"), *Contract.StrategicPermissions.Last()));
    }
    return FString::Join(Lines, TEXT("\n"));
}

FString ALoginHUD::BuildRtsSaveBoundaryStatusText() const
{
    if (!bHasLoadedRuntimeState)
    {
        return TEXT("RTS save boundary unavailable until a save is loaded.");
    }

    const FDemocracyRtsSaveBoundaryState& Boundary = LoadedSaveState.RuntimeState.RtsSaveBoundary;
    TArray<FString> Lines;
    Lines.Add(Boundary.BoundarySummary);
    Lines.Add(FString::Printf(TEXT("Version %s | updated turn %d"), *Boundary.BoundaryVersion, Boundary.LastUpdatedTurn));
    Lines.Add(FString::Printf(TEXT("Simulation owns: %s"), *Boundary.SimulationAuthority));
    Lines.Add(FString::Printf(TEXT("RTS owns: %s"), *Boundary.RtsAuthority));
    Lines.Add(FString::Printf(TEXT("Save authority: %s"), *Boundary.SaveAuthority));
    Lines.Add(FString::Printf(TEXT("Multiplayer authority: %s"), *Boundary.MultiplayerAuthority));
    if (Boundary.SharedHandshakeFields.Num() > 0)
    {
        Lines.Add(FString::Printf(TEXT("Handshake: %s"), *FString::Join(Boundary.SharedHandshakeFields, TEXT(" | "))));
    }
    if (Boundary.ForbiddenSimulationWrites.Num() > 0)
    {
        Lines.Add(FString::Printf(TEXT("Forbidden from simulation office: %s"), *FString::Join(Boundary.ForbiddenSimulationWrites, TEXT(" | "))));
    }
    if (Boundary.SaveRules.Num() > 0)
    {
        Lines.Add(FString::Printf(TEXT("Save rules: %s"), *FString::Join(Boundary.SaveRules, TEXT(" | "))));
    }
    if (Boundary.ServerAuthoritativeFields.Num() > 0)
    {
        Lines.Add(FString::Printf(TEXT("Server authoritative: %s"), *FString::Join(Boundary.ServerAuthoritativeFields, TEXT(" | "))));
    }
    if (Boundary.ClientRequestOnlyFields.Num() > 0)
    {
        Lines.Add(FString::Printf(TEXT("Client request-only: %s"), *FString::Join(Boundary.ClientRequestOnlyFields, TEXT(" | "))));
    }
    if (Boundary.ServerValidationNotes.Num() > 0)
    {
        Lines.Add(FString::Printf(TEXT("Server validation: %s"), *FString::Join(Boundary.ServerValidationNotes, TEXT(" | "))));
    }
    if (Boundary.BoundaryValidationNotes.Num() > 0)
    {
        Lines.Add(FString::Printf(TEXT("Validation: %s"), *FString::Join(Boundary.BoundaryValidationNotes, TEXT(" | "))));
    }
    return FString::Join(Lines, TEXT("\n"));
}
FString ALoginHUD::BuildMapOwnershipStatusText() const
{
    if (!bHasLoadedRuntimeState)
    {
        return TEXT("Map ownership unavailable until a save is loaded.");
    }
    const FDemocracyMapOwnershipState& Ownership = LoadedSaveState.RuntimeState.RtsWorld.Ownership;
    TArray<FString> Lines;
    Lines.Add(Ownership.Summary);
    Lines.Add(FString::Printf(TEXT("Planet %s | %s | target %d countries | regions %d | population weight %d | area weight %d"), *Ownership.PlanetName, *Ownership.MapDataVersion, Ownership.DurableCountryTarget, Ownership.TotalMapRegionCount, Ownership.TotalPopulationWeight, Ownership.TotalAreaWeight));
    Lines.Add(FString::Printf(TEXT("Countries %d | provinces %d | player %d | contested %d | border %d"), Ownership.TotalCountries, Ownership.TotalProvinces, Ownership.PlayerControlledProvinces, Ownership.ContestedProvinces, Ownership.BorderProvinceCount));
    int32 Shown = 0;
    for (const FDemocracyContinentOwnershipState& Continent : Ownership.Continents)
    {
        if (Shown >= 8) break;
        Lines.Add(FString::Printf(TEXT("%s: %d provinces | player %d | contested %d"), *Continent.ContinentName, Continent.ProvinceCount, Continent.PlayerControlledProvinces, Continent.ContestedProvinces));
        ++Shown;
    }
    return FString::Join(Lines, TEXT("\n"));
}

FString ALoginHUD::BuildCommandAuthorityStatusText() const
{
    if (!bHasLoadedRuntimeState)
    {
        return TEXT("Command authority unavailable until a save is loaded.");
    }
    const FDemocracyCommandAuthorityState& Authority = LoadedSaveState.RuntimeState.CommandAuthority;
    TArray<FString> Lines;
    Lines.Add(FString::Printf(TEXT("Posture: %s | actions %d | updated turn %d"), *Authority.ActiveCommandPosture, Authority.Actions.Num(), Authority.LastUpdatedTurn));
    Lines.Add(Authority.OfficeAuthoritySummary);
    Lines.Add(Authority.RtsAuthoritySummary);
    Lines.Add(Authority.LastCommandSummary);
    for (const FDemocracyCommandAuthorityActionState& Action : Authority.Actions)
    {
        Lines.Add(FString::Printf(TEXT("%s [%s/%s]: %s%s"), *Action.Label, *Action.AuthorityLayer, *Action.CommandType, *Action.EffectPreview, Action.bEnabled ? TEXT("") : *FString::Printf(TEXT(" BLOCKED: %s"), *Action.DisabledReason)));
    }
    return FString::Join(Lines, TEXT("\n"));
}

FString ALoginHUD::BuildObjectiveStatusText() const
{
    if (!bHasLoadedRuntimeState)
    {
        return TEXT("No objective state is loaded.");
    }

    const FDemocracyObjectiveState& Objective = LoadedSaveState.RuntimeState.ObjectiveState;
    FString Text = FString::Printf(TEXT("%s\nMode: %s | Conversion: %d%% | Democracies: %d/%d | Dictatorships remaining: %d | Other: %d | Regression risk: %d%%"),
        *Objective.ObjectiveSummary,
        *Objective.Mode,
        Objective.DemocracyConversionProgress,
        Objective.DemocraticCountryCount,
        Objective.TotalTrackedCountryCount,
        Objective.DictatorshipsRemainingForVictory,
        Objective.OtherGovernmentCount,
        Objective.RegressionRisk);
    if (Objective.bSoftVictoryAchieved)
    {
        Text += FString::Printf(TEXT("\nVictory hook: achieved on turn %d | post-victory turns %d | continuation %s | regression monitoring %s."), Objective.SoftVictoryTurn, Objective.PostVictoryTurnsElapsed, Objective.bPostVictoryContinuationActive ? TEXT("active") : TEXT("inactive"), Objective.bRegressionMonitoringActive ? TEXT("active") : TEXT("inactive"));
    }
    if (Objective.bMultiplayerOngoingNoFinalWin)
    {
        Text += FString::Printf(TEXT("\nMultiplayer hook: ongoing server-state objective, no final win condition. Democracy slots %d | Dictatorship slots %d."), Objective.ServerDemocracySlots, Objective.ServerDictatorshipSlots);
    }
    if (Objective.GovernmentTransitionTurnsRemaining > 0)
    {
        Text += FString::Printf(TEXT("\nTransition: %s %d%% complete, %d turns remaining."), *Objective.GovernmentTransitionTarget, Objective.GovernmentTransitionProgress, Objective.GovernmentTransitionTurnsRemaining);
    }
    if (Objective.ActiveObjectiveNotes.Num() > 0)
    {
        Text += FString::Printf(TEXT("\n%s"), *FString::Join(Objective.ActiveObjectiveNotes, TEXT("\n")));
    }
    if (Objective.ObjectiveHooks.Num() > 0)
    {
        Text += FString::Printf(TEXT("\nHooks: %s"), *FString::Join(Objective.ObjectiveHooks, TEXT(", ")));
    }
    return Text;
}
FString ALoginHUD::BuildOngoingBriefingText() const
{
    if (!bHasLoadedRuntimeState)
    {
        return LoadedSaveError.IsEmpty() ? TEXT("No runtime state is loaded for briefing generation.") : LoadedSaveError;
    }

    const FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    const FDemocracyCountryState& Country = State.PlayerCountry;
    const FString GuidanceLevel = GetRuntimeGuidanceLevel(State);
    const int32 InternalPct = State.FailureRisk.AssassinationRiskTrigger > 0 ? (State.FailureRisk.CurrentAssassinationRisk * 100) / State.FailureRisk.AssassinationRiskTrigger : 0;
    const int32 TakeoverPct = State.InvasionRisk.InvasionRiskTrigger > 0 ? (State.InvasionRisk.CurrentInvasionRisk * 100) / State.InvasionRisk.InvasionRiskTrigger : 0;
    TArray<FString> Lines;
    TArray<FString> SuggestedActions;

    auto AddSection = [&Lines](const FString& Title)
    {
        Lines.Add(FString::Printf(TEXT("\n=== %s ==="), *Title));
    };
    auto AddSuggestedAction = [&SuggestedActions](const FString& Action)
    {
        if (!SuggestedActions.Contains(Action))
        {
            SuggestedActions.Add(Action);
        }
    };

    Lines.Add(FString::Printf(TEXT("Ongoing briefing for %s"), *(LoadedStateName.IsEmpty() ? Country.CountryName : LoadedStateName)));
    Lines.Add(FString::Printf(TEXT("Generated turn %d | %s | phase: %s | guidance: %s"), State.Turn, State.bPaused ? TEXT("Paused") : TEXT("Running"), *State.Phase, *GuidanceLevel));

    AddSection(TEXT("Executive Snapshot"));
    Lines.Add(GuidanceText(GuidanceLevel,
        FString::Printf(TEXT("Approval %d | Stability %d | Unrest %d | Treasury %d | Economy %d | Diplomacy %d | Military readiness %d | Internal risk %d%% | Takeover risk %d%%."),
            Country.PublicApproval, Country.Stability, Country.Unrest, Country.Treasury, Country.EconomicHealth, Country.DiplomaticStanding, Country.MilitaryReadiness, InternalPct, TakeoverPct),
        FString::Printf(TEXT("Approval %d | Stability %d | Unrest %d | Treasury %d | Economy %d | Military %d | Internal %d%% | Takeover %d%%."),
            Country.PublicApproval, Country.Stability, Country.Unrest, Country.Treasury, Country.EconomicHealth, Country.MilitaryReadiness, InternalPct, TakeoverPct),
        FString::Printf(TEXT("Approval %d | Stability %d | Unrest %d | Internal %d%% | Takeover %d%%."), Country.PublicApproval, Country.Stability, Country.Unrest, InternalPct, TakeoverPct),
        FString::Printf(TEXT("Internal %d%% | Takeover %d%%."), InternalPct, TakeoverPct)));
    Lines.Add(FString::Printf(TEXT("Time: %s"), *BuildTimeControlStatusText()));
    Lines.Add(FString::Printf(TEXT("Objective: %s"), *State.ObjectiveState.ObjectiveSummary));
    Lines.Add(FString::Printf(TEXT("Policy posture: Economy %s | Environment %s | Military %s | Diplomacy %s | Civil %s."),
        *Country.Policies.EconomicPolicy,
        *Country.Policies.EnvironmentalPolicy,
        *Country.Policies.MilitaryPolicy,
        *Country.Policies.DiplomacyPolicy,
        *Country.Policies.CivilPolicy));
    Lines.Add(FString::Printf(TEXT("Resources: %s"), *BuildResourceStatusText()));
    Lines.Add(FString::Printf(TEXT("Budget: %s"), *BuildEconomyBudgetStatusText()));
    Lines.Add(FString::Printf(TEXT("Population: %s"), *BuildDemographicsStatusText()));

    AddSection(TEXT("Active Crises"));
    int32 ActiveCrisisCount = 0;
    for (const FDemocracyActiveEventState& Event : State.EventSystem.ActiveEvents)
    {
        if (Event.bResolved)
        {
            continue;
        }

        ++ActiveCrisisCount;
        const int32 TurnsRemaining = Event.DeadlineTurn > 0 ? FMath::Max(0, Event.DeadlineTurn - State.Turn) : -1;
        Lines.Add(GuidanceText(GuidanceLevel,
            FString::Printf(TEXT("%s\nDeadline: %s | Severity %d | Completion: %s\nTrigger: %s\nUnresolved penalty: %s"),
                *BuildGuidedEventHeaderText(State, Event),
                TurnsRemaining >= 0 ? *FString::Printf(TEXT("turn %d (%d turn(s) remaining)"), Event.DeadlineTurn, TurnsRemaining) : TEXT("none"),
                Event.Severity,
                *Event.CompletionState,
                *Event.TriggerReason,
                Event.UnresolvedPenaltySummary.IsEmpty() ? TEXT("No penalty text recorded.") : *Event.UnresolvedPenaltySummary),
            FString::Printf(TEXT("%s | deadline %s | severity %d"),
                *Event.Title,
                TurnsRemaining >= 0 ? *FString::Printf(TEXT("turn %d"), Event.DeadlineTurn) : TEXT("none"),
                Event.Severity),
            FString::Printf(TEXT("%s | severity %d"), *Event.Title, Event.Severity),
            Event.Title));

        if (Event.DeadlineTurn > 0 && TurnsRemaining <= 1)
        {
            AddSuggestedAction(FString::Printf(TEXT("Resolve urgent event before stepping time: %s."), *Event.Title));
        }
        else
        {
            AddSuggestedAction(FString::Printf(TEXT("Review active event choices for %s."), *Event.Title));
        }
    }
    if (ActiveCrisisCount == 0)
    {
        Lines.Add(TEXT("No unresolved event crises are currently pending."));
    }

    AddSection(TEXT("Risk Validation"));
    Lines.Add(BuildGuidedFailureWarningText(State));
    Lines.Add(FString::Printf(TEXT("Internal stage: %s | Causes: %s"), *State.FailureRisk.WarningLevel, State.FailureRisk.ActiveUnrestCauses.Num() > 0 ? *FString::Join(State.FailureRisk.ActiveUnrestCauses, TEXT(", ")) : TEXT("none recorded")));
    Lines.Add(FString::Printf(TEXT("Takeover stage: %s | Causes: %s"), *State.InvasionRisk.WarningLevel, State.InvasionRisk.ActiveInvasionCauses.Num() > 0 ? *FString::Join(State.InvasionRisk.ActiveInvasionCauses, TEXT(", ")) : TEXT("none recorded")));
    if (!State.ApprovalStability.Summary.IsEmpty())
    {
        Lines.Add(FString::Printf(TEXT("Approval/stability model: %s"), *State.ApprovalStability.Summary));
    }
    if (!State.FailureRisk.WarningLevel.Equals(TEXT("Stable"), ESearchCase::IgnoreCase))
    {
        AddSuggestedAction(TEXT("Stabilize internal risk first: lower unrest, raise stability, and answer the largest unrest source."));
    }
    if (!State.InvasionRisk.WarningLevel.Equals(TEXT("Stable"), ESearchCase::IgnoreCase))
    {
        AddSuggestedAction(TEXT("Reduce takeover risk: improve military readiness, diplomacy, or border event response."));
    }

    AddSection(TEXT("RTS Tactical Alerts"));
    Lines.Add(BuildRtsOfficeAlertText());
    if (State.RtsWorld.Backflow.PendingOutcomes.Num() > 0)
    {
        AddSuggestedAction(TEXT("Open the RTS command map or phone alerts to review pending tactical results before advancing time."));
    }

    AddSection(TEXT("Advisor Notes"));
    if (State.AdvisorSystem.Reports.Num() > 0)
    {
        const int32 VisibleReports = FMath::Min(State.AdvisorSystem.Reports.Num(), VisibleAdvisorReportCount(State.AdvisorSystem, Country));
        for (int32 Index = 0; Index < VisibleReports; ++Index)
        {
            const FDemocracyAdvisorReport& Report = State.AdvisorSystem.Reports[Index];
            AppendGuidedAdvisorReport(Lines, Report, GuidanceLevel);
            if (Report.Severity >= 60)
            {
                AddSuggestedAction(FString::Printf(TEXT("Meet with %s about %s."), *Report.AdvisorName, *Report.Category));
            }
        }
        if (VisibleReports < State.AdvisorSystem.Reports.Num())
        {
            Lines.Add(FString::Printf(TEXT("%d lower-priority advisor notes withheld by difficulty guidance."), State.AdvisorSystem.Reports.Num() - VisibleReports));
        }
    }
    else
    {
        Lines.Add(TEXT("No advisor reports are available yet. Step the simulation or open the phone to refresh advisory output."));
        AddSuggestedAction(TEXT("Open the phone to refresh advisor warnings."));
    }

    AddSection(TEXT("Recent Decisions"));
    if (State.DecisionHistory.Records.Num() > 0)
    {
        const int32 FirstIndex = FMath::Max(0, State.DecisionHistory.Records.Num() - 6);
        for (int32 Index = State.DecisionHistory.Records.Num() - 1; Index >= FirstIndex; --Index)
        {
            const FDemocracyDecisionRecordState& Record = State.DecisionHistory.Records[Index];
            Lines.Add(GuidanceText(GuidanceLevel,
                FString::Printf(TEXT("Turn %d | %s | %s\nDecision: %s\nConsequence: %s\nAfter: approval %d, stability %d, unrest %d, treasury %d, economy %d, military %d | Severity %d"),
                    Record.Turn,
                    *Record.Category,
                    *Record.DecisionTitle,
                    *Record.DecisionDetail,
                    *Record.ConsequenceSummary,
                    Record.ApprovalAfter,
                    Record.StabilityAfter,
                    Record.UnrestAfter,
                    Record.TreasuryAfter,
                    Record.EconomyAfter,
                    Record.MilitaryAfter,
                    Record.Severity),
                FString::Printf(TEXT("Turn %d | %s | %s\n%s"), Record.Turn, *Record.Category, *Record.DecisionTitle, *Record.ConsequenceSummary),
                FString::Printf(TEXT("Turn %d | %s | %s"), Record.Turn, *Record.Category, *Record.DecisionTitle),
                FString::Printf(TEXT("Turn %d | %s"), Record.Turn, *Record.DecisionTitle)));
        }
    }
    else
    {
        Lines.Add(TEXT("No major decisions have been logged yet."));
    }

    AddSection(TEXT("Meetings And Public Messaging"));
    Lines.Add(State.MeetingSystem.LastMeetingSummary.IsEmpty() ? TEXT("No meetings have been held yet.") : State.MeetingSystem.LastMeetingSummary);
    Lines.Add(State.PressOffice.LastAnnouncementSummary.IsEmpty() ? TEXT("No press releases have been made yet.") : State.PressOffice.LastAnnouncementSummary);
    if (State.PressOffice.Credibility < 45)
    {
        AddSuggestedAction(TEXT("Use a truthful, substantive press release to repair credibility before making major claims."));
    }
    if (State.MeetingSystem.AdvisorCoordination < 45)
    {
        AddSuggestedAction(TEXT("Hold an advisor meeting to improve coordination around the highest-severity issue."));
    }

    AddSection(TEXT("Suggested Next Actions"));
    if (State.ResourceChains.TotalShortagePressure >= 35)
    {
        AddSuggestedAction(TEXT("Open Resources and address the largest food, water, fuel, wood, or metals shortage."));
    }
    if (State.EconomyBudget.bSpendingLimited || State.EconomyBudget.CreditStress >= 45)
    {
        AddSuggestedAction(TEXT("Open Budget and reduce spending pressure, debt stress, or inflation before expanding programs."));
    }
    if (State.DevelopmentSystem.ActiveFocus.IsEmpty() || State.DevelopmentSystem.Tracks.Num() == 0)
    {
        AddSuggestedAction(TEXT("Open Technology / Development and choose a long-term focus."));
    }
    if (SuggestedActions.Num() == 0)
    {
        SuggestedActions.Add(TEXT("No emergency action required. Continue with a controlled simulation tick, then review events and advisors again."));
    }
    const int32 MaxActions = IsDetailedGuidance(GuidanceLevel) ? 8 : ((!IsLimitedGuidance(GuidanceLevel) && !IsMinimalGuidance(GuidanceLevel)) ? 5 : 3);
    for (int32 Index = 0; Index < FMath::Min(MaxActions, SuggestedActions.Num()); ++Index)
    {
        Lines.Add(FString::Printf(TEXT("%d. %s"), Index + 1, *SuggestedActions[Index]));
    }
    if (SuggestedActions.Num() > MaxActions)
    {
        Lines.Add(FString::Printf(TEXT("%d additional lower-priority actions hidden by difficulty guidance."), SuggestedActions.Num() - MaxActions));
    }

    return FString::Join(Lines, TEXT("\n"));
}FString ALoginHUD::BuildResourceStatusText() const
{
    if (!bHasLoadedRuntimeState)
    {
        return TEXT("No resources loaded.");
    }

    const FDemocracyResourceInventory& Resources = LoadedSaveState.RuntimeState.PlayerCountry.Resources;
    return FString::Printf(
        TEXT("Food %d | Gas/Oil %d | Wood %d | Metals %d | Water %d"),
        Resources.Food,
        Resources.GasOil,
        Resources.Wood,
        Resources.Metals,
        Resources.Water);
}


FString ALoginHUD::BuildResourceChainStatusText() const
{
    if (!bHasLoadedRuntimeState)
    {
        return TEXT("No resource chain loaded.");
    }
    return BuildResourceChainSummaryText(LoadedSaveState.RuntimeState.ResourceChains);
}

FString ALoginHUD::BuildPolicyStatusText() const
{
    if (!bHasLoadedRuntimeState)
    {
        return TEXT("No policies loaded.");
    }

    const FDemocracyPolicyState& Policies = LoadedSaveState.RuntimeState.PlayerCountry.Policies;
    const FString EffectsText = Policies.ActivePolicyEffects.Num() > 0
        ? FString::Join(Policies.ActivePolicyEffects, TEXT("\n"))
        : JoinPolicyEffects(Policies);
    const FString RuleText = Policies.PolicyRuleStatus.Num() > 0
        ? FString::Join(Policies.PolicyRuleStatus, TEXT("\n"))
        : FString::Join(BuildPolicyRuleStatusLines(LoadedSaveState.RuntimeState), TEXT("\n"));

    return FString::Printf(
        TEXT("Economic: %s\nEnvironment: %s\nMilitary: %s\nDiplomacy: %s\nCivil: %s\nChanges: %d\nLast: %s\n\nActive effects:\n%s\n\nUnlocks / conflicts / cooldowns:\n%s"),
        *Policies.EconomicPolicy,
        *Policies.EnvironmentalPolicy,
        *Policies.MilitaryPolicy,
        *Policies.DiplomacyPolicy,
        *Policies.CivilPolicy,
        Policies.PolicyChangeCount,
        *Policies.LastPolicyChangeSummary,
        *EffectsText,
        *RuleText);}
FString ALoginHUD::BuildEconomyBudgetStatusText() const
{
    if (!bHasLoadedRuntimeState)
    {
        return TEXT("No economy budget loaded.");
    }
    return BuildEconomyBudgetSummaryText(LoadedSaveState.RuntimeState.EconomyBudget);
}
FString ALoginHUD::BuildDemographicsStatusText() const
{
    if (!bHasLoadedRuntimeState)
    {
        return TEXT("No demographics loaded.");
    }
    return BuildDemographicsSummaryText(LoadedSaveState.RuntimeState.Demographics);
}
FString ALoginHUD::BuildEventStatusText() const
{
    if (!bHasLoadedRuntimeState)
    {
        return TEXT("No event system loaded.");
    }

    TArray<FString> Lines;
    const FDemocracyEventSystemState& EventSystem = LoadedSaveState.RuntimeState.EventSystem;
    Lines.Add(BuildEventSummaryText(EventSystem));
    for (const FDemocracyActiveEventState& Event : EventSystem.ActiveEvents)
    {
        if (!Event.bResolved)
        {
            Lines.Add(FString::Printf(TEXT("%s: %s | Severity %d"), *Event.EventType, *Event.Title, Event.Severity));
        }
    }
    return FString::Join(Lines, TEXT("\n"));
}
FString ALoginHUD::BuildDepartmentStatusText() const
{
    if (!bHasLoadedRuntimeState)
    {
        return TEXT("No departments loaded.");
    }
    return BuildDepartmentSummaryText(LoadedSaveState.RuntimeState.Departments);
}
FString ALoginHUD::BuildDevelopmentStatusText() const
{
    if (!bHasLoadedRuntimeState)
    {
        return TEXT("No development system loaded.");
    }
    return BuildDevelopmentSummaryText(LoadedSaveState.RuntimeState.DevelopmentSystem);
}
FString ALoginHUD::BuildApprovalStabilityStatusText() const
{
    if (!bHasLoadedRuntimeState)
    {
        return TEXT("No approval/stability cause model loaded.");
    }
    return BuildApprovalStabilitySummaryText(LoadedSaveState.RuntimeState.ApprovalStability);
}
FString ALoginHUD::BuildPressOfficeStatusText() const
{
    if (!bHasLoadedRuntimeState)
    {
        return TEXT("No press office loaded.");
    }
    return BuildPressOfficeSummaryText(LoadedSaveState.RuntimeState.PressOffice);
}
FString ALoginHUD::BuildMeetingSystemStatusText() const
{
    if (!bHasLoadedRuntimeState)
    {
        return TEXT("No meeting system loaded.");
    }
    return BuildMeetingSystemSummaryText(LoadedSaveState.RuntimeState.MeetingSystem);
}
FString ALoginHUD::BuildDecisionHistoryStatusText() const
{
    if (!bHasLoadedRuntimeState)
    {
        return TEXT("No decision history loaded.");
    }
    return BuildDecisionHistorySummaryText(LoadedSaveState.RuntimeState.DecisionHistory);
}
FString ALoginHUD::BuildAdvisorWarningText() const
{
    if (!bHasLoadedRuntimeState)
    {
        return TEXT("No runtime state loaded.");
    }

    const FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    const FDemocracyAdvisorSystemState& AdvisorSystem = State.AdvisorSystem;
    const FString GuidanceLevel = AdvisorSystem.GuidanceLevel.IsEmpty() ? AdvisorGuidanceForDifficultyScore(State.PlayerCountry.CountrySizeScore) : AdvisorSystem.GuidanceLevel;
    TArray<FString> Lines;
    Lines.Add(FString::Printf(TEXT("Advisor guidance: %s | active advisors: %d | updated turn: %d"), *GuidanceLevel, AdvisorSystem.AdvisorCount, AdvisorSystem.LastUpdatedTurn));
    Lines.Add(RuntimeGuidanceSummary(State));

    if (AdvisorSystem.Reports.Num() > 0)
    {
        const int32 VisibleReports = FMath::Min(AdvisorSystem.Reports.Num(), VisibleAdvisorReportCount(AdvisorSystem, State.PlayerCountry));
        for (int32 Index = 0; Index < VisibleReports; ++Index)
        {
            AppendGuidedAdvisorReport(Lines, AdvisorSystem.Reports[Index], GuidanceLevel);
        }
        if (VisibleReports < AdvisorSystem.Reports.Num())
        {
            Lines.Add(FString::Printf(TEXT("\n%d lower-priority reports withheld by difficulty guidance."), AdvisorSystem.Reports.Num() - VisibleReports));
        }
    }
    else
    {
        Lines.Add(IsMinimalGuidance(GuidanceLevel) ? TEXT("No advisor signal available.") : TEXT("No structured advisor reports are available yet. Step the simulation once or create a new state."));
    }

    Lines.Add(TEXT("\nApproval / stability causes:"));
    Lines.Add(State.ApprovalStability.Summary);
    if (IsDetailedGuidance(GuidanceLevel))
    {
        Lines.Add(TEXT("Tutorial cue: top causes explain why approval, unrest, and stability moved. Fix high-severity causes first."));
    }

    Lines.Add(TEXT("\nRisk warnings:"));
    Lines.Add(TEXT("Internal stability risk:"));
    if (IsMinimalGuidance(GuidanceLevel))
    {
        Lines.Add(FString::Printf(TEXT("Assassination risk %d/%d."), State.FailureRisk.CurrentAssassinationRisk, State.FailureRisk.AssassinationRiskTrigger));
    }
    else
    {
        Lines.Append(State.FailureRisk.AdvisorWarnings);
        if (!IsLimitedGuidance(GuidanceLevel))
        {
            Lines.Append(State.FailureRisk.RecoveryTips);
        }
    }

    Lines.Add(TEXT("Foreign takeover risk:"));
    if (IsMinimalGuidance(GuidanceLevel))
    {
        Lines.Add(FString::Printf(TEXT("Takeover risk %d/%d."), State.InvasionRisk.CurrentInvasionRisk, State.InvasionRisk.InvasionRiskTrigger));
    }
    else
    {
        Lines.Append(State.InvasionRisk.AdvisorWarnings);
        if (!IsLimitedGuidance(GuidanceLevel))
        {
            Lines.Append(State.InvasionRisk.RecoveryTips);
        }
    }

    Lines.Add(TEXT("\nRTS tactical alerts:"));
    Lines.Add(BuildRtsOfficeAlertText());
    return FString::Join(Lines, TEXT("\n"));
}
FString ALoginHUD::BuildTimeControlStatusText() const
{
    if (!bHasLoadedRuntimeState)
    {
        return TEXT("No runtime state loaded.");
    }

    const FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    const float TickInterval = GetCurrentSimulationTickInterval();
    const float TicksPerMinute = TickInterval > 0.0f ? 60.0f / TickInterval : 0.0f;
    const float TurnInterval = TickInterval * 3.0f;
    const FString SpeedLabel = TickInterval >= 9.5f ? TEXT("Slow") : (TickInterval >= 4.5f ? TEXT("Default") : (TickInterval >= 2.0f ? TEXT("Fast") : TEXT("Very Fast")));

    return FString::Printf(
        TEXT("%s | speed %s | cadence %.2f sec/tick | %.1f ticks/min | 1 turn every %.2f sec | autosave every 3 ticks | total ticks %d | RTS seconds %d"),
        State.bPaused ? TEXT("Paused") : TEXT("Running"),
        *SpeedLabel,
        TickInterval,
        TicksPerMinute,
        TurnInterval,
        SimulationTickCount,
        State.RtsWorld.SimulationSecond);
}

float ALoginHUD::GetCurrentSimulationTickInterval() const
{
    if (!bHasLoadedRuntimeState)
    {
        return 5.0f;
    }

    return FMath::Clamp(LoadedSaveState.RuntimeState.RealTimeTickSeconds > 0.0f ? LoadedSaveState.RuntimeState.RealTimeTickSeconds : 5.0f, 1.25f, 10.0f);
}

void ALoginHUD::ApplySimulationTickInterval(float NewIntervalSeconds)
{
    if (!bHasLoadedRuntimeState)
    {
        return;
    }

    UWorld* World = GetWorld();
    FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    const bool bWasRunning = !State.bPaused;
    State.RealTimeTickSeconds = FMath::Clamp(NewIntervalSeconds, 1.25f, 10.0f);
    SimulationTickSummary = BuildSimulationStatusText();
    LastSaveStatus = FString::Printf(TEXT("Simulation cadence set to %.2f seconds per tick."), State.RealTimeTickSeconds);

    if (World && bWasRunning)
    {
        World->GetTimerManager().ClearTimer(SimulationTickTimerHandle);
        World->GetTimerManager().SetTimer(
            SimulationTickTimerHandle,
            this,
            &ALoginHUD::RunSimulationTick,
            State.RealTimeTickSeconds,
            true);
    }

    RefreshLoginWidget();
}
void ALoginHUD::StartSimulationTimer()
{
    UWorld* World = GetWorld();
    if (!World || !bHasLoadedRuntimeState)
    {
        return;
    }

    LoadedSaveState.RuntimeState.bPaused = false;
    LoadedSaveState.RuntimeState.Phase = TEXT("Prototype Simulation Running");
    LoadedSaveState.RuntimeState.RealTimeTickSeconds = GetCurrentSimulationTickInterval();
    SimulationTickSummary = BuildSimulationStatusText();

    World->GetTimerManager().ClearTimer(SimulationTickTimerHandle);
    World->GetTimerManager().SetTimer(
        SimulationTickTimerHandle,
        this,
        &ALoginHUD::RunSimulationTick,
        LoadedSaveState.RuntimeState.RealTimeTickSeconds,
        true);

    RefreshLoginWidget();
}

void ALoginHUD::StopSimulationTimer()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(SimulationTickTimerHandle);
    }

    if (bHasLoadedRuntimeState)
    {
        LoadedSaveState.RuntimeState.bPaused = true;
        LoadedSaveState.RuntimeState.Phase = TEXT("Prototype Simulation Paused");
        SimulationTickSummary = BuildSimulationStatusText();
    }
}

void ALoginHUD::RunSimulationTick()
{
    if (!bHasLoadedRuntimeState || LoadedSaveState.RuntimeState.bPaused)
    {
        return;
    }

    FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    FDemocracyCountryState& Country = State.PlayerCountry;
    FDemocracyResourceInventory& Resources = Country.Resources;
    const FSimulationTickSnapshot BeforeTick = MakeSimulationTickSnapshot(State);

    ++SimulationTickCount;
    ++State.RtsWorld.SimulationSecond;
    ++State.RtsWorld.RtsTickCount;
    State.RtsWorld.RtsTicksPerSimulationTurn = FMath::Max(1, State.RtsWorld.RtsTicksPerSimulationTurn);
    State.RtsWorld.RtsTicksPerConstructionTurn = FMath::Max(1, State.RtsWorld.RtsTicksPerConstructionTurn);
    if (State.RtsWorld.RtsTickCount % State.RtsWorld.RtsTicksPerSimulationTurn == 0)
    {
        ++State.Turn;
    }
    const bool bAdvancedTurn = State.Turn != BeforeTick.Turn;
    const bool bAdvancedRtsConstructionTick = State.RtsWorld.RtsTickCount % State.RtsWorld.RtsTicksPerConstructionTurn == 0;

    FDemocracyRtsResourceCollectionState& RtsCollection = State.RtsWorld.ResourceCollection;
    int32 BuildingFood = 0;
    int32 BuildingFuel = 0;
    int32 BuildingWood = 0;
    int32 BuildingMetals = 0;
    for (const FDemocracyRtsBuildingState& Building : State.RtsWorld.CityBase.Buildings)
    {
        if (!Building.bConstructed || Building.bDisabled)
        {
            continue;
        }

        if (Building.ResourceFocus.Equals(TEXT("Food"), ESearchCase::IgnoreCase)) { BuildingFood += Building.ProductionPerTick; }
        else if (Building.ResourceFocus.Equals(TEXT("Fuel"), ESearchCase::IgnoreCase)) { BuildingFuel += Building.ProductionPerTick; }
        else if (Building.ResourceFocus.Equals(TEXT("Wood"), ESearchCase::IgnoreCase)) { BuildingWood += Building.ProductionPerTick; }
        else if (Building.ResourceFocus.Equals(TEXT("Metals"), ESearchCase::IgnoreCase)) { BuildingMetals += Building.ProductionPerTick; }
    }

    int32 ProvinceFood = 0;
    int32 ProvinceFuel = 0;
    int32 ProvinceWood = 0;
    int32 ProvinceMetals = 0;
    TArray<FString> CollectionSources;
    for (const FDemocracyProvinceOwnershipState& Province : State.RtsWorld.Ownership.Provinces)
    {
        if (!Province.bPlayerControlled)
        {
            continue;
        }

        const int32 ProvinceYield = FMath::Max(1, Province.StrategicValue / 4);
        if (Province.ResourceFocus.Equals(TEXT("Food"), ESearchCase::IgnoreCase)) { ProvinceFood += ProvinceYield; }
        else if (Province.ResourceFocus.Equals(TEXT("Fuel"), ESearchCase::IgnoreCase)) { ProvinceFuel += ProvinceYield; }
        else if (Province.ResourceFocus.Equals(TEXT("Wood"), ESearchCase::IgnoreCase)) { ProvinceWood += ProvinceYield; }
        else if (Province.ResourceFocus.Equals(TEXT("Metals"), ESearchCase::IgnoreCase)) { ProvinceMetals += ProvinceYield; }
        if (CollectionSources.Num() < 8)
        {
            CollectionSources.Add(FString::Printf(TEXT("%s: %s %+d"), *Province.ProvinceName, *Province.ResourceFocus, ProvinceYield));
        }
    }

    const int32 CollectionPenalty = FMath::Clamp(State.RtsWorld.Backflow.ResourceDisruptionPressure / 20, 0, 8);
    RtsCollection.LastUpdatedTurn = State.Turn;
    RtsCollection.FoodFromBuildings = BuildingFood;
    RtsCollection.FuelFromBuildings = BuildingFuel;
    RtsCollection.WoodFromBuildings = BuildingWood;
    RtsCollection.MetalsFromBuildings = BuildingMetals;
    RtsCollection.FoodFromProvinces = ProvinceFood;
    RtsCollection.FuelFromProvinces = ProvinceFuel;
    RtsCollection.WoodFromProvinces = ProvinceWood;
    RtsCollection.MetalsFromProvinces = ProvinceMetals;
    RtsCollection.DisruptionPenalty = CollectionPenalty;
    RtsCollection.FoodSentToSimulation = FMath::Max(0, BuildingFood + ProvinceFood - CollectionPenalty);
    RtsCollection.FuelSentToSimulation = FMath::Max(0, BuildingFuel + ProvinceFuel - CollectionPenalty);
    RtsCollection.WoodSentToSimulation = FMath::Max(0, BuildingWood + ProvinceWood - CollectionPenalty);
    RtsCollection.MetalsSentToSimulation = FMath::Max(0, BuildingMetals + ProvinceMetals - CollectionPenalty);
    RtsCollection.CollectionSources = CollectionSources;
    RtsCollection.Summary = FString::Printf(TEXT("RTS collection sent food %+d, fuel %+d, wood %+d, metals %+d to simulation. Disruption penalty %d."), RtsCollection.FoodSentToSimulation, RtsCollection.FuelSentToSimulation, RtsCollection.WoodSentToSimulation, RtsCollection.MetalsSentToSimulation, RtsCollection.DisruptionPenalty);
    Resources.Food = FMath::Max(0, Resources.Food + RtsCollection.FoodSentToSimulation);
    Resources.GasOil = FMath::Max(0, Resources.GasOil + RtsCollection.FuelSentToSimulation);
    Resources.Wood = FMath::Max(0, Resources.Wood + RtsCollection.WoodSentToSimulation);
    Resources.Metals = FMath::Max(0, Resources.Metals + RtsCollection.MetalsSentToSimulation);

    if (bAdvancedRtsConstructionTick)
    {
        int32 CompletedConstructionCount = 0;
        int32 ActiveBuildCount = 0;
        int32 ActiveUpgradeCount = 0;
        for (FDemocracyRtsConstructionQueueEntryState& QueueEntry : State.RtsWorld.CityBase.ConstructionQueue)
        {
            if (QueueEntry.bCancelled || QueueEntry.bComplete)
            {
                continue;
            }

            QueueEntry.TurnsRemaining = FMath::Max(0, QueueEntry.TurnsRemaining - 1);
            if (QueueEntry.TurnsRemaining == 0)
            {
                QueueEntry.bComplete = true;
                ++CompletedConstructionCount;
                bool bUpdatedExistingBuilding = false;
                for (FDemocracyRtsBuildingState& Building : State.RtsWorld.CityBase.Buildings)
                {
                    if (Building.BuildingId.Equals(QueueEntry.BuildingId, ESearchCase::IgnoreCase))
                    {
                        Building.bConstructed = true;
                        Building.bUpgradeQueued = false;
                        Building.Level = FMath::Max(Building.Level, QueueEntry.TargetLevel);
                        Building.ProductionPerTick += QueueEntry.QueueType.Equals(TEXT("Upgrade"), ESearchCase::IgnoreCase) ? 2 : 0;
                        Building.MaxHealth += QueueEntry.QueueType.Equals(TEXT("Upgrade"), ESearchCase::IgnoreCase) ? 25 : 0;
                        Building.CurrentHealth = Building.MaxHealth;
                        Building.Status = TEXT("Operational");
                        bUpdatedExistingBuilding = true;
                        break;
                    }
                }
                if (!bUpdatedExistingBuilding && QueueEntry.QueueType.Equals(TEXT("Build"), ESearchCase::IgnoreCase))
                {
                    FDemocracyRtsBuildingState NewBuilding = MakeRtsPlaceholderBuildingForSlot(State.RtsWorld.CityBase.Buildings.Num(), TEXT("Supply"));
                    NewBuilding.BuildingId = QueueEntry.BuildingId;
                    NewBuilding.DisplayName = QueueEntry.DisplayName.Replace(TEXT("Build "), TEXT(""));
                    NewBuilding.Level = FMath::Max(1, QueueEntry.TargetLevel);
                    NewBuilding.bConstructed = true;
                    NewBuilding.Status = TEXT("Operational");
                    NewBuilding.MaxHealth = 125;
                    NewBuilding.CurrentHealth = NewBuilding.MaxHealth;
                    State.RtsWorld.CityBase.Buildings.Add(NewBuilding);
                }
            }
            else if (QueueEntry.QueueType.Equals(TEXT("Upgrade"), ESearchCase::IgnoreCase))
            {
                ++ActiveUpgradeCount;
            }
            else
            {
                ++ActiveBuildCount;
            }
        }

        State.RtsWorld.CityBase.BuildQueueCount = ActiveBuildCount + ActiveUpgradeCount;
        State.RtsWorld.CityBase.UpgradeQueueCount = ActiveUpgradeCount;
        if (CompletedConstructionCount > 0)
        {
            State.RtsWorld.CityBase.RuntimeNotes.Add(FString::Printf(TEXT("Turn %d completed %d RTS construction queue item(s)."), State.Turn, CompletedConstructionCount));
        }
    }

    const int32 DifficultyScore = FMath::Clamp(Country.CountrySizeScore, 1, 4);
    const int32 FoodUse = 6 + DifficultyScore * 3;
    const int32 WaterUse = 5 + DifficultyScore * 2;
    const int32 GasUse = 1 + DifficultyScore;
    const int32 WoodUse = 2 + DifficultyScore;
    const int32 MetalsUse = 1 + DifficultyScore;

    const FPolicyTickModifiers PolicyModifiers = BuildPolicyModifiers(Country.Policies);
    RecalculateEconomyBudget(State);
    RecalculateResourceProductionChains(State, PolicyModifiers, FoodUse, WaterUse, GasUse, WoodUse, MetalsUse);
    ApplyResourceShortageEffects(State);
    TickRtsMovementOrdersAndSupply(State, bAdvancedTurn);
    State.RtsWorld.TickSummary = FString::Printf(TEXT("RTS tick %d | office turn cadence %d | construction cadence %d | movement advanced %s | construction advanced %s | active orders %d | battles %d."), State.RtsWorld.RtsTickCount, State.RtsWorld.RtsTicksPerSimulationTurn, State.RtsWorld.RtsTicksPerConstructionTurn, bAdvancedTurn ? TEXT("yes") : TEXT("no"), bAdvancedRtsConstructionTick ? TEXT("yes") : TEXT("no"), State.RtsWorld.MovementOrders.FilterByPredicate([](const FDemocracyRtsMovementOrderState& Order) { return Order.bActive && !Order.bComplete && !Order.bCancelled; }).Num(), State.RtsWorld.BattleHistory.Num());
    RefreshRtsFogOfWar(State);
    RefreshRtsHudState(State);
    const bool bRtsBackflowApplied = ApplyPendingRtsBackflow(State);

    const bool bFoodShortage = GetResourceChainShortage(State.ResourceChains, TEXT("Food")) > 0;
    const bool bWaterShortage = GetResourceChainShortage(State.ResourceChains, TEXT("Water")) > 0;
    const bool bFuelShortage = GetResourceChainShortage(State.ResourceChains, TEXT("Fuel")) > 0 && DifficultyScore >= 3;
    const bool bWoodShortage = GetResourceChainShortage(State.ResourceChains, TEXT("Wood")) > 0;
    const bool bMetalsShortage = GetResourceChainShortage(State.ResourceChains, TEXT("Metals")) > 0;
    RecalculateDepartments(State);
    TickDevelopmentSystem(State);
    const int32 DefenseEffectiveness = GetDepartmentEffectiveness(State.Departments, TEXT("Defense"));
    const int32 TreasuryEffectiveness = GetDepartmentEffectiveness(State.Departments, TEXT("Treasury"));
    const int32 AgricultureEffectiveness = GetDepartmentEffectiveness(State.Departments, TEXT("Agriculture"));
    const int32 EnergyEffectiveness = GetDepartmentEffectiveness(State.Departments, TEXT("Energy"));
    const int32 HealthEffectiveness = GetDepartmentEffectiveness(State.Departments, TEXT("Health"));
    const int32 EducationEffectiveness = GetDepartmentEffectiveness(State.Departments, TEXT("Education"));
    const int32 InfrastructureEffectiveness = GetDepartmentEffectiveness(State.Departments, TEXT("Infrastructure"));
    const int32 DepartmentSupport = FMath::Clamp(State.Departments.Coordination / 30, 0, 3);
    const int32 ShortagePressure = FMath::Clamp(State.ResourceChains.TotalShortagePressure / 18 + (bFoodShortage ? 1 : 0) + (bWaterShortage ? 1 : 0) - DepartmentSupport, 0, 12);
    Country.Treasury = FMath::Max(0, Country.Treasury + 8 - DifficultyScore * 3 - ShortagePressure + PolicyModifiers.TreasuryDelta + TreasuryEffectiveness / 35);
    Country.EconomicHealth = FMath::Clamp(Country.EconomicHealth + PolicyModifiers.EconomicDelta - ShortagePressure - (bFuelShortage ? 1 : 0) - (bMetalsShortage ? 1 : 0) + (Country.Treasury > 250 ? 1 : 0) + (TreasuryEffectiveness + EducationEffectiveness) / 70, 0, 100);
    Country.DiplomaticStanding = FMath::Clamp(Country.DiplomaticStanding + PolicyModifiers.DiplomacyDelta + (Country.PublicApproval > 65 ? 1 : 0) - (Country.Unrest > 60 ? 1 : 0), 0, 100);
    Country.Infrastructure = FMath::Clamp(Country.Infrastructure + PolicyModifiers.InfrastructureDelta - (bWoodShortage ? 1 : 0) + InfrastructureEffectiveness / 45, 0, 100);
    Country.EnvironmentalHealth = FMath::Clamp(Country.EnvironmentalHealth + PolicyModifiers.EnvironmentDelta, 0, 100);
    Country.PublicApproval = FMath::Clamp(Country.PublicApproval + 1 - ShortagePressure + PolicyModifiers.ApprovalDelta + (Country.EconomicHealth > 65 ? 1 : 0) + HealthEffectiveness / 60, 0, 100);
    Country.Stability = FMath::Clamp(Country.Stability + (Country.PublicApproval >= 55 ? 1 : 0) - ShortagePressure + PolicyModifiers.StabilityDelta + (Country.DiplomaticStanding > 70 ? 1 : 0), 0, 100);
    Country.Unrest = FMath::Clamp(Country.Unrest + ShortagePressure + (Country.PublicApproval < 40 ? 1 : 0) - (Country.Stability > 65 ? 1 : 0) + PolicyModifiers.UnrestDelta, 0, 100);
    Country.MilitaryReadiness = FMath::Clamp(Country.MilitaryReadiness + (Resources.Metals > 80 ? 1 : 0) - (bFuelShortage ? 2 : 0) - (bMetalsShortage ? 1 : 0) + PolicyModifiers.MilitaryDelta + DefenseEffectiveness / 45, 0, 100);
    Resources.Food = FMath::Max(0, Resources.Food + AgricultureEffectiveness / 45);
    Resources.GasOil = FMath::Max(0, Resources.GasOil + EnergyEffectiveness / 50);
    RecalculateDemographics(State);
    Country.PublicApproval = FMath::Clamp((Country.PublicApproval + State.Demographics.AverageGroupApproval + State.Demographics.AverageRegionalApproval) / 3, 0, 100);
    Country.Unrest = FMath::Clamp(Country.Unrest + FMath::Max(0, State.Demographics.DemographicUnrestPressure - 35) / 10 + FMath::Max(0, State.Demographics.NationalNeedsPressure - 35) / 12, 0, 100);
    Country.Stability = FMath::Clamp(Country.Stability - FMath::Max(0, State.Demographics.DemographicUnrestPressure - 45) / 12, 0, 100);
    RecalculateApprovalStability(State);
    const int32 CauseApprovalDelta = FMath::Clamp(State.ApprovalStability.NetApprovalPressure / 25, -4, 3);
    const int32 CauseUnrestDelta = FMath::Clamp(State.ApprovalStability.NetUnrestPressure / 22, -2, 5);
    const int32 CauseStabilityDelta = FMath::Clamp(State.ApprovalStability.NetStabilityPressure / 25, -4, 3);
    Country.PublicApproval = FMath::Clamp(Country.PublicApproval + CauseApprovalDelta, 0, 100);
    Country.Unrest = FMath::Clamp(Country.Unrest + CauseUnrestDelta, 0, 100);
    Country.Stability = FMath::Clamp(Country.Stability + CauseStabilityDelta, 0, 100);
    RecalculateApprovalStability(State);

    State.FailureRisk.CurrentAssassinationRisk = FMath::Clamp(
        State.FailureRisk.CurrentAssassinationRisk + FMath::Max(0, Country.Unrest - 45) / 12 + FMath::Max(0, 45 - Country.Stability) / 12,
        0,
        State.FailureRisk.AssassinationRiskTrigger);
    State.InvasionRisk.CurrentInvasionRisk = FMath::Clamp(
        State.InvasionRisk.CurrentInvasionRisk + FMath::Max(0, 45 - Country.MilitaryReadiness) / 10 + FMath::Max(0, 45 - Country.DiplomaticStanding) / 12 + DifficultyScore - 1 + PolicyModifiers.InvasionRiskDelta,
        0,
        State.InvasionRisk.InvasionRiskTrigger);

    GenerateSimulationEvents(State);
    const bool bEventDeadlineApplied = ApplyExpiredEventFollowUps(State);

    State.AdvisorSystem.GuidanceLevel = AdvisorGuidanceForDifficultyScore(DifficultyScore);
    State.AdvisorSystem.LastUpdatedTurn = State.Turn;
    if (State.AdvisorSystem.AdvisorCount <= 0)
    {
        State.AdvisorSystem.AdvisorCount = FMath::Clamp(6 - DifficultyScore, 1, 5);
    }
    State.AdvisorSystem.Reports = GenerateAdvisorReports(State);
    InitializeDiplomacyMatrixIfMissing(State);
    RefreshObjectiveState(State, LoadedSaveState.Mode.IsEmpty() ? TEXT("SinglePlayer") : LoadedSaveState.Mode);
    RefreshWarConflictState(State);
    RefreshSimulationToRtsContract(State);

    if (!bEventDeadlineApplied && !State.Phase.Equals(TEXT("Event Decision Pending"), ESearchCase::IgnoreCase))
    {
        State.Phase = TEXT("Prototype Simulation Running");
    }
    LoadedSaveSummary = LoadedSaveState.ToSummaryText();
    SimulationTickSummary = BuildTickResultFeedback(BeforeTick, State, SimulationTickCount, bAdvancedTurn, bEventDeadlineApplied);
    if (bRtsBackflowApplied)
    {
        SimulationTickSummary += FString::Printf(TEXT("\nRTS Backflow: %s"), *State.RtsWorld.Backflow.LastOutcomeSummary);
    }

    UE_LOG(LogTemp, Log, TEXT("Simulation tick result:\n%s\n%s"), *SimulationTickSummary, *BuildResourceStatusText());

    if (EvaluateFailState())
    {
        return;
    }

    if (SimulationTickCount % 3 == 0 && !LoadedSavePath.IsEmpty())
    {
        FString AutosaveError;
        if (FDemocracySaveGameRuntime::SaveSinglePlayerAutosave(LoadedSaveState, AutosaveError))
        {
            LoadedSaveSummary = LoadedSaveState.ToSummaryText();
            LastSaveStatus = FString::Printf(TEXT("Autosaved protected runtime state at %s."), *LoadedSaveState.LastPlayedAtUtc);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Autosave failed: %s"), *AutosaveError);
        }
    }

    if (CurrentScreen == ELoginFlowScreen::LoadedGame || CurrentScreen == ELoginFlowScreen::OfficeOpeningBriefing || CurrentScreen == ELoginFlowScreen::OfficeDashboard || CurrentScreen == ELoginFlowScreen::OfficeComputerMenu || CurrentScreen == ELoginFlowScreen::OfficePolicies || CurrentScreen == ELoginFlowScreen::OfficeEvents || CurrentScreen == ELoginFlowScreen::OfficeDemographics || CurrentScreen == ELoginFlowScreen::OfficeBudget || CurrentScreen == ELoginFlowScreen::OfficeResourceChains || CurrentScreen == ELoginFlowScreen::OfficeDepartments || CurrentScreen == ELoginFlowScreen::OfficeDevelopment || CurrentScreen == ELoginFlowScreen::OfficeApprovalStability || CurrentScreen == ELoginFlowScreen::OfficeDecisionHistory || CurrentScreen == ELoginFlowScreen::OfficeWorldRts || CurrentScreen == ELoginFlowScreen::OfficeAdvisorWarnings || CurrentScreen == ELoginFlowScreen::OfficeMeetingAdvisor || CurrentScreen == ELoginFlowScreen::OfficePressRelease)
    {
        RefreshLoginWidget();
    }
}

bool ALoginHUD::EvaluateFailState()
{
    if (!bHasLoadedRuntimeState)
    {
        return false;
    }

    FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    FDemocracyCountryState& Country = State.PlayerCountry;
    RefreshFailureValidationState(State);

    const int32 InternalPct = RiskPercent(State.FailureRisk.CurrentAssassinationRisk, State.FailureRisk.AssassinationRiskTrigger);
    const int32 TakeoverPct = RiskPercent(State.InvasionRisk.CurrentInvasionRisk, State.InvasionRisk.InvasionRiskTrigger);
    const int32 BorderPressure = EstimateBorderPressure(State);
    const bool bAssassinationTriggered =
        State.FailureRisk.bGameOverOnAssassination &&
        State.FailureRisk.CurrentAssassinationRisk >= State.FailureRisk.AssassinationRiskTrigger;
    const bool bCollapseTriggered =
        Country.PublicApproval <= 5 ||
        (Country.Treasury <= 0 && Country.EconomicHealth <= 8 && Country.Stability <= State.FailureRisk.StabilityCriticalThreshold) ||
        (State.Demographics.NationalNeedsPressure >= 95 && Country.Stability <= State.FailureRisk.StabilityCriticalThreshold) ||
        (Country.Stability <= 5 && Country.Unrest >= FMath::Max(85, State.FailureRisk.UnrestCriticalThreshold - 3));
    const bool bMajorInstabilityTriggered =
        !bCollapseTriggered &&
        ((Country.Stability <= State.FailureRisk.StabilityCriticalThreshold && Country.Unrest >= State.FailureRisk.UnrestWarningThreshold) ||
            Country.Unrest >= State.FailureRisk.UnrestCriticalThreshold ||
            (State.Demographics.DemographicUnrestPressure >= 92 && Country.Stability <= State.FailureRisk.StabilityWarningThreshold));
    const bool bForeignTakeoverTriggered =
        State.InvasionRisk.bGameOverOnTakeover &&
        (State.InvasionRisk.CurrentInvasionRisk >= State.InvasionRisk.InvasionRiskTrigger ||
            (Country.MilitaryReadiness <= State.InvasionRisk.MilitaryReadinessCriticalThreshold && TakeoverPct >= 55) ||
            (BorderPressure >= State.InvasionRisk.BorderPressureCriticalThreshold && Country.MilitaryReadiness <= State.InvasionRisk.MilitaryReadinessWarningThreshold) ||
            (Country.DiplomaticStanding <= 8 && TakeoverPct >= 50));

    if (!bAssassinationTriggered && !bCollapseTriggered && !bMajorInstabilityTriggered && !bForeignTakeoverTriggered)
    {
        return false;
    }

    StopSimulationTimer();
    State.bPaused = true;
    State.Phase = TEXT("Game Over");
    bInOfficeMode = false;

    FString ProtectedPath;
    FString ProtectedError;
    const bool bHasProtectedReload = !LoadedSavePath.IsEmpty() && FDemocracySaveGameRuntime::GetProtectedReloadSavePath(LoadedSavePath, ProtectedPath, ProtectedError);
    const FString ReloadLine = bHasProtectedReload
        ? FString::Printf(TEXT("Protected reload source is available: %s"), *ProtectedPath)
        : FString::Printf(TEXT("Protected reload source is not currently available. %s"), *ProtectedError);

    if (bForeignTakeoverTriggered)
    {
        GameOverReason = State.InvasionRisk.GameOverReason.IsEmpty() ? TEXT("Foreign Takeover") : State.InvasionRisk.GameOverReason;
        GameOverDetails = FString::Printf(
            TEXT("Foreign forces or pressure overcame the state. Stage %s. Military readiness %d/%d critical, diplomacy %d, border pressure %d/%d critical, takeover risk %d/%d (%d%%). Causes: %s. %s"),
            *State.InvasionRisk.WarningLevel,
            Country.MilitaryReadiness,
            State.InvasionRisk.MilitaryReadinessCriticalThreshold,
            Country.DiplomaticStanding,
            BorderPressure,
            State.InvasionRisk.BorderPressureCriticalThreshold,
            State.InvasionRisk.CurrentInvasionRisk,
            State.InvasionRisk.InvasionRiskTrigger,
            TakeoverPct,
            *BuildFailureCauseText(State.InvasionRisk.ActiveInvasionCauses),
            *ReloadLine);
    }
    else if (bAssassinationTriggered)
    {
        GameOverReason = State.FailureRisk.GameOverReason.IsEmpty() ? TEXT("Assassination") : State.FailureRisk.GameOverReason;
        GameOverDetails = FString::Printf(
            TEXT("Internal threat level reached the assassination trigger. Stage %s. Stability %d/%d critical, unrest %d/%d critical, assassination risk %d/%d (%d%%). Causes: %s. %s"),
            *State.FailureRisk.WarningLevel,
            Country.Stability,
            State.FailureRisk.StabilityCriticalThreshold,
            Country.Unrest,
            State.FailureRisk.UnrestCriticalThreshold,
            State.FailureRisk.CurrentAssassinationRisk,
            State.FailureRisk.AssassinationRiskTrigger,
            InternalPct,
            *BuildFailureCauseText(State.FailureRisk.ActiveUnrestCauses),
            *ReloadLine);
    }
    else if (bCollapseTriggered)
    {
        GameOverReason = TEXT("State Collapse");
        GameOverDetails = FString::Printf(
            TEXT("The government collapsed from combined legitimacy, economic, and public-need failure. Stage %s. Approval %d, stability %d/%d critical, unrest %d/%d critical, treasury %d, economy %d, needs pressure %d. Causes: %s. %s"),
            *State.FailureRisk.WarningLevel,
            Country.PublicApproval,
            Country.Stability,
            State.FailureRisk.StabilityCriticalThreshold,
            Country.Unrest,
            State.FailureRisk.UnrestCriticalThreshold,
            Country.Treasury,
            Country.EconomicHealth,
            State.Demographics.NationalNeedsPressure,
            *BuildFailureCauseText(State.FailureRisk.ActiveUnrestCauses),
            *ReloadLine);
    }
    else
    {
        GameOverReason = TEXT("Major Instability");
        GameOverDetails = FString::Printf(
            TEXT("The state became ungovernable before direct assassination or takeover. Stage %s. Stability %d/%d critical, unrest %d/%d critical, approval %d, demographic unrest %d. Causes: %s. %s"),
            *State.FailureRisk.WarningLevel,
            Country.Stability,
            State.FailureRisk.StabilityCriticalThreshold,
            Country.Unrest,
            State.FailureRisk.UnrestCriticalThreshold,
            Country.PublicApproval,
            State.Demographics.DemographicUnrestPressure,
            *BuildFailureCauseText(State.FailureRisk.ActiveUnrestCauses),
            *ReloadLine);
    }

    LogDecision(State, TEXT("Game Over"), GameOverReason, GameOverDetails, TEXT("Simulation stopped. Reload a protected previous save or return to the local save list."), 100, { TEXT("game-over"), GameOverReason, State.FailureRisk.WarningLevel, State.InvasionRisk.WarningLevel });
    LoadedSaveSummary = LoadedSaveState.ToSummaryText();
    SimulationTickSummary = BuildSimulationStatusText();
    LastSaveStatus = TEXT("Game over triggered. Current failed state remains in memory and will not be autosaved by the tick loop.");
    UE_LOG(LogTemp, Warning, TEXT("Game over triggered: %s - %s"), *GameOverReason, *GameOverDetails);
    ShowScreen(ELoginFlowScreen::GameOver);
    return true;
}bool ALoginHUD::EnterOfficePrototype(bool bShowOpeningBriefing)
{
    UWorld* World = GetWorld();
    APlayerController* PlayerController = GetOwningPlayerController();
    if (!World || !PlayerController)
    {
        LastSaveStatus = TEXT("Could not enter office prototype: missing world or player controller.");
        RefreshLoginWidget();
        return false;
    }

    if (!bOfficePrototypeSpawned)
    {
        OfficeLevelBuilder = World->SpawnActor<AOfficeLevelBuilder>();
        if (OfficeLevelBuilder.IsValid())
        {
            OfficeLevelBuilder->BuildOffice();
        }
        bOfficePrototypeSpawned = true;
    }

    if (!OfficePlayerPawn.IsValid())
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        OfficePlayerPawn = World->SpawnActor<AOfficePlayerPawn>(AOfficePlayerPawn::StaticClass(), FVector(0.0f, -260.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f), SpawnParams);
    }

    if (OfficePlayerPawn.IsValid())
    {
        OfficePlayerPawn->SetInvertLookY(bInvertLookY);
        PlayerController->Possess(OfficePlayerPawn.Get());
        PlayerController->SetInputMode(FInputModeGameOnly());
        PlayerController->bShowMouseCursor = false;
    }

    bInOfficeMode = true;
    bShowFirstLoginBriefing = bShowOpeningBriefing;
    if (bShowOpeningBriefing)
    {
        CurrentScreen = ELoginFlowScreen::OfficeOpeningBriefing;
        RefreshLoginWidget();
    }
    else
    {
        CurrentScreen = ELoginFlowScreen::OfficeNoOverlay;
        TearDownLoginWidget();
    }
    return true;
}

FString ALoginHUD::GetAddressTitleForGender(const FString& GenderName) const
{
    if (GenderName.Equals(TEXT("Male"), ESearchCase::IgnoreCase))
    {
        return TEXT("Mr. President");
    }
    if (GenderName.Equals(TEXT("Female"), ESearchCase::IgnoreCase))
    {
        return TEXT("Miss President");
    }
    return TEXT("President");
}

FString ALoginHUD::GetOpeningScriptText() const
{
    FString ScriptText;
    const FString ScriptPath = FPaths::ProjectContentDir() / TEXT("Story/OpeningScript_FirstLogin.txt");
    if (FFileHelper::LoadFileToString(ScriptText, *ScriptPath))
    {
        return ScriptText;
    }

    return TEXT("Opening script missing. Expected Content/Story/OpeningScript_FirstLogin.txt.");
}

FReply ALoginHUD::HandleSignInClicked()
{
    UE_LOG(LogTemp, Log, TEXT("Login flow: mock Sign In for '%s'. Routing directly to single-player saves for testing."), *MockUserName);
    ShowScreen(ELoginFlowScreen::LocalSaveSelection);
    return FReply::Handled();
}

FReply ALoginHUD::HandleSignUpClicked()
{
    UE_LOG(LogTemp, Log, TEXT("Login flow: mock Sign Up for '%s'. Routing directly to single-player saves for testing."), *MockUserName);
    ShowScreen(ELoginFlowScreen::LocalSaveSelection);
    return FReply::Handled();
}

FReply ALoginHUD::HandleSettingsClicked()
{
    ShowScreen(ELoginFlowScreen::Settings);
    return FReply::Handled();
}

FReply ALoginHUD::HandleBackToLoginClicked()
{
    ShowScreen(ELoginFlowScreen::Login);
    return FReply::Handled();
}

FReply ALoginHUD::HandleSinglePlayerClicked()
{
    ShowScreen(ELoginFlowScreen::LocalSaveSelection);
    return FReply::Handled();
}

FReply ALoginHUD::HandleMultiplayerClicked()
{
    UE_LOG(LogTemp, Log, TEXT("Login flow: Multiplayer is intentionally disabled until backend services are ready."));
    return FReply::Handled();
}

FReply ALoginHUD::HandleCreateNewStateClicked()
{
    ShowScreen(ELoginFlowScreen::DifficultySelection);
    return FReply::Handled();
}
FReply ALoginHUD::HandleSelectDifficulty(FString DifficultyName)
{
    PendingDifficulty = DifficultyName;
    PendingClimate.Empty();
    PendingLeaderGender.Empty();
    PendingAddressTitle.Empty();
    PendingStartingCountryName.Empty();
    PendingStartingCountrySearchText.Empty();
    PendingStartingCountryMapIndex = 0;
    PendingStartingCountryListScrollOffset = 0.0f;
    PendingNewCountrySetupScrollOffset = 0.0f;
    bNewCountryClimateExpanded = true;
    bNewCountryGenderExpanded = true;
    bNewCountryLocationExpanded = true;
    ShowScreen(ELoginFlowScreen::NewStateSetup);
    return FReply::Handled();
}

FReply ALoginHUD::HandleSelectClimate(FString ClimateName)
{
    PendingClimate = ClimateName;
    bNewCountryClimateExpanded = false;
    RefreshLoginWidget();
    return FReply::Handled();
}

FReply ALoginHUD::HandleSelectLeaderGender(FString GenderName)
{
    PendingLeaderGender = GenderName;
    PendingAddressTitle = GetAddressTitleForGender(GenderName);
    bNewCountryGenderExpanded = false;
    RefreshLoginWidget();
    return FReply::Handled();
}

FReply ALoginHUD::HandleToggleNewCountryClimateSectionClicked()
{
    bNewCountryClimateExpanded = PendingClimate.IsEmpty() ? true : !bNewCountryClimateExpanded;
    RefreshLoginWidget();
    return FReply::Handled();
}

FReply ALoginHUD::HandleToggleNewCountryGenderSectionClicked()
{
    bNewCountryGenderExpanded = PendingLeaderGender.IsEmpty() ? true : !bNewCountryGenderExpanded;
    RefreshLoginWidget();
    return FReply::Handled();
}

FReply ALoginHUD::HandleToggleNewCountryLocationSectionClicked()
{
    bNewCountryLocationExpanded = PendingStartingCountryMapIndex <= 0 ? true : !bNewCountryLocationExpanded;
    RefreshLoginWidget();
    return FReply::Handled();
}
FReply ALoginHUD::HandleStartingCountryMapClicked(const FGeometry& Geometry, const FPointerEvent& MouseEvent)
{
    if (!MouseEvent.GetEffectingButton().IsValid() || MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
    {
        return FReply::Unhandled();
    }

    const FVector2D LocalClick = Geometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
    const FVector2D LocalSize = Geometry.GetLocalSize();
    if (LocalSize.X <= 0.0f || LocalSize.Y <= 0.0f || LocalClick.X < 0.0f || LocalClick.Y < 0.0f || LocalClick.X > LocalSize.X || LocalClick.Y > LocalSize.Y)
    {
        return FReply::Handled();
    }

    const FVector2D MapPoint(
        (LocalClick.X / LocalSize.X) * 2242.0f,
        (LocalClick.Y / LocalSize.Y) * 1104.0f);
    const int32 CountryIndex = SampleDuliaCountryIdAtMapPoint(MapPoint);
    if (CountryIndex <= 0)
    {
        LastSaveStatus = TEXT("No country selected. Click a land country on the Dulia map.");
        RefreshLoginWidget();
        return FReply::Handled();
    }

    const FDifficultyProfile DifficultyProfile = FDifficultyProfileLibrary::GetProfile(PendingDifficulty.IsEmpty() ? TEXT("Normal") : PendingDifficulty);
    const FString PreviewClimate = PendingClimate.IsEmpty() ? TEXT("Middle Moderate") : PendingClimate;
    const FDemocracyWorldMapState PreviewMap = FDemocracyGameStateFactory::BuildStartingCountryPreviewMap(PreviewClimate, DifficultyProfile);
    for (const FDemocracyContinentState& Continent : PreviewMap.Continents)
    {
        for (const FDemocracyGeneratedCountryState& Country : Continent.Countries)
        {
            if (Country.MapCountryIndex == CountryIndex)
            {
                PendingStartingCountryName = Country.CountryName;
                PendingStartingCountryMapIndex = Country.MapCountryIndex;
                bNewCountryLocationExpanded = true;
                LastSaveStatus = FString::Printf(TEXT("Selected starting country: %s."), *PendingStartingCountryName);
                RefreshLoginWidget();
                return FReply::Handled();
            }
        }
    }

    LastSaveStatus = FString::Printf(TEXT("Clicked Dulia map slot %03d, but that country is not available for the current difficulty preview."), CountryIndex);
    RefreshLoginWidget();
    return FReply::Handled();
}
FReply ALoginHUD::HandleSelectStartingCountry(FString CountryName, int32 MapCountryIndex)
{
    PendingStartingCountryName = CountryName;
    PendingStartingCountryMapIndex = MapCountryIndex;
    bNewCountryLocationExpanded = true;
    LastSaveStatus = FString::Printf(TEXT("Starting country selected: %s, map slot %03d."), *PendingStartingCountryName, PendingStartingCountryMapIndex);
    RefreshLoginWidget();
    return FReply::Handled();
}

FReply ALoginHUD::HandleCreateInitialSaveClicked()
{
    if (PendingStateName.TrimStartAndEnd().IsEmpty())
    {
        LastSaveStatus = TEXT("Enter a country name before creating the save.");
        RefreshLoginWidget();
        return FReply::Handled();
    }

    if (PendingDifficulty.IsEmpty() || PendingClimate.IsEmpty() || PendingLeaderGender.IsEmpty())
    {
        LastSaveStatus = TEXT("Choose difficulty, climate, and president address before creating the save.");
        RefreshLoginWidget();
        return FReply::Handled();
    }

    if (PendingStartingCountryMapIndex <= 0)
    {
        LastSaveStatus = TEXT("Choose a starting country on Planet Dulia before creating the save.");
        RefreshLoginWidget();
        return FReply::Handled();
    }

    FString SavePath;
    if (CreateInitialSinglePlayerSave(SavePath))
    {
        OpeningScriptText = GetOpeningScriptText();
        bShowFirstLoginBriefing = true;
        EnterOfficePrototype(true);
    }
    else
    {
        RefreshLoginWidget();
    }

    return FReply::Handled();
}

FReply ALoginHUD::HandleResumeSimulationClicked()
{
    StartSimulationTimer();
    return FReply::Handled();
}

FReply ALoginHUD::HandlePauseSimulationClicked()
{
    StopSimulationTimer();
    RefreshLoginWidget();
    return FReply::Handled();
}

FReply ALoginHUD::HandleStepSimulationClicked()
{
    if (bHasLoadedRuntimeState)
    {
        const bool bWasPaused = LoadedSaveState.RuntimeState.bPaused;
        LoadedSaveState.RuntimeState.bPaused = false;
        RunSimulationTick();
        LoadedSaveState.RuntimeState.bPaused = bWasPaused;
        if (bWasPaused)
        {
            LoadedSaveState.RuntimeState.Phase = TEXT("Prototype Simulation Paused");
            SimulationTickSummary = BuildSimulationStatusText();
        }
        RefreshLoginWidget();
    }

    return FReply::Handled();
}


FReply ALoginHUD::HandleOpenPoliciesClicked()
{
    ShowScreen(ELoginFlowScreen::OfficePolicies);
    return FReply::Handled();
}

FReply ALoginHUD::HandleOpenBudgetClicked()
{
    ShowScreen(ELoginFlowScreen::OfficeBudget);
    return FReply::Handled();
}

FReply ALoginHUD::HandleOpenResourceChainsClicked()
{
    ShowScreen(ELoginFlowScreen::OfficeResourceChains);
    return FReply::Handled();
}

FReply ALoginHUD::HandleOpenAdvisorWarningsClicked()
{
    ShowScreen(ELoginFlowScreen::OfficeAdvisorWarnings);
    return FReply::Handled();
}

FReply ALoginHUD::HandleOpenDepartmentsClicked()
{
    ShowScreen(ELoginFlowScreen::OfficeDepartments);
    return FReply::Handled();
}

FReply ALoginHUD::HandleOpenDevelopmentClicked()
{
    ShowScreen(ELoginFlowScreen::OfficeDevelopment);
    return FReply::Handled();
}

FReply ALoginHUD::HandleSetDevelopmentFocus(FString TrackName)
{
    if (!bHasLoadedRuntimeState)
    {
        return FReply::Handled();
    }

    InitializeDevelopmentSystemIfMissing(LoadedSaveState.RuntimeState);
    FDemocracyDevelopmentTrackState* Track = FindDevelopmentTrack(LoadedSaveState.RuntimeState.DevelopmentSystem, TrackName);
    if (!Track)
    {
        LastSaveStatus = FString::Printf(TEXT("Development focus not found: %s"), *TrackName);
        return FReply::Handled();
    }

    LoadedSaveState.RuntimeState.DevelopmentSystem.ActiveFocus = Track->TrackName;
    LoadedSaveState.RuntimeState.DevelopmentSystem.LastUpdatedTurn = LoadedSaveState.RuntimeState.Turn;
    LoadedSaveState.RuntimeState.DevelopmentSystem.Summary = FString::Printf(TEXT("%s selected as the national development focus. Progress will advance during simulation ticks when treasury and resource costs are available."), *Track->TrackName);

    LogDecision(LoadedSaveState.RuntimeState, TEXT("Development Focus"), Track->TrackName, FString::Printf(TEXT("Development focus changed to %s."), *Track->TrackName), LoadedSaveState.RuntimeState.DevelopmentSystem.Summary, 20, { TEXT("development"), Track->TrackName });
    LastSaveStatus = FString::Printf(TEXT("Development focus set to %s."), *Track->TrackName);
    ShowScreen(ELoginFlowScreen::OfficeDevelopment);
    return FReply::Handled();
}

FReply ALoginHUD::HandleOpenApprovalStabilityClicked()
{
    ShowScreen(ELoginFlowScreen::OfficeApprovalStability);
    return FReply::Handled();
}
FReply ALoginHUD::HandleOpenDecisionHistoryClicked()
{
    ShowScreen(ELoginFlowScreen::OfficeDecisionHistory);
    return FReply::Handled();
}

FReply ALoginHUD::HandleOpenMeetingAdvisorClicked()
{
    if (SelectedMeetingAdvisorName.IsEmpty())
    {
        SelectedMeetingAdvisorName = TEXT("Meeting Advisor");
        SelectedMeetingAdvisorFocus = TEXT("General meeting report and agenda surface.");
    }
    ShowScreen(ELoginFlowScreen::OfficeMeetingAdvisor);
    return FReply::Handled();
}

FReply ALoginHUD::HandleOpenPressReleaseClicked()
{
    ShowScreen(ELoginFlowScreen::OfficePressRelease);
    return FReply::Handled();
}

FReply ALoginHUD::HandleSetDepartmentAction(FString DepartmentName, FString ActionName)
{
    if (!bHasLoadedRuntimeState)
    {
        return FReply::Handled();
    }

    InitializeDefaultDepartments(LoadedSaveState.RuntimeState);
    FDemocracyDepartmentState* Department = FindDepartment(LoadedSaveState.RuntimeState.Departments, DepartmentName);
    if (!Department)
    {
        return FReply::Handled();
    }

    Department->CurrentAction = ActionName;
    if (ActionName.Equals(TEXT("Focused Initiative"), ESearchCase::IgnoreCase))
    {
        Department->Priority = FMath::Clamp(Department->Priority + 12, 0, 100);
        Department->Staffing = FMath::Clamp(Department->Staffing + 4, 0, 100);
        LoadedSaveState.RuntimeState.PlayerCountry.Treasury = FMath::Max(0, LoadedSaveState.RuntimeState.PlayerCountry.Treasury - 35);
    }
    else if (ActionName.Equals(TEXT("Emergency Response"), ESearchCase::IgnoreCase))
    {
        Department->Priority = FMath::Clamp(Department->Priority + 22, 0, 100);
        Department->Staffing = FMath::Clamp(Department->Staffing + 8, 0, 100);
        Department->PublicTrust = FMath::Clamp(Department->PublicTrust - 2, 0, 100);
        LoadedSaveState.RuntimeState.PlayerCountry.Treasury = FMath::Max(0, LoadedSaveState.RuntimeState.PlayerCountry.Treasury - 90);
        LoadedSaveState.RuntimeState.PlayerCountry.Unrest = FMath::Clamp(LoadedSaveState.RuntimeState.PlayerCountry.Unrest - 1, 0, 100);
    }
    else if (ActionName.Equals(TEXT("Administrative Reform"), ESearchCase::IgnoreCase))
    {
        Department->Priority = FMath::Clamp(Department->Priority + 6, 0, 100);
        Department->PublicTrust = FMath::Clamp(Department->PublicTrust + 4, 0, 100);
        Department->Effectiveness = FMath::Clamp(Department->Effectiveness + 3, 0, 100);
        LoadedSaveState.RuntimeState.PlayerCountry.Treasury = FMath::Max(0, LoadedSaveState.RuntimeState.PlayerCountry.Treasury - 45);
    }
    else
    {
        Department->Priority = FMath::Clamp(Department->Priority - 6, 0, 100);
        Department->Staffing = FMath::Clamp(Department->Staffing - 1, 0, 100);
    }

    RecalculateDepartments(LoadedSaveState.RuntimeState);
    LogDecision(LoadedSaveState.RuntimeState, TEXT("Department Action"), DepartmentName, FString::Printf(TEXT("Assigned %s to %s."), *ActionName, *DepartmentName), BuildDepartmentSummaryText(LoadedSaveState.RuntimeState.Departments), ActionName.Equals(TEXT("Emergency Response"), ESearchCase::IgnoreCase) ? 45 : 25, { TEXT("department"), DepartmentName, ActionName });
    LoadedSaveState.RuntimeState.AdvisorSystem.Reports = GenerateAdvisorReports(LoadedSaveState.RuntimeState);
    LoadedSaveState.RuntimeState.Phase = FString::Printf(TEXT("%s Department Action Updated"), *DepartmentName);
    LastSaveStatus = TEXT("Department action changed. Save current state to persist it to disk.");
    RefreshLoginWidget();
    return FReply::Handled();
}
FReply ALoginHUD::HandleApplyResourceAction(FString ResourceActionName)
{
    if (!bHasLoadedRuntimeState)
    {
        return FReply::Handled();
    }

    FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    FDemocracyCountryState& Country = State.PlayerCountry;
    FDemocracyResourceInventory& Resources = Country.Resources;

    int32 TreasuryDelta = 0;
    int32 ApprovalDelta = 0;
    int32 StabilityDelta = 0;
    int32 UnrestDelta = 0;
    int32 EconomyDelta = 0;
    int32 InfrastructureDelta = 0;
    int32 EnvironmentDelta = 0;
    FString Consequence;

    if (ResourceActionName.Equals(TEXT("Emergency Imports"), ESearchCase::IgnoreCase))
    {
        Resources.Food = FMath::Max(0, Resources.Food + 42);
        Resources.Water = FMath::Max(0, Resources.Water + 30);
        Resources.GasOil = FMath::Max(0, Resources.GasOil + 18);
        Resources.Wood = FMath::Max(0, Resources.Wood + 12);
        Resources.Metals = FMath::Max(0, Resources.Metals + 10);
        TreasuryDelta = -85;
        ApprovalDelta = 1;
        StabilityDelta = 2;
        UnrestDelta = -4;
        Consequence = TEXT("Emergency imports increased core reserves and reduced immediate shortage unrest at a treasury cost.");
    }
    else if (ResourceActionName.Equals(TEXT("Production Surge"), ESearchCase::IgnoreCase))
    {
        Resources.Food = FMath::Max(0, Resources.Food + 22);
        Resources.Water = FMath::Max(0, Resources.Water + 12);
        Resources.GasOil = FMath::Max(0, Resources.GasOil + 24);
        Resources.Wood = FMath::Max(0, Resources.Wood + 20);
        Resources.Metals = FMath::Max(0, Resources.Metals + 20);
        TreasuryDelta = -55;
        EconomyDelta = 2;
        InfrastructureDelta = -1;
        EnvironmentDelta = -3;
        Consequence = TEXT("Production surge increased reserves and economic output while straining infrastructure and environmental health.");
    }
    else
    {
        Resources.Food = FMath::Max(0, Resources.Food - 8);
        Resources.Water = FMath::Max(0, Resources.Water - 6);
        Resources.GasOil = FMath::Max(0, Resources.GasOil - 4);
        TreasuryDelta = 12;
        ApprovalDelta = -1;
        StabilityDelta = 1;
        UnrestDelta = -2;
        Consequence = TEXT("Reserve rationing slowed consumption and lowered unrest pressure, but citizens disliked the restrictions.");
    }

    Country.Treasury = FMath::Max(0, Country.Treasury + TreasuryDelta);
    Country.PublicApproval = FMath::Clamp(Country.PublicApproval + ApprovalDelta, 0, 100);
    Country.Stability = FMath::Clamp(Country.Stability + StabilityDelta, 0, 100);
    Country.Unrest = FMath::Clamp(Country.Unrest + UnrestDelta, 0, 100);
    Country.EconomicHealth = FMath::Clamp(Country.EconomicHealth + EconomyDelta, 0, 100);
    Country.Infrastructure = FMath::Clamp(Country.Infrastructure + InfrastructureDelta, 0, 100);
    Country.EnvironmentalHealth = FMath::Clamp(Country.EnvironmentalHealth + EnvironmentDelta, 0, 100);

    State.ResourceChains.LastUpdatedTurn = State.Turn;
    State.ResourceChains.TradeBalance = TreasuryDelta;
    int32 WeightedShortage = 0;
    for (FDemocracyResourceChainEntry& Entry : State.ResourceChains.Chains)
    {
        if (Entry.ResourceName.Equals(TEXT("Food"), ESearchCase::IgnoreCase))
        {
            Entry.Reserve = Resources.Food;
        }
        else if (Entry.ResourceName.Equals(TEXT("Water"), ESearchCase::IgnoreCase))
        {
            Entry.Reserve = Resources.Water;
        }
        else if (Entry.ResourceName.Equals(TEXT("Fuel"), ESearchCase::IgnoreCase) || Entry.ResourceName.Equals(TEXT("Gas/Oil"), ESearchCase::IgnoreCase))
        {
            Entry.Reserve = Resources.GasOil;
        }
        else if (Entry.ResourceName.Equals(TEXT("Wood"), ESearchCase::IgnoreCase))
        {
            Entry.Reserve = Resources.Wood;
        }
        else if (Entry.ResourceName.Equals(TEXT("Metals"), ESearchCase::IgnoreCase))
        {
            Entry.Reserve = Resources.Metals;
        }

        if (ResourceActionName.Equals(TEXT("Emergency Imports"), ESearchCase::IgnoreCase))
        {
            Entry.Imports = FMath::Max(Entry.Imports, 3);
            Entry.Drivers.AddUnique(TEXT("computer emergency imports"));
        }
        else if (ResourceActionName.Equals(TEXT("Production Surge"), ESearchCase::IgnoreCase))
        {
            Entry.Production = FMath::Max(0, Entry.Production + 3);
            Entry.Drivers.AddUnique(TEXT("computer production surge"));
        }
        else
        {
            Entry.Consumption = FMath::Max(0, Entry.Consumption - 2);
            Entry.Drivers.AddUnique(TEXT("computer rationing order"));
        }

        Entry.Shortage = FMath::Max(0, Entry.ReserveTarget - Entry.Reserve);
        Entry.Surplus = FMath::Max(0, Entry.Reserve - Entry.ReserveTarget);
        Entry.Status = Entry.Shortage > 0
            ? FString::Printf(TEXT("Shortage: %d below reserve target."), Entry.Shortage)
            : FString::Printf(TEXT("Stable reserve with surplus %d."), Entry.Surplus);
        WeightedShortage += Entry.Shortage * FMath::Max(1, Entry.StrategicValue);
    }
    State.ResourceChains.TotalShortagePressure = FMath::Clamp(WeightedShortage / 450, 0, 100);
    State.ResourceChains.Summary = FString::Printf(TEXT("%s applied from computer. %s Treasury %+d, approval %+d, stability %+d, unrest %+d."), *ResourceActionName, *Consequence, TreasuryDelta, ApprovalDelta, StabilityDelta, UnrestDelta);

    RecalculateEconomyBudget(State);
    RecalculateDepartments(State);
    RecalculateDemographics(State);
    RecalculateApprovalStability(State);
    State.AdvisorSystem.GuidanceLevel = AdvisorGuidanceForDifficultyScore(Country.CountrySizeScore);
    State.AdvisorSystem.LastUpdatedTurn = State.Turn;
    State.AdvisorSystem.Reports = GenerateAdvisorReports(State);
    State.Phase = TEXT("Resource Action Applied");
    LogDecision(State, TEXT("Resource Action"), ResourceActionName, FString::Printf(TEXT("Computer resource action selected: %s."), *ResourceActionName), State.ResourceChains.Summary, 34, { TEXT("resource"), ResourceActionName });

    LoadedSaveSummary = LoadedSaveState.ToSummaryText();
    SimulationTickSummary = BuildSimulationStatusText();
    LastSaveStatus = FString::Printf(TEXT("Resource action applied: %s. Save current state to persist it."), *ResourceActionName);
    RefreshLoginWidget();
    EvaluateFailState();
    return FReply::Handled();
}

FReply ALoginHUD::HandleApplyAdvisorAction(FString AdvisorActionName)
{
    if (!bHasLoadedRuntimeState)
    {
        return FReply::Handled();
    }

    FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    FDemocracyCountryState& Country = State.PlayerCountry;
    State.AdvisorSystem.GuidanceLevel = AdvisorGuidanceForDifficultyScore(Country.CountrySizeScore);
    State.AdvisorSystem.LastUpdatedTurn = State.Turn;

    FString Consequence;
    int32 Severity = 12;
    if (AdvisorActionName.Equals(TEXT("Emergency Guidance"), ESearchCase::IgnoreCase))
    {
        State.FailureRisk.CurrentAssassinationRisk = FMath::Clamp(State.FailureRisk.CurrentAssassinationRisk - 4, 0, State.FailureRisk.AssassinationRiskTrigger);
        State.InvasionRisk.CurrentInvasionRisk = FMath::Clamp(State.InvasionRisk.CurrentInvasionRisk - 3, 0, State.InvasionRisk.InvasionRiskTrigger);
        Country.Stability = FMath::Clamp(Country.Stability + 1, 0, 100);
        State.AdvisorSystem.AdvisorCount = FMath::Max(State.AdvisorSystem.AdvisorCount, 1);
        State.FailureRisk.AdvisorWarnings.AddUnique(TEXT("Emergency guidance requested: stabilize unrest causes, protect continuity, and address resource shortages first."));
        State.InvasionRisk.AdvisorWarnings.AddUnique(TEXT("Emergency guidance requested: maintain readiness, diplomacy, and border pressure monitoring."));
        Consequence = TEXT("Emergency advisor guidance reduced immediate internal and takeover risk pressure and refreshed all reports.");
        Severity = 28;
    }
    else
    {
        Consequence = TEXT("Advisor reports refreshed from the current runtime state.");
    }

    RecalculateEconomyBudget(State);
    RecalculateDepartments(State);
    RecalculateDemographics(State);
    RecalculateApprovalStability(State);
    State.AdvisorSystem.Reports = GenerateAdvisorReports(State);
    State.Phase = TEXT("Advisor Reports Updated");
    LogDecision(State, TEXT("Advisor Action"), AdvisorActionName, FString::Printf(TEXT("Computer/phone advisor action selected: %s."), *AdvisorActionName), Consequence, Severity, { TEXT("advisor"), AdvisorActionName });

    LoadedSaveSummary = LoadedSaveState.ToSummaryText();
    SimulationTickSummary = BuildSimulationStatusText();
    LastSaveStatus = FString::Printf(TEXT("Advisor action applied: %s. Save current state to persist it."), *AdvisorActionName);
    RefreshLoginWidget();
    EvaluateFailState();
    return FReply::Handled();
}
FReply ALoginHUD::HandleExecuteAuthorityCommand(FString CommandId, FString SurfaceName)
{
    if (!bHasLoadedRuntimeState)
    {
        return FReply::Handled();
    }

    FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    FDemocracyCountryState& Country = State.PlayerCountry;
    RefreshCommandAuthority(State);

    FDemocracyCommandAuthorityActionState* SelectedAction = nullptr;
    for (FDemocracyCommandAuthorityActionState& Action : State.CommandAuthority.Actions)
    {
        if (Action.CommandId.Equals(CommandId, ESearchCase::IgnoreCase))
        {
            SelectedAction = &Action;
            break;
        }
    }

    if (!SelectedAction)
    {
        LastSaveStatus = FString::Printf(TEXT("Command authority action not found: %s"), *CommandId);
        RefreshLoginWidget();
        return FReply::Handled();
    }

    if (SurfaceName.Equals(TEXT("Office"), ESearchCase::IgnoreCase) && !SelectedAction->bOfficeAllowed)
    {
        SelectedAction->DisabledReason = TEXT("This command belongs to the future RTS view and cannot be executed from the office.");
        LastSaveStatus = FString::Printf(TEXT("Command blocked: %s"), *SelectedAction->DisabledReason);
        RefreshLoginWidget();
        return FReply::Handled();
    }

    if (!SelectedAction->bEnabled)
    {
        LastSaveStatus = FString::Printf(TEXT("Command blocked: %s"), SelectedAction->DisabledReason.IsEmpty() ? TEXT("requirements are not met.") : *SelectedAction->DisabledReason);
        RefreshLoginWidget();
        return FReply::Handled();
    }

    Country.Treasury = FMath::Clamp(Country.Treasury - SelectedAction->TreasuryCost, 0, 1000);
    Country.PublicApproval = FMath::Clamp(Country.PublicApproval + SelectedAction->ApprovalDelta, 0, 100);
    Country.Stability = FMath::Clamp(Country.Stability + SelectedAction->StabilityDelta, 0, 100);
    Country.Unrest = FMath::Clamp(Country.Unrest + SelectedAction->UnrestDelta, 0, 100);
    Country.DiplomaticStanding = FMath::Clamp(Country.DiplomaticStanding + SelectedAction->DiplomacyDelta, 0, 100);
    Country.MilitaryReadiness = FMath::Clamp(Country.MilitaryReadiness + SelectedAction->MilitaryDelta, 0, 100);
    State.InvasionRisk.CurrentInvasionRisk = FMath::Clamp(State.InvasionRisk.CurrentInvasionRisk + SelectedAction->InvasionRiskDelta, 0, State.InvasionRisk.InvasionRiskTrigger);

    if (SelectedAction->ResourceDelta != 0)
    {
        Country.Resources.Food = FMath::Max(0, Country.Resources.Food + SelectedAction->ResourceDelta);
        Country.Resources.GasOil = FMath::Max(0, Country.Resources.GasOil + SelectedAction->ResourceDelta);
        Country.Resources.Wood = FMath::Max(0, Country.Resources.Wood + SelectedAction->ResourceDelta);
        Country.Resources.Metals = FMath::Max(0, Country.Resources.Metals + SelectedAction->ResourceDelta);
    }

    if (SelectedAction->CommandId.Equals(TEXT("office_declare_war"), ESearchCase::IgnoreCase))
    {
        FDemocracyDiplomacyRelationshipState* WarTarget = nullptr;
        for (FDemocracyDiplomacyRelationshipState& Relationship : State.DiplomacyMatrix.Relationships)
        {
            const bool bCandidate = Relationship.RelationshipStatus.Equals(TEXT("Hostile"), ESearchCase::IgnoreCase)
                || Relationship.RelationshipStatus.Equals(TEXT("Rival"), ESearchCase::IgnoreCase)
                || Relationship.BorderTension >= 70;
            if (bCandidate && (!WarTarget || Relationship.BorderTension > WarTarget->BorderTension))
            {
                WarTarget = &Relationship;
            }
        }
        const FString OpponentName = WarTarget ? WarTarget->CountryName : TEXT("Unspecified Rival");
        if (WarTarget)
        {
            WarTarget->RelationshipStatus = TEXT("Hostile");
            WarTarget->BorderTension = FMath::Clamp(WarTarget->BorderTension + 20, 0, 100);
            WarTarget->Trust = FMath::Clamp(WarTarget->Trust - 18, 0, 100);
            WarTarget->LastChangedTurn = State.Turn;
            WarTarget->Notes.AddUnique(TEXT("War declared through simulation command authority."));
        }

        const FString ConflictId = FString::Printf(TEXT("war-%s"), *OpponentName.Replace(TEXT(" "), TEXT("-")).ToLower());
        FDemocracyWarConflictState* Conflict = nullptr;
        for (FDemocracyWarConflictState& ExistingConflict : State.WarSystem.ActiveConflicts)
        {
            if (ExistingConflict.ConflictId.Equals(ConflictId, ESearchCase::IgnoreCase))
            {
                Conflict = &ExistingConflict;
                break;
            }
        }
        if (!Conflict)
        {
            FDemocracyWarConflictState NewConflict;
            NewConflict.ConflictId = ConflictId;
            NewConflict.ConflictName = FString::Printf(TEXT("War with %s"), *OpponentName);
            NewConflict.ConflictType = TEXT("Declared War");
            NewConflict.Status = TEXT("Active");
            NewConflict.PrimaryObjective = TEXT("Defend sovereignty and force diplomatic concessions.");
            NewConflict.EnemyObjective = TEXT("Pressure the capital, seize provinces, and weaken stability.");
            NewConflict.StartedTurn = State.Turn;
            NewConflict.LastUpdatedTurn = State.Turn;
            NewConflict.EscalationLevel = 3;
            NewConflict.WarScore = -5;
            NewConflict.VictoryProgress = 5;
            NewConflict.DefeatRisk = FMath::Clamp(State.InvasionRisk.CurrentInvasionRisk / 2 + 15, 0, 100);
            NewConflict.VictoryCondition = TEXT("Resolve through RTS victories, treaty concessions, or negotiated settlement.");
            NewConflict.DefeatCondition = TEXT("Capital threatened, province control collapse, or surrender terms accepted.");
            FDemocracyWarParticipantState PlayerParticipant;
            PlayerParticipant.CountryName = Country.CountryName;
            PlayerParticipant.Role = TEXT("Defender");
            PlayerParticipant.Alignment = State.ObjectiveState.PlayerGovernmentType;
            PlayerParticipant.Commitment = 65;
            PlayerParticipant.WarSupport = FMath::Clamp(Country.PublicApproval - 10, 0, 100);
            NewConflict.Participants.Add(PlayerParticipant);
            FDemocracyWarParticipantState OpponentParticipant;
            OpponentParticipant.CountryName = OpponentName;
            OpponentParticipant.Role = TEXT("Opponent");
            OpponentParticipant.Alignment = WarTarget ? WarTarget->GovernmentType : TEXT("Unknown");
            OpponentParticipant.Commitment = 70;
            OpponentParticipant.WarSupport = 60;
            NewConflict.Participants.Add(OpponentParticipant);
            FDemocracyWarFrontState Front;
            Front.FrontName = TEXT("Primary Border Front");
            Front.RegionName = State.RtsWorld.Ownership.Continents.Num() > 0 ? State.RtsWorld.Ownership.Continents[0].ContinentName : TEXT("Unassigned Border");
            Front.ContestedBorder = FString::Printf(TEXT("%s border"), *OpponentName);
            Front.Pressure = FMath::Clamp(State.InvasionRisk.CurrentInvasionRisk + 20, 0, 100);
            Front.PlayerControl = 50;
            Front.Status = TEXT("Active Combat Pending RTS Resolution");
            NewConflict.Fronts.Add(Front);
            NewConflict.ActiveModifiers = { TEXT("Declared by simulation office"), TEXT("RTS battle resolution required") };
            State.WarSystem.ActiveConflicts.Add(NewConflict);
        }
        State.WarSystem.EscalationPressure = FMath::Clamp(State.WarSystem.EscalationPressure + 25, 0, 100);
        State.WarSystem.WarFatigue = FMath::Clamp(State.WarSystem.WarFatigue + 8, 0, 100);
        State.WarSystem.LastUpdatedTurn = State.Turn;
        State.WarSystem.Summary = FString::Printf(TEXT("Declared war against %s. RTS layer must resolve battles and return outcomes."), *OpponentName);
        State.RtsWorld.Backflow.LastImportQueueSummary = TEXT("War declaration created: future RTS results must feed battle, casualty, province, and supply outcomes back into simulation.");
    }
    else if (SelectedAction->CommandId.Equals(TEXT("office_request_alliance_aid"), ESearchCase::IgnoreCase))
    {
        bool bFoundSupporter = false;
        for (FDemocracyDiplomacyRelationshipState& Relationship : State.DiplomacyMatrix.Relationships)
        {
            const bool bSupporter = Relationship.RelationshipStatus.Equals(TEXT("Ally"), ESearchCase::IgnoreCase)
                || Relationship.TreatyStatus.Contains(TEXT("Defense"), ESearchCase::IgnoreCase)
                || Relationship.Trust >= 70;
            if (bSupporter)
            {
                Relationship.Trust = FMath::Clamp(Relationship.Trust + 2, 0, 100);
                Relationship.LastChangedTurn = State.Turn;
                Relationship.Notes.AddUnique(TEXT("Alliance aid requested through simulation command authority."));
                bFoundSupporter = true;
                break;
            }
        }
        if (bFoundSupporter)
        {
            Country.MilitaryReadiness = FMath::Clamp(Country.MilitaryReadiness + 3, 0, 100);
            State.WarSystem.ReadinessStatus = TEXT("Alliance aid requested; readiness support pending future server/RTS resolution.");
        }
    }
    else if (SelectedAction->CommandId.Equals(TEXT("office_negotiate_ceasefire"), ESearchCase::IgnoreCase))
    {
        for (FDemocracyWarConflictState& Conflict : State.WarSystem.ActiveConflicts)
        {
            Conflict.Status = TEXT("Ceasefire Talks");
            Conflict.LastUpdatedTurn = State.Turn;
            Conflict.EscalationLevel = FMath::Max(0, Conflict.EscalationLevel - 1);
            Conflict.DefeatRisk = FMath::Max(0, Conflict.DefeatRisk - 8);
            Conflict.ActiveModifiers.AddUnique(TEXT("Ceasefire negotiations opened from simulation office."));
        }
        State.WarSystem.EscalationPressure = FMath::Max(0, State.WarSystem.EscalationPressure - 12);
        State.WarSystem.WarFatigue = FMath::Max(0, State.WarSystem.WarFatigue - 4);
        State.WarSystem.Summary = TEXT("Ceasefire talks opened; RTS should pause escalation unless new battle outcomes arrive.");
    }
    else if (SelectedAction->CommandId.Equals(TEXT("office_surrender_territory"), ESearchCase::IgnoreCase))
    {
        bool bProvinceTransferred = false;
        for (FDemocracyProvinceOwnershipState& Province : State.RtsWorld.Ownership.Provinces)
        {
            if (Province.bPlayerControlled && Province.bBorderProvince)
            {
                Province.bPlayerControlled = false;
                Province.CurrentControllerCountryName = TEXT("Ceasefire Occupation");
                Province.LastChangedTurn = State.Turn;
                bProvinceTransferred = true;
                break;
            }
        }
        if (!bProvinceTransferred)
        {
            for (FDemocracyProvinceOwnershipState& Province : State.RtsWorld.Ownership.Provinces)
            {
                if (Province.bPlayerControlled)
                {
                    Province.bPlayerControlled = false;
                    Province.CurrentControllerCountryName = TEXT("Ceasefire Occupation");
                    Province.LastChangedTurn = State.Turn;
                    bProvinceTransferred = true;
                    break;
                }
            }
        }
        State.RtsWorld.Ownership.LastUpdatedTurn = State.Turn;
        State.RtsWorld.Ownership.Summary = bProvinceTransferred ? TEXT("Territory surrender transferred one player-controlled province marker pending RTS/server reconciliation.") : TEXT("Territory surrender recorded, but no player-controlled province marker was available to transfer.");
        State.RtsWorld.Backflow.TotalTerritoryDelta -= bProvinceTransferred ? 1 : 0;
        State.WarSystem.WarFatigue = FMath::Clamp(State.WarSystem.WarFatigue + 10, 0, 100);
        State.WarSystem.Summary = TEXT("Territory surrendered to reduce immediate takeover pressure; ownership must be reconciled by RTS/server authority.");
    }
    else if (SelectedAction->CommandId.Equals(TEXT("office_propose_alliance"), ESearchCase::IgnoreCase))
    {
        FDemocracyDiplomacyRelationshipState* AllianceTarget = nullptr;
        for (FDemocracyDiplomacyRelationshipState& Relationship : State.DiplomacyMatrix.Relationships)
        {
            const bool bSameAlignment = Relationship.GovernmentType.Equals(State.ObjectiveState.PlayerGovernmentType, ESearchCase::IgnoreCase);
            const bool bEligible = bSameAlignment
                && !Relationship.RelationshipStatus.Equals(TEXT("Ally"), ESearchCase::IgnoreCase)
                && !Relationship.RelationshipStatus.Equals(TEXT("Hostile"), ESearchCase::IgnoreCase)
                && Relationship.Trust >= 55;
            if (bEligible && (!AllianceTarget || Relationship.Trust > AllianceTarget->Trust))
            {
                AllianceTarget = &Relationship;
            }
        }
        if (AllianceTarget)
        {
            AllianceTarget->RelationshipStatus = TEXT("Ally");
            AllianceTarget->TreatyStatus = TEXT("Mutual Defense Framework");
            AllianceTarget->bTradePartner = true;
            AllianceTarget->Trust = FMath::Clamp(AllianceTarget->Trust + 8, 0, 100);
            AllianceTarget->TradeValue = FMath::Clamp(AllianceTarget->TradeValue + 8, 0, 100);
            AllianceTarget->BorderTension = FMath::Max(0, AllianceTarget->BorderTension - 10);
            AllianceTarget->LastChangedTurn = State.Turn;
            AllianceTarget->ActiveTreaties.AddUnique(TEXT("Mutual Defense Framework"));
            AllianceTarget->Notes.AddUnique(TEXT("Alliance formed through office command authority; allied aid and reinforcement RTS permissions are available."));
            State.RtsWorld.Hud.Alerts.Add(FString::Printf(TEXT("Alliance formed with %s. Allied aid and reinforcement permissions unlocked."), *AllianceTarget->CountryName));
            State.CommandAuthority.LastCommandSummary = FString::Printf(TEXT("Alliance formed with %s. RTS may request aid and reinforce allied positions."), *AllianceTarget->CountryName);
        }
        else
        {
            State.CommandAuthority.LastCommandSummary = TEXT("Alliance proposal recorded, but no eligible same-government partner was found during execution.");
        }
    }
    else if (SelectedAction->CommandId.Equals(TEXT("office_propose_treaty"), ESearchCase::IgnoreCase))
    {
        FDemocracyDiplomacyRelationshipState* TreatyTarget = nullptr;
        for (FDemocracyDiplomacyRelationshipState& Relationship : State.DiplomacyMatrix.Relationships)
        {
            const bool bEligible = !Relationship.RelationshipStatus.Equals(TEXT("Hostile"), ESearchCase::IgnoreCase)
                && Relationship.Trust >= 45
                && Relationship.BorderTension <= 65;
            if (bEligible && (!TreatyTarget || Relationship.BorderTension > TreatyTarget->BorderTension))
            {
                TreatyTarget = &Relationship;
            }
        }
        if (TreatyTarget)
        {
            TreatyTarget->TreatyStatus = TEXT("Trade And Border Stability Treaty");
            TreatyTarget->bTradePartner = true;
            TreatyTarget->Trust = FMath::Clamp(TreatyTarget->Trust + 5, 0, 100);
            TreatyTarget->TradeValue = FMath::Clamp(TreatyTarget->TradeValue + 10, 0, 100);
            TreatyTarget->BorderTension = FMath::Max(0, TreatyTarget->BorderTension - 12);
            TreatyTarget->LastChangedTurn = State.Turn;
            TreatyTarget->ActiveTreaties.AddUnique(TEXT("Trade And Border Stability Treaty"));
            TreatyTarget->Notes.AddUnique(TEXT("Treaty signed through office command authority; RTS border risk and supply/trade friction are reduced."));
            State.RtsWorld.Hud.Alerts.Add(FString::Printf(TEXT("Treaty signed with %s. Border risk and trade/supply friction reduced."), *TreatyTarget->CountryName));
            State.CommandAuthority.LastCommandSummary = FString::Printf(TEXT("Treaty signed with %s. RTS border risk and trade/supply friction reduced."), *TreatyTarget->CountryName);
        }
        else
        {
            State.CommandAuthority.LastCommandSummary = TEXT("Treaty proposal recorded, but no eligible partner was found during execution.");
        }
    }
    else if (SelectedAction->CommandId.Equals(TEXT("office_impose_sanctions"), ESearchCase::IgnoreCase))
    {
        for (FDemocracyDiplomacyRelationshipState& Relationship : State.DiplomacyMatrix.Relationships)
        {
            if (Relationship.RelationshipStatus.Equals(TEXT("Hostile"), ESearchCase::IgnoreCase) || Relationship.RelationshipStatus.Equals(TEXT("Rival"), ESearchCase::IgnoreCase) || Relationship.BorderTension >= 55)
            {
                Relationship.bSanctionsActive = true;
                Relationship.TradeValue = FMath::Max(0, Relationship.TradeValue - 12);
                Relationship.Trust = FMath::Clamp(Relationship.Trust - 8, 0, 100);
                Relationship.BorderTension = FMath::Clamp(Relationship.BorderTension + 6, 0, 100);
                Relationship.LastChangedTurn = State.Turn;
                Relationship.Notes.AddUnique(TEXT("Sanctions imposed through war command authority."));
                State.WarSystem.EscalationPressure = FMath::Clamp(State.WarSystem.EscalationPressure + 5, 0, 100);
                break;
            }
        }
    }
    else if (SelectedAction->CommandId.Equals(TEXT("office_embargo"), ESearchCase::IgnoreCase))
    {
        for (FDemocracyDiplomacyRelationshipState& Relationship : State.DiplomacyMatrix.Relationships)
        {
            if (Relationship.RelationshipStatus.Equals(TEXT("Hostile"), ESearchCase::IgnoreCase) || Relationship.RelationshipStatus.Equals(TEXT("Rival"), ESearchCase::IgnoreCase) || Relationship.BorderTension >= 55)
            {
                Relationship.bSanctionsActive = true;
                Relationship.TradeValue = FMath::Max(0, Relationship.TradeValue - 8);
                Relationship.Trust = FMath::Clamp(Relationship.Trust - 4, 0, 100);
                Relationship.BorderTension = FMath::Clamp(Relationship.BorderTension + 4, 0, 100);
                Relationship.LastChangedTurn = State.Turn;
                Relationship.Notes.AddUnique(TEXT("Embargo authorized by simulation command authority."));
                break;
            }
        }
    }
    else if (SelectedAction->CommandId.Equals(TEXT("office_trade"), ESearchCase::IgnoreCase))
    {
        for (FDemocracyDiplomacyRelationshipState& Relationship : State.DiplomacyMatrix.Relationships)
        {
            if (!Relationship.RelationshipStatus.Equals(TEXT("Hostile"), ESearchCase::IgnoreCase))
            {
                Relationship.bTradePartner = true;
                Relationship.TradeValue = FMath::Clamp(Relationship.TradeValue + 8, 0, 100);
                Relationship.Trust = FMath::Clamp(Relationship.Trust + 3, 0, 100);
                Relationship.LastChangedTurn = State.Turn;
                Relationship.Notes.AddUnique(TEXT("Trade expanded by simulation command authority."));
                break;
            }
        }
    }
    else if (SelectedAction->CommandId.Equals(TEXT("office_emergency"), ESearchCase::IgnoreCase))
    {
        Country.Policies.CivilPolicy = TEXT("Emergency Powers");
        Country.Policies.LastPolicyChangeSummary = TEXT("Emergency measures declared through command authority.");
        State.FailureRisk.CurrentAssassinationRisk = FMath::Clamp(State.FailureRisk.CurrentAssassinationRisk - 5, 0, State.FailureRisk.AssassinationRiskTrigger);
    }

    SelectedAction->LastExecutedTurn = State.Turn;
    State.CommandAuthority.LastCommandSummary = FString::Printf(TEXT("Turn %d: %s executed from %s. %s"), State.Turn, *SelectedAction->Label, *SurfaceName, *SelectedAction->EffectPreview);
    State.CommandAuthority.ActiveCommandPosture = Country.Policies.CivilPolicy.Equals(TEXT("Emergency Powers"), ESearchCase::IgnoreCase) ? TEXT("Emergency Administration") : TEXT("Civil Administration");

    RecalculateEconomyBudget(State);
    RecalculateDepartments(State);
    RecalculateDemographics(State);
    RecalculateApprovalStability(State);
    RecalculateDiplomacyMatrixSummary(State.DiplomacyMatrix);
    State.AdvisorSystem.GuidanceLevel = AdvisorGuidanceForDifficultyScore(Country.CountrySizeScore);
    State.AdvisorSystem.LastUpdatedTurn = State.Turn;
    State.AdvisorSystem.Reports = GenerateAdvisorReports(State);
    RefreshWarConflictState(State);
    RefreshSimulationToRtsContract(State);
    RefreshCommandAuthority(State);
    LogDecision(State, TEXT("Command Authority"), SelectedAction->Label, FString::Printf(TEXT("%s command executed from %s."), *SelectedAction->CommandType, *SurfaceName), State.CommandAuthority.LastCommandSummary, 38, { TEXT("command"), SelectedAction->CommandType, SelectedAction->AuthorityLayer });

    State.Phase = TEXT("Command Authority Updated");
    LoadedSaveSummary = LoadedSaveState.ToSummaryText();
    SimulationTickSummary = BuildSimulationStatusText();
    LastSaveStatus = FString::Printf(TEXT("Command executed: %s. Save current state to persist it."), *SelectedAction->Label);
    RefreshLoginWidget();
    EvaluateFailState();
    return FReply::Handled();
}

FReply ALoginHUD::HandleSetTaxPolicy(FString TaxPolicyName)
{
    if (!bHasLoadedRuntimeState)
    {
        return FReply::Handled();
    }

    FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    const FBudgetOptionEvaluation Evaluation = EvaluateTaxPolicyRules(State, TaxPolicyName);
    if (!Evaluation.bCanSelect)
    {
        LastSaveStatus = FString::Printf(TEXT("Tax policy blocked: %s"), *Evaluation.Reason);
        State.EconomyBudget.BudgetConstraintStatus = Evaluation.Reason;
        RefreshLoginWidget();
        return FReply::Handled();
    }

    ApplyBudgetPreset(State.EconomyBudget, TaxPolicyName);
    RecalculateEconomyBudget(State);
    RecalculateDemographics(State);
    RecalculateApprovalStability(State);
    State.AdvisorSystem.GuidanceLevel = AdvisorGuidanceForDifficultyScore(State.PlayerCountry.CountrySizeScore);
    State.AdvisorSystem.LastUpdatedTurn = State.Turn;
    InitializeDefaultDepartments(State);
    RecalculateDepartments(State);
    LogDecision(State, TEXT("Tax Policy"), TaxPolicyName, FString::Printf(TEXT("Tax policy changed to %s."), *TaxPolicyName), BuildEconomyBudgetSummaryText(State.EconomyBudget), 30, { TEXT("budget"), TEXT("tax"), TaxPolicyName });
    State.AdvisorSystem.Reports = GenerateAdvisorReports(State);
    State.Phase = TEXT("Budget Updated");
    LoadedSaveSummary = LoadedSaveState.ToSummaryText();
    LastSaveStatus = FString::Printf(TEXT("Tax policy changed. %s Save current state to persist it to disk."), *State.EconomyBudget.BudgetConstraintStatus);
    RefreshLoginWidget();
    EvaluateFailState();
    return FReply::Handled();
}

FReply ALoginHUD::HandleSetSpendingPosture(FString SpendingPostureName)
{
    if (!bHasLoadedRuntimeState)
    {
        return FReply::Handled();
    }

    FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    const FBudgetOptionEvaluation Evaluation = EvaluateSpendingPostureRules(State, SpendingPostureName);
    if (!Evaluation.bCanSelect)
    {
        LastSaveStatus = FString::Printf(TEXT("Spending posture blocked: %s"), *Evaluation.Reason);
        State.EconomyBudget.BudgetConstraintStatus = Evaluation.Reason;
        RefreshLoginWidget();
        return FReply::Handled();
    }

    ApplySpendingPreset(State.EconomyBudget, SpendingPostureName);
    RecalculateEconomyBudget(State);
    RecalculateDemographics(State);
    RecalculateApprovalStability(State);
    State.AdvisorSystem.GuidanceLevel = AdvisorGuidanceForDifficultyScore(State.PlayerCountry.CountrySizeScore);
    State.AdvisorSystem.LastUpdatedTurn = State.Turn;
    InitializeDefaultDepartments(State);
    RecalculateDepartments(State);
    LogDecision(State, TEXT("Spending Posture"), SpendingPostureName, FString::Printf(TEXT("Spending posture changed to %s."), *SpendingPostureName), BuildEconomyBudgetSummaryText(State.EconomyBudget), 30, { TEXT("budget"), TEXT("spending"), SpendingPostureName });
    State.AdvisorSystem.Reports = GenerateAdvisorReports(State);
    State.Phase = TEXT("Budget Updated");
    LoadedSaveSummary = LoadedSaveState.ToSummaryText();
    LastSaveStatus = FString::Printf(TEXT("Spending posture changed. %s Save current state to persist it to disk."), *State.EconomyBudget.BudgetConstraintStatus);
    RefreshLoginWidget();
    EvaluateFailState();
    return FReply::Handled();
}FReply ALoginHUD::HandleOpenDemographicsClicked()
{
    ShowScreen(ELoginFlowScreen::OfficeDemographics);
    return FReply::Handled();
}
FReply ALoginHUD::HandleOpenEventsClicked()
{
    ShowScreen(ELoginFlowScreen::OfficeEvents);
    return FReply::Handled();
}

FReply ALoginHUD::HandleResolveEventChoice(FString EventId, FString ChoiceId)
{
    if (!bHasLoadedRuntimeState)
    {
        return FReply::Handled();
    }

    FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    for (FDemocracyActiveEventState& Event : State.EventSystem.ActiveEvents)
    {
        if (Event.bResolved || !Event.EventId.Equals(EventId, ESearchCase::IgnoreCase))
        {
            continue;
        }

        for (const FDemocracyEventChoiceState& Choice : Event.Choices)
        {
            if (!Choice.ChoiceId.Equals(ChoiceId, ESearchCase::IgnoreCase))
            {
                continue;
            }

            ApplyEventChoiceDeltas(State, Choice);
            Event.bResolved = true;
            Event.CompletionState = TEXT("Resolved - Player Choice");
            Event.SelectedChoiceId = Choice.ChoiceId;
            Event.ResolutionSummary = FString::Printf(TEXT("%s resolved on turn %d with choice: %s. %s"), *Event.Title, State.Turn, *Choice.Label, *Choice.ConsequencePreview);
            State.EventSystem.EventHistory.Add(FString::Printf(TEXT("Turn %d: %s"), State.Turn, *Event.ResolutionSummary));
            LogDecision(State, TEXT("Event Choice"), Event.Title, FString::Printf(TEXT("Selected %s before deadline turn %d."), *Choice.Label, GetEventDeadlineTurn(State, Event)), FString::Printf(TEXT("%s Direct effects: %s"), *Choice.ConsequencePreview, *BuildEventChoiceImpactText(Choice)), Event.Severity, { TEXT("event"), Event.EventType, Choice.ChoiceId });
            State.Phase = TEXT("Event Resolved");
            RebuildSimulationAfterEventChange(State);
            LoadedSaveSummary = LoadedSaveState.ToSummaryText();
            SimulationTickSummary = BuildSimulationStatusText();
            LastSaveStatus = TEXT("Event choice applied. Save current state to persist it to disk.");
            PruneResolvedEvents(State.EventSystem);
            RefreshLoginWidget();
            EvaluateFailState();
            return FReply::Handled();
        }
    }

    LastSaveStatus = TEXT("Event choice was not found or was already resolved.");
    RefreshLoginWidget();
    return FReply::Handled();
}FReply ALoginHUD::HandleSetPolicy(FString PolicyCategory, FString PolicyName)
{
    if (!bHasLoadedRuntimeState)
    {
        return FReply::Handled();
    }

    FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    FDemocracyPolicyState& Policies = State.PlayerCountry.Policies;
    RefreshPolicyRules(State);

    const FPolicyRuleEvaluation RuleEvaluation = EvaluatePolicyRules(State, PolicyCategory, PolicyName);
    if (!RuleEvaluation.bCanSelect)
    {
        LastSaveStatus = FString::Printf(TEXT("Policy blocked: %s"), *RuleEvaluation.Reason);
        Policies.LastPolicyChangeSummary = FString::Printf(TEXT("Blocked %s policy change to %s on turn %d: %s"), *PolicyCategory, *PolicyName, State.Turn, *RuleEvaluation.Reason);
        RefreshPolicyRules(State);
        RefreshLoginWidget();
        return FReply::Handled();
    }

    ApplyPolicySelectionCost(State, RuleEvaluation);
    SetSelectedPolicyForCategory(Policies, PolicyCategory, PolicyName);
    SetPolicyCategoryLastTurn(Policies, PolicyCategory, State.Turn);

    ++Policies.PolicyChangeCount;
    Policies.LastPolicyChangeSummary = FString::Printf(TEXT("%s policy changed to %s on turn %d. Upfront cost: %s"), *PolicyCategory, *PolicyName, State.Turn, *BuildPolicyCostText(RuleEvaluation));
    BuildPolicyModifiers(Policies, &Policies.ActivePolicyEffects);
    RecalculateEconomyBudget(State);
    State.AdvisorSystem.GuidanceLevel = AdvisorGuidanceForDifficultyScore(State.PlayerCountry.CountrySizeScore);
    State.AdvisorSystem.LastUpdatedTurn = State.Turn;
    InitializeDefaultDepartments(State);
    RecalculateDepartments(State);
    RefreshPolicyRules(State);
    LogDecision(State, TEXT("Policy Change"), PolicyName, FString::Printf(TEXT("%s policy changed to %s. %s"), *PolicyCategory, *PolicyName, *BuildPolicyCostText(RuleEvaluation)), BuildPolicyStatusText(), 35, { TEXT("policy"), PolicyCategory, PolicyName });
    State.AdvisorSystem.Reports = GenerateAdvisorReports(State);
    RecalculateDemographics(State);
    if (State.EventSystem.ActiveEventLimit <= 0)
    {
        State.EventSystem.ActiveEventLimit = 3;
    }
    State.Phase = TEXT("Policy Platform Updated");
    LoadedSaveSummary = LoadedSaveState.ToSummaryText();
    LastSaveStatus = TEXT("Policy changed. Costs, cooldowns, conflicts, and prerequisites updated; save current state to persist it to disk.");
    RefreshLoginWidget();
    return FReply::Handled();
}FReply ALoginHUD::HandleSlowerSimulationClicked()
{
    if (bHasLoadedRuntimeState)
    {
        ApplySimulationTickInterval(GetCurrentSimulationTickInterval() * 2.0f);
    }
    return FReply::Handled();
}

FReply ALoginHUD::HandleDefaultSimulationSpeedClicked()
{
    ApplySimulationTickInterval(5.0f);
    return FReply::Handled();
}

FReply ALoginHUD::HandleFasterSimulationClicked()
{
    if (bHasLoadedRuntimeState)
    {
        ApplySimulationTickInterval(GetCurrentSimulationTickInterval() * 0.5f);
    }
    return FReply::Handled();
}
#if !UE_BUILD_SHIPPING
static void RefreshRuntimeAfterDebugTool(FDemocracySimulationState& State)
{
    RebuildSimulationAfterEventChange(State);
    RefreshFailureValidationState(State);
}
#endif
FReply ALoginHUD::HandleRunAutosaveRecoveryTestClicked()
{
#if !UE_BUILD_SHIPPING
    if (!HasGameMasterDebugAccess())
    {
        LastSaveStatus = TEXT("Debug tool denied: autosave recovery test requires GameMaster or Administrator role in single-player.");
        RefreshLoginWidget();
        return FReply::Handled();
    }
#else
    LastSaveStatus = TEXT("Debug tool unavailable in shipping builds.");
    RefreshLoginWidget();
    return FReply::Handled();
#endif
    FString Report;
    FString Error;
    if (FDemocracySaveGameRuntime::RunAutosaveRecoverySelfTest(Report, Error))
    {
        LastSaveStatus = Report;
        UE_LOG(LogTemp, Log, TEXT("%s"), *Report);
    }
    else
    {
        LastSaveStatus = FString::Printf(TEXT("Autosave recovery self-test failed: %s"), *Error);
        UE_LOG(LogTemp, Error, TEXT("Autosave recovery self-test failed: %s"), *Error);
    }
    RefreshLoginWidget();
    return FReply::Handled();
}
#if !UE_BUILD_SHIPPING
// BEGIN SELF-CONTAINED EARLY GAME TEST SCENARIO
// Developer-only smoke path for the simulation side. Remove this block, the dashboard button,
// and HandleRunEarlyGameTestScenarioClicked declaration when bespoke player starts replace it.
FReply ALoginHUD::HandleRunEarlyGameTestScenarioClicked()
{
    if (!HasGameMasterDebugAccess())
    {
        LastSaveStatus = TEXT("Debug tool denied: early-game test scenario requires GameMaster or Administrator role in single-player.");
        RefreshLoginWidget();
        return FReply::Handled();
    }
    TArray<FString> ScenarioLines;
    ScenarioLines.Add(TEXT("Early-game test path started."));

    if (!bHasLoadedRuntimeState)
    {
        PendingDifficulty = TEXT("Easy");
        PendingClimate = TEXT("Middle Moderate");
        PendingLeaderGender = TEXT("Female");
        PendingAddressTitle = GetAddressTitleForGender(PendingLeaderGender);
        PendingStateName = FString::Printf(TEXT("DEV_Early_Test_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Short));

        FString CreatedSavePath;
        if (!CreateInitialSinglePlayerSave(CreatedSavePath))
        {
            LastSaveStatus = TEXT("Early-game test path failed: could not create a test state.");
            RefreshLoginWidget();
            return FReply::Handled();
        }
        ScenarioLines.Add(FString::Printf(TEXT("1. Created test state and save: %s"), *FPaths::GetCleanFilename(CreatedSavePath)));
    }
    else
    {
        ScenarioLines.Add(FString::Printf(TEXT("1. Reused loaded state: %s"), *LoadedStateName));
    }

    FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    FDemocracyCountryState& Country = State.PlayerCountry;
    InitializeEarlyGameBalanceTestData(State);
    RefreshFailureValidationState(State);
    ScenarioLines.Add(TEXT("2. First briefing generated without forcing an overlay or moving the player."));
    ScenarioLines.Add(FString::Printf(TEXT("   Briefing preview: %s"), *GetOpeningScriptText().Left(160)));

    CurrentScreen = ELoginFlowScreen::OfficeComputerMenu;
    ScenarioLines.Add(TEXT("3. Dashboard path opened on the computer menu."));

    RefreshPolicyRules(State);
    const FString PolicyCategory = TEXT("Economic");
    const FString PolicyName = TEXT("Industrial Subsidies");
    FPolicyRuleEvaluation PolicyEvaluation = EvaluatePolicyRules(State, PolicyCategory, PolicyName);
    if (!PolicyEvaluation.bCanSelect)
    {
        Country.Technology = FMath::Max(Country.Technology, 1);
        Country.Treasury = FMath::Max(Country.Treasury, 500);
        PolicyEvaluation = EvaluatePolicyRules(State, PolicyCategory, PolicyName);
    }
    if (PolicyEvaluation.bCanSelect)
    {
        ApplyPolicySelectionCost(State, PolicyEvaluation);
        SetSelectedPolicyForCategory(Country.Policies, PolicyCategory, PolicyName);
        SetPolicyCategoryLastTurn(Country.Policies, PolicyCategory, State.Turn);
        ++Country.Policies.PolicyChangeCount;
        Country.Policies.LastPolicyChangeSummary = FString::Printf(TEXT("Early-game test path selected %s."), *PolicyName);
        BuildPolicyModifiers(Country.Policies, &Country.Policies.ActivePolicyEffects);
        RefreshPolicyRules(State);
        LogDecision(State, TEXT("Early Test Scenario"), TEXT("Policy Choice"), FString::Printf(TEXT("Selected %s policy: %s."), *PolicyCategory, *PolicyName), BuildPolicyStatusText(), 1, { TEXT("debug"), TEXT("early-test"), TEXT("policy") });
        ScenarioLines.Add(FString::Printf(TEXT("4. Chose policy: %s / %s."), *PolicyCategory, *PolicyName));
    }
    else
    {
        ScenarioLines.Add(FString::Printf(TEXT("4. Policy choice blocked: %s"), *PolicyEvaluation.Reason));
    }

    FDemocracyActiveEventState ScenarioEvent = MakeEvent(State, TEXT("Early Test Event"), TEXT("Early Test Supply Decision"),
        TEXT("A controlled test event verifies the event choice screen, direct consequences, history, and follow-up state."),
        TEXT("Triggered by self-contained early-game test scenario."), 42, true, {
            MakeEventChoice(TEXT("stabilize"), TEXT("Stabilize supply lines"), TEXT("Use treasury and logistics to stabilize the first supply warning."), TEXT("Resources improve, unrest drops, treasury falls."), 3, 3, -4, -80, 1, 0, 0, 1, 0, 35, 25, 10, 5, 5, -2, -1),
            MakeEventChoice(TEXT("delay"), TEXT("Delay response"), TEXT("Hold reserves while waiting for more data."), TEXT("Treasury protected, unrest and risk rise."), -2, -2, 4, 20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1)
        });
    State.EventSystem.ActiveEventLimit = FMath::Max(State.EventSystem.ActiveEventLimit, State.EventSystem.ActiveEvents.Num() + 1);
    State.EventSystem.ActiveEvents.Add(ScenarioEvent);
    State.EventSystem.LastEventTurn = State.Turn;
    ScenarioLines.Add(FString::Printf(TEXT("5. Triggered event: %s."), *ScenarioEvent.Title));

    for (FDemocracyActiveEventState& Event : State.EventSystem.ActiveEvents)
    {
        if (!Event.EventId.Equals(ScenarioEvent.EventId, ESearchCase::IgnoreCase) || Event.Choices.Num() == 0)
        {
            continue;
        }
        const FDemocracyEventChoiceState Choice = Event.Choices[0];
        ApplyEventChoiceDeltas(State, Choice);
        Event.bResolved = true;
        Event.CompletionState = TEXT("Resolved - Early Test Scenario");
        Event.SelectedChoiceId = Choice.ChoiceId;
        Event.ResolutionSummary = FString::Printf(TEXT("%s resolved by early-game test path with choice: %s."), *Event.Title, *Choice.Label);
        State.EventSystem.EventHistory.Add(FString::Printf(TEXT("Turn %d: %s"), State.Turn, *Event.ResolutionSummary));
        LogDecision(State, TEXT("Early Test Scenario"), TEXT("Event Choice"), Event.ResolutionSummary, BuildEventChoiceImpactText(Choice), 1, { TEXT("debug"), TEXT("early-test"), TEXT("event") });
        ScenarioLines.Add(FString::Printf(TEXT("6. Resolved event choice: %s."), *Choice.Label));
        break;
    }
    PruneResolvedEvents(State.EventSystem);
    RebuildSimulationAfterEventChange(State);
    RefreshFailureValidationState(State);

    HandleHoldMeeting(TEXT("Advisor"), TEXT("Economic Advisor"), TEXT("Economic Budget Review"));
    ScenarioLines.Add(TEXT("7. Held advisor meeting: Economic Advisor / Budget Review."));

    HandleMakePressRelease(TEXT("Policy Explanation"));
    ScenarioLines.Add(TEXT("8. Issued press release: Policy Explanation."));

    FString SaveError;
    const FString SavePathBeforeReload = LoadedSaveState.SavePath;
    if (FDemocracySaveGameRuntime::SaveSinglePlayerRuntimeState(LoadedSaveState, SaveError))
    {
        ScenarioLines.Add(TEXT("9. Saved runtime state."));
        if (!SavePathBeforeReload.IsEmpty() && LoadSinglePlayerSaveIntoRuntime(SavePathBeforeReload))
        {
            ScenarioLines.Add(TEXT("10. Loaded saved state back into runtime."));
        }
        else
        {
            ScenarioLines.Add(TEXT("10. Reload skipped or failed; current runtime state remained active."));
        }
    }
    else
    {
        ScenarioLines.Add(FString::Printf(TEXT("9. Save failed: %s"), *SaveError));
    }

    if (bHasLoadedRuntimeState)
    {
        const bool bWasPaused = LoadedSaveState.RuntimeState.bPaused;
        LoadedSaveState.RuntimeState.bPaused = false;
        for (int32 TickIndex = 0; TickIndex < 3 && CurrentScreen != ELoginFlowScreen::GameOver; ++TickIndex)
        {
            RunSimulationTick();
        }
        if (CurrentScreen != ELoginFlowScreen::GameOver)
        {
            LoadedSaveState.RuntimeState.bPaused = bWasPaused;
            RefreshRuntimeAfterDebugTool(LoadedSaveState.RuntimeState);
            ScenarioLines.Add(TEXT("11. Advanced three simulation ticks."));
        }
    }

    if (bHasLoadedRuntimeState && CurrentScreen != ELoginFlowScreen::GameOver)
    {
        LogDecision(LoadedSaveState.RuntimeState, TEXT("Early Test Scenario"), TEXT("Scenario Complete"), TEXT("Completed self-contained early-game simulation smoke path."), FString::Join(ScenarioLines, TEXT("\n")), 1, { TEXT("debug"), TEXT("early-test"), TEXT("complete") });
        LoadedSaveSummary = LoadedSaveState.ToSummaryText();
        SimulationTickSummary = FString::Printf(TEXT("Early-game test scenario complete:\n%s\n\nLatest tick:\n%s"), *FString::Join(ScenarioLines, TEXT("\n")), *SimulationTickSummary);
        LastSaveStatus = TEXT("Early-game test scenario complete. This is developer-only and should be removed when bespoke starts replace it.");
        CurrentScreen = ELoginFlowScreen::OfficeComputerMenu;
        RefreshLoginWidget();
        EvaluateFailState();
    }
    return FReply::Handled();
}
// END SELF-CONTAINED EARLY GAME TEST SCENARIO
FReply ALoginHUD::HandleDebugAddResourcesClicked()
{
    if (!HasAdministratorDebugAccess())
    {
        LastSaveStatus = TEXT("Debug tool denied: adding resources requires Administrator role in single-player.");
        RefreshLoginWidget();
        return FReply::Handled();
    }
    if (!bHasLoadedRuntimeState)
    {
        LastSaveStatus = TEXT("Debug tool unavailable: no runtime state loaded.");
        RefreshLoginWidget();
        return FReply::Handled();
    }

    FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    FDemocracyCountryState& Country = State.PlayerCountry;
    FDemocracyResourceInventory& Resources = Country.Resources;
    Resources.Food += 250;
    Resources.Water += 200;
    Resources.GasOil += 150;
    Resources.Wood += 150;
    Resources.Metals += 150;
    Country.Treasury += 250;
    Country.PublicApproval = FMath::Clamp(Country.PublicApproval + 3, 0, 100);

    RefreshRuntimeAfterDebugTool(State);
    LogDecision(State, TEXT("Debug Tool"), TEXT("Add Resources"), TEXT("Developer resource grant applied from the computer dashboard."), TEXT("Food +250, water +200, fuel +150, wood +150, metals +150, treasury +250."), 1, { TEXT("debug"), TEXT("resources") });
    LoadedSaveSummary = LoadedSaveState.ToSummaryText();
    SimulationTickSummary = BuildSimulationStatusText();
    LastSaveStatus = TEXT("Debug resources added. Save current state if you want to persist this test state.");
    RefreshLoginWidget();
    return FReply::Handled();
}

FReply ALoginHUD::HandleDebugTriggerEventClicked()
{
    if (!HasGameMasterDebugAccess())
    {
        LastSaveStatus = TEXT("Debug tool denied: triggering test events requires GameMaster or Administrator role in single-player.");
        RefreshLoginWidget();
        return FReply::Handled();
    }
    if (!bHasLoadedRuntimeState)
    {
        LastSaveStatus = TEXT("Debug tool unavailable: no runtime state loaded.");
        RefreshLoginWidget();
        return FReply::Handled();
    }

    FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    FDemocracyEventSystemState& EventSystem = State.EventSystem;
    PruneResolvedEvents(EventSystem);
    EventSystem.ActiveEventLimit = FMath::Max(EventSystem.ActiveEventLimit, EventSystem.ActiveEvents.Num() + 1);

    FDemocracyActiveEventState Event = MakeEvent(State, TEXT("Debug Crisis"), TEXT("Developer Triggered Crisis"),
        TEXT("This synthetic crisis exists so event choices, deadlines, advisor warnings, and follow-up penalties can be tested quickly."),
        TEXT("Triggered manually from developer test tools."), 62, true, {
            MakeEventChoice(TEXT("stabilize"), TEXT("Emergency response"), TEXT("Spend treasury and department capacity to stabilize the crisis."), TEXT("Approval and stability improve, treasury falls, risk eases."), 5, 5, -7, -120, 1, 1, 0, 1, 0, 35, 25, 20, 10, 10, -5, -4),
            MakeEventChoice(TEXT("delay"), TEXT("Delay response"), TEXT("Keep funds available while accepting public frustration and escalation risk."), TEXT("Treasury protected, unrest and risk rise."), -5, -5, 8, 20, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, 6, 5)
        });
    EventSystem.ActiveEvents.Add(Event);
    EventSystem.LastEventTurn = State.Turn;
    State.Phase = TEXT("Debug Event Pending");

    RefreshRuntimeAfterDebugTool(State);
    LogDecision(State, TEXT("Debug Tool"), TEXT("Trigger Event"), TEXT("Developer-triggered event added to the active event list."), Event.Title, 1, { TEXT("debug"), TEXT("event"), Event.EventId });
    LoadedSaveSummary = LoadedSaveState.ToSummaryText();
    SimulationTickSummary = BuildSimulationStatusText();
    LastSaveStatus = FString::Printf(TEXT("Debug event added: %s. Open Events to resolve or let the deadline expire."), *Event.Title);
    RefreshLoginWidget();
    return FReply::Handled();
}

FReply ALoginHUD::HandleDebugForceUnrestClicked()
{
    if (!HasAdministratorDebugAccess())
    {
        LastSaveStatus = TEXT("Debug tool denied: forcing unrest requires Administrator role in single-player.");
        RefreshLoginWidget();
        return FReply::Handled();
    }
    if (!bHasLoadedRuntimeState)
    {
        LastSaveStatus = TEXT("Debug tool unavailable: no runtime state loaded.");
        RefreshLoginWidget();
        return FReply::Handled();
    }

    FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    FDemocracyCountryState& Country = State.PlayerCountry;
    Country.Unrest = FMath::Clamp(Country.Unrest + 35, 0, 100);
    Country.Stability = FMath::Clamp(Country.Stability - 20, 0, 100);
    Country.PublicApproval = FMath::Clamp(Country.PublicApproval - 12, 0, 100);
    State.FailureRisk.CurrentAssassinationRisk = FMath::Clamp(State.FailureRisk.CurrentAssassinationRisk + 18, 0, State.FailureRisk.AssassinationRiskTrigger);
    State.Phase = TEXT("Debug Unrest Forced");

    RefreshRuntimeAfterDebugTool(State);
    LogDecision(State, TEXT("Debug Tool"), TEXT("Force Unrest"), TEXT("Developer forced unrest and lowered stability to test warnings and fail-state validation."), BuildGuidedFailureWarningText(State), 1, { TEXT("debug"), TEXT("unrest") });
    LoadedSaveSummary = LoadedSaveState.ToSummaryText();
    SimulationTickSummary = BuildSimulationStatusText();
    LastSaveStatus = TEXT("Debug unrest applied. Failure warnings and advisor reports refreshed.");
    RefreshLoginWidget();
    EvaluateFailState();
    return FReply::Handled();
}

FReply ALoginHUD::HandleDebugForceInvasionRiskClicked()
{
    if (!HasAdministratorDebugAccess())
    {
        LastSaveStatus = TEXT("Debug tool denied: forcing takeover risk requires Administrator role in single-player.");
        RefreshLoginWidget();
        return FReply::Handled();
    }
    if (!bHasLoadedRuntimeState)
    {
        LastSaveStatus = TEXT("Debug tool unavailable: no runtime state loaded.");
        RefreshLoginWidget();
        return FReply::Handled();
    }

    FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    FDemocracyCountryState& Country = State.PlayerCountry;
    State.InvasionRisk.CurrentInvasionRisk = FMath::Clamp(FMath::Max(State.InvasionRisk.CurrentInvasionRisk + 28, State.InvasionRisk.InvasionRiskTrigger * 65 / 100), 0, State.InvasionRisk.InvasionRiskTrigger);
    Country.MilitaryReadiness = FMath::Clamp(Country.MilitaryReadiness - 18, 0, 100);
    Country.DiplomaticStanding = FMath::Clamp(Country.DiplomaticStanding - 14, 0, 100);
    Country.Stability = FMath::Clamp(Country.Stability - 8, 0, 100);
    State.Phase = TEXT("Debug Takeover Risk Forced");

    RefreshRuntimeAfterDebugTool(State);
    LogDecision(State, TEXT("Debug Tool"), TEXT("Force Takeover Risk"), TEXT("Developer raised foreign takeover pressure to test invasion warnings and fail-state validation."), BuildGuidedFailureWarningText(State), 1, { TEXT("debug"), TEXT("invasion") });
    LoadedSaveSummary = LoadedSaveState.ToSummaryText();
    SimulationTickSummary = BuildSimulationStatusText();
    LastSaveStatus = TEXT("Debug takeover risk applied. Failure warnings and advisor reports refreshed.");
    RefreshLoginWidget();
    EvaluateFailState();
    return FReply::Handled();
}

FReply ALoginHUD::HandleDebugAdvanceTimeClicked()
{
    if (!HasGameMasterDebugAccess())
    {
        LastSaveStatus = TEXT("Debug tool denied: advancing test time requires GameMaster or Administrator role in single-player.");
        RefreshLoginWidget();
        return FReply::Handled();
    }
    if (!bHasLoadedRuntimeState)
    {
        LastSaveStatus = TEXT("Debug tool unavailable: no runtime state loaded.");
        RefreshLoginWidget();
        return FReply::Handled();
    }

    const bool bWasPaused = LoadedSaveState.RuntimeState.bPaused;
    LoadedSaveState.RuntimeState.bPaused = false;
    for (int32 TickIndex = 0; TickIndex < 3 && bHasLoadedRuntimeState && CurrentScreen != ELoginFlowScreen::GameOver; ++TickIndex)
    {
        RunSimulationTick();
    }
    if (bHasLoadedRuntimeState && CurrentScreen != ELoginFlowScreen::GameOver)
    {
        LoadedSaveState.RuntimeState.bPaused = bWasPaused;
        RefreshRuntimeAfterDebugTool(LoadedSaveState.RuntimeState);
        LogDecision(LoadedSaveState.RuntimeState, TEXT("Debug Tool"), TEXT("Advance Time"), TEXT("Developer advanced three simulation ticks from the computer dashboard."), SimulationTickSummary, 1, { TEXT("debug"), TEXT("time") });
        LoadedSaveSummary = LoadedSaveState.ToSummaryText();
        SimulationTickSummary = BuildSimulationStatusText();
        LastSaveStatus = TEXT("Debug advanced three simulation ticks. Save current state if you want to persist the result.");
        RefreshLoginWidget();
    }
    return FReply::Handled();
}

FReply ALoginHUD::HandleDebugTestGameOverClicked()
{
    if (!HasAdministratorDebugAccess())
    {
        LastSaveStatus = TEXT("Debug tool denied: forcing game over requires Administrator role in single-player.");
        RefreshLoginWidget();
        return FReply::Handled();
    }
    if (!bHasLoadedRuntimeState)
    {
        LastSaveStatus = TEXT("Debug tool unavailable: no runtime state loaded.");
        RefreshLoginWidget();
        return FReply::Handled();
    }

    FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    FDemocracyCountryState& Country = State.PlayerCountry;
    Country.Unrest = 100;
    Country.Stability = 0;
    Country.PublicApproval = FMath::Min(Country.PublicApproval, 10);
    State.FailureRisk.CurrentAssassinationRisk = State.FailureRisk.AssassinationRiskTrigger;
    State.FailureRisk.WarningLevel = TEXT("Critical");
    State.Phase = TEXT("Debug Game Over Forced");

    RefreshRuntimeAfterDebugTool(State);
    LogDecision(State, TEXT("Debug Tool"), TEXT("Test Game Over"), TEXT("Developer forced critical assassination/collapse conditions to test game-over flow and protected reload."), BuildGuidedFailureWarningText(State), 1, { TEXT("debug"), TEXT("game-over") });
    LoadedSaveSummary = LoadedSaveState.ToSummaryText();
    SimulationTickSummary = BuildSimulationStatusText();
    LastSaveStatus = TEXT("Debug game-over conditions applied.");
    if (!EvaluateFailState())
    {
        RefreshLoginWidget();
    }
    return FReply::Handled();
}
FReply ALoginHUD::HandleRunRtsSaveLoadPlaytestClicked()
{
    if (!HasAdministratorDebugAccess())
    {
        LastSaveStatus = TEXT("Debug tool denied: RTS save/load playtest requires Administrator role in single-player.");
        RefreshLoginWidget();
        return FReply::Handled();
    }
    if (!bHasLoadedRuntimeState || LoadedSavePath.IsEmpty())
    {
        LastSaveStatus = TEXT("RTS save/load playtest unavailable: load a single-player save first.");
        RefreshLoginWidget();
        return FReply::Handled();
    }

    FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    InitializeRuntimeMapOwnershipIfMissing(State);
    RefreshRtsFogOfWar(State);
    RefreshRtsHudState(State);

    if (State.RtsWorld.ArmyGroups.Num() == 0 || State.RtsWorld.Ownership.Provinces.Num() == 0)
    {
        LastSaveStatus = TEXT("RTS save/load playtest failed: RTS armies or province ownership data are missing.");
        RefreshLoginWidget();
        return FReply::Handled();
    }

    FDemocracyRtsArmyGroupState& TestArmy = State.RtsWorld.ArmyGroups[0];
    if (TestArmy.CurrentProvinceId.IsEmpty())
    {
        TestArmy.CurrentProvinceId = State.RtsWorld.CityBase.LinkedProvinceId;
    }
    TestArmy.TotalStrength = FMath::Max(TestArmy.TotalStrength, 260);
    TestArmy.InfantryCount = FMath::Max(TestArmy.InfantryCount, 8);
    TestArmy.VehicleCount = FMath::Max(TestArmy.VehicleCount, 4);
    TestArmy.LogisticsCount = FMath::Max(TestArmy.LogisticsCount, 3);
    TestArmy.SupplyStatus = 100;
    TestArmy.Morale = 95;
    TestArmy.bSupplyRouteBroken = false;
    TestArmy.bSelected = true;

    const FString PlayerCountryName = State.PlayerCountry.CountryName;
    FString TargetProvinceId;
    FString TargetProvinceName;
    for (const FDemocracyProvinceOwnershipState& Province : State.RtsWorld.Ownership.Provinces)
    {
        if (!Province.CurrentControllerCountryName.Equals(PlayerCountryName, ESearchCase::IgnoreCase) && !Province.ProvinceId.Equals(TestArmy.CurrentProvinceId, ESearchCase::IgnoreCase))
        {
            TargetProvinceId = Province.ProvinceId;
            TargetProvinceName = Province.ProvinceName;
            break;
        }
    }
    if (TargetProvinceId.IsEmpty())
    {
        LastSaveStatus = TEXT("RTS save/load playtest failed: no hostile province is available to attack.");
        RefreshLoginWidget();
        return FReply::Handled();
    }

    const int32 StartingTurn = State.Turn;
    const int32 StartingBattleCount = State.RtsWorld.BattleHistory.Num();
    const int32 StartingOutcomeHistoryCount = State.RtsWorld.Backflow.OutcomeHistory.Num();
    const int32 StartingHudAlertCount = State.RtsWorld.Hud.Alerts.Num();
    const int32 StartingControlledCount = State.RtsWorld.Ownership.PlayerControlledProvinces;

    RtsSelectedArmyId = TestArmy.ArmyId;
    PendingRtsOrderType = TEXT("Move");
    if (!TryIssueRtsOrderToProvince(TargetProvinceId))
    {
        LastSaveStatus = FString::Printf(TEXT("RTS save/load playtest failed: could not issue move order to %s."), *TargetProvinceId);
        RefreshLoginWidget();
        return FReply::Handled();
    }

    for (int32 TickIndex = 0; TickIndex < 10; ++TickIndex)
    {
        ++State.Turn;
        State.RtsWorld.SimulationSecond += 5;
        TickRtsMovementOrdersAndSupply(State, true);
        RefreshRtsFogOfWar(State);
        RefreshRtsHudState(State);
        ApplyPendingRtsBackflow(State);
    }
    ResolveRtsOccupationOwnershipTimers(State);
    RefreshRtsFogOfWar(State);
    RefreshRtsHudState(State);
    RefreshWarConflictState(State);
    RefreshSimulationToRtsContract(State);

    FString ChangedProvinceId;
    FString ChangedProvinceController;
    FString ChangedProvinceOwner;
    int32 ChangedProvinceTurn = 0;
    for (const FDemocracyProvinceOwnershipState& Province : State.RtsWorld.Ownership.Provinces)
    {
        if (Province.LastChangedTurn >= StartingTurn && Province.CurrentControllerCountryName.Equals(PlayerCountryName, ESearchCase::IgnoreCase) && !Province.ProvinceId.Equals(TestArmy.CurrentProvinceId, ESearchCase::IgnoreCase))
        {
            ChangedProvinceId = Province.ProvinceId;
            ChangedProvinceController = Province.CurrentControllerCountryName;
            ChangedProvinceOwner = Province.CurrentOwnerCountryName;
            ChangedProvinceTurn = Province.LastChangedTurn;
            break;
        }
    }

    const int32 ExpectedBattleCount = State.RtsWorld.BattleHistory.Num();
    const int32 ExpectedOrderCount = State.RtsWorld.MovementOrders.Num();
    const int32 ExpectedOutcomeHistoryCount = State.RtsWorld.Backflow.OutcomeHistory.Num();
    const int32 ExpectedHudAlertCount = State.RtsWorld.Hud.Alerts.Num();
    const FString ExpectedHudAlertSummary = State.RtsWorld.Hud.AlertSummary;
    const int32 ExpectedControlledCount = State.RtsWorld.Ownership.PlayerControlledProvinces;
    const int32 ExpectedConstructionQueueCount = State.RtsWorld.CityBase.ConstructionQueue.Num();
    const int32 ExpectedFogProvinceCount = State.RtsWorld.FogOfWar.Provinces.Num();
    const FString ExpectedArmyProvince = State.RtsWorld.ArmyGroups[0].CurrentProvinceId;
    const FString ExpectedLastBattleProvince = State.RtsWorld.BattleHistory.Num() > 0 ? State.RtsWorld.BattleHistory.Last().ProvinceId : TEXT("");
    const FString ExpectedLastBattleResult = State.RtsWorld.BattleHistory.Num() > 0 ? State.RtsWorld.BattleHistory.Last().Result : TEXT("No Battle");

    const bool bBattleRan = ExpectedBattleCount > StartingBattleCount;
    const bool bBackflowRan = ExpectedOutcomeHistoryCount > StartingOutcomeHistoryCount;
    const bool bOfficeAlertsRan = ExpectedHudAlertCount > StartingHudAlertCount || !ExpectedHudAlertSummary.Equals(TEXT("No RTS alerts."), ESearchCase::IgnoreCase);
    const bool bCaptureRan = ExpectedControlledCount > StartingControlledCount || !ChangedProvinceId.IsEmpty();

    FString SaveError;
    if (!FDemocracySaveGameRuntime::SaveSinglePlayerRuntimeState(LoadedSaveState, SaveError))
    {
        LastSaveStatus = FString::Printf(TEXT("RTS save/load playtest failed during save: %s"), *SaveError);
        RefreshLoginWidget();
        return FReply::Handled();
    }

    FDemocracyLoadedSaveState ReloadedSave;
    FString ReloadError;
    if (!FDemocracySaveGameRuntime::LoadSinglePlayerSaveWithFallback(LoadedSavePath, ReloadedSave, ReloadError))
    {
        LastSaveStatus = FString::Printf(TEXT("RTS save/load playtest failed during reload: %s"), *ReloadError);
        RefreshLoginWidget();
        return FReply::Handled();
    }

    const FDemocracyRtsWorldState& ReloadedRts = ReloadedSave.RuntimeState.RtsWorld;
    const bool bArmyPersisted = ReloadedRts.ArmyGroups.Num() > 0 && ReloadedRts.ArmyGroups[0].CurrentProvinceId.Equals(ExpectedArmyProvince, ESearchCase::IgnoreCase);
    const bool bOrdersPersisted = ReloadedRts.MovementOrders.Num() == ExpectedOrderCount;
    const bool bBattlesPersisted = ReloadedRts.BattleHistory.Num() == ExpectedBattleCount && (ExpectedLastBattleProvince.IsEmpty() || ReloadedRts.BattleHistory.Last().ProvinceId.Equals(ExpectedLastBattleProvince, ESearchCase::IgnoreCase));
    const bool bBackflowPersisted = ReloadedRts.Backflow.OutcomeHistory.Num() == ExpectedOutcomeHistoryCount;
    const bool bOfficeAlertsPersisted = ReloadedRts.Hud.Alerts.Num() == ExpectedHudAlertCount && ReloadedRts.Hud.AlertSummary.Equals(ExpectedHudAlertSummary, ESearchCase::IgnoreCase);
    const bool bConstructionPersisted = ReloadedRts.CityBase.ConstructionQueue.Num() == ExpectedConstructionQueueCount;
    const bool bFogPersisted = ReloadedRts.FogOfWar.Provinces.Num() == ExpectedFogProvinceCount;
    bool bCapturePersisted = ChangedProvinceId.IsEmpty();
    if (!ChangedProvinceId.IsEmpty())
    {
        for (const FDemocracyProvinceOwnershipState& Province : ReloadedRts.Ownership.Provinces)
        {
            if (Province.ProvinceId.Equals(ChangedProvinceId, ESearchCase::IgnoreCase))
            {
                bCapturePersisted = Province.CurrentControllerCountryName.Equals(ChangedProvinceController, ESearchCase::IgnoreCase)
                    && Province.CurrentOwnerCountryName.Equals(ChangedProvinceOwner, ESearchCase::IgnoreCase)
                    && Province.LastChangedTurn == ChangedProvinceTurn;
                break;
            }
        }
    }

    const bool bPassed = bBattleRan && bBackflowRan && bOfficeAlertsRan && bCaptureRan && bArmyPersisted && bOrdersPersisted && bBattlesPersisted && bBackflowPersisted && bOfficeAlertsPersisted && bCapturePersisted && bConstructionPersisted && bFogPersisted;
    LoadedSaveState = ReloadedSave;
    LoadedSavePath = ReloadedSave.SavePath;
    LoadedStateName = ReloadedSave.StateName;
    LoadedSaveSummary = LoadedSaveState.ToSummaryText();
    SimulationTickSummary = BuildSimulationStatusText();
    bHasLoadedRuntimeState = true;

    TArray<FString> ReportLines;
    ReportLines.Add(bPassed ? TEXT("RTS save/load playtest passed.") : TEXT("RTS save/load playtest found an issue."));
    ReportLines.Add(FString::Printf(TEXT("Order: %s -> %s | battle result %s | target battle province %s."), *RtsSelectedArmyId, *TargetProvinceName, *ExpectedLastBattleResult, *ExpectedLastBattleProvince));
    ReportLines.Add(FString::Printf(TEXT("Runtime changed: battles %d->%d, outcome history %d->%d, controlled provinces %d->%d, changed province %s controller %s owner %s."), StartingBattleCount, ExpectedBattleCount, StartingOutcomeHistoryCount, ExpectedOutcomeHistoryCount, StartingControlledCount, ExpectedControlledCount, ChangedProvinceId.IsEmpty() ? TEXT("none") : *ChangedProvinceId, *ChangedProvinceController, *ChangedProvinceOwner));
    ReportLines.Add(FString::Printf(TEXT("Reload verified: army %s, orders %s, battles %s, capture %s, backflow %s, office alerts %s, construction %s, fog %s."), bArmyPersisted ? TEXT("ok") : TEXT("failed"), bOrdersPersisted ? TEXT("ok") : TEXT("failed"), bBattlesPersisted ? TEXT("ok") : TEXT("failed"), bCapturePersisted ? TEXT("ok") : TEXT("failed"), bBackflowPersisted ? TEXT("ok") : TEXT("failed"), bOfficeAlertsPersisted ? TEXT("ok") : TEXT("failed"), bConstructionPersisted ? TEXT("ok") : TEXT("failed"), bFogPersisted ? TEXT("ok") : TEXT("failed")));
    ReportLines.Add(FString::Printf(TEXT("Office alerts: %d->%d | generated %s | summary %s"), StartingHudAlertCount, ExpectedHudAlertCount, bOfficeAlertsRan ? TEXT("ok") : TEXT("failed"), ExpectedHudAlertSummary.IsEmpty() ? TEXT("none") : *ExpectedHudAlertSummary));
    LastSaveStatus = FString::Join(ReportLines, TEXT("\n"));
    LoadedSaveState.RuntimeState.RtsWorld.WorldInteraction.LastInteractionSummary = LastSaveStatus;
    LogDecision(LoadedSaveState.RuntimeState, TEXT("Debug Tool"), TEXT("RTS Save/Load Playtest"), TEXT("Administrator ran the RTS movement, battle, capture, save, reload persistence path."), LastSaveStatus, bPassed ? 1 : 70, { TEXT("debug"), TEXT("rts"), TEXT("save-load") });
    UE_LOG(LogTemp, Log, TEXT("%s"), *LastSaveStatus);
    RefreshLoginWidget();
    return FReply::Handled();
}
#endif
FReply ALoginHUD::HandleSaveRuntimeStateClicked()
{
    if (!bHasLoadedRuntimeState)
    {
        LastSaveStatus = TEXT("No runtime state is loaded.");
        RefreshLoginWidget();
        return FReply::Handled();
    }

    RefreshWarConflictState(LoadedSaveState.RuntimeState);
    RefreshSimulationToRtsContract(LoadedSaveState.RuntimeState);

    FString SaveError;
    if (FDemocracySaveGameRuntime::SaveSinglePlayerRuntimeState(LoadedSaveState, SaveError))
    {
        LoadedSaveSummary = LoadedSaveState.ToSummaryText();
        LastSaveStatus = FString::Printf(TEXT("Saved current runtime state at %s."), *LoadedSaveState.LastPlayedAtUtc);
    }
    else
    {
        LastSaveStatus = SaveError;
        UE_LOG(LogTemp, Error, TEXT("Save current runtime state failed: %s"), *SaveError);
    }

    RefreshLoginWidget();
    return FReply::Handled();
}

FReply ALoginHUD::HandleHoldMeeting(FString MeetingType, FString ParticipantName, FString AgendaItem)
{
    if (!bHasLoadedRuntimeState)
    {
        return FReply::Handled();
    }

    FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    FDemocracyCountryState& Country = State.PlayerCountry;
    FDemocracyMeetingSystemState& MeetingSystem = State.MeetingSystem;
    InitializeMeetingSystemIfMissing(State);

    FDemocracyMeetingRecordState Record;
    Record.Turn = State.Turn;
    Record.MeetingType = MeetingType;
    Record.ParticipantName = ParticipantName;
    Record.AgendaItem = AgendaItem;

    const bool bEmergency = AgendaItem.Equals(TEXT("Emergency Response"), ESearchCase::IgnoreCase)
        || AgendaItem.Contains(TEXT("Emergency"), ESearchCase::IgnoreCase)
        || AgendaItem.Contains(TEXT("Crisis"), ESearchCase::IgnoreCase)
        || AgendaItem.Contains(TEXT("Mobilization"), ESearchCase::IgnoreCase)
        || AgendaItem.Contains(TEXT("Surge"), ESearchCase::IgnoreCase)
        || AgendaItem.Contains(TEXT("Counter Threat"), ESearchCase::IgnoreCase);
    const bool bFocused = AgendaItem.Equals(TEXT("Focused Action Plan"), ESearchCase::IgnoreCase)
        || AgendaItem.Contains(TEXT("Plan"), ESearchCase::IgnoreCase)
        || AgendaItem.Contains(TEXT("Package"), ESearchCase::IgnoreCase)
        || AgendaItem.Contains(TEXT("Program"), ESearchCase::IgnoreCase)
        || AgendaItem.Contains(TEXT("Outreach"), ESearchCase::IgnoreCase)
        || AgendaItem.Contains(TEXT("Upgrade"), ESearchCase::IgnoreCase)
        || AgendaItem.Contains(TEXT("Expansion"), ESearchCase::IgnoreCase);
    const bool bBriefing = AgendaItem.Equals(TEXT("Situation Briefing"), ESearchCase::IgnoreCase)
        || AgendaItem.Contains(TEXT("Briefing"), ESearchCase::IgnoreCase)
        || AgendaItem.Contains(TEXT("Review"), ESearchCase::IgnoreCase)
        || AgendaItem.Contains(TEXT("Assessment"), ESearchCase::IgnoreCase)
        || AgendaItem.Contains(TEXT("Audit"), ESearchCase::IgnoreCase);
    const int32 CoordinationBonus = FMath::Clamp((MeetingSystem.AdvisorCoordination - 45) / 15, -1, 2);
    const int32 ForeignTrustBonus = FMath::Clamp((MeetingSystem.ForeignTrust - 45) / 15, -1, 2);

    if (MeetingType.Equals(TEXT("Foreign Official"), ESearchCase::IgnoreCase))
    {
        if (AgendaItem.Equals(TEXT("Trade Delegation"), ESearchCase::IgnoreCase))
        {
            Record.DiplomacyDelta = 2 + ForeignTrustBonus;
            Record.TreasuryDelta = 45 + MeetingSystem.ForeignTrust / 3;
            Record.EconomyDelta = 1;
            Record.ForeignTrustDelta = 2;
            Country.Resources.Food = FMath::Max(0, Country.Resources.Food + 8);
            Country.Resources.GasOil = FMath::Max(0, Country.Resources.GasOil + 5);
            Record.OutcomeSummary = TEXT("Trade delegates agreed to limited imports and commercial access.");
        }
        else if (AgendaItem.Equals(TEXT("De-escalation Talks"), ESearchCase::IgnoreCase))
        {
            Record.DiplomacyDelta = 3 + ForeignTrustBonus;
            Record.StabilityDelta = 1;
            Record.UnrestDelta = -1;
            Record.TreasuryDelta = -25;
            Record.ForeignTrustDelta = 3;
            State.InvasionRisk.CurrentInvasionRisk = FMath::Clamp(State.InvasionRisk.CurrentInvasionRisk - 8 - FMath::Max(0, ForeignTrustBonus), 0, State.InvasionRisk.InvasionRiskTrigger);
            Record.OutcomeSummary = TEXT("Border envoys lowered immediate foreign pressure at a modest treasury cost.");
        }
        else
        {
            Record.DiplomacyDelta = 5 + ForeignTrustBonus;
            Record.StabilityDelta = 1;
            Record.TreasuryDelta = -40;
            Record.ForeignTrustDelta = 4;
            State.InvasionRisk.CurrentInvasionRisk = FMath::Clamp(State.InvasionRisk.CurrentInvasionRisk - 4, 0, State.InvasionRisk.InvasionRiskTrigger);
            Record.OutcomeSummary = TEXT("Alliance outreach improved diplomatic standing and long-term foreign trust.");
        }
    }
    else
    {
        Record.AdvisorCoordinationDelta = bBriefing ? 1 : (bFocused ? 2 : 3);
        Record.TreasuryDelta = bEmergency ? -70 : (bFocused ? -30 : 0);
        Record.OutcomeSummary = FString::Printf(TEXT("%s completed a %s agenda."), *ParticipantName, *AgendaItem);

        if (ParticipantName.Equals(TEXT("Resource Manager"), ESearchCase::IgnoreCase))
        {
            if (AgendaItem.Equals(TEXT("Resource Stockpile Audit"), ESearchCase::IgnoreCase))
            {
                Country.Resources.Food = FMath::Max(0, Country.Resources.Food + 5 + CoordinationBonus);
                Country.Resources.Water = FMath::Max(0, Country.Resources.Water + 4 + CoordinationBonus);
                Record.UnrestDelta = -1;
                Record.OutcomeSummary = TEXT("Resource stockpiles were audited and immediate reserve gaps were identified.");
            }
            else if (AgendaItem.Equals(TEXT("Resource Import Relief Package"), ESearchCase::IgnoreCase))
            {
                Country.Resources.Food = FMath::Max(0, Country.Resources.Food + 22 + CoordinationBonus);
                Country.Resources.Water = FMath::Max(0, Country.Resources.Water + 16 + CoordinationBonus);
                Country.Resources.GasOil = FMath::Max(0, Country.Resources.GasOil + 6);
                Record.UnrestDelta = -3;
                Record.StabilityDelta = 2;
                Record.TreasuryDelta = -55;
                Record.OutcomeSummary = TEXT("Emergency imports increased core reserves and reduced shortage unrest.");
            }
            else if (AgendaItem.Equals(TEXT("Resource Production Surge"), ESearchCase::IgnoreCase))
            {
                Country.Resources.Food = FMath::Max(0, Country.Resources.Food + 12 + CoordinationBonus);
                Country.Resources.Water = FMath::Max(0, Country.Resources.Water + 8 + CoordinationBonus);
                Country.Resources.GasOil = FMath::Max(0, Country.Resources.GasOil + 12);
                Country.Resources.Wood = FMath::Max(0, Country.Resources.Wood + 10);
                Country.Resources.Metals = FMath::Max(0, Country.Resources.Metals + 10);
                Country.EnvironmentalHealth = FMath::Clamp(Country.EnvironmentalHealth - 2, 0, 100);
                Record.EconomyDelta = 1;
                Record.TreasuryDelta = -35;
                Record.OutcomeSummary = TEXT("Production was surged across core resources at an environmental and budget cost.");
            }
            else
            {
                Country.Resources.Food = FMath::Max(0, Country.Resources.Food + (bEmergency ? 18 : (bFocused ? 10 : 3)) + CoordinationBonus);
                Country.Resources.Water = FMath::Max(0, Country.Resources.Water + (bEmergency ? 14 : (bFocused ? 8 : 2)) + CoordinationBonus);
                Record.UnrestDelta = bEmergency ? -2 : (bFocused ? -1 : 0);
                Record.StabilityDelta = bEmergency ? 1 : 0;
            }
        }
        else if (ParticipantName.Equals(TEXT("Military Advisor"), ESearchCase::IgnoreCase))
        {
            if (AgendaItem.Equals(TEXT("Military Readiness Review"), ESearchCase::IgnoreCase))
            {
                Record.MilitaryDelta = 2;
                State.InvasionRisk.CurrentInvasionRisk = FMath::Clamp(State.InvasionRisk.CurrentInvasionRisk - 2, 0, State.InvasionRisk.InvasionRiskTrigger);
                Record.OutcomeSummary = TEXT("Military readiness was reviewed and weak defensive points were flagged.");
            }
            else if (AgendaItem.Equals(TEXT("Military Border Defense Plan"), ESearchCase::IgnoreCase))
            {
                Record.MilitaryDelta = 5;
                Record.TreasuryDelta = -45;
                Record.StabilityDelta = 1;
                State.InvasionRisk.CurrentInvasionRisk = FMath::Clamp(State.InvasionRisk.CurrentInvasionRisk - 7, 0, State.InvasionRisk.InvasionRiskTrigger);
                Record.OutcomeSummary = TEXT("Border defense planning improved deterrence and reduced foreign takeover pressure.");
            }
            else if (AgendaItem.Equals(TEXT("Military Emergency Mobilization"), ESearchCase::IgnoreCase))
            {
                Record.MilitaryDelta = 9;
                Record.TreasuryDelta = -90;
                Record.UnrestDelta = 2;
                Record.StabilityDelta = 1;
                State.InvasionRisk.CurrentInvasionRisk = FMath::Clamp(State.InvasionRisk.CurrentInvasionRisk - 12, 0, State.InvasionRisk.InvasionRiskTrigger);
                Record.OutcomeSummary = TEXT("Emergency mobilization sharply improved readiness but increased domestic strain.");
            }
            else
            {
                Record.MilitaryDelta = bEmergency ? 6 : (bFocused ? 4 : 1);
                Record.StabilityDelta = bEmergency ? 1 : 0;
                State.InvasionRisk.CurrentInvasionRisk = FMath::Clamp(State.InvasionRisk.CurrentInvasionRisk - (bEmergency ? 5 : (bFocused ? 3 : 1)), 0, State.InvasionRisk.InvasionRiskTrigger);
            }
        }
        else if (ParticipantName.Equals(TEXT("Social Advisor"), ESearchCase::IgnoreCase))
        {
            if (AgendaItem.Equals(TEXT("Social Public Sentiment Briefing"), ESearchCase::IgnoreCase))
            {
                Record.StabilityDelta = 1;
                Record.UnrestDelta = -1;
                Record.OutcomeSummary = TEXT("Public sentiment was reviewed and unrest sources were prioritized.");
            }
            else if (AgendaItem.Equals(TEXT("Social Community Stabilization"), ESearchCase::IgnoreCase))
            {
                Record.ApprovalDelta = 2;
                Record.StabilityDelta = 3;
                Record.UnrestDelta = -3;
                Record.TreasuryDelta = -45;
                Record.OutcomeSummary = TEXT("Community stabilization improved public trust and reduced unrest pressure.");
            }
            else if (AgendaItem.Equals(TEXT("Social Emergency Calm Initiative"), ESearchCase::IgnoreCase))
            {
                Record.ApprovalDelta = 2;
                Record.StabilityDelta = 5;
                Record.UnrestDelta = -6;
                Record.TreasuryDelta = -75;
                State.FailureRisk.CurrentAssassinationRisk = FMath::Clamp(State.FailureRisk.CurrentAssassinationRisk - 4, 0, State.FailureRisk.AssassinationRiskTrigger);
                Record.OutcomeSummary = TEXT("Emergency calm operations reduced unrest and internal failure pressure.");
            }
            else
            {
                Record.ApprovalDelta = bEmergency ? 2 : (bFocused ? 1 : 0);
                Record.StabilityDelta = bEmergency ? 3 : (bFocused ? 2 : 1);
                Record.UnrestDelta = bEmergency ? -4 : (bFocused ? -2 : -1);
            }
        }
        else if (ParticipantName.Equals(TEXT("Economic Advisor"), ESearchCase::IgnoreCase))
        {
            if (AgendaItem.Equals(TEXT("Economic Budget Review"), ESearchCase::IgnoreCase))
            {
                Record.TreasuryDelta = 25;
                Record.EconomyDelta = 1;
                Record.OutcomeSummary = TEXT("Budget review recovered waste and clarified fiscal risk.");
            }
            else if (AgendaItem.Equals(TEXT("Economic Revenue Plan"), ESearchCase::IgnoreCase))
            {
                Record.TreasuryDelta = 70;
                Record.EconomyDelta = 2;
                Record.ApprovalDelta = -1;
                Record.OutcomeSummary = TEXT("Revenue planning improved the treasury while creating mild public tax pressure.");
            }
            else if (AgendaItem.Equals(TEXT("Economic Stabilization Package"), ESearchCase::IgnoreCase))
            {
                Record.TreasuryDelta = -55;
                Record.EconomyDelta = 4;
                Record.ApprovalDelta = 2;
                State.EconomyBudget.PublicServices = FMath::Clamp(State.EconomyBudget.PublicServices + 2, 0, 100);
                State.EconomyBudget.Inflation = FMath::Clamp(State.EconomyBudget.Inflation - 1, 0, 100);
                Record.OutcomeSummary = TEXT("A stabilization package improved economic health and public services at a treasury cost.");
            }
            else
            {
                Record.TreasuryDelta += bEmergency ? 20 : (bFocused ? 45 : 15);
                Record.EconomyDelta = bEmergency ? 1 : (bFocused ? 3 : 1);
                State.EconomyBudget.Inflation = FMath::Clamp(State.EconomyBudget.Inflation - (bFocused ? 1 : 0), 0, 100);
            }
        }
        else if (ParticipantName.Equals(TEXT("Diplomacy Advisor"), ESearchCase::IgnoreCase))
        {
            if (AgendaItem.Equals(TEXT("Diplomacy Foreign Pressure Briefing"), ESearchCase::IgnoreCase))
            {
                Record.DiplomacyDelta = 1;
                Record.ForeignTrustDelta = 1;
                State.InvasionRisk.CurrentInvasionRisk = FMath::Clamp(State.InvasionRisk.CurrentInvasionRisk - 1, 0, State.InvasionRisk.InvasionRiskTrigger);
                Record.OutcomeSummary = TEXT("Foreign pressure briefing clarified border and alliance risks.");
            }
            else if (AgendaItem.Equals(TEXT("Diplomacy Treaty Outreach"), ESearchCase::IgnoreCase))
            {
                Record.DiplomacyDelta = 4;
                Record.ForeignTrustDelta = 3;
                Record.TreasuryDelta = -35;
                State.InvasionRisk.CurrentInvasionRisk = FMath::Clamp(State.InvasionRisk.CurrentInvasionRisk - 4, 0, State.InvasionRisk.InvasionRiskTrigger);
                Record.OutcomeSummary = TEXT("Treaty outreach improved diplomatic standing and lowered foreign pressure.");
            }
            else if (AgendaItem.Equals(TEXT("Diplomacy Crisis De-escalation"), ESearchCase::IgnoreCase))
            {
                Record.DiplomacyDelta = 5;
                Record.ForeignTrustDelta = 3;
                Record.TreasuryDelta = -55;
                Record.StabilityDelta = 1;
                State.InvasionRisk.CurrentInvasionRisk = FMath::Clamp(State.InvasionRisk.CurrentInvasionRisk - 12, 0, State.InvasionRisk.InvasionRiskTrigger);
                Record.OutcomeSummary = TEXT("Crisis de-escalation reduced immediate foreign takeover risk.");
            }
            else
            {
                Record.DiplomacyDelta = bEmergency ? 4 : (bFocused ? 3 : 1);
                Record.ForeignTrustDelta = bEmergency ? 1 : (bFocused ? 2 : 1);
                State.InvasionRisk.CurrentInvasionRisk = FMath::Clamp(State.InvasionRisk.CurrentInvasionRisk - (bEmergency ? 3 : 1), 0, State.InvasionRisk.InvasionRiskTrigger);
            }
        }
        else if (ParticipantName.Equals(TEXT("Infrastructure Advisor"), ESearchCase::IgnoreCase))
        {
            if (AgendaItem.Equals(TEXT("Infrastructure Asset Condition Review"), ESearchCase::IgnoreCase))
            {
                Record.InfrastructureDelta = 2;
                Record.OutcomeSummary = TEXT("Infrastructure assets were reviewed and repair bottlenecks were prioritized.");
            }
            else if (AgendaItem.Equals(TEXT("Infrastructure Repair Priority Plan"), ESearchCase::IgnoreCase))
            {
                Record.InfrastructureDelta = 5;
                Record.EconomyDelta = 1;
                Record.TreasuryDelta = -45;
                Country.Resources.Wood = FMath::Max(0, Country.Resources.Wood - 5);
                Country.Resources.Metals = FMath::Max(0, Country.Resources.Metals - 5);
                Record.OutcomeSummary = TEXT("Priority repairs improved infrastructure and production resilience.");
            }
            else if (AgendaItem.Equals(TEXT("Infrastructure National Works Surge"), ESearchCase::IgnoreCase))
            {
                Record.InfrastructureDelta = 9;
                Record.StabilityDelta = 2;
                Record.TreasuryDelta = -85;
                Country.Resources.Wood = FMath::Max(0, Country.Resources.Wood - 10);
                Country.Resources.Metals = FMath::Max(0, Country.Resources.Metals - 10);
                Record.OutcomeSummary = TEXT("National works improved long-term resilience at a major material and treasury cost.");
            }
            else
            {
                Record.InfrastructureDelta = bEmergency ? 5 : (bFocused ? 3 : 1);
                Country.Resources.Wood = FMath::Max(0, Country.Resources.Wood - (bEmergency ? 4 : 1));
                Country.Resources.Metals = FMath::Max(0, Country.Resources.Metals - (bEmergency ? 4 : 1));
            }
        }
        else if (ParticipantName.Equals(TEXT("Security Advisor"), ESearchCase::IgnoreCase))
        {
            if (AgendaItem.Equals(TEXT("Security Threat Assessment"), ESearchCase::IgnoreCase))
            {
                Record.StabilityDelta = 1;
                State.FailureRisk.CurrentAssassinationRisk = FMath::Clamp(State.FailureRisk.CurrentAssassinationRisk - 3, 0, State.FailureRisk.AssassinationRiskTrigger);
                Record.OutcomeSummary = TEXT("Threat assessment lowered uncertainty around internal security risk.");
            }
            else if (AgendaItem.Equals(TEXT("Security Protective Detail Upgrade"), ESearchCase::IgnoreCase))
            {
                Record.StabilityDelta = 2;
                Record.TreasuryDelta = -45;
                State.FailureRisk.CurrentAssassinationRisk = FMath::Clamp(State.FailureRisk.CurrentAssassinationRisk - 8, 0, State.FailureRisk.AssassinationRiskTrigger);
                Record.OutcomeSummary = TEXT("Protective details reduced assassination risk and improved crisis readiness.");
            }
            else if (AgendaItem.Equals(TEXT("Security Counter Threat Operation"), ESearchCase::IgnoreCase))
            {
                Record.ApprovalDelta = -1;
                Record.StabilityDelta = 3;
                Record.UnrestDelta = -3;
                Record.TreasuryDelta = -75;
                State.FailureRisk.CurrentAssassinationRisk = FMath::Clamp(State.FailureRisk.CurrentAssassinationRisk - 14, 0, State.FailureRisk.AssassinationRiskTrigger);
                Record.OutcomeSummary = TEXT("Counter-threat operations sharply reduced assassination risk but carried public trust costs.");
            }
            else
            {
                Record.StabilityDelta = bEmergency ? 2 : (bFocused ? 1 : 0);
                Record.UnrestDelta = bEmergency ? -2 : (bFocused ? -1 : 0);
                State.FailureRisk.CurrentAssassinationRisk = FMath::Clamp(State.FailureRisk.CurrentAssassinationRisk - (bEmergency ? 8 : (bFocused ? 5 : 2)), 0, State.FailureRisk.AssassinationRiskTrigger);
            }
        }
        else if (ParticipantName.Equals(TEXT("Public Welfare Advisor"), ESearchCase::IgnoreCase))
        {
            if (AgendaItem.Equals(TEXT("Welfare Needs Assessment"), ESearchCase::IgnoreCase))
            {
                Record.ApprovalDelta = 1;
                State.EconomyBudget.PublicServices = FMath::Clamp(State.EconomyBudget.PublicServices + 1, 0, 100);
                Record.OutcomeSummary = TEXT("Welfare needs were assessed and service gaps were identified.");
            }
            else if (AgendaItem.Equals(TEXT("Welfare Targeted Relief Program"), ESearchCase::IgnoreCase))
            {
                Record.ApprovalDelta = 3;
                Record.UnrestDelta = -3;
                Record.TreasuryDelta = -50;
                State.EconomyBudget.PublicServices = FMath::Clamp(State.EconomyBudget.PublicServices + 3, 0, 100);
                Record.OutcomeSummary = TEXT("Targeted relief improved public trust and reduced unrest sources.");
            }
            else if (AgendaItem.Equals(TEXT("Welfare Public Services Expansion"), ESearchCase::IgnoreCase))
            {
                Record.ApprovalDelta = 5;
                Record.StabilityDelta = 2;
                Record.UnrestDelta = -2;
                Record.TreasuryDelta = -85;
                State.EconomyBudget.PublicServices = FMath::Clamp(State.EconomyBudget.PublicServices + 6, 0, 100);
                Record.OutcomeSummary = TEXT("Public services expansion improved welfare and legitimacy at a major budget cost.");
            }
            else
            {
                Record.ApprovalDelta = bEmergency ? 2 : (bFocused ? 1 : 0);
                Record.UnrestDelta = bEmergency ? -2 : (bFocused ? -1 : 0);
                State.EconomyBudget.PublicServices = FMath::Clamp(State.EconomyBudget.PublicServices + (bEmergency ? 4 : (bFocused ? 2 : 1)), 0, 100);
            }
        }
    }
    Country.PublicApproval = FMath::Clamp(Country.PublicApproval + Record.ApprovalDelta, 0, 100);
    Country.Stability = FMath::Clamp(Country.Stability + Record.StabilityDelta, 0, 100);
    Country.Unrest = FMath::Clamp(Country.Unrest + Record.UnrestDelta, 0, 100);
    Country.DiplomaticStanding = FMath::Clamp(Country.DiplomaticStanding + Record.DiplomacyDelta, 0, 100);
    Country.Treasury = FMath::Max(0, Country.Treasury + Record.TreasuryDelta);
    Country.EconomicHealth = FMath::Clamp(Country.EconomicHealth + Record.EconomyDelta, 0, 100);
    Country.MilitaryReadiness = FMath::Clamp(Country.MilitaryReadiness + Record.MilitaryDelta, 0, 100);
    Country.Infrastructure = FMath::Clamp(Country.Infrastructure + Record.InfrastructureDelta, 0, 100);
    MeetingSystem.AdvisorCoordination = FMath::Clamp(MeetingSystem.AdvisorCoordination + Record.AdvisorCoordinationDelta, 0, 100);
    MeetingSystem.ForeignTrust = FMath::Clamp(MeetingSystem.ForeignTrust + Record.ForeignTrustDelta, 0, 100);
    ++MeetingSystem.TotalMeetings;
    MeetingSystem.LastUpdatedTurn = State.Turn;

    MeetingSystem.LastMeetingSummary = FString::Printf(TEXT("%s with %s: %s"), *MeetingType, *ParticipantName, *Record.OutcomeSummary);
    MeetingSystem.Records.Add(Record);
    while (MeetingSystem.Records.Num() > MeetingSystem.MaxRecords)
    {
        MeetingSystem.Records.RemoveAt(0);
    }

    RefreshResourceChainsFromCurrentCadence(State);
    RecalculateEconomyBudget(State);
    RecalculateDemographics(State);
    RecalculateApprovalStability(State);
    RecalculateDepartments(State);
    ApplyAdvisorMeetingDepartmentConsequences(State, Record);
    State.AdvisorSystem.GuidanceLevel = AdvisorGuidanceForDifficultyScore(Country.CountrySizeScore);
    State.AdvisorSystem.LastUpdatedTurn = State.Turn;
    State.AdvisorSystem.Reports = GenerateAdvisorReports(State);

    const FString MeetingConsequenceText = BuildMeetingDecisionConsequenceText(Record, State);
    MeetingSystem.LastMeetingSummary = FString::Printf(TEXT("%s with %s: %s"), *MeetingType, *ParticipantName, *MeetingConsequenceText);
    if (MeetingSystem.Records.Num() > 0)
    {
        MeetingSystem.Records[MeetingSystem.Records.Num() - 1].OutcomeSummary = MeetingConsequenceText;
    }
    LogDecision(State, TEXT("Meeting"), FString::Printf(TEXT("%s - %s"), *ParticipantName, *AgendaItem), Record.OutcomeSummary, MeetingConsequenceText, MeetingType.Equals(TEXT("Foreign Official"), ESearchCase::IgnoreCase) ? 32 : 22, { TEXT("meeting"), MeetingType, ParticipantName, AgendaItem });

    State.Phase = TEXT("Meeting Held");
    SimulationTickSummary = FString::Printf(TEXT("Meeting result:\n%s"), *MeetingConsequenceText);
    LoadedSaveSummary = LoadedSaveState.ToSummaryText();
    LastSaveStatus = TEXT("Meeting held. Consequences applied to runtime state; save current state to persist meeting history and outcomes.");
    RefreshLoginWidget();
    return FReply::Handled();
}
FReply ALoginHUD::HandleMakePressRelease(FString AnnouncementType)
{
    if (!bHasLoadedRuntimeState)
    {
        return FReply::Handled();
    }

    FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    FDemocracyCountryState& Country = State.PlayerCountry;
    FDemocracyPressOfficeState& PressOffice = State.PressOffice;
    InitializePressOfficeIfMissing(State);

    int32 ApprovalDelta = 0;
    int32 StabilityDelta = 0;
    int32 DiplomacyDelta = 0;
    int32 UnrestDelta = 0;
    int32 CredibilityDelta = 0;
    bool bTruthful = true;
    FString MessageQuality = TEXT("Substantive");
    FString Summary;

    const int32 CredibilityBonus = FMath::Clamp((PressOffice.Credibility - 50) / 15, -2, 2);
    if (AnnouncementType.Equals(TEXT("Crisis Reassurance"), ESearchCase::IgnoreCase))
    {
        ApprovalDelta = 1 + CredibilityBonus;
        StabilityDelta = 2 + CredibilityBonus;
        UnrestDelta = -2 - FMath::Max(0, CredibilityBonus);
        CredibilityDelta = 1;
        Summary = TEXT("The administration gave a focused reassurance speech tied to current risks.");
    }
    else if (AnnouncementType.Equals(TEXT("Policy Explanation"), ESearchCase::IgnoreCase))
    {
        ApprovalDelta = 2 + CredibilityBonus;
        StabilityDelta = 1;
        UnrestDelta = -1;
        CredibilityDelta = 2;
        Summary = TEXT("The administration explained current policies and tradeoffs to the public.");
    }
    else if (AnnouncementType.Equals(TEXT("Diplomatic Address"), ESearchCase::IgnoreCase))
    {
        ApprovalDelta = CredibilityBonus;
        StabilityDelta = 1;
        DiplomacyDelta = 3 + CredibilityBonus;
        UnrestDelta = -1;
        CredibilityDelta = 1;
        Summary = TEXT("The administration made a world-facing diplomatic address.");
    }
    else if (AnnouncementType.Equals(TEXT("Victory Claim"), ESearchCase::IgnoreCase))
    {
        const bool bSupportedBySignals = Country.PublicApproval >= 58 || Country.Stability >= 62 || Country.MilitaryReadiness >= 62 || Country.DiplomaticStanding >= 62;
        bTruthful = bSupportedBySignals;
        if (bSupportedBySignals)
        {
            ApprovalDelta = 3 + CredibilityBonus;
            StabilityDelta = 1;
            DiplomacyDelta = 1;
            UnrestDelta = -1;
            CredibilityDelta = 1;
            Summary = TEXT("The administration highlighted a credible recent success.");
        }
        else
        {
            ApprovalDelta = -2;
            StabilityDelta = -1;
            DiplomacyDelta = -2;
            UnrestDelta = 2;
            CredibilityDelta = -8 - PressOffice.ConsecutiveFalseAnnouncements * 4;
            MessageQuality = TEXT("Unsupported");
            Summary = TEXT("The victory claim was not supported by current national signals and damaged trust.");
        }
    }
    else if (AnnouncementType.Equals(TEXT("False Claim"), ESearchCase::IgnoreCase))
    {
        bTruthful = false;
        MessageQuality = TEXT("False");
        ApprovalDelta = -3;
        StabilityDelta = -2;
        DiplomacyDelta = -3;
        UnrestDelta = 3 + PressOffice.ConsecutiveFalseAnnouncements;
        CredibilityDelta = -12 - PressOffice.ConsecutiveFalseAnnouncements * 5;
        Summary = TEXT("A false announcement was challenged by observers and reduced institutional credibility.");
    }
    else
    {
        MessageQuality = TEXT("Empty");
        ApprovalDelta = -1 - PressOffice.ConsecutiveEmptyAnnouncements;
        StabilityDelta = -1;
        UnrestDelta = 1 + PressOffice.ConsecutiveEmptyAnnouncements;
        CredibilityDelta = -6 - PressOffice.ConsecutiveEmptyAnnouncements * 3;
        Summary = TEXT("The announcement contained no useful information and made the press office look evasive.");
    }

    if (MessageQuality.Equals(TEXT("Empty"), ESearchCase::IgnoreCase))
    {
        ++PressOffice.ConsecutiveEmptyAnnouncements;
    }
    else
    {
        PressOffice.ConsecutiveEmptyAnnouncements = 0;
    }

    if (!bTruthful)
    {
        ++PressOffice.ConsecutiveFalseAnnouncements;
    }
    else
    {
        PressOffice.ConsecutiveFalseAnnouncements = 0;
    }

    Country.PublicApproval = FMath::Clamp(Country.PublicApproval + ApprovalDelta, 0, 100);
    Country.Stability = FMath::Clamp(Country.Stability + StabilityDelta, 0, 100);
    Country.DiplomaticStanding = FMath::Clamp(Country.DiplomaticStanding + DiplomacyDelta, 0, 100);
    Country.Unrest = FMath::Clamp(Country.Unrest + UnrestDelta, 0, 100);
    PressOffice.Credibility = FMath::Clamp(PressOffice.Credibility + CredibilityDelta, 0, 100);
    ++PressOffice.TotalAnnouncements;
    PressOffice.LastUpdatedTurn = State.Turn;

    FDemocracyPressReleaseRecordState Record;
    Record.Turn = State.Turn;
    Record.AnnouncementType = AnnouncementType;
    Record.MessageQuality = MessageQuality;
    Record.bTruthful = bTruthful;
    Record.ApprovalDelta = ApprovalDelta;
    Record.StabilityDelta = StabilityDelta;
    Record.DiplomacyDelta = DiplomacyDelta;
    Record.UnrestDelta = UnrestDelta;
    Record.CredibilityDelta = CredibilityDelta;
    Record.CredibilityAfter = PressOffice.Credibility;
    Record.Summary = Summary;
    PressOffice.Records.Add(Record);
    while (PressOffice.Records.Num() > PressOffice.MaxRecords)
    {
        PressOffice.Records.RemoveAt(0);
    }

    PressOffice.LastAnnouncementSummary = FString::Printf(TEXT("%s | approval %+d, stability %+d, diplomacy %+d, unrest %+d, credibility %+d. Credibility now %d."),
        *Summary,
        ApprovalDelta,
        StabilityDelta,
        DiplomacyDelta,
        UnrestDelta,
        CredibilityDelta,
        PressOffice.Credibility);

    RecalculateDemographics(State);
    RecalculateApprovalStability(State);
    State.AdvisorSystem.GuidanceLevel = AdvisorGuidanceForDifficultyScore(Country.CountrySizeScore);
    State.AdvisorSystem.LastUpdatedTurn = State.Turn;
    State.AdvisorSystem.Reports = GenerateAdvisorReports(State);
    LogDecision(State, TEXT("Press Release"), AnnouncementType, Summary, PressOffice.LastAnnouncementSummary, bTruthful ? 18 : 55, { TEXT("press"), AnnouncementType, MessageQuality });

    State.Phase = TEXT("Press Release Issued");
    LoadedSaveSummary = LoadedSaveState.ToSummaryText();
    LastSaveStatus = TEXT("Press release issued. Save current state to persist credibility and announcement history.");
    RefreshLoginWidget();
    return FReply::Handled();
}
FReply ALoginHUD::HandleReloadPreviousSaveClicked()
{
    const FString PrimarySavePath = LoadedSavePath;
    if (PrimarySavePath.IsEmpty())
    {
        LastSaveStatus = TEXT("No local save path is available for protected reload.");
        RefreshLoginWidget();
        return FReply::Handled();
    }

    FString ReloadPath;
    FString ReloadError;
    if (!FDemocracySaveGameRuntime::GetProtectedReloadSavePath(PrimarySavePath, ReloadPath, ReloadError))
    {
        LastSaveStatus = ReloadError;
        RefreshLoginWidget();
        return FReply::Handled();
    }

    if (LoadSinglePlayerSaveIntoRuntime(ReloadPath))
    {
        LoadedSavePath = PrimarySavePath;
        LoadedSaveState.SavePath = PrimarySavePath;
        GameOverReason.Empty();
        GameOverDetails.Empty();
        LastSaveStatus = FString::Printf(TEXT("Reloaded protected previous save: %s"), *ReloadPath);
        EnterOfficePrototype(false);
    }

    return FReply::Handled();
}

FReply ALoginHUD::HandleBackToDifficultyClicked()
{
    ShowScreen(ELoginFlowScreen::DifficultySelection);
    return FReply::Handled();
}

FReply ALoginHUD::HandleBackToLocalSavesClicked()
{
    ShowScreen(ELoginFlowScreen::LocalSaveSelection);
    return FReply::Handled();
}
FReply ALoginHUD::HandleBackFromLocalSavesClicked()
{
    ShowScreen(bInOfficeMode ? ELoginFlowScreen::OfficeComputerMenu : ELoginFlowScreen::Login);
    return FReply::Handled();
}

FReply ALoginHUD::HandleBackToModeSelectionClicked()
{
    ShowScreen(ELoginFlowScreen::GameModeSelection);
    return FReply::Handled();
}

FReply ALoginHUD::HandleBackToOnlineStatesClicked()
{
    ShowScreen(ELoginFlowScreen::MultiplayerStateSelection);
    return FReply::Handled();
}

FReply ALoginHUD::HandleSelectLocalSave(FString SaveName)
{
    const FString SavePath = FPaths::ProjectDir() / TEXT("Saves") / SaveName;
    if (LoadSinglePlayerSaveIntoRuntime(SavePath))
    {
        EnterOfficePrototype(false);
    }
    else
    {
        RefreshLoginWidget();
    }

    return FReply::Handled();
}

FReply ALoginHUD::HandleDeleteLocalSave(FString SaveName)
{
    const FString SavePath = FPaths::ProjectDir() / TEXT("Saves") / SaveName;
    const bool bDeleted = IFileManager::Get().Delete(*SavePath, false, true, true);

    if (bDeleted)
    {
        LastSaveStatus = FString::Printf(TEXT("Deleted local save %s."), *SaveName);
        if (LoadedSavePath.Equals(SavePath, ESearchCase::IgnoreCase))
        {
            LoadedSavePath.Empty();
            LoadedStateName.Empty();
            LoadedSaveSummary.Empty();
            LoadedSaveError.Empty();
            bHasLoadedRuntimeState = false;
        }
    }
    else
    {
        LastSaveStatus = FString::Printf(TEXT("Could not delete local save %s. Check that the file is not open or read-only."), *SaveName);
    }

    RefreshLoginWidget();
    return FReply::Handled();
}

FReply ALoginHUD::HandleSelectOnlineState(FString StateName)
{
    SelectedOnlineState = StateName;
    ShowScreen(ELoginFlowScreen::ServerSelection);
    return FReply::Handled();
}

FReply ALoginHUD::HandleRefreshOnlineStatesClicked()
{
    UE_LOG(LogTemp, Log, TEXT("Login flow: refresh online states placeholder. Server API will populate this screen later."));
    return FReply::Handled();
}

FReply ALoginHUD::HandleSelectServer(FString ServerName)
{
    UE_LOG(LogTemp, Log, TEXT("Login flow: selected server '%s' for online state '%s'. Connection placeholder."), *ServerName, *SelectedOnlineState);
    return FReply::Handled();
}

FReply ALoginHUD::HandleChangeKeybind(FString ActionName)
{
    UE_LOG(LogTemp, Log, TEXT("Settings: keybind capture placeholder for '%s'."), *ActionName);
    return FReply::Handled();
}

FReply ALoginHUD::HandleBeginOfficeFromBriefingClicked()
{
    CurrentScreen = ELoginFlowScreen::OfficeNoOverlay;
    TearDownLoginWidget();

    if (APlayerController* PlayerController = GetOwningPlayerController())
    {
        PlayerController->bShowMouseCursor = false;
        FInputModeGameOnly InputMode;
        PlayerController->SetInputMode(InputMode);
    }

    return FReply::Handled();
}

FReply ALoginHUD::HandleCloseOfficeOverlayClicked()
{
    CurrentScreen = ELoginFlowScreen::OfficeNoOverlay;
    TearDownLoginWidget();

    if (APlayerController* PlayerController = GetOwningPlayerController())
    {
        PlayerController->bShowMouseCursor = false;
        FInputModeGameOnly InputMode;
        PlayerController->SetInputMode(InputMode);
    }

    return FReply::Handled();
}

void ALoginHUD::CloseOfficeOverlayFromInput()
{
    if (!bInOfficeMode || CurrentScreen == ELoginFlowScreen::OfficeNoOverlay)
    {
        return;
    }

    HandleCloseOfficeOverlayClicked();
}

FReply ALoginHUD::HandleExitOfficeClicked()
{
    bInOfficeMode = false;
    ShowScreen(ELoginFlowScreen::LoadedGame);

    if (APlayerController* PlayerController = GetOwningPlayerController())
    {
        PlayerController->bShowMouseCursor = true;
        FInputModeUIOnly InputMode;
        PlayerController->SetInputMode(InputMode);
    }

    return FReply::Handled();
}

FReply ALoginHUD::HandleEnterOfficeClicked()
{
    EnterOfficePrototype(false);
    return FReply::Handled();
}

FString ALoginHUD::GetRememberLoginPath() const
{
    return FPaths::ProjectSavedDir() / TEXT("DemocracyLoginDetails.ini");
}

void ALoginHUD::LoadRememberedLoginDetails()
{
    FString Contents;
    if (!FFileHelper::LoadFileToString(Contents, *GetRememberLoginPath()))
    {
        bRememberLoginDetails = false;
        return;
    }

    TArray<FString> Lines;
    Contents.ParseIntoArrayLines(Lines, false);
    for (const FString& Line : Lines)
    {
        FString Key;
        FString Value;
        if (!Line.Split(TEXT("="), &Key, &Value))
        {
            continue;
        }

        if (Key.Equals(TEXT("Remember"), ESearchCase::IgnoreCase))
        {
            bRememberLoginDetails = Value.Equals(TEXT("true"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("1"));
        }
        else if (Key.Equals(TEXT("UserName"), ESearchCase::IgnoreCase))
        {
            MockUserName = Value;
        }
        else if (Key.Equals(TEXT("Password"), ESearchCase::IgnoreCase))
        {
            MockPassword = Value;
        }
    }

    if (!bRememberLoginDetails)
    {
        MockUserName.Empty();
        MockPassword.Empty();
    }
}

void ALoginHUD::SaveRememberedLoginDetails() const
{
    if (!bRememberLoginDetails)
    {
        return;
    }

    IFileManager::Get().MakeDirectory(*FPaths::ProjectSavedDir(), true);
    const FString Contents = FString::Printf(TEXT("Remember=true\nUserName=%s\nPassword=%s\n"), *MockUserName, *MockPassword);
    FFileHelper::SaveStringToFile(Contents, *GetRememberLoginPath());
}

void ALoginHUD::ClearRememberedLoginDetails() const
{
    IFileManager::Get().Delete(*GetRememberLoginPath(), false, true);
}
FReply ALoginHUD::HandleExitClicked()
{
    UKismetSystemLibrary::QuitGame(GetWorld(), GetOwningPlayerController(), EQuitPreference::Quit, false);
    return FReply::Handled();
}

void ALoginHUD::HandleMockUserNameChanged(const FText& UserNameText)
{
    MockUserName = UserNameText.ToString();
}

void ALoginHUD::HandleMockPasswordChanged(const FText& PasswordText)
{
    MockPassword = PasswordText.ToString();
}

void ALoginHUD::HandleRememberLoginDetailsChanged(ECheckBoxState NewState)
{
    bRememberLoginDetails = NewState == ECheckBoxState::Checked;
    if (bRememberLoginDetails)
    {
        SaveRememberedLoginDetails();
    }
    else
    {
        ClearRememberedLoginDetails();
    }
    RefreshLoginWidget();
}

void ALoginHUD::HandleLocalSaveSearchChanged(const FText& SearchText)
{
    LocalSaveSearchText = SearchText.ToString();
    RefreshLoginWidget();
}

void ALoginHUD::HandlePendingStateNameChanged(const FText& StateNameText)
{
    PendingStateName = StateNameText.ToString();
}

void ALoginHUD::HandleStartingCountrySearchChanged(const FText& SearchText)
{
    PendingStartingCountrySearchText = SearchText.ToString();
    PendingStartingCountryListScrollOffset = 0.0f;
    RefreshLoginWidget();
}

void ALoginHUD::HandleStartingCountryListScrolled(float ScrollOffset)
{
    PendingStartingCountryListScrollOffset = ScrollOffset;
}

void ALoginHUD::HandleNewCountrySetupScrolled(float ScrollOffset)
{
    PendingNewCountrySetupScrollOffset = ScrollOffset;
}

void ALoginHUD::HandleRecentLocalSavesChanged(ECheckBoxState NewState)
{
    bShowRecentLocalSavesOnly = NewState == ECheckBoxState::Checked;
    RefreshLoginWidget();
}

void ALoginHUD::HandleServerSearchChanged(const FText& SearchText)
{
    ServerSearchText = SearchText.ToString();
    RefreshLoginWidget();
}

void ALoginHUD::HandleRecommendedServersChanged(ECheckBoxState NewState)
{
    bShowRecommendedServersOnly = NewState == ECheckBoxState::Checked;
    RefreshLoginWidget();
}

void ALoginHUD::HandleFullscreenChanged(ECheckBoxState NewState)
{
    bFullscreen = NewState == ECheckBoxState::Checked;
}

void ALoginHUD::HandleVSyncChanged(ECheckBoxState NewState)
{
    bVSync = NewState == ECheckBoxState::Checked;
}

void ALoginHUD::HandleInvertLookYChanged(ECheckBoxState NewState)
{
    bInvertLookY = NewState == ECheckBoxState::Checked;
    if (OfficePlayerPawn.IsValid())
    {
        OfficePlayerPawn->SetInvertLookY(bInvertLookY);
    }
}

void ALoginHUD::HandleMasterVolumeChanged(float NewValue)
{
    MasterVolume = NewValue;
}

void ALoginHUD::HandleMusicVolumeChanged(float NewValue)
{
    MusicVolume = NewValue;
}

void ALoginHUD::HandleEffectsVolumeChanged(float NewValue)
{
    EffectsVolume = NewValue;
}

void ALoginHUD::HandleVoiceVolumeChanged(float NewValue)
{
    VoiceVolume = NewValue;
}

void ALoginHUD::HandleBrightnessChanged(float NewValue)
{
    Brightness = NewValue;
}

void ALoginHUD::HandleUiScaleChanged(float NewValue)
{
    UiScale = FMath::Clamp(NewValue, 0.50f, 1.50f);
}

