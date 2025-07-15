using UnrealBuildTool;

public class FragmentsEditor: ModuleRules
{
    public FragmentsEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateDependencyModuleNames.AddRange(new string[] {"Core", "CoreUObject", "Engine",
        "EditorSubsystem",
        "UnrealEd",
        "FragmentsUnreal"});
    }
}
