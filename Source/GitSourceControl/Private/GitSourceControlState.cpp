// Copyright (c) 2014-2020 Sebastien Rombauts (sebastien.rombauts@gmail.com)
//
// Distributed under the MIT License (MIT) (See accompanying file LICENSE.txt
// or copy at http://opensource.org/licenses/MIT)

#include "GitSourceControlState.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "GitSourceControl.State"

int32 FGitSourceControlState::GetHistorySize() const
{
	return History.Num();
}

TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FGitSourceControlState::GetHistoryItem( int32 HistoryIndex ) const
{
	check(History.IsValidIndex(HistoryIndex));
	return History[HistoryIndex];
}

TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FGitSourceControlState::FindHistoryRevision(int32 RevisionNumber) const
{
	for (auto Iter(History.CreateConstIterator()); Iter; Iter++)
	{
		if ((*Iter)->GetRevisionNumber() == RevisionNumber)
		{
			return *Iter;
		}
	}

	return nullptr;
}

TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FGitSourceControlState::FindHistoryRevision(const FString& InRevision) const
{
	for (const auto& Revision : History)
	{
		if (Revision->GetRevision() == InRevision)
		{
			return Revision;
		}
	}

	return nullptr;
}

TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FGitSourceControlState::GetBaseRevForMerge() const
{
	for(const auto& Revision : History)
	{
		// look for the the SHA1 id of the file, not the commit id (revision)
		if(Revision->FileHash == PendingMergeBaseFileHash)
		{
			return Revision;
		}
	}

	return nullptr;
}

FSlateIcon FGitSourceControlState::GetIcon() const
{
	switch (GetGitState())
	{
	case EGitState::NotAtHead:
		return FSlateIcon(FAppStyle::GetAppStyleSetName(), "Perforce.NotAtHeadRevision");
	case EGitState::LockedOther:
		return FSlateIcon(FAppStyle::GetAppStyleSetName(), "Perforce.CheckedOutByOtherUser");
	case EGitState::NotLatest:
		return FSlateIcon(FAppStyle::GetAppStyleSetName(), "Perforce.ModifiedOtherBranch");
	case EGitState::Unmerged:
		return FSlateIcon(FAppStyle::GetAppStyleSetName(), "Perforce.Branched");
	case EGitState::Added:
		return FSlateIcon(FAppStyle::GetAppStyleSetName(), "Perforce.OpenForAdd");
	case EGitState::Untracked:
		return FSlateIcon(FAppStyle::GetAppStyleSetName(), "Perforce.NotInDepot");
	case EGitState::Deleted:
		return FSlateIcon(FAppStyle::GetAppStyleSetName(), "Perforce.MarkedForDelete");
	case EGitState::Modified:
	case EGitState::CheckedOut:
		return FSlateIcon(FAppStyle::GetAppStyleSetName(), "Perforce.CheckedOut");
	case EGitState::Ignored:
		return FSlateIcon(FAppStyle::GetAppStyleSetName(), "Perforce.NotInDepot");
	default:
		return FSlateIcon();
	}
}

FText FGitSourceControlState::GetDisplayName() const
{
	switch (GetGitState())
	{
	case EGitState::NotAtHead:
		return LOCTEXT("NotCurrent", "Not current");
	case EGitState::LockedOther:
		return FText::Format(LOCTEXT("CheckedOutOther", "Checked out by: {0}"), FText::FromString(State.LockUser));
	case EGitState::NotLatest:
		return FText::Format(LOCTEXT("ModifiedOtherBranch", "Modified in branch: {0}"), FText::FromString(HeadBranch));
	case EGitState::Unmerged:
		return LOCTEXT("Conflicted", "Conflicted");
	case EGitState::Added:
		return LOCTEXT("OpenedForAdd", "Opened for add");
	case EGitState::Untracked:
		return LOCTEXT("NotInDepot", "Not in depot");
	case EGitState::Deleted:
		return LOCTEXT("MarkedForDelete", "Marked for delete");
	case EGitState::Modified:
	case EGitState::CheckedOut:
		return LOCTEXT("CheckedOut", "Checked out");
	case EGitState::Ignored:
		return LOCTEXT("Ignore", "Ignore");
	case EGitState::Lockable:
		return LOCTEXT("ReadOnly", "Read only");
	case EGitState::None:
		return LOCTEXT("Unknown", "Unknown");
	default:
		return FText();
	}
}

FText FGitSourceControlState::GetDisplayTooltip() const
{
	switch (GetGitState())
	{
	case EGitState::NotAtHead:
		return LOCTEXT("NotCurrent_Tooltip", "The file(s) are not at the head revision");
	case EGitState::LockedOther:
		return FText::Format(LOCTEXT("CheckedOutOther_Tooltip", "Checked out by: {0}"), FText::FromString(State.LockUser));
	case EGitState::NotLatest:
		return FText::Format(LOCTEXT("ModifiedOtherBranch_Tooltip", "Modified in branch: {0} CL:{1} ({2})"), FText::FromString(HeadBranch), FText::FromString(HeadCommit), FText::FromString(HeadAction));
	case EGitState::Unmerged:
		return LOCTEXT("ContentsConflict_Tooltip", "The contents of the item conflict with updates received from the repository.");
	case EGitState::Added:
		return LOCTEXT("OpenedForAdd_Tooltip", "The file(s) are opened for add");
	case EGitState::Untracked:
		return LOCTEXT("NotControlled_Tooltip", "Item is not under version control.");
	case EGitState::Deleted:
		return LOCTEXT("MarkedForDelete_Tooltip", "The file(s) are marked for delete");
	case EGitState::Modified:
	case EGitState::CheckedOut:
		return LOCTEXT("CheckedOut_Tooltip", "The file(s) are checked out");
	case EGitState::Ignored:
		return LOCTEXT("Ignored_Tooltip", "Item is being ignored.");
	case EGitState::Lockable:
		return LOCTEXT("ReadOnly_Tooltip", "The file(s) are marked locally as read-only");
	case EGitState::None:
		return LOCTEXT("Unknown_Tooltip", "The file(s) status is unknown");
	default:
		return FText();
	}
}

const FString& FGitSourceControlState::GetFilename() const
{
	return LocalFilename;
}

const FDateTime& FGitSourceControlState::GetTimeStamp() const
{
	return TimeStamp;
}

// Deleted and Missing assets cannot appear in the Content Browser, but they do in the Submit files to Source Control window!
bool FGitSourceControlState::CanCheckIn() const
{
	// We can check in if this is new content
	if (IsAdded())
	{
		return true;
	}

	// Cannot check back in if conflicted or not current 
	if (!IsCurrent() || IsConflicted())
	{
		return false;
	}

	// We can check back in if we're locked.
	if (State.LockState == ELockState::Locked)
	{
		return true;
	}

	// We can check in any file that has been modified, unless someone else locked it.
	if (State.LockState != ELockState::LockedOther && IsModified() && IsSourceControlled())
	{
		return true;
	}

	return false;
}

bool FGitSourceControlState::CanCheckout() const
{
	if (State.LockState == ELockState::Unlockable)
	{
		// Everything is already available for check in (checked out).
		return false;
	}
	else
	{
		// We don't want to allow checkout if the file is out-of-date, as modifying an out-of-date binary file will most likely result in a merge conflict
		return State.LockState == ELockState::NotLocked && IsCurrent();
	}
}

bool FGitSourceControlState::IsCheckedOut() const
{
	if (State.LockState == ELockState::Unlockable)
	{
		return IsSourceControlled();
	}
	else
	{
		return State.LockState == ELockState::Locked;
	}
}

bool FGitSourceControlState::IsCheckedOutOther(FString* Who) const
{
	if (State.LockState == ELockState::LockedOther && Who != NULL)
	{
		*Who = State.LockUser;
		return true;
	}
	return false;
}

bool FGitSourceControlState::IsCheckedOutInOtherBranch(const FString& CurrentBranch) const
{
	// You can't check out separately per branch
	return false;
}

bool FGitSourceControlState::IsModifiedInOtherBranch(const FString& CurrentBranch) const
{
	return State.RemoteState == ERemoteState::NotLatest;
}

bool FGitSourceControlState::GetOtherBranchHeadModification(FString& HeadBranchOut, FString& ActionOut, int32& HeadChangeListOut) const
{
	if (!IsModifiedInOtherBranch())
	{
		return false;
	}

	HeadBranchOut = HeadBranch;
	ActionOut = HeadAction; // TODO: from ERemoteState
	HeadChangeListOut = 0; // TODO: get head commit
	return true;
}

bool FGitSourceControlState::IsCurrent() const
{
	return State.RemoteState != ERemoteState::NotAtHead && State.RemoteState != ERemoteState::NotLatest;
}

bool FGitSourceControlState::IsSourceControlled() const
{
	return State.TreeState != ETreeState::Untracked && State.TreeState != ETreeState::Ignored && State.TreeState != ETreeState::NotInRepo;
}

bool FGitSourceControlState::IsAdded() const
{
	// Added is when a file was untracked and is now added.
	return State.FileState == EFileState::Added;
}

bool FGitSourceControlState::IsDeleted() const
{
	return State.FileState == EFileState::Deleted;
}

bool FGitSourceControlState::IsIgnored() const
{
	return State.TreeState == ETreeState::Ignored;
}

bool FGitSourceControlState::CanEdit() const
{
	// Perforce does not care about it being current
	return IsCheckedOut() || IsAdded();
}

bool FGitSourceControlState::CanDelete() const
{
	// Perforce enforces that a deleted file must be current.
	if (!IsCurrent())
	{
		return false;
	}
	// If someone else hasn't checked it out, we can delete source controlled files.
	return !IsCheckedOutOther() && IsSourceControlled();
}

bool FGitSourceControlState::IsUnknown() const
{
	return State.FileState == EFileState::Unknown && State.TreeState == ETreeState::NotInRepo;
}

bool FGitSourceControlState::IsModified() const
{
	return State.TreeState == ETreeState::Working ||
		State.TreeState == ETreeState::Staged;
}


bool FGitSourceControlState::CanAdd() const
{
	return State.TreeState == ETreeState::Untracked;
}

bool FGitSourceControlState::IsConflicted() const
{
	return State.FileState == EFileState::Unmerged;
}

bool FGitSourceControlState::CanRevert() const
{
	return CanCheckIn();
}

EGitState::Type FGitSourceControlState::GetGitState() const
{
	// No matter what, we must pull from remote, even if we have locked or if we have modified.
	switch (State.RemoteState)
	{
	case ERemoteState::NotAtHead:
		return EGitState::NotAtHead;
	default:
		break;
	}

	/** Someone else locked this file across branches. */
	// We cannot push under any circumstance, if someone else has locked.
	if (State.LockState == ELockState::LockedOther)
	{
		return EGitState::LockedOther;
	}

	// We could theoretically push, but we shouldn't.
	if (State.RemoteState == ERemoteState::NotLatest)
	{
		return EGitState::NotLatest;
	}

	switch (State.FileState)
	{
	case EFileState::Unmerged:
		return EGitState::Unmerged;
	case EFileState::Added:
		return EGitState::Added;
	case EFileState::Deleted:
		return EGitState::Deleted;
	case EFileState::Modified:
		return EGitState::Modified;
	default:
		break;
	}

	if (State.TreeState == ETreeState::Untracked)
	{
		return EGitState::Untracked;
	}

	if (State.LockState == ELockState::Locked)
	{
		return EGitState::CheckedOut;
	}

	if (IsSourceControlled())
	{
		if (CanCheckout())
		{
			return EGitState::Lockable;
		}
		return EGitState::Unmodified;
	}

	return EGitState::None;
}

#undef LOCTEXT_NAMESPACE
