# Nexus — Spec

This document records scoped requirements as they are established. See
[README.md](README.md) for the broader application capabilities.

## Goal

Help users manage tasks and rich notes in a desktop application.

## User Stories / Core Scenarios

- As a note reader, I want to inspect a hyperlink's destination by hovering over
  it before deciding whether to open it.

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

## Non-Goals

- Fetching link previews, page titles or remote metadata on hover.
- Changing the existing Ctrl/Command-click behavior for opening links.

## Acceptance Criteria

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

None for hyperlink hover display and authoring.
