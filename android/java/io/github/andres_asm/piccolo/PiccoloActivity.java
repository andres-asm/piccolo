package io.github.andres_asm.piccolo;

import org.libsdl.app.SDLActivity;

// piccolo's activity: SDL's activity with SDL3 linked statically into
// libmain.so, so only "main" is loaded. The display mode selection for
// Android TV (resolution + refresh rate) will live here.
public class PiccoloActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] { "main" };
    }
}
