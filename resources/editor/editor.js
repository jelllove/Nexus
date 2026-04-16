// ============================================================
// Nexus Rich Text Editor — OneNote-style
// Communicates with C++ via QWebChannel bridge
// ============================================================

let editor = null;
let bridge = null;
let isLoadingContent = false;
let currentZoom = 100;

// ---- QWebChannel Bridge ----

function initBridge() {
    new QWebChannel(qt.webChannelTransport, function(channel) {
        bridge = channel.objects.bridge;

        bridge.loadContentRequested.connect(function(content) {
            if (editor) {
                isLoadingContent = true;
                editor.commands.setContent(content || '<p></p>');
                isLoadingContent = false;
            }
        });

        initEditor();
        bridge.onEditorReady();
    });
}

// ---- TipTap Editor Init ----

function initEditor() {
    const { Editor } = window.TipTapCore || {};
    const { StarterKit } = window.TipTapStarterKit || {};
    const { Image } = window.TipTapImage || {};
    const { Placeholder } = window.TipTapPlaceholder || {};
    const { TaskList } = window.TipTapTaskList || {};
    const { TaskItem } = window.TipTapTaskItem || {};
    const { Underline } = window.TipTapUnderline || {};
    const { Highlight } = window.TipTapHighlight || {};
    const { TextAlign } = window.TipTapTextAlign || {};
    const { Color } = window.TipTapColor || {};
    const { TextStyle } = window.TipTapTextStyle || {};
    const { FontFamily } = window.TipTapFontFamily || {};
    const { Table } = window.TipTapTable || {};
    const { TableRow } = window.TipTapTableRow || {};
    const { TableCell } = window.TipTapTableCell || {};
    const { TableHeader } = window.TipTapTableHeader || {};

    if (!Editor) {
        console.warn('TipTap not loaded, using fallback editor');
        initFallbackEditor();
        return;
    }

    const extensions = [StarterKit];

    if (TextStyle) extensions.push(TextStyle);
    if (Color) extensions.push(Color);
    if (FontFamily) extensions.push(FontFamily);
    if (Image) extensions.push(Image.configure({ inline: true, allowBase64: true }));
    if (Placeholder) extensions.push(Placeholder.configure({
        placeholder: 'Start typing here...'
    }));
    if (TaskList) extensions.push(TaskList);
    if (TaskItem) extensions.push(TaskItem.configure({ nested: true }));
    if (Underline) extensions.push(Underline);
    if (Highlight) extensions.push(Highlight.configure({ multicolor: true }));
    if (TextAlign) extensions.push(TextAlign.configure({ types: ['heading', 'paragraph'] }));
    if (Table) extensions.push(Table.configure({ resizable: true }));
    if (TableRow) extensions.push(TableRow);
    if (TableCell) extensions.push(TableCell);
    if (TableHeader) extensions.push(TableHeader);

    editor = new Editor({
        element: document.querySelector('#editor'),
        extensions: extensions,
        content: '<p></p>',
        onUpdate: ({ editor }) => {
            if (!isLoadingContent && bridge) {
                bridge.setContent(editor.getHTML());
            }
            updateToolbarState();
        },
        onSelectionUpdate: () => {
            updateToolbarState();
        }
    });

    // Title change -> notify C++
    const titleEl = document.getElementById('page-title');
    titleEl.addEventListener('input', function() {
        if (bridge) {
            bridge.setContent('__TITLE__:' + titleEl.textContent.trim());
        }
    });
    // Enter in title -> move focus to editor
    titleEl.addEventListener('keydown', function(e) {
        if (e.key === 'Enter') {
            e.preventDefault();
            editor.commands.focus('start');
        }
    });
}

// ---- Fallback Editor ----

function initFallbackEditor() {
    const editorEl = document.querySelector('#editor');
    editorEl.setAttribute('contenteditable', 'true');
    editorEl.innerHTML = '<p></p>';

    editor = {
        commands: {
            setContent: function(html) { editorEl.innerHTML = html || '<p></p>'; },
            toggleBold: function() { document.execCommand('bold'); },
            toggleItalic: function() { document.execCommand('italic'); },
            toggleUnderline: function() { document.execCommand('underline'); },
            toggleStrike: function() { document.execCommand('strikethrough'); },
            toggleBulletList: function() { document.execCommand('insertUnorderedList'); },
            toggleOrderedList: function() { document.execCommand('insertOrderedList'); },
            toggleBlockquote: function() { document.execCommand('formatBlock', false, 'blockquote'); },
            toggleCodeBlock: function() { document.execCommand('formatBlock', false, 'pre'); },
            setHeading: function(attrs) { document.execCommand('formatBlock', false, 'h' + attrs.level); },
            setParagraph: function() { document.execCommand('formatBlock', false, 'p'); },
            setHorizontalRule: function() { document.execCommand('insertHorizontalRule'); },
            toggleHighlight: function() { document.execCommand('hiliteColor', false, '#ffeaa7'); },
            setTextAlign: function(align) {
                document.execCommand('justify' + align.charAt(0).toUpperCase() + align.slice(1));
            },
            focus: function() {}
        },
        chain: function() {
            return {
                focus: function() {
                    return new Proxy({}, {
                        get: function(target, prop) {
                            if (prop === 'run') return function() {};
                            return function() { return this; }.bind(this);
                        }
                    });
                }
            };
        },
        getHTML: function() { return editorEl.innerHTML; },
        isActive: function() { return false; }
    };

    let debounceTimer = null;
    editorEl.addEventListener('input', function() {
        clearTimeout(debounceTimer);
        debounceTimer = setTimeout(function() {
            if (!isLoadingContent && bridge) {
                bridge.setContent(editorEl.innerHTML);
            }
        }, 300);
    });
}

// ---- Ribbon Tab Switching ----

function switchTab(tabName) {
    document.querySelectorAll('.ribbon-tab').forEach(t => t.classList.remove('active'));
    document.querySelectorAll('.ribbon-panel').forEach(p => p.classList.remove('active'));
    event.target.classList.add('active');
    document.getElementById('tab-' + tabName).classList.add('active');
}

// ---- Toolbar State Update ----

function updateToolbarState() {
    if (!editor || !editor.isActive) return;

    const setActive = (id, active) => {
        const el = document.getElementById(id);
        if (el) el.classList.toggle('is-active', active);
    };

    try {
        setActive('btn-bold', editor.isActive('bold'));
        setActive('btn-italic', editor.isActive('italic'));
        setActive('btn-underline', editor.isActive('underline'));
        setActive('btn-strike', editor.isActive('strike'));
        setActive('btn-bullet', editor.isActive('bulletList'));
        setActive('btn-ordered', editor.isActive('orderedList'));
        setActive('btn-task', editor.isActive('taskList'));
        setActive('btn-align-left', editor.isActive({ textAlign: 'left' }));
        setActive('btn-align-center', editor.isActive({ textAlign: 'center' }));
        setActive('btn-align-right', editor.isActive({ textAlign: 'right' }));

        // Update block style dropdown
        const blockStyle = document.getElementById('blockStyle');
        if (blockStyle) {
            if (editor.isActive('heading', { level: 1 })) blockStyle.value = 'h1';
            else if (editor.isActive('heading', { level: 2 })) blockStyle.value = 'h2';
            else if (editor.isActive('heading', { level: 3 })) blockStyle.value = 'h3';
            else if (editor.isActive('heading', { level: 4 })) blockStyle.value = 'h4';
            else if (editor.isActive('blockquote')) blockStyle.value = 'blockquote';
            else if (editor.isActive('codeBlock')) blockStyle.value = 'codeblock';
            else blockStyle.value = 'paragraph';
        }
    } catch(e) { /* ignore */ }
}

// ---- Formatting Commands ----

function cmd(fn) {
    if (!editor) return;
    if (editor.chain) {
        fn(editor.chain().focus()).run();
    } else if (editor.commands && editor.commands[fn.name]) {
        editor.commands[fn.name]();
    }
}

function toggleBold() { cmd(c => c.toggleBold()); }
function toggleItalic() { cmd(c => c.toggleItalic()); }
function toggleUnderline() { cmd(c => c.toggleUnderline()); }
function toggleStrike() { cmd(c => c.toggleStrike()); }
function toggleBulletList() { cmd(c => c.toggleBulletList()); }
function toggleOrderedList() { cmd(c => c.toggleOrderedList()); }
function toggleTaskList() { cmd(c => c.toggleTaskList()); }
function toggleCodeBlock() { cmd(c => c.toggleCodeBlock()); }
function toggleBlockquote() { cmd(c => c.toggleBlockquote()); }
function toggleHighlight() { cmd(c => c.toggleHighlight()); }
function insertHorizontalRule() { cmd(c => c.setHorizontalRule()); }

function setTextAlign(align) { cmd(c => c.setTextAlign(align)); }

function setHeading(level) { cmd(c => c.toggleHeading({ level: level })); }
function setParagraph() { cmd(c => c.setParagraph()); }

function setFontFamily(family) {
    if (editor && editor.chain) {
        editor.chain().focus().setFontFamily(family).run();
    }
}

function setFontSize(size) {
    // TipTap doesn't have a built-in fontSize extension;
    // use inline style via TextStyle
    if (editor && editor.chain) {
        editor.chain().focus().setMark('textStyle', { fontSize: size }).run();
    } else {
        document.execCommand('fontSize', false, '3');
    }
}

function setTextColor(color) {
    if (editor && editor.chain) {
        editor.chain().focus().setColor(color).run();
    } else {
        document.execCommand('foreColor', false, color);
    }
}

function setBlockStyle(value) {
    if (!editor) return;
    switch(value) {
        case 'h1': setHeading(1); break;
        case 'h2': setHeading(2); break;
        case 'h3': setHeading(3); break;
        case 'h4': setHeading(4); break;
        case 'blockquote': toggleBlockquote(); break;
        case 'codeblock': toggleCodeBlock(); break;
        default: setParagraph(); break;
    }
}

function indentMore() {
    if (editor && editor.chain) {
        // Try to sink list item (increase indent)
        try { editor.chain().focus().sinkListItem('listItem').run(); } catch(e) {}
    }
}

function indentLess() {
    if (editor && editor.chain) {
        try { editor.chain().focus().liftListItem('listItem').run(); } catch(e) {}
    }
}

// ---- Clipboard ----

function doCut() { document.execCommand('cut'); }
function doCopy() { document.execCommand('copy'); }
function doPaste() { document.execCommand('paste'); }

// ---- Insert Commands ----

function insertTable() {
    if (editor && editor.chain) {
        editor.chain().focus().insertTable({ rows: 3, cols: 3, withHeaderRow: true }).run();
    }
}

function insertDateTime() {
    if (editor && editor.chain) {
        const now = new Date();
        const str = now.toLocaleDateString() + ' ' + now.toLocaleTimeString([], {hour: '2-digit', minute:'2-digit'});
        editor.chain().focus().insertContent(str).run();
    }
}

function requestInsertImage() {
    if (bridge) {
        bridge.requestImageInsert();
    }
}

window.insertImage = function(url) {
    if (editor && editor.chain) {
        editor.chain().focus().setImage({ src: url }).run();
    } else {
        document.execCommand('insertImage', false, url);
    }
};

// ---- View Commands ----

function zoomIn() {
    currentZoom = Math.min(currentZoom + 10, 200);
    document.body.style.zoom = currentZoom + '%';
}

function zoomOut() {
    currentZoom = Math.max(currentZoom - 10, 60);
    document.body.style.zoom = currentZoom + '%';
}

function zoomReset() {
    currentZoom = 100;
    document.body.style.zoom = '100%';
}

function toggleRuleLines() {
    document.body.classList.toggle('rule-lines');
}

// ---- Page Title & Timestamp ----

window.setPageTitle = function(title) {
    const titleEl = document.getElementById('page-title');
    if (titleEl) titleEl.textContent = title || '';
};

window.setPageTimestamp = function(timestamp) {
    const tsEl = document.getElementById('page-timestamp');
    if (tsEl) tsEl.textContent = timestamp || '';
};

window.getPageTitle = function() {
    const titleEl = document.getElementById('page-title');
    return titleEl ? titleEl.textContent.trim() : '';
};

// ---- Init ----

document.addEventListener('DOMContentLoaded', function() {
    initBridge();
});
