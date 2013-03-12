/*
 *      Copyright (C) 2012-2013 Team XBMC
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

#include <sstream>

#include <unistd.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>

#include <android/native_window.h>
#include <android/configuration.h>
#include <jni.h>

#include "XBMCApp.h"

#include "input/MouseStat.h"
#include "input/XBMC_keysym.h"
#include "guilib/Key.h"
#include "windowing/XBMC_events.h"
#include <android/log.h>

#include "Application.h"
#include "settings/AdvancedSettings.h"
#include "xbmc.h"
#include "windowing/WinEvents.h"
#include "guilib/GUIWindowManager.h"
#include "utils/log.h"
#include "ApplicationMessenger.h"
#include "JNIThreading.h"
#include "BroadcastReceiver.h"
#include "JNIUtils.h"

#define GIGABYTES       1073741824

using namespace std;

template<class T, void(T::*fn)()>
void* thread_run(void* obj)
{
  (static_cast<T*>(obj)->*fn)();
  return NULL;
}

ANativeActivity *CXBMCApp::m_activity = NULL;
ANativeWindow* CXBMCApp::m_window = NULL;

CXBMCApp::CXBMCApp(ANativeActivity *nativeActivity)
  : m_wakeLock(NULL)
{
  m_activity = nativeActivity;
  m_firstrun = true;
  m_exiting=false;
  if (m_activity == NULL)
  {
    android_printf("CXBMCApp: invalid ANativeActivity instance");
    exit(1);
    return;
  }
}

CXBMCApp::~CXBMCApp()
{
}

void CXBMCApp::onStart()
{
  android_printf("%s: ", __PRETTY_FUNCTION__);
  if (!m_firstrun)
  {
    android_printf("%s: Already running, ignoring request to start", __PRETTY_FUNCTION__);
    return;
  }
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  pthread_create(&m_thread, &attr, thread_run<CXBMCApp, &CXBMCApp::run>, this);
  pthread_attr_destroy(&attr);
}

void CXBMCApp::onResume()
{
  android_printf("%s: ", __PRETTY_FUNCTION__);
}

void CXBMCApp::onPause()
{
  android_printf("%s: ", __PRETTY_FUNCTION__);
}

void CXBMCApp::onStop()
{
  android_printf("%s: ", __PRETTY_FUNCTION__);
}

void CXBMCApp::onDestroy()
{
  android_printf("%s", __PRETTY_FUNCTION__);

  // If android is forcing us to stop, ask XBMC to exit then wait until it's
  // been destroyed.
  if (!m_exiting)
  {
    XBMC_Stop();
    pthread_join(m_thread, NULL);
    android_printf(" => XBMC finished");
  }

  if (m_wakeLock != NULL && m_activity != NULL)
  {
    JNIEnv* env = xbmc_jnienv();
    env->DeleteGlobalRef(m_wakeLock);
    m_wakeLock = NULL;
  }
}

void CXBMCApp::onSaveState(void **data, size_t *size)
{
  android_printf("%s: ", __PRETTY_FUNCTION__);
  // no need to save anything as XBMC is running in its own thread
}

void CXBMCApp::onConfigurationChanged()
{
  android_printf("%s: ", __PRETTY_FUNCTION__);
  // ignore any configuration changes like screen rotation etc
}

void CXBMCApp::onLowMemory()
{
  android_printf("%s: ", __PRETTY_FUNCTION__);
  // can't do much as we don't want to close completely
}

void CXBMCApp::onCreateWindow(ANativeWindow* window)
{
  android_printf("%s: ", __PRETTY_FUNCTION__);
  if (window == NULL)
  {
    android_printf(" => invalid ANativeWindow object");
    return;
  }
  m_window = window;
  acquireWakeLock();
  if(!m_firstrun)
  {
    XBMC_SetupDisplay();
    XBMC_Pause(false);
  }
}

void CXBMCApp::onResizeWindow()
{
  android_printf("%s: ", __PRETTY_FUNCTION__);
  // no need to do anything because we are fixed in fullscreen landscape mode
}

void CXBMCApp::onDestroyWindow()
{
  android_printf("%s: ", __PRETTY_FUNCTION__);

  // If we have exited XBMC, it no longer exists.
  if (!m_exiting)
  {
    XBMC_DestroyDisplay();
    XBMC_Pause(true);
  }

  releaseWakeLock();
  m_window=NULL;
}

void CXBMCApp::onGainFocus()
{
  android_printf("%s: ", __PRETTY_FUNCTION__);
}

void CXBMCApp::onLostFocus()
{
  android_printf("%s: ", __PRETTY_FUNCTION__);
}

bool CXBMCApp::getWakeLock(JNIEnv *env)
{
  return CAndroidJNIManager::GetJNIUtils()->getWakeLock(&m_wakeLock);
}

void CXBMCApp::acquireWakeLock()
{
  CAndroidJNIManager::GetJNIUtils()->acquireWakeLock(&m_wakeLock);
}

void CXBMCApp::releaseWakeLock()
{
  CAndroidJNIManager::GetJNIUtils()->releaseWakeLock(&m_wakeLock);
}

void CXBMCApp::run()
{
  int status = 0;

  android_printf(" => waiting for a window");
  // Hack!
  // TODO: Change EGL startup so that we can start headless, then create the
  // window once android gives us a surface to play with.
  CAndroidJNIManager::GetBroadcastReceiver()->WaitForMedia(30000);
  CAndroidJNIManager::GetJNIUtils()->setup_env();
  while(!m_window)
    usleep(1000);
  m_firstrun=false;
  android_printf(" => running XBMC_Run...");
  try
  {
    status = XBMC_Run(true);
    android_printf(" => XBMC_Run finished with %d", status);
  }
  catch(...)
  {
    android_printf("ERROR: Exception caught on main loop. Exiting");
  }

  // If we are have not been force by Android to exit, notify its finish routine.
  // This will cause android to run through its teardown events, it calls:
  // onPause(), onLostFocus(), onDestroyWindow(), onStop(), onDestroy().
  ANativeActivity_finish(m_activity);
  m_exiting=true;
}

void CXBMCApp::XBMC_Pause(bool pause)
{
  android_printf("XBMC_Pause(%s)", pause ? "true" : "false");
  // Only send the PAUSE action if we are pausing XBMC and something is currently playing
  if (pause && g_application.IsPlaying() && !g_application.IsPaused())
    CApplicationMessenger::Get().SendAction(CAction(ACTION_PAUSE), WINDOW_INVALID, true);

  g_application.m_AppActive = g_application.m_AppFocused = !pause;
}

void CXBMCApp::XBMC_Stop()
{
  CApplicationMessenger::Get().Quit();
}

bool CXBMCApp::XBMC_SetupDisplay()
{
  android_printf("XBMC_SetupDisplay()");
  return CApplicationMessenger::Get().SetupDisplay();
}

bool CXBMCApp::XBMC_DestroyDisplay()
{
  android_printf("XBMC_DestroyDisplay()");
  return CApplicationMessenger::Get().DestroyDisplay();
}

int CXBMCApp::SetBuffersGeometry(int width, int height, int format)
{
  return ANativeWindow_setBuffersGeometry(m_window, width, height, format);
}

int CXBMCApp::android_printf(const char *format, ...)
{
  // For use before CLog is setup by XBMC_Run()
  va_list args;
  va_start(args, format);
  int result = __android_log_vprint(ANDROID_LOG_VERBOSE, "XBMC", format, args);
  va_end(args);
  return result;
}

int CXBMCApp::GetDPI()
{
  if (m_activity == NULL || m_activity->assetManager == NULL)
    return 0;

  // grab DPI from the current configuration - this is approximate
  // but should be close enough for what we need
  AConfiguration *config = AConfiguration_new();
  AConfiguration_fromAssetManager(config, m_activity->assetManager);
  int dpi = AConfiguration_getDensity(config);
  AConfiguration_delete(config);

  return dpi;
}

bool CXBMCApp::ListApplications(vector<androidPackage> *applications)
{
  return CAndroidJNIManager::GetJNIUtils()->ListApplications(applications);
}

bool CXBMCApp::GetIconSize(const string &packageName, int *width, int *height)
{
  return CAndroidJNIManager::GetJNIUtils()->GetIconSize(packageName, width, height);
}

bool CXBMCApp::GetIcon(const string &packageName, void* buffer, unsigned int bufSize)
{
  return CAndroidJNIManager::GetJNIUtils()->GetIcon(packageName, buffer, bufSize);
}


bool CXBMCApp::HasLaunchIntent(const string &package)
{
  return CAndroidJNIManager::GetJNIUtils()->HasLaunchIntent(package);
}

// Note intent, dataType, dataURI all default to ""
bool CXBMCApp::StartActivity(const string &package, const string &intent, const string &dataType, const string &dataURI)
{
  return CAndroidJNIManager::GetJNIUtils()->StartActivity(package, intent, dataType, dataURI);
}

int CXBMCApp::GetBatteryLevel()
{
  return CAndroidJNIManager::GetJNIUtils()->GetBatteryLevel();
}

bool CXBMCApp::GetExternalStorage(std::string &path, const std::string &type /* = "" */)
{
  return CAndroidJNIManager::GetJNIUtils()->GetExternalStorage(path, type);
}

bool CXBMCApp::GetStorageUsage(const std::string &path, std::string &usage)
{
  return CAndroidJNIManager::GetJNIUtils()->GetStorageUsage(path, usage);
}

// Used in Application.cpp to figure out volume steps
int CXBMCApp::GetMaxSystemVolume()
{
  static int maxVolume = -1;
  if (maxVolume == -1)
  {
    JNIEnv* env = xbmc_jnienv();
    maxVolume = GetMaxSystemVolume();
  }
  return maxVolume;
}

int CXBMCApp::GetMaxSystemVolume(JNIEnv *env)
{
  return CAndroidJNIManager::GetJNIUtils()->GetMaxSystemVolume();
}

void CXBMCApp::SetSystemVolume(JNIEnv *env, float percent)
{
  CAndroidJNIManager::GetJNIUtils()->SetSystemVolume(percent);
}

