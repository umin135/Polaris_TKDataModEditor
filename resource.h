#pragma once

#define IDI_TKMOD_FILE    101   // .tkmod file extension icon
#define IDR_HOME_LOGO_PNG 201   // Home screen logo (PNG, embedded as RCDATA)
#define IDR_FONT_REGULAR  202   // NotoSans Condensed Regular (TTF, embedded)
#define IDR_FONT_BOLD     203   // NotoSans Condensed Bold (TTF, embedded)

// Preview part meshes (OBJ + skeleton.json packed into a single binary archive)
#define IDR_PREVIEW_MESHES 401   // data/preview_meshes.pack

// data/interfacedata/* embedded as RCDATA fallback (disk files override when present)
#define IDR_DATA_CHARLIST 301   // characterList.txt
#define IDR_DATA_REQTXT   302   // editorRequirements.txt
#define IDR_DATA_PROPTXT  303   // editorProperties.txt
#define IDR_DATA_CMDTXT   304   // editorCommands.txt
#define IDR_DATA_NAMEKEYS 305   // name_keys.json
#define IDR_DATA_SUPPKEYS 306   // supplement_name_keys.json
#define IDR_DATA_ANIMKEYS 307   // anim_keys.json
