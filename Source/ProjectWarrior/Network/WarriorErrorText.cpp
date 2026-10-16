// Fill out your copyright notice in the Description page of Project Settings.


#include "WarriorErrorText.h"

#define LOCTEXT_NAMESPACE "WarriorErrorText"

namespace WarriorErrorTable
{
	using EShow = EWarriorErrorPresentation;
	using EAct = EWarriorErrorAction;

	FWarriorErrorRule Make(const TCHAR* InCode, const FText& InText, EShow InShow, EAct InAction, bool bInClientBug = false)
	{
		FWarriorErrorRule Rule;
		Rule.Code = InCode;
		Rule.Text = InText;
		Rule.Presentation = InShow;
		Rule.Action = InAction;
		Rule.bClientBug = bInClientBug;
		return Rule;
	}

	//(맥락, 규칙). 같은 코드라도 그 화면에서만 다른 문구를 쓸 때
	struct FContextRule
	{
		FName Context;
		FWarriorErrorRule Rule;
	};

	//LOCTEXT는 정적 초기화 때 만들면 현지화가 준비되지 않아 처음 부를 때 만든다.
	//팝업 문구는 한 줄을 20자 안쪽으로 끊어 \n을 직접 넣는다(팝업 글씨 칸이 폭 620에서 자동 줄바꿈하면 어중간한 곳에서 꺾인다).
	//화면 안 문구(Inline·Field)는 화면 칸에 맞춰 줄을 바꾸므로 한 문장으로 둔다
	const TArray<FWarriorErrorRule>& Rules()
	{
		static const TArray<FWarriorErrorRule> Table = {
			//~ 공통(README 오류 코드). retryable true인 코드는 Action Retry
			Make(TEXT("VALIDATION_FAILED"),      LOCTEXT("ValidationFailed", "입력한 값을 다시 확인해 주세요."), EShow::Inline, EAct::None),
			Make(TEXT("INVALID_REQUEST_BODY"),   LOCTEXT("InvalidRequestBody", "문제가 발생했습니다.\n(오류 코드: {Code})"), EShow::Popup, EAct::DiscardStored, true),
			Make(TEXT("NOT_FOUND"),              LOCTEXT("NotFound", "문제가 발생했습니다.\n(오류 코드: {Code})"), EShow::Popup, EAct::DiscardStored, true),
			Make(TEXT("METHOD_NOT_ALLOWED"),     LOCTEXT("MethodNotAllowed", "문제가 발생했습니다.\n(오류 코드: {Code})"), EShow::Popup, EAct::DiscardStored, true),
			Make(TEXT("INTERNAL_ERROR"),         LOCTEXT("InternalError", "서버가 잠시 응답하지 않습니다. 잠시 뒤 다시 시도해 주세요."), EShow::Popup, EAct::Retry),
			Make(TEXT("SERVICE_UNAVAILABLE"),    LOCTEXT("ServiceUnavailable", "서버가 잠시 응답하지 않습니다. 잠시 뒤 다시 시도해 주세요."), EShow::Popup, EAct::Retry),
			Make(TEXT("VERSION_CONFLICT"),       LOCTEXT("VersionConflict", "요청이 몰려 저장하지 못했습니다.\n잠시 뒤 다시 시도해 주세요."), EShow::Popup, EAct::Retry),
			Make(TEXT("REQUEST_IN_PROGRESS"),    LOCTEXT("RequestInProgress", "요청을 처리하는 중입니다.\n잠시 뒤 다시 시도해 주세요."), EShow::Popup, EAct::Retry),
			Make(TEXT("IDEMPOTENCY_KEY_REUSED"), LOCTEXT("IdempotencyKeyReused", "문제가 발생했습니다.\n(오류 코드: {Code})"), EShow::Popup, EAct::DiscardStored, true),

			//~ 게임이 만드는 코드(서버 코드 아님)
			Make(TEXT("NETWORK_ERROR"),          LOCTEXT("NetworkError", "서버에 연결할 수 없습니다. 잠시 뒤 다시 시도해 주세요."), EShow::Popup, EAct::Retry),
			Make(TEXT("INVALID_RESPONSE"),       LOCTEXT("InvalidResponse", "서버 응답을 읽지 못했습니다.\n(오류 코드: {Code})"), EShow::Popup, EAct::None, true),
			Make(TEXT("PENDING_CORRUPTED"),      LOCTEXT("PendingCorrupted", "저장해 둔 결과 파일이 손상되어\n보낼 수 없습니다."), EShow::Popup, EAct::DiscardStored),
			Make(TEXT("PENDING_EXPIRED"),        LOCTEXT("PendingExpired", "보관한 지 24시간이 지나\n보낼 수 없습니다."), EShow::Popup, EAct::DiscardStored),
			Make(TEXT("SESSION_CONNECTION_LOST"), LOCTEXT("SessionConnectionLost", "서버와 연결이 끊어졌습니다.\n다시 로그인해 주세요."), EShow::Popup, EAct::GoTitle),
			Make(TEXT("UNKNOWN"),                LOCTEXT("Unknown", "문제가 발생했습니다.\n(오류 코드: {Code})"), EShow::Popup, EAct::None),

			//~ 인증(auth-api.md)
			Make(TEXT("AUTH_INVALID_CREDENTIALS"), LOCTEXT("AuthInvalidCredentials", "아이디 또는 비밀번호가 맞지 않습니다."), EShow::Inline, EAct::None),
			Make(TEXT("AUTH_LOGIN_LOCKED"),      LOCTEXT("AuthLoginLocked", "로그인 시도가 너무 많습니다. 잠시 뒤 다시 시도해 주세요."), EShow::Inline, EAct::None),
			Make(TEXT("ACCOUNT_SUSPENDED"),      LOCTEXT("AccountSuspended", "이용이 제한된 계정입니다."), EShow::Inline, EAct::None),
			Make(TEXT("ACCOUNT_LOGIN_ID_DUPLICATED"), LOCTEXT("AccountLoginIdDuplicated", "이미 사용 중인 아이디입니다."), EShow::Field, EAct::None),
			Make(TEXT("ACCOUNT_NICKNAME_DUPLICATED"), LOCTEXT("AccountNicknameDuplicated", "이미 사용 중인 닉네임입니다."), EShow::Field, EAct::None),
			Make(TEXT("AUTH_TOKEN_MISSING"),     LOCTEXT("AuthTokenMissing", "로그인 정보가 올바르지 않습니다.\n다시 로그인해 주세요."), EShow::Popup, EAct::GoLogin, true),
			Make(TEXT("AUTH_TOKEN_INVALID"),     LOCTEXT("AuthTokenInvalid", "로그인 정보가 올바르지 않습니다.\n다시 로그인해 주세요."), EShow::Popup, EAct::GoLogin, true),
			Make(TEXT("AUTH_SESSION_NOT_FOUND"), LOCTEXT("AuthSessionNotFound", "접속이 종료되었습니다.\n다시 로그인해 주세요."), EShow::Popup, EAct::GoLogin),
			Make(TEXT("AUTH_SESSION_REPLACED"),  LOCTEXT("AuthSessionReplaced", "다른 곳에서 같은 계정으로 로그인하여\n접속이 종료되었습니다."), EShow::Popup, EAct::GoTitle),

			//~ 스테이지(stage-api.md)
			Make(TEXT("STAGE_NOT_FOUND"),        LOCTEXT("StageNotFound", "스테이지 정보를 찾을 수 없습니다."), EShow::Popup, EAct::GoMainMenu, true),
			Make(TEXT("STAGE_LOCKED"),           LOCTEXT("StageLocked", "아직 열리지 않은 스테이지입니다."), EShow::Popup, EAct::GoMainMenu),
			//게임이 만드는 판 시작 실패(M24 서버 웨이브 구성, stage-play-api.md "웨이브 구성"). 서버는 보내지 않는다
			Make(TEXT("STAGE_WAVE_PLAN_TIMEOUT"),  LOCTEXT("StageWavePlanTimeout", "스테이지 정보를 받지 못했습니다.\n잠시 뒤 다시 시도해 주세요."), EShow::Popup, EAct::GoMainMenu),
			Make(TEXT("STAGE_WAVE_PLAN_MISSING"),  LOCTEXT("StageWavePlanMissing", "이 버전에서 열 수 없는 스테이지입니다."), EShow::Popup, EAct::GoMainMenu, true),
			Make(TEXT("STAGE_WAVE_MONSTER_UNKNOWN"), LOCTEXT("StageWaveMonsterUnknown", "이 버전에서 열 수 없는 스테이지입니다."), EShow::Popup, EAct::GoMainMenu),
			//게임이 만드는 포기 실패(일시정지 → 나가기의 P4가 연결 실패·5xx 등). 메인메뉴 이동은 부른 쪽이 하므로 다음 행동은 없다.
			//서버 코드(HTTP_5xx 등)에 맥락 줄을 다는 방식은 HTTP 상태 기본값(④)이 맥락보다 먼저 잡혀 이 문구가 나오지 않으므로 코드 하나로 둔다
			Make(TEXT("STAGE_PLAY_ABANDON_FAILED"), LOCTEXT("StagePlayAbandonFailed", "진행 중인 스테이지를 포기하지 못했습니다.\n다음 로그인 때 다시 묻습니다."), EShow::Popup, EAct::None),

			//~ 스테이지 플레이(stage-play-api.md)
			Make(TEXT("STAGE_PLAY_NOT_FOUND"),   LOCTEXT("StagePlayNotFound", "스테이지 플레이 기록이 없어\n결과를 저장할 수 없습니다."), EShow::Popup, EAct::DiscardStored, true),
			Make(TEXT("STAGE_PLAY_IN_PROGRESS"), LOCTEXT("StagePlayInProgress", "진행 중인 스테이지 플레이가 있습니다."), EShow::LogOnly, EAct::None),
			Make(TEXT("STAGE_PLAY_NOT_IN_PROGRESS"), LOCTEXT("StagePlayNotInProgress", "이미 끝난 스테이지 플레이라\n결과를 저장할 수 없습니다."), EShow::Popup, EAct::DiscardStored),
			Make(TEXT("STAGE_PLAY_EXPIRED"),     LOCTEXT("StagePlayExpired", "플레이한 지 24시간이 지나\n결과를 저장할 수 없습니다."), EShow::Popup, EAct::DiscardStored),

			//~ 결과(result-api.md)
			Make(TEXT("RESULT_INVALID"),         LOCTEXT("ResultInvalid", "결과를 확인할 수 없어\n저장하지 못했습니다."), EShow::Popup, EAct::DiscardStored),
			Make(TEXT("RESULT_MISMATCH"),        LOCTEXT("ResultMismatch", "결과를 확인할 수 없어\n저장하지 못했습니다."), EShow::Popup, EAct::DiscardStored, true),
			Make(TEXT("STAGE_RESULT_NOT_FOUND"), LOCTEXT("StageResultNotFound", "저장된 결과가 없습니다."), EShow::LogOnly, EAct::None),

			//~ c: STAT_·SKILL_ (S3·S4) 줄을 여기에 더한다
			//~ d: RANK_ (랭킹) 줄을 여기에 더한다
		};
		return Table;
	}

	const TArray<FContextRule>& ContextRules()
	{
		static const TArray<FContextRule> Table = {
			{ WarriorErrorContext::Login,  Make(TEXT("VALIDATION_FAILED"),    LOCTEXT("LoginValidation", "아이디와 비밀번호를 입력해 주세요."), EShow::Inline, EAct::None) },
			{ WarriorErrorContext::Login,  Make(TEXT("INVALID_REQUEST_BODY"), LOCTEXT("LoginUnknown", "로그인하지 못했습니다. 잠시 뒤 다시 시도해 주세요."), EShow::Inline, EAct::None, true) },
			{ WarriorErrorContext::Login,  Make(TEXT("INVALID_RESPONSE"),     LOCTEXT("LoginInvalidResponse", "로그인하지 못했습니다. 잠시 뒤 다시 시도해 주세요."), EShow::Inline, EAct::None, true) },
			{ WarriorErrorContext::Login,  Make(TEXT("UNKNOWN"),              LOCTEXT("LoginUnknownCode", "로그인하지 못했습니다. 잠시 뒤 다시 시도해 주세요."), EShow::Inline, EAct::None) },
			{ WarriorErrorContext::Signup, Make(TEXT("INVALID_REQUEST_BODY"), LOCTEXT("SignupUnknown", "가입하지 못했습니다. 잠시 뒤 다시 시도해 주세요."), EShow::Inline, EAct::None, true) },
			{ WarriorErrorContext::Signup, Make(TEXT("UNKNOWN"),              LOCTEXT("SignupUnknownCode", "가입하지 못했습니다. 잠시 뒤 다시 시도해 주세요."), EShow::Inline, EAct::None) },
		};
		return Table;
	}

	//접두어 기본값. 표에 없는 새 코드가 와도 그 기능의 기본 처리를 하게 한다
	const TArray<FWarriorErrorRule>& PrefixRules()
	{
		static const TArray<FWarriorErrorRule> Table = {
			Make(TEXT("AUTH_"),       LOCTEXT("PrefixAuth", "다시 로그인해 주세요.\n(오류 코드: {Code})"), EShow::Popup, EAct::GoLogin),
			Make(TEXT("STAGE_PLAY_"), LOCTEXT("PrefixStagePlay", "결과를 저장하지 못했습니다.\n(오류 코드: {Code})"), EShow::Popup, EAct::DiscardStored),
			Make(TEXT("RESULT_"),     LOCTEXT("PrefixResult", "결과를 저장하지 못했습니다.\n(오류 코드: {Code})"), EShow::Popup, EAct::DiscardStored),
			Make(TEXT("STAGE_"),      LOCTEXT("PrefixStage", "스테이지를 시작하지 못했습니다.\n(오류 코드: {Code})"), EShow::Popup, EAct::GoMainMenu),
		};
		return Table;
	}

	const FWarriorErrorRule* FindIn(const TArray<FWarriorErrorRule>& InTable, const FString& InCode)
	{
		return InTable.FindByPredicate([&InCode](const FWarriorErrorRule& Rule) { return Rule.Code == InCode; });
	}
}

FWarriorErrorRule FWarriorErrorText::Resolve(const FWarriorApiError& InError, FName InContext)
{
	using namespace WarriorErrorTable;

	const FString& Code = InError.Code;
	const FWarriorErrorRule* Found = nullptr;

	auto FindContext = [InContext](const FString& InCode) -> const FWarriorErrorRule*
	{
		if (InContext.IsNone() || InCode.IsEmpty())
		{
			return nullptr;
		}
		const FContextRule* ContextRule = ContextRules().FindByPredicate([InContext, &InCode](const FContextRule& Each)
		{
			return Each.Context == InContext && Each.Rule.Code == InCode;
		});
		return ContextRule ? &ContextRule->Rule : nullptr;
	};

	//① 맥락 덮어쓰기
	Found = FindContext(Code);

	//② 코드
	if (!Found)
	{
		Found = FindIn(Rules(), Code);
	}

	//③ 접두어(긴 것부터 적어 두었다: STAGE_PLAY_ → STAGE_)
	if (!Found && !Code.IsEmpty())
	{
		Found = PrefixRules().FindByPredicate([&Code](const FWarriorErrorRule& Rule) { return Code.StartsWith(Rule.Code); });
	}

	//④ HTTP 상태
	if (!Found)
	{
		if (InError.HttpStatus == 0)
		{
			Found = FindIn(Rules(), TEXT("NETWORK_ERROR"));
		}
		else if (InError.HttpStatus == 401)
		{
			Found = FindIn(Rules(), TEXT("AUTH_SESSION_NOT_FOUND"));
		}
		else if (InError.HttpStatus >= 500)
		{
			Found = FindIn(Rules(), TEXT("INTERNAL_ERROR"));
		}
	}

	//⑤ 알 수 없는 오류(맥락의 UNKNOWN 줄이 있으면 그것)
	if (!Found)
	{
		Found = FindContext(TEXT("UNKNOWN"));
	}
	if (!Found)
	{
		Found = FindIn(Rules(), TEXT("UNKNOWN"));
	}

	FWarriorErrorRule Rule = *Found;
	if (!Code.IsEmpty())
	{
		Rule.Code = Code;
	}
	Rule.Text = FText::FormatNamed(Rule.Text, TEXT("Code"), FText::FromString(Rule.Code));
	return Rule;
}

const FWarriorErrorRule* FWarriorErrorText::Find(const FString& InCode)
{
	return WarriorErrorTable::FindIn(WarriorErrorTable::Rules(), InCode);
}

FText FWarriorErrorText::GetText(const FString& InCode, FName InContext)
{
	FWarriorApiError Error;
	Error.Code = InCode;
	Error.HttpStatus = -1;	//상태 기본값을 타지 않게 한다
	return Resolve(Error, InContext).Text;
}

const TArray<FWarriorErrorRule>& FWarriorErrorText::GetAllRules()
{
	return WarriorErrorTable::Rules();
}

FText UWarriorErrorLibrary::GetErrorText(const FString& InCode)
{
	return FWarriorErrorText::GetText(InCode);
}

#undef LOCTEXT_NAMESPACE
