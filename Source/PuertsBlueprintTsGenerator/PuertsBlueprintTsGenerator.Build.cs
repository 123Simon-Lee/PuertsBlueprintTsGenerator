using UnrealBuildTool;

public class PuertsBlueprintTsGenerator : ModuleRules
{
    public PuertsBlueprintTsGenerator(ReadOnlyTargetRules Target)
        : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new[]
            {
                "Core",
                "CoreUObject",
                "DeveloperSettings",
                "Engine"
            });

        PrivateDependencyModuleNames.AddRange(
            new[]
            {
                "AssetRegistry",
                "Kismet",
                "ContentBrowser",
                "EngineSettings",
                "Projects",
                "Slate",
                "SlateCore",
                "UnrealEd"
            });
    }
}
