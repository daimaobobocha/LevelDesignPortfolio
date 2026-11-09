using UnrealBuildTool;
using System.Collections.Generic;

public class RE01EditorTarget : TargetRules
{
    public RE01EditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_6;
        ExtraModuleNames.Add("RE01");
    }
}
