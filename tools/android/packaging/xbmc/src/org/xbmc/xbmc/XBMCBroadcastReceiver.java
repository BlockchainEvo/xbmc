package org.xbmc.xbmc;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

public class XBMCBroadcastReceiver extends BroadcastReceiver
{
  native void ReceiveGenericIntent(Intent intent);
  native void ReceiveMediaMounted(Intent intent);

  @Override
  public void onReceive(Context context, Intent intent)
  {
    String actionName = intent.getAction();
    if (actionName == null)
      return;

    if (actionName.equals(Intent.ACTION_MEDIA_MOUNTED))
        ReceiveMediaMounted(intent);
    else
        ReceiveGenericIntent(intent);
  }
}
