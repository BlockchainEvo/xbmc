/*
 *      Copyright (C) 2013 Team XBMC
 *      http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */
#include "JNIUtils.h"
#include "JNIThreading.h"
#include <jni.h>
#include <stdlib.h>
#include <stdio.h>
#include <android/log.h>
#include "utils/log.h"

CJNIUtils::CJNIUtils() : CAndroidJNIBase("")
{
}

bool CJNIUtils::IsMediaMounted()
{
  JNIEnv *env = xbmc_jnienv();

  jclass cEnvironment = env->FindClass("android/os/Environment");
  jmethodID midEnvironmentGetExternalStorageState = env->GetStaticMethodID(cEnvironment, "getExternalStorageState", "()Ljava/lang/String;");
  jstring sStorageState = (jstring)env->CallStaticObjectMethod(cEnvironment, midEnvironmentGetExternalStorageState);
  // if (sStorageState != Environment.MEDIA_MOUNTED && sStorageState != Environment.MEDIA_MOUNTED_READ_ONLY) return false;
  const char* storageState = env->GetStringUTFChars(sStorageState, NULL);
  bool mounted = strcmp(storageState, "mounted") == 0 || strcmp(storageState, "mounted_ro") == 0;
  env->ReleaseStringUTFChars(sStorageState, storageState);
  env->DeleteLocalRef(sStorageState);
  return mounted;
}

void CJNIUtils::setup_env()
{
  JNIEnv* env = xbmc_jnienv();
  if (!env)
  {
    __android_log_print(ANDROID_LOG_VERBOSE, "XBMC","android_main: env is invalid");
    exit(1);
  }
  const char* temp;

  jobject oActivity = CAndroidJNIManager::GetInstance().GetActivityInstance();
  jclass cActivity = env->GetObjectClass(oActivity);
  
  // get the path to the android system libraries
  jclass cSystem = env->FindClass("java/lang/System");
  jmethodID midSystemGetProperty = env->GetStaticMethodID(cSystem, "getProperty", "(Ljava/lang/String;)Ljava/lang/String;");
  jstring sJavaLibraryPath = env->NewStringUTF("java.library.path");
  jstring sSystemLibraryPath = (jstring)env->CallStaticObjectMethod(cSystem, midSystemGetProperty, sJavaLibraryPath);
  temp = env->GetStringUTFChars(sSystemLibraryPath, NULL);
  setenv("XBMC_ANDROID_SYSTEM_LIBS", temp, 0);
  env->ReleaseStringUTFChars(sSystemLibraryPath, temp);
  env->DeleteLocalRef(sJavaLibraryPath);
  env->DeleteLocalRef(sSystemLibraryPath);
  env->DeleteLocalRef(cSystem);

  // get the path to XBMC's data directory (usually /data/data/<app-name>)
  jmethodID midActivityGetApplicationInfo = env->GetMethodID(cActivity, "getApplicationInfo", "()Landroid/content/pm/ApplicationInfo;");
  jobject oApplicationInfo = env->CallObjectMethod(oActivity, midActivityGetApplicationInfo);
  jclass cApplicationInfo = env->GetObjectClass(oApplicationInfo);
  jfieldID fidApplicationInfoDataDir = env->GetFieldID(cApplicationInfo, "dataDir", "Ljava/lang/String;");
  jstring sDataDir = (jstring)env->GetObjectField(oApplicationInfo, fidApplicationInfoDataDir);
  temp = env->GetStringUTFChars(sDataDir, NULL);
  setenv("XBMC_ANDROID_DATA", temp, 0);
  env->ReleaseStringUTFChars(sDataDir, temp);
  env->DeleteLocalRef(sDataDir);
  
  // get the path to where android extracts native libraries to
  jfieldID fidApplicationInfoNativeLibraryDir = env->GetFieldID(cApplicationInfo, "nativeLibraryDir", "Ljava/lang/String;");
  jstring sNativeLibraryDir = (jstring)env->GetObjectField(oApplicationInfo, fidApplicationInfoNativeLibraryDir);
  temp = env->GetStringUTFChars(sNativeLibraryDir, NULL);
  setenv("XBMC_ANDROID_LIBS", temp, 0);
  env->ReleaseStringUTFChars(sNativeLibraryDir, temp);
  env->DeleteLocalRef(sNativeLibraryDir);
  env->DeleteLocalRef(oApplicationInfo);
  env->DeleteLocalRef(cApplicationInfo);

  // get the path to the APK
  char apkPath[PATH_MAX] = {0};
  jmethodID midActivityGetPackageResourcePath = env->GetMethodID(cActivity, "getPackageResourcePath", "()Ljava/lang/String;");
  jstring sApkPath = (jstring)env->CallObjectMethod(oActivity, midActivityGetPackageResourcePath);
  temp = env->GetStringUTFChars(sApkPath, NULL);
  strcpy(apkPath, temp);
  setenv("XBMC_ANDROID_APK", apkPath, 0);
  env->ReleaseStringUTFChars(sApkPath, temp);
  env->DeleteLocalRef(sApkPath);
  
  // Get the path to the temp/cache directory
  char cacheDir[PATH_MAX] = {0};
  char tempDir[PATH_MAX] = {0};
  jmethodID midActivityGetCacheDir = env->GetMethodID(cActivity, "getCacheDir", "()Ljava/io/File;");
  jobject oCacheDir = env->CallObjectMethod(oActivity, midActivityGetCacheDir);

  jclass cFile = env->GetObjectClass(oCacheDir);
  jmethodID midFileGetAbsolutePath = env->GetMethodID(cFile, "getAbsolutePath", "()Ljava/lang/String;");
  env->DeleteLocalRef(cFile);

  jstring sCachePath = (jstring)env->CallObjectMethod(oCacheDir, midFileGetAbsolutePath);
  temp = env->GetStringUTFChars(sCachePath, NULL);
  strcpy(cacheDir, temp);
  strcpy(tempDir, temp);
  env->ReleaseStringUTFChars(sCachePath, temp);
  env->DeleteLocalRef(sCachePath);
  env->DeleteLocalRef(oCacheDir);

  strcat(tempDir, "/temp");
  setenv("XBMC_TEMP", tempDir, 0);

  strcat(cacheDir, "/apk");
  strcat(cacheDir, "/assets");
  setenv("XBMC_BIN_HOME", cacheDir, 0);
  setenv("XBMC_HOME"    , cacheDir, 0);

  // Get the path to the external storage
  // The path would actually be available from state->activity->externalDataPath (apart from a (known) bug in Android 2.3.x)
  // but calling getExternalFilesDir() will automatically create the necessary directories for us.
  // There might NOT be external storage present so check it before trying to setup.
  char storagePath[PATH_MAX] = {0};
  jmethodID midActivityGetExternalFilesDir = env->GetMethodID(cActivity, "getExternalFilesDir", "(Ljava/lang/String;)Ljava/io/File;");
  jobject oExternalDir = env->CallObjectMethod(oActivity, midActivityGetExternalFilesDir, (jstring)NULL);
  if (oExternalDir)
  {
    jstring sExternalPath = (jstring)env->CallObjectMethod(oExternalDir, midFileGetAbsolutePath);
    temp = env->GetStringUTFChars(sExternalPath, NULL);
    strcpy(storagePath, temp);
    env->ReleaseStringUTFChars(sExternalPath, temp);
    env->DeleteLocalRef(sExternalPath);
    env->DeleteLocalRef(oExternalDir);
  }

  // Check if we don't have a valid path yet
  if (strlen(storagePath) <= 0)
  {
    // Get the path to the internal storage
    // For more details see the comment on getting the path to the external storage
    jstring sXbmcName = env->NewStringUTF("org.xbmc.xbmc");
    jmethodID midActivityGetDir = env->GetMethodID(cActivity, "getDir", "(Ljava/lang/String;I)Ljava/io/File;");
    jobject oInternalDir = env->CallObjectMethod(oActivity, midActivityGetDir, sXbmcName, 1 /* MODE_WORLD_READABLE */);
    env->DeleteLocalRef(sXbmcName);

    jstring sInternalPath = (jstring)env->CallObjectMethod(oInternalDir, midFileGetAbsolutePath);
    temp = env->GetStringUTFChars(sInternalPath, NULL);
    strcpy(storagePath, temp);
    env->ReleaseStringUTFChars(sInternalPath, temp);
    env->DeleteLocalRef(sInternalPath);
    env->DeleteLocalRef(oInternalDir);
  }

  env->DeleteLocalRef(cActivity);

  // Check if we have a valid home path
  if (strlen(storagePath) > 0)
    setenv("HOME", storagePath, 0);
  else
    setenv("HOME", getenv("XBMC_TEMP"), 0);
}

int CJNIUtils::GetMaxSystemVolume()
{
  JNIEnv *env = xbmc_jnienv();
  jobject oActivity = CAndroidJNIManager::GetInstance().GetActivityInstance();
  jclass cActivity = env->GetObjectClass(oActivity);

  // Get Audio manager
  //  (AudioManager)getSystemService(Context.AUDIO_SERVICE)
  jmethodID mgetSystemService = env->GetMethodID(cActivity, "getSystemService","(Ljava/lang/String;)Ljava/lang/Object;");
  jstring sAudioService = env->NewStringUTF("audio");
  jobject oAudioManager = env->CallObjectMethod(oActivity, mgetSystemService, sAudioService);
  env->DeleteLocalRef(sAudioService);
  env->DeleteLocalRef(cActivity);

  // Get max volume
  //  int max_volume = mAudioManager.getStreamMaxVolume(AudioManager.STREAM_MUSIC);
  jclass cAudioManager = env->GetObjectClass(oAudioManager);
  jmethodID mgetStreamMaxVolume = env->GetMethodID(cAudioManager, "getStreamMaxVolume", "(I)I");
  jfieldID fstreamMusic = env->GetStaticFieldID(cAudioManager, "STREAM_MUSIC", "I");
  jint stream_music = env->GetStaticIntField(cAudioManager, fstreamMusic);
  int maxVolume = (int)env->CallObjectMethod(oAudioManager, mgetStreamMaxVolume, stream_music); // AudioManager.STREAM_MUSIC

  env->DeleteLocalRef(oAudioManager);
  env->DeleteLocalRef(cAudioManager);

  return maxVolume;
}

bool CJNIUtils::SetSystemVolume(float percent)
{
  CLog::Log(LOGDEBUG, "CXBMCApp::SetSystemVolume: %f", percent);

  JNIEnv *env = xbmc_jnienv();
  jobject oActivity = CAndroidJNIManager::GetInstance().GetActivityInstance();
  jclass cActivity = env->GetObjectClass(oActivity);

  // Get Audio manager
  //  (AudioManager)getSystemService(Context.AUDIO_SERVICE)
  jmethodID mgetSystemService = env->GetMethodID(cActivity, "getSystemService","(Ljava/lang/String;)Ljava/lang/Object;");
  jstring sAudioService = env->NewStringUTF("audio");
  jobject oAudioManager = env->CallObjectMethod(oActivity, mgetSystemService, sAudioService);
  jclass cAudioManager = env->GetObjectClass(oAudioManager);
  env->DeleteLocalRef(sAudioService);
  env->DeleteLocalRef(cActivity);

  // Set volume
  //   mAudioManager.setStreamVolume(AudioManager.STREAM_MUSIC, max_volume, 0);
  jfieldID fstreamMusic = env->GetStaticFieldID(cAudioManager, "STREAM_MUSIC", "I");
  jint stream_music = env->GetStaticIntField(cAudioManager, fstreamMusic);
  jmethodID msetStreamVolume = env->GetMethodID(cAudioManager, "setStreamVolume", "(III)V");
  env->CallObjectMethod(oAudioManager, msetStreamVolume, stream_music, int(GetMaxSystemVolume()*percent), 0);
  env->DeleteLocalRef(oAudioManager);
  env->DeleteLocalRef(cAudioManager);
}
