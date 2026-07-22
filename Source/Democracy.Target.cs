using UnrealBuildTool;
using System.Collections.Generic;

public class DemocracyTarget : TargetRules
{
    public DemocracyTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
        ExtraModuleNames.Add("Democracy");
    }
}
