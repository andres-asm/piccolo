package io.github.andres_asm.piccolo;

import android.Manifest;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.Settings;
import android.util.Log;

import org.libsdl.app.SDLActivity;

// piccolo's activity: SDL's activity with SDL3 linked statically into
// libmain.so, so only "main" is loaded. The display mode selection for
// Android TV (resolution + refresh rate) will live here too.
//
// Storage: piccolo's folder is /sdcard/piccolo, which survives uninstalling.
// That needs "All files access" on Android 11+ (WRITE_EXTERNAL_STORAGE
// before). This class tells the native side where to put it through the
// environment variable MULTITAP_DATA_DIR (read by frontends/common/app_paths),
// set before SDL_main runs. Without the permission it's not set (the app's
// private folder is used) and the permission is asked for; it applies from
// the next start.
public class PiccoloActivity extends SDLActivity {
    private static final String TAG = "piccolo";
    private static final int STORAGE_REQUEST = 1;

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
        if (hasSharedStorageAccess()) {
            String sdcard = Environment.getExternalStorageDirectory().getAbsolutePath();
            nativeSetenv("MULTITAP_DATA_DIR", sdcard);
            Log.i(TAG, "data folder: " + sdcard + "/piccolo");
        } else {
            askForSharedStorageAccess();
        }
    }

    private boolean hasSharedStorageAccess() {
        if (Build.VERSION.SDK_INT >= 30) {
            return Environment.isExternalStorageManager();
        }
        return checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE) == PackageManager.PERMISSION_GRANTED;
    }

    private void askForSharedStorageAccess() {
        if (Build.VERSION.SDK_INT < 30) {
            requestPermissions(new String[] { Manifest.permission.WRITE_EXTERNAL_STORAGE }, STORAGE_REQUEST);
            return;
        }
        // "All files access" is granted in Settings, not in a dialog. Some
        // Android TV builds don't have that screen: then only adb can grant it.
        Uri package_uri = Uri.parse("package:" + getPackageName());
        try {
            startActivity(new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION, package_uri));
            return;
        } catch (ActivityNotFoundException e) {
            Log.w(TAG, "no per-app All files access screen, trying the list");
        }
        try {
            startActivity(new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION));
        } catch (ActivityNotFoundException e) {
            Log.w(TAG, "no All files access screen on this device; grant it with: adb shell appops set "
                       + getPackageName() + " MANAGE_EXTERNAL_STORAGE allow");
        }
    }
}
