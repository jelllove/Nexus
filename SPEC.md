# Nexus — Spec

This document records scoped requirements as they are established. See
[README.md](README.md) for the broader application capabilities.

## Goal

Help users manage tasks and rich notes in a desktop application.

## User Stories / Core Scenarios

- As a note reader, I want to inspect a hyperlink's destination by hovering over
  it before deciding whether to open it.
- As a task-list user, I want one checkbox to hide completed tasks and show
  them again when needed.

## Functional Requirements

- Display the hovered content-editor hyperlink's destination URL in the existing
  bottom status bar, including links in task and subtask notes.
- Update the destination when the pointer moves directly to another hyperlink.
- Clear the preview when the pointer leaves the link, the editor loads another
  page, or the user switches or clears the note.
- Preserve a newer operational status message when an old link preview ends.
- Offer **Insert > Link** in the editor ribbon and Ctrl+K (Command+K on macOS)
  while editing note content.
- Insert a link at the cursor or apply it to selected text. A modal dialog
  provides address and display-text fields. Existing links populate both fields
  for editing and offer removal that preserves their text.
- Cancel and Escape leave note content unchanged and restore the selection.
- Preserve selected rich-text formatting when applying or changing only an
  address. Link modifications use normal note autosave and persistence.
- Accept HTTP(S), email, phone and FTP(S) addresses; normalize a leading `www.`
  to HTTPS. Reject invalid addresses and executable or data URL schemes without
  closing the dialog or changing content.

### Task completion visibility

- Provide one checkbox in the existing Tasks pane to hide and show tasks whose
  work status is Completed. Hiding must preserve the underlying tasks and notes.
- Default to showing completed items and remember the preference across products,
  lifecycle views, search results and application restarts.
- Hide a completed main task with its subtree; under unfinished main tasks,
  hide completed subtasks individually. Preserve expansion and visible selection.
- Save pending edits before hiding an open note. A failed save or status update
  must preserve the prior view and note. Explain when filtering hides all hits.
- Preserve hidden items during reorder and movement. Reveal a completed move
  destination by enabling the checkbox. Export and summary scopes are independent.
- See the [completion visibility design](docs/superpowers/specs/2026-10-20-task-completion-visibility-design.md)
  for full behavior and verification coverage.

## Non-Goals

- Fetching link previews, page titles or remote metadata on hover.
- Changing the existing Ctrl/Command-click behavior for opening links.

## Acceptance Criteria

- The completion checkbox filters main and subtask work statuses in all Tasks
  views, persists its preference, preserves note edits and provides accurate
  search counts and an explanation when all results are hidden.
- Regression coverage includes subtree visibility, expansion/selection restore,
  save/status/settings failures and movement through hidden tasks or siblings.

- Hovering editable hyperlink text, including nested formatting, shows its
  destination through DOM events and the editor bridge without
  navigating or opening a browser.
- HTTPS, email and long destinations are handled as plain status text. Long
  destinations remain subject to the available status-bar width.
- Ending a hover or changing notes removes a stale destination without clearing
  an unrelated newer status message.
- Rendered-editor tests cover mouse hover, ribbon insertion, editing, removal,
  keyboard shortcut, cancel/Escape, address validation, formatting and saved HTML.

## Open Questions

None for hyperlink hover/authoring and task completion visibility.
