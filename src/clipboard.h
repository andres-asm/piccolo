#pragma once

// Text to the system clipboard. SDL's on desktop and Android; in the browser
// SDL has none of its own (it would keep the text inside the page), so
// clipboard_web.cpp asks the browser (navigator.clipboard), which allows it
// right after a click, as when a button calls this.

void clipboard_set_text(const char* text);
