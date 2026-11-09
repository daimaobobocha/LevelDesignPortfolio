using UnrealBuildTool;
using System.Collections.Generic;

public class RE01Target : TargetRules
{
    public RE01Target(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_6;
        ExtraModuleNames.Add("RE01");
    }
}
