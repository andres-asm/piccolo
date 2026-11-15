package io.github.andres_asm.piccolo;

import android.Manifest;
import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.Settings;
import android.util.Log;

// What the launcher opens. It only settles storage access, then starts
// PiccoloActivity (SDL + piccolo) and goes away.
//
// Why a separate activity: piccolo picks its folder once, at start
// (frontends/common/app_paths: /sdcard/piccolo with access, the private folder
// without). SDL starts SDL_main as soon as its activity is resumed with a
// surface, and the permission dialog doesn't stop that (it's a translucent
// activity on top). Asking here, before SDL exists, means piccolo always
// starts with the answer known. With access already given it passes straight
// through.
//
// How access is asked for depends on the targetSdk (see AndroidManifest.xml):
//   29 or lower (what we build now): the classic storage permission dialog;
//     with legacy storage that gives all of /sdcard, Android 11+ included.
//   30 or higher (a future Play Store variant): "All files access", granted
//     in Settings; some Android TV builds have no such screen (adb appops then).
public class StartActivity extends Activity {
    private static final String TAG = "piccolo";
    private static final int STORAGE_REQUEST = 1;

    private boolean waiting_for_settings = false;

    // Also used by PiccoloActivity, which sets the native side's folder from it.
    static boolean usesLegacyStorage(Activity activity) {
        return activity.getApplicationInfo().targetSdkVersion <= 29 || Build.VERSION.SDK_INT < 30;
    }

    static boolean hasSharedStorageAccess(Activity activity) {
        if (usesLegacyStorage(activity)) {
            return activity.checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE)
                   == PackageManager.PERMISSION_GRANTED;
        }
        return Environment.isExternalStorageManager();
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (hasSharedStorageAccess(this)) {
            startPiccolo();
        } else if (usesLegacyStorage(this)) {
            requestPermissions(new String[] { Manifest.permission.WRITE_EXTERNAL_STORAGE }, STORAGE_REQUEST);
        } else if (!askForAllFilesAccess()) {
            startPiccolo();   // no screen to ask on: start with the private folder
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        if (requestCode == STORAGE_REQUEST) {
            if (grantResults.length == 0 || grantResults[0] != PackageManager.PERMISSION_GRANTED) {
                Log.w(TAG, "storage permission refused: using the app's private folder (deleted on uninstall)");
            }
            startPiccolo();
        }
    }

    // Back from the All files access screen, whatever the user chose.
    @Override
    protected void onResume() {
        super.onResume();
        if (waiting_for_settings) {
            waiting_for_settings = false;
            startPiccolo();
        }
    }

    // False when the device has no screen for it.
    private boolean askForAllFilesAccess() {
        Uri package_uri = Uri.parse("package:" + getPackageName());
        try {
            startActivity(new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION, package_uri));
            waiting_for_settings = true;
            return true;
        } catch (ActivityNotFoundException e) {
            Log.w(TAG, "no per-app All files access screen, trying the list");
        }
        try {
            startActivity(new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION));
            waiting_for_settings = true;
            return true;
        } catch (ActivityNotFoundException e) {
            Log.w(TAG, "no All files access screen on this device; grant it with: adb shell appops set "
                       + getPackageName() + " MANAGE_EXTERNAL_STORAGE allow");
        }
        return false;
    }

    private void startPiccolo() {
        startActivity(new Intent(this, PiccoloActivity.class));
        finish();
    }
}
