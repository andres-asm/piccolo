package site.multitap.piccolo;

import android.os.Bundle;
import android.os.Environment;
import android.util.Log;

import java.io.File;

import org.libsdl.app.SDLActivity;

// piccolo's activity: SDL's activity with SDL3 linked statically into
// libmain.so, so only "main" is loaded. The display mode selection for
// Android TV (resolution + refresh rate) will live here too.
//
// Storage access was settled by StartActivity before this one started. With
// access, piccolo's folder is /sdcard/piccolo (it survives uninstalling): this
// class tells the native side through the environment variable
// MULTITAP_DATA_DIR (read by frontends-common app_paths), set before SDL_main
// runs. Without access it's the app's Android/media folder.
public class PiccoloActivity extends SDLActivity {
    private static final String TAG = "piccolo";

    @Override
    protected String[] getLibraries() {
        return new String[] { "main" };
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);   // loads libmain.so: nativeSetenv works from here on
        if (mBrokenLibraries) {
            return;
        }
        // Where browsing for games starts (app_paths_home_folder): SDL has no
        // user folders on Android.
        String sdcard = Environment.getExternalStorageDirectory().getAbsolutePath();
        nativeSetenv("MULTITAP_HOME_DIR", sdcard);
        // Debug builds (package site.multitap.piccolo.debug, see build-apk.sh)
        // keep their own folder: their cores are in their own private storage.
        String folder = getPackageName().endsWith(".debug") ? "piccolo-debug" : "piccolo";
        nativeSetenv("PICCOLO_FOLDER_NAME", folder);
        if (StartActivity.hasSharedStorageAccess(this)) {
            nativeSetenv("MULTITAP_DATA_DIR", sdcard);
            Log.i(TAG, "data folder: " + sdcard + "/" + folder);
        } else {
            // Android/media/<package>: no permission needed, and unlike
            // Android/data, file managers can still open it (Android 11+).
            // Deleted on uninstall, so the storage dialog stays the better
            // choice. getExternalMediaDirs is deprecated since API 30 but works.
            File[] media = getExternalMediaDirs();
            if (media.length > 0 && media[0] != null) {
                String path = media[0].getAbsolutePath();
                nativeSetenv("MULTITAP_DATA_DIR", path);
                Log.w(TAG, "no storage access: data folder " + path + "/" + folder + " (deleted on uninstall)");
            } else {
                Log.w(TAG, "no storage access: using the app's private folder (deleted on uninstall)");
            }
        }
    }
}
