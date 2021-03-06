// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo_shell_apk;

import android.content.Context;
import android.util.Log;

import org.chromium.base.JNINamespace;

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * A placeholder class to call native functions.
 **/
@JNINamespace("mojo::shell")
public class MojoMain {
    private static final String TAG = "MojoMain";

    // Directory where applications bundled with the shell will be extracted.
    private static final String LOCAL_APP_DIRECTORY = "local_apps";
    // Individual applications bundled with the shell as assets.
    private static final String NETWORK_LIBRARY_APP = "network_service.mojo";
    // The mojo_shell library is also an executable run in forked processes when running
    // multi-process.
    private static final String MOJO_SHELL_EXECUTABLE = "libmojo_shell.so";

    /**
     * A guard flag for calling nativeInit() only once.
     **/
    private static boolean sInitialized = false;

    /**
     * Deletes directories holding the temporary files. This should be called early on shell startup
     * to clean up after the previous run.
     */
    static void clearTemporaryFiles(Context context) {
        FileHelper.deleteRecursively(getLocalAppsDir(context));
    }

    /**
     * Initializes the native system.
     **/
    static void ensureInitialized(Context applicationContext, String[] parameters) {
        if (sInitialized)
            return;
        try {
            FileHelper.extractFromAssets(applicationContext, NETWORK_LIBRARY_APP,
                    getLocalAppsDir(applicationContext), false);
            File mojoShell = new File(applicationContext.getApplicationInfo().nativeLibraryDir,
                    MOJO_SHELL_EXECUTABLE);

            List<String> parametersList = new ArrayList<String>();
            // Program name.
            parametersList.add("mojo_shell");
            if (parameters != null) {
                parametersList.addAll(Arrays.asList(parameters));
            }

            nativeInit(applicationContext,
                    mojoShell.getAbsolutePath(),
                    parametersList.toArray(new String[parametersList.size()]),
                    getLocalAppsDir(applicationContext).getAbsolutePath());
            sInitialized = true;
        } catch (Exception e) {
            Log.e(TAG, "MojoMain initialization failed.", e);
            throw new RuntimeException(e);
        }
    }

    /**
     * Starts the specified application in the specified context.
     **/
    static void start(final String appUrl) {
        nativeStart(appUrl);
    }

    private static File getLocalAppsDir(Context context) {
        return context.getDir(LOCAL_APP_DIRECTORY, Context.MODE_PRIVATE);
    }

    /**
     * Initializes the native system. This API should be called only once per process.
     **/
    private static native void nativeInit(Context context, String mojoShellPath,
            String[] parameters,
            String bundledAppsDirectory);

    private static native void nativeStart(String appUrl);

}
