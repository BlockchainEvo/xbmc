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
#include <sstream>
#define GIGABYTES       1073741824

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

bool CJNIUtils::GetStorageUsage(const std::string &path, std::string &usage)
{
  if (path.empty())
  {
    std::ostringstream fmt;
    fmt.width(24);  fmt << std::left  << "Filesystem";
    fmt.width(12);  fmt << std::right << "Size";
    fmt.width(12);  fmt << "Used";
    fmt.width(12);  fmt << "Avail";
    fmt.width(12);  fmt << "Use %";

    usage = fmt.str();
    return false;
  }

  JNIEnv* env = xbmc_jnienv();

  // android.os.StatFs oStats = new android.os.StatFs(sPath);
  jclass cStatFs = env->FindClass("android/os/StatFs");
  jmethodID midStatFsCtor = env->GetMethodID(cStatFs, "<init>", "(Ljava/lang/String;)V");
  jstring sPath = env->NewStringUTF(path.c_str());
  jobject oStats = env->NewObject(cStatFs, midStatFsCtor, sPath);
  env->DeleteLocalRef(sPath);

  // int iBlockSize = oStats.getBlockSize();
  jmethodID midStatFsGetBlockSize = env->GetMethodID(cStatFs, "getBlockSize", "()I");
  jint iBlockSize = env->CallIntMethod(oStats, midStatFsGetBlockSize);
  
  // int iBlocksTotal = oStats.getBlockCount();
  jmethodID midStatFsGetBlockCount = env->GetMethodID(cStatFs, "getBlockCount", "()I");
  jint iBlocksTotal = env->CallIntMethod(oStats, midStatFsGetBlockCount);
  
  // int iBlocksFree = oStats.getFreeBlocks();
  jmethodID midStatFsGetFreeBlocks = env->GetMethodID(cStatFs, "getFreeBlocks", "()I");
  jint iBlocksFree = env->CallIntMethod(oStats, midStatFsGetFreeBlocks);

  env->DeleteLocalRef(oStats);
  env->DeleteLocalRef(cStatFs);

  if (iBlockSize <= 0 || iBlocksTotal <= 0 || iBlocksFree < 0)
    return false;
  
  float totalSize = (float)iBlockSize * iBlocksTotal / GIGABYTES;
  float freeSize = (float)iBlockSize * iBlocksFree / GIGABYTES;
  float usedSize = totalSize - freeSize;
  float usedPercentage = usedSize / totalSize * 100;

  std::ostringstream fmt;
  fmt << std::fixed;
  fmt.precision(1);
  fmt.width(24);  fmt << std::left  << path;
  fmt.width(12);  fmt << std::right << totalSize << "G"; // size in GB
  fmt.width(12);  fmt << usedSize << "G"; // used in GB
  fmt.width(12);  fmt << freeSize << "G"; // free
  fmt.precision(0);
  fmt.width(12);  fmt << usedPercentage << "%"; // percentage used
  
  usage = fmt.str();
  return true;
}

bool CJNIUtils::GetExternalStorage(std::string &path, const std::string &type /* = "" */)
{
  JNIEnv* env = xbmc_jnienv();

  // check if external storage is available
  // String sStorageState = android.os.Environment.getExternalStorageState();
  jclass cEnvironment = env->FindClass("android/os/Environment");
  jmethodID midEnvironmentGetExternalStorageState = env->GetStaticMethodID(cEnvironment, "getExternalStorageState", "()Ljava/lang/String;");
  jstring sStorageState = (jstring)env->CallStaticObjectMethod(cEnvironment, midEnvironmentGetExternalStorageState);
  // if (sStorageState != Environment.MEDIA_MOUNTED && sStorageState != Environment.MEDIA_MOUNTED_READ_ONLY) return false;
  const char* storageState = env->GetStringUTFChars(sStorageState, NULL);
  bool mounted = strcmp(storageState, "mounted") == 0 || strcmp(storageState, "mounted_ro") == 0;
  env->ReleaseStringUTFChars(sStorageState, storageState);
  env->DeleteLocalRef(sStorageState);

  if (mounted)
  {
    jobject oExternalStorageDirectory = NULL;
    if (type.empty() || type == "files")
    {
      // File oExternalStorageDirectory = Environment.getExternalStorageDirectory();
      jmethodID midEnvironmentGetExternalStorageDirectory = env->GetStaticMethodID(cEnvironment, "getExternalStorageDirectory", "()Ljava/io/File;");
      oExternalStorageDirectory = env->CallStaticObjectMethod(cEnvironment, midEnvironmentGetExternalStorageDirectory);
    }
    else if (type == "music" || type == "videos" || type == "pictures" || type == "photos" || type == "downloads")
    {
      jstring sType = NULL;
      if (type == "music")
        sType = env->NewStringUTF("Music"); // Environment.DIRECTORY_MUSIC
      else if (type == "videos")
        sType = env->NewStringUTF("Movies"); // Environment.DIRECTORY_MOVIES
      else if (type == "pictures")
        sType = env->NewStringUTF("Pictures"); // Environment.DIRECTORY_PICTURES
      else if (type == "photos")
        sType = env->NewStringUTF("DCIM"); // Environment.DIRECTORY_DCIM
      else if (type == "downloads")
        sType = env->NewStringUTF("Download"); // Environment.DIRECTORY_DOWNLOADS

      // File oExternalStorageDirectory = Environment.getExternalStoragePublicDirectory(sType);
      jmethodID midEnvironmentGetExternalStoragePublicDirectory = env->GetStaticMethodID(cEnvironment, "getExternalStoragePublicDirectory", "(Ljava/lang/String;)Ljava/io/File;");
      oExternalStorageDirectory = env->CallStaticObjectMethod(cEnvironment, midEnvironmentGetExternalStoragePublicDirectory, sType);
      env->DeleteLocalRef(sType);
    }

    if (oExternalStorageDirectory != NULL)
    {
      // path = oExternalStorageDirectory.getAbsolutePath();
      jclass cFile = env->GetObjectClass(oExternalStorageDirectory);
      jmethodID midFileGetAbsolutePath = env->GetMethodID(cFile, "getAbsolutePath", "()Ljava/lang/String;");
      env->DeleteLocalRef(cFile);
      jstring sPath = (jstring)env->CallObjectMethod(oExternalStorageDirectory, midFileGetAbsolutePath);
      const char* cPath = env->GetStringUTFChars(sPath, NULL);
      path = cPath;
      env->ReleaseStringUTFChars(sPath, cPath);
      env->DeleteLocalRef(sPath);
      env->DeleteLocalRef(oExternalStorageDirectory);
    }
    else
      mounted = false;
  }

  env->DeleteLocalRef(cEnvironment);

  return mounted && !path.empty();
}

int CJNIUtils::GetBatteryLevel()
{
  JNIEnv* env = xbmc_jnienv();

  jobject oActivity = CAndroidJNIManager::GetInstance().GetActivityInstance();

  // IntentFilter oIntentFilter = new IntentFilter(Intent.ACTION_BATTERY_CHANGED);
  jclass cIntentFilter = env->FindClass("android/content/IntentFilter");
  jmethodID midIntentFilterCtor = env->GetMethodID(cIntentFilter, "<init>", "(Ljava/lang/String;)V");
  jstring sIntentBatteryChanged = env->NewStringUTF("android.intent.action.BATTERY_CHANGED"); // Intent.ACTION_BATTERY_CHANGED
  jobject oIntentFilter = env->NewObject(cIntentFilter, midIntentFilterCtor, sIntentBatteryChanged);
  env->DeleteLocalRef(cIntentFilter);
  env->DeleteLocalRef(sIntentBatteryChanged);

  // Intent oBatteryStatus = activity.registerReceiver(null, oIntentFilter);
  jclass cActivity = env->GetObjectClass(oActivity);
  jmethodID midActivityRegisterReceiver = env->GetMethodID(cActivity, "registerReceiver", "(Landroid/content/BroadcastReceiver;Landroid/content/IntentFilter;)Landroid/content/Intent;");
  env->DeleteLocalRef(cActivity);
  jobject oBatteryStatus = env->CallObjectMethod(oActivity, midActivityRegisterReceiver, NULL, oIntentFilter);

  jclass cIntent = env->GetObjectClass(oBatteryStatus);
  jmethodID midIntentGetIntExtra = env->GetMethodID(cIntent, "getIntExtra", "(Ljava/lang/String;I)I");
  env->DeleteLocalRef(cIntent);
  
  // int iLevel = oBatteryStatus.getIntExtra(BatteryManager.EXTRA_LEVEL, -1);
  jstring sBatteryManagerExtraLevel = env->NewStringUTF("level"); // BatteryManager.EXTRA_LEVEL
  jint iLevel = env->CallIntMethod(oBatteryStatus, midIntentGetIntExtra, sBatteryManagerExtraLevel, (jint)-1);
  env->DeleteLocalRef(sBatteryManagerExtraLevel);
  // int iScale = oBatteryStatus.getIntExtra(BatteryManager.EXTRA_SCALE, -1);
  jstring sBatteryManagerExtraScale = env->NewStringUTF("scale"); // BatteryManager.EXTRA_SCALE
  jint iScale = env->CallIntMethod(oBatteryStatus, midIntentGetIntExtra, sBatteryManagerExtraScale, (jint)-1);
  env->DeleteLocalRef(sBatteryManagerExtraScale);
  env->DeleteLocalRef(oBatteryStatus);
  env->DeleteLocalRef(oIntentFilter);

  if (iLevel <= 0 || iScale < 0)
    return iLevel;

  return ((int)iLevel * 100) / (int)iScale;
}

// Note intent, dataType, dataURI all default to ""
bool CJNIUtils::StartActivity(const std::string &package, const std::string &intent, const std::string &dataType, const std::string &dataURI)
{
  CLog::Log(LOGDEBUG, "CXBMCApp::StartActivity package: '%s' intent: '%s' dataType: '%s' dataURI: '%s'", package.c_str(), intent.c_str(), dataType.c_str(), dataURI.c_str());

  jthrowable exc;
  JNIEnv* env = xbmc_jnienv();

  jobject oActivity = CAndroidJNIManager::GetInstance().GetActivityInstance();
  jclass cActivity = env->GetObjectClass(oActivity);

  jobject oIntent = NULL;
  jclass cIntent = NULL;
  if (intent.size())
  {
    // Java equivalent for following JNI
    //    Intent oIntent = new Intent(Intent.ACTION_VIEW);
    cIntent = env->FindClass("android/content/Intent");
    jmethodID midIntentCtor = env->GetMethodID(cIntent, "<init>", "(Ljava/lang/String;)V");
    jstring sIntent = env->NewStringUTF(intent.c_str());
    oIntent = env->NewObject(cIntent, midIntentCtor, sIntent);
    env->DeleteLocalRef(sIntent);
  }
  else
  {
    // oPackageManager = new PackageManager();
    jmethodID mgetPackageManager = env->GetMethodID(cActivity, "getPackageManager", "()Landroid/content/pm/PackageManager;");
    jobject oPackageManager = (jobject)env->CallObjectMethod(oActivity, mgetPackageManager);

    // oPackageIntent = oPackageManager.getLaunchIntentForPackage(package);
    jclass cPackageManager = env->GetObjectClass(oPackageManager);
    jmethodID mgetLaunchIntentForPackage = env->GetMethodID(cPackageManager, "getLaunchIntentForPackage", "(Ljava/lang/String;)Landroid/content/Intent;");
    jstring sPackageName = env->NewStringUTF(package.c_str());
    oIntent = env->CallObjectMethod(oPackageManager, mgetLaunchIntentForPackage, sPackageName);
    cIntent = env->GetObjectClass(oIntent);
    env->DeleteLocalRef(cPackageManager);
    env->DeleteLocalRef(sPackageName);
    env->DeleteLocalRef(oPackageManager);

    exc = env->ExceptionOccurred();
    if (exc)
    {
      CLog::Log(LOGERROR, "CXBMCApp::StartActivity Failed to load %s. Exception follows:", package.c_str());
      env->ExceptionDescribe();
      env->ExceptionClear();
      env->DeleteLocalRef(cActivity);
      return false;
    }
    if (!oIntent)
    {
      CLog::Log(LOGERROR, "CXBMCApp::StartActivity %s has no Launch Intent", package.c_str());
      env->DeleteLocalRef(cActivity);
      return false;
    }
  }

  jobject oUri;
  if (dataURI.size())
  {
    // Java equivalent for the following JNI
    //   Uri oUri = Uri.parse(sPath);
    jclass cUri = env->FindClass("android/net/Uri");
    jmethodID midUriParse = env->GetStaticMethodID(cUri, "parse", "(Ljava/lang/String;)Landroid/net/Uri;");
    jstring sPath = env->NewStringUTF(dataURI.c_str());
    oUri = env->CallStaticObjectMethod(cUri, midUriParse, sPath);
    env->DeleteLocalRef(sPath);
    env->DeleteLocalRef(cUri);

    // Run setData or setDataAndType depending on what was passed into the method
    //   This allows opening market links or external players using the same method
    if (dataType.size())
    {
      // Java equivalent for the following JNI
      //   oIntent.setDataAndType(oUri, "video/*");
      jmethodID midIntentSetDataAndType = env->GetMethodID(cIntent, "setDataAndType", "(Landroid/net/Uri;Ljava/lang/String;)Landroid/content/Intent;");
      jstring sMimeType = env->NewStringUTF(dataType.c_str());
      oIntent = env->CallObjectMethod(oIntent, midIntentSetDataAndType, oUri, sMimeType);
      env->DeleteLocalRef(sMimeType);
    }
    else 
    {
      // Java equivalent for the following JNI
      //   oIntent.setData(oUri);
      jmethodID midIntentSetData = env->GetMethodID(cIntent, "setData", "(Landroid/net/Uri;)Landroid/content/Intent;");
      oIntent = env->CallObjectMethod(oIntent, midIntentSetData, oUri);
    }
  }
  
  // Java equivalent for the following JNI
  //   oIntent.setPackage(sPackage);
  jstring sPackage = env->NewStringUTF(package.c_str());
  jmethodID mSetPackage = env->GetMethodID(cIntent, "setPackage", "(Ljava/lang/String;)Landroid/content/Intent;");
  oIntent = env->CallObjectMethod(oIntent, mSetPackage, sPackage);

  if (oUri != NULL)
  {
    env->DeleteLocalRef(oUri);
  }
  env->DeleteLocalRef(cIntent);
  env->DeleteLocalRef(sPackage);
 
  // Java equivalent for the following JNI
  //   startActivity(oIntent);
  jmethodID mStartActivity = env->GetMethodID(cActivity, "startActivity", "(Landroid/content/Intent;)V");
  env->CallVoidMethod(oActivity, mStartActivity, oIntent);
  env->DeleteLocalRef(cActivity);
  env->DeleteLocalRef(oIntent);

  exc = env->ExceptionOccurred();
  if (exc)
  {
    CLog::Log(LOGERROR, "CXBMCApp::StartActivity Failed to load %s. Exception follows:", package.c_str());
    env->ExceptionDescribe();
    env->ExceptionClear();
    return false;
  }
  return true;
}

bool CJNIUtils::HasLaunchIntent(const std::string &package)
{
  JNIEnv* env = xbmc_jnienv();

  jthrowable exc;
  jobject oActivity = CAndroidJNIManager::GetInstance().GetActivityInstance();
  jclass cActivity = env->GetObjectClass(oActivity);

  // oPackageManager = new PackageManager();
  jmethodID mgetPackageManager = env->GetMethodID(cActivity, "getPackageManager", "()Landroid/content/pm/PackageManager;");
  jobject oPackageManager = (jobject)env->CallObjectMethod(oActivity, mgetPackageManager);

  // oPackageIntent = oPackageManager.getLaunchIntentForPackage(package);
  jclass cPackageManager = env->GetObjectClass(oPackageManager);
  jmethodID mgetLaunchIntentForPackage = env->GetMethodID(cPackageManager, "getLaunchIntentForPackage", "(Ljava/lang/String;)Landroid/content/Intent;");
  jstring sPackageName = env->NewStringUTF(package.c_str());
  jobject oPackageIntent = env->CallObjectMethod(oPackageManager, mgetLaunchIntentForPackage, sPackageName);
  env->DeleteLocalRef(sPackageName);
  env->DeleteLocalRef(cPackageManager);
  env->DeleteLocalRef(oPackageManager);

  exc = env->ExceptionOccurred();
  if (exc)
  {
    CLog::Log(LOGERROR, "CXBMCApp::HasLaunchIntent Error checking for  Launch Intent for %s. Exception follows:", package.c_str());
    env->ExceptionDescribe();
    env->ExceptionClear();
    return false;
  }
  if (!oPackageIntent)
  {
    return false;
  }

  env->DeleteLocalRef(oPackageIntent);
  return true;
}

bool CJNIUtils::GetIcon(const std::string &packageName, void* buffer, unsigned int bufSize)
{
  jthrowable exc;
  JNIEnv* env = xbmc_jnienv();

  CLog::Log(LOGERROR, "CXBMCApp::GetIconSize Looking for: %s", packageName.c_str());

  jobject oActivity = CAndroidJNIManager::GetInstance().GetActivityInstance();
  jclass cActivity = env->GetObjectClass(oActivity);

  // oPackageManager = new PackageManager();
  jmethodID mgetPackageManager = env->GetMethodID(cActivity, "getPackageManager", "()Landroid/content/pm/PackageManager;");
  jobject oPackageManager = (jobject)env->CallObjectMethod(oActivity, mgetPackageManager);
  env->DeleteLocalRef(cActivity);

  jclass cPackageManager = env->GetObjectClass(oPackageManager);
  jmethodID mgetApplicationIcon = env->GetMethodID(cPackageManager, "getApplicationIcon", "(Ljava/lang/String;)Landroid/graphics/drawable/Drawable;");
  env->DeleteLocalRef(cPackageManager);

  jclass cBitmapDrawable = env->FindClass("android/graphics/drawable/BitmapDrawable");
  jmethodID mBitmapDrawableCtor = env->GetMethodID(cBitmapDrawable, "<init>", "()V");
  jmethodID mgetBitmap = env->GetMethodID(cBitmapDrawable, "getBitmap", "()Landroid/graphics/Bitmap;");

   // BitmapDrawable oBitmapDrawable;
  jobject oBitmapDrawable = env->NewObject(cBitmapDrawable, mBitmapDrawableCtor);
  jstring sPackageName = env->NewStringUTF(packageName.c_str());

  // oBitmapDrawable = oPackageManager.getApplicationIcon(sPackageName)
  oBitmapDrawable =  env->CallObjectMethod(oPackageManager, mgetApplicationIcon, sPackageName);
  env->DeleteLocalRef(sPackageName);
  env->DeleteLocalRef(cBitmapDrawable);
  env->DeleteLocalRef(oPackageManager);
  exc = env->ExceptionOccurred();
  if (exc)
  {
    CLog::Log(LOGERROR, "CXBMCApp::GetIcon Error getting icon for  %s. Exception follows:", packageName.c_str());
    env->ExceptionDescribe();
    env->ExceptionClear();
    return false;
  }
  jobject oBitmap = env->CallObjectMethod(oBitmapDrawable, mgetBitmap);
  env->DeleteLocalRef(oBitmapDrawable);
  jclass cBitmap = env->GetObjectClass(oBitmap);
  jmethodID mcopyPixelsToBuffer = env->GetMethodID(cBitmap, "copyPixelsToBuffer", "(Ljava/nio/Buffer;)V");
  jobject oPixels = env->NewDirectByteBuffer(buffer,bufSize);
  env->DeleteLocalRef(cBitmap);

  // memcpy(buffer,oPixels,bufSize); 
  env->CallVoidMethod(oBitmap, mcopyPixelsToBuffer, oPixels);
  env->DeleteLocalRef(oPixels);
  env->DeleteLocalRef(oBitmap);
  exc = env->ExceptionOccurred();
  if (exc)
  {
    CLog::Log(LOGERROR, "CXBMCApp::GetIcon Error copying icon for  %s. Exception follows:", packageName.c_str());
    env->ExceptionDescribe();
    env->ExceptionClear();
    return false;
  }
  return true;
}

bool CJNIUtils::GetIconSize(const std::string &packageName, int *width, int *height)
{
  jthrowable exc;
  JNIEnv* env = xbmc_jnienv();

  jobject oActivity = CAndroidJNIManager::GetInstance().GetActivityInstance();
  jclass cActivity = env->GetObjectClass(oActivity);

  // oPackageManager = new PackageManager();
  jmethodID mgetPackageManager = env->GetMethodID(cActivity, "getPackageManager", "()Landroid/content/pm/PackageManager;");
  jobject oPackageManager = (jobject)env->CallObjectMethod(oActivity, mgetPackageManager);
  env->DeleteLocalRef(cActivity);

  jclass cPackageManager = env->GetObjectClass(oPackageManager);
  jmethodID mgetApplicationIcon = env->GetMethodID(cPackageManager, "getApplicationIcon", "(Ljava/lang/String;)Landroid/graphics/drawable/Drawable;");
  env->DeleteLocalRef(cPackageManager);

  jclass cBitmapDrawable = env->FindClass("android/graphics/drawable/BitmapDrawable");
  jmethodID mBitmapDrawableCtor = env->GetMethodID(cBitmapDrawable, "<init>", "()V");
  jmethodID mgetBitmap = env->GetMethodID(cBitmapDrawable, "getBitmap", "()Landroid/graphics/Bitmap;");

  // BitmapDrawable oBitmapDrawable;
  jobject oBitmapDrawable = env->NewObject(cBitmapDrawable, mBitmapDrawableCtor);
  jstring sPackageName = env->NewStringUTF(packageName.c_str());

  // oBitmapDrawable = oPackageManager.getApplicationIcon(sPackageName)
  oBitmapDrawable =  env->CallObjectMethod(oPackageManager, mgetApplicationIcon, sPackageName);
  jobject oBitmap = env->CallObjectMethod(oBitmapDrawable, mgetBitmap);
  env->DeleteLocalRef(sPackageName);
  env->DeleteLocalRef(cBitmapDrawable);
  env->DeleteLocalRef(oBitmapDrawable);
  env->DeleteLocalRef(oPackageManager);
  exc = env->ExceptionOccurred();
  if (exc)
  {
    CLog::Log(LOGERROR, "CXBMCApp::GetIconSize Error getting icon size for  %s. Exception follows:", packageName.c_str());
    env->ExceptionDescribe();
    env->ExceptionClear();
    env->DeleteLocalRef(oBitmap);
    return false;
  } 
  jclass cBitmap = env->GetObjectClass(oBitmap);
  jmethodID mgetWidth = env->GetMethodID(cBitmap, "getWidth", "()I");
  jmethodID mgetHeight = env->GetMethodID(cBitmap, "getHeight", "()I");
  env->DeleteLocalRef(cBitmap);

  // width = oBitmap.getWidth;
  *width = (int)env->CallIntMethod(oBitmap, mgetWidth);

  exc = env->ExceptionOccurred();
  if (exc)
  {
    CLog::Log(LOGERROR, "CXBMCApp::GetIconSize Error getting icon width for %s. Exception follows:", packageName.c_str());
    env->ExceptionDescribe();
    env->ExceptionClear();
    env->DeleteLocalRef(oBitmap);
    return false;
  }
  // height = oBitmap.getHeight;
  *height = (int)env->CallIntMethod(oBitmap, mgetHeight);
  env->DeleteLocalRef(oBitmap);

  exc = env->ExceptionOccurred();
  if (exc)
  {
    CLog::Log(LOGERROR, "CXBMCApp::GetIconSize Error getting icon height for %s. Exception follows:", packageName.c_str());
    env->ExceptionDescribe();
    env->ExceptionClear();
    return false;
  }
  return true;
}
