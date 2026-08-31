# Aurora Player — right-click menu fix + remove-from-playlist

Copy over your existing file, then rebuild:

- src/UI.cpp

## Why right-click did nothing

`BeginPopupContextItem("menu")` opens based on the *most recently submitted
widget* — but that call sat after several other widgets (artist/album/
duration text, the heart button, the plus button), so it was really only
listening for right-clicks on the tiny plus button, not the row you'd
actually right-click on. Fixed by capturing the right-click immediately
after the row's `Selectable` (right where the row itself is defined) and
opening the popup explicitly from there.

## Remove from playlist

Added a "Remove from This Playlist" entry to the right-click menu — only
shows up when you're looking at a playlist's song list (not on Home/
Search/Library/etc., where "remove" wouldn't mean anything). Uses
`Database::removeSongFromPlaylist()`, which already existed and worked —
it just wasn't wired into the UI yet.
