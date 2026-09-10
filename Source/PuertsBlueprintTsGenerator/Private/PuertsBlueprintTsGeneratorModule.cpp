#include "PuertsBlueprintTsGeneratorSettings.h"

#include "AssetRegistry/AssetData.h"
#include "BlueprintEditorModule.h"
#include "Brushes/SlateImageBrush.h"
#include "ContentBrowserModule.h"
#include "Engine/Blueprint.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "UObject/WeakObjectPtr.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "FPuertsBlueprintTsGeneratorModule"

DEFINE_LOG_CATEGORY_STATIC(LogPuertsBlueprintTsGenerator, Log, All);

namespace PuertsBlueprintTsGenerator
{
const FName StyleSetName(TEXT("PuertsBlueprintTsGeneratorStyle"));
const FName GenerateIconName(TEXT("PuertsBlueprintTsGenerator.Generate"));

FString SanitizeTypeScriptIdentifier(const FString& Source)
{
    FString Result;
    Result.Reserve(Source.Len() + 1);

    for (const TCHAR Character : Source)
    {
        Result.AppendChar(
            FChar::IsAlnum(Character) || Character == TEXT('_')
                ? Character
                : TEXT('_'));
    }

    if (Result.IsEmpty())
    {
        return TEXT("_");
    }

    if (FChar::IsDigit(Result[0]))
    {
        Result.InsertAt(0, TEXT('_'));
    }

    return Result;
}

FString BuildTypeScriptNamespace(const FString& PackageName)
{
    TArray<FString> Segments;
    PackageName.ParseIntoArray(Segments, TEXT("/"), true);

    for (FString& Segment : Segments)
    {
        Segment = SanitizeTypeScriptIdentifier(Segment);
    }

    return FString::Join(Segments, TEXT("."));
}

FString NormalizeVirtualRoot(FString RootPath)
{
    RootPath.TrimStartAndEndInline();
    RootPath.ReplaceInline(TEXT("\\"), TEXT("/"));

    if (!RootPath.StartsWith(TEXT("/")))
    {
        RootPath.InsertAt(0, TEXT('/'));
    }

    while (RootPath.EndsWith(TEXT("/")) && RootPath.Len() > 1)
    {
        RootPath.LeftChopInline(1);
    }

    return RootPath;
}
} // namespace PuertsBlueprintTsGenerator

class FPuertsBlueprintTsGeneratorModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        if (IsRunningCommandlet())
        {
            return;
        }

        InitializeStyle();

        FContentBrowserModule& ContentBrowserModule =
            FModuleManager::LoadModuleChecked<FContentBrowserModule>(
                TEXT("ContentBrowser"));

        FContentBrowserMenuExtender_SelectedAssets MenuExtender =
            FContentBrowserMenuExtender_SelectedAssets::CreateRaw(
                this,
                &FPuertsBlueprintTsGeneratorModule::ExtendAssetContextMenu);

        AssetMenuExtenderHandle = MenuExtender.GetHandle();
        ContentBrowserModule.GetAllAssetViewContextMenuExtenders().Add(
            MoveTemp(MenuExtender));

        FBlueprintEditorModule& BlueprintEditorModule =
            FModuleManager::LoadModuleChecked<FBlueprintEditorModule>(
                TEXT("Kismet"));

        FAssetEditorExtender BlueprintEditorExtender =
            FAssetEditorExtender::CreateRaw(
                this,
                &FPuertsBlueprintTsGeneratorModule::ExtendBlueprintEditorToolbar);

        BlueprintEditorExtenderHandle = BlueprintEditorExtender.GetHandle();
        BlueprintEditorModule.GetMenuExtensibilityManager()
            ->GetExtenderDelegates()
            .Add(MoveTemp(BlueprintEditorExtender));
    }

    virtual void ShutdownModule() override
    {
        if (AssetMenuExtenderHandle.IsValid() &&
            FModuleManager::Get().IsModuleLoaded(TEXT("ContentBrowser")))
        {
            FContentBrowserModule& ContentBrowserModule =
                FModuleManager::GetModuleChecked<FContentBrowserModule>(
                    TEXT("ContentBrowser"));

            TArray<FContentBrowserMenuExtender_SelectedAssets>& Extenders =
                ContentBrowserModule.GetAllAssetViewContextMenuExtenders();
            Extenders.RemoveAll(
                [Handle = AssetMenuExtenderHandle](
                    const FContentBrowserMenuExtender_SelectedAssets& Extender)
                {
                    return Extender.GetHandle() == Handle;
                });
        }

        AssetMenuExtenderHandle.Reset();

        if (BlueprintEditorExtenderHandle.IsValid() &&
            FModuleManager::Get().IsModuleLoaded(TEXT("Kismet")))
        {
            FBlueprintEditorModule& BlueprintEditorModule =
                FModuleManager::GetModuleChecked<FBlueprintEditorModule>(
                    TEXT("Kismet"));

            TArray<FAssetEditorExtender>& Extenders =
                BlueprintEditorModule.GetMenuExtensibilityManager()
                    ->GetExtenderDelegates();
            Extenders.RemoveAll(
                [Handle = BlueprintEditorExtenderHandle](
                    const FAssetEditorExtender& Extender)
                {
                    return Extender.GetHandle() == Handle;
                });
        }

        BlueprintEditorExtenderHandle.Reset();
        ShutdownStyle();
    }

private:
    void InitializeStyle()
    {
        if (StyleSet.IsValid())
        {
            return;
        }

        const TSharedPtr<IPlugin> PuertsPlugin =
            IPluginManager::Get().FindPlugin(TEXT("Puerts"));
        if (!PuertsPlugin.IsValid())
        {
            UE_LOG(
                LogPuertsBlueprintTsGenerator,
                Warning,
                TEXT("Puerts plugin was not found; toolbar icon is unavailable."));
            return;
        }

        const FString IconPath = FPaths::Combine(
            PuertsPlugin->GetBaseDir(),
            TEXT("Resources/ButtonIcon_40x.png"));
        if (!IFileManager::Get().FileExists(*IconPath))
        {
            UE_LOG(
                LogPuertsBlueprintTsGenerator,
                Warning,
                TEXT("Puerts toolbar icon was not found: %s"),
                *IconPath);
            return;
        }

        StyleSet = MakeShared<FSlateStyleSet>(
            PuertsBlueprintTsGenerator::StyleSetName);
        StyleSet->Set(
            PuertsBlueprintTsGenerator::GenerateIconName,
            new FSlateImageBrush(IconPath, FVector2D(40.f, 40.f)));
        StyleSet->Set(
            TEXT("PuertsBlueprintTsGenerator.Generate.Small"),
            new FSlateImageBrush(IconPath, FVector2D(20.f, 20.f)));
        FSlateStyleRegistry::RegisterSlateStyle(*StyleSet);
    }

    void ShutdownStyle()
    {
        if (!StyleSet.IsValid())
        {
            return;
        }

        FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet);
        ensure(StyleSet.IsUnique());
        StyleSet.Reset();
    }

    FSlateIcon GetGenerateIcon() const
    {
        return StyleSet.IsValid()
            ? FSlateIcon(
                  PuertsBlueprintTsGenerator::StyleSetName,
                  PuertsBlueprintTsGenerator::GenerateIconName,
                  TEXT("PuertsBlueprintTsGenerator.Generate.Small"))
            : FSlateIcon();
    }

    TSharedRef<FExtender> ExtendAssetContextMenu(
        const TArray<FAssetData>& SelectedAssets)
    {
        TSharedRef<FExtender> Extender = MakeShared<FExtender>();

        bool bContainsBlueprint = false;
        for (const FAssetData& AssetData : SelectedAssets)
        {
            if (Cast<UBlueprint>(AssetData.GetAsset()))
            {
                bContainsBlueprint = true;
                break;
            }
        }

        if (bContainsBlueprint)
        {
            Extender->AddMenuExtension(
                TEXT("GetAssetActions"),
                EExtensionHook::After,
                nullptr,
                FMenuExtensionDelegate::CreateRaw(
                    this,
                    &FPuertsBlueprintTsGeneratorModule::AddBlueprintMenuEntry,
                    SelectedAssets));
        }

        return Extender;
    }

    void AddBlueprintMenuEntry(
        FMenuBuilder& MenuBuilder,
        TArray<FAssetData> SelectedAssets)
    {
        MenuBuilder.AddMenuEntry(
            LOCTEXT(
                "GeneratePuertsTypeScript",
                "生成 Puerts TypeScript 文件"),
            LOCTEXT(
                "GeneratePuertsTypeScriptTooltip",
                "在工程 TypeScript 目录中按蓝图路径创建 Puerts Mixin 模板。"),
            GetGenerateIcon(),
            FUIAction(FExecuteAction::CreateRaw(
                this,
                &FPuertsBlueprintTsGeneratorModule::GenerateSelectedBlueprints,
                MoveTemp(SelectedAssets))));
    }

    TSharedRef<FExtender> ExtendBlueprintEditorToolbar(
        const TSharedRef<FUICommandList> CommandList,
        const TArray<UObject*> ContextSensitiveObjects)
    {
        TSharedRef<FExtender> Extender = MakeShared<FExtender>();

        UBlueprint* Blueprint = nullptr;
        for (UObject* Object : ContextSensitiveObjects)
        {
            Blueprint = Cast<UBlueprint>(Object);
            if (Blueprint)
            {
                break;
            }
        }

        if (Blueprint)
        {
            Extender->AddToolBarExtension(
                TEXT("Asset"),
                EExtensionHook::After,
                CommandList,
                FToolBarExtensionDelegate::CreateRaw(
                    this,
                    &FPuertsBlueprintTsGeneratorModule::AddBlueprintToolbarButton,
                    TWeakObjectPtr<UBlueprint>(Blueprint)));
        }

        return Extender;
    }

    void AddBlueprintToolbarButton(
        FToolBarBuilder& ToolBarBuilder,
        TWeakObjectPtr<UBlueprint> Blueprint)
    {
        ToolBarBuilder.AddToolBarButton(
            FUIAction(FExecuteAction::CreateRaw(
                this,
                &FPuertsBlueprintTsGeneratorModule::GenerateBlueprintFromToolbar,
                Blueprint)),
            NAME_None,
            LOCTEXT("GeneratePuertsTypeScriptToolbarLabel", "生成 TS"),
            LOCTEXT(
                "GeneratePuertsTypeScriptToolbarTooltip",
                "为当前蓝图生成 Puerts TypeScript Mixin 文件。"),
            GetGenerateIcon(),
            EUserInterfaceActionType::Button,
            TEXT("GeneratePuertsTypeScript"));
    }

    void GenerateBlueprintFromToolbar(TWeakObjectPtr<UBlueprint> Blueprint)
    {
        if (!Blueprint.IsValid())
        {
            return;
        }

        TArray<UBlueprint*> Blueprints;
        Blueprints.Add(Blueprint.Get());
        GenerateBlueprints(MoveTemp(Blueprints));
    }

    void GenerateSelectedBlueprints(TArray<FAssetData> SelectedAssets)
    {
        TArray<UBlueprint*> Blueprints;
        for (const FAssetData& AssetData : SelectedAssets)
        {
            if (UBlueprint* Blueprint = Cast<UBlueprint>(AssetData.GetAsset()))
            {
                Blueprints.Add(Blueprint);
            }
        }

        if (Blueprints.IsEmpty())
        {
            return;
        }

        GenerateBlueprints(MoveTemp(Blueprints));
    }

    void GenerateBlueprints(TArray<UBlueprint*> Blueprints)
    {
        int32 ExistingFileCount = 0;
        for (const UBlueprint* Blueprint : Blueprints)
        {
            if (IFileManager::Get().FileExists(*GetOutputFilePath(Blueprint)))
            {
                ++ExistingFileCount;
            }
        }

        bool bOverwriteExistingFiles = false;
        if (ExistingFileCount > 0)
        {
            const EAppReturnType::Type Answer = FMessageDialog::Open(
                EAppMsgType::YesNoCancel,
                FText::Format(
                    LOCTEXT(
                        "ConfirmOverwrite",
                        "已有 {0} 个 TypeScript 文件。\n\n"
                        "选择“是”覆盖；选择“否”只生成缺失文件；选择“取消”停止。"),
                    FText::AsNumber(ExistingFileCount)));

            if (Answer == EAppReturnType::Cancel)
            {
                return;
            }

            bOverwriteExistingFiles = Answer == EAppReturnType::Yes;
        }

        int32 GeneratedCount = 0;
        int32 SkippedCount = 0;
        int32 FailedCount = 0;
        int32 AddedImportCount = 0;
        FString FirstGeneratedDirectory;
        TArray<FString> ScriptFilesToImport;

        for (UBlueprint* Blueprint : Blueprints)
        {
            const FString OutputFile = GetOutputFilePath(Blueprint);
            if (IFileManager::Get().FileExists(*OutputFile) &&
                !bOverwriteExistingFiles)
            {
                ++SkippedCount;
                ScriptFilesToImport.Add(OutputFile);
                continue;
            }

            FString ErrorMessage;
            if (GenerateBlueprintScript(Blueprint, OutputFile, ErrorMessage))
            {
                ++GeneratedCount;
                ScriptFilesToImport.Add(OutputFile);
                if (FirstGeneratedDirectory.IsEmpty())
                {
                    FirstGeneratedDirectory = FPaths::GetPath(OutputFile);
                }
            }
            else
            {
                ++FailedCount;
                UE_LOG(
                    LogPuertsBlueprintTsGenerator,
                    Error,
                    TEXT("%s: %s"),
                    *GetNameSafe(Blueprint),
                    *ErrorMessage);
            }
        }

        FString ImportErrorMessage;
        const bool bImportsUpdated = UpdateMainEntryImports(
            ScriptFilesToImport,
            AddedImportCount,
            ImportErrorMessage);
        if (!bImportsUpdated)
        {
            UE_LOG(
                LogPuertsBlueprintTsGenerator,
                Error,
                TEXT("Failed to update MainGame imports: %s"),
                *ImportErrorMessage);
        }

        const FText ResultText = FText::Format(
            LOCTEXT(
                "GenerationResult",
                "Puerts TS：生成 {0}，跳过 {1}，失败 {2}，MainGame 导入 {3}{4}"),
            FText::AsNumber(GeneratedCount),
            FText::AsNumber(SkippedCount),
            FText::AsNumber(FailedCount),
            FText::AsNumber(AddedImportCount),
            bImportsUpdated
                ? FText::GetEmpty()
                : LOCTEXT("MainGameImportFailed", "（更新失败）"));

        FNotificationInfo Notification(ResultText);
        Notification.ExpireDuration = 6.f;
        FSlateNotificationManager::Get().AddNotification(Notification);

        if (!FirstGeneratedDirectory.IsEmpty())
        {
            UE_LOG(
                LogPuertsBlueprintTsGenerator,
                Display,
                TEXT("Generated TypeScript files under: %s"),
                *FirstGeneratedDirectory);
        }
    }

    FString GetOutputRootDirectory() const
    {
        const UPuertsBlueprintTsGeneratorSettings* Settings =
            GetDefault<UPuertsBlueprintTsGeneratorSettings>();

        FString OutputDirectory = Settings->OutputDirectory;
        OutputDirectory.TrimStartAndEndInline();
        OutputDirectory.ReplaceInline(TEXT("\\"), TEXT("/"));

        // 输出目录始终限制在工程目录内，避免配置错误写到工程外部。
        if (OutputDirectory.IsEmpty() ||
            !FPaths::IsRelative(OutputDirectory) ||
            OutputDirectory.Contains(TEXT("..")))
        {
            OutputDirectory = TEXT("TypeScript");
        }

        return FPaths::ConvertRelativePathToFull(
            FPaths::Combine(FPaths::ProjectDir(), OutputDirectory));
    }

    FString GetMainEntryFilePath() const
    {
        const UPuertsBlueprintTsGeneratorSettings* Settings =
            GetDefault<UPuertsBlueprintTsGeneratorSettings>();

        FString MainEntryFile = Settings->MainEntryFile;
        MainEntryFile.TrimStartAndEndInline();
        MainEntryFile.ReplaceInline(TEXT("\\"), TEXT("/"));

        if (MainEntryFile.IsEmpty() ||
            !FPaths::IsRelative(MainEntryFile) ||
            MainEntryFile.Contains(TEXT("..")))
        {
            MainEntryFile = TEXT("MainGame.ts");
        }

        return FPaths::Combine(GetOutputRootDirectory(), MainEntryFile);
    }

    FString GetOutputFilePath(const UBlueprint* Blueprint) const
    {
        const UPuertsBlueprintTsGeneratorSettings* Settings =
            GetDefault<UPuertsBlueprintTsGeneratorSettings>();

        const FString PackageName = Blueprint->GetOutermost()->GetName();
        FString PackagePath = FPackageName::GetLongPackagePath(PackageName);
        const FString RootPath =
            PuertsBlueprintTsGenerator::NormalizeVirtualRoot(
                Settings->BlueprintRootPath);

        FString RelativePackagePath = PackagePath;
        if (RelativePackagePath == RootPath)
        {
            RelativePackagePath.Reset();
        }
        else if (RelativePackagePath.StartsWith(RootPath + TEXT("/")))
        {
            RelativePackagePath.RightChopInline(RootPath.Len() + 1);
        }
        else
        {
            RelativePackagePath.RemoveFromStart(TEXT("/"));
        }

        const FString AbsoluteOutputRoot = GetOutputRootDirectory();
        const FString AbsoluteOutputDirectory = RelativePackagePath.IsEmpty()
            ? AbsoluteOutputRoot
            : FPaths::Combine(AbsoluteOutputRoot, RelativePackagePath);

        return FPaths::Combine(
            AbsoluteOutputDirectory,
            Blueprint->GetName() + TEXT(".ts"));
    }

    bool UpdateMainEntryImports(
        const TArray<FString>& ScriptFiles,
        int32& OutAddedImportCount,
        FString& OutErrorMessage) const
    {
        OutAddedImportCount = 0;

        const UPuertsBlueprintTsGeneratorSettings* Settings =
            GetDefault<UPuertsBlueprintTsGeneratorSettings>();
        if (!Settings->bUpdateMainEntryImports || ScriptFiles.IsEmpty())
        {
            return true;
        }

        const FString MainEntryFilePath = GetMainEntryFilePath();
        FString MainEntryContent;
        if (IFileManager::Get().FileExists(*MainEntryFilePath) &&
            !FFileHelper::LoadFileToString(
                MainEntryContent,
                *MainEntryFilePath))
        {
            OutErrorMessage = FString::Printf(
                TEXT("无法读取入口文件：%s"),
                *MainEntryFilePath);
            return false;
        }

        TArray<FString> ImportLinesToAdd;
        for (const FString& ScriptFile : ScriptFiles)
        {
            FString ImportPath = ScriptFile;
            if (!FPaths::MakePathRelativeTo(
                    ImportPath,
                    *MainEntryFilePath))
            {
                OutErrorMessage = FString::Printf(
                    TEXT("无法计算相对于入口文件的导入路径：%s"),
                    *ScriptFile);
                return false;
            }

            ImportPath.ReplaceInline(TEXT("\\"), TEXT("/"));
            if (ImportPath.EndsWith(TEXT(".ts"), ESearchCase::IgnoreCase))
            {
                ImportPath.LeftChopInline(3);
            }

            if (!ImportPath.StartsWith(TEXT(".")))
            {
                ImportPath = TEXT("./") + ImportPath;
            }

            const FString DoubleQuotedPath =
                TEXT("\"") + ImportPath + TEXT("\"");
            const FString SingleQuotedPath =
                TEXT("'") + ImportPath + TEXT("'");
            if (MainEntryContent.Contains(DoubleQuotedPath) ||
                MainEntryContent.Contains(SingleQuotedPath))
            {
                continue;
            }

            ImportLinesToAdd.AddUnique(
                FString::Printf(TEXT("import \"%s\";"), *ImportPath));
        }

        if (ImportLinesToAdd.IsEmpty())
        {
            return true;
        }

        ImportLinesToAdd.Sort();

        TArray<FString> Lines;
        MainEntryContent.ParseIntoArrayLines(Lines, false);

        int32 FirstImportLine = INDEX_NONE;
        int32 LastRelativeSideEffectImportLine = INDEX_NONE;
        for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
        {
            FString TrimmedLine = Lines[LineIndex];
            TrimmedLine.TrimStartAndEndInline();

            if (FirstImportLine == INDEX_NONE &&
                TrimmedLine.StartsWith(TEXT("import ")))
            {
                FirstImportLine = LineIndex;
            }

            if (TrimmedLine.StartsWith(TEXT("import \"./")) ||
                TrimmedLine.StartsWith(TEXT("import \"../")) ||
                TrimmedLine.StartsWith(TEXT("import './")) ||
                TrimmedLine.StartsWith(TEXT("import '../")))
            {
                LastRelativeSideEffectImportLine = LineIndex;
            }
        }

        int32 InsertLine = 0;
        if (LastRelativeSideEffectImportLine != INDEX_NONE)
        {
            InsertLine = LastRelativeSideEffectImportLine + 1;
        }
        else if (FirstImportLine != INDEX_NONE)
        {
            InsertLine = FirstImportLine;
        }

        for (const FString& ImportLine : ImportLinesToAdd)
        {
            Lines.Insert(ImportLine, InsertLine++);
        }

        if (Lines.IsValidIndex(InsertLine) &&
            !Lines[InsertLine].TrimStartAndEnd().IsEmpty())
        {
            Lines.Insert(FString(), InsertLine);
        }

        FString UpdatedContent = FString::Join(Lines, TEXT("\n"));
        UpdatedContent.AppendChar(TEXT('\n'));

        const FString MainEntryDirectory = FPaths::GetPath(MainEntryFilePath);
        if (!IFileManager::Get().MakeDirectory(*MainEntryDirectory, true))
        {
            OutErrorMessage = FString::Printf(
                TEXT("无法创建入口文件目录：%s"),
                *MainEntryDirectory);
            return false;
        }

        if (!FFileHelper::SaveStringToFile(
                UpdatedContent,
                *MainEntryFilePath,
                FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
        {
            OutErrorMessage = FString::Printf(
                TEXT("无法写入入口文件：%s"),
                *MainEntryFilePath);
            return false;
        }

        OutAddedImportCount = ImportLinesToAdd.Num();
        return true;
    }

    bool GenerateBlueprintScript(
        UBlueprint* Blueprint,
        const FString& OutputFile,
        FString& OutErrorMessage) const
    {
        if (!IsValid(Blueprint) || !IsValid(Blueprint->GeneratedClass))
        {
            OutErrorMessage = TEXT("蓝图没有有效的 GeneratedClass，请先编译蓝图。");
            return false;
        }

        const FString PackageName = Blueprint->GetOutermost()->GetName();
        const FString Namespace =
            PuertsBlueprintTsGenerator::BuildTypeScriptNamespace(PackageName);
        const FString GeneratedClassName =
            PuertsBlueprintTsGenerator::SanitizeTypeScriptIdentifier(
                Blueprint->GeneratedClass->GetName());
        const FString TypeScriptClassName = TEXT("TS_") +
            PuertsBlueprintTsGenerator::SanitizeTypeScriptIdentifier(
                Blueprint->GetName());
        const FString UnrealTypeName = FString::Printf(
            TEXT("UE.%s.%s"),
            *Namespace,
            *GeneratedClassName);
        const FString GeneratedClassPath =
            Blueprint->GeneratedClass->GetPathName();

        FString LifecycleStub;
        const UPuertsBlueprintTsGeneratorSettings* Settings =
            GetDefault<UPuertsBlueprintTsGeneratorSettings>();
        if (Settings->bGenerateLifecycleStub)
        {
            if (Blueprint->GeneratedClass->FindFunctionByName(
                    TEXT("ReceiveBeginPlay")))
            {
                LifecycleStub = TEXT("  ReceiveBeginPlay(): void {}\n");
            }
            else if (Blueprint->GeneratedClass->FindFunctionByName(
                         TEXT("Construct")))
            {
                LifecycleStub = TEXT("  Construct(): void {}\n");
            }
        }

        const FString FileContent = FString::Printf(
            TEXT("import * as UE from \"ue\";\n")
            TEXT("import { blueprint, toDelegate, $Nullable, $ref, $unref } from \"puerts\";\n")
            TEXT("\n")
            TEXT("declare module \"ue\" {\n")
            TEXT("  namespace %s {\n")
            TEXT("    interface %s {}\n")
            TEXT("  }\n")
            TEXT("}\n")
            TEXT("\n")
            TEXT("const UClass = UE.Class.Load(\n")
            TEXT("  \"%s\",\n")
            TEXT(");\n")
            TEXT("const JSClass = blueprint.tojs(UClass);\n")
            TEXT("\n")
            TEXT("interface %s\n")
            TEXT("  extends %s {}\n")
            TEXT("\n")
            TEXT("class %s implements %s {\n")
            TEXT("%s")
            TEXT("}\n")
            TEXT("\n")
            TEXT("blueprint.mixin(JSClass, %s);\n"),
            *Namespace,
            *GeneratedClassName,
            *GeneratedClassPath,
            *TypeScriptClassName,
            *UnrealTypeName,
            *TypeScriptClassName,
            *TypeScriptClassName,
            *LifecycleStub,
            *TypeScriptClassName);

        const FString OutputDirectory = FPaths::GetPath(OutputFile);
        if (!IFileManager::Get().MakeDirectory(*OutputDirectory, true))
        {
            OutErrorMessage = FString::Printf(
                TEXT("无法创建输出目录：%s"),
                *OutputDirectory);
            return false;
        }

        if (!FFileHelper::SaveStringToFile(
                FileContent,
                *OutputFile,
                FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
        {
            OutErrorMessage = FString::Printf(
                TEXT("无法写入文件：%s"),
                *OutputFile);
            return false;
        }

        return true;
    }

    FDelegateHandle AssetMenuExtenderHandle;
    FDelegateHandle BlueprintEditorExtenderHandle;
    TSharedPtr<FSlateStyleSet> StyleSet;
};

IMPLEMENT_MODULE(
    FPuertsBlueprintTsGeneratorModule,
    PuertsBlueprintTsGenerator)

#undef LOCTEXT_NAMESPACE
