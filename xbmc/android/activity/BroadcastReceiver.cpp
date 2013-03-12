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
#include <jni.h>
#include "Intents.h"
#include "BroadcastReceiver.h"
#include "JNIUtils.h"
#include <android/log.h>

CBroadcastReceiver::CBroadcastReceiver() : CAndroidJNIBase("org/xbmc/xbmc/XBMCBroadcastReceiver")
{
  AddMethod(jniNativeMethod("ReceiveGenericIntent", "(Landroid/content/Intent;)V", (void*)CBroadcastReceiver::ReceiveGenericIntent));
  AddMethod(jniNativeMethod("ReceiveMediaMounted", "(Landroid/content/Intent;)V", (void*)CBroadcastReceiver::ReceiveMediaMounted));
}

void CBroadcastReceiver::ReceiveGenericIntent(JNIEnv *env, jobject thiz, jobject intent)
{
  CAndroidIntents::getInstance().ReceiveIntent(env, intent);
}

void CBroadcastReceiver::ReceiveMediaMounted(JNIEnv *env, jobject thiz, jobject intent)
{
  __android_log_print(ANDROID_LOG_VERBOSE, "XBMC", "CBroadcastReceiver ReceiveMediaMounted");
  CAndroidJNIManager::GetBroadcastReceiver()->m_mediaMounted.Set();
}

bool CBroadcastReceiver::WaitForMedia(int timeout)
{
  if (CAndroidJNIManager::GetJNIUtils()->IsMediaMounted())
    return true;
  m_mediaMounted.WaitMSec(timeout);
  return CAndroidJNIManager::GetJNIUtils()->IsMediaMounted();
}
