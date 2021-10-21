using UnrealBuildTool;

public class MetaballsPlugin : ModuleRules
{

    public MetaballsPlugin(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(new string[] { "Engine", "Core", "CoreUObject", "InputCore", "ProceduralMeshComponent" });
        
        // Change this to 1 have more info in the profiler, there is a slight CPU performance hit when active.
        PublicDefinitions.Add("METABALLS_PROFILE=0");
    }
}