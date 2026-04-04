#pragma once
#include <cstdint>
#include <cstring>

// -------------------------------------------------------------
//  TkdataNames
//  Static hash -> VFS path table for tkdata.bin character files.
//  Sourced from tkdata_names.txt (game internal VFS hash mapping).
//
//  Hash values are 32-bit stored as uint64_t (high 32 bits are 0).
//  Coverage: .anmbin, .stllstb, .mvl entries for all known characters.
//
//  When the game updates and hashes change, ExtractFile() returns
//  an empty vector -- the caller skips gracefully and motbin-only
//  extraction proceeds without error.
// -------------------------------------------------------------

struct TkdataNameEntry {
    uint64_t    hash;
    const char* vfsPath;
};

static const TkdataNameEntry kTkdataNames[] = {
    // ---- .anmbin ------------------------------------------------
    { 0x000cab489ULL, "mothead/bin/pgn.anmbin"  },
    { 0x0664bdb3ULL,  "mothead/bin/crw.anmbin"  },
    { 0x0ad28f05ULL,  "mothead/bin/kgr.anmbin"  },
    { 0x100f16f1ULL,  "mothead/bin/tgr.anmbin"  },
    { 0x13b694a0ULL,  "mothead/bin/cml.anmbin"  },
    { 0x16afe2bfULL,  "mothead/bin/xxa.anmbin"  },
    { 0x1dd86e07ULL,  "mothead/bin/hms.anmbin"  },
    { 0x256320b4ULL,  "mothead/bin/okm.anmbin"  },
    { 0x2762d943ULL,  "mothead/bin/aml.anmbin"  },
    { 0x279b199cULL,  "mothead/bin/hrs.anmbin"  },
    { 0x295f1ce9ULL,  "mothead/bin/lzd.anmbin"  },
    { 0x2c647b21ULL,  "mothead/bin/ghp.anmbin"  },
    { 0x2cea1fe5ULL,  "mothead/bin/snk.anmbin"  },
    { 0x3119356cULL,  "mothead/bin/bee.anmbin"  },
    { 0x34572e24ULL,  "mothead/bin/ttr.anmbin"  },
    { 0x3521b658ULL,  "mothead/bin/com.anmbin"  },
    { 0x36b2f98dULL,  "mothead/bin/mnt.anmbin"  },
    { 0x3c352b02ULL,  "mothead/bin/xxf.anmbin"  },
    { 0x3e46e2a5ULL,  "mothead/bin/got.anmbin"  },
    { 0x4211d01cULL,  "mothead/bin/rbt.anmbin"  },
    { 0x42743f3aULL,  "mothead/bin/grf.anmbin"  },
    { 0x49a33039ULL,  "mothead/bin/test.anmbin" },
    { 0x535c7a25ULL,  "mothead/bin/kal.anmbin"  },
    { 0x5fc3b1ebULL,  "mothead/bin/swl.anmbin"  },
    { 0x65cf2e41ULL,  "mothead/bin/xxc.anmbin"  },
    { 0x69cb82a9ULL,  "mothead/bin/bbn.anmbin"  },
    { 0x72c268f9ULL,  "mothead/bin/cht.anmbin"  },
    { 0x76e311a3ULL,  "mothead/bin/der.anmbin"  },
    { 0x7f01fd5cULL,  "mothead/bin/grl.anmbin"  },
    { 0x87bb891cULL,  "mothead/bin/cat.anmbin"  },
    { 0x8804a83eULL,  "mothead/bin/wkz.anmbin"  },
    { 0x8c8fea0aULL,  "mothead/bin/cbr.anmbin"  },
    { 0x8d901f1eULL,  "mothead/bin/zbr.anmbin"  },
    { 0x9a1acca8ULL,  "mothead/bin/knk.anmbin"  },
    { 0xb11beb16ULL,  "mothead/bin/xxd.anmbin"  },
    { 0xb205af57ULL,  "mothead/bin/lon.anmbin"  },
    { 0xb3a27a68ULL,  "mothead/bin/rat.anmbin"  },
    { 0xbc0527c2ULL,  "mothead/bin/xxe.anmbin"  },
    { 0xc1bdd983ULL,  "mothead/bin/wlf.anmbin"  },
    { 0xc9bb5d57ULL,  "mothead/bin/bsn.anmbin"  },
    { 0xca185cfcULL,  "mothead/bin/ctr.anmbin"  },
    { 0xd617b195ULL,  "mothead/bin/dog.anmbin"  },
    { 0xe0497485ULL,  "mothead/bin/kmd.anmbin"  },
    { 0xe181f1b1ULL,  "mothead/bin/jly.anmbin"  },
    { 0xe4065591ULL,  "mothead/bin/ccn.anmbin"  },
    { 0xe5f099d4ULL,  "mothead/bin/ant.anmbin"  },
    { 0xed42d28dULL,  "mothead/bin/xxb.anmbin"  },
    { 0xed6ba266ULL,  "mothead/bin/pig.anmbin"  },
    { 0xf5a4f2a0ULL,  "mothead/bin/xxg.anmbin"  },
    { 0xfda4fc12ULL,  "mothead/bin/klw.anmbin"  },

    // ---- .stllstb -----------------------------------------------
    { 0x0544972eULL,  "mothead/bin/xxg.stllstb" },
    { 0x1309f36eULL,  "mothead/bin/der.stllstb" },
    { 0x157939f1ULL,  "mothead/bin/xxc.stllstb" },
    { 0x18366c8cULL,  "mothead/bin/cat.stllstb" },
    { 0x19b38b20ULL,  "mothead/bin/mnt.stllstb" },
    { 0x21b477f9ULL,  "mothead/bin/ctr.stllstb" },
    { 0x27bb6012ULL,  "mothead/bin/kal.stllstb" },
    { 0x28ec29aeULL,  "mothead/bin/cbr.stllstb" },
    { 0x2bde80d8ULL,  "mothead/bin/dog.stllstb" },
    { 0x2dc8045bULL,  "mothead/bin/cml.stllstb" },
    { 0x2f76b3adULL,  "mothead/bin/knk.stllstb" },
    { 0x3159adcfULL,  "mothead/bin/ant.stllstb" },
    { 0x326a2620ULL,  "mothead/bin/xxf.stllstb" },
    { 0x370ff22cULL,  "mothead/bin/zbr.stllstb" },
    { 0x3ec91966ULL,  "mothead/bin/lzd.stllstb" },
    { 0x42c0b8a1ULL,  "mothead/bin/got.stllstb" },
    { 0x4f5f55c9ULL,  "mothead/bin/bee.stllstb" },
    { 0x58e101d1ULL,  "mothead/bin/xxe.stllstb" },
    { 0x5a9e2bd5ULL,  "mothead/bin/okm.stllstb" },
    { 0x5ca59b96ULL,  "mothead/bin/pig.stllstb" },
    { 0x5d780971ULL,  "mothead/bin/aml.stllstb" },
    { 0x6136de90ULL,  "mothead/bin/ttr.stllstb" },
    { 0x6362d236ULL,  "mothead/bin/tgr.stllstb" },
    { 0x6390d1c1ULL,  "mothead/bin/jly.stllstb" },
    { 0x644bd263ULL,  "mothead/bin/xxb.stllstb" },
    { 0x69aacfe2ULL,  "mothead/bin/swl.stllstb" },
    { 0x6d35a144ULL,  "mothead/bin/grf.stllstb" },
    { 0x7534b716ULL,  "mothead/bin/ccn.stllstb" },
    { 0x75831852ULL,  "mothead/bin/wlf.stllstb" },
    { 0x75b708feULL,  "mothead/bin/snk.stllstb" },
    { 0x81e08937ULL,  "mothead/bin/ghp.stllstb" },
    { 0x8c4eae1dULL,  "mothead/bin/hrs.stllstb" },
    { 0x95033852ULL,  "mothead/bin/pgn.stllstb" },
    { 0x96897b71ULL,  "mothead/bin/cht.stllstb" },
    { 0x9ee79aacULL,  "mothead/bin/lon.stllstb" },
    { 0xa236ed12ULL,  "mothead/bin/rbt.stllstb" },
    { 0xacba7d40ULL,  "mothead/bin/wkz.stllstb" },
    { 0xad833b8dULL,  "mothead/bin/kgr.stllstb" },
    { 0xb4f45595ULL,  "mothead/bin/xxd.stllstb" },
    { 0xbb36e8eaULL,  "mothead/bin/test.stllstb"},
    { 0xc524b788ULL,  "mothead/bin/bbn.stllstb" },
    { 0xc813b968ULL,  "mothead/bin/crw.stllstb" },
    { 0xcf1fc786ULL,  "mothead/bin/hms.stllstb" },
    { 0xcfd811c2ULL,  "mothead/bin/rat.stllstb" },
    { 0xec973750ULL,  "mothead/bin/xxa.stllstb" },
    { 0xed25d4f6ULL,  "mothead/bin/grl.stllstb" },
    { 0xf856732aULL,  "mothead/bin/bsn.stllstb" },
    { 0xfbb9ec8fULL,  "mothead/bin/kmd.stllstb" },
    { 0xfd402fa4ULL,  "mothead/bin/klw.stllstb" },

    // ---- .mvl ---------------------------------------------------
    { 0x08668c5aULL,  "mothead/movelist/grl.mvl" },
    { 0x0938a55aULL,  "mothead/movelist/knk.mvl" },
    { 0x0a2ebe46ULL,  "mothead/movelist/cmn.mvl" },
    { 0x15563876ULL,  "mothead/movelist/okm.mvl" },
    { 0x20683930ULL,  "mothead/movelist/dog.mvl" },
    { 0x20bb5017ULL,  "mothead/movelist/klw.mvl" },
    { 0x26288457ULL,  "mothead/movelist/cat.mvl" },
    { 0x28089dd7ULL,  "mothead/movelist/hrs.mvl" },
    { 0x298353b9ULL,  "mothead/movelist/cbr.mvl" },
    { 0x33a3a8d0ULL,  "mothead/movelist/wkz.mvl" },
    { 0x39279177ULL,  "mothead/movelist/ccn.mvl" },
    { 0x43043a1cULL,  "mothead/movelist/bbn.mvl" },
    { 0x51f8adfdULL,  "mothead/movelist/ghp.mvl" },
    { 0x5b3fd9fdULL,  "mothead/movelist/cht.mvl" },
    { 0x6a1ea5bbULL,  "mothead/movelist/ctr.mvl" },
    { 0x6eaec4b0ULL,  "mothead/movelist/lzd.mvl" },
    { 0x72825f8bULL,  "mothead/movelist/jly.mvl" },
    { 0x73bea3c1ULL,  "mothead/movelist/rat.mvl" },
    { 0x7da44738ULL,  "mothead/movelist/bee.mvl" },
    { 0x80a27072ULL,  "mothead/movelist/snk.mvl" },
    { 0x824a9b48ULL,  "mothead/movelist/crw.mvl" },
    { 0x87e1a47fULL,  "mothead/movelist/lon.mvl" },
    { 0x892cc71bULL,  "mothead/movelist/wlf.mvl" },
    { 0x8cb80b5cULL,  "mothead/movelist/pgn.mvl" },
    { 0x8fdfa780ULL,  "mothead/movelist/der.mvl" },
    { 0x95e8a763ULL,  "mothead/movelist/pig.mvl" },
    { 0xa0768aa9ULL,  "mothead/movelist/bsn.mvl" },
    { 0xa2589b3fULL,  "mothead/movelist/cml.mvl" },
    { 0xad231cf3ULL,  "mothead/movelist/dek.mvl" },
    { 0xbd5e0b00ULL,  "mothead/movelist/hms.mvl" },
    { 0xc318c8ebULL,  "mothead/movelist/rbt.mvl" },
    { 0xc6f7d2f8ULL,  "mothead/movelist/swl.mvl" },
    { 0xcb1667b5ULL,  "mothead/movelist/mnt.mvl" },
    { 0xd1ab3a37ULL,  "mothead/movelist/aml.mvl" },
    { 0xd268df17ULL,  "mothead/movelist/kgr.mvl" },
    { 0xd9ee9c6cULL,  "mothead/movelist/zbr.mvl" },
    { 0xe19e8462ULL,  "mothead/movelist/ant.mvl" },
    { 0xe3b6ee56ULL,  "mothead/movelist/tgr.mvl" },
    { 0xe95a71a2ULL,  "mothead/movelist/kal.mvl" },
    { 0xecb742faULL,  "mothead/movelist/swl.mvl" }, // alternate hash
    { 0xfc909b36ULL,  "mothead/movelist/grf.mvl" },
    { 0xfede5925ULL,  "mothead/movelist/kmd.mvl" },
};

static const int kTkdataNamesCount =
    static_cast<int>(sizeof(kTkdataNames) / sizeof(kTkdataNames[0]));

// Look up hash by VFS path. Returns 0 if not found.
inline uint64_t TkdataNameLookup(const char* vfsPath)
{
    for (int i = 0; i < kTkdataNamesCount; ++i) {
        if (strcmp(kTkdataNames[i].vfsPath, vfsPath) == 0)
            return kTkdataNames[i].hash;
    }
    return 0;
}
