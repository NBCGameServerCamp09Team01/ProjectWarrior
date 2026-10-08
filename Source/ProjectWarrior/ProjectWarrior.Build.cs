// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ProjectWarrior : ModuleRules
{
	public ProjectWarrior(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "EnhancedInput",
            "GameplayTags",
            "GameplayTasks",
            "GameplayAbilities",
            "AIModule",
            "NavigationSystem",
            "UMG",
            "AnimGraphRuntime",
            "MotionWarping",
            "Niagara",
            "ALSV4_CPP"
    });

		// 사운드 틀(Audio/): 프로젝트 설정(DeveloperSettings), MetaSound 입력(AudioExtensions), 버튼 소리(SlateCore)
		PrivateDependencyModuleNames.AddRange(new string[] { "DeveloperSettings", "AudioExtensions", "SlateCore" });

        // 서버 통신용 JSON 변환(FJsonObjectConverter)
        PrivateDependencyModuleNames.AddRange(new string[] { "Json", "JsonUtilities", "HTTP" });
        // Uncomment if you are using Slate UI
        // PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

        // Uncomment if you are using online features
        // PrivateDependencyModuleNames.Add("OnlineSubsystem");

        // To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
    }
}
