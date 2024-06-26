//  
// Copyright (c) 2023 articy Software GmbH & Co. KG. All rights reserved.  
//

#include "CodeFileGenerator.h"
#include "Misc/App.h"
#include "ArticyEditorModule.h"
#include "Misc/FileHelper.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "HAL/PlatformFilemanager.h"
#include "SourceControlHelpers.h"

void CodeFileGenerator::Line(const FString& Line, const bool bSemicolon, const bool bIndent, const int IndentOffset)
{
	if(bIndent)
	{
		//add indenting tabs
		for(int i = 0; i < IndentCount + IndentOffset; ++i)
			FileContent += TEXT("\t");
	}

	FileContent += Line + (bSemicolon ? TEXT(";") : TEXT("")) + TEXT("\n");
}

void CodeFileGenerator::Comment(const FString& Text)
{
	Line(TEXT("/** ") + Text + TEXT(" */"));
}

void CodeFileGenerator::AccessModifier(const FString& Text)
{
	Line(Text + (Text.EndsWith(TEXT(":")) ? TEXT("") : TEXT(":")), false, true, -1);
}

void CodeFileGenerator::UPropertyMacro(const FString& Specifiers)
{
	Line(TEXT("UPROPERTY(") + Specifiers + TEXT(")"));
}

void CodeFileGenerator::UFunctionMacro(const FString& Specifiers)
{
	Line(TEXT("UFUNCTION(") + Specifiers + TEXT(")"));
}

void CodeFileGenerator::Variable(const FString& Type, const FString& Name, const FString& Value, const FString& Comment, const bool bUproperty, const FString& UpropertySpecifiers)
{
	if(!Comment.IsEmpty())
		this->Comment(Comment);
	
	if(bUproperty)
	{
		this->UPropertyMacro(UpropertySpecifiers);
	}

	//type and name
	auto str = Type + TEXT(" ") + Name;
	//default value if set
	if(!Value.IsEmpty())
		str += TEXT(" = ") + Value;
	
	Line(str, true);

	if(bUproperty && Type.Equals(TEXT("FText")))
	{
		static TArray<FString> ReservedNames = { TEXT("Text"), TEXT("DisplayName"), TEXT("MenuText"), TEXT("CreatedBy"), TEXT("StageDirections")};

		if(!ReservedNames.Contains(Name))
		{
			Line(TEXT("UFUNCTION(BlueprintPure, meta=(DisplayName=\"Get ") + SplitName(Name) + TEXT(" (Localized)\"))"));
			Line(Type + TEXT(" Get") + Name + TEXT("() { return GetPropertyText(") + Name + "); }");
		}
	}
}

//---------------------------------------------------------------------------//

void CodeFileGenerator::StartBlock(const bool bIndent)
{
	++BlockCount;
	Line(TEXT("{"));
	if(bIndent)
		PushIndent();
}

void CodeFileGenerator::EndBlock(const bool bUnindent, const bool bSemicolon)
{
	if(BlockCount == 0)
	{
		UE_LOG(LogArticyEditor, Error, TEXT("Block end missmatch!"));
	}
	else
		--BlockCount;

	if(bUnindent)
		PopIndent();

	Line(TEXT("}"), bSemicolon);
}

void CodeFileGenerator::StartClass(const FString& Classname, const FString& Comment, const bool bUClass, const FString& UClassSpecifiers)
{
	const FString ExportMacro = bUClass ? GetExportMacro() : FString();
	
	if(!Comment.IsEmpty())
		this->Comment(Comment);
	if(bUClass)
		Line(TEXT("UCLASS(") + UClassSpecifiers + TEXT(")"));
	Line(TEXT("class ") + ExportMacro + Classname);

	StartBlock(true);
	if(bUClass)
	{
		Line(TEXT("GENERATED_BODY()"));
		Line();
	}
}

void CodeFileGenerator::StartStruct(const FString& Structname, const FString& Comment, const bool bUStruct)
{
	const FString ExportMacro = bUStruct ? GetExportMacro() : FString();
	
	if(!Comment.IsEmpty())
		this->Comment(Comment);
	if(bUStruct)
		Line(TEXT("USTRUCT(BlueprintType)"));
	Line(TEXT("struct ") + ExportMacro + Structname);

	StartBlock(true);
	if(bUStruct)
	{
		Line(TEXT("GENERATED_BODY()"));
		Line();
	}
}

void CodeFileGenerator::EndStruct(const FString& InlineDeclaration)
{
	EndBlock(true, InlineDeclaration.IsEmpty());
	if(!InlineDeclaration.IsEmpty())
		Line(InlineDeclaration, true);
}

FString CodeFileGenerator::GetExportMacro()
{
	// added space
	return FString(FApp::GetProjectName()).ToUpper() + FString(TEXT("_API "));
}

void CodeFileGenerator::WriteToFile() const
{
	if(FileContent.IsEmpty())
		return;

	if(BlockCount > 0)
		UE_LOG(LogArticyEditor, Warning, TEXT("Block count is %d when writing to file!"), BlockCount);

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	ISourceControlModule& SCModule = ISourceControlModule::Get();

	bool bFileExisted = false;
	if(PlatformFile.FileExists(*Path))
	{
		// If the content won't change, don't write the file
		FString OldContent;
		FFileHelper::LoadFileToString(OldContent, *Path);
		if (FileContent.Equals(OldContent))
		{
			return;
		}
	}
	
	bool bCheckOutEnabled = false;
	if(SCModule.IsEnabled())
	{
		bCheckOutEnabled = ISourceControlModule::Get().GetProvider().UsesCheckout();
	}
	
	// try check out the file if it existed
	if(bFileExisted && bCheckOutEnabled)
	{
		USourceControlHelpers::CheckOutFile(*Path);
	}
	
	const bool bFileWritten = FFileHelper::SaveStringToFile(FileContent, *Path, FFileHelper::EEncodingOptions::ForceUTF8);

	// mark the file for add if it's the first time we've written it
	if(!bFileExisted && bFileWritten && SCModule.IsEnabled())
	{
		USourceControlHelpers::MarkFileForAdd(*Path);
	}
}

FString CodeFileGenerator::SplitName(const FString& Name)
{
	FString Result;

	for (int32 i = 0; i < Name.Len(); i++)
	{
		if (FChar::IsUpper(Name[i]) && i > 0)
		{
			Result.AppendChar(' ');
		}
		Result.AppendChar(Name[i]);
	}

	return Result;
}
