// Entry point — re-exports everything TipTap needs as globals
import { Editor } from '@tiptap/core'
import { StarterKit } from '@tiptap/starter-kit'
import { Image } from '@tiptap/extension-image'
import { Placeholder } from '@tiptap/extension-placeholder'
import { TaskList } from '@tiptap/extension-task-list'
import { TaskItem } from '@tiptap/extension-task-item'
import { Underline } from '@tiptap/extension-underline'
import { Highlight } from '@tiptap/extension-highlight'
import { TextAlign } from '@tiptap/extension-text-align'
import { Color } from '@tiptap/extension-color'
import { TextStyle } from '@tiptap/extension-text-style'
import { FontFamily } from '@tiptap/extension-font-family'
import { Table } from '@tiptap/extension-table'
import { TableRow } from '@tiptap/extension-table-row'
import { TableCell } from '@tiptap/extension-table-cell'
import { TableHeader } from '@tiptap/extension-table-header'

// Expose on window for use by editor.js
window.TipTapCore = { Editor }
window.TipTapStarterKit = { StarterKit }
window.TipTapImage = { Image }
window.TipTapPlaceholder = { Placeholder }
window.TipTapTaskList = { TaskList }
window.TipTapTaskItem = { TaskItem }
window.TipTapUnderline = { Underline }
window.TipTapHighlight = { Highlight }
window.TipTapTextAlign = { TextAlign }
window.TipTapColor = { Color }
window.TipTapTextStyle = { TextStyle }
window.TipTapFontFamily = { FontFamily }
window.TipTapTable = { Table }
window.TipTapTableRow = { TableRow }
window.TipTapTableCell = { TableCell }
window.TipTapTableHeader = { TableHeader }
