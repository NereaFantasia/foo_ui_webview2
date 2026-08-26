// test_path_security_write_tiers.cpp - MediaWrite vs FileWrite admission chains
//
// Pins the split between PathSecurity::ValidateMediaWriteAccess and
// ValidateFileWriteAccess. The security-relevant property is that a path is no
// longer admitted for media writes merely because it sits on a non-system
// drive: inheriting the read policy's non-system-drive allowance onto the write
// side would mean any theme could rewrite arbitrary audio files on D: or E:.
//
// FileWrite keeps that allowance on purpose. file.mkdir and file.write create
// paths that by definition are in no library and no playlist, so those two
// trust sources alone would reject every call.
//
// Watch folders are the exception, and the reason FileWrite consults them:
// is_path_addable answers a configuration question — do the user's current
// media library settings allow this path in — which a path that does not exist
// yet can still satisfy. It is checked ahead of the non-system-drive allowance
// so that the deciding step stays stable once that allowance is removed. The
// position does not change which paths are admitted: no step in either chain
// rejects on a miss, so a later step is still reached.
//
// The real PathSecurity singleton constructor depends on core_api (profile /
// install paths, library_manager, playlist_manager), so it is not unit-testable
// here. Following the convention of test_path_security_unc_bypass.cpp and
// test_path_prefix_boundary.cpp, the admission chain is reimplemented with
// injectable predicates so both the outcome and the deciding step are pinned.
#include "pch.h"
#include <string>

namespace {

// Which step of the chain decided the outcome. Asserting the step rather than
// just the boolean is what keeps the ordering contract observable: several
// steps can admit the same path, and a reordering that changed which one fires
// would otherwise pass unnoticed.
enum class Admission {
    BlacklistRejected,
    WriteWhitelist,
    NonSystemDrive,
    LibraryOrPlaylist,
    TrustedMediaRoots,
    SidecarSameDirectory,
    Denied
};

// Stand-ins for the predicates PathSecurity derives from Win32 and the
// foobar2000 SDK. Each test sets only what it needs.
struct Facts {
    bool blacklisted = false;
    bool inWriteWhitelist = false;
    bool onNonSystemDrive = false;
    bool inLibraryOrPlaylist = false;
    bool inTrustedMediaRoots = false;
    bool sidecarOfTrustedMedia = false;
};

// Reimpl of ValidateMediaWriteAccess, in source order.
Admission RunMediaWriteChain(const Facts& f) {
    if (f.blacklisted) {
        return Admission::BlacklistRejected;
    }
    if (f.inWriteWhitelist) {
        return Admission::WriteWhitelist;
    }
    if (f.inLibraryOrPlaylist) {
        return Admission::LibraryOrPlaylist;
    }
    if (f.inTrustedMediaRoots) {
        return Admission::TrustedMediaRoots;
    }
    if (f.sidecarOfTrustedMedia) {
        return Admission::SidecarSameDirectory;
    }
    return Admission::Denied;
}

// Reimpl of ValidateFileWriteAccess, in source order. Differs from the media
// chain by the non-system-drive step, by consulting watch folders ahead of that
// step instead of after library membership, and by not offering sidecar trust.
Admission RunFileWriteChain(const Facts& f) {
    if (f.blacklisted) {
        return Admission::BlacklistRejected;
    }
    if (f.inWriteWhitelist) {
        return Admission::WriteWhitelist;
    }
    if (f.inTrustedMediaRoots) {
        return Admission::TrustedMediaRoots;
    }
    if (f.onNonSystemDrive) {
        return Admission::NonSystemDrive;
    }
    if (f.inLibraryOrPlaylist) {
        return Admission::LibraryOrPlaylist;
    }
    return Admission::Denied;
}

bool Admitted(Admission a) {
    return a != Admission::Denied && a != Admission::BlacklistRejected;
}

} // namespace

// ============================================
// The GAP_600 fix: a non-system drive is not media-write context
// ============================================

TEST(PathSecurityWriteTiers, MediaWriteDeniesBareNonSystemDrivePath) {
    Facts f;
    f.onNonSystemDrive = true;

    // An audio file sitting on D: that the user never added to the library, a
    // playlist or a watch folder has no media context, so tag writes to it
    // must be refused.
    EXPECT_EQ(RunMediaWriteChain(f), Admission::Denied);
}

TEST(PathSecurityWriteTiers, FileWriteStillAdmitsBareNonSystemDrivePath) {
    Facts f;
    f.onNonSystemDrive = true;

    EXPECT_EQ(RunFileWriteChain(f), Admission::NonSystemDrive);
}

TEST(PathSecurityWriteTiers, TheTwoChainsDifferOnlyByTheNonSystemDriveStep) {
    Facts f;
    f.onNonSystemDrive = true;

    // Same input, opposite verdicts. This asymmetry is the whole point of
    // splitting the tiers rather than deleting the allowance outright.
    EXPECT_FALSE(Admitted(RunMediaWriteChain(f)));
    EXPECT_TRUE(Admitted(RunFileWriteChain(f)));
}

// ============================================
// MediaWrite admission sources
// ============================================

TEST(PathSecurityWriteTiers, MediaWriteAdmitsStrictWriteWhitelist) {
    Facts f;
    f.inWriteWhitelist = true;
    EXPECT_EQ(RunMediaWriteChain(f), Admission::WriteWhitelist);
}

TEST(PathSecurityWriteTiers, MediaWriteAdmitsLibraryOrPlaylistMember) {
    Facts f;
    f.inLibraryOrPlaylist = true;
    EXPECT_EQ(RunMediaWriteChain(f), Admission::LibraryOrPlaylist);
}

TEST(PathSecurityWriteTiers, MediaWriteAdmitsWatchFolderMemberNotYetScanned) {
    Facts f;
    f.inTrustedMediaRoots = true;

    // A file already on disk under a configured watch folder but not yet
    // scanned into the library is not a library member. Without this step it
    // would be impossible to tag freshly downloaded tracks.
    EXPECT_EQ(RunMediaWriteChain(f), Admission::TrustedMediaRoots);
}

TEST(PathSecurityWriteTiers, MediaWriteAdmitsSidecarSharingTrustedMediaDirectory) {
    Facts f;
    f.sidecarOfTrustedMedia = true;

    // A .lrc written next to a trusted audio file is itself in no library.
    EXPECT_EQ(RunMediaWriteChain(f), Admission::SidecarSameDirectory);
}

TEST(PathSecurityWriteTiers, MediaWriteDeniesWhenNoTrustSourceMatches) {
    EXPECT_EQ(RunMediaWriteChain(Facts{}), Admission::Denied);
}

// ============================================
// Blacklist precedence — must win over every trust source
// ============================================

TEST(PathSecurityWriteTiers, BlacklistBeatsEveryMediaWriteTrustSource) {
    Facts f;
    f.blacklisted = true;
    f.inWriteWhitelist = true;
    f.inLibraryOrPlaylist = true;
    f.inTrustedMediaRoots = true;
    f.sidecarOfTrustedMedia = true;

    // A protected system directory stays blocked even when the item genuinely
    // appears in a library or playlist, which is how an injected system path
    // is prevented from laundering itself into write access.
    EXPECT_EQ(RunMediaWriteChain(f), Admission::BlacklistRejected);
}

TEST(PathSecurityWriteTiers, BlacklistBeatsNonSystemDriveOnFileWrite) {
    Facts f;
    f.blacklisted = true;
    f.onNonSystemDrive = true;

    // Reachable when a junction on D: resolves into a protected system
    // directory: the blacklist is evaluated against the resolved path.
    EXPECT_EQ(RunFileWriteChain(f), Admission::BlacklistRejected);
}

// ============================================
// FileWrite retains the rest of its previous behaviour
// ============================================

TEST(PathSecurityWriteTiers, FileWriteAdmitsStrictWriteWhitelist) {
    Facts f;
    f.inWriteWhitelist = true;
    EXPECT_EQ(RunFileWriteChain(f), Admission::WriteWhitelist);
}

TEST(PathSecurityWriteTiers, FileWriteAdmitsSystemDriveLibraryMember) {
    Facts f;
    f.inLibraryOrPlaylist = true;
    EXPECT_EQ(RunFileWriteChain(f), Admission::LibraryOrPlaylist);
}

TEST(PathSecurityWriteTiers, FileWriteDeniesSystemDrivePathWithoutContext) {
    EXPECT_EQ(RunFileWriteChain(Facts{}), Admission::Denied);
}

TEST(PathSecurityWriteTiers, WriteWhitelistOutranksNonSystemDriveOnFileWrite) {
    Facts f;
    f.inWriteWhitelist = true;
    f.onNonSystemDrive = true;

    // Both admit, but the whitelist is checked first. Pinning the deciding
    // step keeps a future reordering visible.
    EXPECT_EQ(RunFileWriteChain(f), Admission::WriteWhitelist);
}

// ============================================
// Watch folders as a FileWrite trust source of their own
// ============================================

TEST(PathSecurityWriteTiers, FileWriteAdmitsSystemDriveWatchFolderPath) {
    Facts f;
    f.inTrustedMediaRoots = true;

    // A path the user's media library settings would accept is writable through
    // file.* even on the system drive and outside profile/temp. This is what
    // lets a theme write into a configured watch folder that happens to live
    // under C:\Users.
    EXPECT_EQ(RunFileWriteChain(f), Admission::TrustedMediaRoots);
}

TEST(PathSecurityWriteTiers, WatchFolderOutranksNonSystemDriveOnFileWrite) {
    Facts f;
    f.inTrustedMediaRoots = true;
    f.onNonSystemDrive = true;

    // Watch-folder trust decides before the non-system-drive allowance, so the
    // same step keeps deciding these paths once that allowance is removed
    // (GAP_606). Behind the allowance the path would still be admitted — no
    // step rejects on a miss — but the verdict would be attributed to a step
    // that is about to disappear. That is why the deciding step rather than the
    // verdict is what this case pins.
    EXPECT_EQ(RunFileWriteChain(f), Admission::TrustedMediaRoots);
}

TEST(PathSecurityWriteTiers, FileWriteDeniesSystemDrivePathOutsideWatchFolders) {
    Facts f;
    f.sidecarOfTrustedMedia = true;

    // Sidecar trust stays exclusive to the media chain, so a system-drive path
    // that no watch folder covers is still refused. Widening file.* by
    // same-directory trust would be a separate decision, not a side effect.
    EXPECT_EQ(RunFileWriteChain(f), Admission::Denied);
}
