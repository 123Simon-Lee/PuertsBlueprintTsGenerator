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
                "Projects",
                "Slate",
                "SlateCore",
                "UnrealEd"
            });
    }
}
