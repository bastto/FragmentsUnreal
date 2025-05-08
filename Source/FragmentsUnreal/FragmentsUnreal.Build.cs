// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class FragmentsUnreal : ModuleRules
{
	public FragmentsUnreal(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				Path.Combine(ModuleDirectory, "../../ThirdParty/FlatBuffers/include"),
                Path.Combine(ModuleDirectory, "../../ThirdParty/libtess2/Include")
            }
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
                // ---
            }
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
                "ProceduralMeshComponent",
				"MeshDescription",
				"StaticMeshDescription",
				"GeometryScriptingCore",
				"GeometryCore"
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Projects",
				"InputCore",
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore"
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);

        PublicIncludePaths.Add(Path.Combine(EngineDirectory, "Source/ThirdParty/zlib"));
        AddEngineThirdPartyPrivateStaticDependencies(Target, "zlib");

        string TessIncludePath = Path.Combine(ModuleDirectory, "../../ThirdParty/libtess2/Include");
        string TessLibPath = Path.Combine(ModuleDirectory, "../../ThirdParty/libtess2/Lib/Win64");

        //PublicIncludePaths.Add(TessIncludePath);
        PublicAdditionalLibraries.Add(Path.Combine(TessLibPath, "tess2.lib"));
    }
}
