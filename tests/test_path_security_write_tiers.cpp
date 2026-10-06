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
// Trust by spelling and the landing gate. The library / playlist and watch
// folder steps consult a trust set of spellings rather than one string: the
// caller's raw spelling as given, plus the form rewrites (drive letter <-> UNC,
// loopback <-> local) of the caller form (what foobar2000 may hold for an item
// reached through a junction) and of the resolved object path, deduplicated
// byte-exactly. A hit in that set admits only together with the landing gate,
// which asks where the object itself sits: on the write side it must be in the
// write whitelist, on a non-system volume, or trusted under its own spelling;
// on the read side it must not be a blacklisted object unless trusted under
// its own spelling. A junction inside a watch folder that lands on a
// system-drive target is thus refused, while a junction landing on a user-data
// volume stays admitted. The non-system-drive allowance uses a single
// non-system-volume predicate that covers true remote shares as well as local
// D: / E: volumes. MediaRead is mirrored too, because a basic-check failure
// there must not fall back to the trust sources.
//
// The real PathSecurity singleton constructor depends on core_api (profile /
// install paths, library_manager, playlist_manager), so it is not unit-testable
// here. Following the convention of test_path_security_unc_bypass.cpp and
// test_path_prefix_boundary.cpp, the admission chain is reimplemented with
// injectable predicates so both the outcome and the deciding step are pinned.
//
// Mirrored production code (src/utils/PathSecurity.h): ValidateMediaWriteAccess,
// ValidateFileWriteAccess, ValidateMediaAccess, LandingSafeForWrite,
// LandingSafeForRead and IsOnNonSystemVolume. Whenever the step order of any
// of these changes, this file must be updated in the same change.
#include "pch.h"
#include <string>
#include <cwctype>

namespace {

// Which step of the chain decided the outcome. Asserting the step rather than
// just the boolean is what keeps the ordering contract observable: several
// steps can admit the same path, and a reordering that changed which one fires
// would otherwise pass unnoticed.
enum class Admission {
    BlacklistRejected,
    WriteWhitelist,
    NonSystemVolume,
    LibraryOrPlaylist,
    TrustedMediaRoots,
    SidecarSameDirectory,
    Denied
};

// Stand-ins for the predicates PathSecurity derives from Win32 and the
// foobar2000 SDK. inLibraryOrPlaylist / inTrustedMediaRoots mean "any
// spelling in the trust set hits". That set is the caller's raw spelling plus
// the form rewrites of the caller form and of the resolved object path
// (drive letter <-> UNC, loopback <-> local), deduplicated byte-exactly; the
// flags here do not model individual spellings. landingSafe is derived, not
// set directly: the write-side landing gate is exactly whitelist OR non-system
// volume OR the object's own spelling being trusted, and modelling it as an
// independent flag would let a test assert a combination the real chain
// cannot produce.
struct Facts {
    bool blacklisted = false;
    bool inWriteWhitelist = false;
    bool onNonSystemVolume = false;
    bool inLibraryOrPlaylist = false;
    bool inTrustedMediaRoots = false;
    bool objectSpellingTrusted = false;
    bool sidecarOfTrustedMedia = false;

    bool landingSafe() const {
        return inWriteWhitelist || onNonSystemVolume || objectSpellingTrusted;
    }
};

// Reimpl of ValidateMediaWriteAccess, in source order.
Admission RunMediaWriteChain(const Facts& f) {
    if (f.blacklisted) return Admission::BlacklistRejected;
    if (f.inWriteWhitelist) return Admission::WriteWhitelist;
    if (f.inLibraryOrPlaylist && f.landingSafe()) return Admission::LibraryOrPlaylist;
    if (f.inTrustedMediaRoots && f.landingSafe()) return Admission::TrustedMediaRoots;
    if (f.sidecarOfTrustedMedia) return Admission::SidecarSameDirectory;
    return Admission::Denied;
}

// Reimpl of ValidateFileWriteAccess, in source order. Differs from the media
// chain by the non-system-volume step, by consulting watch folders ahead of
// that step instead of after library membership, and by not offering sidecar
// trust.
Admission RunFileWriteChain(const Facts& f) {
    if (f.blacklisted) return Admission::BlacklistRejected;
    if (f.inWriteWhitelist) return Admission::WriteWhitelist;
    if (f.inTrustedMediaRoots && f.landingSafe()) return Admission::TrustedMediaRoots;
    if (f.onNonSystemVolume) return Admission::NonSystemVolume;
    if (f.inLibraryOrPlaylist && f.landingSafe()) return Admission::LibraryOrPlaylist;
    return Admission::Denied;
}

bool Admitted(Admission a) {
    return a != Admission::Denied && a != Admission::BlacklistRejected;
}

// MediaRead: basic checks -> read rules -> two fallbacks gated by the read-side
// landing gate (object not blacklisted, or its own spelling trusted).
enum class ReadAdmission { BasicRejected, ReadRules, TrustedMediaRoots, LibraryOrPlaylist, Denied };

struct ReadFacts {
    bool basicChecksPass = true;
    bool readRulesPass = false;
    bool blacklisted = false;
    bool inLibraryOrPlaylist = false;
    bool inTrustedMediaRoots = false;
    bool objectSpellingTrusted = false;

    bool landingSafeRead() const { return !blacklisted || objectSpellingTrusted; }
};

ReadAdmission RunMediaReadChain(const ReadFacts& f) {
    if (!f.basicChecksPass) return ReadAdmission::BasicRejected;
    if (f.readRulesPass) return ReadAdmission::ReadRules;
    if (f.inTrustedMediaRoots && f.landingSafeRead()) return ReadAdmission::TrustedMediaRoots;
    if (f.inLibraryOrPlaylist && f.landingSafeRead()) return ReadAdmission::LibraryOrPlaylist;
    return ReadAdmission::Denied;
}

// Reimpl of PathSecurity::IsOnNonSystemVolume, the single predicate behind
// Facts::onNonSystemVolume. The resolution pipeline tags its output Local or
// RemoteShare; a remote share is a non-system volume by definition, a local
// path is one when it carries a drive letter other than the system drive. A
// virtual-protocol path passes through the pipeline as Local with no drive
// letter and is therefore not a non-system volume.
enum class TargetKind { Local, RemoteShare };

bool IsOnNonSystemVolume(TargetKind kind, const std::wstring& path, wchar_t systemDrive) {
    if (kind == TargetKind::RemoteShare) return true;
    if (path.length() < 2 || path[1] != L':') return false;
    return static_cast<wchar_t>(::towupper(path[0])) != systemDrive;
}

} // namespace

// ============================================
// The GAP_600 fix: a non-system drive is not media-write context
// ============================================

TEST(PathSecurityWriteTiers, MediaWriteDeniesBareNonSystemDrivePath) {
    Facts f;
    f.onNonSystemVolume = true;

    // An audio file sitting on D: that the user never added to the library, a
    // playlist or a watch folder has no media context, so tag writes to it
    // must be refused.
    EXPECT_EQ(RunMediaWriteChain(f), Admission::Denied);
}

TEST(PathSecurityWriteTiers, FileWriteStillAdmitsBareNonSystemDrivePath) {
    Facts f;
    f.onNonSystemVolume = true;

    EXPECT_EQ(RunFileWriteChain(f), Admission::NonSystemVolume);
}

TEST(PathSecurityWriteTiers, TheTwoChainsDifferOnlyByTheNonSystemDriveStep) {
    Facts f;
    f.onNonSystemVolume = true;

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
    // The object itself is in the configured context, which is what a
    // system-drive admission needs after the landing gate; Facts defaults place
    // the object on the system drive.
    f.objectSpellingTrusted = true;
    EXPECT_EQ(RunMediaWriteChain(f), Admission::LibraryOrPlaylist);
}

TEST(PathSecurityWriteTiers, MediaWriteAdmitsWatchFolderMemberNotYetScanned) {
    Facts f;
    f.inTrustedMediaRoots = true;
    // The object itself is in the configured context, which is what a
    // system-drive admission needs after the landing gate; Facts defaults place
    // the object on the system drive.
    f.objectSpellingTrusted = true;

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
    f.onNonSystemVolume = true;

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
    // The object itself is in the configured context, which is what a
    // system-drive admission needs after the landing gate; Facts defaults place
    // the object on the system drive.
    f.objectSpellingTrusted = true;
    EXPECT_EQ(RunFileWriteChain(f), Admission::LibraryOrPlaylist);
}

TEST(PathSecurityWriteTiers, FileWriteDeniesSystemDrivePathWithoutContext) {
    EXPECT_EQ(RunFileWriteChain(Facts{}), Admission::Denied);
}

TEST(PathSecurityWriteTiers, WriteWhitelistOutranksNonSystemDriveOnFileWrite) {
    Facts f;
    f.inWriteWhitelist = true;
    f.onNonSystemVolume = true;

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
    // The object itself is in the configured context, which is what a
    // system-drive admission needs after the landing gate; Facts defaults place
    // the object on the system drive.
    f.objectSpellingTrusted = true;

    // A path the user's media library settings would accept is writable through
    // file.* even on the system drive and outside profile/temp. This is what
    // lets a theme write into a configured watch folder that happens to live
    // under C:\Users.
    EXPECT_EQ(RunFileWriteChain(f), Admission::TrustedMediaRoots);
}

TEST(PathSecurityWriteTiers, WatchFolderOutranksNonSystemDriveOnFileWrite) {
    Facts f;
    f.inTrustedMediaRoots = true;
    f.onNonSystemVolume = true;

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

// ============================================
// Landing gate: trust by spelling is not enough on its own
// ============================================

TEST(PathSecurityWriteTiers, MediaWriteDeniesTrustedSpellingLandingOnUntrustedSystemDriveObject) {
    // A junction inside a watch folder pointing at a system-drive directory:
    // the caller's spelling is trusted, but the object it lands on is on the
    // system drive, outside the whitelist, and not itself in any context.
    Facts f;
    f.inTrustedMediaRoots = true;
    f.inLibraryOrPlaylist = true;
    EXPECT_EQ(RunMediaWriteChain(f), Admission::Denied);
}

TEST(PathSecurityWriteTiers, FileWriteDeniesTrustedSpellingLandingOnUntrustedSystemDriveObject) {
    Facts f;
    f.inTrustedMediaRoots = true;
    f.inLibraryOrPlaylist = true;
    EXPECT_EQ(RunFileWriteChain(f), Admission::Denied);
}

TEST(PathSecurityWriteTiers, TrustedSpellingLandingOnNonSystemVolumeIsAdmitted) {
    // Legitimate junction user: E:\OST\Extra -> D:\MoreMusic. Spelling trusted,
    // landing on a user-data volume.
    Facts f;
    f.inLibraryOrPlaylist = true;
    f.onNonSystemVolume = true;
    EXPECT_EQ(RunMediaWriteChain(f), Admission::LibraryOrPlaylist);
}

TEST(PathSecurityWriteTiers, TrustedSpellingLandingOnObjectWithTrustedOwnSpellingIsAdmitted) {
    Facts f;
    f.inLibraryOrPlaylist = true;
    f.objectSpellingTrusted = true;
    EXPECT_EQ(RunMediaWriteChain(f), Admission::LibraryOrPlaylist);
}

TEST(PathSecurityWriteTiers, RemoteShareIsANonSystemVolumeForFileWrite) {
    // A true remote share has no drive letter yet is a user-data location;
    // the single predicate covers it the same way it covers D: or E:. The
    // predicate itself is pinned by the IsOnNonSystemVolume mirror tests below.
    Facts f;
    f.onNonSystemVolume = true;
    EXPECT_EQ(RunFileWriteChain(f), Admission::NonSystemVolume);
    EXPECT_EQ(RunMediaWriteChain(f), Admission::Denied);
}

// ============================================
// IsOnNonSystemVolume: one predicate for local and remote
// ============================================

TEST(PathSecurityWriteTiers, RemoteShareCountsAsNonSystemVolume) {
    EXPECT_TRUE(IsOnNonSystemVolume(TargetKind::RemoteShare, L"\\\\nas\\music\\x", L'C'));
}

TEST(PathSecurityWriteTiers, LocalNonSystemDriveCountsAsNonSystemVolume) {
    EXPECT_TRUE(IsOnNonSystemVolume(TargetKind::Local, L"D:\\x", L'C'));
    // Drive-letter comparison is case-insensitive: c: is still the system drive.
    EXPECT_FALSE(IsOnNonSystemVolume(TargetKind::Local, L"c:\\x", L'C'));
}

TEST(PathSecurityWriteTiers, VirtualProtocolHasNoDriveLetterSoIsNotNonSystemVolume) {
    // A virtual-protocol path leaves the pipeline as Local with the raw string;
    // without a drive letter it must not be mistaken for a user-data volume.
    EXPECT_FALSE(IsOnNonSystemVolume(TargetKind::Local, L"http://x", L'C'));
}

// ============================================
// MediaRead: no fallback once the basic checks fail
// ============================================

TEST(PathSecurityMediaRead, BasicCheckFailureIsFinal) {
    ReadFacts f;
    f.basicChecksPass = false;
    f.readRulesPass = true;
    f.inLibraryOrPlaylist = true;
    f.inTrustedMediaRoots = true;
    EXPECT_EQ(RunMediaReadChain(f), ReadAdmission::BasicRejected);
}

TEST(PathSecurityMediaRead, ReadRulesPassShortCircuits) {
    ReadFacts f;
    f.readRulesPass = true;
    EXPECT_EQ(RunMediaReadChain(f), ReadAdmission::ReadRules);
}

TEST(PathSecurityMediaRead, FallbackAdmitsTrustedNonBlacklistedObject) {
    ReadFacts f;
    f.inLibraryOrPlaylist = true;
    EXPECT_EQ(RunMediaReadChain(f), ReadAdmission::LibraryOrPlaylist);
}

TEST(PathSecurityMediaRead, FallbackDeniesBlacklistedObjectReachedThroughTrustedSpelling) {
    // Junction inside a watch folder pointing at C:\Windows: reading through it
    // must not be laundered by the watch-folder trust.
    ReadFacts f;
    f.inTrustedMediaRoots = true;
    f.blacklisted = true;
    EXPECT_EQ(RunMediaReadChain(f), ReadAdmission::Denied);
}

TEST(PathSecurityMediaRead, FallbackAdmitsBlacklistedObjectWhoseOwnSpellingIsTrusted) {
    // C:\Windows\Media\*.wav sitting in a playlist stays readable.
    ReadFacts f;
    f.inLibraryOrPlaylist = true;
    f.blacklisted = true;
    f.objectSpellingTrusted = true;
    EXPECT_EQ(RunMediaReadChain(f), ReadAdmission::LibraryOrPlaylist);
}

TEST(PathSecurityMediaRead, MediaReadWatchFolderOutranksLibraryMembership) {
    // The read chain consults watch folders before library / playlist
    // membership, the opposite order to the media-write chain. Both admit here;
    // pinning the deciding step keeps a reordering visible.
    ReadFacts f;
    f.inTrustedMediaRoots = true;
    f.inLibraryOrPlaylist = true;
    EXPECT_EQ(RunMediaReadChain(f), ReadAdmission::TrustedMediaRoots);
}
