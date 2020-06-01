using UnrealBuildTool;
using System.IO;

public class MetaballsPlugin : ModuleRules
{

    public MetaballsPlugin(ReadOnlyTargetRules Target) : base(Target)
    {
        //PrivatePCHHeaderFile = "Private/MetaballsPluginPrivatePCH.h";

        PublicDependencyModuleNames.AddRange(new string[] { "Engine", "Core", "CoreUObject", "InputCore", "ProceduralMeshComponent" });
    }
}