using UnrealBuildTool;
using System.Collections.Generic;

public class DemocracyEditorTarget : TargetRules
{
    public DemocracyEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
        ExtraModuleNames.Add("Democracy");
    }
}
