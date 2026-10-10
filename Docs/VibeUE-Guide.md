# VibeUE 도입 가이드 — AI가 언리얼 에디터를 직접 만지게 하자

> **한 줄 요약:** Claude Code(또는 Cursor 등)에게 "BT 만들어줘", "이 위젯에 버튼 추가해줘"라고 말하면
> **AI가 에디터 안에서 에셋을 직접 생성·수정·검증**합니다. 설치는 이미 레포에 들어가 있고,
> 팀원은 **빌드 + 설정 1개**만 하면 됩니다.

---

## 1. 이게 뭔가요?

- 언리얼 5.8에는 AI 에이전트가 에디터를 조작할 수 있는 **MCP 서버**가 기본 내장되어 있습니다.
- **VibeUE**는 그 MCP 서버에 붙는 **확장 플러그인**입니다(MIT 라이선스, 무료). 별도 서버나 채팅창이 없고,
  엔진 기본 MCP 엔드포인트에 툴을 대량으로 추가합니다.
- AI는 에디터 안에서 **Python 스크립트를 한 번에 실행**하는 방식으로 일하기 때문에, 여러 단계 작업
  (생성 → 편집 → 컴파일 → 검증)을 한 번의 호출로 끝냅니다.

---

## 2. 왜 써야 하나요? (장점)

### 2-1. 할 수 있는 작업이 2배 이상

| | 툴셋 | 툴 수 |
|---|---|---|
| 언리얼 5.8 기본 MCP | 53 | 831 |
| **+ VibeUE** | **+37** | **+1,032** |
| 합계 | 90 | **1,863 (약 2.2배)** |

특히 **기본 MCP로는 "조회만" 되던 영역이 "편집"까지** 됩니다.

| 영역 | 기본 MCP | VibeUE 추가 후 |
|---|---|---|
| Behavior Tree | 조회 7개 | **생성·편집·검증 23개** |
| Blackboard | 없음 | **11개** (키 추가/이름변경/참조 추적) |
| EQS | 없음 | **24개** |
| StateTree | 조회 9개 | **98개** |
| 애니메이션 (Sequence/Montage/AnimGraph/Skeleton) | 거의 없음 | **238개** |
| Enhanced Input | 없음 | **28개** (+ PIE 중 입력 주입) |
| 블루프린트 그래프 | 53개 | **+110개** (노드/컴포넌트/타임라인) |
| 위젯(UMG) | 23개 | **+43개** (MVVM, 애니메이션, PIE 확인) |
| 랜드스케이프/폴리지/사운드/성능 분석 | 거의 없음 | 대거 추가 |

### 2-2. 실제 사례 — 우리 프로젝트에서 BT 만들기

요청: *"TargetActor가 보이면 무기를 들고, 800 밖이면 추격, 800 안이면 Strafing 해줘"*

AI가 한 일:
1. 기존 `BT_EnemyAI`(사무라이)를 읽어서 **우리 팀이 쓰는 노드/패턴을 파악**
   (`BTD_HasWeapon`, `BT_ActivateAbilityByTag`, `BT_ToggleStrafing`, `BTS_GetDistToTarget`, `EQS_FindStrafingLocation`)
2. 블랙보드 키 추가 → BT 노드 20여 개 배치 → 데코레이터/서비스/키 바인딩/Abort 모드 설정
3. 컴파일 + 저장 + `validate_tree`로 **깨진 키 바인딩이 없는지 검증**

→ 사람이 노드를 하나하나 끌어다 놓던 작업이 **Python 호출 한 번(약 5초)**에 끝났습니다.

### 2-3. 안전장치가 똑똑함

AI가 마구 쓰는 게 아니라, VibeUE 서비스가 **위험한 쓰기를 스스로 거부**합니다. 이번 작업 중 실제로 막힌 사례:

- **에셋 에디터가 열려 있으면 쓰기 거부** — 열린 에디터가 나중에 저장하면서 변경을 덮어쓰는 사고를 방지
- **타입이 안 맞는 블랙보드 키 바인딩 거부** — `MoveTo`에 기본 클래스 없는 Object 키를 넣으려 하자
  "저장 시 조용히 None으로 초기화된다"는 이유와 함께 거부
- **PIE 실행 중 블랙보드 수정 거부** — 런타임 크래시 방지

### 2-4. 눈으로 확인까지

- `capture_image`로 **에디터 창 / 뷰포트 / PIE 게임 화면(UMG HUD 포함)** 스크린샷을 AI가 직접 보고 판단합니다.
- PIE 중 **입력 주입**(`InputService.inject_action`)과 **테스트 액터 스폰**(`PIEActorService`)으로
  "실제로 동작하는지"까지 AI가 돌려볼 수 있습니다.
- GAS 쪽은 엔진 기본 툴로 **런타임 ASC 상태(부여 어빌리티, 활성 태그/이펙트, 어트리뷰트 값)**를 조회할 수 있어
  "무기 장착 어빌리티가 진짜 발동했나?" 같은 확인이 가능합니다.

### 2-5. 도메인 지식(스킬) 내장

약 88개의 **스킬 팩**(블루프린트, 상태트리, 머티리얼, 나이아가라 등의 작업 요령/주의사항)이 들어 있어,
AI가 필요한 순간에만 읽어서 엔진 특유의 함정을 피해 갑니다.

---

## 3. 세팅법 (우리 레포 기준, 약 10분)

> 플러그인 소스와 MCP 연결 설정은 **이미 레포에 커밋**되어 있습니다. 직접 할 일은 아래뿐입니다.

### 현재 레포 상태

| 항목 | 위치 | 상태 |
|---|---|---|
| VibeUE 플러그인 소스 | `ProjectWarrior/Plugins/VibeUE/` | ✅ 커밋됨 (`test/plugin` 브랜치) |
| MCP 클라이언트 설정 | `PW01/.mcp.json` (`http://127.0.0.1:8000/mcp`) | ✅ 커밋됨 |
| Claude Code 설정 | `PW01/.claude/settings.json` (`unreal-mcp` 허용) | ✅ 커밋됨 |
| AI용 사용 가이드 | `ProjectWarrior/CLAUDE.md` | ✅ 커밋됨 |
| 플러그인 바이너리 | `Plugins/VibeUE/Binaries/` | ❌ gitignore → **프로젝트 빌드 시 함께 빌드됨** |
| MCP 서버 자동 시작 | 에디터 개인 설정 | ❌ **각자 켜기** |

### Step 1 — 빌드

평소처럼 프로젝트를 빌드하면 VibeUE도 함께 컴파일됩니다. 아래 중 편한 방법을 쓰세요.

- `.sln`에서 **Development Editor**로 빌드
- `.uproject`를 열고 "모듈을 다시 빌드할까요?" 창에서 **예**
- **플러그인 빌드 스크립트** (빌드 후 에디터까지 실행) ↓

#### 빌드 스크립트 사용 시

`BuildAndLaunchGame.ps1`은 **PowerShell 스크립트**입니다. Git Bash나 cmd가 아니라 **PowerShell**에서,
`ProjectWarrior` 폴더(`.uproject`가 있는 곳)를 기준으로 실행하세요. 실행 중인 에디터는 스크립트가 종료시킵니다.

```powershell
cd D:\경로\PW01\ProjectWarrior
.\Plugins\VibeUE\BuildAndLaunchGame.ps1
```

"스크립트를 실행할 수 없습니다(실행 정책)" 오류가 나면 이렇게 실행합니다.

```powershell
powershell -ExecutionPolicy Bypass -File .\Plugins\VibeUE\BuildAndLaunchGame.ps1
```

**엔진이 기본 경로에 설치되어 있지 않다면 엔진 경로를 알려줘야 합니다.**
스크립트는 아래 순서로 엔진을 찾습니다.

1. 레지스트리에 등록된 소스 빌드 엔진
2. 레지스트리에 등록된 런처 설치 엔진
3. `C:` / `D:` / `E:` 드라이브의 `\Program Files\Epic Games\UE_5.8`

모두 실패하면 PowerShell 창에 **`Enter the full path to your Unreal Engine install ...`** 이라는 입력 요청이 뜹니다.
여기에 엔진 루트 폴더(`Engine` 폴더의 상위, 예: `F:\Epic\UE_5.8`)를 입력하면 그대로 진행됩니다.
그 아래에 `Engine\Build\BatchFiles\Build.bat`가 있어야 올바른 경로입니다.

매번 입력하기 번거롭다면 처음부터 옵션으로 넘겨도 됩니다.

```powershell
.\Plugins\VibeUE\BuildAndLaunchGame.ps1 -UnrealEnginePath "F:\Epic\UE_5.8"
```

> **AI에게 빌드를 시킬 때 주의:** AI가 스크립트를 실행하면 입력을 받을 수 없어서, 엔진을 못 찾으면
> 경로를 묻지 않고 바로 실패합니다. 엔진이 기본 경로에 없다면 AI에게도 `-UnrealEnginePath`를 붙여서
> 실행하라고 알려주세요.

자주 쓰는 옵션:

| 옵션 | 용도 |
|---|---|
| `-UnrealEnginePath "<경로>"` | 엔진 경로 직접 지정 |
| `-Map /Game/Maps/<맵이름>` | 특정 맵으로 에디터 열기 |
| `-SkipBuild` | 빌드 없이 에디터만 다시 실행 |
| `-Clean` | 빌드 산출물 삭제 후 다시 빌드 |

### Step 2 — MCP 서버 자동 시작 켜기 (최초 1회)

에디터에서 **편집 → 에디터 환경설정 → General → Model Context Protocol → `Auto Start Server` 체크**
(또는 콘솔에 `ModelContextProtocol.StartServer`)

- 함께 **`Tool Search`도 켜는 것을 권장** — AI의 컨텍스트 사용량이 크게 줄어듭니다.
- **편집 → 플러그인**에서 `Unreal MCP`, `Editor Tools`, `VibeUE`가 켜져 있는지 확인하세요.
  (Unreal MCP/Editor Tools는 Experimental 표시가 뜨는 게 정상입니다.)

### Step 3 — AI 에이전트 연결

**Claude Code:** `PW01` 레포 루트에서 Claude Code를 실행하면 `.mcp.json`을 읽어 `unreal-mcp`에 자동 연결됩니다.
처음에 MCP 서버 사용 승인을 물으면 허용하세요.

**다른 에이전트(Cursor, VS Code Copilot, Gemini, Codex)를 쓴다면** 에디터 콘솔에서:

```
ModelContextProtocol.GenerateClientConfig Cursor
VibeUE.GenerateAgentConfig Cursor
```

(`ClaudeCode`, `Cursor`, `VSCode`, `Gemini`, `Codex`, `All` 지원)

### Step 4 — 연결 확인

에디터를 켠 상태에서 AI에게:

> "VibeUE 툴셋 목록 보여줘"

툴셋 목록이 나오면 성공입니다.

---

## 4. 사용법

### 이렇게 말하면 됩니다

```
BT_EnemyAI 구조를 읽어서 설명해줘
BB_BossAI에 Phase(Int) 키 추가하고, 어떤 BT가 쓰는지 알려줘
WBP_HUD에 체력바 아래 스태미나 바 추가해줘
AI.Ability.* 태그가 어디서 쓰이는지 정리해줘
PIE 켜서 보스 2페이즈 진입하는지 스크린샷으로 확인해줘
지금 프레임 드랍이 CPU 때문인지 GPU 때문인지 봐줘
```

### 잘 쓰는 팁

- **"기존 ○○를 참고해서"라고 꼭 말하세요.** 말하지 않으면 AI가 일반적인 구조로 새로 만들어서
  팀 컨벤션(우리 커스텀 태스크/데코레이터)과 어긋날 수 있습니다.
- **작업할 에셋의 에디터 창은 닫아두세요.** 열려 있으면 BT/블랙보드 등은 쓰기가 거부됩니다.
- **테스트는 별도 폴더에서** (예: `/Game/AI/Test/`) 먼저 만들어 보고, 확인 후 실제 에셋에 반영하세요.
- **결과는 "검증까지" 요청하세요.** "만들고 validate/컴파일 결과랑 스크린샷도 보여줘"라고 하면
  AI가 증거와 함께 보고합니다.

---

## 5. 주의사항 (꼭 읽어주세요)

| 주의 | 내용 / 대응 |
|---|---|
| **자동 저장** | AI의 Python 실행 툴은 기본적으로 **실행 전에 수정된(더티) 에셋을 모두 저장**합니다. 에디터에서 저장 안 한 작업이 있다면 AI 작업 전에 정리하세요. |
| **.uasset은 머지 불가** | BT/블루프린트는 바이너리라 충돌 시 한쪽을 버려야 합니다. **AI로 수정할 에셋은 담당자를 정하고**, 작업 전후로 커밋하세요. |
| **AI 작업 기록 위치** | AI가 문제를 해결하면 그 요령을 **`CLAUDE.local.md`**(프로젝트 루트, gitignore됨)에 기록하고 다음 세션부터 자동으로 읽습니다. 공유 파일인 `CLAUDE.md`는 고치지 않으니 충돌이 없습니다. 기록은 **내 PC에만** 남으므로, 팀 전체에 유용한 요령은 골라서 `CLAUDE.md`에 PR로 반영해 주세요. |
| **`VibeUE.GenerateAgentConfig` 재실행 주의** | 이 명령은 `CLAUDE.md`의 VibeUE 영역을 다시 씁니다. 실행했다면 "Living gotchas" 지침이 `CLAUDE.local.md`를 가리키는지 확인하세요. |
| **로컬 전용·인증 없음** | MCP 서버는 `127.0.0.1`에서만 열리고 인증이 없습니다. **포트 포워딩/외부 노출 금지.** |
| **Experimental** | Unreal MCP/Editor Tools는 엔진에서 Experimental입니다. 에디터가 멈추면 재시작하면 됩니다. |
| **창이 비활성이면 느림** | 에디터가 백그라운드면 ~3FPS로 떨어집니다. PIE 자동 테스트 시 AI에게 "백그라운드 스로틀링 꺼줘"라고 하면 됩니다. |
| **C++/AttributeSet** | C++ 클래스는 툴로 "조회"만 됩니다. 코드 수정은 일반 코딩 + 빌드로 진행합니다. |

---

## 6. FAQ

**Q. 비용이 드나요?**
VibeUE는 MIT 라이선스 무료입니다. (실세계 지형 생성 기능만 무료 API 키 필요 — 우리 프로젝트엔 불필요.)
AI 에이전트(Claude Code 등) 사용 비용은 별도입니다.

**Q. AI가 우리 에셋을 망가뜨리면요?**
작업 전에 커밋해 두면 `git checkout`으로 되돌릴 수 있습니다. VibeUE에는 에디터 Undo/Redo를 다루는
`TransactionService`도 있습니다. 그리고 앞서 말했듯 위험한 쓰기는 서비스가 먼저 거부합니다.

**Q. 기존 작업 방식을 바꿔야 하나요?**
아니요. 에디터는 평소처럼 쓰고, 반복적이거나 노드가 많은 작업만 AI에게 맡기면 됩니다.

**Q. 어떤 작업이 가장 효과적인가요?**
- 기존 에셋 **구조 파악/정리** (BT, 블루프린트, 태그 참조 추적)
- **반복 배치 작업** (태그 일괄 추가, 여러 BT 검증, 설정값 일괄 변경)
- **패턴 복제** ("사무라이 BT처럼 궁수 BT도 만들어줘")
- **PIE 검증** (입력 주입 + 스크린샷 + ASC 상태 확인)

---

### 참고 링크

- VibeUE: <https://www.vibeue.com/docs> · GitHub `kevinpbuckley/VibeUE`
- Epic 문서: [Unreal MCP in the Unreal Editor](https://dev.epicgames.com/documentation/unreal-engine/unreal-mcp-in-unreal-editor)
- 플러그인 README: `ProjectWarrior/Plugins/VibeUE/README.md`
