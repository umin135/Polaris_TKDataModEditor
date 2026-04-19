# KEF 루트 모션 본 파싱 — 발견 및 수정 사항

## 개요

KEF(Keyframe Encoded) 압축 모드에서 **루트 모션 본(Top, Trans 등)** 은 일반 본과는
다른 채널 테이블 레이아웃을 가진다. 이를 잘못 파싱하면 위치 데이터가 회전 데이터(qw)로
오인되어 회전 오류 및 슬라이딩 이슈가 발생한다.

---

## C-block 헤더 — mode 필드

C-block 헤더 오프셋 `[2:4]`에 2바이트 `mode` 값이 있다.

```
mode = 0x0006  →  일반 본 (회전 + 위치 데이터가 모두 있을 수 있음)
mode = 0x000A  →  루트 모션 본 (Top, Trans 등 절대 세계 좌표계 위치 기반)
```

이 `mode` 값에 따라 채널 테이블의 의미가 달라진다.

---

## 44바이트 FBF 기준 채널 순서 (기준 레이아웃)

FBF(비압축) 방식의 44바이트 샘플은 다음과 같이 정의된다.

```
comp[0]  = Scale X
comp[1]  = Scale Y
comp[2]  = Scale Z
comp[3]  = Quaternion X
comp[4]  = Quaternion Y
comp[5]  = Quaternion Z
comp[6]  = Quaternion W
comp[7]  = Position X  (엔진: 좌/우)
comp[8]  = Position Y  (엔진: 전/후)
comp[9]  = Position Z  (엔진: 상/하)
comp[10] = Padding (항상 0)
```

KEF C-block의 채널 테이블 각 엔트리(16바이트)는
`float min, float max, uint32 bits, uint32 (unused)` 구조이며,
테이블 내 인덱스 i는 comp[i]에 대응하는 것이 **일반 본(mode=0x0006)의 규칙**이다.

---

## mode=0x000A 루트 모션 본의 채널 오프셋 (핵심 발견)

루트 모션 본(mode=0x000A)의 채널 테이블은 **인덱스가 1 ずれ(어긋남)** 있다.

```
채널 테이블 인덱스 →  실제 comp[] 슬롯
      idx 0 ~ 5   →  comp[0 ~ 5]   (Scale X/Y/Z, Quat X/Y/Z — 동일)
      idx 6       →  comp[7]       = Position X   ← qw가 아님!
      idx 7       →  comp[8]       = Position Y
      idx 8       →  comp[9]       = Position Z
```

**comp[6](qw 슬롯)은 채널 테이블에 존재하지 않는다.** 루트 모션 본은 항상
회전이 항등(identity)이거나 qx/qy/qz로부터 유도(derive)한다.

### 오프셋 발생 원인

루트 모션 본의 C-block에는 **track이 9개**만 존재한다 (일반 본은 최대 11개).
일반 본의 `idx 6 = qw` 슬롯이 루트 모션 본에서는 생략되고,
위치 채널 3개(posX, posY, posZ)가 idx 6~8에 연속 배치된다.
그 결과 채널 테이블 인덱스가 comp[] 배열 인덱스보다 1 앞선다.

### 검증 (test.bin 기준)

```
base_frame[7] = -0.0241   →  Top ch6(idx=6)의 frame0 디코딩 값 ≈ -0.0239  ✓
base_frame[9] =  0.0279   →  Top ch8(idx=8)의 frame0 디코딩 값 ≈  0.0278  ✓
base_frame[8] = 110.2754  →  Trans ch7(idx=7)의 frame0 디코딩 값 = 110.2754  ✓
```

frame0에서 디코딩된 값이 base_frame의 대응 슬롯과 일치함으로써
1 ずれ 오프셋이 확인된다.

---

## 잘못된 기존 접근: kef_pos_swap

기존 코드는 애니메이션된 채널 1개를 swap으로 이동시키는 방식을 사용했다.

```
Top   → kef_pos_swap(8, 9) : comp[9] = comp[8]; comp[8] = base_frame[8]
Trans → kef_pos_swap(7, 8) : comp[8] = comp[7]; comp[7] = base_frame[7]
```

이 방식의 문제점:

| 항목 | 증상 |
|------|------|
| ch6(posX)가 comp[6]에 기록됨 | `has_qw=True` 판정 → qw = posX 값으로 오인 → 회전 오류 |
| comp[7]이 base_frame[7]로 고정 | posX가 상수(애니메이션 미반영) → 좌우 오프셋 오류 |
| bits=0(정적 채널)일 때 swap 실행 | base_frame[8]의 rest 위치가 base_frame[7]=0으로 덮어짐 → 스탠딩 애니메이션에서 슬라이딩 발생 |

**스탠딩 애니메이션 슬라이딩 재현:**
Trans의 base_frame[8]=110.2754(rest 전방 위치)이 swap으로 인해 0으로 대체되어,
캐릭터가 rest 위치가 아닌 원점에 배치되는 위치 오류 발생.

---

## 올바른 파싱 로직

### 조건 판별

```python
c_mode = struct.unpack('<H', header_chunk[2:4])[0]
is_root_motion_bone = (c_mode == 0x000A)
```

### 채널 디코딩

```python
for idx, ch in enumerate(channels_info):
    if ch["bits"] > 0:
        max_val = (1 << ch["bits"]) - 1
        raw = reader.read_bits(ch["bits"])
        decoded = ch["min"] + (raw / float(max_val)) * (ch["max"] - ch["min"])

        if is_root_motion_bone and idx >= 6:
            comp[idx + 1] = decoded   # 위치 채널: +1 오프셋 적용
        else:
            comp[idx] = decoded       # 일반 본 및 스케일/회전: 그대로
```

### qw 처리

```python
# 루트 모션 본은 qw를 항상 유도, 채널 테이블에 qw 없음
if is_root_motion_bone:
    has_qw = False
else:
    has_qw = (len(channels_info) > 6 and channels_info[6]["bits"] > 0)
```

`has_qw=False`이면 기존 로직대로 `sqrt(1 - qx² - qy² - qz²)`로 qw를 재구성한다.
루트 모션 본은 대부분 회전이 없으므로 qx=qy=qz=0 → qw=1.0(항등).

### 최종 위치 읽기

```python
# 수정 불필요 — comp[7-9]가 이미 올바른 posX/Y/Z를 가짐
_process_frame(comp[7], comp[8], comp[9], qx, qy, qz, qw, sx, sy, sz, frame)
```

---

## D3D11 프리뷰 구현 시 주의사항

KEF 디코딩 후 comp[] 배열에서 위치를 읽을 때:

```cpp
// mode == 0x000A (루트 모션 본)인 경우
float posX = comp[7];   // ch_table[6]이 기록한 값
float posY = comp[8];   // ch_table[7]이 기록한 값
float posZ = comp[9];   // ch_table[8]이 기록한 값
float qw   = derive_qw(qx, qy, qz);  // 항상 유도, 채널 없음

// mode == 0x0006 (일반 본)인 경우
float qw   = comp[6];   // 채널에 명시적으로 인코딩됨(있을 때)
float posX = comp[7];
float posY = comp[8];
float posZ = comp[9];
```

mode=0x000A 본에서 채널 테이블 인덱스 6의 bits > 0이더라도
그것은 qw가 아니라 posX이므로 qw 판별에 사용하면 안 된다.

---

## B-block 스키마 버전 (보너스: 파일에 따른 필드 위치 차이)

일부 파일은 B-block 필드 위치가 다른 스키마를 사용한다.

| 필드 | OLD schema | NEW schema |
|------|-----------|-----------|
| anim_flag | b_off + 0x14 | b_off + 0x04 |
| bone_frames | b_off + 0x18 | b_off + 0x08 |
| indicator | b_off + 0x0C | b_off + 0x0C (동일) |
| c_rel (ind=0x24) | b_off + 0x20 | b_off + 0x10 |
| c_rel (ind=0x34) | 없음 | b_off + 0x18 |
| c_rel (ind=0x38) | 없음 | b_off + 0x10 |

**자동 감지 방법**: indicator가 0x34/0x38이면 무조건 NEW schema.
indicator가 0x20/0x24이면 b_off+0x14의 값이 {1,2,3} 이내인지로 판별.
{1,2,3} 범위면 OLD, 아니면 NEW.

**indicator 값 정리:**

| indicator | 의미 | c_start 계산 |
|-----------|------|-------------|
| 0x20 | first_c_ptr 직접 사용 | `c_start = first_c_ptr` |
| 0x24 | first_c_ptr + c_rel | `c_start = first_c_ptr + c_rel` |
| 0x34 | Top 전용 (수직 루트) | `c_start = first_c_ptr + c_rel` (NEW at +0x18) |
| 0x38 | Trans 계열 (수평 루트) | `c_start = first_c_ptr + c_rel` (NEW at +0x10) |
