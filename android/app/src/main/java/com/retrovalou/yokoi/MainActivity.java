package com.retrovalou.yokoi;

import android.app.Activity;
import android.content.res.AssetManager;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.opengl.GLES30;
import android.opengl.GLSurfaceView;
import android.opengl.GLUtils;
import android.os.Bundle;
import android.view.MotionEvent;

import java.io.IOException;
import java.io.InputStream;

public final class MainActivity extends Activity {
    static {
        System.loadLibrary("yokoi");
    }

    private GLSurfaceView glView;

    private static native void nativeSetAssetManager(AssetManager assetManager);
    private static native void nativeSetStorageRoot(String path);
    private static native void nativeInit();
    private static native String[] nativeGetTextureAssetNames();
    private static native void nativeSetTextures(
            int segmentTex, int segmentW, int segmentH,
            int backgroundTex, int backgroundW, int backgroundH,
            int consoleTex, int consoleW, int consoleH);
    private static native void nativeResize(int width, int height);
    private static native void nativeRender();
    private static native void nativeTouch(float x, float y, int action);

    private static final class TextureInfo {
        final int id;
        final int width;
        final int height;

        TextureInfo(int id, int width, int height) {
            this.id = id;
            this.width = width;
            this.height = height;
        }
    }

    private TextureInfo loadTextureFromAsset(String assetName) {
        if (assetName == null || assetName.isEmpty()) {
            return new TextureInfo(0, 0, 0);
        }

        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inPreferredConfig = Bitmap.Config.ARGB_8888;

        Bitmap bitmap;
        try (InputStream is = getAssets().open(assetName)) {
            bitmap = BitmapFactory.decodeStream(is, null, options);
        } catch (IOException e) {
            e.printStackTrace();
            return new TextureInfo(0, 0, 0);
        }

        if (bitmap == null) {
            return new TextureInfo(0, 0, 0);
        }

        if (bitmap.getConfig() != Bitmap.Config.ARGB_8888) {
            Bitmap converted = bitmap.copy(Bitmap.Config.ARGB_8888, false);
            bitmap.recycle();
            bitmap = converted;
        }

        int[] tex = new int[1];
        GLES30.glGenTextures(1, tex, 0);
        int texId = tex[0];
        GLES30.glBindTexture(GLES30.GL_TEXTURE_2D, texId);

        GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D, GLES30.GL_TEXTURE_MIN_FILTER, GLES30.GL_LINEAR);
        GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D, GLES30.GL_TEXTURE_MAG_FILTER, GLES30.GL_LINEAR);
        GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D, GLES30.GL_TEXTURE_WRAP_S, GLES30.GL_CLAMP_TO_EDGE);
        GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D, GLES30.GL_TEXTURE_WRAP_T, GLES30.GL_CLAMP_TO_EDGE);

        GLES30.glPixelStorei(GLES30.GL_UNPACK_ALIGNMENT, 1);
        GLUtils.texImage2D(GLES30.GL_TEXTURE_2D, 0, bitmap, 0);

        int w = bitmap.getWidth();
        int h = bitmap.getHeight();
        bitmap.recycle();

        return new TextureInfo(texId, w, h);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        nativeSetAssetManager(getAssets());
        nativeSetStorageRoot(getFilesDir().getAbsolutePath());

        glView = new GLSurfaceView(this);
        glView.setEGLContextClientVersion(3);
        glView.setPreserveEGLContextOnPause(true);
        glView.setRenderer(new GLSurfaceView.Renderer() {
            @Override
            public void onSurfaceCreated(javax.microedition.khronos.opengles.GL10 gl, javax.microedition.khronos.egl.EGLConfig config) {
                nativeInit();

                // Textures must be created on the GL thread.
                String[] names = nativeGetTextureAssetNames();
                String segName = (names != null && names.length > 0) ? names[0] : "";
                String bgName = (names != null && names.length > 1) ? names[1] : "";
                String csName = (names != null && names.length > 2) ? names[2] : "";

                TextureInfo seg = loadTextureFromAsset(segName);
                TextureInfo bg = loadTextureFromAsset(bgName);
                TextureInfo cs = loadTextureFromAsset(csName);

                nativeSetTextures(
                        seg.id, seg.width, seg.height,
                        bg.id, bg.width, bg.height,
                        cs.id, cs.width, cs.height);
            }

            @Override
            public void onSurfaceChanged(javax.microedition.khronos.opengles.GL10 gl, int width, int height) {
                nativeResize(width, height);
            }

            @Override
            public void onDrawFrame(javax.microedition.khronos.opengles.GL10 gl) {
                nativeRender();
            }
        });

        glView.setOnTouchListener((v, event) -> {
            nativeTouch(event.getX(), event.getY(), event.getActionMasked());
            return true;
        });

        setContentView(glView);
    }

    @Override
    protected void onPause() {
        super.onPause();
        glView.onPause();
    }

    @Override
    protected void onResume() {
        super.onResume();
        glView.onResume();
    }
}
