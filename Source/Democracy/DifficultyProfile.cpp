#include "DifficultyProfile.h"

#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"

namespace
{
    FString JsonEscape(FString Text)
    {
        Text.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
        Text.ReplaceInline(TEXT("\""), TEXT("\\\""));
        Text.ReplaceInline(TEXT("\r"), TEXT("\\r"));
        Text.ReplaceInline(TEXT("\n"), TEXT("\\n"));
        return Text;
    }

    FString StringArrayToJson(const TArray<FString>& Values)
    {
        FString Output = TEXT("[");
        for (int32 Index = 0; Index < Values.Num(); ++Index)
        {
            if (Index > 0)
            {
                Output += TEXT(", ");
            }

            Output += FString::Printf(TEXT("\"%s\""), *JsonEscape(Values[Index]));
        }
        Output += TEXT("]");
        return Output;
    }

    FString DifficultyConfigPath()
    {
        return FPaths::ProjectConfigDir() / TEXT("DifficultyProfiles.ini");
    }

    void ReadString(const FString& Section, const TCHAR* Key, const FString& ConfigPath, FString& Value)
    {
        FString LoadedValue;
        if (GConfig && GConfig->GetString(*Section, Key, LoadedValue, ConfigPath))
        {
            Value = LoadedValue;
        }
    }

    void ReadInt(const FString& Section, const TCHAR* Key, const FString& ConfigPath, int32& Value)
    {
        int32 LoadedValue = 0;
        if (GConfig && GConfig->GetInt(*Section, Key, LoadedValue, ConfigPath))
        {
            Value = LoadedValue;
        }
    }

    void ReadResourceTypes(const FString& Section, const FString& ConfigPath, TArray<FString>& Values)
    {
        FString RawValue;
        if (GConfig && GConfig->GetString(*Section, TEXT("StartingResourceTypes"), RawValue, ConfigPath))
        {
            Values.Reset();
            RawValue.ParseIntoArray(Values, TEXT(","), true);
            for (FString& Value : Values)
            {
                Value.TrimStartAndEndInline();
            }
        }
    }
}

FString FDifficultyProfile::ToSummaryText() const
{
    return FString::Printf(
        TEXT("Country: %s | Help: %s | AI Support: %s | Advisors: %d | Tips: %s | Resources: %s"),
        *CountrySize,
        *PlayerHelpLevel,
        *AiSupportLevel,
        AdvisorCount,
        *TipFrequency,
        *FString::Join(StartingResourceTypes, TEXT(", ")));
}

FString FDifficultyProfile::ToJson(int32 IndentSpaces) const
{
    const FString Indent = FString::ChrN(IndentSpaces, TEXT(' '));
    const FString InnerIndent = FString::ChrN(IndentSpaces + 2, TEXT(' '));

    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"name\": \"%s\",\n")
        TEXT("%s\"countrySize\": \"%s\",\n")
        TEXT("%s\"countrySizeScore\": %d,\n")
        TEXT("%s\"playerHelpLevel\": \"%s\",\n")
        TEXT("%s\"aiSupportLevel\": \"%s\",\n")
        TEXT("%s\"advisorCount\": %d,\n")
        TEXT("%s\"tipFrequency\": \"%s\",\n")
        TEXT("%s\"startingResourceTypes\": %s,\n")
        TEXT("%s\"startingResources\": {\n")
        TEXT("%s\"food\": %d,\n")
        TEXT("%s\"gasOil\": %d,\n")
        TEXT("%s\"wood\": %d,\n")
        TEXT("%s\"metals\": %d,\n")
        TEXT("%s\"water\": %d\n")
        TEXT("%s},\n")
        TEXT("%s\"startingApproval\": %d,\n")
        TEXT("%s\"startingTreasury\": %d,\n")
        TEXT("%s\"startingCrisisPressure\": \"%s\"\n")
        TEXT("%s}"),
        *Indent, *JsonEscape(Name),
        *Indent, *JsonEscape(CountrySize),
        *Indent, CountrySizeScore,
        *Indent, *JsonEscape(PlayerHelpLevel),
        *Indent, *JsonEscape(AiSupportLevel),
        *Indent, AdvisorCount,
        *Indent, *JsonEscape(TipFrequency),
        *Indent, *StringArrayToJson(StartingResourceTypes),
        *Indent,
        *InnerIndent, StartingResources.Food,
        *InnerIndent, StartingResources.GasOil,
        *InnerIndent, StartingResources.Wood,
        *InnerIndent, StartingResources.Metals,
        *InnerIndent, StartingResources.Water,
        *Indent,
        *Indent, StartingApproval,
        *Indent, StartingTreasury,
        *Indent, *JsonEscape(StartingCrisisPressure),
        *Indent);
}

FDifficultyProfile FDifficultyProfileLibrary::GetProfile(const FString& DifficultyName)
{
    FDifficultyProfile Profile = BuildDefaultProfile(DifficultyName);
    ApplyConfigOverrides(Profile);
    return Profile;
}

TArray<FString> FDifficultyProfileLibrary::GetDifficultyNames()
{
    return { TEXT("Easy"), TEXT("Normal"), TEXT("Hard"), TEXT("Expert") };
}

FDifficultyProfile FDifficultyProfileLibrary::BuildDefaultProfile(const FString& DifficultyName)
{
    FDifficultyProfile Profile;
    Profile.Name = DifficultyName;

    if (DifficultyName.Equals(TEXT("Easy"), ESearchCase::IgnoreCase))
    {
        Profile.Name = TEXT("Easy");
        Profile.CountrySize = TEXT("Small");
        Profile.CountrySizeScore = 1;
        Profile.PlayerHelpLevel = TEXT("High");
        Profile.AiSupportLevel = TEXT("High");
        Profile.AdvisorCount = 5;
        Profile.TipFrequency = TEXT("Frequent");
        Profile.StartingResourceTypes = { TEXT("Food"), TEXT("Wood") };
        Profile.StartingResources = { 1200, 0, 900, 0, 700 };
        Profile.StartingApproval = 72;
        Profile.StartingTreasury = 1500;
        Profile.StartingCrisisPressure = TEXT("Low");
    }
    else if (DifficultyName.Equals(TEXT("Hard"), ESearchCase::IgnoreCase))
    {
        Profile.Name = TEXT("Hard");
        Profile.CountrySize = TEXT("Large");
        Profile.CountrySizeScore = 3;
        Profile.PlayerHelpLevel = TEXT("Low");
        Profile.AiSupportLevel = TEXT("Limited");
        Profile.AdvisorCount = 2;
        Profile.TipFrequency = TEXT("Sparse");
        Profile.StartingResourceTypes = { TEXT("Food"), TEXT("GasOil"), TEXT("Wood"), TEXT("Metals") };
        Profile.StartingResources = { 560, 360, 420, 300, 360 };
        Profile.StartingApproval = 48;
        Profile.StartingTreasury = 700;
        Profile.StartingCrisisPressure = TEXT("High");
    }
    else if (DifficultyName.Equals(TEXT("Expert"), ESearchCase::IgnoreCase))
    {
        Profile.Name = TEXT("Expert");
        Profile.CountrySize = TEXT("Massive");
        Profile.CountrySizeScore = 4;
        Profile.PlayerHelpLevel = TEXT("Minimal");
        Profile.AiSupportLevel = TEXT("None");
        Profile.AdvisorCount = 1;
        Profile.TipFrequency = TEXT("None");
        Profile.StartingResourceTypes = { TEXT("Food"), TEXT("GasOil"), TEXT("Wood"), TEXT("Metals"), TEXT("Water") };
        Profile.StartingResources = { 360, 240, 280, 220, 260 };
        Profile.StartingApproval = 38;
        Profile.StartingTreasury = 420;
        Profile.StartingCrisisPressure = TEXT("Severe");
    }
    else
    {
        Profile.Name = TEXT("Normal");
        Profile.CountrySize = TEXT("Medium");
        Profile.CountrySizeScore = 2;
        Profile.PlayerHelpLevel = TEXT("Standard");
        Profile.AiSupportLevel = TEXT("Moderate");
        Profile.AdvisorCount = 4;
        Profile.TipFrequency = TEXT("Standard");
        Profile.StartingResourceTypes = { TEXT("Food"), TEXT("Wood"), TEXT("Metals") };
        Profile.StartingResources = { 850, 0, 650, 450, 520 };
        Profile.StartingApproval = 60;
        Profile.StartingTreasury = 1050;
        Profile.StartingCrisisPressure = TEXT("Moderate");
    }

    return Profile;
}

void FDifficultyProfileLibrary::ApplyConfigOverrides(FDifficultyProfile& Profile)
{
    const FString ConfigPath = DifficultyConfigPath();
    const FString Section = Profile.Name;

    ReadString(Section, TEXT("CountrySize"), ConfigPath, Profile.CountrySize);
    ReadInt(Section, TEXT("CountrySizeScore"), ConfigPath, Profile.CountrySizeScore);
    ReadString(Section, TEXT("PlayerHelpLevel"), ConfigPath, Profile.PlayerHelpLevel);
    ReadString(Section, TEXT("AiSupportLevel"), ConfigPath, Profile.AiSupportLevel);
    ReadInt(Section, TEXT("AdvisorCount"), ConfigPath, Profile.AdvisorCount);
    ReadString(Section, TEXT("TipFrequency"), ConfigPath, Profile.TipFrequency);
    ReadResourceTypes(Section, ConfigPath, Profile.StartingResourceTypes);
    ReadInt(Section, TEXT("ResourceFood"), ConfigPath, Profile.StartingResources.Food);
    ReadInt(Section, TEXT("ResourceGasOil"), ConfigPath, Profile.StartingResources.GasOil);
    ReadInt(Section, TEXT("ResourceWood"), ConfigPath, Profile.StartingResources.Wood);
    ReadInt(Section, TEXT("ResourceMetals"), ConfigPath, Profile.StartingResources.Metals);
    ReadInt(Section, TEXT("ResourceWater"), ConfigPath, Profile.StartingResources.Water);
    ReadInt(Section, TEXT("StartingApproval"), ConfigPath, Profile.StartingApproval);
    ReadInt(Section, TEXT("StartingTreasury"), ConfigPath, Profile.StartingTreasury);
    ReadString(Section, TEXT("StartingCrisisPressure"), ConfigPath, Profile.StartingCrisisPressure);
}
