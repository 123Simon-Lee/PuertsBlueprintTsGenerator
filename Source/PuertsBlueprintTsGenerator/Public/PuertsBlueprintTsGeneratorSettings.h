#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "PuertsBlueprintTsGeneratorSettings.generated.h"

/** Puerts 蓝图 TypeScript 模板生成器设置。 */
UCLASS(
    Config = EditorPerProjectUserSettings,
    DefaultConfig,
    meta = (DisplayName = "Puerts Blueprint TypeScript Generator"))
class PUERTSBLUEPRINTTSGENERATOR_API UPuertsBlueprintTsGeneratorSettings
    : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    /** 相对于工程根目录的 TypeScript 输出目录。 */
    UPROPERTY(
        Config,
        EditAnywhere,
        Category = "Output",
        meta = (RelativeToGameDir))
    FString OutputDirectory = TEXT("TypeScript");

    /**
     * 从蓝图包路径中移除的根路径。
     * 例如 /Game/GZNY/BP_Test 在根路径为 /Game 时输出到
     * TypeScript/GZNY/BP_Test.ts。
     */
    UPROPERTY(Config, EditAnywhere, Category = "Output")
    FString BlueprintRootPath = TEXT("/Game");

    /** 是否把生成的脚本作为副作用导入写入 TypeScript 入口文件。 */
    UPROPERTY(Config, EditAnywhere, Category = "Entry File")
    bool bUpdateMainEntryImports = true;

    /** 相对于 TypeScript 输出目录的入口文件。 */
    UPROPERTY(Config, EditAnywhere, Category = "Entry File")
    FString MainEntryFile = TEXT("MainGame.ts");

    /** 根据蓝图类型生成 ReceiveBeginPlay 或 Construct 空实现。 */
    UPROPERTY(Config, EditAnywhere, Category = "Template")
    bool bGenerateLifecycleStub = true;
};
